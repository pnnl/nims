/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  task.h
 *  
 *  Created by Adam Maxwell on 04/22/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#include <sys/resource.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include "task.h"

using namespace nims;
using namespace std;

// TODO: monitor SIGCHLD and notify the main program
// when processes exit.

Task::Task(const fs::path &absolute_path, const vector<string> &args)
{
    absolute_path_ = absolute_path;
    arguments_ = args;
}

Task::~Task()
{
    
}

std::string Task::name()
{
    return absolute_path_.filename().string();
}

bool Task::launch()
{
    char **env = environ;
    char **argv = (char **)calloc(arguments_.size() + 2, sizeof(char *));

    // first arg is always the program we're executing
    argv[0] = strdup(absolute_path_.string().c_str());

    // copy the remaining arguments as C strings
    int argidx = 1;
    vector<string>::const_iterator it;
    for (it = arguments_.begin(); it != arguments_.end(); ++it) {
        argv[argidx] = strdup((*it).c_str());
        argidx++;
    }
    
    int blockpipe[2] = { -1, -1 };
    if (pipe(blockpipe))
        perror("nims::Task failed to create blockpipe");
    
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
    
    pid_ = fork();
    if (0 == pid_) {
        
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
        
    } else if (-1 == pid_) {
        perror("nims::Task fork()");
        
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
    
    return pid_ > 0;
}

int Task::signal(int sig)
{
    int ret = 0;
    if (0 != kill(pid_, sig)) {
       ret = errno;
       perror("nims::Task kill failed");
    }
    return ret;
}
