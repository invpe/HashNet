#!/bin/bash

# Wait for the network to be up
sleep 60

# Remove the old binary
rm -f /home/pi/hashnet_arm64.bin

# Download the latest binary
wget https://github.com/invpe/HashNet/releases/latest/download/hashnet_arm64.bin -O /home/pi/hashnet_arm64.bin

# Make the binary executable
chmod +x /home/pi/hashnet_arm64.bin

# Get the number of CPU cores
CORES=$(nproc)

# Start one instance per core in separate screen sessions
for ((i=0; i<CORES; i++)); do
    screen -dmS hashnet_runner_$i bash -c "while true; do ./hashnet_arm64.bin HASHNET_SERVER YOURNODEID; done"
done
