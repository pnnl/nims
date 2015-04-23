#!/bin/sh

# System Startup for NIMS
# to be installed in /etc/init.d

APPLICATION_PATH=/home/nims/bin

killproc() {
        pid=`pidof $1`
        [ "$pid" != "" ] && kill -9 $pid
}

case $1 in

   start)
      echo "Starting NIMS applications."
      if [ -e $APPLICATION_PATH/nims ]; then
          pushd .
          cd $APPLICATION_PATH
          $APPLICATION_PATH/nims &
          popd
      fi
      ;; 

   stop)
      echo "Stopping NIMS (Normal Mode)"
      killall nims
      ;;

   *)
     echo "Usage: nims.sh  {start|stop}"
     exit 1
     ;;
esac

exit 0

