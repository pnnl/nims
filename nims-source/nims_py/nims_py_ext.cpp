#include "nims_py_ext.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include "tracks_message.h"
#include <time.h>

mqd_t create_message_queue(size_t message_size, const char *name)
{
    struct mq_attr attr;
    memset(&attr, 0, sizeof(struct mq_attr));
    
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
    
    mqd_t ret = mq_open(name, opts, mode, &attr);
    if (-1 == ret)
        perror("mq_open in create_message_queue");
    
    return ret;
}

int get_next_message(mqd_t mq, void *msg, size_t msgsize)
{
  
  int ret = mq_receive(mq, (char *)msg, msgsize, NULL);
  
  if (-1 == ret)
      perror("mq_receive failed");
  
  return ret;
  
}

int get_next_message_timed(mqd_t mq, void *msg, size_t msgsize, int secs, int ns)
{
    struct timespec ts;
    ts.tv_sec = time(NULL) + secs;
    ts.tv_nsec = ns;
    
    // returns -1 if time expired, and errno set to ETIMEDOUT
    // http://pubs.opengroup.org/onlinepubs/9699919799/functions/mq_receive.html
    int ret = mq_timedreceive(mq, (char *)msg, msgsize, NULL, &ts);
    if (ret >= 0) return ret;
    if (ETIMEDOUT == errno) return 0;
    return -1;
}

size_t sizeof_track()
{
   return sizeof(struct Track);
}

size_t sizeof_tracks_message()
{
   return sizeof(struct TracksMessage);
}
