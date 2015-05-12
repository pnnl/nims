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

// for POSIX shared memory
#include <fcntl.h>    // O_* constants
#include <unistd.h>   // sysconf
#include <sys/mman.h> // mmap, shm_open
#include "queues.h"
#include <sys/epoll.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include "yaml-cpp/yaml.h"

#include "nims_includes.h"
#include "task.h"

using namespace std;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

static volatile sig_atomic_t sigint_received_ = 0;
static vector<nims::Task *> *child_tasks_ = NULL;

static void ProcessSharedFramebufferMessage(const NimsIngestMessage *msg)
{        
    int fd = shm_open(msg->shm_open_name, O_RDONLY, S_IRUSR);
    
    // !!! early return
    if (-1 == fd) {
        perror("shm_open() in nims::ProcessSharedFramebufferMessage");
        return;
    }
    
    // size of mmap region
    assert(msg->mapped_data_length > sizeof(NimsFramebuffer));
    cout << "expected mmap data name: " << msg->shm_open_name << endl;
    cout << "expected mmap data size: " << msg->mapped_data_length << endl;

    // mmap a shared framebuffer on the file descriptor we have from shm_open
    NimsFramebuffer *shared_buffer;
    shared_buffer = (NimsFramebuffer *)mmap(NULL, msg->mapped_data_length,
            PROT_READ, MAP_PRIVATE, fd, 0);
    
    close(fd);

    // !!! early return
    if (MAP_FAILED == shared_buffer) {
        perror("mmap() in nims::ProcessSharedFramebufferMessage");
        shm_unlink(msg->shm_open_name);
    } else {

        assert(shared_buffer->data_length + sizeof(NimsFramebuffer) 
                <= msg->mapped_data_length);

        // !!! extract the shared content as a C string for debugging _ONLY_
        char *content = (char *)malloc(shared_buffer->data_length + 1);
        memset(content, '\0', shared_buffer->data_length + 1);
        memcpy(content, &shared_buffer->data, shared_buffer->data_length);
        cout << "shared data length " << shared_buffer->data_length << endl;
        cout << "shared data content " << content << endl;
        free(content);

        munmap(shared_buffer, msg->mapped_data_length);
        shm_unlink(msg->shm_open_name);
    }
}

static void SigintHandler(int sig)
{
    if (SIGINT == sig)
        sigint_received_ = 1;
}

static void SignalChildProcesses(const int sig)
{
    cerr << "running nims::SignalChildProcesses" << endl;

    // !!! early return if it hasn't been created yet
    if (NULL == child_tasks_) return;
    
    vector<nims::Task *>::iterator it;
    for (it = child_tasks_->begin(); it != child_tasks_->end(); ++it) {
        nims::Task *t = *it;
        if (getpgid(getpid()) == getpgid(t->get_pid())) {
            cerr << "sending signal to child " << t->get_pid() << endl;
            t->signal(sig);
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
    cerr << "running nims::ExitHandler" << endl;
    SignalChildProcesses(SIGINT);
    fflush(stderr);
}

static void LaunchProcessesFromConfig(const YAML::Node &config)
{
    fs::path bin_dir(config["BINARY_DIR"].as<string>());
    
    YAML::Node applications = config["APPLICATIONS"];
    child_tasks_ = new vector<nims::Task *>;
    
    mqd_t mq = CreateMessageQueue(sizeof(pid_t), MQ_SUBPROCESS_CHECKIN_QUEUE);
    if (-1 == mq) {
        cerr << "nims: failed to create message queue" << endl;
        exit(1);
    }
    
    for (int i = 0; i < applications.size(); i++) {
        YAML::Node app = applications[i];
        vector<string> app_args;
        for (int j = 0; j < app["args"].size(); j++)
            app_args.push_back(app["args"][j].as<string>());
        fs::path name(app["name"].as<string>());
        nims::Task *Task = new nims::Task(bin_dir / name, app_args);
        if (Task->launch()) {
            child_tasks_->push_back(Task);
            cerr << "Launched " << name.string() << " pid " << Task->get_pid() << endl;
        }
        else {
            cerr << "Failed to launch " << name.string() << endl;
        }
    }
    
    cout << "launched processes: " << endl;
    vector<nims::Task *>::iterator it;
    for (it = child_tasks_->begin(); it != child_tasks_->end(); ++it) {
        cout << "pid: " << (*it)->get_pid() << endl;
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
    cerr << "waiting for tasks to check in..." << endl;
    do { 
        
        if (mq_timedreceive(mq, (char *)&checkin_pid, sizeof(pid_t), NULL, 
                &timeout) == -1) {
            
            if (ETIMEDOUT == errno) {
                cerr << "subtask(s) failed to checkin after 10 seconds" << endl;
                exit(errno);
            }
            cerr << "failed to receive checkin message" << endl;
            exit(errno);
        }

    } while(--remaining);
    
    time_t dt = timeout.tv_sec - CHECKIN_TIME_LIMIT_SECONDS - time(NULL);
    cerr << "all tasks checked in after " << dt << " seconds" << endl;
    
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

    /*
     Use atexit to guarantee cleanup when we exit, primarily with
     nonzero status because of some error condition. I'd like to
     avoid zombies and unmanaged child processes.
    */
    atexit(ExitHandler);
    
    // some default registrations for cleanup
    struct sigaction new_action, old_action;
    new_action.sa_handler = SigintHandler;
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
	    
    fs::path cfgfilepath( options["cfg"].as<string>() );
    YAML::Node config = YAML::LoadFile(cfgfilepath.string());
    
    // launch all default processes listed in the config file
    LaunchProcessesFromConfig(config);
    
    // launching subprocesses may or may not create this
    mqd_t ingest_queue = CreateMessageQueue(sizeof(NimsIngestMessage),
            MQ_INGEST_QUEUE);

    if (-1 == ingest_queue) {
        cerr << "nims: failed to create message queue" << endl;
        exit(1);
    }
     
    int epollfd = epoll_create(1);
    if (-1 == epollfd) {
        perror("nims: epollcreate() failed");
        exit(1);
    }
        
    // monitor our ingest queue descriptor
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = ingest_queue;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ingest_queue, &ev) == -1) {
        perror("nims: epoll_ctl() failed");
        exit(2);
    }

    while (true) {
        
        cerr << "### entering epoll" << endl;
        // no particular reason for 10, but it was in sample code that I grabbed
#define EVENT_MAX 10
        struct epoll_event events[EVENT_MAX];
        
        // pass -1 for timeout to block indefinitely
        int nfds = epoll_wait(epollfd, events, EVENT_MAX, -1);
        cerr << "### epoll_wait returned" << endl;
        
        // handle error condition first, in case we're exiting on a signal
        if (-1 == nfds) {
            
            if (EINTR == errno && sigint_received_) {
            
                // atexit will call SignalChildProcesses
                cerr << "nims: received SIGINT" << endl;            
                break;
                
            } else if (EINTR != errno) {
                // unhandled signal or some other error
                perror("nims: epoll_wait() failed");
                exit(3);
            }
        
        }
        
        for (int n = 0; n < nfds; ++n) {
            
            if (events[n].data.fd == ingest_queue) {
                
                /*
                 Process a single message per loop; this avoids blocking
                 on mq_receive, since we don't get SIGINT if we do while().
                 Queued messages are still processed, though; I tested this
                 by tickling the ingester several times, then launching nims
                 to see it process all of the enqueued messages.
                */
                NimsIngestMessage msg;
                if (mq_receive (ingest_queue, (char *)&msg, 
                        sizeof(msg), NULL) != -1) 
                    ProcessSharedFramebufferMessage(&msg);
                
            } else {
                cerr << "*** ERROR *** Monitoring file descriptor with no event handler" << endl;
            }
        }
        
    }
    
    close(epollfd);
    mq_close(ingest_queue);
    mq_unlink(MQ_INGEST_QUEUE);
    
    // just in case we screw up and call SignalChildProcesses twice...
    delete child_tasks_;
    child_tasks_ = NULL;

    return sigint_received_;
}
