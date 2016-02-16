/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  detections.h
 *  
 *  Created by Shari Matzner on 08/20/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef __NIMS_DETECTIONS_H__
#define __NIMS_DETECTIONS_H__

#include <cstdint>  // fixed width integer types
#include <cstring>  // memset

#define MAX_DETECTIONS_PER_FRAME 100

// Data structure for describing the objects detected
// in one ping image.  Detections are possibly fish
// or marine mammals or some other object moving
// through the sonar field of view.
struct __attribute__ ((__packed__)) Detection
{
    // spatial shape information
    float center[3]; // x,y,z
    float size[3];   // width, height, length
    float rot_deg[2]; // rotation clockwise in degrees
    float mean_intensity;
    float max_intensity;
    
    Detection()
    {
         center[0] = 0.0; center[1] = 0.0; center[2] = 0.0;
        size[0] = 0.0; size[1] = 0.0; size[2] = 0.0;
        rot_deg[0] = 0.0; rot_deg[1] = 0.0;
        mean_intensity = 0.0;
        max_intensity = 0.0;
       
    };
    
    // create a detection from a set of connected points
    Detection(std::vector<cv::Point2i>& points)
    {
        cv::RotatedRect rr = fitEllipse(points);
        center[0] = rr.center.x; center[1] = rr.center.y; center[2] = 0.0;
        size[0] = rr.size.width; size[1] = rr.size.height; size[2] = 0.0;
        rot_deg[0] = rr.angle; rot_deg[1] = 0.0;
        // TODO: figure out how to set these.
        mean_intensity =0.0;
        max_intensity = 0.0;
    };
    
};

struct __attribute__ ((__packed__)) DetectionMessage
{
    uint32_t  frame_num; // frame number from FrameBuffer
    uint32_t  ping_num; // sonar ping number 
    uint32_t  num_detections; // number of detections
    Detection detections[MAX_DETECTIONS_PER_FRAME];

    DetectionMessage(uint32_t fnum, uint32_t pnum = 0, uint32_t numdetects = 0 )
    {
        frame_num = fnum;
        ping_num = pnum;
        num_detections = numdetects;
        memset(detections, 0, sizeof(detections));
    };
    
    
}; // Detections

std::ostream& operator<<(std::ostream& strm, const DetectionMessage& dm)
{
    strm << "    ping_num = " << dm.ping_num << "\n"
         << "    num_detections = " << dm.num_detections << "\n"
    << std::endl;
    
    
    return strm;
};


#endif // __NIMS_DETECTIONS_H__
