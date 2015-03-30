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

using namespace std;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;


int main (int argc, char * const argv[]) {
	//--------------------------------------------------------------------------
    // PARSE COMMAND LINE
	//
	po::options_description desc;
	desc.add_options()
	("help",                                                    "print help message")
	("cfg,c", po::value<string>()->default_value( "nimsconfig.txt" ),         "path to config file")
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
	//unsigned int bar = options["bar"].as<unsigned int>();
    
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
	
    pid_t pids[10]; // more than enough
    // TODO:  Read list of processes from config file.
    vector<string> proc_filenames;
    proc_filenames.push_back("./ingester");
    proc_filenames.push_back("./detector");
    proc_filenames.push_back("./tracker");
    
    for (int i=0; i<proc_filenames.size(); ++i) {
        cout << proc_filenames[i] << endl;
    }
	   
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
