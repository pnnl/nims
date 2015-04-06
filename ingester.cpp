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

void cleanupiNotify(int fd, int wd)
{
    ( void ) inotify_rm_watch( fd, wd );
    ( void ) close( fd );
}

int main (int argc, char * const argv[]) {
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
	
    string inputpath = options["indir"].as<string>();
    
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
	
	// TODO: make sure input path exists
    clog << "Watching directory " << inputpath << endl;
	
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

    while( 1 ) {
		
        int fd = inotify_init();
		
        if( fd < 0 ) {
            perror( "Error initializing inotify" );
            break;
        }
		
        int wd;
        if( watchDirectory != "" ) {
            // watch for newly created files
            wd = inotify_add_watch( fd, watchDirectory.c_str(), IN_CLOSE_WRITE );
        }
        // watch for newly created directories
        int cd = inotify_add_watch( fd, inputpath.c_str(), IN_CREATE );
		
        int length = read( fd, buffer, BUF_LEN );
		
        if( length < 0 ) {
            perror( "Error reading newly created file" );
            cleanupiNotify(fd,wd);
            cleanupiNotify(fd,cd);
            continue;
        }
		
        int i=0;
        while( i < length ) {
            struct inotify_event *event = ( struct inotify_event * ) &buffer[i];
            if( event->len ) {
                if( event->mask & IN_CREATE ) {
                    if( event->mask & IN_ISDIR ) {
                        watchDirectory = inputpath + "/" + event->name;
                        clog << "Found new directory: " << watchDirectory << endl;
                    }
                } 
                if( event->mask & IN_CLOSE_WRITE ) {
                    if( !(event->mask & IN_ISDIR) ) {
                        clog << "Found file: " << event->name << endl;
                        //processFile( watchDirectory, event->name, jw );
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
        cleanupiNotify( fd, wd );
        cleanupiNotify( fd, cd );
		
    }
	
    
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
