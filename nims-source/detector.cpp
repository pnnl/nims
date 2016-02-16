/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  filename.cpp
 *
 *  Created by Firstname Lastname on mm/dd/yyyy.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
#include <iostream> // cout, cin, cerr
#include <string>   // for strings


#include <boost/filesystem.hpp>
#include <opencv2/opencv.hpp>
#include "yaml-cpp/yaml.h"

#include "nims_ipc.h" // NIMS signal handling, queues, shared mem
#include "log.h"      // NIMS logging
#include "frame_buffer.h"   // sensor data
#include "detections.h"  // detection message
#include "pixelgroup.h"     // connected components

 using namespace std;
 using namespace boost;
 namespace fs = boost::filesystem;
 using namespace cv;

// Generate the mapping from beam-range to x-y for display
#define PI 3.14159265
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

struct Background 
{
    int N; // number of frames for moving window
    int dim_sizes[2]; // size of each frame data dimension
    int total_samples; // number of elements in frame data
    int cv_type;       // openCV code for frame data type
    int oldest_frame; // index of oldest frame in moving window
    Mat pings; // moving window
    Mat ping_mean;
    Mat ping_stdv;
};

int initialize_background(Background& bg, int num_frames, FrameBufferReader& fb)
{
    bg.N = num_frames;
    bg.oldest_frame = 0;

    // Initialize the moving window.
        Frame next_ping;
    if ( fb.GetNextFrame(&next_ping)==-1 )
        {
            NIMS_LOG_ERROR << "Error getting ping for initial moving average.";
            return -1;
        }
    // NOTE:  data is stored transposed
    bg.dim_sizes[0] = (int)next_ping.header.num_beams;
    bg.dim_sizes[1] = (int)next_ping.header.num_samples;
    bg.total_samples = next_ping.header.num_beams*next_ping.header.num_samples;

    // the framedata_t (frame_buffer.h) is either float or double
    bg.cv_type = sizeof(framedata_t)==4 ? CV_32FC1 : CV_64FC1;
    bg.pings.create(bg.N, bg.total_samples, bg.cv_type);
    for (int k=0; k<bg.N; ++k)
    {
        if ( fb.GetNextFrame(&next_ping)==-1 )
        {
            NIMS_LOG_ERROR << "Error getting ping for initial moving average.";
            return -1;
        }
            // Create a cv::Mat wrapper for the ping data
        Mat ping_data(2,bg.dim_sizes,bg.cv_type,next_ping.data_ptr());
        ping_data.reshape(0,1).copyTo(bg.pings.row(k));
    }
    reduce(bg.pings, bg.ping_mean, 0, CV_REDUCE_AVG);

        // Initialize the moving std dev.
        // v = sum( (x(k) - u)^2 ) / (N-1)
        // s = sqrt( v )
    Mat sq_diff;
    pow(bg.pings - repeat(bg.ping_mean, bg.N, 1),2.0f,sq_diff);
    Mat ping_var;
    reduce(sq_diff, ping_var, 0, CV_REDUCE_SUM);
    ping_var = ping_var/(bg.N-1);
    sqrt(ping_var, bg.ping_stdv);

} // initialize_background


int update_background(Background& bg, const Frame& new_ping)
{
    // replace oldest frame with new one
    Mat ping_data(2,bg.dim_sizes,bg.cv_type,new_ping.data_ptr());
    ping_data.reshape(0,1).copyTo(bg.pings.row(bg.oldest_frame));
    ++bg.oldest_frame;
    bg.oldest_frame %= bg.N; // wrap around from N-1 to 0

    // calc new mean
    reduce(bg.pings, bg.ping_mean, 0, CV_REDUCE_AVG);

    // calc new std dev
        // v = sum( (x(k) - u)^2 ) / (N-1)
        // s = sqrt( v )
    Mat sq_diff;
    pow(bg.pings - repeat(bg.ping_mean, bg.N, 1),2.0f,sq_diff);
    Mat ping_var;
    reduce(sq_diff, ping_var, 0, CV_REDUCE_SUM);
    ping_var = ping_var/(bg.N-1);
    sqrt(ping_var, bg.ping_stdv);

    return 0;
} // update_background


int detect_objects(const Background& bg, const Frame& ping, 
    float thresh_stdevs, int min_size, vector<Detection>& detections)
{
    detections.clear();
    Mat ping_data(1,bg.total_samples,bg.cv_type,ping.data_ptr());
    Mat foregroundMask = ((ping_data - bg.ping_mean) / bg.ping_stdv) > thresh_stdevs;
    int nz = countNonZero(foregroundMask);
    NIMS_LOG_DEBUG << "number of samples above threshold: "<< nz << " ("
                   << ceil( ((float)nz/bg.total_samples) * 100.0 ) << "%)";
    if (nz > 0)
    {
        PixelGrouping objects;
        group_pixels(foregroundMask, min_size, objects);
        int n_obj = objects.size();
        NIMS_LOG_DEBUG << "number of detected objects: " << n_obj;
        for (int k=0; k<n_obj; ++k)
        {
            // convert pixel grouping to detections
            detections.push_back(Detection(objects[k]));
        }

    }
    return detections.size();
} // detect_objects


///////////////////////////////////////////////////////////////////////////////
//  MAIN
///////////////////////////////////////////////////////////////////////////////
int main (int argc, char * argv[]) {

    string cfgpath, log_level;
    if ( parse_command_line(argc, argv, cfgpath, log_level) != 0 ) return -1;
    setup_logging(string(basename(argv[0])), cfgpath, log_level);
    setup_signal_handling();
    
    // READ CONFIG FILE
    string fb_name; // frame buffer
    int N;  // number of pings for moving average
    float thresh_stdevs = 3.0;
    int min_size = 1;
    
    try
    {
        YAML::Node config = YAML::LoadFile(cfgpath); // throws exception if bad path
        fb_name = config["FRAMEBUFFER_NAME"].as<string>();
        YAML::Node params = config["TRACKER"];
        N             = params["num_pings_for_moving_avg"].as<int>();
        NIMS_LOG_DEBUG << "num_pings_for_moving_avg = " << N;
       thresh_stdevs = params["threshold_in_stdevs"].as<float>();
        NIMS_LOG_DEBUG << "threshold_in_stdevs = " << thresh_stdevs;
        min_size      = params["min_target_size"].as<int>();
       NIMS_LOG_DEBUG << "min_target_size = " << min_size;
 }
    catch( const std::exception& e )
    {
        NIMS_LOG_ERROR << "Error reading config file: " << cfgpath << endl;
        NIMS_LOG_ERROR << e.what() << endl;
        return -1;
    }
    
     
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
    NIMS_LOG_DEBUG << endl << "Starting " << argv[0] << endl;
    SubprocessCheckin(getpid()); // Synchronize with main NIMS process.

    //-------------------------------------------------------------------------
    // INITIALIZE MOVING AVG, STDEV
    FrameBufferReader fb(fb_name);
    if ( -1 == fb.Connect() )
    {
        NIMS_LOG_ERROR << "Error connecting to framebuffer.";
        return -1;
    }
    
    // Initialize the moving average.
        NIMS_LOG_DEBUG << "Initializing moving average and std dev";
        Background bg;
        if ( initialize_background(bg, N, fb)<0 )
        {
            NIMS_LOG_ERROR << "Error initializing background!";
        }
        NIMS_LOG_DEBUG << "Moving average and std dev initialized";

    // Get one ping to get header info.
    Frame next_ping;
    fb.GetNextFrame(&next_ping);
    Mat map_x, map_y;
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
    //Mat imc(ping_mean.size(), CV_8UC3); // used for VIEW
    
    mqd_t mq_det = CreateMessageQueue(MQ_DETECTOR_TRACKER_QUEUE, 
        sizeof(DetectionMessage), true); // non-blocking
    
    //-------------------------------------------------------------------------
    // MAIN LOOP
    
    int frame_index = -1;
    while ( (frame_index = fb.GetNextFrame(&next_ping)) != -1)
    {
        if (sigint_received) {
            NIMS_LOG_WARNING << "exiting due to SIGINT";
            break;
        }

        // Update background
        NIMS_LOG_DEBUG << "Updating mean background";
        update_background(bg, next_ping);

        double min_val,max_val;
        minMaxIdx(bg.pings.row(bg.N-1), &min_val, &max_val);
        NIMS_LOG_DEBUG << "ping data values from " << min_val << " to " << max_val;
        minMaxIdx(bg.ping_mean, &min_val, &max_val);
        NIMS_LOG_DEBUG << "mean background values from " << min_val << " to " << max_val;
        minMaxIdx(bg.ping_stdv, &min_val, &max_val);
        NIMS_LOG_DEBUG << "std dev background values from " << min_val << " to " << max_val;

        if (VIEW)
        {
            // create 3-channel color image for viewing/saving
            // ping data values are float from 0 to whatever
            // need to scale from 0 to 1.
            //Mat imgray;
           // Mat(ping_data/50.0).convertTo(imgray, CV_8U, 255, 0);
            // convert grayscale to color image
            //cvtColor(imgray, imc, CV_GRAY2RGB);
        }
            
        // Detect objects
        NIMS_LOG_DEBUG << "Detecting objects";

        DetectionMessage msg_det(frame_index, next_ping.header.ping_num, 0);

        vector<Detection> detections;
        int n_obj = detect_objects(bg, next_ping, thresh_stdevs, min_size, detections);  
        NIMS_LOG_DEBUG << "number of detected objects: " << n_obj;

        // guard against tracking a lot of noise            
        if (n_obj > MAX_DETECTIONS_PER_FRAME) // from detections.h
        {
            NIMS_LOG_WARNING << "Too many false positives, not detecting";
        }
        else if (n_obj > 0)
        {
            msg_det.num_detections = n_obj;
            std::copy(detections.begin(), detections.end(), msg_det.detections);     
       }
        mq_send(mq_det, (const char *)&msg_det, sizeof(msg_det), 0); // non-blocking
       
       
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
        /*
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
*/
    } // while getting frames
        
    cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
