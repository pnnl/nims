#!/usr/bin/env Rscript

# get FrameBufferWriter log messages
# cat /var/tmp/NIMS-shari-1000/log/ingester_0.log | tr -d '\000' | grep FrameBufferWriter >> ingester_pings.txt
# cat /var/tmp/NIMS-shari-1000/log/tracker_0.log | tr -d '\000' | grep "got frame" >> tracker_pings.txt

# read in the data
pings <- read.table("ingester_pings.txt",header=FALSE)
pings <- pings[,c("V1","V2","V9")]
colnames(pings) <- c("date","time","pingnum")
options("digits.secs"=6)

# calculate time between ping messages
x <- as.POSIXlt(paste(pings$date,pings$time))
runtime <- x[length(x)] - x[1]
xd <- as.numeric(diff(x))
summary(xd)
#hist(xd,xlab="time difference (seconds)")
#plot(density(xd),main="Time Interval Between Frames")

boxplot(xd, horizontal=TRUE,main=paste("Time Interval Between Frames (N=",length(x),")"),xlab="seconds")
leg1 = paste("min = ", round(min(xd),digits=6))
leg3 = paste("median = ", round(median(xd),digits=6))
leg5 = paste("max = ", round(max(xd),digits=6))
leg2 = paste("Q1 = ", round(quantile(xd,0.25),digits=6))
leg4 = paste("Q3 = ", round(quantile(xd,0.75),digits=6))
legend(x="top",paste(leg1, leg2, leg3, leg4, leg5, sep=",  "), bty="n")

# summarize dropped pings
pd = diff(pings$pingnum)
dropped = pd > 1
tdropped = x[dropped] # x is calculated above
td = as.numeric(diff(tdropped))

boxplot(td, horizontal=TRUE,main=paste("Time Between Dropped Frames (N=",sum(dropped),")"),xlab="seconds")
leg1 = paste("min = ", round(min(td),digits=3))
leg3 = paste("median = ", round(median(td),digits=3))
leg5 = paste("max = ", round(max(td),digits=3))
leg2 = paste("Q1 = ", round(quantile(td,0.25),digits=3))
leg4 = paste("Q3 = ", round(quantile(td,0.75),digits=3))
legend(x="top",paste(leg1, leg2, leg3, leg4, leg5, sep=",  "), bty="n")

