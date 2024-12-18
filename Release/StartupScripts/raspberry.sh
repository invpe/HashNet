#!/bin/bash

# Wait for the network to be up
sleep 60

# Define the path to the home directory dynamically
USER_HOME="$HOME"

# Ensure arguments for SERVER and NODE_ID are provided
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 SERVER NODE_ID"
    exit 1
fi

SERVER="$1"
NODE_ID="$2"

# Remove the old binary
rm -f "$USER_HOME/hashnet_arm64.bin"

# Download the latest binary
wget https://github.com/invpe/HashNet/releases/latest/download/hashnet_arm64.bin -O "$USER_HOME/hashnet_arm64.bin"

# Make the binary executable
chmod +x "$USER_HOME/hashnet_arm64.bin"

# Get the number of CPU cores
CORES=$(nproc)

# Start one instance per core in separate screen sessions
for ((i=0; i<CORES; i++)); do
    screen -dmS hashnet_runner_$i bash -c "cd $USER_HOME; while true; do ./hashnet_arm64.bin $SERVER $NODE_ID; done"
done
