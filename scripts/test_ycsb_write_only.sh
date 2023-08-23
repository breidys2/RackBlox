#!/bin/bash
echo "Testing End to End Functionality"
echo "This test sends both read and write requests to both vssd 1 and vssd 2 using a subset of a 100% write YCSB trace"
echo "If completed correctly, the output should show replies from both vssds"
echo "Round trip latencies are dumped at the end in a sorted fashion"
ssh -t $CLIENT "cd ~/client_code; ./run.sh YCSB_write_only.trace 10000 9500 1> ${CLIENT_LOG} 2> ${CLIENT_ERROR}" 
echo "Done.."
scp $USER@$CLIENT:~/client_code/${CLIENT_LOG} $RESULTS
scp $USER@$CLIENT:~/client_code/${CLIENT_ERROR} $RESULTS
cat $RESULTS/${CLIENT_LOG}
./clean_bits.sh
