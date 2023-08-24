#!/bin/bash
#TODO Update these
BF_PATH="/path/to/bfshell/"
PATH_TO_SETUP="/path/to/setup_switch.py"
echo "Setting up ports.."
cd $BF_PATH
sudo ./run_bfshell.sh -b $PATH_TO_SETUP
echo "Done.."
