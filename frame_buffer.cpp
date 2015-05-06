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

struct FrameMsg {
    int64_t frame_number;
    size_t mapped_data_size;
    char   shm_open_name[NAME_MAX]; // TODO: should this be PATH_MAX (posix)?
    
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
static size_t SizeForFramebuffer(const Frame &f)
{
    static size_t page_size = 0;
    if (0 == page_size)
        page_size = sysconf(_SC_PAGE_SIZE);

    assert(page_size > 0);
    const size_t len = f.data_size + sizeof(f);
    const size_t number_of_pages = len / page_size + 1;
    return page_size * number_of_pages;
}

// Constructor
FrameBufferInterface::FrameBufferInterface(const std::string &fb_name, bool writer)
: fb_name_(fb_name)
{ 
    shm_prefix_ = "/nims_" + fb_name_ + "-";
    mqw_name_ = "/nims_" + fb_name_ + "-connect";
    mqw_ = -1;
    mqr_name_prefix_ = "/nims_" + fb_name_ + "-mq-";
    mqr_ = -1;
   
   clog << "max messsage size is " << kMaxMessageSize << endl;
   
    // There must be one and only one instance of a writer for 
    // a FrameBufferInterface with a particular name.
    if (writer)
    {
        // Create the message queue for receiving reader messages.
        struct mq_attr attr;
        attr.mq_flags = 0; // block on mq_send and mq_receive
        attr.mq_maxmsg = 8;
        attr.mq_msgsize = kMaxMessageSize;
        
        clog << "creating frame buffer connection msg queue " << mqw_name_ << endl;
        // NOTE:  writer queue needs to be read/write so parent can send exit message to thread
        mqw_ = mq_open(mqw_name_.c_str(),O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, &attr);
        if (mqw_ == -1) 
        {
            BadWriterQueue e;
            throw e;
            return;
        }
        
        // Start thread for servicing reader connections
        clog << "starting connection thread" << endl;
        t_ = std::thread(&FrameBufferInterface::HandleMessages, this);
    
    } else // reader
    {
         // create the message queue for reading new data messages
        //std::string mqr_name(mqr_name_prefix_);
        mqr_name_ = mqr_name_prefix_ + boost::lexical_cast<std::string>(getpid());
        struct mq_attr attr;
        attr.mq_flags = 0;
        attr.mq_maxmsg = 8; //TODO:  Define max number of buffered frames
        attr.mq_msgsize = kMaxMessageSize;
        clog << "creating reader message queue " << mqr_name_ << endl;
        mqr_ = mq_open(mqr_name_.c_str(),O_RDONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, &attr);
        if (mqr_ == -1) 
        {
            BadReaderQueue e;
            throw e;
            return;
        }

       // connect to the writer and send reader queue name
       clog << "opening writer queue" << endl;
       mqw_ = mq_open(mqw_name_.c_str(), O_WRONLY);
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
      
        // is this blocking?
       //mq_unlink(mqr_name.c_str()); // queue will be destroyed when writer closes it
 
    } // reader
} // Constructor

// Destructor
FrameBufferInterface::~FrameBufferInterface()
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
    }
    
    // close reader message queues
    if (mqr_ != -1) 
    {
        mq_close(mqr_);
        mq_unlink(mqr_name_.c_str());

    }
    for (int k=0; k<mq_readers_.size(); ++k) mq_close(mq_readers_[k]);
    
    // clean up shared memory
    
} // Destructor

// Put a new frame into the buffer.  Returns the
// index of the new frame.
long FrameBufferInterface::PutNewFrame(const Frame &new_frame)
{
    static int64_t framebuffer_frame_count = 0;
    
    std::string shared_name(shm_prefix_);
    shared_name += boost::lexical_cast<std::string>(framebuffer_frame_count++);
    
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
    
    const size_t map_length = SizeForFramebuffer(new_frame);
    assert(map_length > sizeof(Frame));
    
    // !!! early return
    // could use fallocate or posix_fallocate, but ftruncate is portable
    if (0 != ftruncate(fd, map_length)) {
        perror("ftruncate() in FrameBufferInterface::PutNewFrame");
        shm_unlink(shared_name.c_str());
        return -1;
    }

    // mmap a shared framebuffer on the file descriptor we have from shm_open
    Frame *shared_buffer;
    shared_buffer = (Frame *)mmap(NULL, map_length, 
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    close(fd);
    
    // !!! early return
    if (MAP_FAILED == shared_buffer) {
        perror("mmap() in FrameBufferInterface::PutNewFrame");
        shm_unlink(shared_name.c_str());
        return -1;
    }
        
    // copy data to the shared framebuffer
    shared_buffer->data_size = new_frame.data_size;
    memcpy(&shared_buffer->data, new_frame.data, new_frame.data_size);
     
    // done with the region in this process; ok to do this?
    // pretty sure we can't shm_unlink here
    munmap(shared_buffer, map_length);
    shared_buffer = nullptr;
        
    // create a message to notify the consumers
    cout << "sending frame messages to " << mq_readers_.size() << " readers" << endl;
    FrameMsg msg(framebuffer_frame_count, map_length, shared_name);
    for (int k=0; k<mq_readers_.size(); ++k)
    {
        if (0 != mq_send(mq_readers_[k], (char *)(&msg), sizeof(msg), 0)) // priority 0
        {
            // TODO:  Need to handle an error here more comprehensively. 
            //        Check errno and respond accordingly.
            perror("mq_send() in FrameBufferInterface::PutNewFrame");
        }
    } // for mq_readers_

    return framebuffer_frame_count; 
    
} // PutNewFrame
	    
// Get the next frame in the buffer, "next" meaning
// relative to the last frame that was retrieved by the
// calling process.  Returns the index of the frame.
long FrameBufferInterface::GetNextFrame(Frame* next_frame)
{
    FrameMsg msg;
    mq_receive(mqr_, (char *)&msg, sizeof(msg), 0);
    
    int fd = shm_open(msg.shm_open_name, O_RDONLY, S_IRUSR);
    
    // !!! early return
    if (-1 == fd) {
        perror("shm_open() in FrameBufferInterface::GetNextFrame");
        return -1;
    }
    
    // size of mmap region
    //assert(msg.mapped_data_length > sizeof(Frame));
    //clog << "expected mmap data name: " << msg.shm_open_name << endl;
    //clog << "expected mmap data size: " << msg.mapped_data_length << endl;
    
    // mmap a shared framebuffer on the file descriptor we have from shm_open
    Frame *shared_frame;
    shared_frame = (Frame *)mmap(NULL, msg.mapped_data_size,
                                            PROT_READ, MAP_PRIVATE, fd, 0);
    
    close(fd);
    
    // !!! early return
    if (MAP_FAILED == shared_frame) {
        perror("mmap() in FrameBufferInterface::GetNextFrame");
        shm_unlink(msg.shm_open_name);
        return -1;
    }
    
    munmap(shared_frame, msg.mapped_data_size);
    shm_unlink(msg.shm_open_name);
    
    return msg.frame_number;
    
} // GetNextFrame
	    
	    
void FrameBufferInterface::HandleMessages()
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

