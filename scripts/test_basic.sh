#!/bin/bash
echo "Testing Basic"
echo "This read and write requests to vssd 1 and 2"
echo "If completed correctly, the output should show replies from both vssd 1 and 2"
ssh -t $CLIENT "cd ~/client_code; ./run.sh basic.trace 10 10 1> ${CLIENT_LOG} 2> ${CLIENT_ERROR}" 
echo "Done.."
scp $USER@$CLIENT:~/client_code/${CLIENT_LOG} $RESULTS
scp $USER@$CLIENT:~/client_code/${CLIENT_ERROR} $RESULTS
cat $RESULTS/${CLIENT_LOG}
./clean_bits.sh
