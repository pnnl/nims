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
    
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
    SubprocessCheckin(getpid()); // Synchronize with main NIMS process.
	
	   
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
