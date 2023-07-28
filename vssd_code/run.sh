#!/bin/bash
PORTS=(8888 8886 8887)
PORT=8886

if [[ $# -gt 0 ]] ; then
    PORT=${PORTS[$1]}
fi

make
./cnexcmd $PORT
