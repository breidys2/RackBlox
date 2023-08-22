echo "Kill Server.."
ssh -t $SERVER "sudo pkill -9 rackblox; exit" 
echo "Done.."
echo "Kill Straggling Client.."
ssh -t $CLIENT "sudo pkill -9 rackblox; exit" 
echo "Done.."
echo "Kill Switch.."
ssh -t $SWITCH "./stop_switch.sh" 
echo "Done.."
echo "Kill vSSDs.."
sudo pkill -9 cnexcmd
echo "Done.."
echo "Copy Log Files.."
scp $USER@$SERVER:~/server_code/${SERVER_LOG} $RESULTS
scp $USER@$CLIENT:~/client_code/${CLIENT_LOG} $RESULTS
cp ../$VSSD_1_LOG $RESULTS
cp ../$VSSD_2_LOG $RESULTS
echo "Done.."
