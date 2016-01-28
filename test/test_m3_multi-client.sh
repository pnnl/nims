#!/bin/bash

# The purpose of this test is to determine if multiple clients can connect to the M3
# host software TCP/IP output stream and read the same ping data.

# The test setup requires one M3 host software application running in playback
# mode.  
# On two different nims systems, set the SONAR_HOST_ADDRESS in the config.yaml 
# file to point to the M3 host. 
# Start ingester and capture the ping numbers of the received data.

# The log messages to capture look like:
# 2016-01-08 17:22:37.014773 [debug] ingester (32367)	FrameBufferWriter: putting frame 14904 in /nims_framebuffer-3078
# The frame number is the M3 ping number, so we want to save the time stamp and the ping number.
./ingester 
cd "/var/tmp/NIMS-$USER-$(id -u)/log"
cat ingester_0.log | awk '/FrameBufferWriter/ {print $2, $9}' > frames.out

# Get both out files on the same machine and rename as frame1.out and frame2.out.
mv frames.out frames1.out
# sftp 

# Verify files are not empty.
head frames1.out
head frames2.out


# Read the out files into R and test.
R 
dat1 <- read.table("frames1.out"); dat2 <- read.table("frames2.out"); 
# find the first index where both have same ping number
# assuming frames1.out is from first machine that started ingester
idx1 <- match(dat2$V2[1],dat1$V2)
# now find the last matching ping number
# assuming frames1.out is from first machine that killed ingester
idx2 <- match(dat1$V2[nrow(dat1)],dat2$V2)
# test both machines got the same pings
setequal(dat1$V2[idx1:nrow(dat1)], dat2$V2[1:idx2]); 
length(setdiff(dat1$V2, dat2$V2))
