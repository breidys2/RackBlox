This directory contains the code for the vssd.

Please note this code will not compile due to confidential headers that are missing, these will need to be replaced with those for your
programmable SSD.

The vssd accepts requests from the network, schedules them to a programmable SSD via Coordinated I/O scheduling and replies.
Additionally, it generates and processes GC requests following Algorithm 2 in the paper.

If you want to extend RackBlox, please replace the functions labeled "read", "write", and "erase" with the equivalent for your SSD in order to run.

Usage ./run.sh <vssd_id> \<timeout> 

Where vssd_id is used to match against incoming packets to ensure correctness and timeout is how long the vSSD is active. This is included to simplify proper cleanup in shorter experiments.

