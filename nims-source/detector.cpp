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
#include <boost/program_options.hpp>
#include <opencv2/opencv.hpp>

#include "yaml-cpp/yaml.h"

#include "nims_ipc.h" // NIMS signal handling, queues, shared mem
#include "log.h"      // NIMS logging
#include "frame_buffer.h"   // sensor data

using namespace std;
using namespace boost;
namespace po = boost::program_options;
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


int main (int argc, char * const argv[]) {
	//--------------------------------------------------------------------------
    // PARSE COMMAND LINE
	//
	po::options_description desc;
	desc.add_options()
	("help",                                                    "print help message")
	("cfg,c", po::value<string>()->default_value( "./config.yaml" ), "path to config file; default is ./config.yaml")
	("log,l", po::value<string>()->default_value("debug"), "debug|warning|error")
    ;
	po::variables_map options;
    try
    {
        po::store( po::parse_command_line( argc, argv, desc ), options );
    }
    catch( const std::exception& e )
    {
        cerr << "Sorry, couldn't parse that: " << e.what() << endl;
        cerr << desc << endl;
        return -1;
    }
	
	po::notify( options );
	
    if( options.count( "help" ) > 0 )
    {
        cerr << desc << endl;
        return 0;
    }
	
    string cfgpath = options["cfg"].as<string>();
    setup_logging(string(basename(argv[0])), cfgpath, options["log"].as<string>());
    setup_signal_handling();

  // READ CONFIG FILE
    string fb_name; // frame buffer
    int N;  // number of pings for moving average
    float thresh_stdevs = 3.0;
     try
    {
        YAML::Node config = YAML::LoadFile(cfgpath); // throws exception if bad path
        fb_name = config["FRAMEBUFFER_NAME"].as<string>();
        YAML::Node params = config["TRACKER"];
        N             = params["num_pings_for_moving_avg"].as<int>();
        thresh_stdevs = params["threshold_in_stdevs"].as<float>();
     }
    catch( const std::exception& e )
    {
        NIMS_LOG_ERROR << "Error reading config file: " << cfgpath << endl;
        NIMS_LOG_ERROR << e.what() << endl;
        NIMS_LOG_ERROR << desc << endl;
        return -1;
    }
    
    NIMS_LOG_DEBUG << "num_pings_for_moving_avg = " << N;

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
	cout << endl << "Starting " << argv[0] << endl;
    SubprocessCheckin(getpid()); // Synchronize with main NIMS process.
	
     //-------------------------------------------------------------------------
    // INITIALIZE MOVING AVG, STDEV
     FrameBufferReader fb(fb_name);
    if ( -1 == fb.Connect() )
    {
        NIMS_LOG_ERROR << "Error connecting to framebuffer.";
        return -1;
    }

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
            << " Error getting ping for initial moving average.";
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
        if (sigint_received) {
            NIMS_LOG_WARNING << "exiting due to SIGINT";
            break;
        }
        
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
        
        //DetectionMessage msg_ui(next_ping.header.ping_num, 0);
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
        
    }
	   
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
