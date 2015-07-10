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
#include <string>   // for strings
#include <unordered_map>

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>  // perror(3)
#include <stdlib.h> // exit()
#include <signal.h>
#include <sys/epoll.h>
#include <sys/wait.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "yaml-cpp/yaml.h"

#include "queues.h"
#include "task.h"//
#include "log.h"

using namespace std;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

static volatile sig_atomic_t sigint_received_ = 0;
static vector<nims::Task *> *child_tasks_ = NULL;

static void sigint_handler(int sig)
{
    if (SIGINT == sig)
        sigint_received_ = 1;
}

static void InterruptChildProcesses()
{
    NIMS_LOG_WARNING << "running nims::InterruptChildProcesses";

    // !!! early return if it hasn't been created yet
    if (NULL == child_tasks_) return;
    
    vector<nims::Task *>::iterator it;
    for (it = child_tasks_->begin(); it != child_tasks_->end(); ++it) {
        nims::Task *t = *it;
        if (getpgid(getpid()) == getpgid(t->get_pid())) {
            NIMS_LOG_WARNING << "sending signal to child " << t->get_pid();
            t->signal(SIGINT);
            
            // make sure we reap all child processes
            waitpid(t->get_pid(), NULL, 0);
        }
    }
}

/*
  Will not be called if the program terminates abnormally due to
  a signal, but it will be called if we call exit() anywhere.
  This avoids cleaning up child processes after a failure.
*/
static void ExitHandler()
{
    NIMS_LOG_DEBUG << "running nims::ExitHandler";
    InterruptChildProcesses();
    fflush(stderr);
}

static void LaunchProcessesFromConfig(const YAML::Node &config)
{
    fs::path bin_dir(config["BINARY_DIR"].as<string>());
    
    YAML::Node applications = config["APPLICATIONS"];
    child_tasks_ = new vector<nims::Task *>;
    
    mqd_t mq = CreateMessageQueue(sizeof(pid_t), MQ_SUBPROCESS_CHECKIN_QUEUE);
    if (-1 == mq) {
        NIMS_LOG_ERROR << "failed to create message queue";
        exit(1);
    }
    
    std::unordered_map<pid_t, std::string> checkin_map;
    
    for (int i = 0; i < applications.size(); i++) {
        YAML::Node app = applications[i];
        vector<string> app_args;
        for (int j = 0; j < app["args"].size(); j++)
            app_args.push_back(app["args"][j].as<string>());
        fs::path name(app["name"].as<string>());
        nims::Task *Task = new nims::Task(bin_dir / name, app_args);
        if (Task->launch()) {
            child_tasks_->push_back(Task);
            NIMS_LOG_DEBUG << "Launched " << name.string() << " pid " << Task->get_pid();
            checkin_map[Task->get_pid()] = name.string();
        }
        else {
            NIMS_LOG_ERROR << "Failed to launch " << name.string();
        }
    }

    /*
      Ensure that all processes are launched before continuing. This
      is a way to sidestep some race conditions, but also ensures
      that we're starting with a consistent state (all programs
      and queues running) before we start waiting for messages.
    */
#define CHECKIN_TIME_LIMIT_SECONDS 10
    struct timespec timeout;
    timeout.tv_sec = time(NULL) + CHECKIN_TIME_LIMIT_SECONDS;
    timeout.tv_nsec = 0;

    pid_t checkin_pid;
    int remaining = applications.size();
    NIMS_LOG_DEBUG << "waiting for tasks to check in...";
    do { 
        
        if (mq_timedreceive(mq, (char *)&checkin_pid, sizeof(pid_t), NULL, 
                &timeout) == -1) {
            
            // log which task(s) haven't checked in yet
            for (auto it = checkin_map.begin(); it != checkin_map.end(); ++it)
                NIMS_LOG_ERROR << it->second << " (pid " << it->first << ") failed to check in";
            
            if (ETIMEDOUT == errno) {
                NIMS_LOG_ERROR << "check in timed out after " << CHECKIN_TIME_LIMIT_SECONDS << " seconds";
                exit(errno);
            }
            
            // any other error
            nims_perror("subtask checkin failed");
            exit(errno);
        }
        
        checkin_map.erase(checkin_pid);

    } while(--remaining);
    
    time_t dt = timeout.tv_sec - CHECKIN_TIME_LIMIT_SECONDS - time(NULL);
    NIMS_LOG_DEBUG << "all tasks checked in after " << dt << " seconds";
    
    mq_close(mq);
    mq_unlink(MQ_SUBPROCESS_CHECKIN_QUEUE);
}

int main (int argc, char * argv[]) {
	//--------------------------------------------------------------------------
    // PARSE COMMAND LINE
	//
	po::options_description desc;
	desc.add_options()
	("help",                                                    "print help message")
    ("cfg,c", po::value<string>()->default_value("config.yaml"),         "path to config file")
    ("log,l", po::value<string>()->default_value("debug"), "debug|warning|error")
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
    
    std::string cfgpath = options["cfg"].as<string>();
    setup_logging(string(basename(argv[0])), cfgpath, options["log"].as<string>());
        
    /*
     Use atexit to guarantee cleanup when we exit, primarily with
     nonzero status because of some error condition. I'd like to
     avoid zombies and unmanaged child processes.
    */
    atexit(ExitHandler);
    
    // some default registrations for cleanup
    struct sigaction new_action, old_action;
    new_action.sa_handler = sigint_handler;
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
	try 
	{    
        YAML::Node config = YAML::LoadFile(cfgpath);
        // launch all default processes listed in the config file
        LaunchProcessesFromConfig(config);
    }
    catch( const std::exception& e )
    {
        NIMS_LOG_ERROR << "Error reading config file: " << cfgpath << endl; 
        NIMS_LOG_ERROR << e.what() << endl;
        NIMS_LOG_ERROR << desc << endl;
        return -1;
    }
       
     
    int epollfd = epoll_create(1);
    if (-1 == epollfd) {
        nims_perror("epollcreate() failed");
        exit(1);
    }
    
    // monitor stdin, just to have a dummy fd for our event loop
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1) {
        nims_perror("epoll_ctl() failed");
        exit(2);
    }

    while (true) {
        
        NIMS_LOG_DEBUG << "### entering epoll";
        // no particular reason for 10, but it was in sample code that I grabbed
#define EVENT_MAX 10
        struct epoll_event events[EVENT_MAX];
        
        // pass -1 for timeout to block indefinitely
        int nfds = epoll_wait(epollfd, events, EVENT_MAX, -1);
        
        NIMS_LOG_DEBUG << "### epoll_wait returned";
        
        // handle error condition first, in case we're exiting on a signal
        if (-1 == nfds) {
            
            if (EINTR == errno && sigint_received_) {
            
                // atexit will call InterruptChildProcesses
                NIMS_LOG_WARNING << "nims: received SIGINT";            
                break;
                
            } else if (EINTR != errno) {
                // unhandled signal or some other error
                nims_perror("epoll_wait() failed");
                exit(3);
            }
        
        }
        
    }
    
    close(epollfd);
    NIMS_LOG_DEBUG << "nims process shutting down";
    
    // just in case we screw up and call InterruptChildProcesses twice...
    delete child_tasks_;
    child_tasks_ = NULL;

    return sigint_received_;
}
