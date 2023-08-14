This directory contains the server code that processes requests from the client.
The code is written using dpdk for packet processing (v22.11.1)

The server code in this directory simply forwards packet to vSSDs, see the vssd_code directory for the storage logic. 

Usage: ./run.sh
