/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  ingester.cpp
 *  
 *  Created by Shari Matzner on 02/14/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
#include <iostream> // cout, cin, cerr
#include <fstream>  // ifstream, ofstream
#include <string>   // for strings
#include <sys/inotify.h> // watch a directory
#include <signal.h>

// for POSIX shared memory
#include "nims_includes.h"
#include <fcntl.h>    // O_* constants
#include <unistd.h>   // sysconf
#include <sys/mman.h> // mmap, shm_open
#include "queues.h"

//#include <opencv2/opencv.hpp>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;
//using namespace cv;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

#define EVENT_SIZE ( sizeof(struct inotify_event) )
#define BUF_LEN ( 1024 * (EVENT_SIZE + 16) )

static volatile int sigint_received = 0;

/*
 Mmap doesn't technically require rounding up to a page boundary
 for the length you pass in, but we'll do that anyway. If other
 components start sharing memory, we can make this external.
*/
static size_t SizeForFramebuffer(const NimsFramebuffer *nfb)
{
    static size_t page_size = 0;
    if (0 == page_size)
        page_size = sysconf(_SC_PAGE_SIZE);

    assert(page_size > 0);
    const size_t len = nfb->data_length + sizeof(NimsFramebuffer);
    const size_t number_of_pages = len / page_size + 1;
    return page_size * number_of_pages;
}

static size_t ShareFrameBuffer(const NimsFramebuffer *nfb,
        NimsIngestMessage *ingest_message)
{
    static int64_t framebuffer_name_count = 0;
    
    string shared_name(FRAMEBUFFER_SHM_PREFIX);
    shared_name += boost::lexical_cast<string>(framebuffer_name_count++);
    
    /*
     Create -rw------- since we don't need executable pages.
     Using O_EXCL could be safer, but these can still exist
     in /dev/shm after a program crash.
    */
    int fd = shm_open(shared_name.c_str(), O_CREAT | O_TRUNC | O_RDWR, 
            S_IRUSR | S_IWUSR);
    
    // !!! early return
    if (-1 == fd) {
        perror("shm_open() in ingester::ShareFrameBufferAndNotify");
        return -1;
    }
    
    const size_t map_length = SizeForFramebuffer(nfb);
    assert(map_length > sizeof(NimsFramebuffer));
    
    // !!! early return
    // could use fallocate or posix_fallocate, but ftruncate is portable
    if (0 != ftruncate(fd, map_length)) {
        perror("ftruncate() in ingest::ShareFrameBufferAndNotify");
        shm_unlink(shared_name.c_str());
        return -1;
    }

    // mmap a shared framebuffer on the file descriptor we have from shm_open
    NimsFramebuffer *shared_buffer;
    shared_buffer = (NimsFramebuffer *)mmap(NULL, map_length, 
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    close(fd);
    
    // !!! early return
    if (MAP_FAILED == shared_buffer) {
        perror("mmap() in ingester::ShareFrameBufferAndNotify");
        shm_unlink(shared_name.c_str());
        return -1;
    }
        
    // copy data to the shared framebuffer
    shared_buffer->data_length = nfb->data_length;
    memcpy(&shared_buffer->data, nfb->data, nfb->data_length);
    
    // done with the region in this process; ok to do this?
    // pretty sure we can't shm_unlink here
    munmap(shared_buffer, map_length);
    shared_buffer = NULL;
        
    // create a message to notify the observer
    ingest_message->mapped_data_length = map_length;
    ingest_message->shm_open_name = { '\0' };
    
    // Shouldn't happen unless we have framebuffer_name_count with >200 digits
    assert((shared_name.size() + 1) < sizeof(ingest_message->shm_open_name));
    strcpy(ingest_message->shm_open_name, shared_name.c_str());

    cout << "shared name: " << shared_name << endl;
    cout << "data length: " << nfb->data_length << endl;
    cout << "mmap length: " << map_length << endl;
    
    return nfb->data_length;
}

// returns length of data copied to shared memory
static size_t ProcessFile(string &watchDirectory, string &fileName)
{    
    cout << "process file: " << fileName << " in dir: " << watchDirectory << endl;
    
    // do transformation into frame buffer object
        
    NimsFramebuffer nfb;

    // test data; ignore constness here
    const char *test_data = "this is only a test";
    nfb.data_length = strlen(test_data) + 1;
    nfb.data = (void *)test_data;
    
    NimsIngestMessage msg;
    size_t len = ShareFrameBuffer(&nfb, &msg);
    
    if (len > 0) {
        
        mqd_t ingest_queue = CreateMessageQueue(sizeof(msg), MQ_INGEST_QUEUE);
            
        if (-1 == ingest_queue) {
            cerr << "ingester: failed to create message queue" << endl;
            exit(1);
        }

        // why is the message a const char *?
        if (mq_send(ingest_queue, (const char *)&msg, sizeof(msg), 0) == -1) {
            perror("mq_send in ingester::ProcessFile");
            return -1;
        }
        mq_close(ingest_queue);
        // no mq_unlink here!
    }
    
    return len;
}

static void sig_handler(int sig)
{
    if (SIGINT == sig)
        sigint_received++;
}

int main (int argc, char * argv[]) {
	//--------------------------------------------------------------------------
    // PARSE COMMAND LINE
	//
	po::options_description desc;
	desc.add_options()
	("help",                                                    "print help message")
	("indir,i", po::value<string>()->default_value( "/home/input" ), "path for input files")
	;
	po::variables_map options;
    try
    {
        po::store( po::parse_command_line( argc, argv, desc ), options );
    }
    catch( const std::exception& e )
    {
        cerr << "Sorry, couldn't parse that: " << e.what() << endl;
        cerr << desc << endl;
        return -1;
    }
	
	po::notify( options );
	
    if( options.count( "help" ) > 0 )
    {
        cerr << desc << endl;
        return 0;
    }
	
    string inputDirectory = options["indir"].as<string>();
    
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
	
	// TODO: make sure input path exists
    clog << "Watching directory " << inputDirectory << endl;
    
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sig_handler);
	
	// from Beluga.cpp (Michael Henry)
	// This is a semi-complicated watch that we're setting up
    //  When data comes in, it's binned based on the hour
    //  At the top of a new hour, a directory for that hour is
    //  created, and data is copied to that directory
    //  So, what we need to do is monitor a top-level directory
    //  for the creation of new subdirectories. Once we detect
    //  that event, we then need to monitor that subdirectory for
    //  new files. Simple, right?
    char buffer[BUF_LEN];
    string watchDirectory = "";

    int fd = inotify_init();

    if( fd < 0 ) {
        perror( "Error initializing inotify" );
        exit(1);
    }

    // watch for newly created directories at the top level
    int cd = inotify_add_watch( fd, inputDirectory.c_str(), IN_CREATE );
    
    SubprocessCheckin(getpid());

    while( 1 ) {
		
        int wd;
        if( watchDirectory != "" ) {
            // watch for newly created files
            wd = inotify_add_watch( fd, watchDirectory.c_str(), IN_CLOSE_WRITE );
        }
		
        int length = read( fd, buffer, BUF_LEN );
		
        // in case the read() was interrupted by a signal
        if (sigint_received) {
            cerr << "ingester received SIGINT; cleaning up" << endl;
            inotify_rm_watch(fd, wd);
            break;
        }
        
        if( length < 0 ) {
            perror( "Error in read() on inotify file descriptor" );
            inotify_rm_watch(fd, wd);
            continue;
        }
		
        int i=0;
        while( i < length ) {
            struct inotify_event *event = ( struct inotify_event * ) &buffer[i];
            if( event->len ) {
                if( event->mask & IN_CREATE ) {
                    if( event->mask & IN_ISDIR ) {
                        watchDirectory = inputDirectory + "/" + event->name;
                        clog << "Found new directory: " << watchDirectory << endl;
                    }
                } 
                if( event->mask & IN_CLOSE_WRITE ) {
                    if( !(event->mask & IN_ISDIR) ) {
                        clog << "Found file: " << event->name << endl;
                        string eventName(event->name);
                        ProcessFile(watchDirectory, eventName);
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }

        // done watching this subdirectory
        inotify_rm_watch(fd, wd);

    }
	
    inotify_rm_watch(fd, cd);
    close(fd);
    
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
