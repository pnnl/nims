#include "nims_py_ext.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include "tracks_message.h"
#include "nims_ipc.h"
#include <unistd.h>
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

struct TracksMessage get_next_track(mqd_t mq)
{
   struct TracksMessage trk;
   memset(&trk, 0, sizeof(trk));
   
   get_next_message(mq, &trk, sizeof(trk));
   return trk;
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

struct TracksMessage get_next_track_timed(mqd_t mq, int secs, int ns)
{
   struct TracksMessage trk;
   memset(&trk, 0, sizeof(trk));
   
   get_next_message_timed(mq, &trk, sizeof(trk), secs, ns);
   if (trk.num_tracks) {
      struct Track tk = trk.tracks[0];
      fprintf(stderr, "**** id=%d, size_sq_m=%.2f, last_pos_bearing=%.2f, last_vel_elevation=%.2f\n", tk.id, tk.size_sq_m, tk.last_pos_bearing, tk.last_vel_elevation);
   }
   
   return trk;
}

int nims_checkin()
{
   mqd_t mq = create_message_queue(sizeof(pid_t), MQ_SUBPROCESS_CHECKIN_QUEUE);
       
   if (-1 == mq) {
       return 1;
   }

   int ret = 0;
   pid_t pid = getpid();
   if (mq_send(mq, (const char *)&pid, sizeof(pid_t), 0) == -1) {
       perror("nims_checkin mq_send()");
       ret = 2;
   }

   mq_close(mq);
   return ret;
}

size_t sizeof_track()
{
   return sizeof(struct Track);
}

size_t sizeof_tracks_message()
{
   return sizeof(struct TracksMessage);
}
