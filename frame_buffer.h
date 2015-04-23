/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  frame_buffer.h
 *  
 *  Created by Shari Matzner on 04/02/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
#ifndef __NIMS_FRAMEBUFFER_H__
#define __NIMS_FRAMEBUFFER_H__

const int kMaxBeams = 512;
const int kMaxSamples = 20000;

struct FrameHeader
{
    char           device[64];      // may want a predefined list?
    unsigned int   version;         // of data file or firmware, vendor-specific
    unsigned int   ping_num;        // ping counter
    unsigned int   ping_sec;        // time of ping in seconds since midnight 1-Jan-1970
    unsigned int   ping_ms;         // milliseconds part of ping time
    float          soundspeed_mps;  // speed of sound (meters per second)
    unsigned int   num_samples;     // number of samples per beam
    float          range_min_m;     // near range (meters)
    float          range_max_m;     // far range (meters)
    float          winstart_sec;    // start of sampling window (sec)
    float          winlen_sec;      // length of sampling window (sec)
    unsigned int   num_beams;       // number of beams
    float          beam_angles_deg[kMaxBeams]; // beam angles (deg)
    unsigned int   freq_hz;         // sonar frequency (Hz)
    unsigned int   pulselen_us;     // pulse length (microsec)
    float          pulserep_hz;     // pulse repitition frequency (Hz)
    
    FrameHeader() 
    {
        //device = { '\0' };  // compiler complains about this
        device[0] = '\0';   
        version = 0;         
        ping_num = 0;        
        ping_sec = 0;        
        ping_ms = 0;         
        soundspeed_mps = 0.0;  
        num_samples = 0;     
        range_min_m = 0.0;     
        range_max_m = 0.0;     
        winstart_sec = 0.0;    
        winlen_sec = 0.0;      
        num_beams = 0;       
        beam_angles_deg[kMaxBeams] = { 0.0 }; 
        freq_hz = 0;         
        pulselen_us = 0;     
        pulserep_hz = 0.0;     
    };
}; // struct FrameHeader

struct Frame
{
    FrameHeader header;
    float *data;
    
    Frame()
    {
        data = nullptr;
    };
    
}; // struct Frame

// The FrameBuffer is a global system entity that
// acts as a FIFO queue for raw sonar data.  As a new
// data frame is ingested into the system, it is stored 
// in the FrameBuffer until all the processes that 
// consume frames have consumed it.  This class provides
// an interface for processes to the global entity.
class FrameBufferInterface
{
	public:
	    FrameBufferInterface();
	    ~FrameBufferInterface();
	    
	    // Put a new frame into the buffer.  Returns the
	    // index of the new frame.
	    int PutNewFrame(const Frame &new_frame); 
	    
	    // Get the next frame in the buffer, "next" meaning
	    // relative to the last frame that was retrieved by the
	    // calling process.  Returns the index of the frame.
	    int GetNextFrame(Frame* next_frame);
	    
	    
    private:
    
}; // class FrameBufferInterface


#endif // __NIMS_FRAMEBUFFER_H__
