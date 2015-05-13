/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  test_frame_buffer_get.cpp
 *  
 *  Created by Shari Matzner on 04/23/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
#include <iostream> // cout, cin, cerr
//#include <fstream>  // ifstream, ofstream
#include <string>   // for strings


//#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "frame_buffer.h"

using namespace std;
using namespace boost;
namespace po = boost::program_options;
//namespace fs = boost::filesystem;


int main (int argc, char * const argv[]) {
	//--------------------------------------------------------------------------
    // PARSE COMMAND LINE
	//
	po::options_description desc;
	desc.add_options()
	("help",                                                    "print help message")
	("buffer,b", po::value<string>()->default_value( "test" ),    "name of frame buffer")
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
	
    string buffer_name = options["buffer"].as<string>();
	//unsigned int bar = options["bar"].as<unsigned int>();
    
	//--------------------------------------------------------------------------
	// DO STUFF
	cout << endl << "Starting " << argv[0] << endl;
	
    try
    {
        FrameBufferReader fb(buffer_name);
        Frame *next_frame;
        int frame_index = -1;
        while (frame_index = fb.GetNextFrame(next_frame) != -1)
        {
            cout << argv[0] << ": " << "got frame " << frame_index << endl;
        }
    }
	catch( const std::exception& e )
	{
        cerr << argv[0] << ":  " << e.what() << endl;
	}
	   
	cout << endl << "Ending " << argv[0] << endl << endl;
    return 0;
}
