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
//#include "pixelgroup.h"     // connected components
#include "detections.h"     // detection message for UI
#include "tracks_message.h"

using namespace std;
using namespace boost;
namespace fs = boost::filesystem;
using namespace cv;



int main (int argc, char * argv[]) {

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
    
    int maxgap = 3; // maximum gap in frames allowed within a track
    int mintrack = 10; // minimum number of steps to be considered a track
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
        maxgap            = params["max_ping_gap_in_track"].as<int>();
        NIMS_LOG_DEBUG << "max_ping_gap_in_track: " << maxgap;
        mintrack          = params["min_pings_for_track"].as<int>();
        NIMS_LOG_DEBUG << "min_pings_for_track: " << mintrack;
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
    
    //-------------------------------------------------------------------------
	// SETUP TRACKING
    
	vector<TrackedObject> active_tracks;   // tracks still being updated
	vector<int> active_id;                 // unique id for each track
    //	vector<TrackedObject> completed_tracks;// completed tracks (no updates)
    //	vector<int> completed_id;              // unique id
	int next_id = 0;
	long completed_tracks_count = 0;
	
    // TODO: Make the output a class, so can implement as csv
    //       file or database or whatever.
 	// output file for saving track data
 	time_t rawtime;
    -   time(&rawtime);
    char timestr[16]; // YYYYMMDD-hhmmss
 	strftime(timestr, 16, "%Y%m%d-%H%M%S", gmtime(&rawtime));
    fs::path outtxtfilepath("nims_tracks-" + string(timestr) + ".csv");
	NIMS_LOG_DEBUG << "saving track data to " << outtxtfilepath;
	ofstream outtxtfile(outtxtfilepath.string().c_str(),ios::out | ios::binary);
	if (!outtxtfile.is_open())
	{
		NIMS_LOG_ERROR << "Error opening output file: " << outtxtfilepath;
		return (-1);
	}
	print_attribute_labels(outtxtfile);
	outtxtfile << endl;
    
    // Create message queues
    mqd_t mq_ui = CreateMessageQueue(mq_ui_name, sizeof(TracksMessage), true); // non-blocking
    mqd_t mq_socket = CreateMessageQueue(mq_socket_name, sizeof(TracksMessage), true);
    mqd_t mq_det = CreateMessageQueue(MQ_DETECTOR_TRACKER_QUEUE, sizeof(DetectionMessage));
    if (mq_det < 0) 
    {
        NIMS_LOG_ERROR << "Error creating MQ_DETECTOR_TRACKER_QUEUE";
        return -1;
    }
    NIMS_LOG_DEBUG << "message queues created";

    //-------------------------------------------------------------------------
    // MAIN LOOP
    while (true)
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
                detected_positions_y(f) = msg_det.detections[f].center_y;
                detected_positions_x(f) = msg_det.detections[f].center_x;
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
                    Point2f pnt = active_tracks[a].predict(msg_det.ping_num);
                    predicted_positions_x(a) = pnt.x;
                    predicted_positions_y(a) = pnt.y;
                    
                    NIMS_LOG_DEBUG << "   ID " << active_id[a]
                    << ": predicted object at " << pnt.x
                    << ", " << pnt.y << ", last updated in frame "
                    << active_tracks[a].last_epoch();
                    
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
                    
                    NIMS_LOG_DEBUG << "ID " << active_id[min_idx[0]]
                    << ": matched detection " << f;
                    active_tracks[min_idx[0]].update(msg_det.ping_num,
                                                     Point2f(detected_positions_x(f), detected_positions_y(f)));
                    // mark detection as matched
                    detected_not_matched.at<unsigned char>(f) = 0;
                    // mark track as matched
                    active_not_matched.at<unsigned char>(min_idx[0]) = 0;
                    // update UI message
                    
                    
                } // for each detection
                
            } // if active tracks
            
            // create new active tracks for unmatched detections
            for (int f=0; f<n_obj; ++f)
            {
                if ( detected_not_matched.at<unsigned char>(f) )
                {
                    NIMS_LOG_DEBUG  << "ID " << next_id
                    << ": starting new track at "
                    << detected_positions_x(f) << ", "
                    << detected_positions_y(f);
                    
                    TrackedObject newfish(Point2f(detected_positions_x(f),
                                                  detected_positions_y(f)),
                                          cv::noArray(), msg_det.ping_num,
                                          process_noise, measurement_noise);
                    active_tracks.push_back(newfish);
                    active_id.push_back(next_id);
                    
                    // update UI message
                    /*
                     msg_ui.detections[f].center_range = detected_positions_y(f);
                     msg_ui.detections[f].center_beam = detected_positions_x(f);
                     msg_ui.detections[f].track_id = next_id;
                     msg_ui.detections[f].new_track = true;
                     */
                    ++next_id;
                }
            }
            
            // send UI and socket messages
            TracksMessage msg_trks(msg_det.ping_num, active_tracks.size());
            mq_send(mq_ui, (const char *)&msg_trks, sizeof(msg_trks), 0); // non-blocking
            mq_send(mq_socket, (const char *)&msg_trks, sizeof(msg_trks), 0);
            NIMS_LOG_DEBUG << "sent UI message (" << sizeof(msg_trks)
            << " bytes); ping_num = " << msg_trks.ping_num << "; num_tracks = "
            << msg_trks.num_tracks;
             
        } // if detections
        
        // de-activate tracks
        int idx = 0;
        while (idx < active_tracks.size())
        {
            if ( msg_det.ping_num - active_tracks[idx].last_epoch() > maxgap )
            {
                NIMS_LOG_DEBUG << "ID " << active_id[idx]
                << ": deactivated, last updated in frame "
                << active_tracks[idx].last_epoch();
                // save if greater than minimum length
                if ( active_tracks[idx].track_length() > mintrack )
                {
                    //completed_tracks.push_back(active_tracks[idx]);
                    //completed_id.push_back(active_id[idx]);
                    // save completed track to disk
                    ++completed_tracks_count;
                    TrackAttributes attr;
                   // active_tracks[idx].get_track_attributes(
                   //                                         start_range,range_step,
                   //                                         start_bearing,bearing_step,
                   //                                         ping_rate, attr);
                    attr.track_id = completed_tracks_count;
                    outtxtfile << attr << endl;
                    
                    NIMS_LOG_DEBUG << "ID " << active_id[idx]
                    << ": saved as completed track";
                }
                active_tracks.erase(active_tracks.begin()+idx);
                active_id.erase(active_id.begin()+idx);
            }
            else
            {
                ++idx;
            }
        } // for each active
        
        
        if (sigint_received) {
            NIMS_LOG_WARNING << "tracker end of loop: exiting due to SIGINT";
            break;
        }
        
    } // main loop
    
    //-------------------------------------------------------------------------
	// CLEANUP
    // close message queues
    mq_close(mq_ui);
    mq_close(mq_socket);
    //mq_unlink(mq_ui_name.c_str());
    //mq_unlink(mq_socket_name.c_str());
    
    // close output files
    outtxtfile.close();
    
	cout << endl << "Ending " << argv[0] << endl;
    return 0;
}
