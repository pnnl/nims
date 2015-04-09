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
#include <stdio.h>  // perror(3)
#include <stdlib.h> // exit()
#include <signal.h>

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
    char **env = environ;
    char **argv = (char **)calloc(args.size() + 2, sizeof(char *));

    // first arg is always the program we're executing
    argv[0] = strdup(absolute_path.string().c_str());

    // copy the remaining arguments as C strings
    int argidx = 1;
    for (vector<string>::const_iterator it = args.begin(); it != args.end(); ++it) {
        argv[argidx] = strdup((*it).c_str());
        argidx++;
    }
    
    int blockpipe[2] = { -1, -1 };
    if (pipe(blockpipe))
        perror("failed to create blockpipe");
    
    // set child process group ID to be the same as parent
    const pid_t pgid = getpgid(getpid());
    
    pid_t pid = fork();
    if (0 == pid) {
        
        (void)setpgid(getpid(), pgid);
        
        // block until the parent finishes any setup
        char ignored;
        read(blockpipe[0], &ignored, 1);
        close(blockpipe[0]);
        
        int ret = execve(argv[0], argv, env);
        _exit(ret);
        
    }
    else if (-1 == pid) {
        int fork_error = errno;
        
        // all setup is complete, so now widow the pipe and exec in the child
        close(blockpipe[0]);   
        close(blockpipe[1]);
    }
    else {
        // parent process
        
        // all setup is complete, so now widow the pipe and exec in the child
        close(blockpipe[0]);   
        close(blockpipe[1]);
    }
    
    // free all of the strdup'ed args
    char **freePtr = argv;
    while (NULL != *freePtr) { 
        free(*freePtr++);
    }
    free(argv);
    
    return pid;
}

volatile int sigint_received = 0;

static void sig_handler(int sig)
{
    if (SIGINT == sig)
        sigint_received++;
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
        if (pid > 0) {
            pids.push_back(pid);
            cerr << "Launched " << name.string() << " as pid " << pid << endl;
        }
        else {
            cerr << "Failed to launch " << name.string() << endl;
        }
    }
    
    cout << "dumping config:" << "\n";
    cout << config << "\n";
    
    cout << "launched processes: " << endl;
    for (vector<pid_t>::iterator it = pids.begin(); it != pids.end(); ++it) {
        cout << "pid: " << *it << endl;
    }
    
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sig_handler);
    
    while (true) {
        
        if (sigint_received) {
            
            cerr << "received SIGINT" << endl;
            
            for (vector<pid_t>::iterator it = pids.begin(); it != pids.end(); ++it) {
                
                const pid_t child_pid = *it;
                // of course, this is a race condition if true and the child exits...
                if (getpgid(getpid()) == getpgid(child_pid)) {
                    cerr << "sending signal to child pid " << child_pid << endl;
                    kill(child_pid, SIGINT);
                }
                else {
                    cerr << "pid: " << child_pid << " is not part of this process group" << endl;
                }
            }
            
            //killpg(getpgrp(), SIGINT);
            
            exit(1);
        }
        
        sleep(1);
    }
    
    /*
    Need to wait here?
    */

    return 0;
}
