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

#define MAX_DETECTIONS_PER_FRAME 100

// Data structure for describing the objects detected
// in one ping image.  Detections are possibly fish
// or marine mammals or some other object moving
// through the sonar field of view.
struct __attribute__ ((__packed__)) Detection
{
    // location of the center of the detected object
    float center_range;
    float center_beam;
    // unique id of track that detection is assigned to
    uint32_t track_id;
    // true if this detection is starting a new track
    bool new_track;
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

#endif // __NIMS_DETECTIONS_H__
