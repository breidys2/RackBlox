This directory contains all the scripts necessary to evalute the RackBlox Artifact

The tests are the following and each test has a brief description printed at the beginning of the script:
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

##BASIC TESTING
Before running any tests please do:
source ./defines.sh
./setup.sh
This will initiate the vSSDs (which takes ~70 seconds) and setups the server code.

Then run each your chosen test script above, e.g.: 
./test_gc_1.sh

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
