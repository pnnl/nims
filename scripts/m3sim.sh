# playback M3 data file with NIMS on nims1
#/home/shari/Developer/NIMS/build/m3sim -m replay -f ../data/M3/2016,Mar,30,15-42-48.imb -r 10 -h 130.20.41.168 -p 20001 -l true

# playback M3 data file with NIMS on dev system
#/home/shari/Developer/NIMS/build/m3sim -m replay -f ../data/M3/2016,Mar,30,15-42-48.imb -r 10 -h 127.0.0.1 -p 20001 -l true

# playback simulated data file, no looping
/home/shari/Developer/NIMS/build/m3sim -m replay -f ../data/sim_bg50-10_tarrng.imb -r 10 -h 127.0.0.1 -p 20001
