#!/bin/bash

#TODO Update these
P4_PROG="path/to/switch/program/"
P4_NAME="basic_rackblox"

echo "Starting switch.."
cd $P4_PROG
#Tested with run_swithd.sh from SDE 9.10.0
./run_switchd.sh -p $P4_NAME 
echo "Done.."
