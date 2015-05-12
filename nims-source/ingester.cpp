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

using namespace std;
//using namespace cv;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

#define EVENT_SIZE ( sizeof(struct inotify_event) )
#define BUF_LEN ( 1024 * (EVENT_SIZE + 16) )

static volatile int sigint_received = 0;

// returns length of data copied to shared memory
static size_t ProcessFile(string &watchDirectory, string &fileName)
{    
    cout << "processing file " << fileName << " in dir " << watchDirectory << endl;
    
    
    return 0;
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
    
    //SubprocessCheckin(getpid());

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
