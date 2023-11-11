# RackBlox: A Software-Defined Rack-Scale Storage System with Network-Storage Co-Design
RackBlox is our software-defined rack-scale storage architecture built with network-storage co-design. 
RackBlox RackBlox decouples the storage management functions of flash-based solid-state drives (SSDs), and allow the SDN to track and manage the states of SSDs in a rack.  
RackBlox has three major components: (1) coordinated I/O scheduling, in which it dynamically adjusts the I/O scheduling in the storage stack with the measured and predicted network latency, such that it can coordinate the effort of I/O scheduling across the network and storage stack for achieving predictable end-to-end performance; (2) coordinated garbage collection (GC), in which it will coordinate the GC activities across the SSDs in a rack to minimize their impact on incoming I/O requests; (3) rack-scale wear leveling, in which it enables global wear leveling among SSDs in a rack by periodically swapping data, for achieving improved device lifetime for the entire rack.

RackBlox is published in *The proceedings of the 29th ACM Symposium on Operating Systems Principles (SOSP 2023)* [Link](https://dl.acm.org/doi/10.1145/3600006.3613170).

## 1. Overview/Prerequisites

This README outlines the contents of this repository and how to run a basic example. Please note, to preserve confidentiality, certain parts of the codebase necessary to fully compile and run the code are ommitted. For AE reviewers, more detailed READMEs are available on our testbed along with all ommitted components necessary to run RackBlox. For all non-AE members looking to extend RackBlox, we provide pointers on what changes are necessary to configure and run RackBlox.

### Hardware Requirements
At least two nodes are required to deploy RackBlox (one client and one server), though RackBlox scales to an entire rack. Servers should be connected via a P4 programmable switch. Our code was developed and tested with a switch equipped with an Intel Tofino 1. Server nodes should be equipped with a programmable SSD that exposes Read/Write/Erase operations such that RackBlox can manage GC.   

### Software Requirements
**P4 Code.**  
The P4 code was developed and tested with the BF SDE version 9.10.0. This can be downloaded and installed through Intel's website but currently requires a non-disclosure agreement (NDA) to access. Accordingly, we have ommitted the tofino specific headers that are included from the SDE. In order to run the RackBlox code, you must include the tna headers. The p4 code can also be ported to bmv2 as it does not rely on any externs not present in bmv2.

**DPDK.**   
The server/client code is developed with DPDK v22.11.1. 

**Programmable SSD.**   
As mentioned in the hardware requirements, RackBlox needs a programmable SSD that exposes Read/Write/Erase functionality to the host. Due to confidentiality, we do not include headers necessary to successfully run the vssd_code included. The excluded code is specific to our programmable SSD and running RackBlox with a different SSD would require replacing the ommitted code anyways. Anyone looking to extend RackBlox should replace the Read/Write/Erase calls in the RackBlox code with the relevant functions for their SSD.

## 2. Contents
```
├── README.md     #This file
├── client_code   #Client code, submits requests and receives replies
│   ├── Makefile
│   ├── README.md
│   ├── YCSBA_small.trace #YCSB .trace files
│   ├── YCSBC_small.trace
│   ├── YCSB_write_only.trace
│   ├── auctionmark_small.trace #.trace files that include small request patterns to demonstrate functionality
│   ├── basic.trace #simple read/writes
│   ├── clear.trace
│   ├── gc_1.trace
│   ├── gc_2.trace
│   ├── gc_both.trace
│   ├── gc_req_1.trace
│   ├── gc_req_2.trace
│   ├── gc_req_both_1.trace
│   ├── gc_req_both_2.trace
│   ├── main.c      #main client code
│   ├── meson.build
│   ├── run.sh      #helper script runs with <trace_file_name> <# sent reqs> <# expected replies>
│   ├── trace.c     #Helps read the trace
│   ├── trace.h
│   └── util.h      #Helper methods to setup network and parse packets
├── p4_code
│   ├── basic_switch
│   │   ├── README.md
│   │   ├── basic_rackblox.p4 #All p4 code of RackBlox
│   │   └── compile.sh        #Helper script to compile and run RackBlox p4 code on the switch
│   └── includes
│       ├── constants.p4
│       └── headers.p4      #Contains the packet header format
├── server_code   #Contains a simple server code
│   ├── Makefile
│   ├── README.md
│   ├── main.c    #Main function
│   ├── meson.build
│   ├── net_trace.dat #Network trace used to mimic real data center latency
│   ├── run.sh    #Script for building and running the server
│   └── util.h    #Network utilities
├── scripts
│   ├── README.txt   #Please read for detailed description of scripts
│   ├── switch_setup.py #Setup for switch
│   ├── clean_bits.sh
│   ├── clean_client.sh
│   ├── clean_up.sh  #Shut down client/server/vssds
│   ├── defines.sh   #Please update with your topology
│   ├── run_vssd.sh  #Sets up vSSD
│   ├── setup.sh     #Single script to initiate server/switch/vssds
│   ├── test_basic.sh  #Scripts to initiate various experiments to play with RackBlox
│   ├── test_end_to_end.sh
│   ├── test_gc_1.sh
│   ├── test_gc_2.sh
│   ├── test_gc_both.sh
│   ├── test_gc_req_1.sh
│   ├── test_gc_req_2.sh
│   ├── test_gc_req_both_1.sh
│   ├── test_gc_req_both_2.sh
│   ├── test_ycsb_50_50.sh
│   ├── test_ycsb_read_only.sh
│   └── test_ycsb_write_only.sh
└── vssd_code
    ├── Makefile
    ├── README.md
    ├── blklist.c
    ├── blklist.h
    ├── channel.c
    ├── channel.h
    ├── cmd.c
    ├── parser.c
    ├── parser_nexus.c
    ├── queue.c
    ├── queue.h
    ├── rb_nexus.c   #Main SDF code for Rackblox, implements coordinated scheduling GC logic.
    ├── rb_nexus.h
    ├── run.sh
    ├── usage.c
    ├── usage.h
    ├── vssd.c       #Main vssd code
    └── vssd.h
```

## 3. Basic Usage

Before running anything, please replace placeholder addresses at client_code/main.c:36 and server_code/main.c:35 (marked with TODOs).

Basic functionality can be achieve with the following commands regardless of topology: 

First initialize the vSSDs. The current run.sh sets up hardware isolated vSSDs where the input represents the ID for the vssd when routing packets.
```
./vssd_code/run.sh 1 &
./vssd_code/run.sh 2 &
```
Next, we setup the server code:
```
./server_code/run.sh 
```

Finally, we run the client. In this example, we test with 10k requests:
```
./run.sh auctionmark_small.trace 10000 10000
```

We include scripts/ with many helpful scripts for playing with RackBlox. 

Before running any scripts, please update CLIENT, SWITCH, SERVER, and RESULT in defines.sh with your corresponding topology. 
Next, please copy setup_switch.py to your switch and modify it to include the ports for your topology. The script also includes a skeleton
for how to add table entries with the script which you can use to adjust RackBlox for your topology.  
Finally, run:
```
source ./defines.sh
./setup.sh
```

Afterwards, you may run any test script. E.g.:
```
./test_basic.sh
```

You may create your trace/test script to test different requests patterns.



## 4. Extending RackBlox

The switch logic in p4_code/basic_switch/basic_rackblox.p4. The ingress processing shows the GC redirection logic while the egress is only involved in per-hop latency computation. Changes to the core logic of RackBlox will need to extend ingress processing. Additionally, the packet header is defined in p4_code/includes/headers.p4 where users may wish to add/remove fields for their use cases. Be sure to also modify the packet format in the server/client/vssd_code to match. 

Coordinated I/O scheduling and the GC SDF based GC logic are implemented in vssd_code/rb_nexus.c 

## 5. License

This project is licensed under the terms of the Apache 2.0 license.
