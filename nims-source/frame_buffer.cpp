/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  frame_buffer.cpp
 *  
 *  Created by Adam Maxwell and Shari Matzner on 04/23/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
// for POSIX shared memory
#include <fcntl.h>    // O_* constants
#include <unistd.h>   // sysconf, getpid
#include <assert.h>   // assert
#include <sys/mman.h> // mmap, shm_open

#include <exception>  // exception class

#include <boost/lexical_cast.hpp>

#include "frame_buffer.h"

using namespace std;

struct __attribute__ ((__packed__)) FrameMsg {
    // Python code unpacks these as long long and unsigned long long,
    // respectively, so we used fixed-length types.
    int64_t  frame_number;
    uint64_t mapped_data_size;
    char     shm_open_name[NAME_MAX];
    
    FrameMsg()
    {
        frame_number = -1;
        mapped_data_size = 0;
        shm_open_name[0] = '\0';
    };
    
    FrameMsg(int64_t count, size_t size, const std::string& name)
    {
        frame_number = count;
        mapped_data_size = size;
     
        // Shouldn't happen but better check anyway
        assert((name.size() + 1) < sizeof(shm_open_name));
        memset(shm_open_name, '\0', sizeof(shm_open_name));
        strcpy(shm_open_name, name.c_str());

    };
};

const size_t kMaxMessageSize = sizeof(FrameMsg);

struct BadWriterQueue : std::exception
{
    const char* what() const noexcept {return "Can't create/open writer message queue.\n";}
};

struct BadReaderQueue : std::exception
{
    const char* what() const noexcept {return "Can't create/open reader message queue.\n";}
};

/*
 Mmap doesn't technically require rounding up to a page boundary
 for the length you pass in, but we'll do that anyway. If other
 components start sharing memory, we can make this external.
*/
// TODO:  Does this need to be static?
static size_t SizeForSharedFrame(const Frame &f)
{
    static size_t page_size = 0;
    if (0 == page_size)
        page_size = sysconf(_SC_PAGE_SIZE);

    assert(page_size > 0);
    const size_t len = f.size() + sizeof(f);
    const size_t number_of_pages = len / page_size + 1;
    return page_size * number_of_pages;
}

std::ostream& operator<<(std::ostream& strm, const FrameHeader& fh)
{
    strm << "   device = " << fh.device << endl;
    strm << "   version = " << fh.version << endl;
    strm << "   ping_num = " << fh.ping_num << endl;
    strm << "   ping_sec = " << fh.ping_sec << endl;
    strm << "   ping_millisec = " << fh.ping_millisec << endl;
    strm << "   soundspeed_mps = " << fh.soundspeed_mps << endl;
    strm << "   num_samples = " << fh.num_samples << endl;
    strm << "   range_min_m = " << fh.range_min_m << endl;
    strm << "   range_max_m = " << fh.range_max_m << endl;
    strm << "   winstart_sec = " << fh.winstart_sec << endl;
    strm << "   winlen_sec = " << fh.winlen_sec << endl;
    strm << "   num_beams = " << fh.num_beams << endl;
    if (fh.num_beams > 0)
        strm << "   beam_angles_deg = " << fh.beam_angles_deg[0] << " to " << fh.beam_angles_deg[fh.num_beams-1] << endl;
    strm << "   freq_hz = " << fh.freq_hz << endl;
    strm << "   pulselen_microsec = " << fh.pulselen_microsec << endl;
    strm << "   pulserep_hz = " << fh.pulserep_hz << endl;
    
	return strm;
    
} // operator<<

//-----------------------------------------------------------------------------
// FrameBufferWriter Constructor
FrameBufferWriter::FrameBufferWriter(const std::string &fb_name, const std::string &mqw_name)
: fb_name_(fb_name)
{ 
    shm_prefix_ = "/nims_" + fb_name_ + "-";
    mqw_name_ = mqw_name;
    mqw_ = -1;
   
    clog << "max messsage size is " << kMaxMessageSize << endl;
   
    // There must be one and only one instance of a writer for 
    // a FrameBufferInterface with a particular name.
    // Create the message queue for receiving reader messages.
    struct mq_attr attr;
    attr.mq_flags = 0; // block on mq_send and mq_receive
    attr.mq_maxmsg = 8;
    attr.mq_msgsize = NAME_MAX;
    
    clog << "creating frame buffer connection msg queue " << mqw_name_ << endl;
    // NOTE:  writer queue needs to be read/write so parent can send exit message to thread
    mqw_ = mq_open(mqw_name_.c_str(),O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, &attr);
    if (mqw_ == -1) 
    {
        perror("FrameBufferWriter");
        BadWriterQueue e;
        throw e;
        return;
    }
    frame_count_ = 0;
    
    // Start thread for servicing reader connections
    clog << "starting connection thread" << endl;
    t_ = std::thread(&FrameBufferWriter::HandleMessages, this);
    
} // FrameBufferWriter Constructor

//-----------------------------------------------------------------------------
// FrameBufferWriter Destructor
FrameBufferWriter::~FrameBufferWriter()
{
    // wait for connection thread to return
    if (t_.joinable())
    {
       char msg = 'x';
       clog << "sending exit message to connection thread" << endl;
        mq_send(mqw_, &msg, sizeof(msg), 0);
        clog << "waiting for thread to return" << endl;
        t_.join();
        mq_unlink(mqw_name_.c_str());
        mq_close(mqw_);
        clog << __func__ << " cleaned up thread and writer queue" << endl;
    }
    
    // close reader message queues
    for (int k=0; k<mq_readers_.size(); ++k) mq_close(mq_readers_[k]);
    
    // clean up shared memory
    for (int k=0; k<kMaxFramesInBuffer; ++k) shm_unlink(shm_names_[k].c_str());
    
    clog << __func__ << " cleaned up reader message queues" << endl;
    
} // FrameBufferWriter Destructor


//-----------------------------------------------------------------------------
// Put a new frame into shared memory.  Returns the
// index of the new frame.
long FrameBufferWriter::PutNewFrame(const Frame &new_frame)
{
    //static int64_t framebuffer_frame_count = 0; // made this a member var
    
    std::string shared_name(shm_prefix_);
    shared_name += boost::lexical_cast<std::string>(frame_count_++);
    
    /*
     Create -rw------- since we don't need executable pages.
     Using O_EXCL could be safer, but these can still exist
     in /dev/shm after a program crash.
    */
    int fd = shm_open(shared_name.c_str(), O_CREAT | O_TRUNC | O_RDWR, 
            S_IRUSR | S_IWUSR);
    
    // !!! early return
    if (-1 == fd) {
        perror("shm_open() in FrameBufferInterface::PutNewFrame");
        return -1;
    }
    
    const size_t map_length = SizeForSharedFrame(new_frame);
    assert(map_length > sizeof(Frame));
    
    // !!! early return
    // could use fallocate or posix_fallocate, but ftruncate is portable
    if (0 != ftruncate(fd, map_length)) {
        perror("ftruncate() in FrameBufferInterface::PutNewFrame");
        shm_unlink(shared_name.c_str());
        return -1;
    }

    // mmap the file descriptor from shm_open into this address space
    char *shared_frame;
    shared_frame = (char *)mmap(NULL, map_length,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    close(fd);
    
    // !!! early return
    if (MAP_FAILED == shared_frame) {
        perror("mmap() in FrameBufferInterface::PutNewFrame");
        shm_unlink(shared_name.c_str());
        return -1;
    }
        
    // copy frame header and data to the shared frame
    memcpy(shared_frame, &(new_frame.header), sizeof(new_frame.header));
    size_t data_size = new_frame.size();
    memcpy(shared_frame + sizeof(new_frame.header), &data_size, sizeof(data_size));
    memcpy(shared_frame + sizeof(new_frame.header) + sizeof(data_size), 
           new_frame.data_ptr(), data_size);
     
    // done with the region in this process; ok to do this?
    // pretty sure we can't shm_unlink here
    munmap(shared_frame, map_length);
    shared_frame = nullptr;
        
    // create a message to notify the consumers
    cout << "sending frame messages to " << mq_readers_.size() << " readers" << endl;
    FrameMsg msg(frame_count_, map_length, shared_name);
    struct timespec tm;
    clock_gettime(CLOCK_REALTIME, &tm); // get the current time
    for (int k=0; k<mq_readers_.size(); ++k)
    {
        // TODO:  if a reader queue is full, then mq_send blocks, so one slow reader
        //        could hold up other processes.  Could set O_NONBLOCK flag...
        // O_NONBLOCK is not good because we wantreaders to block if queue is empty.
        // If queue is full and time is past, then we'll return immediately.
        if (0 != mq_timedsend(mq_readers_[k], (char *)(&msg), sizeof(msg), 0, &tm)) 
        {
            // TODO:  Need to handle an error here more comprehensively. 
            //        If there is problem with queue, may need to remove it from the list.
            perror("mq_send() in FrameBufferInterface::PutNewFrame");
        }
    } // for mq_readers_
    
    // unlink oldest shared frame and save the name of new frame
    int ind = frame_count_ % kMaxFramesInBuffer;
    shm_unlink(shm_names_[ind].c_str());
    shm_names_[ind] = shared_name;

    return frame_count_;
    
} // FrameBufferWriter::PutNewFrame
	    
//-----------------------------------------------------------------------------	    
void FrameBufferWriter::HandleMessages()
{
    char msg[kMaxMessageSize];
    mqd_t mqr;
    ssize_t numbytes; 
    
    while(1)
    {
        // Wait for a message.
        numbytes = mq_receive(mqw_, msg, kMaxMessageSize, 0);
        if (-1 == numbytes) 
        {
            perror("connection thread");
            clog << "connection thread returning" << endl;
            return;
        }
        msg[numbytes] = '\0';
        clog << "got a message with " << numbytes << " bytes: " << msg << endl;
        if (numbytes==1 && msg[0] == 'x') return;
        
        // Open reader message queue.
        clog << "opening message queue " << msg << endl;
        mqr = mq_open(msg,O_WRONLY);
        mq_readers_.push_back(mqr); // Add it to the list.
    }
    
} // ConnectReaders

//-----------------------------------------------------------------------------
// *******  FrameBufferReader  ********
//-----------------------------------------------------------------------------
// FrameBufferReader Constructor
FrameBufferReader::FrameBufferReader(const std::string &fb_name, const std::string &mqw_name)
: fb_name_(fb_name)
{ 
    mqw_name_ = mqw_name;
    mqw_ = -1;
    mqr_ = -1;
   
   clog << "max messsage size is " << kMaxMessageSize << endl;
   
         // create the message queue for reading new data messages
        //std::string mqr_name(mqr_name_prefix_);
        mqr_name_ = "/nims_" + fb_name_ + "-mq-" + boost::lexical_cast<std::string>(getpid());
        struct mq_attr attr;
        attr.mq_flags = 0;
        attr.mq_maxmsg = 10; // system limit in /proc/sys/fs/mqueue/msg_max
        attr.mq_msgsize = kMaxMessageSize;
        clog << "creating reader message queue " << mqr_name_ << endl;
        mqr_ = mq_open(mqr_name_.c_str(),O_RDONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, &attr);
        if (mqr_ == -1) 
        {
            perror("FrameBufferReader");
            BadReaderQueue e;
            throw e;
            return;
        }

       // connect to the writer and send reader queue name
       clog << "opening writer queue" << endl;
       mqw_ = mq_open(mqw_name_.c_str(), O_WRONLY);
       // TODO:  Should probably wait and try again and eventually time out.
       if (mqw_ == -1) 
       {
           // clean up
           mq_close(mqr_);
           mq_unlink(mqr_name_.c_str());
           
           BadWriterQueue e;
           throw e;
           return;
       }
       clog << "sending connection message" << endl;
       if (-1 == mq_send(mqw_, mqr_name_.c_str(), mqr_name_.size(), 0))
       {
           // clean up
           mq_close(mqr_);
           mq_unlink(mqr_name_.c_str());
           
           BadWriterQueue e;
           throw e;
           return;
       }
       mq_close(mqw_);
       mqw_ = -1;
      
} // FrameBufferReader Constructor 

// FrameBufferReader Destructor
FrameBufferReader::~FrameBufferReader()
{
        mq_close(mqr_);
        mq_unlink(mqr_name_.c_str());

    // clean up shared memory
    
} // FrameBufferReader Destructor

//-----------------------------------------------------------------------------
// Get the next frame in the buffer, "next" meaning
// relative to the last frame that was retrieved by the
// calling process.  Returns the index of the frame if successful.
long FrameBufferReader::GetNextFrame(Frame* next_frame)
{
    // TODO:  Not sure if this is a valid test to make sure the caller
    //        is passing the address of a Frame struct.
    if (next_frame == nullptr)
    {
        cerr << "GetNextFrame: pointer argument must be initialized!" << endl;
        return -1;
    }
    // Read messages until we get a valid shared memory name.  If the reader
    // is behind, then a message may be old and the shared memory name 
    // contained in the message may already be unlinked.
    FrameMsg msg;
     int fd = -1; // shared memory file descriptor
     while ( fd == -1 )
    {
        // Note this will block if queue is empty.
        if ( -1 == mq_receive(mqr_, (char *)&msg, sizeof(msg), 0) )
        {
            perror("GetNextFrame");
            return -1;
        }
        // Attempt to open the shared memory.
        fd = shm_open(msg.shm_open_name, O_RDONLY, S_IRUSR);
   }
    
    cout << "GetNextFrame: getting " << msg.shm_open_name << endl;
    // size of mmap region
    assert(msg.mapped_data_size > sizeof(Frame));
    clog << "GetNextFrame: data size = " << msg.mapped_data_size << endl;
    
    // mmap a shared framebuffer on the file descriptor we have from shm_open
    char *shared_frame;
    shared_frame = (char *)mmap(NULL, msg.mapped_data_size,
                                            PROT_READ, MAP_PRIVATE, fd, 0);
    
    close(fd);
    
    // !!! early return
    if (MAP_FAILED == shared_frame) {
        perror("mmap() in GetNextFrame");
        return -1;
    }
    
    // copy the data into this address space
    memset(&(next_frame->header), 0, sizeof(next_frame->header));
    memcpy(&(next_frame->header), shared_frame, sizeof(next_frame->header));
    size_t data_size;
    memcpy(&data_size, shared_frame + sizeof(next_frame->header), sizeof(data_size));
    next_frame->malloc_data(data_size);
    if (next_frame->size() != data_size)
    {
        cerr << "Error: Can't allocate memory for frame data." << endl;
        return -1;
    }
    memcpy(next_frame->data_ptr(), shared_frame + sizeof(next_frame->header) 
           + sizeof(data_size), data_size);
    
    // clean up
    //clog << "GetNextFrame: unmapping shared memory" << endl;
    munmap(shared_frame, msg.mapped_data_size);
    
    //clog << "GetNextFrame: Done." << endl;
    return msg.frame_number;
    
} // FrameBufferReader::GetNextFrame
	    

