This directory contains the code for the vssd.

Please note this code will not compile due to confidential headers that are missing, these will need to be replaced with those for your
programmable SSD..

The vssd accepts requests from the network, schedules them to a programmable SSD via Coordinated I/O scheduling and replies.
Additionally, it generates GC requests, as described in the paper.

If extending, replace the functions labeled "read" and "write" with the equivalent of your SSD.

Usage ./run.sh vssd_id
