/*
  HASHNET (https://github.com/invpe/HashNet) miner node (c) invpe 2k24  
*/
#ifndef __CMINER_H__
#define __CMINER_H__
#include <signal.h>
#include <assert.h>
#include <random>
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <unistd.h>
#include <cstring>
#include <sodium.h>
#include "CJZon.h"
#include "sha1.hpp"
#include "BLAKE3/c/blake3.h"

#ifdef __WIN32__
#include <winsock2.h>  // Winsock library for Windows socket handling
#include <ws2tcpip.h>  // For address structures and inet_pton
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#define STATS_INTERVAL 1
#define CHECK_INTERVAL 5
#define SHA256M_BLOCK_SIZE 32
#define MINER_VERSION "1.3"
#define EXPONENT_SHIFT 24;
#define MANTISSA_MASK 0xffffff;


class CMiner {
public:
  CMiner();
  ~CMiner();
  bool Setup(const std::string &rstrServerName, const std::string &rstrMinerIdent);
  void Tick();
  void SetLogging(const bool &bFlag);

private:

  inline uint32_t bytereverse(uint32_t x) {
    return (((x) << 24) | (((x) << 8) & 0x00ff0000) | (((x) >> 8) & 0x0000ff00) | ((x) >> 24));
  }

  std::string sha1_hash(const std::string &input) {
    SHA1 checksum;
    checksum.update(input);
    return checksum.final();
  }
  bool checkValid(unsigned char *hash, unsigned char *target) {
    for (int i = 0; i < 32; i++) {  // Compare from most significant byte (byte 0) to least significant byte

      if (hash[i] > target[i]) {
        return false;  // Hash is greater than the target, invalid
      } else if (hash[i] < target[i]) {
        return true;  // Hash is smaller than the target, valid
      }
      // If hash[i] == target[i], continue comparing the next byte
    }

    // If all bytes are equal, it's valid
    return true;
  }
  // Convert uint64_t to big-endian format
  void ConvertToBigEndian(uint64_t value, uint8_t *buffer, size_t length) {
    for (size_t i = 0; i < length; ++i) {
      buffer[length - 1 - i] = (value >> (i * 8)) & 0xFF;
    }
  }
  inline void AlephiumNonceToBytes(uint64_t nonce, uint8_t *nonceBytes) {
    for (int i = 0; i < 8; i++)
      nonceBytes[23 - i] = (nonce >> (8 * i)) & 0xFF;
  }

  // Check chain index
  bool CheckIndex(uint8_t *blockHash, uint32_t fromGroup, uint32_t toGroup, uint32_t chainNums, uint32_t groupNums) {
    uint8_t bigIndex = blockHash[31] % chainNums;  // Extract chain index
    return (bigIndex / groupNums == fromGroup) && (bigIndex % groupNums == toGroup);
  }
  void sha256_double(const void *data, size_t len, uint8_t output[32]) {
    uint8_t intermediate_hash[32];
    crypto_hash_sha256(intermediate_hash, (const unsigned char *)data, len);
    crypto_hash_sha256(output, intermediate_hash, sizeof(intermediate_hash));
  }

  // Initialize SHA256 context with the block header (without nonce)
  void sha256_init_with_intermediate_state(const uint8_t *bytearray_blockheader, size_t header_size_without_nonce, crypto_hash_sha256_state *state) {
    crypto_hash_sha256_init(state);
    crypto_hash_sha256_update(state, bytearray_blockheader, header_size_without_nonce);
  }
  std::string double_sha256_to_bin_string(const std::string &input) {
    uint8_t final_hash[32];
    sha256_double(reinterpret_cast<const unsigned char *>(input.data()), input.size(), final_hash);
    return std::string(reinterpret_cast<char *>(final_hash), sizeof(final_hash));
  }
  // Finalize the double SHA256 hash with the nonce
  void sha256_double_with_intermediate_state(crypto_hash_sha256_state *state, uint32_t nonce, uint8_t output[32]) {
    uint8_t nonce_bytes[4];
    nonce_bytes[0] = (nonce >> 0) & 0xFF;  // Little-endian
    nonce_bytes[1] = (nonce >> 8) & 0xFF;
    nonce_bytes[2] = (nonce >> 16) & 0xFF;
    nonce_bytes[3] = (nonce >> 24) & 0xFF;

    // Copy the state to avoid modifying the original state
    crypto_hash_sha256_state temp_state = *state;
    crypto_hash_sha256_update(&temp_state, nonce_bytes, sizeof(nonce_bytes));

    uint8_t intermediate_hash[32];
    crypto_hash_sha256_final(&temp_state, intermediate_hash);

    crypto_hash_sha256(output, intermediate_hash, sizeof(intermediate_hash));  // Second SHA-256
  }
  std::string byteArrayToHexString(const uint8_t *byteArray, size_t length) {
    std::ostringstream oss;
    for (size_t i = 0; i < length; ++i) {
      oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byteArray[i]);
    }
    return oss.str();
  }
  bool CheckChainIndex(const uint8_t *blockHash, int expectedChainIndex) {
    // Extract the chain index from the block hash (e.g., using specific bits)
    // Assume the chain index is encoded in the lower 4 bits of the first byte (example)
    int chainIndex = blockHash[0] & 0x0F;

    // Compare with the expected chain index
    return (chainIndex == expectedChainIndex);
  }

  void HexStringToBytes(const std::string &hex, uint8_t *bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
      bytes[i] = std::stoi(hex.substr(i * 2, 2), nullptr, 16);
    }
  }
  void nbitsToBETarget(const std::string &nbits, uint8_t target[32]) {
    memset(target, 0, 32);

    char *endPtr;
    uint32_t bits_value = strtoul(nbits.c_str(), &endPtr, 16);

    if (*endPtr != '\0' || bits_value == 0) {
      std::cerr << "Error: Invalid nbits value" << std::endl;
    }

    uint32_t exp = bits_value >> EXPONENT_SHIFT;
    uint32_t mant = bits_value & MANTISSA_MASK;
    uint32_t bitShift = 8 * (exp - 3);
    int byteIndex = 29 - (bitShift / 8);

    if (byteIndex < 0) {
      std::cerr << "Error: Invalid index, nBits exponent too small" << std::endl;
    }

    target[byteIndex] = (mant >> 16) & 0xFF;
    target[byteIndex + 1] = (mant >> 8) & 0xFF;
    target[byteIndex + 2] = mant & 0xFF;
  }

  inline int calculateDistanceToTarget(const uint8_t *block_hash, size_t hash_size) {
    int zero_bytes = 0;

    // Iterate from the end of block_hash towards the beginning
    for (size_t i = 0; i < hash_size; i++) {
      if (block_hash[i] == 0x00) {
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


  int connect_to_pool(const char *host, int port) {

#ifdef __WIN32__
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {

      printf("[ERROR] WSA\n");
      return -1;
    }

    struct addrinfo *result = NULL, *ptr = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int iResult = getaddrinfo(host, std::to_string(port).c_str(), &hints, &result);
    if (iResult != 0) {

      printf("[ERROR] Unknown hostname\n");
      WSACleanup();
      return -1;
    }

    sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {

      printf("[ERROR] Socket failed\n");
      freeaddrinfo(result);
      WSACleanup();
      return -1;
    }

    iResult = connect(sock, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {

      printf("[ERROR] Cant connect\n");
      closesocket(sock);
      freeaddrinfo(result);
      WSACleanup();
      return -1;
    }

    freeaddrinfo(result);
#else

    hostent *he;
    if ((he = gethostbyname(host)) == 0) {
      printf("[ERROR] Unknown hostname\n");
      return -1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = *((in_addr *)he->h_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
      printf("[ERROR] Can't connect...\n");

      return -1;
    }

#endif
    return sock;
  }

  void send_data(const std::string &data) {
    if (sock > 0) {

#ifdef __WIN32__
      ssize_t result = send(sock, data.c_str(), data.size(), 0);
#else
      ssize_t result = send(sock, data.c_str(), data.size(), MSG_NOSIGNAL);
#endif

      if (result == -1) {
#ifdef _WIN32
        int err = WSAGetLastError();
        std::cerr << "[ERROR] Send failed " << err << std::endl;
#else
        if (errno == EPIPE) {
          printf("[ERROR] Closed by server\n");
        } else {
          printf("[ERROR] Failed to send\n");
        }
#endif
      }

    } else
      printf("[ERROR] Invalid socket\n");
  }

  std::string receive_data() {
    std::string strOutput;       // To accumulate the incoming data
    char buffer[20000] = { 0 };  // Temporary buffer for each recv call

    while (true) {
      ssize_t bytes_read = recv(sock, buffer, sizeof(buffer), 0);

      if (bytes_read == 0) {
        // Connection has been gracefully closed by the server
        printf("[ERROR] Connection closed by the server\n");
        close(sock);
        sock = -1;
        return "-1";
      } else if (bytes_read < 0) {
        // An error occurred
        printf("[ERROR] Failed to receive data\n");
        close(sock);
        sock = -1;
        return "-1";
      }

      // Append the received data to the output string
      strOutput.append(buffer, bytes_read);

      // Check if a complete JSON message has been received
      size_t pos = strOutput.find('}');
      if (pos != std::string::npos) {
        // Return the full JSON message
        return strOutput.substr(0, pos + 1);
      }

      // If not complete, continue to receive more data
    }
  }


  std::string m_strServerName;
  std::string m_strIdent;
  uint32_t uiHashesPerSec;
  uint64_t uiTick;
  int iCurrentJobScore;
  int iCurrentJobScoreBest;
  bool m_bLogging;

#ifdef __WIN32__
  SOCKET sock;
#else
  int sock;
#endif
};
#endif