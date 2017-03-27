/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  viewer.cpp
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
#include "frame_buffer.h"
#include "detections.h"
#include "tracks_message.h"

using namespace std;
using namespace boost;
namespace fs = boost::filesystem;
using namespace cv;

// Generate the mapping from beam-range to x-y for display
int PingImagePolarToCart(const FrameHeader &hdr, OutputArray _map_x, OutputArray _map_y)
{
    // range bin resolution in meters per pixel
    float rng_step = (hdr.range_max_m - hdr.range_min_m)/(hdr.num_samples - 1);
    cout << "PingImagePolarToCart: range resolution is " << rng_step;
    float y1 = hdr.range_min_m; // y coordinate of first row of image pixels
    float y2 = hdr.range_max_m; // y coordinate of first row of image pixels
    
    //float beam_step = hdr.beam_angles_deg[1] - hdr.beam_angles_deg[0];
    double theta1 = (double)hdr.beam_angles_deg[0] * M_PI/180.0;
    double theta2 = (double)hdr.beam_angles_deg[hdr.num_beams-1] * M_PI/180.0;
    float x1 = hdr.range_max_m*sin(theta1);
    float x2 = hdr.range_max_m*sin(theta2);
    cout << "PingImagePolarToCart: beam angles from " << hdr.beam_angles_deg[0]
    << " to " << hdr.beam_angles_deg[hdr.num_beams-1];
    //cout << "PingImagePolarToCart: beam angle step is " << beam_step;
    cout << "PingImagePolarToCart: x1 = " << x1 << ", x2 = " << x2;
    
    // size of image
    int nrows = hdr.num_samples;
    int ncols = (x2 - x1) / rng_step;
    cout << "PingImagePolarToCart: image size is " << nrows << " x " << ncols;
    
    _map_x.create(nrows, ncols, CV_32FC1);
    Mat_<float> map_x = _map_x.getMat();
    _map_y.create(nrows, ncols, CV_32FC1);
    Mat_<float> map_y = _map_y.getMat();
    
    // initialize map to 0's
    map_x = Mat::zeros(nrows, ncols, CV_32FC1);
    map_y = Mat::zeros(nrows, ncols, CV_32FC1);

    vector<float> beam_angles_deg(hdr.beam_angles_deg,
        hdr.beam_angles_deg + hdr.num_beams );
    vector<float>::iterator low,up;
    float x,y, rng,beam_deg,i_rng,i_beam;
    // each row is a range
    for (int m=0; m<nrows; ++m)
    {
        // each column is an angle
        for (int n=0; n<ncols; ++n)
        {
            // convert row, col to x,y coordinates, centered at origin
           x = x1 + n*rng_step;
           y = y2 - m*rng_step;
            // convert x,y to beam-range
           rng = sqrt(pow(x,2)+pow(y,2));
           beam_deg = 90 - (atan2(y,x)*(180/M_PI));
            //cout << endl << "rng = " << rng << ", beam_deg = " << beam_deg << endl;
            // convert beam-range to indices
           if (rng >= hdr.range_min_m && rng <= hdr.range_max_m 
            && beam_deg >= hdr.beam_angles_deg[0] && beam_deg <= hdr.beam_angles_deg[hdr.num_beams-1])
           {
               i_rng = (rng - hdr.range_min_m) / rng_step;
               up = upper_bound(beam_angles_deg.begin(), beam_angles_deg.end(), beam_deg);
                // cout << "*(up - 1) = " << *(up-1)  << ", *up = " << *up << endl;
                if (up != beam_angles_deg.end() ) 
                    i_beam = up - beam_angles_deg.begin() - (*up - beam_deg)/(*up - *(up -1));
                else
                    i_beam = hdr.num_beams-1;

                map_x(m,n) = i_beam;
                map_y(m,n) = i_rng;
            }
        }
    }
}

int main (int argc, char * argv[]) {

    string cfgpath, log_level;
    if ( parse_command_line(argc, argv, cfgpath, log_level) != 0 ) return -1;
    setup_logging(string(basename(argv[0])), cfgpath, log_level);
   setup_signal_handling();
    
    
  // READ CONFIG FILE
  // string cfgpath("./config.yaml");
  string fb_name; // frame buffer
   try 
    {
        YAML::Node config = YAML::LoadFile(cfgpath);
       fb_name = config["FRAMEBUFFER_NAME"].as<string>();
      }
     catch( const std::exception& e )
    {
        cerr << "Error reading config file." << e.what();
        return -1;
    }
     
    
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
  //SubprocessCheckin(getpid()); // Synchronize with main NIMS process.

// set up view window and data structures
  const char *WIN_PING="Ping Image";
  namedWindow(WIN_PING, CV_WINDOW_NORMAL );
  //namedWindow(WIN_MEAN, CV_WINDOW_NORMAL );
  //int disp_ms = 100; // time duration of window display

  // connect to frame buffer to get raw ping data
  //cout << "connecting to frame buffer ... ";
    FrameBufferReader fb(fb_name);
    if ( -1 == fb.Connect() )
    {
        cerr << "Error connecting to framebuffer.";
        return -1;
    }
cout << "connected." << endl;

    Frame raw_ping;
    fb.GetNextFrame(&raw_ping);
    FrameHeader *fh = &raw_ping.header; // for notational convenience
    //float beam_max = fh->beam_angles_deg[(fh->num_beams-1)];
    //float beam_min = fh->beam_angles_deg[0];
    Mat map_x, map_y;
    PingImagePolarToCart(raw_ping.header, map_x, map_y);
    
    double min_beam, min_rng, max_beam, max_rng;
    minMaxIdx(map_x, &min_beam, &max_beam);
    minMaxIdx(map_y, &min_rng, &max_rng);

    cout << "image mapping:  beam index is from "
        << min_beam << " to " << max_beam << endl;
     cout << "image mapping:  range index is from "
        << min_rng << " to " << max_rng << endl;

int nrows = fh->num_samples;
int ncols = fh->num_beams;
long total_samples = nrows*ncols;
int cv_type = sizeof(framedata_t)==4 ? CV_32FC1 : CV_64FC1;
//vector<float> brg(fh->beam_angles_deg, fh->beam_angles_deg+fh->num_beams);
float range_res = (fh->range_max_m - fh->range_min_m) / (fh->num_samples-1);
float ymin = fh->range_min_m;
float theta1 = (double)fh->beam_angles_deg[0] * M_PI/180.0;
float xmin = fh->range_max_m*sin(theta1);

/*
vector<float> rng;
for (int r=0;r<fh->num_samples;++r)
  rng.push_back(fh->range_min_m + r*range_res);
*/
  // connect to detector to get detection messages
    mqd_t mq_det = CreateMessageQueue(MQ_DETECTOR_VIEWER_QUEUE, sizeof(DetectionMessage));
    if (mq_det < 0) 
    {
        NIMS_LOG_ERROR << "Error creating MQ_DETECTOR_VIEWER_QUEUE";
        return -1;
    }

  // connect to tracker to get track messages
	
    int frame_index = -1;
    while ( (frame_index = fb.GetNextFrame(&raw_ping)) != -1 && 0 == sigint_received)
    {    
        if (sigint_received) {
            cout << "received SIGINT; exiting main loop" << endl;
            break;
        }

            double v1,v2;
           // ping data as 1 x total_samples vector, 32F from 0.0 to ?
            Mat ping_data(1,total_samples,cv_type,raw_ping.data_ptr());
             // reshape to single channel, num_samples rows
            //Mat im1 = ping_data.reshape(0,nrows);
            minMaxIdx(ping_data, &v1, &v2);
            Mat im1 = ping_data.reshape(0,nrows) * 1./v2; // scale to [0,1]
            
            Mat im2;
            im1.convertTo(im2,CV_8U,255.0,0.0);
            Mat temp[] = {im2, im2, im2};
            Mat im3;
            merge(temp, 3, im3);

           // map from beam,range to x,y
            Mat im_out;
            remap(im3, im_out, map_x, map_y,
                   INTER_LINEAR, BORDER_CONSTANT, Scalar(0,0,0));

       DetectionMessage msg_det;
        int ret =  mq_receive(mq_det, (char *)&msg_det, sizeof(DetectionMessage), nullptr);
        if (ret < 0)
        {
            nims_perror("Viewer mq_receive");
            NIMS_LOG_ERROR << "error receiving message from detector";
            NIMS_LOG_WARNING << "sigint_received is " << sigint_received;
        }
        NIMS_LOG_DEBUG << "received detections message with " 
                       << msg_det.num_detections << " detections";
        if (ret > 0 & msg_det.num_detections > 0)
        {
            int n_obj = msg_det.num_detections;
            vector<float>::iterator it_x, it_y;
            for (int n=0; n<n_obj; ++n)
            {
              float x = msg_det.detections[n].center[RANGE]*sin(msg_det.detections[n].center[BEARING]);
              x = (x - xmin) / range_res;
               float y = msg_det.detections[n].center[RANGE]*cos(msg_det.detections[n].center[BEARING]);
              y = (y - ymin)/range_res;
               circle(im_out, Point2f( x,y ), 2, CV_RGB(255,0,0), -1, 8, 0);
            }
        }

            //stringstream pngfilepath;
            //pngfilepath <<  "ping-" << frame_index % 30 << ".png";
           // imwrite(pngfilepath.str(), im_out);
        imshow(WIN_PING, im_out); waitKey(25);
    } // main loop
	   
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
