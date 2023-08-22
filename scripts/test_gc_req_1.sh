#!/bin/bash
echo "Testing GC Request 1"
echo "This test sends a GC request packet for vssd 1 followed by read requests to vssd 1"
echo "If completed correctly, the output should show replies from vssd 2 from redirected requests as GC is granted for vssd 1"
ssh -t $CLIENT "cd ~/client_code; ./run.sh gc_req_1.trace 10 9 1> ${CLIENT_LOG} 2> ${CLIENT_ERROR}" 
echo "Done.."
scp $USER@$CLIENT:~/client_code/${CLIENT_LOG} $RESULTS
scp $USER@$CLIENT:~/client_code/${CLIENT_ERROR} $RESULTS
cat $RESULTS/${CLIENT_LOG}
./clean_bits.sh
