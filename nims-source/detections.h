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
    float center_x;
    float center_y;
    float width;
    float height;
    float rot_deg; // rotation clockwise in degrees
    float mean_intensity;
    
    Detection()
    {
        center_x = 0.0;
        center_y = 0.0;
        width = 0.0;
        height = 0.0;
        rot_deg = 0.0;
        mean_intensity = 0.0;
       
    };
    
    Detection(float x, float y, float w, float h, float r, float i)
    {
        center_x = x;
        center_y = y;
        width = w;
        height = h;
        rot_deg = r;
        mean_intensity = i;
    };
    
};

struct __attribute__ ((__packed__)) DetectionMessage
{
    uint32_t  ping_num; // same ping number as in FrameHeader
    uint32_t  num_detections; // number of detections
    Detection detections[MAX_DETECTIONS_PER_FRAME];

    DetectionMessage(uint32_t pnum = 0, uint32_t numdetects = 0 )
    {
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
