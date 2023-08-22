#!/bin/bash
PORTS=(8888 8886 8887)
PORT=8886

if [[ $# -gt 0 ]] ; then
    PORT=${PORTS[$1]}
    LOG="vssd_${1}_log.txt"
fi

TIMEOUT=$2

cd ..
make
sudo ./cnexcmd $PORT $TIMEOUT > $LOG
mv $LOG results
cd scripts
