/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  filename.cpp
 *  
 *  Created by Firstname Lastname on mm/dd/yyyy.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
#include <iostream> // cout, cin, cerr
#include <string>   // for strings
#include <signal.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
//#include <opencv2/opencv.hpp>

#include "yaml-cpp/yaml.h"

#include "queues.h" // SubprocessCheckin()

using namespace std;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;
//using namespace cv;

static volatile sig_atomic_t sigint_received_ = 0;

static void sigint_handler(int sig)
{
    if (SIGINT == sig)
        sigint_received_ = 1;
}

int main (int argc, char * const argv[]) {
	//--------------------------------------------------------------------------
    // PARSE COMMAND LINE
	//
	po::options_description desc;
	desc.add_options()
	("help",                                                    "print help message")
	("cfg,c", po::value<string>()->default_value( "./nims-config.yaml" ), "path to config file; default is ./nims-config.yaml")
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
	
    // READ CONFIG FILE
    fs::path cfgfilepath( options["cfg"].as<string>() );
    if ( ! (fs::exists(cfgfilepath) && fs::is_regular_file(cfgfilepath)) )
    {
        cerr << "Can't find config file " << cfgfilepath.string() << endl;
        return -1;
    }
    YAML::Node config = YAML::LoadFile(cfgfilepath.string()); // throws exception if bad path
    
    // some default registrations for cleanup
    struct sigaction new_action, old_action;
    new_action.sa_handler = sigint_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGINT, NULL, &old_action);
    if (SIG_IGN != old_action.sa_handler)
        sigaction(SIGINT, &new_action, NULL);
    
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
    SubprocessCheckin(getpid()); // Synchronize with main NIMS process.
	
    while (1) {
        
        if (sigint_received) {
            cout << "received SIGINT; exiting event loop" << endl;
            break;
        }
        sleep(10);
    }
	   
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
