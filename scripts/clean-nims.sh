#!/bin/bash
if [ ! -d /dev/mqueue ]; then
    mkdir /dev/mqueue
    mount -t mqueue none /dev/mqueue
fi

# remove message queues
rm -rf /dev/mqueue/*

# remove shared memory
rm -rf /dev/shm/*

# remove log files
rm -rf /var/tmp/NIMS*

