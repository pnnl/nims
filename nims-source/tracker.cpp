/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  tracker.cpp
 *  
 *  Created by Shari Matzner on 02/14/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
#include <iostream> // cout, cin, cerr
#include <fstream>  // ifstream, ofstream
#include <string>   // for strings


#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <opencv2/opencv.hpp>

#include "yaml-cpp/yaml.h"

#include "queues.h"
#include "frame_buffer.h"
#include "tracked_object.h"
#include "pixelgroup.h"

using namespace std;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;
using namespace cv;




int main (int argc, char * argv[]) {
	//--------------------------------------------------------------------------
    // PARSE COMMAND LINE
	//
	po::options_description desc;
	desc.add_options()
	("help",                                                              "print help message")
	("cfg,c", po::value<string>()->default_value( "./config.yaml" ), "path to config file; default is ./config.yaml")
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
    
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
	
	string fb_name;
	int N;
    fs::path cfgfilepath( options["cfg"].as<string>() );
    try 
    {
        YAML::Node config = YAML::LoadFile(cfgfilepath.string());
         fb_name = config["FRAMEBUFFER_NAME"].as<string>();
         N = config["NUM_PINGS_FOR_MOVING_AVG"].as<int>();
     }
     catch( const std::exception& e )
    {
        cerr << "Error reading config file." << e.what() << endl;
        cerr << desc << endl;
        return -1;
    }
    
    
	
 	/*
     fs::path avifilepath("tracks.avi");
   VideoWriter outvideo(avifilepath.string().c_str(), CV_FOURCC('D','I','V','X'), 10,
						 Size(DUALFRAME ? 2*info.ixsize : info.ixsize, info.iysize));
	if ( !outvideo.isOpened() )
	{
		cerr << "Could not open output video file " << avifilepath.string() << endl;
		return -1;
	}*/
    //-------------------------------------------------------------------------
	// SETUP TRACKING
    
	vector<TrackedObject> active_tracks;   // tracks still being updated
	vector<int> active_id;                 // unique id for each track
	vector<TrackedObject> completed_tracks;// completed tracks (no updates)
	vector<int> completed_id;              // unique id
	int next_id = 0;
	
	// NOTE:  The noise values are somewhat arbitrary.  The important 
	//        thing is their relative values. Here, the process noise
	//         should be much greater than the measurement noise because
	//        an animals's progress through the 3D space of moving water 
	//        is very noisy.  The measurement noise is the resolution
	//        of the sample space (range bin, beam width).
	// TODO:  make these params in config file
    float process_noise = 1e-1;     // process is fish swimming
    float measurement_noise = 1e-2; // measurement is backscatter
	int pred_err_max = 15; // max difference in pixels between prediction and actual

    clog << endl << "tracking with process noise = " << process_noise 
         << " and measurement noise = " << measurement_noise << endl;


    Mat_<framedata_t> ping_mean; // moving average echo intensity (background)
    Mat_<framedata_t> ping_stdv; // moving std dev of echo intensity (background)
    
        FrameBufferReader fb(fb_name);
        if ( -1 == fb.Connect() )
        {
            cerr << argv[0] << "Error connecting to framebuffer." << endl;
            return -1;
        }
        
        // Initialize the mean and std dev of the echo intensity.
        // Get one ping to get the dimensions of the ping data.
         Frame next_ping;
        
       if ( fb.GetNextFrame(&next_ping)==-1 )
        {
            cerr << argv[0] << "Error getting ping to initialize moving average." << endl;
            return -1;
        }

    SubprocessCheckin(getpid()); // synchronize with main NIMS process
    
       int dim_sizes[] = {(int)next_ping.header.num_samples, 
                           (int)next_ping.header.num_beams};
        int cv_type = sizeof(framedata_t)==32 ? CV_32FC1 : CV_64FC1; 
        
        size_t total_samples = next_ping.header.num_samples*next_ping.header.num_beams;
        Mat_<framedata_t> pings(N, total_samples);
        
        clog << "initializing moving average and std dev" << endl;
        for (int k=0; k<N; ++k)
        {
            if ( fb.GetNextFrame(&next_ping)==-1 )
            {
                cerr << argv[0] 
                     << "Error getting ping to initialize moving average." << endl;
                return -1;
            }
            // Create a cv::Mat wrapper for the ping data
            Mat ping_data(2,dim_sizes,cv_type,next_ping.data_ptr());
            ping_data.reshape(0,1).copyTo(pings.rowRange(k,k+1));
        }
        meanStdDev(pings, ping_mean, ping_stdv);
        clog << "moving average and std dev initialized" << endl;
        
        int frame_index = -1;
        while ( (frame_index = fb.GetNextFrame(&next_ping)) != -1)
        {
            clog << argv[0] << ": " << "got frame " << frame_index << endl;
            clog << endl << next_ping.header << endl;
            Mat ping_data(2,dim_sizes,cv_type,next_ping.data_ptr());
            
            // update background
            clog << "updating mean background"  << endl;
            Mat new_mean = ping_mean + (ping_data.reshape(0,1) - ping_mean)/N;
            ping_stdv = ping_stdv + 
                (ping_data.reshape(0,1) - ping_mean).mul(ping_data.reshape(0,1) - new_mean);
            ping_mean = new_mean;
            
            // detect targets
            clog << "detecting targets" << endl;
            // TODO: Make number of std devs a parameter
            Mat foregroundMask = (ping_data.reshape(0,1) - ping_mean) / ping_stdv > 3; 
            int nz = countNonZero(foregroundMask);
            if (nz > 0)
            {
                PixelGrouping objects;
                group_pixels(foregroundMask, objects);
                int n_obj = objects.size();
                cout << "found " << n_obj << " obj:" << endl;

            } // nz > 0
            
            // update tracks
            clog << "tracking" << endl;
            
        }
     
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
