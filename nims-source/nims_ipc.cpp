/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  nims_ipc.cpp
 *
 *  Created by Shari Matzner on 10/23/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
// for POSIX shared memory
#include <fcntl.h>    // O_* constants file permissions
#include <unistd.h>   // sysconf
#include <assert.h>   // assert
#include <sys/stat.h>
#include <sys/mman.h> // mmap, shm_open

#include <cstring> // memcpy

#include <boost/program_options.hpp>

#include "nims_ipc.h"
#include "log.h"

using namespace std;
using namespace boost;
namespace po = boost::program_options;


//************************************************************************
// Signal Handling
void setup_signal_handling()
{
 struct sigaction new_action, old_action;
    new_action.sa_handler = sig_handler;
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
         
} // setup_signal_handling


//************************************************************************
// Message Queues
mqd_t CreateMessageQueue(const string &name, size_t message_size, bool blocking)
{
    struct mq_attr attr;
    memset(&attr, 0, sizeof(struct mq_attr));
    
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = message_size;
    attr.mq_flags = 0;
    
    int opts = O_CREAT | O_RDWR;
    int mode = S_IRUSR | S_IWUSR;
    
    if (!blocking) opts |= O_NONBLOCK;
    
    mqd_t ret = mq_open(name.c_str(), opts, mode, &attr);
    if (-1 == ret)
        perror("CreateMessageQueue");
    
    return ret;
}

void SubprocessCheckin(pid_t pid)
{
    mqd_t mq = CreateMessageQueue(MQ_SUBPROCESS_CHECKIN_QUEUE, sizeof(pid_t));
        
    if (-1 == mq) {
        exit(1);
    }

    if (mq_send(mq, (const char *)&pid, sizeof(pid_t), 0) == -1) {
        perror("SubprocessCheckin mq_send()");
    }

    mq_close(mq);
}


//************************************************************************
// Shared Memory
/*
 Mmap doesn't technically require rounding up to a page boundary
 for the length you pass in, but we'll do that anyway. 
*/
const size_t kPageSize = sysconf(_SC_PAGE_SIZE);
static size_t SizeForSharedData(size_t size_data_to_share)
{
    assert(kPageSize > 0);
    size_t number_of_pages = size_data_to_share / kPageSize + 1;
    return kPageSize * number_of_pages;

}

int share_data(const string& shared_name, size_t hdr_size, char* phdr, size_t data_size, char* pdata )
{
/*
     Create -rw------- since we don't need executable pages.
     Using O_EXCL could be safer, but these can still exist
     in /dev/shm after a program crash.
    */
    int fd = shm_open(shared_name.c_str(), O_CREAT | O_TRUNC | O_RDWR, 
            S_IRUSR | S_IWUSR);
    
    // !!! early return
    if (-1 == fd) {
        nims_perror("shm_open() in share_data()");
        return -1;
    }
    
    const size_t map_length = SizeForSharedData(hdr_size + data_size);
    assert(map_length > hdr_size + data_size);
    
    // !!! early return
    // could use fallocate or posix_fallocate, but ftruncate is portable
    if (0 != ftruncate(fd, map_length)) {
        nims_perror("ftruncate() in share_data()");
        shm_unlink(shared_name.c_str());
        return -1;
    }

    // mmap the file descriptor from shm_open into this address space
    char *pshared;
    pshared = (char *)mmap(NULL, map_length,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    close(fd);
    
    // !!! early return
    if (MAP_FAILED == pshared) {
        nims_perror("mmap() in share_data");
        shm_unlink(shared_name.c_str());
        return -1;
    }
        
    // copy data to the shared mem
    if (hdr_size > 0)
    {
        memcpy(pshared, phdr, hdr_size);
    }
   if (data_size > 0)
   {
       memcpy(pshared + hdr_size, &data_size, sizeof(data_size));
       memcpy(pshared + hdr_size + sizeof(data_size), pdata, data_size);
   }
     
    // done with the region in this process; ok to do this?
    // pretty sure we can't shm_unlink here
    munmap(pshared, map_length);

}; // share_data()

int parse_command_line(int argc, char * argv[], std::string& cfgpath, std::string& log_level)
{
    po::options_description desc;
    desc.add_options()
    ("help",                                                "print help message")
    ("cfg,c", po::value<string>()->default_value( "./config.yaml" ),
                                                           "path to config file")
    ("log,l", po::value<string>()->default_value("debug"), "debug|warning|error")
    ;
    po::variables_map options;
    try
    {
        po::store( po::parse_command_line( argc, argv, desc ), options );
    }
    catch( const std::exception& e )
    {
        cerr << "Sorry, couldn't parse that: " << e.what() << endl;
        cerr << desc;
        return -1;
    }
    
    po::notify( options );
    
    if( options.count( "help" ) > 0 )
    {
        cerr << desc;
        return -1;
    }
    cfgpath =  options["cfg"].as<string>();
   log_level =  options["log"].as<string>();
   return 0;

}