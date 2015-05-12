/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  queues.h
 *  
 *  Created by Adam Maxwell on 04/20/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef _QUEUES_H
#define _QUEUES_H 1

#include <string>
#include <mqueue.h>

__BEGIN_DECLS
  
mqd_t CreateMessageQueue(size_t message_size, const std::string &name);

void SubprocessCheckin(pid_t pid);
  
__END_DECLS

#endif /* _QUEUES_H */
