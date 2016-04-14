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
 #include <fstream> // ofstream
#include <string>   // for strings


#include <boost/filesystem.hpp>
#include <opencv2/opencv.hpp>
#include "yaml-cpp/yaml.h"

#include "nims_ipc.h" // NIMS signal handling, queues, shared mem
#include "log.h"      // NIMS logging
#include "frame_buffer.h"   // sensor data
#include "detections.h"  // detection message
#include "pixelgroup.h"     // connected components
#include <math.h> // M_PI

 using namespace std;
 using namespace boost;
 namespace fs = boost::filesystem;
 using namespace cv;

bool TEST=false;

template<typename T> void write_mat_to_file(InputArray _mat, fs::path outfilepath)
{
    Mat m = _mat.getMat();
    ofstream ofs( outfilepath.string().c_str() ); 

    Size size = m.size();
    for (int i=0; i<size.height; ++i) // each row
    {
        ofs << m.at<T>(i,0);
        for (int j=1; j<size.width; ++j) // each column
        {
            ofs << "," << m.at<T>(i,j);
        }
        ofs << endl;
    }
    ofs.close();
}

// Generate the mapping from beam-range to x-y for display
int PingImagePolarToCart(const FrameHeader &hdr, OutputArray _map_x, OutputArray _map_y)
 {
    // range bin resolution in meters per pixel
    float rng_step = (hdr.range_max_m - hdr.range_min_m)/(hdr.num_samples - 1);
    NIMS_LOG_DEBUG << "PingImagePolarToCart: range resolution is " << rng_step;
    float y2 = hdr.range_max_m; // y coordinate of first row of image pixels
    
    float beam_step = hdr.beam_angles_deg[1] - hdr.beam_angles_deg[0];
    double theta1 = (double)hdr.beam_angles_deg[0] * M_PI/180.0;
    double theta2 = (double)hdr.beam_angles_deg[hdr.num_beams-1] * M_PI/180.0;
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
            float beam_deg = 90 - (atan2(y,x)*(180/M_PI));
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

int initialize_background(Background& bg, float bg_secs, FrameBufferReader& fb)
{
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
    bg.N = (int)(next_ping.header.pulserep_hz * bg_secs);
    bg.oldest_frame = 0;


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
    float thresh_stdevs, int min_size, const PixelToWorld& ptw, 
    vector<Detection>& detections)
{
    detections.clear();
    Mat ping_data(1,bg.total_samples,bg.cv_type,ping.data_ptr());
    Mat foregroundMask = ((ping_data - bg.ping_mean) / bg.ping_stdv) > thresh_stdevs;
    int nz = countNonZero(foregroundMask);
    NIMS_LOG_DEBUG << ping.header.ping_num << " number of samples above threshold: "<< nz << " ("
                   << ceil( ((float)nz/bg.total_samples) * 100.0 ) << "%)";
    if (nz > 0)
    {
        float timestamp = ping.header.ping_sec + (float)ping.header.ping_millisec/1000.0;
        PixelGrouping objects;
        // Mat::reshape(nchan, nrows)
        group_pixels(foregroundMask.reshape(0,(int)ping.header.num_samples), min_size, objects);
        int n_obj = objects.size();
        NIMS_LOG_DEBUG << ping.header.ping_num << " number of detected objects: " << n_obj;
            // guard against tracking a lot of noise            
        if (n_obj > MAX_DETECTIONS_PER_FRAME) // from detections.h
        {
            NIMS_LOG_WARNING << "Too many false positives, not detecting";
        }
        else 
        {
            for (int k=0; k<n_obj; ++k)
            {
            // convert pixel grouping to detections
                detections.push_back(Detection(timestamp, objects[k], ptw));
            }
        }

    }
    if (TEST)
    {
        ostringstream ss;
        ss << ping.header.ping_num << "_" << ping.header.ping_sec << "-" << ping.header.ping_millisec;
        write_mat_to_file<framedata_t>(ping_data, string(ss.str() + "_ping.csv"));
        write_mat_to_file<framedata_t>(bg.ping_mean, string(ss.str() + "_mean.csv"));
        write_mat_to_file<framedata_t>(bg.ping_stdv, string(ss.str() + "_stdv.csv"));
        /*
        ofstream ofs( string(ss.str() + "_det.csv").c_str() ); 
        ofs << ptw;
        for (int d=0; d<detections.size(); ++ d)
            ofs << detections[d];
        ofs.close();
        */
    }
    return detections.size();
} // detect_objects


///////////////////////////////////////////////////////////////////////////////
//  MAIN
///////////////////////////////////////////////////////////////////////////////
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
    
    // READ CONFIG FILE
    string fb_name; // frame buffer
    float bg_secs;
    float thresh_stdevs = 3.0;
    int min_size = 1;
    
    try
    {
        YAML::Node config = YAML::LoadFile(cfgpath); // throws exception if bad path
        fb_name = config["FRAMEBUFFER_NAME"].as<string>();
        YAML::Node params = config["DETECTOR"];
        bg_secs             = params["moving_avg_seconds"].as<float>();
        NIMS_LOG_DEBUG << "moving_avg_seconds = " << bg_secs;
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
        if ( initialize_background(bg, bg_secs, fb)<0 )
        {
           // ??? arm: is log and continue the correct behavior?
            NIMS_LOG_ERROR << "Error initializing background!";
            
            // need to exit if we got a SIGINT
            if (sigint_received) {
                NIMS_LOG_WARNING << "exiting due to SIGINT";
                return 0;
            }
        }
        NIMS_LOG_DEBUG << "Moving average and std dev initialized";

    // Get one ping to get header info.
    Frame next_ping;
    fb.GetNextFrame(&next_ping);
    FrameHeader *fh = &next_ping.header; // for notational convenience
    float beam_max = fh->beam_angles_deg[(fh->num_beams-1)];
    float beam_min = fh->beam_angles_deg[0];
    PixelToWorld ptw(fh->range_min_m,
                     beam_min,
                     0.0,
                     (fh->range_max_m - fh->range_min_m)/(fh->num_samples-1),
                     (beam_max - beam_min)/(fh->num_beams-1),
                     0.0);
    Mat map_x, map_y;
    if (VIEW)
    {
        PingImagePolarToCart(next_ping.header, map_x, map_y);
        double min_beam, min_rng, max_beam, max_rng;
        minMaxIdx(map_x, &min_beam, &max_beam);
        minMaxIdx(map_y, &min_rng, &max_rng);

        NIMS_LOG_DEBUG << "image mapping:  beam index is from "
        << min_beam << " to " << max_beam;
        NIMS_LOG_DEBUG << "image mapping:  range index is from "
        << min_rng << " to " << max_rng;
    }
    //Mat imc(ping_mean.size(), CV_8UC3); // used for VIEW
    
    // check before entering loop; GetNextFrame may have been interrupted
    if (sigint_received) {
        NIMS_LOG_WARNING << "exiting due to SIGINT";
        return 0;
    }
    
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
        NIMS_LOG_DEBUG << "got frame " << frame_index;
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
        /*
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
        */    
        // Detect objects
        NIMS_LOG_DEBUG << "Detecting objects";

        vector<Detection> detections;
        int n_obj = detect_objects(bg, next_ping, thresh_stdevs, min_size, ptw, detections);  
        NIMS_LOG_DEBUG << "sending message with " << detections.size() << " detections";
        DetectionMessage msg_det(frame_index, next_ping.header.ping_num, 
            next_ping.header.ping_sec + (float)next_ping.header.ping_millisec/1000.0, detections);
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
