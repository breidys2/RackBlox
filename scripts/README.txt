This directory contains all the scripts necessary to evalute the RackBlox Artifact

The tests are the following and each test has a brief description printed at the beginning of the script:
#Basic functionality
test_basic.sh           
#Tests hard GC packets
test_gc_1.sh           
test_gc_2.sh           
#Tests soft GC requests with various orderings
test_gc_req_1.sh       
test_gc_req_2.sh
test_gc_both.sh        
test_gc_req_both_1.sh
test_gc_req_both_2.sh
#Tests end to end functionality
test_end_to_end.sh     
test_ycsb_read_only.sh
test_ycsb_write_only.sh
test_ycsb_50_50.sh

##BASIC TESTING
Please coordinate with other reviewers to ensure that you are the only one evaluating at a given time.
You can check this with: 'who' or 'last'

Before running any tests please use:
source ./defines.sh
./setup.sh
This will initiate the vSSDs (which takes ~70 seconds) and setups the server code.

Then run each your chosen test script above, e.g.: 
./test_basic.sh

For each script, it starts the client code to send message using the trace. After completion it prints the trace of sent/received packets.
At the beginning of each script, it prints what each trace is does and what the expected output is. You can verify that all packets are received and that
the correct redirection is performed in the background.
After each script, we run a helper script (clean_bits.sh) to clear the gc bits we explicitly set in each test.

##TROUBLESHOOTING
The vSSDs are set to expire after a given period of time. The default is 15 minutes, but this is configurable in the setup.sh script.
Therefore, please check that the vSSDs are still running vi 'htop'

If the client hangs, you can ctrl-c and then run:
./clean_client.sh
To kill the hanging client.
The main cause for this should be the vSSD being down (see above).

##FUNCTIONALITY

# Section 3.4
The Coordinated I/O scheduling logic corresponds to lines 153-161 in vssd_code/rb_nexus.c and is used by all experiments.
The network latency values are printed by the returned packets in the lat field output for each packet in each script.

# Section 3.5:
All test_gc_* scripts are used to stress the functionality of coordinating GC, shown in Algorithm 1 in the Paper.
* test_basic.sh uses reads and writes, using lines 2-3 and lines 4-9.
* test_gc_1.sh, test_gc_2.sh, and test_gc_both.sh use reads, testing lines 4-9 and lines 21-23.
* test_gc_req_both_1.sh and test_gc_req_both_2.sh test gc requests in lines 12-18.

The SDF GC logic shown in Algorithm 2 corresponds to lines 318-364 in vssd_code/rb_nexus.c and is used by all experiments.
* rb_nexus.c:324 corresponds to hard gc requests/background gc requests (lines 11-12 and lines 15-16)
* rb_nexus.c:326 corresponds to soft gc requests (lines 13-14)



