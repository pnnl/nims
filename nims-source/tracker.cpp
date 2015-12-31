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


#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "yaml-cpp/yaml.h"

#include "log.h"            // NIMS_LOG_* macros
#include "queues.h"         // SubprocessCheckin()
#include "nims_ipc.h"       // shared mem
#include "frame_buffer.h"   // sensor data
#include "tracked_object.h" // tracking
#include "pixelgroup.h"     // connected components
#include "detections.h"     // detection message for UI

using namespace std;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;
using namespace cv;

#define PI 3.14159265

// Generate the mapping from beam-range to x-y for display
int PingImagePolarToCart(const FrameHeader &hdr, OutputArray _map_x, OutputArray _map_y)
{
    // range bin resolution in meters per pixel
    float rng_step = (hdr.range_max_m - hdr.range_min_m)/(hdr.num_samples - 1);
    NIMS_LOG_DEBUG << "PingImagePolarToCart: range resolution is " << rng_step;
    float y2 = hdr.range_max_m; // y coordinate of first row of image pixels
    
    float beam_step = hdr.beam_angles_deg[1] - hdr.beam_angles_deg[0];
    double theta1 = (double)hdr.beam_angles_deg[0] * PI/180.0;
    double theta2 = (double)hdr.beam_angles_deg[hdr.num_beams-1] * PI/180.0;
    float x1 = hdr.range_max_m*sin(theta1);
    float x2 = hdr.range_max_m*sin(theta2);
    NIMS_LOG_DEBUG << "PingImagePolarToCart: beam angles from " << hdr.beam_angles_deg[0]
    << " to " << hdr.beam_angles_deg[hdr.num_beams-1];
    NIMS_LOG_DEBUG << "PingImagePolarToCart: beam angle step is " << beam_step;
    NIMS_LOG_DEBUG << "PingImagePolarToCart: x1 = " << x1 << ", x2 = " << x2;
    
    // size of image
    int nrows = hdr.num_samples;
    int ncols = (x2 - x1) / rng_step;
    NIMS_LOG_DEBUG << "PingImagePolarToCart: image size is " << nrows << " x " << ncols;
    
    _map_x.create(nrows, ncols, CV_32FC1);
    Mat_<float> map_x = _map_x.getMat();
    _map_y.create(nrows, ncols, CV_32FC1);
    Mat_<float> map_y = _map_y.getMat();
    
    // initialize map to 0's
    map_x = Mat::zeros(nrows, ncols, CV_32FC1);
    map_y = Mat::zeros(nrows, ncols, CV_32FC1);
    
    // each row is a range
    for (int m=0; m<nrows; ++m)
    {
        // each column is an angle
        for (int n=0; n<ncols; ++n)
        {
            // convert row, col to x,y coordinates, centered at origin
            float x = x1 + n*rng_step;
            float y = y2 - m*rng_step;
            // convert x,y to beam-range
            float rng = sqrt(pow(x,2)+pow(y,2));
            float beam_deg = 90 - (atan2(y,x)*(180/PI));
            // convert beam-range to indices
            float i_rng = (rng - hdr.range_min_m) / rng_step;
            float i_beam = (beam_deg - hdr.beam_angles_deg[0]) / beam_step;
            if (i_rng >=0 && i_rng < hdr.num_samples
                && i_beam >= 0 && i_beam < hdr.num_beams)
            {
                map_x(m,n) = i_beam;
                map_y(m,n) = i_rng;
            }
        }
    }
}


/*
 int MakePingImage(InputArray _ping, OutputArray _img)
 {
 Mat ping = _ping.getMat();
 
 // check for single channel gray scale
 
 Mat img1;
 remap(ping.t(), img1, map_x, map_y,
 INTER_LINEAR, BORDER_CONSTANT, Scalar(0,0,0));
 Mat im_out;
 img1.convertTo(im_out, CV_16U, 65535, 0);
 
 return 0;
 }
 */

// Tracker creates the message queue to pass the detections from each
// ping (frame) to the User Interface (UI).  The UI opens the queue
// and, if messages are pending, receives messages until the ping number
// of the last message matches the ping number of the last frame
// that the UI received.  The queue will persist, so if the tracker
// dies and restarts it will write to this same queue (I think.)
mqd_t CreateTrackerMessageQueue(size_t message_size, const string &name)
{
    struct mq_attr attr;
    memset(&attr, 0, sizeof(struct mq_attr));
    
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = message_size;
    attr.mq_flags = O_NONBLOCK;  // non-blocking, don't need to sync 
    
    const int opts = O_CREAT | O_WRONLY;
    const int mode = S_IRUSR | S_IWUSR;
    
    NIMS_LOG_DEBUG << "creating tracker message queue " << name 
                   << " with msg size " << message_size;
    mqd_t mqd = mq_open(name.c_str(), opts, mode, &attr);
    if (-1 == mqd)
        nims_perror("CreateMessageQueue mq_open()");
    
    return mqd;
}


int main (int argc, char * argv[]) {
	time_t rawtime;
	time(&rawtime);
	
	//--------------------------------------------------------------------------
    // PARSE COMMAND LINE
	//
	po::options_description desc;
	desc.add_options()
	("help",                                                "print help message")
	("cfg,c", po::value<string>()->default_value( "./config.yaml" ),
     "path to config file; default is ./config.yaml")
	("log,l", po::value<string>()->default_value("debug"), "debug|warning|error")
	;
	po::variables_map options;
    try
    {
        po::store( po::parse_command_line( argc, argv, desc ), options );
    }
    catch( const std::exception& e )
    {
        NIMS_LOG_ERROR << "Sorry, couldn't parse that: " << e.what();
        NIMS_LOG_ERROR << desc;
        return -1;
    }
	
	po::notify( options );
	
    if( options.count( "help" ) > 0 )
    {
        NIMS_LOG_ERROR << desc;
        return 0;
    }
    
    string cfgpath = options["cfg"].as<string>();
    setup_logging(string(basename(argv[0])), cfgpath, options["log"].as<string>());
    
    // For testing
    bool VIEW = true; // display new ping images
    const char *WIN_PING="Ping Image";
    const char *WIN_MEAN="Mean Intensity Image";
    if (VIEW)
    {
        //namedWindow(WIN_PING, CV_WINDOW_NORMAL );
        //namedWindow(WIN_MEAN, CV_WINDOW_NORMAL );
    }
    //int disp_ms = 100; // time duration of window display
    
	//--------------------------------------------------------------------------
	// DO STUFF
    NIMS_LOG_DEBUG << "Starting " << argv[0];
    
    // Get parameters from config file.
	string fb_name;
    string mq_ui_name;
	 string mq_socket_name;
    int N;
    int maxgap = 3; // maximum gap in frames allowed within a track
    int mintrack = 10; // minimum number of steps to be considered a track
    float thresh_stdevs = 3.0;
    int min_size = 1;
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
        fb_name = config["FRAMEBUFFER_NAME"].as<string>();
        mq_ui_name = "/" + config["TRACKER_NAME"].as<string>();
         mq_socket_name = "/" + config["TRACKER_SOCKET_NAME"].as<string>();
        YAML::Node params = config["TRACKER"];
        N                 = params["num_pings_for_moving_avg"].as<int>();
        maxgap            = params["max_ping_gap_in_track"].as<int>();
        mintrack          = params["min_pings_for_track"].as<int>();
        thresh_stdevs     = params["threshold_in_stdevs"].as<float>();
        min_size          = params["min_target_size"].as<int>();
        process_noise     = params["process_noise"].as<float>();
        measurement_noise = params["measurement_noise"].as<float>();
        pred_err_max      = params["max_prediction_error"].as<int>();
    }
    catch( const std::exception& e )
    {
        NIMS_LOG_ERROR << "Error reading config file: " << cfgpath << endl;
        NIMS_LOG_ERROR << e.what() << endl;
        NIMS_LOG_ERROR << desc << endl;
        return -1;
    }
    
    NIMS_LOG_DEBUG << "num_pings_for_moving_avg = " << N;
    NIMS_LOG_DEBUG << "max_ping_gap_in_track = " << maxgap;
    NIMS_LOG_DEBUG << "min_pings_for_track = " << mintrack;
    NIMS_LOG_DEBUG << "threshold_in_stdevs = " << thresh_stdevs;
    NIMS_LOG_DEBUG << "process_noise = " << process_noise;
    NIMS_LOG_DEBUG << "measurement_noise = " << measurement_noise;
    NIMS_LOG_DEBUG << "max_prediction_error = " << pred_err_max;
    
    //-------------------------------------------------------------------------
	// SETUP TRACKING
    
	vector<TrackedObject> active_tracks;   // tracks still being updated
	vector<int> active_id;                 // unique id for each track
    //	vector<TrackedObject> completed_tracks;// completed tracks (no updates)
    //	vector<int> completed_id;              // unique id
	int next_id = 0;
	long completed_tracks_count = 0;
	
 	// output file for saving track data
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
    
    FrameBufferReader fb(fb_name);
    if ( -1 == fb.Connect() )
    {
        NIMS_LOG_ERROR << "Error connecting to framebuffer.";
        return -1;
    }
    
    mqd_t mq_ui = CreateTrackerMessageQueue(sizeof(DetectionMessage), mq_ui_name);
    mqd_t mq_socket = CreateTrackerMessageQueue(sizeof(DetectionMessage), mq_socket_name);
    
    // check in before calling GetNextFrame, to avoid timeout in the
    // NIMS parent process (and subsequent termination).
    SubprocessCheckin(getpid());
    
    //-------------------------------------------------------------------------
	// INITIALIZE MOVING AVG, STDEV
    
    // Get one ping to get the dimensions of the ping data.
    Frame next_ping;
    
    if ( fb.GetNextFrame(&next_ping)==-1 )
    {
        NIMS_LOG_ERROR << "Error getting first ping for moving average.";
        return -1;
    }
    
    // NOTE:  data is stored transposed
    int dim_sizes[] = {(int)next_ping.header.num_beams,
        (int)next_ping.header.num_samples};
    
    // the framedata_t (frame_buffer.h) is either float or double
    int cv_type = sizeof(framedata_t)==4 ? CV_32FC1 : CV_64FC1;
    size_t total_samples = next_ping.header.num_samples*next_ping.header.num_beams;
    
    NIMS_LOG_DEBUG << "   num samples = " << dim_sizes[0]
                   << ", num beams = " << dim_sizes[1];
    NIMS_LOG_DEBUG << "   bytes per sample = " << sizeof(framedata_t);
    NIMS_LOG_DEBUG << "   total samples = " << total_samples;
    
    Mat map_x;
    Mat map_y;
    if (VIEW)
    {
        PingImagePolarToCart(next_ping.header, map_x, map_y);
        double min_beam, min_rng, max_beam, max_rng;
        minMaxIdx(map_x.reshape(0,1), &min_beam, &max_beam);
        minMaxIdx(map_y.reshape(0,1), &min_rng, &max_rng);
        
        NIMS_LOG_DEBUG << "image mapping:  beam index is from "
        << min_beam << " to " << max_beam;
        NIMS_LOG_DEBUG << "image mapping:  range index is from "
        << min_rng << " to " << max_rng;
    }
    
    NIMS_LOG_DEBUG << "initializing moving average and std dev";
    // TODO:  would be good to free this when done intitializing
    Mat_<framedata_t> pings(N, total_samples);
    
    // Initialize the moving average.
    Mat ping_mean = Mat::zeros(dim_sizes[0],dim_sizes[1],cv_type);
    double temp, ping_max_mean = 0.0;
    for (int k=0; k<N; ++k)
    {
        if ( fb.GetNextFrame(&next_ping)==-1 )
        {
            NIMS_LOG_ERROR << argv[0]
            << "Error getting ping for initial moving average.";
            return -1;
        }
        // Create a cv::Mat wrapper for the ping data
        Mat ping_data(2,dim_sizes,cv_type,next_ping.data_ptr());
        ping_data.reshape(0,1).copyTo(pings.row(k));
        ping_mean += ping_data;
        minMaxIdx(ping_data, nullptr, &temp);
        ping_max_mean += temp;
    }
    ping_mean = ping_mean/N;
    ping_max_mean = ping_max_mean/N;
    
    double min_val,max_val, min_val1,max_val1, min_val2,max_val2;
    minMaxIdx(pings.row(N-1), &min_val, &max_val);
    NIMS_LOG_DEBUG << "ping data values from " << min_val
    << " to " << max_val;
    // NOTE:  Values in Dolphin(0) are from 0 to 13.0686
    minMaxIdx(ping_mean.reshape(0,1), &min_val1, &max_val1);
    NIMS_LOG_DEBUG << "mean background values from " << min_val1
    << " to " << max_val1;
    // Initialize the moving std dev.
    // v = sum( (x(k) - u)^2 ) / (N-1)
    // s = sqrt( v )
    Mat ping_var = Mat::zeros(dim_sizes[0],dim_sizes[1],cv_type);
    for (int k=0; k<N; ++k)
    {
        Mat sqrDiff;
        pow(ping_mean - pings.row(k).reshape(0,dim_sizes[0]),2.0f,sqrDiff);
        ping_var += sqrDiff;
    }
    ping_var = ping_var/(N-1);
    Mat ping_stdv;
    sqrt(ping_var, ping_stdv);
    
    minMaxIdx(ping_stdv.reshape(0,1), &min_val2, &max_val2);
    NIMS_LOG_DEBUG << "background std. dev. values from " << min_val2
    << " to " << max_val2;
    
    NIMS_LOG_DEBUG << "moving average and std dev initialized";
    
    // NOTE:  Assumes that these do not change.  If sonar mode/config is
    //        changed, then tracker should be restarted because any
    //        active tracks will be messed up.
    float start_range = next_ping.header.range_min_m;
    float range_step = (next_ping.header.range_max_m - next_ping.header.range_min_m)
    / (next_ping.header.num_samples - 1);
    float start_bearing = next_ping.header.beam_angles_deg[0];
    float bearing_step = next_ping.header.beam_angles_deg[1] -
    next_ping.header.beam_angles_deg[0];
    float ping_rate = next_ping.header.pulserep_hz;
    Mat imc(ping_mean.size(), CV_8UC3); // used for VIEW
    //-------------------------------------------------------------------------
	// MAIN LOOP
    
    int frame_index = -1;
    while ( (frame_index = fb.GetNextFrame(&next_ping)) != -1)
    {
        NIMS_LOG_DEBUG << "got frame " << frame_index << endl << next_ping.header;
        Mat ping_data(2,dim_sizes,cv_type,next_ping.data_ptr());
        minMaxIdx(ping_data, &min_val, &max_val);
        NIMS_LOG_DEBUG << "frame values from " << min_val << " to " << max_val;
        
         // update background
        // http://www.johndcook.com/blog/standard_deviation/
        NIMS_LOG_DEBUG << "updating mean background";
        // u(k) = u(k-1) + (x - u(k-1))/N
        Mat new_mean = ping_mean + (ping_data - ping_mean)/N;
        // s2(k) = s2(k-1) + (x - u(k-1))(x - u(k))/(N-1)
        ping_var = ping_var +
        (ping_data - ping_mean).mul(ping_data - new_mean)/(N-1);
        sqrt(ping_var, ping_stdv);
        ping_mean = new_mean;
        
        ping_max_mean = ping_max_mean + (max_val - ping_max_mean)/N;
        NIMS_LOG_DEBUG << "average ping max value is " << ping_max_mean;
        
        // This is optional, could be removed for optimization.
        minMaxIdx(ping_mean, &min_val1, &max_val1);
        minMaxIdx(ping_stdv, &min_val2, &max_val2);
        NIMS_LOG_DEBUG << "background mean from " << min_val1 << " to " << max_val1
                       << ", std. dev. from " << min_val2 << " to " << max_val2;
        
       if (VIEW)
        {
            // create 3-channel color image for viewing/saving
            // ping data values are float from 0 to whatever
            // need to scale from 0 to 1.
            Mat imgray;
            Mat(ping_data/50.0).convertTo(imgray, CV_8U, 255, 0);
            // convert grayscale to color image
            
            cvtColor(imgray, imc, CV_GRAY2RGB);
            /*
            double min_valc, max_valc;
            minMaxIdx(imc.reshape(1), &min_valc, &max_valc);
            NIMS_LOG_DEBUG << "imc values from " << min_valc << " to " << max_valc;
            */
           
            
         }
        // construct detection message for UI
        // TODO:  Make max shared mem objects a constant at beginning of code.
        // TODO:  This could mess up the consumer, overwriting old shared mem.
        
        DetectionMessage msg_ui(next_ping.header.ping_num, 0);
       // TODO:  Debug this.  As is, causes tracker to hang.
        /*
        string shared_name = "mean_bg-" + to_string(next_ping.header.ping_num%10);
        size_t data_size = ping_mean.step[0] * ping_mean.rows;
        int ret = share_data(shared_name, 0, nullptr, data_size, (char*)ping_mean.ptr(0) );
        if (ret != -1)
        {
            msg_ui.background_data_size = data_size;
            strcpy(msg_ui.background_shm_name, shared_name.c_str());
        }
        */
        // detect targets
        NIMS_LOG_DEBUG << "detecting targets";
        Mat foregroundMask = ((ping_data - ping_mean) / ping_stdv) > thresh_stdevs;
        int nz = countNonZero(foregroundMask);
        NIMS_LOG_DEBUG << "number of samples above threshold: "
        << nz << " ("
        << ceil((float)nz/total_samples * 100.0) << "%)";
        
        if (nz > 0)
        {
            PixelGrouping objects;
            group_pixels(foregroundMask, min_size, objects);
            int n_obj = objects.size();
            NIMS_LOG_DEBUG << "number of detected objects: " << n_obj;
            /*
            if (VIEW)
            {
                vector< vector<Point> > contours;
                findContours(foregroundMask, contours, noArray(),
                             CV_RETR_CCOMP, CV_CHAIN_APPROX_NONE);
                // draw all contours in red
                drawContours(imc, contours, -1, Scalar(0,0,255));
            }
            */
            // guard against tracking a lot of noise
            if (n_obj > MAX_DETECTIONS_PER_FRAME) // from detections.h
            {
                NIMS_LOG_WARNING << "Too many false positives, not tracking";
            }
            else if (n_obj > 0)
            {
                msg_ui.num_detections = n_obj;
                // update tracks
                NIMS_LOG_DEBUG << "tracking detected objects";
                Mat_<float> detected_positions_x(1,n_obj,0.0);
                Mat_<float> detected_positions_y(1,n_obj,0.0);
			    for (int f=0; f<n_obj; ++f)
			    {
                    Rect bb = objects.bounding_box(f);
                    NIMS_LOG_DEBUG << "   detected target at "
                    << bb.x << ", " << bb.y
                    << " with " << objects.size(f)
                    << " pixels" << endl;
                    detected_positions_x(f) = bb.x;
                    detected_positions_y(f) = bb.y;
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
                        Point2f pnt = active_tracks[a].predict(frame_index);
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
                        active_tracks[min_idx[0]].update(frame_index,
                                                         Point2f(detected_positions_x(f), detected_positions_y(f)));
                        // mark detection as matched
                        detected_not_matched.at<unsigned char>(f) = 0;
                        // mark track as matched
                        active_not_matched.at<unsigned char>(min_idx[0]) = 0;
                        // update UI message
                        
                        msg_ui.detections[f].center_range = detected_positions_y(f);
                        msg_ui.detections[f].center_beam = detected_positions_x(f);
                        msg_ui.detections[f].track_id = active_id[min_idx[0]];
                        msg_ui.detections[f].new_track = false;
                        
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
                                              cv::noArray(), frame_index,
                                              process_noise, measurement_noise);
                        active_tracks.push_back(newfish);
                        active_id.push_back(next_id);
                        
                        // update UI message
                        
                        msg_ui.detections[f].center_range = detected_positions_y(f);
                        msg_ui.detections[f].center_beam = detected_positions_x(f);
                        msg_ui.detections[f].track_id = next_id;
                        msg_ui.detections[f].new_track = true;
                        
                        ++next_id;
                    }
                }
            } // if not too many false detections
            
        } // nz > 0
        
        // send UI and socket messages
        
        mq_send(mq_ui, (const char *)&msg_ui, sizeof(msg_ui), 0); // non-blocking
        mq_send(mq_socket, (const char *)&msg_ui, sizeof(msg_ui), 0);
        NIMS_LOG_DEBUG << "sent UI message (" << sizeof(msg_ui) 
                       << " bytes); ping_num = " << msg_ui.ping_num << "; num_detect = " 
                       << msg_ui.num_detections;
        
        // de-activate tracks
        int idx = 0;
        while (idx < active_tracks.size())
        {
            if ( frame_index - active_tracks[idx].last_epoch() > maxgap )
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
                    active_tracks[idx].get_track_attributes(
                                                            start_range,range_step,
                                                            start_bearing,bearing_step,
                                                            ping_rate, attr);
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
        
        if (VIEW)
        {
            Mat im_out;
            remap(imc.t(), im_out, map_x, map_y,
                  INTER_LINEAR, BORDER_CONSTANT, Scalar(0,0,0));
            stringstream pngfilepath;
            pngfilepath <<  "ping-" << frame_index % 20 << ".png";
            //imwrite(pngfilepath.str(), im_out);
            imwrite(pngfilepath.str(), imc);
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
