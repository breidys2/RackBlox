# RackBlox: A Software-Defined Rack-Scale Storage System with Network-Storage Co-Design
RackBlox is our software-defined rack-scale storage architecture built with network-storage co-design. 
RackBlox RackBlox decouples the storage management functions of flash-based solid-state drives (SSDs), and allow the SDN to track and manage the states of SSDs in a rack.  
RackBlox has three major components: (1) coordinated I/O scheduling, in which it dynamically adjusts the I/O scheduling in the storage stack with the measured and predicted network latency, such that it can coordinate the effort of I/O scheduling across the network and storage stack for achieving predictable end-to-end performance; (2) coordinated garbage collection (GC), in which it will coordinate the GC activities across the SSDs in a rack to minimize their impact on incoming I/O requests; (3) rack-scale wear leveling, in which it enables global wear leveling among SSDs in a rack by periodically swapping data, for achieving improved device lifetime for the entire rack.

RackBlox will be published in *The 29th ACM Symposium on Operating Systems Principles (SOSP 2023)*.

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
│   ├── auctionmark_small.trace #.trace files that include small request patterns to demonstrate functionality
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

Please note that server_code/main.c and client_code/main.c have placeholder IP addresses. Please update these with your specific
client/server topology before running.

## 4. Extending RackBlox

The switch logic in p4_code/basic_switch/basic_rackblox.p4. The ingress processing shows the GC redirection logic while the egress is only involved in per-hop latency computation. Changes to the core logic of RackBlox will need to extend ingress processing. Additionally, the packet header is defined in p4_code/includes/headers.p4 where users may wish to add/remove fields for their use cases. Be sure to also modify the packet format in the server/client/vssd_code to match. 

Coordinated I/O scheduling and the GC SDF based GC logic are implemented in vssd_code/rb_nexus.c 

## 5. License

This project is licensed under the terms of the Apache 2.0 license.
