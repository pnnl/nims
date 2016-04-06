/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  nims_ipc.h
 *  
 *  Created by Shari Matzner on 10/23/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef __NIMS_NIMS_IPC_H__
#define __NIMS_NIMS_IPC_H__

#include <signal.h> // signal handling
#include <mqueue.h> // POSIX message queues
#include <string>

//************************************************************************
// Signal Handling
extern volatile int sigint_received;
void setup_signal_handling();

//************************************************************************
// Message Queues
#define MQ_SUBPROCESS_CHECKIN_QUEUE "/nims_subprocess_checkin_queue"
#define MQ_DETECTOR_TRACKER_QUEUE "/nims_detector_tracker_queue"
#define MQ_TRACKER_ARCHIVER_QUEUE "/nims_tracker_archiver_queue"

// create/open a POSIX message queue with read/write permissions
// the default mode is blocking
// the queue is created, if it doesn't already exist
// return value is the return from mq_open() call
mqd_t CreateMessageQueue(const std::string &name, size_t message_size, bool non_block=false);

// Called by NIMS processes started by the main nims process
void SubprocessCheckin(pid_t pid);


//************************************************************************
// Shared Memory

// Share data using shared memory, optionally prefaced with a header.
// The size of the data is written to shared mem, then the data itself.
int share_data(const std::string& shared_name,
               size_t hdr_size=0, char* phdr=nullptr,
               size_t data_size=0, char* pdata=nullptr );


//************************************************************************
// Command Line
//
// Parse the command line arguments to get the configuration file path
// and the logging level.
int parse_command_line(int argc, char * argv[], std::string& cfgpath, std::string& log_level);

#endif // __NIMS_NIMS_IPC_H__
