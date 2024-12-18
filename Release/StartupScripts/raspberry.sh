#!/bin/bash


# Define the path to the home directory dynamically
USER_HOME="$HOME"

# Ensure arguments for SERVER and NODE_ID are provided
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 SERVER NODE_ID"
    exit 1
fi

SERVER="$1"
NODE_ID="$2"

# Wait for the network to be up
sleep 60

# Detect architecture
ARCH=$(uname -m)
if [[ "$ARCH" == "aarch64" ]]; then
    BINARY_URL="https://github.com/invpe/HashNet/releases/latest/download/hashnet_arm64.bin"
    BINARY_NAME="hashnet_arm64.bin"
elif [[ "$ARCH" == "armv7l" || "$ARCH" == "armhf" ]]; then
    BINARY_URL="https://github.com/invpe/HashNet/releases/latest/download/hashnet_arm32.bin"
    BINARY_NAME="hashnet_arm32.bin"
else
    echo "Unsupported architecture: $ARCH"
    exit 1
fi

# Remove the old binary
rm -f "$USER_HOME/$BINARY_NAME"

# Download the latest binary
wget "$BINARY_URL" -O "$USER_HOME/$BINARY_NAME"

# Make the binary executable
chmod +x "$USER_HOME/$BINARY_NAME"

# Get the number of CPU cores
CORES=$(nproc)

# Start one instance per core in separate screen sessions
for ((i=0; i<CORES; i++)); do
    screen -dmS hashnet_runner_$i bash -c "cd $USER_HOME; while true; do ./$BINARY_NAME $SERVER $NODE_ID; done"
done
