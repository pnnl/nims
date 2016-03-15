#include <mqueue.h>

// this header exposes functions in nims_py_ext.cpp for usage in
// nims_py.pxd

mqd_t create_message_queue(size_t message_size, const char *name);
int get_next_message(mqd_t mq, void *msg, size_t msgsize);
int get_next_message_timed(mqd_t mq, void *msg, size_t msgsize, int secs, int ns);

// expose structure sizes here for message queue usage
size_t sizeof_detection_message();
size_t sizeof_detection();

size_t sizeof_track();
size_t sizeof_tracks_message();
