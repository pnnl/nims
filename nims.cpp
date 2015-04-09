/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  nims.cpp
 *  
 *  Created by Firstname Lastname on mm/dd/yyyy.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
#include <iostream> // cout, cin, cerr
//#include <fstream>  // ifstream, ofstream
#include <string>   // for strings

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h> // exit()

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "yaml-cpp/yaml.h"

using namespace std;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

#define CFG_PATH "/home/amaxwell/NIMS/nims-source/config.yaml"

static pid_t nims_launch_process(const fs::path& absolute_path, const vector<string>& args)
{
    cout << string(__func__) + string(":\n\t") + absolute_path.string() << endl;
    for (vector<string>::const_iterator it = args.begin(); it != args.end(); ++it)
        cout << "\t\t" + *it << endl;
    static pid_t pid = 0;
    return ++pid;
}

int main (int argc, char * argv[]) {
	//--------------------------------------------------------------------------
    // PARSE COMMAND LINE
	//
	po::options_description desc;
	desc.add_options()
	("help",                                                    "print help message")
	("cfg,c", po::value<string>()->default_value( CFG_PATH ),         "path to config file")
	//("bar,b",   po::value<unsigned int>()->default_value( 101 ),"an integer value")
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
	
    fs::path cfgfilepath( options["cfg"].as<string>() );
    YAML::Node config = YAML::LoadFile(cfgfilepath.string());
    fs::path bin_dir(config["BINARY_DIR"].as<string>());
    
    YAML::Node applications = config["APPLICATIONS"];
    vector<pid_t> pids;
    
    for (int i = 0; i < applications.size(); i++) {
        YAML::Node app = applications[i];
        vector<string> app_args;
        for (int j = 0; j < app["args"].size(); j++)
            app_args.push_back(app["args"][j].as<string>());
        fs::path name(app["name"].as<string>());
        const pid_t pid = nims_launch_process(bin_dir / name, app_args);
        if (pid > 0)
            pids.push_back(pid);
    }
    
    cout << "dumping config:" << "\n";
    cout << config << "\n";
    
    cout << "launched processes: " << endl;
    for (vector<pid_t>::iterator it = pids.begin(); it != pids.end(); ++it) {
        cout << "pid: " << *it << endl;
    }
    
    /*
    Need to wait here?
    */

    return 0;
}
