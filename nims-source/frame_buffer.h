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
#include <vector>
#include <thread>   // for threads
#include <iostream> // clog

const int kMaxBeams = 512;
const int kMaxSamples = 20000;
// TODO:  Need a rational determination based on memory, frame size/page size, 
//        frame rate.  
const int kMaxFramesInBuffer = 100;

struct FrameHeader
{
    char           device[64];      // may want a predefined list?
    unsigned int   version;         // of data file or firmware, vendor-specific
    unsigned int   ping_num;        // ping counter
    unsigned int   ping_sec;        // time of ping in seconds since midnight 1-Jan-1970
    unsigned int   ping_millisec;         // milliseconds part of ping time
    float          soundspeed_mps;  // speed of sound (meters per second)
    unsigned int   num_samples;     // number of samples per beam
    float          range_min_m;     // near range (meters)
    float          range_max_m;     // far range (meters)
    float          winstart_sec;    // start of sampling window (sec)
    float          winlen_sec;      // length of sampling window (sec)
    unsigned int   num_beams;       // number of beams
    float          beam_angles_deg[kMaxBeams]; // beam angles (deg)
    unsigned int   freq_hz;         // sonar frequency (Hz)
    unsigned int   pulselen_microsec;     // pulse length (microsec)
    float          pulserep_hz;     // pulse repitition frequency (Hz)
    
    FrameHeader() 
    {
        //device = { '\0' };  // compiler complains about this
        device[0] = '\0';   
        version = 0;         
        ping_num = 0;        
        ping_sec = 0;        
        ping_millisec = 0;
        soundspeed_mps = 0.0;  
        num_samples = 0;     
        range_min_m = 0.0;     
        range_max_m = 0.0;     
        winstart_sec = 0.0;    
        winlen_sec = 0.0;      
        num_beams = 0;       
        beam_angles_deg[kMaxBeams] = { 0.0 }; 
        freq_hz = 0;         
        pulselen_microsec = 0;
        pulserep_hz = 0.0;     
    };
}; // struct FrameHeader

std::ostream& operator<<(std::ostream& strm, const FrameHeader& fh);

typedef float framedata_t; // type for data values
struct Frame
{
    FrameHeader header;
    
    Frame()
    {
       data_size = 0;
       pdata = nullptr;
     };
    
    ~Frame()
    {
        if (data_size > 0) free(pdata);
    };
    
    size_t size() const { return data_size; };
    framedata_t * const data_ptr() const { return pdata; };
    
    void malloc_data(size_t size) {
      if (data_size > 0) free(pdata);
      pdata = (framedata_t*)malloc(size);
      if (pdata != nullptr) data_size = size;
    };
    
private:
    size_t data_size;
    framedata_t *pdata;

}; // struct Frame


// One process (the ingester) will instantiate  a FrameBufferWriter.
// This process will put new frames
// in the buffer as data arrives from a sonar device.  Other
// processes will instantiate a FrameBufferReader.
// The reader processess will consume frames in the
// order that they arrived in the buffer.  A specified number
// of frames will exist at any one time; old frames are removed
// as new frames arrive.
class FrameBufferWriter
{
	public:
    // Each sonar device has a unique buffer. The buffer name is obtained from config.yml
    //  and passed in to the constructor.
       
	    FrameBufferWriter(const std::string &fb_name);
	    ~FrameBufferWriter();

	   // Open for business.  Will re-initialize if already intialized.
	   int Initialize();
    
	    bool initialized() { return (mqw_ != -1); };
	    
	    // Put a new frame into the buffer.  Returns the
	    // index of the new frame.
	    long PutNewFrame(const Frame &new_frame); 
	    
    private:
        void CleanUp();  // used by destructor and intialize
        void HandleMessages();  // thread function run by writer
    
        std::string fb_name_;    // unique name for this frame buffer
        std::string shm_prefix_; // framebuffer shared memory path name prefix
        std::string mqw_name_;   // writer message queue name
        mqd_t mqw_;              // writer message queue (FIFO)
        std::thread t_;          // writer's connection service thread
        std::vector<mqd_t> mq_readers_;             // list of reader queues
        std::string shm_names_[kMaxFramesInBuffer]; // "slots" for frames in shared mem
        int64_t frame_count_;   // number of frames written
    
 }; // class FrameBufferWriter

class FrameBufferReader
{
	public:
    // Each sonar device has a unique name.
	    FrameBufferReader(const std::string &fb_name);
	    ~FrameBufferReader();
	    
	    // Connect to the writer 
	    int Connect();
	   
 	    bool connected() { return (mqr_ != -1); };
	    
	    // Get the next frame in the buffer, "next" meaning
	    // relative to the last frame that was retrieved by the
	    // calling process.  Returns the index of the frame.
	    long GetNextFrame(Frame* next_frame);
	    
	    
    private:
        std::string fb_name_;    // unique name for this frame buffer
        std::string mqw_name_;    // writer message queue name
        mqd_t mqw_;                // writer message queue
        std::string mqr_name_;
        mqd_t mqr_;                // reader message queue


}; // class FrameBufferReader

#endif // __NIMS_FRAMEBUFFER_H__
