# analyze NIMS processing time from log files
library(chron)
options(digits.secs=6)

# ingester

# 2016-07-25 14:34:44.463052 [debug] ingester (24571) got frame!
# 2016-07-25 14:34:44.463130 [debug] ingester (24571)    device = Kongsberg M3 Multibeam sonar
#    version = 8
#    ping_num = 29643
#    ping_sec = 1438818231
#    ping_millisec = 323
#    soundspeed_mps = 1502
#    num_samples = 1373
#    range_min_m = 0.4
#    range_max_m = 20
#    winstart_sec = 0.000538
#    winlen_sec = 0.028109
#    num_beams = 108
#    beam_angles_deg = -70.038 to 70.038
#    freq_hz = 500000
#    pulselen_microsec = 50
#    pulserep_hz = 28


# 2016-07-25 14:34:44.463191 [debug] ingester (24571) FrameBufferWriter: putting frame 28644(ping 29643) in /nims_framebuffer-28643
# 2016-07-25 14:34:44.463644 [debug] ingester (24571) sending frame messages to 1 readers
# 2016-07-25 14:34:44.463702 [debug] ingester (24571) size of frame msg is 271 bytes
# 2016-07-25 14:34:44.872735 [debug] ingester (24571) sent msg to queue id 15
# 2016-07-25 14:34:44.872957 [debug] ingester (24571) Replacing framebuffer slot 44 (/nims_framebuffer-28543) with (/nims_framebuffer-28643)
# 2016-07-25 14:34:44.873075 [debug] ingester (24571) GetPing

# cat ingester*.log | tr -d '\000' | grep 'got ping' > ingester.log
ing <- read.table("ingester.log")
# just keep date time and frame number
ing <- ing[ , c("V1", "V2")]
# convert factors to char
i <- sapply(ing, is.factor)
ing[i] <- lapply(ing[i], as.character)

# convert into time and calc difference
ingt <- as.POSIXct(paste(ing$V1,ing$V2),format='%Y-%m-%d %H:%M:%OS')
ing_dt <- ingt[-1] - ingt[-length(ingt)]


# detector

# 2016-07-25 11:19:13.932755 [debug] detector (24574) got frame 1009
# 2016-07-25 11:19:13.932793 [debug] detector (24574) Updating mean background
# 2016-07-25 11:19:14.351479 [debug] detector (24574) mean background values from 0.000290226 to 4.59048
# 2016-07-25 11:19:14.351743 [debug] detector (24574) std dev background values from 0.00290622 to 4.10151
# 2016-07-25 11:19:14.351799 [debug] detector (24574) Detecting objects
# 2016-07-25 11:19:14.352497 [debug] detector (24574) ping 2008: number of samples above threshold is 184 (1%)
# 2016-07-25 11:19:14.352544 [debug] detector (24574) grouping pixels
# 2016-07-25 11:19:14.353403 [debug] detector (24574) 2008 number of detected objects: 75
# 2016-07-25 11:19:14.353589 [debug] detector (24574) sending message with 75 detections

# cat detector*.log | grep 'got frame' > detector.log
det <- read.table("detector.log")
# just keep date time and frame number
det <- det[ , c("V1", "V2", "V8")]
# convert factors to char
i <- sapply(det, is.factor)
det[i] <- lapply(det[i], as.character)

# convert into time and calc difference
t <- as.POSIXct(paste(det$V1,det$V2),format='%Y-%m-%d %H:%M:%OS')
dt <- t[-1] - t[-length(t)]

# calc frame number difference
df = det$V8[-1] - det$V8[-length(det$V8)]

# tracker
# 016-07-25 11:19:13.932520 [debug] tracker (24581)   received detections message with 85 detections
# 2016-07-25 11:19:13.932605 [debug] tracker (24581)  0 active tracks:

# cat tracker*.log | grep 'received ping' > tracker.log
trk <- read.table("tracker.log")
# just keep date time and frame number
trk <- trk[ , c("V1", "V2", "V8")]
# convert factors to char
i <- sapply(trk, is.factor)
trk[i] <- lapply(trk[i], as.character)

# convert into time and calc difference
t <- as.POSIXct(paste(trk$V1,trk$V2),format='%Y-%m-%d %H:%M:%OS')
dt <- t[-1] - t[-length(t)]


