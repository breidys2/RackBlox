#!/bin/bash

CORE_1=0
CORE_2=1
PORT_MASK=3

while getopts "c:p:h" o; do
    case "${o}" in 
        c)
            CORE_1=${OPTARG}
            ;;
        p)
            PORT_MASK=${OPTARG}
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




echo "Making..."
make || exit 1
echo "Done with Make, now running server..."
sudo ./build/netflash -l $CORE_1,$CORE_2 -- -p $PORT_MASK
echo "Done..."

