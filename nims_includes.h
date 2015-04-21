/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  nims_includes.h
 *  
 *  Created by Adam Maxwell on 04/16/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef _NIMS_INCLUDES_H
#define _NIMS_INCLUDES_H 1

#include <limits.h> // for NAME_MAX

#define NIMS_MAJOR 0
#define NIMS_MINOR 1
#define NIMS_BUILD 1

/*
 An object living in shared memory; created by ingester,
 to be read by others. This is a dummy structure for now.
 
 - dataLength is the length in bytes of the data
   member, not of the entire structure.
*/
typedef struct __attribute__ ((__packed__)) _NimsFramebuffer {
    size_t  data_length;
    void   *data;
} NimsFramebuffer;

// shared memory will be created at /dev/shm/framebuffer-N
#define FRAMEBUFFER_SHM_PREFIX "/nims-framebuffer-"

/*
 A notification that the frame buffer at shared
 memory location 'name' has been created and
 is ready for processing.

 - dataLength is the expected frame buffer length
 - name is a null-terminated C-string with the
   name of the shared memory path where the frame
   buffer resides.
*/
#define MQ_INGEST_QUEUE "/nims_ingest_queue"
typedef struct __attribute__ ((__packed__)) _NimsIngestMessage {
    size_t mapped_data_length;
    char   shm_open_name[NAME_MAX];
} NimsIngestMessage;

#define MQ_SUBPROCESS_CHECKIN_QUEUE "/nims_subprocess_checkin_queue"

#endif /* _NIMS_INCLUDES_H */