/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  queues.cpp
 *  
 *  Created by Adam Maxwell on 04/20/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#include <queues.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

using namespace std;

mqd_t CreateMessageQueue(size_t message_size, const string &name)
{
    struct mq_attr attr;
    memset(&attr, 0, sizeof(struct mq_attr));
    
    // ??? I can set this at 300 in my test program, but this chokes if it's > 10. WTF?
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = message_size;
    attr.mq_flags = 0;
    
    /*
     If we have a race condition where two processes need the same
     queue, they either need to create with the same permissions,
     or synchronize access to it. It's easier just to create and
     open RW at both ends than worry about opening RO.
    */
    const int opts = O_CREAT | O_RDWR;
    const int mode = S_IRUSR | S_IWUSR;
    
    mqd_t ret = mq_open(name.c_str(), opts, mode, &attr);
    if (-1 == ret)
        perror("mq_open in queues::InitializeIngestMessageQueue");
    
    return ret;
}

void SubprocessCheckin(pid_t pid)
{
    mqd_t mq = CreateMessageQueue(sizeof(pid_t), MQ_SUBPROCESS_CHECKIN_QUEUE);
        
    if (-1 == mq) {
        exit(1);
    }

    // why is the message a const char *?
    if (mq_send(mq, (const char *)&pid, sizeof(pid_t), 0) == -1) {
        perror("SubprocessCheckin mq_send()");
    }

    mq_close(mq);
    //mq_unlink(MQ_SUBPROCESS_CHECKIN_QUEUE);
}


