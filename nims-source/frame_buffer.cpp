/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  frame_buffer.cpp
 *  
 *  Created by Adam Maxwell and Shari Matzner on 04/23/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

 #include "frame_buffer.h"

// for POSIX shared memory
#include <fcntl.h>    // O_* constants
#include <unistd.h>   // sysconf, getpid
#include <assert.h>   // assert
#include <sys/mman.h> // mmap, shm_open

#include <exception>  // exception class

#include <boost/lexical_cast.hpp>
#include <boost/log/trivial.hpp>

#include "log.h"
#include "nims_ipc.h"

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
const size_t kMaxMessage = 8; // maximum number of messages that can exist on the queue

/*
 Mmap doesn't technically require rounding up to a page boundary
 for the length you pass in, but we'll do that anyway. If other
 components start sharing memory, we can make this external.
*/
const size_t kPageSize = sysconf(_SC_PAGE_SIZE);
static size_t SizeForSharedFrame(const Frame &f)
{
/*
    static size_t page_size = 0;
    if (0 == page_size)
        page_size = sysconf(_SC_PAGE_SIZE);

    assert(page_size > 0);
    const size_t len = f.size() + sizeof(f);
    const size_t number_of_pages = len / page_size + 1;
    return page_size * number_of_pages;
*/
    assert(kPageSize > 0);
    const size_t len = f.size() + sizeof(f);
    const size_t number_of_pages = len / kPageSize + 1;
    return kPageSize * number_of_pages;

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
FrameBufferWriter::FrameBufferWriter(const std::string &fb_name)
: fb_name_(fb_name)
{ 
    shm_prefix_ = "/" + fb_name_ + "-";
    mqw_name_ = "/" + fb_name;
    mqw_ = -1;
    (void) pthread_mutex_init(&mqr_lock_, NULL);
   
    NIMS_LOG_DEBUG << "max messsage size is " << kMaxMessageSize;
   
    
} // FrameBufferWriter Constructor

//-----------------------------------------------------------------------------
// FrameBufferWriter Destructor
FrameBufferWriter::~FrameBufferWriter()
{
     CleanUp();
     
} // FrameBufferWriter Destructor

//-----------------------------------------------------------------------------
int FrameBufferWriter::Initialize()
{
    // Fresh start.
    CleanUp();
    
     // Create the message queue for receiving reader connections.
   NIMS_LOG_DEBUG << "creating frame buffer connection msg queue " << mqw_name_;
   /*
     struct mq_attr attr;
    attr.mq_flags = 0; // block on mq_send and mq_receive
    attr.mq_maxmsg = 8;
    attr.mq_msgsize = NAME_MAX;
    
    // NOTE:  writer queue needs to be read/write so parent can send exit message to thread
    mqw_ = mq_open(mqw_name_.c_str(),O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, &attr);
    */
    mqw_ = CreateMessageQueue(mqw_name_, NAME_MAX);
    if (mqw_ == -1) 
    {
        nims_perror("FrameBufferWriter");
        return -1;
    }
    frame_count_ = 0;
    
    // Start thread for servicing reader connections
    NIMS_LOG_DEBUG << "starting connection thread";
    t_ = std::thread(&FrameBufferWriter::HandleMessages, this);
    
    return 0;
} // FrameBufferWriter::Initialize


//-----------------------------------------------------------------------------
// Put a new frame into shared memory.  Returns the
// index of the new frame.
long FrameBufferWriter::PutNewFrame(const Frame &new_frame)
{
    if ( !initialized() ) return -1;
    
    std::string shared_name(shm_prefix_);
    shared_name += boost::lexical_cast<std::string>(frame_count_++);
    
    NIMS_LOG_DEBUG << "FrameBufferWriter: putting frame " 
                   << frame_count_ << "(ping " << new_frame.header.ping_num << ")" << " in " << shared_name;
        
    /*
     Create -rw------- since we don't need executable pages.
     Using O_EXCL could be safer, but these can still exist
     in /dev/shm after a program crash.
    */
    int fd = shm_open(shared_name.c_str(), O_CREAT | O_TRUNC | O_RDWR, 
            S_IRUSR | S_IWUSR);
    
    // !!! early return
    if (-1 == fd) {
        nims_perror("shm_open() in FrameBufferWriter::PutNewFrame");
        return -1;
    }
    
    const size_t map_length = SizeForSharedFrame(new_frame);
    assert(map_length > sizeof(Frame));
    
    // !!! early return
    // could use fallocate or posix_fallocate, but ftruncate is portable
    if (0 != ftruncate(fd, map_length)) {
        nims_perror("ftruncate() in FrameBufferWriter::PutNewFrame");
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
        nims_perror("mmap() in FrameBufferInterface::PutNewFrame");
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
    // lock around access to mq_readers_, since it's shared between threads
    (void) pthread_mutex_lock(&mqr_lock_);
    NIMS_LOG_DEBUG << "sending frame messages to " << mq_readers_.size() << " readers";
    //FrameMsg msg(frame_count_, map_length, shared_name);
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
            nims_perror("mq_send() in FrameBufferInterface::PutNewFrame");
        }
    } // for mq_readers_
    (void) pthread_mutex_unlock(&mqr_lock_);
    
    // unlink oldest shared frame and save the name of new frame
    int ind = frame_count_ % kMaxFramesInBuffer;
    shm_unlink(shm_names_[ind].c_str());
    NIMS_LOG_DEBUG << "Replacing framebuffer slot " << ind << " (" << shm_names_[ind]
       << ") with (" << shared_name << ")";
    shm_names_[ind] = shared_name;

    return frame_count_;
    
} // FrameBufferWriter::PutNewFrame

//-----------------------------------------------------------------------------	    
void FrameBufferWriter::CleanUp()
{
   // wait for connection thread to return
    if (t_.joinable())
    {
       char msg = 'x';
       NIMS_LOG_DEBUG << "sending exit message to connection thread";
        mq_send(mqw_, &msg, sizeof(msg), 0);
        NIMS_LOG_DEBUG << "waiting for thread to return";
        t_.join();
        mq_unlink(mqw_name_.c_str());
        mq_close(mqw_);
        NIMS_LOG_DEBUG << __func__ << " cleaned up thread and writer queue";
     }
    
    // close reader message queues
    for (int k=0; k<mq_readers_.size(); ++k) {
        mq_close(mq_readers_[k]);
        NIMS_LOG_DEBUG << __func__ << " cleaned up reader queue " << mq_readers_[k];
    }
    
    // clean up shared memory
    for (int k=0; k<kMaxFramesInBuffer; ++k) {
        shm_unlink(shm_names_[k].c_str());
        // ??? could this be a std::vector
        if (shm_names_[k].size())
            NIMS_LOG_DEBUG << __func__ << " cleaned up " << shm_names_[k];
    }
    
    pthread_mutex_destroy(&mqr_lock_);
    
} // FrameBufferWriter::CleanUp
	    
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
            nims_perror("connection thread");
            NIMS_LOG_ERROR << "connection thread returning";
            return;
        }
        msg[numbytes] = '\0';
        NIMS_LOG_DEBUG << "got a message with " << numbytes << " bytes: " << msg;
        if (numbytes==1 && msg[0] == 'x') return;
        
        // Open reader message queue.
        NIMS_LOG_DEBUG << "opening message queue " << msg;
        mqr = mq_open(msg,O_WRONLY);
        //mq_unlink(msg);
        
        (void) pthread_mutex_lock(&mqr_lock_);
        mq_readers_.push_back(mqr); // Add it to the list.
        (void) pthread_mutex_unlock(&mqr_lock_);
    }
    
} // HandleMessages

//-----------------------------------------------------------------------------
// *******  FrameBufferReader  ********
//-----------------------------------------------------------------------------
// FrameBufferReader Constructor
FrameBufferReader::FrameBufferReader(const std::string &fb_name)
: fb_name_(fb_name)
{ 
    mqw_name_ = "/" + fb_name;
    mqw_ = -1;
    mqr_ = -1;
   
   NIMS_LOG_DEBUG << "max messsage size is " << kMaxMessageSize;
   
      
} // FrameBufferReader Constructor 

// FrameBufferReader Destructor
FrameBufferReader::~FrameBufferReader()
{
    mq_close(mqr_);
    mq_unlink(mqr_name_.c_str());
    NIMS_LOG_DEBUG << __func__ << " cleaned up message queue " << mqr_name_;

} // FrameBufferReader Destructor

int FrameBufferReader::Connect()
{
         // create the message queue for reading new data messages
        mqr_name_ = "/" + fb_name_ + "-mq-" + boost::lexical_cast<std::string>(getpid());
       NIMS_LOG_DEBUG << "creating reader message queue " << mqr_name_;
        mqr_ = CreateMessageQueue(mqr_name_, kMaxMessageSize);
        if (mqr_ == -1) 
        {
            nims_perror("FrameBufferReader::Connect mq_open reader");
            return -1;
        }

       // connect to the writer and send reader queue name
       // we want to wait for the writer, because we can't
       // really do anything until the data is flowing
       NIMS_LOG_DEBUG << "opening writer connection queue: " << mqw_name_;
       
   while ( (mqw_ = mq_open(mqw_name_.c_str(), O_WRONLY)) == -1 )
    {
        
        // may get SIGINT at any time to reload config
        if (sigint_received) {
            NIMS_LOG_WARNING << "exiting due to SIGINT";
            break;
        }
        
        NIMS_LOG_DEBUG << "waiting to connect to writer...";
        sleep(5);
        
   } // while mqw_ == -1

       if (mqw_ == -1) 
       {
           nims_perror("FrameBufferReader::Connect mq_open writer");
           // clean up
           mq_close(mqr_);
           mq_unlink(mqr_name_.c_str());
           return -1;
       }

       NIMS_LOG_DEBUG << "sending connection message";
       if (-1 == mq_send(mqw_, mqr_name_.c_str(), mqr_name_.size(), 0))
       {
           nims_perror("FrameBufferReader::Connect mq_send");
           // clean up
           mq_close(mqr_);
           mq_unlink(mqr_name_.c_str());
           return -1;
       }
       mq_close(mqw_);
       mqw_ = -1;
       return 0;

}

//-----------------------------------------------------------------------------
// Get the next frame in the buffer, "next" meaning
// relative to the last frame that was retrieved by the
// calling process.  Returns the index of the frame if successful.
long FrameBufferReader::GetNextFrame(Frame* next_frame)
{
    if ( !connected() ) return -1;
    
    // TODO:  Not sure if this is a valid test to make sure the caller
    //        is passing the address of a Frame struct.
    if (next_frame == nullptr)
    {
        NIMS_LOG_ERROR << "GetNextFrame: pointer argument must be initialized!";
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
            nims_perror("GetNextFrame");
            return -1;
        }
        // Attempt to open the shared memory.
        fd = shm_open(msg.shm_open_name, O_RDONLY, S_IRUSR);
   }
    
     NIMS_LOG_DEBUG << "FrameBufferReader: getting frame " 
                   << msg.frame_number << " in " << msg.shm_open_name
                   << ", " << msg.mapped_data_size << " bytes";
    // size of mmap region
    assert(msg.mapped_data_size > sizeof(Frame));
     
    // mmap a shared framebuffer on the file descriptor we have from shm_open
    char *shared_frame;
    shared_frame = (char *)mmap(NULL, msg.mapped_data_size,
                                            PROT_READ, MAP_PRIVATE, fd, 0);
    
    close(fd);
    
    // !!! early return
    if (MAP_FAILED == shared_frame) {
        nims_perror("mmap() in GetNextFrame");
        return -1;
    }
    // copy the data into this address space
    memset(&(next_frame->header), 0, sizeof(next_frame->header));
    //NIMS_LOG_DEBUG << "copying header, " << sizeof(next_frame->header) << " bytes";
    memcpy(&(next_frame->header), shared_frame, sizeof(next_frame->header));
    size_t data_size;
    memcpy(&data_size, shared_frame + sizeof(next_frame->header), sizeof(data_size));
    next_frame->malloc_data(data_size);
    if (next_frame->size() != data_size)
    {
        NIMS_LOG_ERROR << "Error: Can't allocate memory for frame data.";
        return -1;
    }
    //NIMS_LOG_DEBUG << "copying data, " << data_size << " bytes";
    memcpy(next_frame->data_ptr(), shared_frame + sizeof(next_frame->header) 
           + sizeof(data_size), data_size);
    
    // clean up
    //clog << "GetNextFrame: unmapping shared memory" << endl;
    munmap(shared_frame, msg.mapped_data_size);
    
    //clog << "GetNextFrame: Done." << endl;
    return msg.frame_number;
    
} // FrameBufferReader::GetNextFrame
	    

