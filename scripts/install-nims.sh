#!/bin/bash

function abspath {
    if [[ -d "$1" ]]
    then
        pushd "$1" >/dev/null
        pwd
        popd >/dev/null
    elif [[ -e $1 ]]
    then
        pushd "$(dirname "$1")" >/dev/null
        echo "$(pwd)/$(basename "$1")"
        popd >/dev/null
    else
        echo "$1" does not exist! >&2
        return 127
    fi
}

# this will be an absolute path, so now we can run the
# build script from any directory
SCRIPTS_DIR=$(abspath $(dirname $0))
BUILD_DIR=$SCRIPTS_DIR/../build

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

#copy_to_bin $BUILD_DIR/config.yaml
echo "*** WARNING *** not copying config.yaml to avoid clobbering it"
copy_to_bin $BUILD_DIR/nims-init

copy_to_bin $BUILD_DIR/nims
copy_to_bin $BUILD_DIR/detector
copy_to_bin $BUILD_DIR/ingester
copy_to_bin $BUILD_DIR/tracker
copy_to_bin $BUILD_DIR/../webapp
copy_to_bin $BUILD_DIR/tracks_server.py
copy_to_bin $BUILD_DIR/tracks_client.py
copy_to_bin $BUILD_DIR/lib

$SCP $BUILD_DIR/../vendorsrc/bvt_sdk_4.2.0.9446/lib $USER@$HOST:~/bin
