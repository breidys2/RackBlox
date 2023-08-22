#!/bin/bash
VSSD_TIME=900

#source ./defines.sh
echo "****!!!Please run 'source ./defines.sh' before running this script!!!****"

sudo echo "Starting vSSDs"
./run_vssd.sh 1 $VSSD_TIME &> $VSSD_1_LOG &
./run_vssd.sh 2 $VSSD_TIME &> $VSSD_2_LOG &
echo "Done.."
echo "Start switch.."
ssh -t $SWITCH "./start_switch.sh &> ${SWITCH_LOG}" &
sleep 25
ssh -t $SWITCH "./setup_ports.sh &>> ${SWITCH_LOG}" 
echo "Done.."
echo "Sleeping for vSSD setup.."
sleep 70
echo "Start Server.."
ssh -t $SERVER "cd ~/server_code; ./run.sh &> ${SERVER_LOG}"  &
echo "Done.."
sleep 5
echo "Setup Complete, You may run tests"
