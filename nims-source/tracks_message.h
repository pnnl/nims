/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  tracks_message.h
 *  
 *  Created by Shari Matzner on 02/04/2016.
 *  Copyright 2016 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef __NIMS_TRACKS_MESSAGE_H__
#define __NIMS_TRACKS_MESSAGE_H__

#include <cstdint>  // fixed width integer types
#include <cstring>  // memset

#define MAX_ACTIVE_TRACKS 50

// Message structure sent by the tracker to describe
// the currently active tracks.

struct __attribute__ ((__packed__)) Track
{
	uint16_t id;           // unique track identifier
	float size_sq_m;       // avg size in square meters
	float speed_mps;       // avg speed in meters per sec.
	float target_strength; // avg intensity
	float min_range_m;     // bounding box of the entire track
	float max_range_m;
	float min_angle_m;
	float max_angle_m;
    uint32_t first_ping;    // ping number when track started
	uint16_t pings_visible; // number of pings the target was visible
	float last_pos_range;   // last observed position
	float last_pos_angle;
	float width;            // last observed width in meters
	float height;           // last observed height in meters
}; // Track

struct __attribute__ ((__packed__)) TracksMessage 
{
    uint32_t  ping_num;   // NIMS internal ping number from FrameHeader
    uint32_t  num_tracks; // number of tracks
    Track     tracks[MAX_ACTIVE_TRACKS]; // track data

    TracksMessage(uint32_t pnum = 0, uint32_t numtracks = 0 )
    {
        ping_num = pnum;
        num_tracks = numtracks;
        memset(tracks, 0, sizeof(tracks));
    };

}; // TracksMessage

#endif // __NIMS_TRACKS_MESSAGE_H__
