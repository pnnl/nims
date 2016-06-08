/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  data_source_blueview.cpp
 *
 *  Created by Shari Matzner on 01/19/2016.
 *  Copyright 2016 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

 /***************************************
 NOTE:  The BlueView SDK provides a C interface
 and a C++ wrapper for the C interface.  The
 disadvantage of the C++ wrapper is that it throws
 exceptions which is a pain in the arse. Therefore
 I will use the C interface.
 */
#include "data_source_blueview.h"

#include <dirent.h> // list files in directory
#include <cmath> // modf
#include <cv.h> // opencv
#include <highgui.h> // imwrite for TEST

# include "frame_buffer.h"
#include "log.h"

using namespace std;
using namespace cv; // openCV
//-----------------------------------------------------------------------------
DataSourceBlueView::DataSourceBlueView(BlueViewParams const &params)
{
    files_ = params.files;
    if (files_)
        host_or_path_ = params.datadir;
    else
        host_or_path_ = params.host_addr;
    pulse_rate_hz_ = params.pulse_rate_hz;
    son_ = NULL;
    head_ = NULL;
    imager_ = NULL;
    ping_count_ = 0;
	
	
	son_ = BVTSonar_Create();
	if( son_ == NULL )
	{
		NIMS_LOG_ERROR << "BVTSonar_Create: failed";
	}
    
} // DataSourceBlueView::DataSourceBlueView

//-----------------------------------------------------------------------------
DataSourceBlueView::~DataSourceBlueView() 
{
    BVTImageGenerator_Destroy(imager_);
    BVTHead_Destroy(head_);
	BVTSonar_Destroy(son_); 
}

//-----------------------------------------------------------------------------
int DataSourceBlueView::connect()
{
	if( son_ == NULL )
	{
		NIMS_LOG_ERROR << "BVTSonar_Create: failed";
		return -1;
	}
    int ret = -1;
    if (files_)
    {
        // make sure directory exists
        DIR           *d;
        struct dirent *dir;
        d = opendir(host_or_path_.c_str());
        if (d == nullptr)
        {
            // TODO: perror
            NIMS_LOG_ERROR << "Error reading data directory " << host_or_path_;
            return -1;
        }
         // put existing files in queue
         while ((dir = readdir(d)) != NULL)
        {
            NIMS_LOG_DEBUG <<  dir->d_name;
        }
       closedir(d);
            
        // watch for new files
        // open first file
       string filename("../data/BlueView/2016_03_24_06_41_01.son");
        ret = BVTSonar_Open(son_, "FILE", filename.c_str());
    }
    else
        ret = BVTSonar_Open(son_, "NET", host_or_path_.c_str());
	if( ret != 0 )
	{
		NIMS_LOG_ERROR << "BVTSonar_Open " << host_or_path_ << ": ret = " << ret;
		return -1;
	}

    char buffer[256];
    buffer[0] = '\0';
    BVTSonar_GetSonarName (son_, buffer, sizeof(buffer));
    NIMS_LOG_DEBUG << "Sonar name: " << string(buffer);
    buffer[0] = '\0';
    BVTSonar_GetSonarModelName (son_, buffer, sizeof(buffer));
    NIMS_LOG_DEBUG << "Sonar model: " << string(buffer);
    buffer[0] = '\0';
    BVTSonar_GetFirmwareRevision (son_, buffer, sizeof(buffer));
    NIMS_LOG_DEBUG << "Firmware revision: " << string(buffer);

	// Make sure we have the right number of heads
	int heads = -1;
	BVTSonar_GetHeadCount(son_, &heads);
	NIMS_LOG_DEBUG << "BVTSonar_GetHeadCount: " << heads;


    // TODO:  Make the head a configuration parameter.
	// Get the first head.
	ret = BVTSonar_GetHead(son_, 0, &head_);
	if( ret != 0 )
	{
		NIMS_LOG_ERROR << "BVTSonar_GetHead: ret = " << ret;
		return -1;
	}
    
    // TODO: Might need to set the sound speed.
    
    // Initialize an image generator for the head.
     imager_ = BVTImageGenerator_Create();
     ret = BVTImageGenerator_SetHead(imager_, head_);
	if( ret != 0 )
	{
		NIMS_LOG_ERROR << "BVTImageGenerator_SetHead: ret = " << ret;
		return -1;
	}
    
    return 0;
    
} // DataSourceBlueView::connect


//-----------------------------------------------------------------------------
int DataSourceBlueView::GetPing(Frame* pframe)
{
    
    if ( head_ == NULL ) {
        NIMS_LOG_ERROR << ("DataSourceBlueView::GetPing() Not connected to source.");
        return -1;
    }

    BVTPing ping;
    int ret = BVTHead_GetPing(head_, -1, &ping);
    if( ret != 0 )
	{
		NIMS_LOG_ERROR << "BVTHead_GetPing failed: ret = " << ret;
		return -1;
	}
    // get the ping data in range-bearing coordinates
    BVTMagImage img;
    BVTImageGenerator_GetImageRTheta(imager_, ping, &img);

    ++ping_count_;
    
    NIMS_LOG_DEBUG << "    extracting header";
    
    //TODO:  use name and model returned from calls in constructor
    strncpy(pframe->header.device, "Teledyne BlueView Imaging Sonar",
            sizeof(pframe->header.device));
    pframe->header.version = 0; // TODO:  firmware version is a string
    pframe->header.ping_num = ping_count_;
    
    double timestamp;
    ret = BVTPing_GetTimestamp(ping, &timestamp);
    //NIMS_LOG_DEBUG << "BVTPing_GetTimestamp (" << ret << "): " << timestamp;
    double secs = 0.0;
    double frac_secs = modf(timestamp, &secs);
    pframe->header.ping_sec = (uint32_t)secs;
    pframe->header.ping_millisec = (uint32_t)floor(frac_secs*1000);
    
    int sound_speed = 0;
    BVTPing_GetSoundSpeed(ping, &sound_speed);
    pframe->header.soundspeed_mps = (float)sound_speed;
    
    int imh(0);
   BVTMagImage_GetHeight(img, &imh);
    pframe->header.num_samples = imh;

    float range = 0.0;
    BVTPing_GetStartRange(ping, &range);
    pframe->header.range_min_m = range;
    BVTPing_GetStopRange(ping, &range);
    pframe->header.range_max_m = range;
    
    // TODO:  Figure out if we need this for anything.
    pframe->header.winstart_sec = 0;
    pframe->header.winlen_sec = 0;
    
     int imw(0);
    BVTMagImage_GetWidth(img, &imw);
    pframe->header.num_beams = std::min(imw,kMaxBeams);
    float min_beam(0.0), max_beam(0.0);
    BVTPing_GetFOV(ping, &min_beam, &max_beam);
    double brg_res_deg;
    BVTMagImage_GetBearingResolution(img, &brg_res_deg);
 
    for (int m=0; m<pframe->header.num_beams; ++m)
    {
        if (kMaxBeams == m) break; // max in frame_buffer.h
        pframe->header.beam_angles_deg[m] = min_beam + float(m*brg_res_deg);
    }
    
     int freq;
    BVTHead_GetCenterFreq(head_, &freq);
    pframe->header.freq_hz = freq;
    
    pframe->header.pulselen_microsec = 0;

    if (files_)
      pframe->header.pulserep_hz = pulse_rate_hz_;
    else
    { 
        int millisec;
        BVTHead_GetPingInterval(head_, &millisec);
        pframe->header.pulserep_hz = 1.0/(millisec/1000);
    }

    NIMS_LOG_DEBUG << "    extracting data";
    
  // TEST
    // Note that few programs actually support loading a 16bit PGM.
    //BVTMagImage_SavePGM(img, "MagImage.pgm");
    
    // allocate memory for data
    size_t frame_data_size = sizeof(framedata_t)*imh*imw;
    NIMS_LOG_DEBUG << "frame_data_size = " << frame_data_size;
    pframe->malloc_data(frame_data_size);
    if ( pframe->size() != frame_data_size )
        NIMS_LOG_ERROR << "Error allocating memory for frame data.";
    framedata_t* fdp = pframe->data_ptr();
    
    //  The image data is organized in Row-Major order (just like C/C++).
    //  The data type is unsigned short 16-bit.  Use openCV conversion for speed.
    unsigned short *pdata = nullptr;
    BVTMagImage_GetBits(img, &pdata);
    Mat im1(imh, imw, CV_16UC1, pdata); // Mat wrapper
    int type_code = (sizeof(framedata_t) == 64) ? CV_64FC1 : CV_32FC1;
    Mat im2(imh, imw, type_code, fdp); // Mat wrapper
    im1.convertTo(im2, type_code, 1./65535., 0); // convert type with scaling
    
    // TEST
     imwrite("MatGetBits.png", im1);

    BVTMagImage_Destroy(img);
    BVTPing_Destroy(ping);

    return ping_count_;
    
} // DataSourceBlueView::GetPing