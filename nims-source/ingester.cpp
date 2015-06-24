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
#include "yaml-cpp/yaml.h"

#include "data_source_m3.h"
#include "frame_buffer.h"
#include "queues.h" // SubprocessCheckin

using namespace std;
//using namespace cv;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

// Moved the size of the buffer to where the buffer is declared.
//#define EVENT_SIZE ( sizeof(struct inotify_event) )
//#define BUF_LEN ( 1024 * (EVENT_SIZE + 16) )

static volatile int sigint_received = 0;

// returns number of processed frames
static size_t ProcessFile(const string &watchDirectory, const string &fileName, FrameBufferWriter &fb)
{
    cout << "processing file " << fileName << " in dir " << watchDirectory << endl;
    DataSourceM3 input(string(watchDirectory + "/" + fileName));
    if ( !input.is_good() )
    {
        cout << "source NOT open. :(" << endl;
        return 0;
    }
    cout << "source is open!" << endl;
    size_t frame_count=0;
    while ( input.more_data() )
    {
        Frame frame;
        if ( -1 == input.GetPing(&frame) ) break;
        cout << "got frame!" << endl;
        cout << frame.header << endl;
        fb.PutNewFrame(frame);
        ++frame_count;
		// TODO:  This is temporary fix to keep from reading file faster than realtime
		std::this_thread::sleep_for (std::chrono::milliseconds(100));
    }
    
    return frame_count;
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
    // TODO:  boost options is overkill if we just have one arg (see above).
	po::options_description desc;
	desc.add_options()
	("help",                                                     "print help message")
    ("cfg,c", po::value<string>()->default_value("config.yaml"), "path to config file")
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
	
    fs::path cfgfilepath(options["cfg"].as<string>());
    YAML::Node config = YAML::LoadFile(cfgfilepath.string());
	// TODO: get params from config file
    string M3_host_addr("130.20.41.25");
    
    
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
    
    FrameBufferWriter fb("nims", config["FB_WRITER_QUEUE"].as<string>());
    
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sig_handler);
	
    
    SubprocessCheckin(getpid()); // sync with main NIMS process
    
        DataSourceM3 input(M3_host_addr);
        if ( !input.is_good() )
        {
            cerr << "source NOT open. :(" << endl;
            return -1;
        }
        clog << "ingester:  source is open!" << endl;
        size_t frame_count=0;
        while ( input.more_data() )
        {
            Frame frame;
            if ( -1 == input.GetPing(&frame) ) break;
            cout << "got frame!" << endl;
            cout << frame.header << endl;
            fb.PutNewFrame(frame);
            ++frame_count;
         }
        
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
