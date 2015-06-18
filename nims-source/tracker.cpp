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
	("cfg,c", po::value<string>()->default_value( "./nims-config.yaml" ), "path to config file; default is ./nims-config.yaml")
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
	
    // TODO:  validate config path
    fs::path cfgfilepath( options["cfg"].as<string>() );
    if ( ! (fs::exists(cfgfilepath) && fs::is_regular_file(cfgfilepath)) )
    {
        cerr << "Can't find config file " << cfgfilepath.string() << endl;
        return -1;
    }
    YAML::Node config = YAML::LoadFile(cfgfilepath.string()); // throws exception if bad path
    
    // TODO: read parameters from config file
    string buffer_name = "nims";
    fs::path avifilepath("tracks.avi");
    int N = 100; // number of pings used for moving average
    
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
    //SubprocessCheckin(getpid()); // synchronize with main NIMS process
	/*
    VideoWriter outvideo(avifilepath.string().c_str(), CV_FOURCC('D','I','V','X'), 10,
						 Size(DUALFRAME ? 2*info.ixsize : info.ixsize, info.iysize));
	if ( !outvideo.isOpened() )
	{
		cerr << "Could not open output video file " << avifilepath.string() << endl;
		return -1;
	}*/
    Mat_<framedata_t> ping_mean; // moving average echo intensity (background)
    Mat_<framedata_t> ping_stdv; // moving std dev of echo intensity (background)
    
    //--------------------------------------------------------------------------
	// SETUP TRACKING
    
	vector<TrackedObject> active_tracks;   // objects that are actively being tracked
	vector<int> active_id; // unique id for each tracked object
	vector<TrackedObject> completed_tracks; // objects that have permanently left the field of view
	vector<int> completed_id; // unique id
	int next_id = 0;
	// NOTE:  The noise values are somewhat arbitrary.  The important thing is their relative values;
	//        in this case, the process noise should be much greater than the measurement noise because
	//        a juvenile fish's progress through the 3D space of moving water is very noisy.  The measurement noise
	//        is the resolution of the sample space (range bin, beam width).
    float process_noise = 1e-1;     // process is fish swimming
    float measurement_noise = 1e-2; // measurement is detection in range-beam space
	// TODO:  Set max prediction error based on pixel size/fish size? noise?
	int pred_err_max = 15; // max distance in pixels between prediction and actual

    clog << endl << "tracking with process noise = " << process_noise << " and measurement noise = " << measurement_noise << endl;


    try
    {
        FrameBufferReader fb(buffer_name, config["FB_WRITER_QUEUE"].as<string>());
        Frame next_ping;
        
        // Initialize the mean and std dev of the echo intensity.
        // Get one ping to get the dimensions of the ping data.
        if ( fb.GetNextFrame(&next_ping)==-1 )
        {
            cerr << argv[0] << "Error getting ping to initialize moving average." << endl;
            return -1;
        }

        int dim_sizes[] = {(int)next_ping.header.num_samples, (int)next_ping.header.num_beams};
        int cv_type = CV_64FC1; // assuming double, TODO:  Need to get this dynamically from framedata_t
        size_t total_samples = next_ping.header.num_samples*next_ping.header.num_beams;
        Mat_<framedata_t> pings(N, total_samples);
        
        clog << "initializing moving average and std dev" << endl;
        for (int k=0; k<N; ++k)
        {
            if ( fb.GetNextFrame(&next_ping)==-1 )
            {
                cerr << argv[0] << "Error getting ping to initialize moving average." << endl;
                return -1;
            }
            // Create a cv::Mat wrapper for the ping data
            Mat ping_data(2,dim_sizes,cv_type,next_ping.data_ptr());
            ping_data.reshape(0,1).copyTo(pings.rowRange(k,k+1));
        }
        meanStdDev(pings, ping_mean, ping_stdv);
        clog << "initialized" << endl;
        
        int frame_index = -1;
        while ( (frame_index = fb.GetNextFrame(&next_ping)) != -1)
        {
            clog << argv[0] << ": " << "got frame " << frame_index << endl;
            clog << endl << next_ping.header << endl;
            Mat ping_data(2,dim_sizes,cv_type,next_ping.data_ptr());
            
            // update background
            clog << "updating mean background"  << endl;
            Mat new_mean = ping_mean + (ping_data.reshape(0,1) - ping_mean)/N;
            ping_stdv = ping_stdv + (ping_data.reshape(0,1) - ping_mean).mul(ping_data.reshape(0,1) - new_mean);
            ping_mean = new_mean;
            
            // detect targets
            clog << "detecting targets" << endl;
            Mat foregroundMask = (ping_data.reshape(0,1) - ping_mean) / ping_stdv > 3; // TODO: Make this a parameter
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
    }
	catch( const std::exception& e )
	{
        cerr << argv[0] << ":  " << e.what() << endl;
	}
    
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
