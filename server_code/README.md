This directory contains the server code that processes requests from the client.
The code is written using dpdk for packet processing (v22.11.1)

The server code in this directory simply forwards packet to vSSDs, see the vssd_code directory for the storage logic. 

Note: Please replace the client and server ips in main.c with the ones for your topology, the current ones are placeholders.

Usage: ./run.sh
