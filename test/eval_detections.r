#!/usr/bin/env Rscript

# Analyze detections

# read the detections file
# "ping, time, bearing, range, elevation, width, length, height, rotation1, rotation2, ts_min, ts_max, ts_sum"
det <- read.csv("detections.csv",head=TRUE,sep=",")
names(det) # variable names
dim(det) # number of rows and cols

# plot detections for a quick evaluation
plot(det$bearing, det$range)


# sim_bg50-10_tarbeam

# read the annotation file
ann <- read.csv("sim_bg50-10_tarbeam.csv",head=TRUE,sep=",")
dim(ann)

# missed detections
# ping is in ann but not in det
# assuming that if there's a det, it's a true positive
detected <- ann$ping %in% det$ping
num_missed <- sum(!detected)

# false positives
# only one det per ping
table(det$ping)

# no det outside range of ann

# drop extra columns not in ann
det <- det[,names(ann)]

# compare bearing,range,width,length
