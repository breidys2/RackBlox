echo "Kill Straggling Client.."
ssh -t $CLIENT "sudo pkill -9 rackblox; exit" 
echo "Done.."
