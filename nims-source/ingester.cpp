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
#include "log.h"

using namespace std;
//using namespace cv;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

// TODO: Somehow the defined sonar types need to 
//       be accessible to users for the config file,
//       maybe for user interface
#define NIMS_SONAR_M3 1


static volatile int sigint_received = 0;

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
    ("log,l", po::value<string>()->default_value("debug"), "debug|warning|error")
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
    
    std::string cfgpath = options["cfg"].as<string>();
    setup_logging(string(basename(argv[0])), cfgpath, options["log"].as<string>());
	
	//--------------------------------------------------------------------------
	// DO STUFF
	NIMS_LOG_DEBUG << "Starting " << argv[0];
	
	int sonar_type;
	string sonar_host_addr;
	string fb_name;
    try 
    {
        YAML::Node config = YAML::LoadFile(cfgpath);
        sonar_type = config["SONAR_TYPE"].as<int>();
        sonar_host_addr = config["SONAR_HOST_ADDR"].as<string>();
        fb_name = config["FRAMEBUFFER_NAME"].as<string>();
     }
     catch( const std::exception& e )
    {
        NIMS_LOG_ERROR << "Error reading config file." << e.what();
        NIMS_LOG_ERROR << desc;
        return -1;
    }
    
    struct sigaction new_action, old_action;
    new_action.sa_handler = sig_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGINT, NULL, &old_action);
    if (SIG_IGN != old_action.sa_handler)
        sigaction(SIGINT, &new_action, NULL);
    
    // ignore SIGPIPE
    new_action.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, NULL, &old_action);
    if (SIG_IGN != old_action.sa_handler)
        sigaction(SIGPIPE, &new_action, NULL);  
   
    // check in before blocking while creating the DataSource
    SubprocessCheckin(getpid()); // sync with main NIMS process
    
    // create fb before datasource, so tracker doesn't bail out
    // when it tries to open a nonexistent writer queue
    FrameBufferWriter fb(fb_name);
    if ( -1 == fb.Initialize() )
    {
       NIMS_LOG_ERROR << "Error initializing frame buffer writer.";
       return -1;
    }
   
 	DataSource *input; // This is a virtual class.   
    
	// TODO: Define an enum or ?  Need to match config file definition.
	switch ( sonar_type )
	{
	    case NIMS_SONAR_M3 :  
            NIMS_LOG_DEBUG << "opening M3 sonar as datasource";
            input = new DataSourceM3(sonar_host_addr);
            break;
        default :
             NIMS_LOG_ERROR << "Ingester:  unknown sonar type: " << sonar_type;
             return -1;
             break;
     } // switch SONAR_TYPE    
   
    // TODO:  May want to check errno to see why connect failed.
    input->connect();
    while ( !input->is_good() )
    {
        NIMS_LOG_DEBUG << "waiting to connect to source...";
        sleep(5);
        input->connect();
   }
    
    NIMS_LOG_DEBUG << "connected to source!";
    size_t frame_count=0;
    while ( input->more_data() )
    {
        Frame frame;
        if ( -1 == input->GetPing(&frame) ) break;
        
        // if we get INT during a recv(), GetPing returns -1 before this is hit
        if (sigint_received) {
            NIMS_LOG_WARNING << "ingester: exiting due to SIGINT";
            break;
        }
        
        NIMS_LOG_DEBUG << "got frame!";
        NIMS_LOG_DEBUG << frame.header << endl;
        fb.PutNewFrame(frame);
        ++frame_count;
     }
        
	NIMS_LOG_DEBUG << "Ending runloop; SIGINT: " << sigint_received;
    return 0;
}
