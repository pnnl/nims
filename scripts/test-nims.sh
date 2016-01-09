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

# start nims
pushd ../build
./nims > nims.out 2>&1 &
popd

# follow log files
#gnome-terminal -e 'tail -F /var/tmp/NIMS-shari-1000/log/nims_0.log' 
#gnome-terminal -e 'tail -F /var/tmp/NIMS-shari-1000/log/ingester_0.log' 
#gnome-terminal -e 'tail -F /var/tmp/NIMS-shari-1000/log/tracker_0.log' 

# top left
gnome-terminal --geometry 124x30+0+0 -x bash -c "tail -f ../build/nims.out | awk '\$4 ~ /ingester/'"
# bottom left
gnome-terminal --geometry 124x30+0-0 -x bash -c "tail -f ../build/nims.out | awk '\$4 ~ /tracker/'"
# top right
gnome-terminal --geometry 124x30-0+0 -x bash -c "tail -f ../build/nims.out | awk '\$4 ~ /nims/'"

# start the webapp
pushd ../webapp
./nims.py &
popd
