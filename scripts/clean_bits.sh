#!/bin/bash
#This script cleans the GC bits in the switch by setting them all to '0'. This is intended to reset the state between invocations of the other experiments.

echo "Cleaning switch bits"
ssh -t $CLIENT "cd ~/client_code; ./run.sh clear.trace 2 0 &> ${CLIENT_LOG}" 
echo "Done.."
