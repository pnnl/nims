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

#include <mqueue.h>

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

static mqd_t ingestMessageQueue;
static volatile int sigint_received = 0;

/*
 Mmap doesn't technically require rounding up to a page boundary
 for the length you pass in, but we'll do that anyway.
*/
static size_t sizeForFrameBuffer(const NIMSFrameBuffer *nfb)
{
    static size_t pageSize = 0;
    if (0 == pageSize)
        pageSize = sysconf(_SC_PAGE_SIZE);

    assert(pageSize > 0);
    const size_t len = nfb->dataLength + sizeof(NIMSFrameBuffer);
    const size_t numberOfPages = len / pageSize + 1;
    return pageSize * numberOfPages;
}

static size_t shareFrameBufferAndNotify(const NIMSFrameBuffer *nfb)
{
    static int64_t fbCount = 0;
    
    string sharedName("/framebuffer-");
    sharedName += boost::lexical_cast<string>(fbCount++);
    
    /*
     Create -rw------- since we don't need executable pages.
     Using O_EXCL could be safer, but these can still exist
     in /dev/shm after a program crash.
    */
    int fd = shm_open(sharedName.c_str(), O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
    
    // !!! early return
    if (-1 == fd) {
        perror("shm_open() in ingester::processFile");
        return -1;
    }
    
    size_t mapLength = sizeForFrameBuffer(nfb);
    assert(mapLength > sizeof(NIMSFrameBuffer));
    
    // !!! early return
    // could use fallocate or posix_fallocate, but ftruncate is portable
    if (0 != ftruncate(fd, mapLength)) {
        perror("ftruncate() in ingest::processFile");
        shm_unlink(sharedName.c_str());
        return -1;
    }

    // mmap a shared framebuffer on the file descriptor we have from shm_open
    NIMSFrameBuffer *sharedBuffer;
    sharedBuffer = (NIMSFrameBuffer *)mmap(NULL, mapLength, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    // !!! early return
    if (MAP_FAILED == sharedBuffer) {
        perror("mmap() in ingester::processFile");
        shm_unlink(sharedName.c_str());
        return -1;
    }
    
    // copy data to the shared framebuffer
    sharedBuffer->dataLength = nfb->dataLength;
    memcpy(&sharedBuffer->data, nfb->data, nfb->dataLength);
    
    // create a message to notify the observer
    NIMSIngestMessage msg;
    msg.dataLength = sharedBuffer->dataLength;
    msg.name = { '\0' };
    
    // this should never happen, unless we have fbCount with >200 digits
    assert((sharedName.size() + 1) < sizeof(msg.name));
    strcpy(msg.name, sharedName.c_str());
    
    // done with the region in this process; ok to do this?
    // pretty sure we can't shm_unlink here
    munmap(sharedBuffer, mapLength);
    sharedBuffer = NULL;
    
    // why is the message a const char *?
    if (mq_send(ingestMessageQueue, (const char *)&msg, sizeof(msg), 0) == -1) {
        perror("mq_send in ingester::processFile");
        return -1;
    }
    
    return nfb->dataLength;
}

// returns length of data copied to shared memory
static size_t processFile( string & watchDirectory, string & fileName )
{    
    cout << "process file: " << fileName << " in dir: " << watchDirectory << endl;
    
    // do transformation into frame buffer object
        
    NIMSFrameBuffer nfb;

    // test data; ignore constness here
    const char *testData = "this is only a test";
    nfb.dataLength = strlen(testData) + 1;
    nfb.data = (void *)testData;
    
    return shareFrameBufferAndNotify(&nfb);
}

static void sig_handler(int sig)
{
    if (SIGINT == sig)
        sigint_received++;
}

static void initializeMessageQueue()
{
    struct mq_attr attr;
    memset(&attr, 0, sizeof(struct mq_attr));
    // ??? I can set this at 300 in my test program, but this chokes if it's > 10. WTF?
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(NIMSIngestMessage);
    attr.mq_flags = 0;
    
    ingestMessageQueue = mq_open(MQ_INGEST_QUEUE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, &attr);
    if (-1 == ingestMessageQueue){
        perror("mq_open in ingester::initializeMessageQueue");
        exit(2);
    }
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
    
    initializeMessageQueue();
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
                        processFile( watchDirectory, eventName );
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
    
    mq_close(ingestMessageQueue);
    mq_unlink(MQ_INGEST_QUEUE);
    
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
