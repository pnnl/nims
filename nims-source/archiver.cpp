/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  archiver.cpp
 *  
 *  Created by Shari Matzner on 04/05/2016.
 *  Copyright 2016 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
#include <iostream> // cout, cin, cerr
#include <string>   // for strings


#include <boost/filesystem.hpp>
//#include <opencv2/opencv.hpp>

#include "yaml-cpp/yaml.h"

#include "nims_ipc.h" // NIMS signal handling, queues, shared mem
#include "log.h"      // NIMS logging
#include "tracks_message.h"

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
     
    time_t rawtime;
    time(&rawtime);
    char timestr[16]; // YYYYMMDD-hhmmss
    strftime(timestr, 16, "%Y%m%d-%H%M%S", gmtime(&rawtime));
    fs::path outtxtfilepath("nims_tracks-" + string(timestr) + ".csv");
    NIMS_LOG_DEBUG << "saving track data to " << outtxtfilepath;
    ofstream outtxtfile(outtxtfilepath.string().c_str(),ios::out | ios::binary);
    if (!outtxtfile.is_open())
    {
      NIMS_LOG_ERROR << "Error opening output file: " << outtxtfilepath;
      return (-1);
  }
  print_attribute_labels(outtxtfile);
  outtxtfile << endl;

	//--------------------------------------------------------------------------
	// DO STUFF
	NIMS_LOG_ERROR << "Starting " << argv[0] << endl;
    SubprocessCheckin(getpid()); // Synchronize with main NIMS process.
	
   mqd_t mq_arc = CreateMessageQueue(MQ_TRACKER_ARCHIVER_QUEUE, sizeof(TracksMessage));
    if (mq_det < 0) 
    {
        NIMS_LOG_ERROR << "Error creating MQ_TRACKER_ARCHIVER_QUEUE";
        return -1;
    }

    while (1) {
        
        if (sigint_received) {
            NIMS_LOG_WARNING << "received SIGINT; exiting main loop" << endl;
            break;
        }
        sleep(10);
    }

        // close output files
    outtxtfile.close();

	   
	NIMS_LOG_ERROR << "Ending " << argv[0] << endl << endl;
    return 0;
}
