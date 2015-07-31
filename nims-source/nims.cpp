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
static volatile sig_atomic_t sigchld_received_ = 0;
static vector<nims::Task *> *child_tasks_ = NULL;

static void sigint_handler(int sig)
{
    if (SIGINT == sig)
        sigint_received_ += 1;
}

static void sigchld_handler(int sig)
{
    if (SIGCHLD == sig)
        sigchld_received_ += 1;
}

static void InterruptChildProcesses()
{
    NIMS_LOG_WARNING << "running InterruptChildProcesses";

    // !!! early return if it hasn't been created yet
    if (NULL == child_tasks_) {
        NIMS_LOG_ERROR << "no child tasks running";
        return;
    }
    
    vector<nims::Task *>::iterator it;
    for (it = child_tasks_->begin(); it != child_tasks_->end(); ++it) {
        nims::Task *t = *it;
        if (getpgid(getpid()) == getpgid(t->get_pid())) {
            NIMS_LOG_WARNING << "sending SIGINT to " << t->name()
                << " [" << t->get_pid() << "]";
            t->signal(SIGINT);
            
            // make sure we reap all child processes
            waitpid(t->get_pid(), NULL, 0);
        }
    }
    
    // just in case we screw up and call InterruptChildProcesses twice...
    delete child_tasks_;
    child_tasks_ = NULL;
    
}

/*
  Will not be called if the program terminates abnormally due to
  a signal, but it will be called if we call exit() anywhere.
  This avoids cleaning up child processes after a failure.
*/
static void ExitHandler()
{
    NIMS_LOG_DEBUG << "running ExitHandler";
    InterruptChildProcesses();
    fflush(stderr);
}

static int WaitForTaskLaunch(nims::Task *task, const mqd_t mq)
{
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
    NIMS_LOG_DEBUG << "waiting for task " << task->name() << " to check in...";
        
    if (mq_timedreceive(mq, (char *)&checkin_pid, sizeof(pid_t), NULL, 
            &timeout) == -1) {
        
        if (ETIMEDOUT == errno) {
            NIMS_LOG_ERROR << "check in timed out after " << CHECKIN_TIME_LIMIT_SECONDS << " seconds";
            exit(errno);
        }
        else if (EAGAIN != errno) {
            // any other error
            nims_perror("subtask checkin failed");
            exit(errno);
        }
        
        assert(checkin_pid == task->get_pid());
        
    }
    
    time_t dt = timeout.tv_sec - CHECKIN_TIME_LIMIT_SECONDS - time(NULL);
    NIMS_LOG_DEBUG << task->name() << " checked in after " << dt << " seconds";
    
}

static void LaunchProcessesFromConfig(const YAML::Node &config)
{
    fs::path bin_dir(config["BINARY_DIR"].as<string>());
    
    YAML::Node applications = config["APPLICATIONS"];
    
    // in case we're relaunching processes
    if (NULL != child_tasks_)
        delete child_tasks_;
    
    child_tasks_ = new vector<nims::Task *>;
    
    mqd_t mq = CreateMessageQueue(sizeof(pid_t), MQ_SUBPROCESS_CHECKIN_QUEUE);
    if (-1 == mq) {
        NIMS_LOG_ERROR << "failed to create checkin message queue";
        exit(1);
    }
    
    time_t start_time = time(NULL);
        
    for (int i = 0; i < applications.size(); i++) {
        YAML::Node app = applications[i];
        vector<string> app_args;
        for (int j = 0; j < app["args"].size(); j++)
            app_args.push_back(app["args"][j].as<string>());
        fs::path name(app["name"].as<string>());
        nims::Task *task = new nims::Task(bin_dir / name, app_args);
        if (task->launch()) {
            child_tasks_->push_back(task);
            int err = WaitForTaskLaunch(task, mq);
            if (0 == err) {
                NIMS_LOG_DEBUG << "Launched " << name.string() << " pid " << task->get_pid();
            } else {
                exit(errno);
            }
        }
        else {
            NIMS_LOG_ERROR << "Failed to launch " << name.string();
        }
    }
    
    time_t dt = start_time - time(NULL);
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
    ("log,l", po::value<string>()->default_value("warning"), "debug|warning|error")
    ("daemon,D", "run in background")
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
    
    if (options.count("daemon"))
    {
        NIMS_LOG_WARNING << "daemonizing nims";
        // don't change directory
        daemon(1, 0);
    }
    
    /*
     Use atexit to guarantee cleanup when we exit, primarily with
     nonzero status because of some error condition. I'd like to
     avoid zombies and unmanaged child processes.
    */
    atexit(ExitHandler);
    
    struct sigaction new_action, old_action;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;

    // register for SIGINT
    new_action.sa_handler = sigint_handler;
    sigaction(SIGINT, NULL, &old_action);
    if (SIG_IGN != old_action.sa_handler)
        sigaction(SIGINT, &new_action, NULL);
    
    // register for SIGCHLD
    new_action.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, NULL, &old_action);
    if (SIG_IGN != old_action.sa_handler)
        sigaction(SIGCHLD, &new_action, NULL);
    
    // ignore SIGPIPE
    new_action.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, NULL, &old_action);
    if (SIG_IGN != old_action.sa_handler)
        sigaction(SIGPIPE, &new_action, NULL);  
    
    YAML::Node config;
	try 
	{    
        config = YAML::LoadFile(cfgpath);
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
    
    // set up a dummy pipe for our event loop
    int event_fd[2];
    if (-1 == pipe(event_fd)) {
        nims_perror("pipe failed");
        exit(2);
    }
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = event_fd[0];
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, event_fd[0], &ev) == -1) {
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
            
            if (EINTR == errno) {
            
                // atexit will call InterruptChildProcesses
                if (sigint_received_) {
                    NIMS_LOG_WARNING << "caught SIGINT";            
                    break;
                }
                else if (sigchld_received_) {
                    NIMS_LOG_ERROR << "caught SIGCHLD";
                    static time_t last_relaunch_time_ = 0;
                    time_t current_time = time(NULL);
#define MIN_RELAUNCH_TIME_INTERVAL 60
                    if ((current_time - last_relaunch_time_) > MIN_RELAUNCH_TIME_INTERVAL) {
                        NIMS_LOG_ERROR << "attempting warm restart";
                        InterruptChildProcesses();
                        LaunchProcessesFromConfig(config);
                        last_relaunch_time_ = current_time;
                        NIMS_LOG_ERROR << "warm restart succeeded";
                    }
                    else {
                        // stop and let atexit clean up
                        NIMS_LOG_ERROR << "Last relaunch was less than " 
                            << MIN_RELAUNCH_TIME_INTERVAL
                            << " seconds ago. Exiting.";
                        break;
                    }
                }
                
            } else if (EINTR != errno) {
                // unhandled signal or some other error
                nims_perror("epoll_wait() failed");
                exit(3);
            }
        
        }
        
    }
    
    close(epollfd);
    NIMS_LOG_DEBUG << "nims process shutting down";
    
    return sigint_received_;
}
