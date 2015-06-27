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
#include <cmath>    // for trigonometric functions


#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>

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

#define PI 3.14159265

// Generate the mapping from beam-range to x-y for display
int PingImagePolarToCart(const FrameHeader &hdr, OutputArray _map_x, OutputArray _map_y)
{
    // range bin resolution in meters per pixel
    float rng_step = (hdr.range_max_m - hdr.range_min_m)/(hdr.num_samples - 1);
    clog << "PingImagePolarToCart: range resolution is " << rng_step << endl;
    float y2 = hdr.range_max_m; // y coordinate of first row of image pixels
    
    float beam_step = hdr.beam_angles_deg[1] - hdr.beam_angles_deg[0];
    double theta1 = (double)hdr.beam_angles_deg[0] * PI/180.0;
    double theta2 = (double)hdr.beam_angles_deg[hdr.num_beams-1] * PI/180.0;
    float x1 = hdr.range_max_m*sin(theta1);
    float x2 = hdr.range_max_m*sin(theta2);
    clog << "PingImagePolarToCart: beam angles from " << hdr.beam_angles_deg[0]
         << " to " << hdr.beam_angles_deg[hdr.num_beams-1] << endl;
    clog << "PingImagePolarToCart: beam angle step is " << beam_step << endl;
    clog << "PingImagePolarToCart: x1 = " << x1 << ", x2 = " << x2 << endl;

    // size of image
    int nrows = hdr.num_samples;
    int ncols = (x2 - x1) / rng_step;
    clog << "PingImagePolarToCart: image size is " << nrows << " x " << ncols << endl;
    
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
            if (i_rng >=0 && i_rng < hdr.num_samples && i_beam >= 0 && i_beam < hdr.num_beams)
            {
                map_x(m,n) = i_beam;
                map_y(m,n) = i_rng;
            }
        }
    }
}

int MakePingImage(InputArray _ping, OutputArray _img)
{
    
    
    return 0;
}

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
    
    // For testing
    bool VIEW = true; // display new ping images
    const char *WIN_PING="Ping Image";
     const char *WIN_MEAN="Mean Intensity Image";
  if (VIEW)
  {
        //namedWindow(WIN_PING, CV_WINDOW_AUTOSIZE );
        //namedWindow(WIN_MEAN, CV_WINDOW_AUTOSIZE );
  }
  int disp_ms = 100; // time duration of window display
  
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


        FrameBufferReader fb(fb_name);
        if ( -1 == fb.Connect() )
        {
            cerr << argv[0] << "Error connecting to framebuffer." << endl;
            return -1;
        }
        
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
        // the framedata_t (frame_buffer.h) is either float or double
        int cv_type = sizeof(framedata_t)==4 ? CV_32FC1 : CV_64FC1; 
        size_t total_samples = next_ping.header.num_samples*next_ping.header.num_beams;
        
        clog << "   num samples = " << dim_sizes[0] << ", num beams = " << dim_sizes[1] << endl;
        clog << "   bytes per sample = " << sizeof(framedata_t) << " (" ;
             if ( sizeof(framedata_t)==4 ) clog << "CV_32FC1";
             else clog << "CV_64FC1";
             clog << ")" << endl;
        clog << "   total samples = " << total_samples << endl;
        
        Mat map_x;
        Mat map_y;
        if (VIEW) 
        {
            PingImagePolarToCart(next_ping.header, map_x, map_y);
            double min_beam, min_rng, max_beam, max_rng;
            minMaxIdx(map_x.reshape(0,1), &min_beam, &max_beam);
            minMaxIdx(map_y.reshape(0,1), &min_rng, &max_rng);
            
            clog << "image mapping:  beam index is from " << min_beam << " to " << max_beam << endl;
            clog << "image mapping:  range index is from " << min_rng << " to " << max_rng << endl;
        }
        
         // Initialize the mean and std dev of the echo intensity.
       clog << "initializing moving average and std dev" << endl;
       Mat_<framedata_t> pings(N, total_samples);  // TODO:  would be good to clear this when done intitializing
        Mat ping_mean = Mat::zeros(dim_sizes[0],dim_sizes[1],cv_type); // moving average      
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
            ping_data.reshape(0,1).copyTo(pings.row(k));
            ping_mean += ping_data;
         }
        ping_mean = ping_mean/N;
        double min_val, max_val;
        minMaxIdx(pings.row(N-1), &min_val, &max_val);
        clog << "ping data values from " << min_val << " to " << max_val << endl;
    
       Mat ping_stdv = Mat::zeros(dim_sizes[0],dim_sizes[1],cv_type); // moving std dev 
        for (int k=0; k<N; ++k)
        {
            Mat sqrDiff;
            pow(ping_mean - pings.row(k).reshape(0,dim_sizes[0]),2.0f,sqrDiff);
            ping_stdv += sqrDiff;
        }
        ping_stdv = ping_stdv/(N-1);
   
       //meanStdDev(pings, ping_mean, ping_stdv);
        clog << "moving average and std dev initialized" << endl;
        
        int frame_index = -1;
        while ( (frame_index = fb.GetNextFrame(&next_ping)) != -1)
        {
            clog << argv[0] << ": " << "got frame " << frame_index << endl;
            clog << endl << next_ping.header << endl;
            Mat ping_data(2,dim_sizes,cv_type,next_ping.data_ptr());
            
            // update background
            clog << "updating mean background"  << endl;
            // u(k) = u(k-1) + (x - u(k-1))/N
            Mat new_mean = ping_mean + (ping_data - ping_mean)/N;
            // s(k) = s(k-1) + (x - u(k-1))(x - u(k))/(N-1)
            ping_stdv = ping_stdv + (ping_data - ping_mean).mul(ping_data - new_mean)/(N-1);
            ping_mean = new_mean;
            
            if (VIEW)
            {
                // display new ping image
                Mat img1;
                 remap(ping_data, img1, map_x, map_y, INTER_LINEAR, BORDER_CONSTANT, Scalar(0,0,0));
                Mat im_out;
                img1.convertTo(im_out, CV_16U, 65535, 0);
                stringstream pngfilepath;
                pngfilepath <<  "ping-" << frame_index << ".png";
                imwrite(pngfilepath.str(), im_out);
                //imshow(WIN_PING, ping_data);
                //waitKey(disp_ms); // have to call this to get image to display
                // display updated mean image
                Mat img2;
                 remap(ping_mean, img2, map_x, map_y, INTER_LINEAR, BORDER_CONSTANT, Scalar(0,0,0));
                //imshow(WIN_MEAN, ping_mean);
                //waitKey(disp_ms);
            }
             
            // detect targets
            clog << "detecting targets" << endl;
            // TODO: Make number of std devs a parameter
            Mat foregroundMask = (ping_data - ping_mean) / ping_stdv > 3; 
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
