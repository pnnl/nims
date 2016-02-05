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
//#include <opencv2/opencv.hpp>

#include "yaml-cpp/yaml.h"

#include "nims_ipc.h" // NIMS signal handling, queues, shared mem
#include "log.h"      // NIMS logging

using namespace std;
using namespace boost;
namespace fs = boost::filesystem;
//using namespace cv;


int main (int argc, char * const argv[]) {

    string cfgpath, log_level;
    if ( parse_command_line(argc, argv, cfgpath, log_level) != 0 ) return -1;
    setup_logging(string(basename(argv[0])), cfgpath, log_level);
   setup_signal_handling();
    
    
  // READ CONFIG FILE
   int my_var;
   try 
    {
        YAML::Node config = YAML::LoadFile(cfgpath);
        my_var = config["CONFIG_VAR"].as<int>;
      }
     catch( const std::exception& e )
    {
        NIMS_LOG_ERROR << "Error reading config file." << e.what();
        return -1;
    }
     
    
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
    SubprocessCheckin(getpid()); // Synchronize with main NIMS process.
	
    while (1) {
        
        if (sigint_received) {
            cout << "received SIGINT; exiting main loop" << endl;
            break;
        }
        sleep(10);
    }
	   
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
