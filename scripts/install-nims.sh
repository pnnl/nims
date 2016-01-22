#!/bin/bash

BUILD_DIR=../build

# this is here for embedded systems work, where the
# IP address is used for multiple hosts
SSH_OPTS='-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null'

HOST=nims1
USER=nims
PASS=$1

if [ "$1" = "" ] ; then
    echo "password for $USER is required as argument" >&2
    exit 1
fi

if ! [ "$2" = "" ] ; then
  HOST="$2"
fi

echo "installing to $HOST"

if ! ping -c 1 $HOST > /dev/null ; then
    echo "change IP or connect host"
    exit 1
fi

# set up after we get the IP right...
SSH="sshpass -p $PASS ssh $SSH_OPTS $USER@$HOST"
SCP="sshpass -p $PASS scp -r $SSH_OPTS"

function copy_to_bin () {
    
    $SCP "$1" $USER@$HOST:~/bin
    if ! [ $? = 0 ]; then
        echo "failed to copy $1 to $HOST"
        exit 1
    fi
}

$SSH 'mkdir -p ~/bin'
if ! [ $? = 0 ]; then
    echo "failed to create ~/bin on $HOST"
    exit 1
fi

copy_to_bin $BUILD_DIR/config.yaml
copy_to_bin $BUILD_DIR/nims-init

copy_to_bin $BUILD_DIR/nims
#copy_to_bin $BUILD_DIR/detector
copy_to_bin $BUILD_DIR/ingester
copy_to_bin $BUILD_DIR/tracker
copy_to_bin $BUILD_DIR/../webapp
