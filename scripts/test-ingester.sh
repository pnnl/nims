
#!/bin/bash

# Test the ingester module by copying
# data files to the input directory.

NIMS_INPUT_PATH=/home/shari/tmp
NIMS_DATA_PATH="/home/shari/Developer/NIMS/data"
FILE1="/Small fish at short range/FineEIQ_14m6.mmb"
FILE2="/Fraser River Sockeye/2014,Sep,17,12-29-08.001"
FILE3="/Dolphin/Dolphin.mmb"
FILE4="/16Dec2014 Testing/2014,Dec,16,21-53-38.mmb"

killproc() {
        pid=`pidof $1`
        [ "$pid" != "" ] && kill -9 $pid
}

mkdir "$NIMS_INPUT_PATH"
echo ""
echo "Ready to copy data from $NIMS_DATA_PATH to $NIMS_INPUT_PATH."
read -p "Start ingester then press any key..." -n1 -s
echo ""
read -p "Press any key to copy data file $FILE1..." -n1 -s
echo ""
cp "$NIMS_DATA_PATH/$FILE1" "$NIMS_INPUT_PATH"
read -p "Press any key to copy data file $FILE2..." -n1 -s
echo ""
cp "$NIMS_DATA_PATH/$FILE2" "$NIMS_INPUT_PATH"
read -p "Press any key to copy data file $FILE3..." -n1 -s
echo ""
cp "$NIMS_DATA_PATH/$FILE3" "$NIMS_INPUT_PATH"
read -p "Press any key to copy data file $FILE4..." -n1 -s
echo ""
cp "$NIMS_DATA_PATH/$FILE4" "$NIMS_INPUT_PATH"

read -p "Press any key to clean up..." -n1 -s
echo ""
rm -r $NIMS_INPUT_PATH
echo "Done."
echo ""
exit 0

