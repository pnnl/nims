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
    
   /*
    Figure out the max number of file descriptors for a process; getrlimit is not listed as
    async-signal safe in the sigaction(2) man page, so we assume it's not safe to call after 
    fork().  The fork(2) page says that child rlimits are set to zero.
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

#if 0
static void readFrameBuffer()
{
    static int64_t fbCount = 0;
    
    string sharedName("/framebuffer-");
    sharedName += boost::lexical_cast<string>(fbCount++);
    
    /*
     Create -rw------- since we don't need executable pages.
     Using O_EXCL could be safer, but these can still exist
     in /dev/shm after a program crash.
    */
    int fd = shm_open(sharedName.c_str(), O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
    
    // !!! early return
    if (-1 == fd) {
        perror("shm_open() in ingester::processFile");
        return -1;
    }
    
    size_t mapLength = sizeForFrameBuffer(nfb);
    assert(mapLength > sizeof(NIMSFrameBuffer));
    
    // !!! early return
    // could use fallocate or posix_fallocate, but ftruncate is portable
    if (0 != ftruncate(fd, mapLength)) {
        perror("ftruncate() in ingest::processFile");
        shm_unlink(sharedName.c_str());
        return -1;
    }

    // mmap a shared framebuffer on the file descriptor we have from shm_open
    NIMSFrameBuffer *sharedBuffer;
    sharedBuffer = (NIMSFrameBuffer *)mmap(NULL, mapLength, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    // !!! early return
    if (MAP_FAILED == sharedBuffer) {
        perror("mmap() in ingester::processFile");
        shm_unlink(sharedName.c_str());
        return -1;
    }
    
    // copy data to the shared framebuffer
    sharedBuffer->dataLength = nfb->dataLength;
    memcpy(&sharedBuffer->data, nfb->data, nfb->dataLength);
    
    // create a message to notify the observer
    NIMSIngestMessage msg;
    msg.dataLength = sharedBuffer->dataLength;
    msg.name = { '\0' };
    
    // this should never happen, unless we have fbCount with >200 digits
    assert((sharedName.size() + 1) < sizeof(msg.name));
    strcpy(msg.name, sharedName.c_str());
    
    // done with the region in this process; ok to do this?
    // pretty sure we can't shm_unlink here
    munmap(sharedBuffer, mapLength);
    sharedBuffer = NULL;
    
    // why is the message a const char *?
    if (mq_send(ingestMessageQueue, (const char *)&msg, sizeof(msg), 0) == -1) {
        perror("mq_send in ingester::processFile");
        return -1;
    }
    
    return nfb->dataLength;
}
#endif

static volatile int sigint_received = 0;

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
	("cfg,c", po::value<string>()->default_value( "config.yaml" ),         "path to config file")
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
    
    struct mq_attr attr;
    memset(&attr, 0, sizeof(struct mq_attr));
    // ??? I can set this at 300 in my test program, but this chokes if it's > 10. WTF?
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(NIMSIngestMessage);
    attr.mq_flags = 0;
    
    /*
      Now that all subprocesses are running, this message queue should already
      exist, so we can open it read-only.
     */
    
    mqd_t ingestMessageQueue = mq_open(MQ_INGEST_QUEUE, O_RDWR, S_IRUSR | S_IWUSR, &attr);
    
    int nfds, epollfd;
    epollfd = epoll_create(1);
    if (-1 == epollfd) {
        perror("epollcreate() failed in nims");
        exit(1);
    }
    
#define MAX_EVENTS 10
    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = ingestMessageQueue;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ingestMessageQueue, &ev) == -1) {
        perror("epoll_ctl() failed in nims");
        exit(2);
    }
    
    while (true) {
        
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (-1 == nfds) {
            perror("epoll_wait() failed in nims");
            exit(3);
        }
        
        for (int n = 0; n < nfds; ++n) {
            
            if (events[n].data.fd == ingestMessageQueue) {
                
                // Now eat all of the messages
                NIMSIngestMessage msg;
                while (mq_receive (ingestMessageQueue, (char *)&msg, sizeof(msg), NULL) != -1) 
                     printf ("Received a message with length %d for name %s.\n", msg.dataLength, msg.name);
            }
        }
        
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
    
    close(epollfd);
    mq_close(ingestMessageQueue);
    
    /*
    Need to wait here?
    */

    return 0;
}
