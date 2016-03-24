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

#include "nims_ipc.h"
#include "task.h"
#include "log.h"

using namespace std;
using namespace boost;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

static volatile sig_atomic_t sigint_received_ = 0;
static volatile sig_atomic_t sigchld_received_ = 0;
static volatile sig_atomic_t sighup_received_ = 0;
static vector<nims::Task *> *child_tasks_ = NULL;

#define HANDLE_EINTR(x) ({ \
    __typeof__(x) __eintr_result__; \
    do { \
        __eintr_result__ = x; \
    } while (__eintr_result__ == -1 && errno == EINTR); \
    __eintr_result__;\
})
    
#define NIMS_HOME_VAR "NIMS_HOME"

// treat SIGINT and SIGTERM as identical
static void sigint_handler(int sig)
{
    if (SIGINT == sig || SIGTERM == sig)
        sigint_received_ += 1;
}

static void sigchld_handler(int sig)
{
    if (SIGCHLD == sig)
        sigchld_received_ += 1;
}

static void sighup_handler(int sig)
{
    if (SIGHUP == sig)
        sighup_received_ += 1;
}

static void InterruptChildProcesses()
{
    NIMS_LOG_WARNING << "running InterruptChildProcesses";

    // !!! early return if it hasn't been created yet
    if (NULL == child_tasks_) {
        NIMS_LOG_ERROR << "no child tasks running";
        return;
    }
    
    /* 
        Ignore SIGCHLD while interrupting the child processes.
        This avoids incrementing our SIGCHLD counter while doing
        a warm restart in response to SIGHUP (and thus rate-limiting the
        number of times we can do a SIGHUP).
    */
    struct sigaction new_action, old_action;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;

    new_action.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, NULL, &old_action);
    if (SIG_DFL != old_action.sa_handler)
      sigaction(SIGCHLD, &new_action, NULL);

    vector<nims::Task *>::iterator it;
    for (it = child_tasks_->begin(); it != child_tasks_->end(); ++it) {
        nims::Task *t = *it;
        if (getpgid(getpid()) == getpgid(t->get_pid())) {
            NIMS_LOG_WARNING << "sending SIGINT to " << t->name()
                << " [" << t->get_pid() << "]";
            t->signal(SIGINT);
            
            // make sure we reap all child processes
            if (-1 == waitpid(t->get_pid(), NULL, 0))
               nims_perror("waitpid failed");
        }
    }
    
    // just in case we screw up and call InterruptChildProcesses twice...
    delete child_tasks_;
    child_tasks_ = NULL;    
    
    // restore SIGCHLD handler
    sigaction(SIGCHLD, &old_action, NULL);
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

// exit() on failure
static void LaunchProcessesFromConfig(const YAML::Node &config)
{
    fs::path bin_dir = fs::path(getenv(NIMS_HOME_VAR));
    
    YAML::Node applications = config["APPLICATIONS"];
    
    // in case we're relaunching processes
    if (NULL != child_tasks_)
        delete child_tasks_;
    
    child_tasks_ = new vector<nims::Task *>;
    
    mqd_t mq = CreateMessageQueue(MQ_SUBPROCESS_CHECKIN_QUEUE, sizeof(pid_t), false);
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

// exit() on failure
static void WarmRestart(std::string &cfgpath)
{   
    // will ignore SIGCHLD
    InterruptChildProcesses();
    
    // reload config file; mainly for HUP, but ok for CHLD
    // don't ignore SIGCHLD here, as we want to count crashes/exits
    YAML::Node config = YAML::LoadFile(cfgpath);
    LaunchProcessesFromConfig(config);
        
    // Ignore SIGCHLD while launching nims.py
    struct sigaction new_action, old_action;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;

    new_action.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, NULL, &old_action);
    if (SIG_DFL != old_action.sa_handler)
      sigaction(SIGCHLD, &new_action, NULL);
    
    const char * stop_args[] = {"--stop", "--oknodo", "--name", "nims.py"};
    vector<string> args(stop_args, stop_args + sizeof(stop_args) / sizeof(char *));
    nims::Task task = nims::Task("/sbin/start-stop-daemon", args);
           
    int ret, status;
    if (task.launch()) {
        ret = HANDLE_EINTR(waitpid(task.get_pid(), &status, 0));
    }
    if (-1 != ret && WIFEXITED(status)) {
        NIMS_LOG_DEBUG << "nims.py exited; start-stop-daemon status " << WEXITSTATUS(status);
    } else {
        // NB: start-stop-daemon --oknodo returns 0 if no matching process
        nims_perror("start-stop-daemon failed to stop nims.py");
        NIMS_LOG_ERROR << "failed to stop nims.py";
        NIMS_LOG_ERROR << "ret = " << ret << ", WIFEXITED(status) = " << WIFEXITED(status);
        exit(errno);
    }
    
    fs::path webapp_dir = fs::path(getenv(NIMS_HOME_VAR)) / "webapp";
    fs::path webapp_path = webapp_dir / "nims.py";
    NIMS_LOG_WARNING << "using webapp at " << webapp_path.string();
    const char *start_args[] = {"--start", "--background",
        "--chdir", webapp_dir.c_str(), "--exec", webapp_path.c_str()};
    args = std::vector<string>(start_args, start_args + sizeof(start_args) / sizeof(char *));
    task = nims::Task("/sbin/start-stop-daemon", args);
    ret, status;
    if (task.launch()) {
        ret = HANDLE_EINTR(waitpid(task.get_pid(), &status, 0));
    }
    if (-1 != ret && WIFEXITED(status)) {
        NIMS_LOG_DEBUG << "nims.py started with status " << WEXITSTATUS(status);
    } else {
        nims_perror("start-stop-daemon failed to start nims.py");
        NIMS_LOG_ERROR << "failed to start nims.py";
        NIMS_LOG_ERROR << "ret = " << ret << ", WIFEXITED(status) = " << WIFEXITED(status);
        exit(errno);
    }
    
    // restore SIGCHLD handler
    sigaction(SIGCHLD, &old_action, NULL);
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
    
    const char *nims_home = getenv(NIMS_HOME_VAR);
    if (NULL == nims_home) {
        NIMS_LOG_ERROR << "*** ERROR *** environment variable " << NIMS_HOME_VAR << " not set";
        return -2;
    }
    NIMS_LOG_WARNING << "NIMS_HOME=" << nims_home;
    
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
    sigaction(SIGINT, &new_action, NULL);

    // register for SIGTERM, same action as SIGINT
    sigaction(SIGTERM, &new_action, NULL);
    
    // register for SIGCHLD
    new_action.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &new_action, NULL);

    // register for SIGHUP
    new_action.sa_handler = sighup_handler;
    sigaction(SIGHUP, &new_action, NULL);
    
    // ignore SIGPIPE
    new_action.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, NULL, &old_action);
    if (SIG_IGN != old_action.sa_handler)
        sigaction(SIGPIPE, &new_action, NULL);  
    
    YAML::Node config;
	try 
	{    
        config = YAML::LoadFile(cfgpath);
    }
    catch( const std::exception& e )
    {
        NIMS_LOG_ERROR << "Error reading config file: " << cfgpath << endl; 
        NIMS_LOG_ERROR << e.what() << endl;
        NIMS_LOG_ERROR << desc << endl;
        return -1;
    }

    // launch all default processes listed in the config file
    LaunchProcessesFromConfig(config);
       
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
    
    // reset before entering the event loop
    sigchld_received_ = 0;

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
            
            if (EINTR != errno) {
                // unhandled error
                nims_perror("epoll_wait() failed");
                exit(3);
            }
            
            assert(EINTR == errno);
            NIMS_LOG_DEBUG << "SIGINT:  " << sigint_received_;
            NIMS_LOG_DEBUG << "SIGCHLD: " << sigchld_received_;
                        
            // atexit will call InterruptChildProcesses
            if (sigint_received_) {
                NIMS_LOG_WARNING << "caught SIGINT or SIGTERM";            
                break;
            }
            else if (sigchld_received_) {
                // reset, so we don't misinterpret HUP/CHLD
                sigchld_received_ = 0;
                NIMS_LOG_ERROR << "caught SIGCHLD";
                static time_t last_relaunch_time_ = 0;
                time_t current_time = time(NULL);
#define MIN_RELAUNCH_TIME_INTERVAL 60
                if ((current_time - last_relaunch_time_) > MIN_RELAUNCH_TIME_INTERVAL) {
                    NIMS_LOG_ERROR << "attempting warm restart";
                    WarmRestart(cfgpath);
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
            else if (sighup_received_) {
                // reset, so we don't misinterpret HUP/CHLD
                sighup_received_ = 0;
                NIMS_LOG_ERROR << "SIGHUP caught: attempting warm restart";
                WarmRestart(cfgpath);
                NIMS_LOG_ERROR << "SIGHUP warm restart succeeded";
            }
            else {
                // this is a programmer error; die
                NIMS_LOG_ERROR << "Unhandled signal";
                break;
            }

        }
        
    }
    
    close(epollfd);
    NIMS_LOG_DEBUG << "nims process shutting down";
    
    return sigint_received_;
}
