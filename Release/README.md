# Hashnet 1.3

Multiplatform release for W32, Linux, ESP32 - the iOS comes with an invite from the AppStore Test Flight.

This release supports jobs with `DSHA256`, `SHA1`, `BLAKE3` stay up to date with new ones coming.



✋ It is convenient to use [Startup Scripts](https://github.com/invpe/HashNet/tree/main/Release/StartupScripts) when running binaries, they ensure miners run in the loop so there's no hussle to restart anything. Since this is in POC, apps are programmed to quit on issues. To keep them alive, use either a script or a simple `while` loop.


# Instructions

## Windows

![image](https://github.com/user-attachments/assets/de001ec0-5173-43f4-b61e-855f7b2dcc41)

Download the `hashnet.exe` file and run with `hashnet.exe YOUR_HASHNET_SERVER_DOT_COM your_miner_id`.

Example: `hashnet.exe YOUR_HASHNET_SERVER_DOT_COM MY_RIG1`

## Linux, Raspberry PI

![image](https://github.com/user-attachments/assets/95d63df1-811c-436e-a757-a2b81d8f583d)

Download the `hashnet.bin` file and run with `./hashnet.bin YOUR_HASHNET_SERVER_DOT_COM your_miner_id`.

Example: `while true; do ./hashnet.bin YOUR_HASHNET_SERVER_DOT_COM MY_RIG1;done`

## Raspberry PI

Grab the `hashnet_arm64.bin` and use the [startup script](https://github.com/invpe/HashNet/tree/main/Release/StartupScripts).

Example: `while true; do ./hashnet.bin YOUR_HASHNET_SERVER_DOT_COM MY_RIG1;done`

## Android phones

![image](https://github.com/user-attachments/assets/da1fdc35-c3cf-4350-8134-5894b56731c9)

If you have some old Android phones with arm64, use `UserLAND` and follow the raspberry instructions.


## ESP32

![image](https://github.com/user-attachments/assets/b0bc1250-0e31-4ba3-a0dd-a879f37ad8f8)

1. Download the `esp32merged.bin` file 
2. Connect your ESP32 
3. Flash it with the [ESPTOOL](https://espressif.github.io/esptool-js/)
4. Configure over serial terminal answering prompts

## iOS - Iphones, IPADs

![image](https://github.com/user-attachments/assets/cdc74d20-0d80-48db-9e94-87598923db37)

You have to be invited to the TestFlight in order to join with your phone or tablet, let us know you are interested.

## ASIC

![image](https://github.com/user-attachments/assets/ace6338b-44f8-46a3-8776-c2d545bfbbd6)

This version is **only** for SHA256 and works with USB ASIC Miners i.e Block Erupter.
By default it opens a serial connection with `/dev/ttyUSB0` and squeezes Hash rate around 330MH/s.


# 👷 Advanced

### Autorun hashnet's newest version with crontab (Linux, Raspberry)

This example is based on raspberry pi, but you can update to any linux debian like dist.


1. Execute `crontab -e`

2. Add `@reboot /home/pi/raspberry.sh` at the end.

3. Exit crontab editing

4. Execute `cd /home/pi`

5. Download [startup script](https://github.com/invpe/HashNet/tree/main/Release/StartupScripts)
 
6. Make the script executable with `chmod +x ./raspberry.sh`

7. Simply `reboot` to test.

From now on, your raspberry/linux will download the latest binary from the releases and run it.

------ 

### How to run 'hashnet.bin' in the background as a systemd service for Linux:

**Enter sudo mode**: `sudo su`

**Download the latest release**:    
in this case it is 1.3, if a newer one is released, simply edit the part after /download : `wget -P /root/hashnet https://github.com/invpe/HashNet/releases/download/1.3/hashnet.bin`  
then run: `chmod +x /root/hashnet/hashnet.bin`

**Create an '.sh' file**: Create a new file in desired directory (in this case it will be /root/hashnet): `nano /root/hashnet/hashnet.sh` and add the following lines to it (modify the last parameter with your's pool name):

#!/bin/bash     
/root/hashnet/hashnet.bin hashnet.server.com NAME_OF_YOUR_POOL;

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

------    

### Run hashnet.exe in terminal with auto-restart function [Windows]:    

Create a file `hashnet.txt` with the following text:    
```
:start    
C:\Users\admin\Desktop\hashnet.exe hashnet.server.com NAME_OF_YOUR_POOL    
:: Wait 60 seconds before restarting.    
TIMEOUT /T 60    
GOTO:Start    
```

path can be specified in a different way, just point it to the place where you have your `hashnet.exe` file.    
Rename file from `hashnet.txt` to `hashnet.bat`    
Double-click on it to start.    
If you want to run it on start-up then from Start menu open `Run` -> `shell:startup` and copy the hashnet.bat file to the prompted directory. 
