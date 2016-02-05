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
#include "yaml-cpp/yaml.h"

#include "data_source_m3.h"
#include "data_source_blueview.h"
#include "data_source_ek60.h"
#include "frame_buffer.h"
#include "nims_ipc.h" 
#include "log.h"

using namespace std;
//using namespace cv;
using namespace boost;
namespace fs = boost::filesystem;

// TODO: Somehow the defined sonar types need to 
//       be accessible to users for the config file,
//       maybe for user interface
#define NIMS_SONAR_M3 1
#define NIMS_SONAR_BLUEVIEW 2
#define NIMS_SONAR_EK60 3


int main (int argc, char * argv[]) {

    string cfgpath, log_level;
    if ( parse_command_line(argc, argv, cfgpath, log_level) != 0 ) return -1;
    setup_logging(string(basename(argv[0])), cfgpath, log_level);
   setup_signal_handling();
    
	
	//--------------------------------------------------------------------------
	// DO STUFF
	NIMS_LOG_DEBUG << "Starting " << argv[0];
	
	int sonar_type;
	string sonar_host_addr;
  int sonar_port;
	string fb_name;
    try 
    {
        YAML::Node config = YAML::LoadFile(cfgpath);
        NIMS_LOG_DEBUG << "opened config file " << cfgpath;
        sonar_type = config["SONAR_TYPE"].as<int>();
        NIMS_LOG_DEBUG << "SONAR_TYPE: " << sonar_type;
        sonar_host_addr = config["SONAR_HOST_ADDR"].as<string>();
        NIMS_LOG_DEBUG << "SONAR_HOST_ADDR: " << sonar_host_addr;
        sonar_port = config["SONAR_PORT"].as<int>();
        NIMS_LOG_DEBUG << "SONAR_PORT: " << sonar_port;
        fb_name = config["FRAMEBUFFER_NAME"].as<string>();
        NIMS_LOG_DEBUG << "FRAMEBUFFER_NAME: " << fb_name;
     }
     catch( const std::exception& e )
    {
        NIMS_LOG_ERROR << "Error reading config file." << e.what();
        return -1;
    }
    
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
 	    case NIMS_SONAR_BLUEVIEW :
            NIMS_LOG_DEBUG << "opening BlueView sonar as datasource";
            input = new DataSourceBlueView(sonar_host_addr);
            break;
	    case NIMS_SONAR_EK60 :
            NIMS_LOG_DEBUG << "opening EK60 sonar as datasource";
            input = new DataSourceEK60(sonar_host_addr, sonar_port);
            break;
       default :
             NIMS_LOG_ERROR << "unknown sonar type: " << sonar_type;
             return -1;
             break;
     } // switch SONAR_TYPE    
   
    // TODO:  May want to check errno to see why connect failed.
    do
    {
        input->connect();
        
        // may get SIGINT at any time to reload config
        if (sigint_received) {
            NIMS_LOG_WARNING << "exiting due to SIGINT";
            break;
        }
        
        // no need to sleep if we connected
        if (input->is_good())
            break;
        
        NIMS_LOG_DEBUG << "waiting to connect to source...";
        sleep(5);
        
   } while ( !input->is_good() );
    
   // Sprinkling the checks for sigint_received everywhere is kind
   // of gross, but we have multiple loops on blocking calls that
   // can be interrupted by a signal, so there's not much choice.
   // If the IP is wrong and you edit the config file and send HUP
   // to nims to reload, it'll send ingester an INT; if we only break
   // out of the first loop, we could then enter the one below.
   // Calling exit() is an option, but may prevent destructors from
   // running. Could just return and avoid any final cleanup?
   if (0 == sigint_received) 
   {
       NIMS_LOG_DEBUG << "connected to source!";
       size_t frame_count=0;
       while ( input->more_data() )
       {
           Frame frame;
           if ( -1 == input->GetPing(&frame) ) break;
    
           // if we get INT during a recv(), GetPing returns -1 
           if (sigint_received) {
               NIMS_LOG_WARNING << "exiting due to SIGINT";
               break;
           }
    
           NIMS_LOG_DEBUG << "got frame!";
           NIMS_LOG_DEBUG << frame.header << endl;
           fb.PutNewFrame(frame);
           ++frame_count;
        }
    }
        
	NIMS_LOG_DEBUG << "Ending runloop; SIGINT: " << sigint_received;
    return 0;
}
