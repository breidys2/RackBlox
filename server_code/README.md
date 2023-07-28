This directory contains the server code that processes requests from the client.
The code is written using dpdk for packet processing (v22.11.1)

Please note a slight variation between our paper and AE testbed. 
Due to hardware constraints in our AE testbed, we cannot directly connect the programmable switch to the server with the programmable SSD.
Therefore, the server code in this directory simply forwards packets, see the vssd_code directory for the storage logic. 

Usage: ./run.sh
