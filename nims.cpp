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
#include <sys/resource.h>

// for POSIX shared memory
#include "nims_includes.h"
#include <fcntl.h>    // O_* constants
#include <unistd.h>   // sysconf
#include <sys/mman.h> // mmap, shm_open
#include <mqueue.h>
#include <sys/epoll.h>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include "yaml-cpp/yaml.h"

using namespace std;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

static volatile int sigint_received = 0;

// !!! create NimsTask objects that are killable and track name/pid
static pid_t LaunchProcess(const fs::path &absolute_path, 
        const vector<string> &args)
{    
    char **env = environ;
    char **argv = (char **)calloc(args.size() + 2, sizeof(char *));

    // first arg is always the program we're executing
    argv[0] = strdup(absolute_path.string().c_str());

    // copy the remaining arguments as C strings
    int argidx = 1;
    vector<string>::const_iterator it;
    for (it = args.begin(); it != args.end(); ++it) {
        argv[argidx] = strdup((*it).c_str());
        argidx++;
    }
    
    int blockpipe[2] = { -1, -1 };
    if (pipe(blockpipe))
        perror("failed to create blockpipe");
    
   /*
    Figure out the max number of file descriptors for a process; 
    getrlimit is not listed as async-signal safe in the sigaction(2) 
    man page, so we assume it's not safe to call after  fork().
    The fork(2) page says that child rlimits are set to zero.
    */
   int max_open_files = sysconf(_SC_OPEN_MAX);
   struct rlimit open_file_limit;
   if (getrlimit(RLIMIT_NOFILE, &open_file_limit) == 0)
       max_open_files = (int)open_file_limit.rlim_cur;
    
    // set child process group ID to be the same as parent
    const pid_t pgid = getpgid(getpid());
    
    pid_t pid = fork();
    if (0 == pid) {
        
        (void)setpgid(getpid(), pgid);
        
        int j;
        for (j = (STDERR_FILENO + 1); j < max_open_files; j++) {
            
            // don't close this until we're done reading from it!
            if (blockpipe[0] != j)
                (void) close(j);
        }
        
        // block until the parent finishes any setup
        char ignored;
        read(blockpipe[0], &ignored, 1);
        close(blockpipe[0]);
        
        int ret = execve(argv[0], argv, env);
        _exit(ret);
        
    } else if (-1 == pid) {
        int fork_error = errno;
        
        // all setup is complete, so now widow the pipe and exec in the child
        close(blockpipe[0]);   
        close(blockpipe[1]);
    } else {
        // parent process
        
        // all setup is complete, so now widow the pipe and exec in the child
        close(blockpipe[0]);   
        close(blockpipe[1]);
    }
    
    // free all of the strdup'ed args
    char **free_ptr = argv;
    while (NULL != *free_ptr) { 
        free(*free_ptr++);
    }
    free(argv);
    
    return pid;
}

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
    cout << "shared data name: " << msg->shm_open_name << endl;

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
        cout << "shared data name " << msg->shm_open_name << endl;
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
        sigint_received++;
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
	
    // !!! should probably move this to an options class
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
        const pid_t pid = LaunchProcess(bin_dir / name, app_args);
        if (pid > 0) {
            pids.push_back(pid);
            cerr << "Launched " << name.string() << " as pid " << pid << endl;
        }
        else {
            cerr << "Failed to launch " << name.string() << endl;
        }
    }
    
    cout << "launched processes: " << endl;
    for (vector<pid_t>::iterator it = pids.begin(); it != pids.end(); ++it) {
        cout << "pid: " << *it << endl;
    }
    
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, SigintHandler);
    
    struct mq_attr attr;
    memset(&attr, 0, sizeof(struct mq_attr));
    // ??? I can set this at 300 in my test program, but this chokes if it's > 10. WTF?
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(NimsIngestMessage);
    attr.mq_flags = 0;
    
    /*
      Now that all subprocesses are running, this message queue should already
      exist, so we can open it read-only.
     */
    
    mqd_t ingest_message_queue;
    ingest_message_queue = mq_open(MQ_INGEST_QUEUE, O_RDONLY, S_IRUSR, &attr);
#warning this fails
    if (-1 == ingest_message_queue) {
        perror("failed to create ingest_message_queue");
        exit(1);
    }
    
    int epollfd = epoll_create(1);
    if (-1 == epollfd) {
        perror("epollcreate() failed in nims");
        exit(1);
    }
        
    // monitor our ingest queue descriptor
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = ingest_message_queue;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ingest_message_queue, &ev) == -1) {
        perror("epoll_ctl() failed in nims");
        exit(2);
    }
    
    while (true) {
        
        cerr << "entering epoll" << endl;
        // no particular reason for 10, but it was in sample code that I grabbed
#define EVENT_MAX 10
        struct epoll_event events[EVENT_MAX];
        
        // pass -1 for timeout to block indefinitely
        int nfds = epoll_wait(epollfd, events, EVENT_MAX, -1);
        cerr << "epoll_wait returned" << endl;
        
        // handle error condition first, in case we're exiting on a signal
        if (-1 == nfds) {
            
            if (EINTR == errno && sigint_received) {
            
                cerr << "received SIGINT" << endl;
                
                vector<pid_t>::iterator it;
                for (it = pids.begin(); it != pids.end(); ++it) {
                
                    const pid_t child_pid = *it;
                    // of course, this is a race condition if true and the child exits...
                    if (getpgid(getpid()) == getpgid(child_pid)) {
                        cerr << "sending signal to child " << child_pid << endl;
                        kill(child_pid, SIGINT);
                    } else {
                        cerr << "pid " << child_pid << " not in group" << endl;
                    }
                }
            
                //killpg(getpgrp(), SIGINT);
            
                break;
            } else {
                // unhandled signal or some other error
                perror("epoll_wait() failed in nims");
                exit(3);
            }
        
        }
        
        for (int n = 0; n < nfds; ++n) {
            
            if (events[n].data.fd == ingest_message_queue) {
                
                /*
                 Process a single message per loop; this avoids blocking
                 on mq_receive, since we don't get SIGINT if we do while().
                 Queued messages are still processed, though; I tested this
                 by tickling the ingester several times, then launching nims
                 to see it process all of the enqueued messages.
                */
                NimsIngestMessage msg;
                if (mq_receive (ingest_message_queue, (char *)&msg, 
                        sizeof(msg), NULL) != -1) 
                    ProcessSharedFramebufferMessage(&msg);
                
            } else {
                cerr << "*** ERROR *** Monitoring file descriptor with no event handler" << endl;
            }
        }
        
    }
    
    close(epollfd);
    mq_close(ingest_message_queue);
    
    /*
    Need to wait here?
    */

    return sigint_received;
}
