#!/bin/bash

usage() { 
    echo "Usage:    " 
    echo "          "
    echo "          "
    exit 1
}

RUN=0

while getopts "rh" o; do
    case "${o}" in 
        r)
            RUN=1
            ;;
        h)
            echo "Help text!"
            usage
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

COPY_DIR="/home/breidys2/sde/bf-sde-9.10.0/install/"
SRC_NAME="basic_rackblox"


echo "Compiling..."
bf-p4c $SRC_NAME.p4 || exit 1
echo "Copying compiled files to ${COPY_DIR}"
cp -r $SRC_NAME.tofino/ $COPY_DIR

if [ $RUN -eq 1 ]; then
    ./run_switchd.sh --arch tofino -p $SRC_NAME
fi

echo "Done..."
