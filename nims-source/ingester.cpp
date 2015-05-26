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


//#include <opencv2/opencv.hpp>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "data_source_m3.h"
#include "frame_buffer.h"

using namespace std;
//using namespace cv;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

// Moved the size of the buffer to where the buffer is declared.
//#define EVENT_SIZE ( sizeof(struct inotify_event) )
//#define BUF_LEN ( 1024 * (EVENT_SIZE + 16) )

static volatile int sigint_received = 0;

// returns length of data copied to shared memory
static size_t ProcessFile(const string &watchDirectory, const string &fileName, FrameBufferWriter &fb)
{
    cout << "processing file " << fileName << " in dir " << watchDirectory << endl;
    DataSourceM3 input(string(watchDirectory + "/" + fileName));
    if ( !input.is_open() )
    {
        cout << "source NOT open. :(" << endl;
        return 0;
    }
    cout << "source is open!" << endl;
    Frame frame;
    input.GetPing(&frame);
    cout << "got frame!" << endl;
    cout << frame.header << endl;
    fb.PutNewFrame(frame);
    
    return 1;
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
	// TODO:  Make one input arg, the path of the config file.  Then get other
	//        params from config file.
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
	
	// TODO: make sure input path exists
    string inputDirectory = options["indir"].as<string>();
    
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
    
    FrameBufferWriter fb("ingester");
    
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sig_handler);
	
    // Watch the input directory for new files.
    size_t BASE_EVENT_SIZE = sizeof(struct inotify_event);
    size_t BUF_LEN = 32 * (BASE_EVENT_SIZE + NAME_MAX + 1); // max size of one event with pathname in it
    clog << "Buffer length is " << BUF_LEN << " bytes." << endl;
    char buffer[BUF_LEN]; //
    
    int fd = inotify_init();
    
    if( fd < 0 ) {
        perror( "Error initializing inotify" );
        exit(1);
    }
    
    // watch for new files
    int wd = inotify_add_watch( fd, inputDirectory.c_str(), IN_CLOSE_WRITE );
    clog << "Watching directory " << inputDirectory << ", wd = " << wd << endl;
    
    //SubprocessCheckin(getpid()); // sync with main NIMS process
    
    while( 1 ) {
		
        // read event buffer
        int length = read( fd, buffer, BUF_LEN ); // blocking read
		
        // in case the read() was interrupted by a signal
        if (sigint_received) {
            cerr << "ingester received SIGINT; cleaning up" << endl;
            inotify_rm_watch(fd, wd);
            break;
        }
        
        if( length < 0 ) {
            perror( "Error in read() on inotify file descriptor" );
            inotify_rm_watch(fd, wd);
            break;
        }
		
        clog << "Read " << length << " bytes from event buffer. " << endl;
        int i=0;
        while( i < length ) {
            clog << "reading event at index i = " << i << endl;
            struct inotify_event *event = ( struct inotify_event * ) &buffer[i];
            clog <<  "event length:  " << event->len << ", wd = " << event->wd << endl;
            
            if( event->mask & IN_IGNORED ) {
                clog << "Input dir no longer exists." << endl;
                close(fd);  // close event queue to trigger error in outer loop
                break;
            }
            if( event->mask & IN_CLOSE_WRITE ) {
                if( !(event->mask & IN_ISDIR) ) {
                    clog << "Found file: " << event->name << endl;
                    string eventName(event->name);
                    ProcessFile(inputDirectory, eventName, fb);
                    clog << "Done processing file." << endl;
                }
            }
            i += BASE_EVENT_SIZE + event->len;
        } // while (i < length)
        
        
    } // main event loop
	
    inotify_rm_watch(fd, wd);
    close(fd);
    
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
