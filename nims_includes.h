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
typedef struct __attribute__ ((__packed__)) _NIMSFrameBuffer {
    size_t  dataLength;
    void    *data;
} NIMSFrameBuffer;

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
typedef struct __attribute__ ((__packed__)) _NIMSIngestMessage {
    size_t dataLength;
    char   name[NAME_MAX];
} NIMSIngestMessage;

#endif /* _NIMS_INCLUDES_H */