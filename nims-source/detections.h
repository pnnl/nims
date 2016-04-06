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
 #include <vector>
 #include <algorithm> // std::copy, min

//#include "log.h"      // NIMS logging

#define MAX_DETECTIONS_PER_FRAME 100

 enum dimension_t {RANGE=0, BEARING, ELEVATION};

// used to convert pixels (cells) in a frame image (echogram) to 
// world coordinates relative to the sonar head
 struct PixelToWorld
 {
    float start[3];
    float step[3];

    PixelToWorld(float start_rng, float start_brg, float start_el, 
                 float step_rng, float step_brg, float step_el)
    {
        start[RANGE] = start_rng;
        start[BEARING] = start_brg;
        start[ELEVATION] = start_el;

        step[RANGE] = step_rng;
        step[BEARING] = step_brg;
        step[ELEVATION] = step_el;
    };
 };

// Data structure for describing the objects detected
// in one ping image.  Detections are possibly fish
// or marine mammals or some other object moving
// through the sonar field of view.
struct __attribute__ ((__packed__)) Detection
{
    float timestamp; // seconds since midnight 1-Jan-1970
    // spatial shape information
    float center[3]; // x,y,z
    float size[3];   // width, length, height
    // rotation in degrees, clockwise
    // rot_deg[0] is rotation in x-y plane
    // rot_deg[1] is elevation angle with respect to x-y plane
    float rot_deg[2];  
    // target strength
    float mean_intensity;
    float max_intensity;
    
    Detection()
    {
        timestamp = 0.0;
        center[RANGE] = 0.0; center[BEARING] = 0.0; center[ELEVATION] = 0.0;
        size[RANGE] = 0.0; size[BEARING] = 0.0; size[ELEVATION] = 0.0;
        rot_deg[0] = 0.0; rot_deg[1] = 0.0;
        mean_intensity = 0.0;
        max_intensity = 0.0;
       
    };
    
    // create a detection from a set of connected points
    Detection(float ts, const std::vector<cv::Point2i>& points, const PixelToWorld& ptw)
    {
        timestamp = ts;
        //cv::RotatedRect rr = fitEllipse(points);
        cv::RotatedRect rr = minAreaRect(points);
        // TODO:  Test this conversion.
        center[BEARING] = ptw.start[BEARING] + ptw.step[BEARING]*rr.center.x; 
        center[RANGE] =   ptw.start[RANGE] +   ptw.step[RANGE]*rr.center.y; 
        center[ELEVATION] = 0.0;

        rot_deg[0] = rr.angle; rot_deg[1] = 0.0;

        cv::Rect bb = boundingRect(points);
        size[BEARING] = ptw.step[BEARING]*bb.width; 
        size[RANGE] =   ptw.step[RANGE]*bb.height; 
        size[ELEVATION] = 1.0;

        // TODO: figure out how to set these.
        mean_intensity =0.0;
        max_intensity = 0.0;
    };
    
};

struct __attribute__ ((__packed__)) DetectionMessage
{
    uint32_t  frame_num; // frame number from FrameBuffer
    uint32_t  ping_num; // sonar ping number 
    float ping_time; // seconds since midnight 1-Jan-1970
    uint32_t  num_detections; // number of detections
    Detection detections[MAX_DETECTIONS_PER_FRAME];

    DetectionMessage()
    {
        frame_num = 0;
        ping_num = 0;
        ping_time = 0.0;

        num_detections = 0;
        memset(detections, 0, sizeof(detections));
    };
    
     DetectionMessage(uint32_t fnum, uint32_t pnum, float ptime, std::vector<Detection> vec_detections )
    {
        frame_num = fnum;
        ping_num = pnum;
        ping_time = ptime;
        num_detections = std::min((int)vec_detections.size(),MAX_DETECTIONS_PER_FRAME);
        std::copy(vec_detections.begin(), 
                vec_detections.begin()+num_detections, detections);
    };
   
}; // Detections

/*
std::ostream& operator<<(std::ostream& strm, const DetectionMessage& dm)
{
    strm << "    ping_num = " << dm.ping_num << "\n"
         << "    num_detections = " << dm.num_detections << "\n"
    << std::endl;
    
    
    return strm;
};
*/

#endif // __NIMS_DETECTIONS_H__
