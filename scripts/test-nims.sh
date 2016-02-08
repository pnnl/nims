#!/bin/bash

if [ ! -d /dev/mqueue ]; then
    sudo mkdir /dev/mqueue
    sudo mount -t mqueue none /dev/mqueue
fi

# remove message queues
sudo rm -rf /dev/mqueue/*

# remove shared memory
sudo rm -rf /dev/shm/*

# remove log files
sudo rm -rf /var/tmp/NIMS*

# set environment var
export NIMS_HOME='/home/shari/Developer/NIMS/build'
# start nims
pushd ../build
./nims -l debug > nims.out 2>&1 &
popd


# top left
gnome-terminal --geometry 124x30+0+0 -x bash -c "tail -f ../build/nims.out | awk '\$4 ~ /ingester/'"
# bottom left
gnome-terminal --geometry 124x30+0-0 -x bash -c "tail -f ../build/nims.out | awk '\$4 ~ /nims/'"
# top right
gnome-terminal --geometry 124x30-0+0 -x bash -c "tail -f ../build/nims.out | awk '\$4 ~ /detector/'"
# bottom right
gnome-terminal --geometry 124x30-0-0 -x bash -c "tail -f ../build/nims.out | awk '\$4 ~ /tracker/'"

# start the webapp
pushd ../webapp
./nims.py &
popd
