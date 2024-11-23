/*
* HashNet XPlatform (W32/Linux) client 
* https://github.com/invpe/HashNet/
*/
#include <assert.h>
#include <random>
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <sodium.h>
#include "../../../Core/CJZon.h"
#define STATS_INTERVAL 1
#define CHECK_INTERVAL 5
#define SHA256M_BLOCK_SIZE 32 
#define MINER_VERSION "1.2"
static const uint32_t EXPONENT_SHIFT = 24;
static const uint32_t MANTISSA_MASK = 0xffffff;
uint32_t uiHashesPerSec = 0;
int uiTick = 0;
int leadingZeroBits = 0;
int iLastZeroBitsDistance = 0;
int sock = - 1;

    


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
void sha256_double(const void *data, size_t len, uint8_t output[32]) {
    uint8_t intermediate_hash[32];
    crypto_hash_sha256(intermediate_hash, (const unsigned char *)data, len);
    crypto_hash_sha256(output, intermediate_hash, sizeof(intermediate_hash));
}
 
// Initialize SHA256 context with the block header (without nonce)
void sha256_init_with_intermediate_state(const uint8_t *bytearray_blockheader, size_t header_size_without_nonce, crypto_hash_sha256_state* state) {
    crypto_hash_sha256_init(state);
    crypto_hash_sha256_update(state, bytearray_blockheader, header_size_without_nonce);
}
std::string double_sha256_to_bin_string(const std::string &input) {
  uint8_t final_hash[32];
  sha256_double(reinterpret_cast<const unsigned char *>(input.data()), input.size(), final_hash);
  return std::string(reinterpret_cast<char *>(final_hash), sizeof(final_hash));
}
// Finalize the double SHA256 hash with the nonce
void sha256_double_with_intermediate_state(crypto_hash_sha256_state* state, uint32_t nonce, uint8_t output[32]) {
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
void nbitsToBETarget(const std::string &nbits, uint8_t target[32]) {
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


int connect_to_pool(const char* host, int port) {
    hostent *he;
    if ((he = gethostbyname(host)) == 0) {
        std::cerr << "Cant get hostname]\n";
        return -1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = *((in_addr *)he->h_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection Failed\n";
        return -1;
    }
    return sock;
}

void send_data(const std::string& data) {  
    send(sock, data.c_str(), data.size(), 0);
}

std::string receive_data() {
    char buffer[5024] = {0};
    ssize_t bytes_read = recv(sock, buffer, sizeof(buffer), 0);

    if (bytes_read == 0) {
        // Connection has been gracefully closed by the server
        std::cerr << "Connection closed by server\n";
        close(sock);
        sock=-1;
        return "-1";
    } else if (bytes_read < 0) {
        // An error occurred
        std::cerr << "Error receiving data\n";
        close(sock);
        sock=-1;
        return "-1";
    }

    return std::string(buffer, bytes_read);
}

int main(int argc, char*argv[]) {   
  
    if(argc<3)
    {
        printf("./start server node_id\n");
        exit(0);
    }

    std::string strHost = argv[1];
    std::string strIdent = argv[2];

    if (sodium_init() < 0) {
        std::cerr << "libsodium initialization failed" << std::endl;
        return 1;
    }  
    while(1)
    {
        // Disco
        if(sock<=0)
        {
          sleep(5);

          printf("[MINER] Connecting to the pool\n");
          sock = connect_to_pool(strHost.c_str(), 5000);


          if (sock == -1)
          {
              printf("[MINER] Cant connect\n"); 
          }
          else
            printf("[MINER] Connected\n");

        }
        else
        {

          std::string strJson = receive_data();
          if(strJson == "-1")
          {
              printf("[MINER] Disconnected %i\n",sock); 
          }
          else
          {
            Jzon::Node rootNode;
            Jzon::Parser _Parser; 
            //////strJson = R"({"job_id": "648516380014efa9", "prevhash": "811c393cdc2edc26535b6cdfa4192548a4c00f580002a29c0000000000000000", "coinb1": "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff3503d51f0d000487cada660420349a0d0c", "coinb2": "0a636b706f6f6c112f736f6c6f2e636b706f6f6c2e6f72672fffffffff03f6d2cc1200000000160014cbcfd21833582f2781f78464797ca26a9026bd5a8c3862000000000016001451ed61d2f6aa260cc72cdf743e4e436a82c010270000000000000000266a24aa21a9ed6521046c65e766f460f2c535e0fd35645ec19b8e9ea66e3278fbb3f8526e266e00000000", "merkle_branch": ["da8ca658db4861ef441d5437aaf9ae47268a8f8fd05e090c6f2f7cc854651c12", "2ab39300ac0990e4f6b687731f3ed3b8ac03fbcea2872941d5ffca8bb862e57e", "1152056b5017766d350adddd51e6b49a7cd653c04f3629b4e8b2071a2af98dab", "a39420b47ae38d50f8e8fab3baa6d977fa37793a44c0db08f56a59cd067eae47", "d6d1c7bc112ba70cc878eb0462e4988d77f8c3ccb27685f2c2a6d81915f713b1", "bdf4bccf4bbbdd4beddd351b5c8d064929862c755326d15bccce7987bbb0931a", "1b806602d187d55b4151daa29832ebfd2babc30d5741e1e6614190bc5098a57d", "5f560fe23cda2769c8799d791b1441267aaf03d70aff7d3988ebd116c3810617", "bdca6ecff2be2cb922d3ff3780397048777254f2a79e4c54c01acd870f964bbe", "bf2e055878ebf4019b9eee0e5540689f62d40a5ead1d7185d04f6516651066f1", "de900b17a6712c740d138191506f2107ca1f6e51cceba72284a310dca34b438d", "8255f9a5496fdc73ae118134ce1349f86a72f6a60f40c0c931c3d5d64a1bb700"], "version": "20000000", "nbits": "1703255b", "ntime": "66daca87", "extranonce1": "61e36875", "extranonce2_size": 8, "clean_jobs": false, "method": "work", "nonce_start": 457000000, "nonce_end": 458000000, "extranonce2": "9ff3fad690bad6f1", "sversion": "1.2"})";
            
            printf("[GOT] %s\n", strJson.data());

            rootNode = _Parser.parseString(strJson);
            std::string strMethod = rootNode.get("method").toString();

            if(strMethod == "work")
            {   
                uint32_t uiNonce = 0; 
                uint64_t uiExtraNonce2 = 0;
                int extranonce2_size;
                uint32_t nonce_start, nonce_end;
                std::vector<uint8_t> vHeader;
                uint8_t block_hash[32];
                uint8_t target[32];
                std::string job_id = rootNode.get("job_id").toString();
                std::string prevhash = rootNode.get("prevhash").toString();
                std::string coinb1 = rootNode.get("coinb1").toString();
                std::string coinb2 = rootNode.get("coinb2").toString();
                std::string version = rootNode.get("version").toString();
                std::string nbits = rootNode.get("nbits").toString();
                std::string ntime = rootNode.get("ntime").toString();
                std::string extranonce1 = rootNode.get("extranonce1").toString();
                extranonce2_size = rootNode.get("extranonce2_size").toInt();
                nonce_start = std::stoull(rootNode.get("nonce_start").toString());
                nonce_end = std::stoull(rootNode.get("nonce_end").toString());
                std::string extranonce2 = rootNode.get("extranonce2").toString();
                std::string strServerVersion = rootNode.get("sversion").toString();
                std::vector<std::string> merkle_branch;
                const Jzon::Node jzMerkeleBranch = rootNode.get("merkle_branch");   
                for(int a = 0; a < jzMerkeleBranch.getCount(); a++) {
                    Jzon::Node _Node = jzMerkeleBranch.get(a); 
                    std::string strMerkle =  _Node.toString();                
                    merkle_branch.push_back(strMerkle);        
                }



                uiNonce = nonce_start;

                if(strServerVersion != MINER_VERSION)
                {
                    printf("[ERROR] Version mismatch\n");
                    return -1;
                }

                
                // Calculate target this never changes
              nbitsToBETarget(nbits, target);
              std::string targetStr = byteArrayToHexString(target, 32);

              // Generate coinbase and calculate its hash
              std::string coinbase = coinb1 + extranonce1 + extranonce2 + coinb2;
              size_t str_len = coinbase.length() / 2;
              uint8_t coinbase_bytes[str_len];
              to_byte_array(coinbase.c_str(), str_len * 2, coinbase_bytes);
              uint8_t final_hash[32];
              sha256_double(coinbase_bytes, str_len, final_hash);
              std::string coinbase_hash_hex = bin_to_hex(final_hash, sizeof(final_hash));
              printf("CBASE: %s\n", coinbase_hash_hex.data());

              // Merkle
              std::string merkle_root = calculate_merkle_root_to_hex(final_hash, 32, merkle_branch);

              // BLOCK CONSTRUCTION (dummy nonce 000000 at the end)
              std::string blockheader = version + prevhash + merkle_root + nbits + ntime + "00000000";
              str_len = blockheader.length() / 2;
              uint8_t bytearray_blockheader[str_len];
              to_byte_array(blockheader.c_str(), str_len * 2, bytearray_blockheader);

              // Reverse specific sections of the block header 
              reverse_bytes(bytearray_blockheader, 36, 32);  // Reverse merkle root (32 bytes) 

              // Initialize SHA256 state with intermediate state (excluding nonce)
              crypto_hash_sha256_state sha256_state;
              sha256_init_with_intermediate_state(bytearray_blockheader, 76 /*without nonce*/, &sha256_state);

              // Clear stats
              iLastZeroBitsDistance = 0;
              leadingZeroBits = 0;
              std::string blockHashStr = "";           
               
              
              printf("Blockdump:");
              for (size_t i = 0; i < sizeof(bytearray_blockheader); i++) printf("%02x", bytearray_blockheader[i]);
              printf("\n");

              printf("Target:");
              for (size_t i = 0; i < sizeof(target); i++) printf("%02x", target[i]);
              printf("\n"); 
              
              printf("[MINER] Enonce2 %s  Iterating %u - %u\n", 
                extranonce2.data(), 
                nonce_start, 
                nonce_end
                );

              for (uiNonce=nonce_start; uiNonce < nonce_end; uiNonce++) 
              {            

                /*
                bytearray_blockheader[76] = (uiNonce >> 0) & 0xFF;   // Least significant byte first
                bytearray_blockheader[77] = (uiNonce >> 8) & 0xFF;
                bytearray_blockheader[78] = (uiNonce >> 16) & 0xFF;
                bytearray_blockheader[79] = (uiNonce >> 24) & 0xFF;
                sha256_double(bytearray_blockheader, sizeof(bytearray_blockheader), block_hash);
                */

                  sha256_double_with_intermediate_state(&sha256_state, uiNonce, block_hash);                
                  leadingZeroBits = calculateDistanceToTarget(block_hash,sizeof(block_hash));

                  // Update iLastZeroBitsDistance if the miner found a closer distance to the target
                  if (leadingZeroBits > iLastZeroBitsDistance) 
                  {
                        iLastZeroBitsDistance = leadingZeroBits;
                        blockHashStr = byteArrayToHexString(block_hash, 32); 
                        printf("[MINER] Distance %u %s\n", iLastZeroBitsDistance,blockHashStr.data());

                        printf("[BLOCK] ");
                        for (size_t i = 0; i < sizeof(bytearray_blockheader); i++) printf("%02x", bytearray_blockheader[i]);
                        printf("\n");
                  
                        // Only compare if we're progressing towards the target
                        if (checkValid(block_hash, target)) 
                        {
                            
                           std::string strTemp = std::string("{\"method\": \"found\", \"job_id\": \"") + job_id
                                  + std::string("\", \"extranonce1\": \"") + extranonce1
                                  + std::string("\", \"extranonce2\": \"") + extranonce2
                                  + std::string("\", \"nonce\": \"") + std::to_string(uiNonce)
                                  + std::string("\", \"ntime\": \"") + ntime + "\"}";


                            send_data(strTemp);                        
                            printf("[SUCCESS] Found %s\n", strTemp.data());
                            printf("[SUCCESS] %s\n",blockHashStr.data());               
                            break;
                        }
                  }

                  uiHashesPerSec++;

                  // Stats
                  if (time(0) - uiTick >= STATS_INTERVAL) {
                      std::string strPayload = std::string("{\"method\": \"stats\", \"combinations_per_sec\": ") + std::to_string(uiHashesPerSec/STATS_INTERVAL) + ", \"distance\": " + std::to_string(iLastZeroBitsDistance) + ", \"ident\":\"" + strIdent + "\",\"version\":\"" + MINER_VERSION + "\",\"besthash\":\"" + blockHashStr + "\", \"current_nonce\": "+std::to_string(uiNonce)+"}\r\n";                    
                      send_data(strPayload);                    
                      uiTick = time(0);
                      printf("[HASHRATE] %s\n", std::to_string(uiHashesPerSec/STATS_INTERVAL).data());
                      uiHashesPerSec = 0;
                  }

              } 

              // Nonce check completed
              // Report progress and request the next chunk
              std::string progressPayload = std::string("{\"method\": \"completed\", \"job_id\": \"") + job_id + "\", \"nonce_start\": " + std::to_string(nonce_start) + ", \"nonce_end\": " + std::to_string(nonce_end) + "}\r\n";
              send_data(progressPayload);
            } 
          } 

        }
    }
}
  
