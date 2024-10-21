# Release manual

This simple process will take you through a process of setting up your mining node on ESP32 for HashNet Xperiment.

## 1 Download latest release

Download `esp32merged.bin` from the latest [release](https://github.com/invpe/HashNet/releases).

## 2 Flash your ESP32

- Connect your ESP32 to USB.

- Go to [Espressif's web ESP Toool](https://espressif.github.io/esptool-js/)

- Connect to your ESP32 - choose `115200` speed

  ![image](https://github.com/user-attachments/assets/c2b81736-1bbb-435a-b3f9-8ce8afbe71c9)

- Select `esp32merged.bin` you have downloaded and set flash address to `0x00`

  ![image](https://github.com/user-attachments/assets/95583355-d711-40e8-913f-b45df9ebbfbd)

- Click `Program`
  
  ![image](https://github.com/user-attachments/assets/1398cd67-db5b-4c52-8218-b326c26dec8d)

Once programming is completed, close the web page.

## 3 Configure your Hashnet miner

- Open Web [Serial Terminal](https://serial.huhn.me/)

  ![image](https://github.com/user-attachments/assets/293fafc7-be23-4eb1-b404-cbef36a746af)
  
- Click connect and choose your ESP32 port and click Connect
  
  ![image](https://github.com/user-attachments/assets/29611d40-fea5-4e35-8f42-14cf478e1203)


- Answer all questions to configure your miner

  ![image](https://github.com/user-attachments/assets/cf682d6a-5247-4467-9f09-2f07430fdb24)


# Job done

If you made mistake in the configuration process, simply follow this manual again.
