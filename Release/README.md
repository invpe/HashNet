# Hashnet 1.3

Multiplatform release for W32, Linux, ESP32 - the iOS comes with an invite from the AppStore Test Flight.
This release supports jobs with `DSHA256`, `SHA1` stay up to date with new ones coming.

# Instructions

## Windows

Download the `hashnet.exe` file and run with `hashnet.exe server.tessie.club your_miner_id`.
Example: `hashnet.exe server.tessie.club MY_RIG1`

## Linux

Download the `hashnet.bin` file and run with `./hashnet.bin server.tessie.club your_miner_id`.
Example: `while true; do ./hashnet.bin server.tessie.club MY_RIG1;done`

## ESP32

1. Download the `esp32merged.bin` file 
2. Connect your ESP32 
3. Flash it with the [ESPTOOL](https://espressif.github.io/esptool-js/)
4. Configure over serial terminal answering prompts

## iOS - Iphones, IPADs

You have to be invited to the TestFlight in order to join with your phone or tablet, let us know you are interested.

---
### How to run 'hashnet.bin' in the background as a systemd service for Linux:

**Enter sudo mode**: `sudo su`

**Download the latest release**:    
in this case it is 1.3, if a newer one is released, simply edit the part after /download : `wget -P /root/hashnet https://github.com/invpe/HashNet/releases/download/1.3/hashnet.bin`  
then run: `chmod +x /root/hashnet/hashnet.bin`

**Create an '.sh' file**: Create a new file in desired directory (in this case it will be /root/hashnet): `nano /root/hashnet/hashnet.sh` and add the following lines to it (modify the last parameter with your's pool name):

#!/bin/bash     
/root/hashnet/hashnet.bin server.tessie.club NAME_OF_YOUR_POOL;

**Save the file (Ctrl+O), exit the editor (Ctrl+X) and run the following command**:
`chmod +x /root/hashnet/hashnet.sh`

**Create a Service File**: Create a new service in `/etc/systemd/system/`. Run: `nano /etc/systemd/system/hashnet.service`

**Edit the Service File**: Add the following content:
   
[Unit]  
Description=Hashnet

[Service]   
User=root   
WorkingDirectory=/root/hashnet  
ExecStart=/root/hashnet/hashnet.sh  
Type=simple   
Restart=always  
RestartSec=60s

[Install]   
WantedBy=multi-user.target   

**Save the file (Ctrl+O), exit the editor (Ctrl+X)**

**Reload Systemd**: Run `systemctl daemon-reload` to reload systemd manager configuration.

**Enable the Service**: Use `systemctl enable hashnet.service` to enable the service to start at boot.

**Start the Service**: Start the service with `systemctl start hashnet.service`

**Check Status**: Verify the service status using `systemctl status hashnet.service`

Logs can be monitored via `journalctl -u hashnet.service -n 20` where after -n you can specify the number of lines which will be prompted.
