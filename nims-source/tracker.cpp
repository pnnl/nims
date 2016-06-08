/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  tracker.cpp
 *
 *  Created by Shari Matzner on 02/14/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
#include <iostream> // cout, cin, NIMS_LOG_ERROR
#include <fstream>  // ifstream, ofstream
#include <string>   // for strings
#include <cmath>    // for trigonometric functions
#include <ctime>    // date and time functions

#include <boost/filesystem.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "yaml-cpp/yaml.h"

#include "log.h"            // NIMS_LOG_* macros
#include "nims_ipc.h"       // shared mem
#include "tracked_object.h" // tracking
#include "detections.h"     // detection message 
#include "tracks_message.h"

 using namespace std;
 using namespace boost;
 namespace fs = boost::filesystem;
 using namespace cv;

bool TEST=false;

// TODO:  Resolve multiple definition error so this can be in detections.h
std::ostream& operator<<(std::ostream& strm, const Detection& d)
{
    std::ios_base::fmtflags fflags = strm.setf(std::ios::fixed,std::ios::floatfield);
    int prec = strm.precision();
    strm.precision(3);

    strm << d.timestamp 
    << "," << d.center[BEARING] << "," << d.center[RANGE] << "," << d.center[ELEVATION]
    << "," << d.size[BEARING] << "," << d.size[RANGE] << "," << d.size[ELEVATION]
    << "," << d.rot_deg[0] << "," << d.rot_deg[1]
    << "," << d.intensity_min << "," << d.intensity_max << "," << d.intensity_sum
    << std::endl;

    // restore formatting
    strm.precision(prec);
    strm.setf(fflags);
    return strm;
};

std::ostream& operator<<(std::ostream& strm, const DetectionMessage& dm)
{
    strm << "    ping_num = " << dm.ping_num << "\n"
         << "    num_detections = " << dm.num_detections << "\n"
    << std::endl;
    
    
    return strm;
};

std::ostream& operator<<(std::ostream& strm, const TracksMessage& tm)
{
    std::ios_base::fmtflags fflags = strm.setf(std::ios::fixed,std::ios::floatfield);
    int prec = strm.precision();
    strm.precision(3);

    strm << tm.ping_time 
    << "," << tm.frame_num << "," << tm.ping_num_sonar << "," << tm.num_tracks
    << std::endl;

    // restore formatting
    strm.precision(prec);
    strm.setf(fflags);

    return strm;
};

 int main (int argc, char * argv[]) {

    if ( argc == 2 && (string(argv[1]) == string("test")) ) 
        {
            TEST = true;
            cout << endl << "!!!!!!!!!!!! TEST MODE !!!!!!!!!!!!!!!!!!" << endl;
        }
    string cfgpath, log_level;
    if ( parse_command_line(argc, argv, cfgpath, log_level) != 0 ) return -1;
    setup_logging(string(basename(argv[0])), cfgpath, log_level);
    setup_signal_handling();
    //--------------------------------------------------------------------------
	// DO STUFF
    NIMS_LOG_DEBUG << "Starting " << argv[0];
    
    // Get parameters from config file.
	//string fb_name;
    string mq_ui_name;
    string mq_socket_name;
    
    float maxgap_secs = 1.0; // maximum gap in seconds allowed within a track
    int mintrack_steps = 10; // minimum number of steps to be considered a track
    float process_noise = 1e-1;     // process is animal swimming
    float measurement_noise = 1e-2; // measurement is backscatter
	int pred_err_max = 15; // max difference in pixels between prediction and actual
	// NOTE:  The noise values are somewhat arbitrary.  The important
	//        thing is their relative values. Here, the process noise
	//         should be much greater than the measurement noise because
	//        an animals's progress through the 3D space of moving water
	//        is very noisy.  The measurement noise is the resolution
	//        of the sample space (range bin, beam width).

    try
    {
        YAML::Node config = YAML::LoadFile(cfgpath);
        mq_ui_name = "/" + config["TRACKER_NAME"].as<string>();
        mq_socket_name = "/" + config["TRACKER_SOCKET_NAME"].as<string>();
        YAML::Node params = config["TRACKER"];        
        maxgap_secs            = params["max_gap_in_track_seconds"].as<float>();
        NIMS_LOG_DEBUG << "max_gap_in_track_seconds: " << maxgap_secs;
        mintrack_steps          = params["min_track_steps"].as<int>();
        NIMS_LOG_DEBUG << "min_track_steps: " << mintrack_steps;
        process_noise     = params["process_noise"].as<float>();
        NIMS_LOG_DEBUG << "process_noise: " << process_noise;
        measurement_noise = params["measurement_noise"].as<float>();
        NIMS_LOG_DEBUG << "measurement_noise: " << measurement_noise;
        pred_err_max      = params["max_prediction_error"].as<int>();
        NIMS_LOG_DEBUG << "max_prediction_error: " << pred_err_max;
    }
    catch( const std::exception& e )
    {
        NIMS_LOG_ERROR << "Error reading config file: " << cfgpath << endl;
        NIMS_LOG_ERROR << e.what() << endl;
        return -1;
    }
    
    
    // check in before calling GetNextFrame, to avoid timeout in the
    // NIMS parent process (and subsequent termination).
    SubprocessCheckin(getpid());
    
    // for TEST
    ofstream ofs,ofs2; 

    if (TEST)
    {
        ostringstream ss;
        ss << "tracks_" << maxgap_secs << "-" << mintrack_steps << "-" << pred_err_max << ".csv";
        ofs.open(ss.str());
        ofs << "Id,Ping,Time,Bearing,Range,El,Width,Length,Height,Theta,Phi" << endl;
        ostringstream ss2;
        ss2 << "tracks-msg_" << maxgap_secs << "-" << mintrack_steps << "-" << pred_err_max << ".csv";
        ofs2.open(ss2.str());
        ofs2 << "Time,Frame,Ping,NTracks" << endl;
    }
                

    //-------------------------------------------------------------------------
	// SETUP TRACKING
    
	vector<TrackedObject> active_tracks;   // tracks still being updated
	vector<int> active_id;                 // unique id for each track
    //	vector<TrackedObject> completed_tracks;// completed tracks (no updates)
    //	vector<int> completed_id;              // unique id
	int next_id = 0;
	long completed_tracks_count = 0;
	
    // Create message queues
  // queues used by Python processes are named in the config file
  // other queues are named in nims_ipc.h
    mqd_t mq_ui = CreateMessageQueue(mq_ui_name, sizeof(TracksMessage), true); // non-blocking
    mqd_t mq_socket = CreateMessageQueue(mq_socket_name, sizeof(TracksMessage), true);
    /*
    NOTE:  The detector will open/create this queue non-blocking, but tracker must block.
    The same queue, opened under a different descriptor, can have different blocking behavior.
    */
    mqd_t mq_det = CreateMessageQueue(MQ_DETECTOR_TRACKER_QUEUE, sizeof(DetectionMessage));
    if (mq_det < 0) 
    {
        NIMS_LOG_ERROR << "Error creating MQ_DETECTOR_TRACKER_QUEUE";
        return -1;
    }
    mqd_t mq_arc = CreateMessageQueue(MQ_TRACKER_ARCHIVER_QUEUE, sizeof(TracksMessage), true);
    if (mq_det < 0) 
    {
        NIMS_LOG_ERROR << "Error creating MQ_TRACKER_ARCHIVER_QUEUE";
        return -1;
    }
    NIMS_LOG_DEBUG << "message queues created";
    if (sigint_received) {
        NIMS_LOG_WARNING << "exiting due to SIGINT";
        return 0;
    }

    //-------------------------------------------------------------------------
    // MAIN LOOP
    sigint_received = 0;
    while (!sigint_received)
    {
        DetectionMessage msg_det;
        int ret =  mq_receive(mq_det, (char *)&msg_det, sizeof(DetectionMessage), nullptr);
        if (ret < 0)
        {
            nims_perror("Tracker mq_receive");
            NIMS_LOG_ERROR << "error receiving message from detector";
            NIMS_LOG_WARNING << "sigint_received is " << sigint_received;
        }
        NIMS_LOG_DEBUG << "received detections message with " 
                       << msg_det.num_detections << " detections";
        if (ret > 0 & msg_det.num_detections > 0)
        {
            int n_obj = msg_det.num_detections;
            
            Mat_<float> detected_positions_x(1,n_obj,0.0);
            Mat_<float> detected_positions_y(1,n_obj,0.0);
            for (int f=0; f<n_obj; ++f)
            {
                detected_positions_y(f) = msg_det.detections[f].center[1];
                detected_positions_x(f) = msg_det.detections[f].center[0];
            }
            
            Mat detected_not_matched =  Mat::ones(1,n_obj,CV_8UC1);
            
            int n_active = active_tracks.size();
            NIMS_LOG_DEBUG << n_active << " active tracks:";
            if ( n_active > 0)
            {

                Mat_<float> predicted_positions_x(1,n_active,0.0);
                Mat_<float> predicted_positions_y(1,n_active,0.0);
                Mat_<float> distances(n_active, n_obj, 0.0);
                for (int a=0; a<n_active; ++a)
                {

                    //Point2f pnt = active_tracks[a].predict(msg_det.ping_num);
                    Detection pred = active_tracks[a].predict(msg_det.ping_time);
                    predicted_positions_x(a) = pred.center[0];
                    predicted_positions_y(a) = pred.center[1];
                    
                    magnitude(predicted_positions_x(a)-detected_positions_x,
                      predicted_positions_y(a)-detected_positions_y,
                      distances.row(a));
                }
                
                // match detections to active tracks
                Mat active_not_matched  = Mat::ones(n_active,1,CV_8UC1);
                for (int f=0; f<n_obj; ++f)
                {
                    // if all active tracks have been matched, break
                    if ( countNonZero(active_not_matched) < 1 ) break;
                    
                    // find closest active track
                    double min_dist;
                    int min_idx[2];
                    minMaxIdx(distances.col(f), &min_dist, NULL, min_idx,
                      NULL,active_not_matched);
                    // if detection is too far from all active tracks...
                    if (pred_err_max < min_dist) continue;
                    
                    // make sure one of the remaining detections is not
                    // closer to the found active track
                    if ( countNonZero(distances(Range(min_idx[0],min_idx[0]+1),
                        Range(f+1,n_obj)) < min_dist) ) continue;

                   // active_tracks[min_idx[0]].update(msg_det.ping_time, msg_det.detections[f]);
                    active_tracks[min_idx[0]].update(msg_det.detections[f]);
                    if (TEST)
                    {
                        ofs << active_tracks[min_idx[0]].get_id() << "," << msg_det.ping_num << ", " << msg_det.detections[f];

                    }
                    // mark detection as matched
                    detected_not_matched.at<unsigned char>(f) = 0;
                    // mark track as matched
                    active_not_matched.at<unsigned char>(min_idx[0]) = 0;
                    
                } // for each detection
                
            } // if active tracks
            
            if (sigint_received) {
                NIMS_LOG_WARNING << "tracker end of loop: exiting due to SIGINT";
                break;
            }


            // create new active tracks for unmatched detections
            for (int f=0; f<n_obj; ++f)
            {
                if ( detected_not_matched.at<unsigned char>(f) )
                {
                        active_tracks.push_back( TrackedObject(next_id, 
                        msg_det.detections[f], 
                        process_noise, measurement_noise) );
                    if (TEST)
                    {
                        ofs << active_tracks.back().get_id() << "," << msg_det.ping_num << ", " << msg_det.detections[f];

                    }
                    active_id.push_back(next_id);

                     ++next_id;
                 } // if not matched
             } // for each detection

           } // if detections

        // de-activate tracks and report still active ones 
        vector<Track> tracks; // active tracks 
        vector<Track> completed; // completed tracks 
          int idx = 0;
           while (idx < active_tracks.size())
           {
            if ( msg_det.ping_time - active_tracks[idx].last_epoch() > maxgap_secs )
            {
                // save if greater than minimum length
                if ( active_tracks[idx].track_length() > mintrack_steps )
                {
                    completed.push_back( Track(completed_tracks_count, 
                        active_tracks[idx].get_track()) );
                    ++completed_tracks_count;
                    
                 } // if it's a keeper
                active_tracks.erase(active_tracks.begin()+idx);
                //active_id.erase(active_id.begin()+idx);
            } // if deactivate 
            else
            {
                if (active_tracks[idx].track_length() > mintrack_steps)
                {

                    tracks.push_back( Track(active_tracks[idx].get_id(), 
                        active_tracks[idx].get_track()) );
                } // if add to tracks message
                ++idx;
            } // else

         
        } // for each active
        
            // send UI and socket messages
        TracksMessage msg_trks(msg_det.frame_num, msg_det.ping_num, msg_det.ping_time, tracks);
            mq_send(mq_ui, (const char *)&msg_trks, sizeof(msg_trks), 0); // non-blocking
            mq_send(mq_socket, (const char *)&msg_trks, sizeof(msg_trks), 0);
            NIMS_LOG_DEBUG << "sent tracks message  ping_time = " << msg_trks.ping_time 
                           << ", ping_num = " << msg_trks.ping_num_sonar 
                           << ", num_tracks = " << msg_trks.num_tracks;

if (TEST)
    ofs2 << msg_trks;

    TracksMessage msg_complete(msg_det.frame_num, msg_det.ping_num, msg_det.ping_time, completed);            
    mq_send(mq_arc, (const char *)&msg_complete, sizeof(msg_complete), 0); // non-blocking

        NIMS_LOG_DEBUG << "sent completed tracks message (" << sizeof(msg_complete)
                << " bytes);  num_tracks = " << msg_complete.num_tracks;
    


} // main loop
    
    //-------------------------------------------------------------------------
	// CLEANUP
    if (TEST)
        ofs.close(); 
    // close message queues
    mq_close(mq_ui);
    mq_close(mq_socket);
    //mq_unlink(mq_ui_name.c_str());
    //mq_unlink(mq_socket_name.c_str());
    
    // close output files
   // outtxtfile.close();
    
    // this is the only way to exit
    NIMS_LOG_WARNING << "exiting due to SIGINT";
     return 0;
}
