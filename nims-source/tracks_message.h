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
#include <algorithm> // max, min

// Too many dependencies when dragging this header and code in
// (logging, opencv, etc), so the cython module setup script
// sets this flag.
#if !(NIMS_CYTHON)
#include "detections.h"
#endif

#define MAX_ACTIVE_TRACKS 50

// Message structure sent by the tracker to describe
// the currently active tracks.  Using fundamental non_compound
// types because this structure may be used by an
// external application.

struct __attribute__ ((__packed__)) Track
{
	uint16_t id;           // unique track identifier
	float size_sq_m;       // avg size in square meters
	float speed_mps;       // avg speed in meters per sec.
	float target_strength; // avg intensity

	float min_range_m;     // bounding box of the entire track
	float max_range_m;
	float min_bearing_deg;
	float max_bearing_deg;
    float min_elevation_deg;
	float max_elevation_deg;

    float first_detect;    // time when track started
	uint16_t pings_visible; // number of pings the target was visible

	float last_pos_range;   // last observed position
	float last_pos_bearing;
	float last_pos_elevation;

	float last_vel_range;   // last observed velocity
	float last_vel_bearing; 
	float last_vel_elevation; 

	float width;            // last observed size
	float length;
	float height;           

	Track()
	{
		id = 0;           
		size_sq_m = 0.0;   
		speed_mps = 0.0;    
		target_strength = 0.0; 

		min_range_m = 0.0;     
		max_range_m = 0.0;
		min_bearing_deg = 0.0;
		max_bearing_deg = 0.0;
		min_elevation_deg = 0.0;
		max_elevation_deg = 0.0;

	    first_detect = 0.0;    
		pings_visible = 0; 

		last_pos_range = 0.0;   
		last_pos_bearing = 0.0;
		last_pos_elevation = 0.0;

		last_vel_range = 0.0;   
		last_vel_bearing = 0.0;
		last_vel_elevation = 0.0;

		width = 0.0; 
		length = 0.0;           
		height = 0.0;           
	};
  
#if !(NIMS_CYTHON)
	Track(uint16_t trknum, const std::vector<Detection>& detections)
	{
		id = trknum;           
		size_sq_m = 0.0; 
		speed_mps = 0.0;      
		target_strength = 0.0; 
		min_range_m = 1000.0;     
		max_range_m = -1000.0;
		min_bearing_deg = 1000.0;
		max_bearing_deg = -1000.0;
		min_elevation_deg = 1000.0;
		max_elevation_deg = -1000.0;

		for (int k=0; k<detections.size(); ++k)
		{
			size_sq_m += detections[k].size[RANGE]*detections[k].size[BEARING];       
			target_strength += detections[k].intensity_max; 
			min_range_m = std::min(min_range_m,detections[k].center[RANGE]);     
			max_range_m = std::max(max_range_m,detections[k].center[RANGE]);
			min_bearing_deg = std::min(min_bearing_deg,detections[k].center[BEARING]);
			max_bearing_deg = std::max(max_bearing_deg,detections[k].center[BEARING]);
			min_elevation_deg = std::min(min_elevation_deg,detections[k].center[ELEVATION]);
			max_elevation_deg = std::max(max_elevation_deg,detections[k].center[ELEVATION]);
		}
		pings_visible = detections.size(); 
		size_sq_m /= pings_visible;
		target_strength /= pings_visible;

	    first_detect = detections.front().timestamp;    
		last_pos_range = detections.back().center[RANGE];   
		last_pos_bearing = detections.back().center[BEARING];
		last_pos_elevation = detections.back().center[ELEVATION];

		last_vel_range = 0.0;   
		last_vel_bearing = 0.0;
		last_vel_elevation = 0.0;

	if (pings_visible > 1)
		{
			std::vector<int>::size_type next_to_last = detections.size() - 2; 
			float dt = detections.back().timestamp - detections[next_to_last].timestamp;
			last_vel_range = (last_pos_range - detections[next_to_last].center[1])/dt;   
			last_vel_bearing = (last_pos_bearing - detections[next_to_last].center[0])/dt;
			last_vel_elevation = (last_pos_elevation - detections[next_to_last].center[2])/dt;
		}

		// TODO: convert width and height to meters instead of degrees
	    width = detections.back().size[BEARING];            
		length = detections.back().size[RANGE];        
		height = detections.back().size[ELEVATION];        
	};
#endif

}; // Track

struct __attribute__ ((__packed__)) TracksMessage 
{
    uint32_t  frame_num;   // NIMS internal ping number from FrameHeader
    uint32_t  ping_num_sonar; // sonar ping number
    double     ping_time; // seconds since Jan 1, 1970
    uint32_t  num_tracks; // number of tracks
    Track     tracks[MAX_ACTIVE_TRACKS]; // track data

    TracksMessage()
    {
        frame_num = 0;
        ping_num_sonar = 0;
        ping_time = 0.0;
        num_tracks = 0;
        memset(tracks, 0, sizeof(Track)*MAX_ACTIVE_TRACKS);
    };
    TracksMessage(uint32_t fnum, uint32_t pnum_s, double ts, std::vector<Track> vec_tracks )
    {
        frame_num = fnum;
        ping_num_sonar = pnum_s;
        ping_time = ts;
        num_tracks = std::min((int)vec_tracks.size(),MAX_ACTIVE_TRACKS);
        memset(tracks, 0, sizeof(Track)*MAX_ACTIVE_TRACKS);
        // is this really ok?
        std::copy(vec_tracks.begin(), 
                vec_tracks.begin()+num_tracks, tracks);
   };

}; // TracksMessage
/*
#if !(NIMS_CYTHON)

std::ostream& operator<<(std::ostream& strm, const TracksMessage& tm)
{
    std::ios_base::fmtflags fflags = strm.setf(std::ios::fixed,std::ios::floatfield);
    int prec = strm.precision();
    strm.precision(3);

    strm << tm.ping_time 
    << "," << tm.frame_num << "," << tm.ping_num_sonar << "," << tm.num_tracks
    << std::endl;

    // restore formatting
    strm.precision(prec);
    strm.setf(fflags);

    return strm;
};
#endif // NIMS_CYTHON

*/
#endif // __NIMS_TRACKS_MESSAGE_H__
