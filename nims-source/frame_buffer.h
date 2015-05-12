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

#include <mqueue.h> // POSIX message queue
#include <limits.h> // for NAME_MAX
#include <string>   // for strings
#include <thread>   // for threads
#include <iostream> // clog

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
    size_t data_size;
    
    Frame()
    {
        data = nullptr;
        data_size = 0;
    };
    
}; // struct Frame

// The FrameBufferInterface is an interface for accessing
// the data from a particular sonar device.  Each sonar
// device will have its own FrameBufferInterace.  One
// process (the ingester) will instatiate  a FrameBufferInterface
// with writer=true.  This process will put new frames
// in the buffer as data arrives from the device.  Other
// processes will instantiate a FrameBufferInterface as readers
// (writer=false).  The reader processess will consume frames in the
// order that they arrived in the buffer, with each frame
// remaining in the buffer until all the reader processes
// have consumed it, or some set amount of time expires.
class FrameBufferInterface
{
	public:
    // Each sonar device has a unique name.
	    FrameBufferInterface(const std::string &fb_name, bool writer=false);
	    ~FrameBufferInterface();
	   
    // NOTE: Don't need this now because constructor throws exception.
	    // Call immediately following construction to test
	    // that the interface was properly initialized.
	    //bool IsOpen() { return (mqw_ != -1 || mqr_ != -1); };
	    
	    // Put a new frame into the buffer.  Returns the
	    // index of the new frame.
	    long PutNewFrame(const Frame &new_frame); 
	    
	    // Get the next frame in the buffer, "next" meaning
	    // relative to the last frame that was retrieved by the
	    // calling process.  Returns the index of the frame.
	    long GetNextFrame(Frame* next_frame);
	    
	    
    private:
        void HandleMessages();  // thread function run by writer
    
        std::string fb_name_;    // unique name for this frame buffer
        std::string shm_prefix_; // framebuffer shared memory path name prefix
        std::string mqw_name_;    // writer message queue name
        mqd_t mqw_;                // writer message queue (FIFO)
        std::thread t_;            // writer's connection service thread
        std::vector<mqd_t> mq_readers_; // list of reader queues, only used by writer 
        std::string mqr_name_prefix_;    // reader message queue name
    std::string mqr_name_;
        mqd_t mqr_;                // reader message queue, only used by reader


}; // class FrameBufferInterface


#endif // __NIMS_FRAMEBUFFER_H__
