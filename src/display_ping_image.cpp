/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  filename.cpp
 *  
 *  Created by Firstname Lastname on mm/dd/yyyy.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

// From M3 Matlab Toolbox, beamformer/plot_beamformed_data.m

int DisplayPingImage(InputArray _ping, const FrameHeader &hdr, const string &win_name)
{
   string func_name("DisplayPingImage");

   // Validate the sizes of the rangelist and beamlist vectors
   Mat ping = _ping.getMat();
   CV_Assert( ping.type() == CV_32FC1 || ping.type() == CV_64FC1 );
 
   // Validate the sizes of the rangelist and beamlist vectors
   if ( ping.rows != hdr.num_samples || ping.cols != hdr.num_beams )
   {
       cerr << func_name << ":  Error data size does not match header!" << endl;
       cerr << "             data size is " << ping.rows << " x " << ping.cols;
       cerr << ", header num samples is " << hdr.num_samples << ", header num beams is " 
         << hdr.num_beams << endl;
       return -1;
   }

   // Convert polar to cartisan coordinates
   
   return 0;
} // DisplayPingImage


