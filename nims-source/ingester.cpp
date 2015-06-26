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
	
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
	
	int sonar_type;
	string m3_host_addr;
	string fb_name;
    fs::path cfgfilepath(options["cfg"].as<string>());
    try 
    {
        YAML::Node config = YAML::LoadFile(cfgfilepath.string());
        sonar_type = config["SONAR_TYPE"].as<int>();
        m3_host_addr = config["M3_HOST_ADDR"].as<string>();
        fb_name = config["FRAMEBUFFER_NAME"].as<string>();
     }
     catch( const std::exception& e )
    {
        cerr << "Error reading config file." << e.what() << endl;
        cerr << desc << endl;
        return -1;
    }
   
 	DataSource *input; // This is a virtual class.   
    
	// TODO: Define an enum or ?
	switch ( sonar_type )
	{
	    case NIMS_SONAR_M3 :  
            input = new DataSourceM3(m3_host_addr);
            break;
        default :
             cerr << "Ingester:  unknown sonar type." << endl;
             return -1;
             break;
     } // switch SONAR_TYPE
    
    
    
	   
     FrameBufferWriter fb(fb_name);
     if ( -1 == fb.Initialize() )
   {
        cerr << "Error initializing frame buffer writer." << endl;
        return -1;
    }
     
    
   
    if ( !input->is_good() )
    {
        cerr << "Error opening data source." << endl;
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
   
    SubprocessCheckin(getpid()); // sync with main NIMS process
      
    clog << "ingester:  source is open!" << endl;
    size_t frame_count=0;
    while ( input->more_data() )
    {
        Frame frame;
        if ( -1 == input->GetPing(&frame) ) break;
        
        // if we get INT during a recv(), GetPing returns -1 before this is hit
        if (sigint_received) {
            cout << "ingester: exiting due to SIGINT" << endl;
            break;
        }
        
        cout << "got frame!" << endl;
        cout << frame.header << endl;
        fb.PutNewFrame(frame);
        ++frame_count;
     }
        
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
