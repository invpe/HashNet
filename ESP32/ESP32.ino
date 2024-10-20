// HashNet ESP32 miner
// https://github.com/invpe/HashNet/
///////////////////////////////////
// Headers                       //
///////////////////////////////////
#include <random>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <Update.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Preferences.h>

#include <Adafruit_NeoPixel.h>
#include "SPIFFS.h"
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include "esp_task_wdt.h"
#include "nerdSHA256plus.h"

///////////////////////////////////
// Defines                       //
///////////////////////////////////
#define SERVER_PORT 5000
#define SHA256M_BLOCK_SIZE 32
#define CHECK_INTERVAL 10000
#define VERSION_URL "https://github.com/invpe/HashNet/releases/latest/download/esp32.bin"
///////////////////////////////////
// Variables                     //
///////////////////////////////////

uint8_t block_hash0[32];
uint8_t block_hash1[32];
uint8_t block_hash_tmp[32];
uint8_t target[32];
Preferences preferences;
WiFiClient client;
std::string extranonce2;
std::string job_id;
std::string prevhash;
std::string coinb1;
std::string coinb2;
std::string version;
std::string nbits;
std::string ntime;
std::string extranonce1;

const String strVersion = "1.3";

String strWIFI_SSID = "";
String strWIFI_Password = "";
String strHASHNET_SERVER = "";
String strMINER_IDENT = "";
String strBestDistanceBlockHash = "";
static const uint32_t EXPONENT_SHIFT = 24;
static const uint32_t MANTISSA_MASK = 0xffffff;
uint64_t uiTick = 0;
uint32_t uiTask0CS = 0;
uint32_t uiTask1CS = 0;
int extranonce2_size;
uint32_t nonce_start, nonce_end;

bool bTask0Completed = false;
bool bTask1Completed = false;

int iLastZeroBitsDistance = 0;

nerdSHA256_context midstate0;
nerdSHA256_context midstate1;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, 27, NEO_GRB + NEO_KHZ800);

bool StreamFile(const String &rstrURL, const String &rstrPath) {
  HTTPClient WebClient;
  WebClient.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  WebClient.setTimeout(10000);
  WebClient.setReuse(false);
  WebClient.begin(rstrURL);
  int httpCode = WebClient.GET();
  bool bSuccess = false;
  if (httpCode == HTTP_CODE_OK) {
    File file = SPIFFS.open(rstrPath, "w");
    if (!file) {
    } else {
      WiFiClient *stream = WebClient.getStreamPtr();
      uint8_t buffer[1024] = { 0 };
      int bytesRead = 0;
      while (WebClient.connected() && (bytesRead = stream->readBytes(buffer, sizeof(buffer))) > 0) {
        file.write(buffer, bytesRead);
      }
      file.close();
      bSuccess = true;
    }
  }
  WebClient.end();
  return bSuccess;
}
bool OTA() {
  if (StreamFile(VERSION_URL, "/update") == true) {
    Serial.println("OTA File downloaded");
    File firmwareFile = SPIFFS.open("/update", "r");
    if (firmwareFile) {
      size_t fileSize = firmwareFile.size();

      Serial.println("OTA File opened size " + String(fileSize));
      Update.begin(fileSize);
      Update.writeStream(firmwareFile);
      firmwareFile.close();
      if (Update.end()) {
        return true;
      }
    }
  }
  return false;
}
void SaveConfig() {
  preferences.begin("config", false);
  preferences.putString("ssid", strWIFI_SSID);
  preferences.putString("pass", strWIFI_Password);
  preferences.putString("server", strHASHNET_SERVER);
  preferences.putString("ident", strMINER_IDENT);
  preferences.end();
}
void LoadConfig() {
  preferences.begin("config", false);
  strWIFI_SSID = preferences.getString("ssid", "");
  strWIFI_Password = preferences.getString("pass", "");
  strHASHNET_SERVER = preferences.getString("server", "");
  strMINER_IDENT = preferences.getString("ident", "");
  preferences.end();
}
bool checkValid(unsigned char *hash, unsigned char *target) {
  bool valid = true;
  for (int i = 31; i >= 0; i--) {  // Use signed int instead of uint8_t to avoid wrapping
    if (hash[i] > target[i]) {
      valid = false;
      break;
    } else if (hash[i] < target[i]) {
      valid = true;
      break;
    }
  }
  if (valid) {
    Serial.print("\tvalid : ");
    for (size_t i = 0; i < 32; i++)
      Serial.printf("%02x ", hash[i]);
    Serial.println();
  }
  return valid;
}
std::string byteArrayToHexString(const uint8_t *byteArray, size_t length) {
  std::ostringstream oss;
  for (size_t i = 0; i < length; ++i) {
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byteArray[i]);
  }
  return oss.str();
}
void nbitsToTarget(const std::string &nbits, uint8_t target[32]) {
  memset(target, 0, 32);

  char *endPtr;
  uint32_t bits_value = strtoul(nbits.c_str(), &endPtr, 16);

  if (*endPtr != '\0' || bits_value == 0) {
    std::cerr << "Error: Invalid nbits value" << std::endl;
    exit(EXIT_FAILURE);
  }

  uint32_t exp = bits_value >> EXPONENT_SHIFT;
  uint32_t mant = bits_value & MANTISSA_MASK;
  uint32_t bitShift = 8 * (exp - 3);
  int byteIndex = 29 - (bitShift / 8);

  if (byteIndex < 0) {
    std::cerr << "Error: Invalid index, nBits exponent too small" << std::endl;
    exit(EXIT_FAILURE);
  }

  target[byteIndex] = (mant >> 16) & 0xFF;
  target[byteIndex + 1] = (mant >> 8) & 0xFF;
  target[byteIndex + 2] = mant & 0xFF;
}

inline int calculateDistanceToTarget(const uint8_t *block_hash, size_t hash_size) {
  int zero_bytes = 0;

  // Iterate from the end of block_hash towards the beginning
  for (size_t i = hash_size; i > 0; i--) {
    if (block_hash[i - 1] == 0x00) {  // Use i - 1 to correctly index
      zero_bytes++;
    } else {
      break;  // Stop counting once a non-zero byte is encountered
    }
  }

  return zero_bytes;
}

std::string hexStringToBinary(const std::string &hexString) {
  std::string binaryString;
  for (size_t i = 0; i < hexString.size(); i += 2) {
    std::string byteString = hexString.substr(i, 2);
    char byte = (char)std::strtol(byteString.c_str(), nullptr, 16);
    binaryString.push_back(byte);
  }
  return binaryString;
}
std::string double_sha256_to_bin_string(const std::string &input) {
  // Buffer to store the intermediate and final hash
  uint8_t intermediate_hash[32];
  uint8_t final_hash[32];

  // First SHA-256 hash
  mbedtls_sha256(reinterpret_cast<const unsigned char *>(input.data()), input.size(), intermediate_hash, 0);  // 0 for SHA-256

  // Second SHA-256 hash
  mbedtls_sha256(intermediate_hash, sizeof(intermediate_hash), final_hash, 0);  // 0 for SHA-256

  // Return the result as a binary string
  return std::string(reinterpret_cast<char *>(final_hash), sizeof(final_hash));
}
std::string bin_to_hex(const uint8_t *data, size_t len) {
  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  for (size_t i = 0; i < len; i++) {
    ss << std::setw(2) << static_cast<int>(data[i]);
  }
  return ss.str();
}
uint8_t hex(char ch) {
  uint8_t r = (ch > 57) ? (ch - 55) : (ch - 48);
  return r & 0x0F;
}
int to_byte_array(const char *in, size_t in_size, uint8_t *out) {
  int count = 0;
  if (in_size % 2) {
    while (*in && out) {
      *out = hex(*in++);
      if (!*in)
        return count;
      *out = (*out << 4) | hex(*in++);
      *out++;
      count++;
    }
    return count;
  } else {
    while (*in && out) {
      *out++ = (hex(*in++) << 4) | hex(*in++);
      count++;
    }
    return count;
  }
}
std::string calculate_merkle_root_to_hex(const uint8_t *coinbase_hash_bin, size_t hash_size, const std::vector<std::string> &merkle_branch) {
  // Convert the binary coinbase hash to hexadecimal string for processing
  std::string merkle_root = std::string(reinterpret_cast<const char *>(coinbase_hash_bin), hash_size);

  // Iterate over the merkle branch and compute the combined hash
  for (const std::string &hash : merkle_branch) {
    std::string combined_hash = merkle_root + hexStringToBinary(hash);
    merkle_root = double_sha256_to_bin_string(combined_hash);
  }

  // Return the final merkle root as a hexadecimal string
  return bin_to_hex(reinterpret_cast<const uint8_t *>(merkle_root.data()), merkle_root.size());
}
void reverse_bytes(uint8_t *array, size_t offset, size_t length) {
  size_t end = offset + length - 1;
  for (size_t start = offset; start < offset + (length / 2); start++, end--) {
    uint8_t temp = array[start];
    array[start] = array[end];
    array[end] = temp;
  }
}
void sha256_double(const void *data, size_t len, uint8_t output[32]) {
  uint8_t intermediate_hash[32];

  // First SHA-256 hash
  mbedtls_sha256((const unsigned char *)data, len, intermediate_hash, 0);  // 0 means SHA-256 (as opposed to SHA-224)

  // Second SHA-256 hash
  mbedtls_sha256(intermediate_hash, sizeof(intermediate_hash), output, 0);  // 0 means SHA-256 (as opposed to SHA-224)
}

// Task 1 for core 0: Searches the first half of the nonce space
void taskSearchCore0(void *pParam) {
  bTask0Completed = false;

  uint32_t uiNonce = nonce_start;
  uint32_t uiEndAt = nonce_start + (nonce_end - nonce_start) / 2;

  while (uiNonce < uiEndAt) {
    // Periodically reset the watchdog to prevent system reset
    if (uiNonce % 100000 == 0) {
      //Serial.println("T0 Nonce Progress: " + String(uiNonce));
      esp_task_wdt_reset();
    }

    // Nerd
    nerd_sha256d(&midstate0, reinterpret_cast<uint8_t *>(&uiNonce), block_hash0);

    //
    int leadingZeroBits0 = calculateDistanceToTarget(block_hash0, 32);


    // Check if better than last one
    if (leadingZeroBits0 > iLastZeroBitsDistance) {
      iLastZeroBitsDistance = leadingZeroBits0;
      strBestDistanceBlockHash = String(byteArrayToHexString(block_hash0, 32).c_str());

      if (checkValid(block_hash0, target)) {
        std::string strTemp = std::string("{\"method\": \"found\", \"job_id\": \"") + job_id
                              + std::string("\", \"extranonce1\": \"") + extranonce1
                              + std::string("\", \"extranonce2\": \"") + extranonce2
                              + std::string("\", \"nonce\": \"") + std::to_string(uiNonce)
                              + std::string("\", \"ntime\": \"") + ntime + "\"}";

        String strOutputPayload = String(strTemp.c_str());
        client.println(strOutputPayload.c_str());
        client.stop();
        Serial.println(strOutputPayload.c_str());
        bTask0Completed = true;
        vTaskDelete(NULL);
      }
    }

    uiTask0CS++;
    uiNonce++;
  }
  bTask0Completed = true;
  vTaskDelete(NULL);  // End the task when done
}
// Task 2 for core 1: Searches the second half of the nonce space
void taskSearchCore1(void *pParam) {
  bTask1Completed = false;
  uint32_t uiNonce = nonce_start + (nonce_end - nonce_start) / 2;
  Serial.println("T1 Start: " + String(nonce_start) + " End: " + String(nonce_end));
  Serial.println("T1 Calculated Midpoint Start: " + String(uiNonce));

  while (uiNonce < nonce_end) {

    // Periodically reset the watchdog to prevent system reset
    if (uiNonce % 100000 == 0) {
      //Serial.println("T1 Nonce Progress: " + String(uiNonce));
      esp_task_wdt_reset();
    }

    // Nerd
    nerd_sha256d(&midstate1, reinterpret_cast<uint8_t *>(&uiNonce), block_hash1);

    // Experimental - Calculate distance to target
    int leadingZeroBits1 = calculateDistanceToTarget(block_hash1, 32);

    if (leadingZeroBits1 > iLastZeroBitsDistance) {
      iLastZeroBitsDistance = leadingZeroBits1;
      strBestDistanceBlockHash = String(byteArrayToHexString(block_hash1, 32).c_str());

      if (checkValid(block_hash1, target)) {
        std::string strTemp = std::string("{\"method\": \"found\", \"job_id\": \"") + job_id
                              + std::string("\", \"extranonce1\": \"") + extranonce1
                              + std::string("\", \"extranonce2\": \"") + extranonce2
                              + std::string("\", \"nonce\": \"") + std::to_string(uiNonce)
                              + std::string("\", \"ntime\": \"") + ntime + "\"}";

        String strOutputPayload = String(strTemp.c_str());

        client.println(strOutputPayload.c_str());
        client.stop();
        Serial.println(strOutputPayload.c_str());
        bTask1Completed = true;
        vTaskDelete(NULL);
      }
    }

    uiTask1CS++;
    uiNonce++;
  }

  bTask1Completed = true;
  vTaskDelete(NULL);  // End the task when done
}

void setup() {
  pixels.begin();
  pixels.setBrightness(255);
  pixels.setPixelColor(0, 255, 255, 0);
  pixels.show();

  Serial.begin(115200);

  disableCore0WDT();
  disableCore1WDT();

  while (!SPIFFS.begin()) {
    SPIFFS.format();
    Serial.println("Failed to mount file system");
    delay(1000);
  }

  // Load config
  LoadConfig();

  if (strWIFI_SSID == "" || strHASHNET_SERVER == "") {
    Serial.println("Time to setup, hit ENTER to start.");

    // Wait for user readiness
    while (!Serial.available()) {
      // Wait for input
    }
    Serial.readString();

    // Wait for user to provide WiFi name
    Serial.println("Please enter WiFi name:");
    while (!Serial.available()) {
      // Wait for input
    }
    strWIFI_SSID = Serial.readString();
    strWIFI_SSID.trim();  // Remove leading/trailing spaces

    // Wait for user to provide WiFi password
    Serial.println("Please enter WiFi password:");
    while (!Serial.available()) {
      // Wait for input
    }
    strWIFI_Password = Serial.readString();
    strWIFI_Password.trim();  // Remove leading/trailing spaces

    //
    Serial.println("Please enter HASHNET Server:");
    while (!Serial.available()) {
      // Wait for input
    }
    strHASHNET_SERVER = Serial.readString();
    strHASHNET_SERVER.trim();  // Remove leading/trailing spaces

    //
    Serial.println("Please enter MINER NAME:");
    while (!Serial.available()) {
      // Wait for input
    }
    strMINER_IDENT = Serial.readString();
    strMINER_IDENT.trim();  // Remove leading/trailing spaces
  }

  // Avoid empty names
  if (strMINER_IDENT.isEmpty()) strMINER_IDENT = "ESP32_NONAME";

  // Save config
  SaveConfig();

  //
  WiFi.setHostname("HASHNET_NODE");




  // Wifi up
  WiFi.begin(strWIFI_SSID, strWIFI_Password);
  while (WiFi.status() != WL_CONNECTED) {
    pixels.setPixelColor(0, 255, 0, 0);
    pixels.show();
    Serial.println(".");
    delay(500);
  }

  // Ensure connection to HASHNET
  while (!client.connect(strHASHNET_SERVER.c_str(), SERVER_PORT)) {
    pixels.setPixelColor(0, 255, 0, 255);
    pixels.show();
    Serial.println("!");
    delay(5000);
  }

  pixels.setPixelColor(0, 0, 0, 0);
  pixels.show();
}

void loop() {

  // As simple as it gets
  if (WiFi.isConnected() == false) {
    Serial.println("WIFI");
    ESP.restart();
  }
  if (client.connected() == false) {
    Serial.println("DISCO");
    ESP.restart();
  }


  if (client.available()) {
    //String line = R"({"job_id": "648516380014efa9", "prevhash": "811c393cdc2edc26535b6cdfa4192548a4c00f580002a29c0000000000000000", "coinb1": "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff3503d51f0d000487cada660420349a0d0c", "coinb2": "0a636b706f6f6c112f736f6c6f2e636b706f6f6c2e6f72672fffffffff03f6d2cc1200000000160014cbcfd21833582f2781f78464797ca26a9026bd5a8c3862000000000016001451ed61d2f6aa260cc72cdf743e4e436a82c010270000000000000000266a24aa21a9ed6521046c65e766f460f2c535e0fd35645ec19b8e9ea66e3278fbb3f8526e266e00000000", "merkle_branch": ["da8ca658db4861ef441d5437aaf9ae47268a8f8fd05e090c6f2f7cc854651c12", "2ab39300ac0990e4f6b687731f3ed3b8ac03fbcea2872941d5ffca8bb862e57e", "1152056b5017766d350adddd51e6b49a7cd653c04f3629b4e8b2071a2af98dab", "a39420b47ae38d50f8e8fab3baa6d977fa37793a44c0db08f56a59cd067eae47", "d6d1c7bc112ba70cc878eb0462e4988d77f8c3ccb27685f2c2a6d81915f713b1", "bdf4bccf4bbbdd4beddd351b5c8d064929862c755326d15bccce7987bbb0931a", "1b806602d187d55b4151daa29832ebfd2babc30d5741e1e6614190bc5098a57d", "5f560fe23cda2769c8799d791b1441267aaf03d70aff7d3988ebd116c3810617", "bdca6ecff2be2cb922d3ff3780397048777254f2a79e4c54c01acd870f964bbe", "bf2e055878ebf4019b9eee0e5540689f62d40a5ead1d7185d04f6516651066f1", "de900b17a6712c740d138191506f2107ca1f6e51cceba72284a310dca34b438d", "8255f9a5496fdc73ae118134ce1349f86a72f6a60f40c0c931c3d5d64a1bb700"], "version": "20000000", "nbits": "1703255b", "ntime": "66daca87", "extranonce1": "61e36875", "extranonce2_size": 8, "clean_jobs": false, "method": "work", "nonce_start": 457000000, "nonce_end": 458000000, "extranonce2": "9ff3fad690bad6f1", "sversion": "1.1"})";
    String line = client.readStringUntil('\n');

    // Parse the received JSON
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, line);

    std::string method = doc["method"].as<std::string>();
    if (method == "work") {
      std::vector<uint8_t> vHeader;

      // Check if versions match
      String strServerVersion = doc["sversion"].as<String>();
      if (strServerVersion != strVersion) {
        Serial.println("Version mismatch " + strServerVersion + " " + strVersion);

        // We have to OTA now
        client.stop();

        if (OTA()) {
          Serial.println("OTA completed");
        } else Serial.println("OTA failed");

        // Boot at the end
        ESP.restart();
      }

      // Extract the work details
      job_id = doc["job_id"].as<std::string>();
      prevhash = doc["prevhash"].as<std::string>();
      coinb1 = doc["coinb1"].as<std::string>();
      coinb2 = doc["coinb2"].as<std::string>();
      version = doc["version"].as<std::string>();
      nbits = doc["nbits"].as<std::string>();
      ntime = doc["ntime"].as<std::string>();
      extranonce1 = doc["extranonce1"].as<std::string>();
      extranonce2_size = doc["extranonce2_size"];
      nonce_start = doc["nonce_start"];
      nonce_end = doc["nonce_end"];
      extranonce2 = doc["extranonce2"].as<std::string>();

      // Extract the merkle_branch
      std::vector<std::string> merkle_branch;
      for (size_t i = 0; i < doc["merkle_branch"].size(); i++) {
        merkle_branch.push_back(doc["merkle_branch"][i].as<std::string>());
      }

      // Calculate target this never changes
      nbitsToTarget(nbits, target);

      // Generate coinbase and calculate its hash
      std::string coinbase = coinb1 + extranonce1 + extranonce2 + coinb2;
      size_t str_len = coinbase.length() / 2;
      uint8_t coinbase_bytes[str_len];
      to_byte_array(coinbase.c_str(), str_len * 2, coinbase_bytes);
      uint8_t final_hash[32];
      sha256_double(coinbase_bytes, str_len, final_hash);
      std::string coinbase_hash_hex = bin_to_hex(final_hash, sizeof(final_hash));

      // Merkle
      std::string merkle_root = calculate_merkle_root_to_hex(final_hash, 32, merkle_branch);

      // BLOCK CONSTRUCTION (dummy nonce 000000 at the end)
      std::string blockheader = version + prevhash + merkle_root + nbits + ntime + "00000000";
      str_len = blockheader.length() / 2;
      uint8_t bytearray_blockheader[str_len];
      to_byte_array(blockheader.c_str(), str_len * 2, bytearray_blockheader);

      // Reverse specific sections of the block header
      reverse_bytes(bytearray_blockheader, 0, 4);    // Reverse version (4 bytes)
      reverse_bytes(bytearray_blockheader, 36, 32);  // Reverse merkle root (32 bytes)
      reverse_bytes(bytearray_blockheader, 72, 4);   // Reverse difficulty (4 bytes)

      // Prepare the initial state for nerdSHA256plus
      nerd_mids(&midstate0, bytearray_blockheader);

      // Prepare the initial state for nerdSHA256plus
      nerd_mids(&midstate1, bytearray_blockheader);

      // Clear out the work
      bTask0Completed = false;
      bTask1Completed = false;
      iLastZeroBitsDistance = 0;
      strBestDistanceBlockHash.clear();

      // Dump work material
      Serial.println("Start: " + String(nonce_start) + " End " + String(nonce_end) + " EN2 " + String(extranonce2.c_str()));
      Serial.print("Target: ");
      for (size_t x = 0; x < sizeof(target); x++) {
        Serial.printf("%02X", target[x]);
      }
      Serial.println("");


      // Create two tasks, one for each core
      xTaskCreatePinnedToCore(
        taskSearchCore0,  // Task function for core 0
        "Task Core 0",    // Name of the task
        10000,            // Stack size
        NULL,             // Parameter
        1,                // Priority
        NULL,             // Task handle
        0                 // Core ID (core 0)
      );

      xTaskCreatePinnedToCore(
        taskSearchCore1,  // Task function for core 1
        "Task Core 1",    // Name of the task
        10000,            // Stack size
        NULL,             // Parameter
        1,                // Priority
        NULL,             // Task handle
        1                 // Core ID (core 1)
      );

      while (!bTask0Completed || !bTask1Completed) {
        yield();

        // Stats
        if (millis() - uiTick >= CHECK_INTERVAL) {

          if (!client.connected()) {
            ESP.restart();
          }
          pixels.setPixelColor(0, 0, 255, 0);
          pixels.show();

          uint32_t uiTotalCS = (uiTask0CS + uiTask1CS) / (CHECK_INTERVAL / 1000);
          String strPayload = String("{\"method\": \"stats\", \"combinations_per_sec\": ") + uiTotalCS + ", \"distance\": " + String(iLastZeroBitsDistance) + ", \"ident\":\"" + strMINER_IDENT + "\",\"version\":\"" + strVersion + "\",\"besthash\":\"" + strBestDistanceBlockHash + "\"}";
          client.println(strPayload);
          Serial.println(strPayload);

          pixels.setPixelColor(0, 0, 0, 0);
          pixels.show();

          uiTask0CS = 0;
          uiTask1CS = 0;
          uiTick = millis();
        }
      }

      // Nonces check completed
      String progressPayload = String("{\"method\": \"completed\", \"job_id\": \"") + job_id.c_str() + "\", \"nonce_start\": " + String(nonce_start) + ", \"nonce_end\": " + String(nonce_end) + "}";
      client.println(progressPayload);
    }
  }
}