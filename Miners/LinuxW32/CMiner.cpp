/*
  HASHNET (https://github.com/invpe/HashNet) miner node (c) invpe 2k24  
*/
#include "CMiner.h"

CMiner::CMiner() {
  uiHashesPerSec = 0;
  uiTick = time(0);
  iCurrentJobScore = 0;
  iCurrentJobScoreBest = 0;
}

bool CMiner::Setup(const std::string& rstrServerName, const std::string& rstrMinerIdent) {
  m_strServerName = rstrServerName;
  m_strIdent = rstrMinerIdent;

  if (sodium_init() < 0) {
    std::cerr << "libsodium initialization failed" << std::endl;
    return false;
  }
  return true;
}
void CMiner::SetLogging(const bool& bFlag) {
  m_bLogging = bFlag;
}
void CMiner::Tick() {
  // Disco
  if (sock <= 0) {

    printf("[MAIN] Connecting to the pool\n");
    sock = connect_to_pool(m_strServerName.c_str(), 5000);


    if (sock == -1) {
      printf("[ERROR] Cant connect\n");
      sleep(5);
    } else
      printf("[MAIN] Connected\n");

  } else {

    std::string strJson = receive_data();
    if (strJson == "-1") {
      printf("[ERROR] Disconnected %i\n", sock);
      exit(0);
    } else {
      Jzon::Node rootNode;
      Jzon::Parser _Parser;
      //////strJson = R"({"job_id": "648516380014efa9", "prevhash": "811c393cdc2edc26535b6cdfa4192548a4c00f580002a29c0000000000000000", "coinb1": "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff3503d51f0d000487cada660420349a0d0c", "coinb2": "0a636b706f6f6c112f736f6c6f2e636b706f6f6c2e6f72672fffffffff03f6d2cc1200000000160014cbcfd21833582f2781f78464797ca26a9026bd5a8c3862000000000016001451ed61d2f6aa260cc72cdf743e4e436a82c010270000000000000000266a24aa21a9ed6521046c65e766f460f2c535e0fd35645ec19b8e9ea66e3278fbb3f8526e266e00000000", "merkle_branch": ["da8ca658db4861ef441d5437aaf9ae47268a8f8fd05e090c6f2f7cc854651c12", "2ab39300ac0990e4f6b687731f3ed3b8ac03fbcea2872941d5ffca8bb862e57e", "1152056b5017766d350adddd51e6b49a7cd653c04f3629b4e8b2071a2af98dab", "a39420b47ae38d50f8e8fab3baa6d977fa37793a44c0db08f56a59cd067eae47", "d6d1c7bc112ba70cc878eb0462e4988d77f8c3ccb27685f2c2a6d81915f713b1", "bdf4bccf4bbbdd4beddd351b5c8d064929862c755326d15bccce7987bbb0931a", "1b806602d187d55b4151daa29832ebfd2babc30d5741e1e6614190bc5098a57d", "5f560fe23cda2769c8799d791b1441267aaf03d70aff7d3988ebd116c3810617", "bdca6ecff2be2cb922d3ff3780397048777254f2a79e4c54c01acd870f964bbe", "bf2e055878ebf4019b9eee0e5540689f62d40a5ead1d7185d04f6516651066f1", "de900b17a6712c740d138191506f2107ca1f6e51cceba72284a310dca34b438d", "8255f9a5496fdc73ae118134ce1349f86a72f6a60f40c0c931c3d5d64a1bb700"], "version": "20000000", "nbits": "1703255b", "ntime": "66daca87", "extranonce1": "61e36875", "extranonce2_size": 8, "clean_jobs": false, "method": "work", "nonce_start": 457000000, "nonce_end": 458000000, "extranonce2": "9ff3fad690bad6f1", "sversion": "1.2"})";
      //printf("[GOT] %s\n", strJson.data());

      rootNode = _Parser.parseString(strJson);
      std::string strMethod = rootNode.get("method").toString();

      std::string job_id = "";
      uint32_t nonce_start, nonce_end;

      if (strMethod == "work") {
        std::string strServerVersion = rootNode.get("sversion").toString();
        std::string strJobName = rootNode.get("name").toString();

        if (strServerVersion != MINER_VERSION) {
          printf("[BOOM] Wrong version, update your miner, %s != %s\n", strServerVersion.data(), MINER_VERSION);
          exit(0);
        }

        printf("[MINER] New Job for %s\n", strJobName.data());

        iCurrentJobScoreBest = 0;
        iCurrentJobScore = 0;


        // TODO: This should be a class
        if (strJobName == "DUCOSHA1") {
          job_id = rootNode.get("job_id").toString();
          nonce_start = std::stoull(rootNode.get("nonce_start").toString());
          nonce_end = std::stoull(rootNode.get("nonce_end").toString());
          std::string prevhash = rootNode.get("prevhash").toString();
          std::string expectedhash = rootNode.get("expectedhash").toString();
          for (int x = nonce_start; x < nonce_end; x++) {
            std::string strTemp = prevhash + std::to_string(x);
            std::string strOutput = sha1_hash(strTemp);

            uiHashesPerSec++;

            // Stats
            if (time(0) - uiTick >= STATS_INTERVAL) {
              std::string strPayload = std::string("{\"method\": \"stats\", \"combinations_per_sec\": ") + std::to_string(uiHashesPerSec / STATS_INTERVAL) + ", \"distance\": " + std::to_string(iCurrentJobScoreBest) + ", \"ident\":\"" + m_strIdent + "\",\"version\":\"" + MINER_VERSION + "\",\"current_nonce\": " + std::to_string(x) + "}\r\n";

              send_data(strPayload);
              uiTick = time(0);
              printf("[MINER] HashRate: %s\n", std::to_string(uiHashesPerSec / STATS_INTERVAL).data());
              uiHashesPerSec = 0;
            }


            // Found
            if (strOutput == expectedhash) {
              std::string strTemp = std::string("{\"method\": \"found\", \"job_id\": \"") + job_id
                                    + std::string("\", \"nonce\": \"") + std::to_string(x) + "\"}";


              send_data(strTemp);
              printf("[MINER] Succes, Found %s\n", strOutput.data());
              break;
              ;
            }
          }
        }
        //https://docs.alephium.org/mining/alephium-stratum/
        else if (strJobName == "BLAKE3") {

          //Pool-Provided headerBlob:
          //The pool sends a headerBlob that is larger than 80 bytes.
          //The miner is only required to use the first 80 bytes for the double BLAKE3 hashing.


          nonce_start = std::stoull(rootNode.get("nonce_start").toString());
          nonce_end = std::stoull(rootNode.get("nonce_end").toString());
          std::string extranonce = rootNode.get("extranonce").toString();
          std::string jobid = rootNode.get("jobId").toString();
          std::string txsBlob = rootNode.get("txsBlob").toString();
          std::string headerBlobStr = rootNode.get("headerBlob").toString();
          std::string targetBlobStr = rootNode.get("targetBlob").toString();
          uint32_t fromGroup = std::stoull(rootNode.get("fromGroup").toString());
          uint32_t toGroup = std::stoull(rootNode.get("toGroup").toString());


          // TEST SCENARIO
          // Simulate lower diff
          //targetBlobStr = "0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
          /*
          extranonce = "d7ab";
          jobid="4b5a3155";
          txsBlob="";
          headerBlobStr="000700000000000029fe96f8a098fc78c43a862761fd0e9e0d2a1dd3131994de6ae500000000000010f37d5995bf8071b20f4083df9dac4763a32a2ccdff642c307a0000000000002ba5fcaba40c0f7fa893c7dc76a80dfc927e85d1ee381de576ff000000000000238d9a09a6b3fe047bd6f18bd19b239b650be38418be07c11b6000000000000018b09d67674ab7976661e7f44094a5cfeb444c0795eb9c6921010000000000001819812e9bcdd38d4657e0c056dc0283611259a8630145df01b20000000000001df902e87ec0f605e3d69b91aed230eb0d0622f14b9ba5162f234aeddf2755ce868f272cddf4c4586196ba8c48cf5292e95195a9f935da07bd73016185e04fc2b8e3c0a31eb80044bbf13b9d3b2a1b240a735a3def2b76d2c35800000193aada59791a2d10e7";
          targetBlobStr="0000000001ffffffffffffffffffffffffffffffffffffffffffffffffffffff";
          fromGroup = 0;
          toGroup = 3;
          // Expected hash: e6a5c178a5368880218259534121ac7644749dc74c3d64634fa87a189c324e75

          printf("Got extranonce: %s\n", extranonce.data());
          printf("Got jobid: %s\n", jobid.data());
          printf("Got txsBlob: %s\n", txsBlob.data());
          printf("Got headerBlob: %s\n", headerBlobStr.data());
          printf("Got targetBlob: %s\n", targetBlobStr.data());
          printf("Got group: %u-%u\n", fromGroup, toGroup);
          
          uint8_t blockHeader[80] = { 0 };
          uint8_t targetBlob[32] = { 0 };
          uint8_t intermediateHash[32] = { 0 };

          std::string blockHashStr = "";

          // First 80 bytes from headerBlob go as Block Header
          HexStringToBytes(headerBlobStr, blockHeader, sizeof(blockHeader));
          HexStringToBytes(targetBlobStr, targetBlob, sizeof(targetBlob));


          // Precompute Intermediate Hash
          blake3_hasher hasher;
          blake3_hasher_init(&hasher);
          blake3_hasher_update(&hasher, blockHeader, sizeof(blockHeader));
          blake3_hasher_finalize(&hasher, intermediateHash, sizeof(intermediateHash));
 

          // Outside loop: Pre-initialize the hasher for the final step
          blake3_hasher finalHasher;
          blake3_hasher_init(&finalHasher);

          // Convert nonce to 24-byte big-endian hexadecimal format
            // In Alephium, the nonce is 24 bytes..
            uint64_t nonce = 0;
            uint8_t nonceBytes[24] = { 0 };  // 24-byte nonce, padded with leading zeros
            for (int i = 0; i < 8; i++)
              nonceBytes[23 - i] = (nonce >> (8 * i)) & 0xFF;  // Write nonce in big-endian


            // Concatenate nonce and intermediateHash
            uint8_t nonceWithIntermediateHash[56];  // 24-byte nonce + 32-byte intermediateHash
            memcpy(nonceWithIntermediateHash, nonceBytes, 24);
            memcpy(nonceWithIntermediateHash + 24, intermediateHash, 32);

            // Final BLAKE3 Hash
            uint8_t blockHash[32];
            blake3_hasher_reset(&finalHasher);
            blake3_hasher_update(&finalHasher, nonceWithIntermediateHash, sizeof(nonceWithIntermediateHash));
            blake3_hasher_finalize(&finalHasher, blockHash, sizeof(blockHash)); 
 
             // Now convert to little endian for our scoring to work
            uint8_t littleEndianBlockHash[32];
            memcpy(littleEndianBlockHash, blockHash, 32);
            reverse_bytes(littleEndianBlockHash, 0, 32);

            blockHashStr = byteArrayToHexString(blockHash, 32);
            printf("BLAKE3: %s\n", blockHashStr.data());*/

          uint8_t blockHeader[80] = { 0 };
          uint8_t targetBlob[32] = { 0 };
          uint8_t intermediateHash[32] = { 0 };

          std::string blockHashStr = "";

          // First 80 bytes from headerBlob go as Block Header
          HexStringToBytes(headerBlobStr, blockHeader, sizeof(blockHeader));
          HexStringToBytes(targetBlobStr, targetBlob, sizeof(targetBlob));


          // Precompute Intermediate Hash
          blake3_hasher hasher;
          blake3_hasher_init(&hasher);
          blake3_hasher_update(&hasher, blockHeader, sizeof(blockHeader));
          blake3_hasher_finalize(&hasher, intermediateHash, sizeof(intermediateHash));

          // Outside loop: Pre-initialize the hasher for the final step
          blake3_hasher finalHasher;
          blake3_hasher_init(&finalHasher);


          for (uint64_t nonce = nonce_start; nonce < nonce_end; nonce++) {

            // Convert nonce to 24-byte big-endian hexadecimal format
            // In Alephium, the nonce is 24 bytes..
            uint8_t nonceBytes[24] = { 0 };  // 24-byte nonce, padded with leading zeros
            AlephiumNonceToBytes(nonce, nonceBytes);

            // Concatenate nonce and intermediateHash
            uint8_t nonceWithIntermediateHash[56];  // 24-byte nonce + 32-byte intermediateHash
            memcpy(nonceWithIntermediateHash, nonceBytes, 24);
            memcpy(nonceWithIntermediateHash + 24, intermediateHash, 32);

            // Final BLAKE3 Hash
            uint8_t blockHash[32];
            blake3_hasher_reset(&finalHasher);
            blake3_hasher_update(&finalHasher, nonceWithIntermediateHash, sizeof(nonceWithIntermediateHash));
            blake3_hasher_finalize(&finalHasher, blockHash, sizeof(blockHash));

            // OUTPUT IS BIG ENDIAN AS EXPECTED BY THE POOL //
            // 000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff//

            iCurrentJobScore = calculateDistanceToTarget(blockHash, sizeof(blockHash));

            // Update if the miner found a closer distance to the target in this job
            if (iCurrentJobScore > iCurrentJobScoreBest)
              iCurrentJobScoreBest = iCurrentJobScore;

            // Check chain index (Alephium constants)
            const uint32_t chainNums = 16;  // Total number of chains
            const uint32_t groupNums = 4;   // Chains per group

            // Check if the hash meets the target
            if (memcmp(blockHash, targetBlob, sizeof(blockHash)) < 0) {

              blockHashStr = byteArrayToHexString(blockHash, 32);

              if (!CheckIndex(blockHash, fromGroup, toGroup, chainNums, groupNums)) {
                //printf("INVALID CHAIN %u\n",nonce);
                //printf("BLOCKHASH: %s\n", blockHashStr.data());
                //printf("TARGETHSH: %s\n", targetBlobStr.data());
              } else {

                // If extranonce provided, the padding of nonce should be - len of extranonce
                // otherwise full 16b padding of nonce https://eips.ethereum.org/EIPS/eip-1571#solution-submission
                // client { id: "4", method: "mining.submit", params: [<jobId: string>, <nonceSansExtraNonce: hex>, <workerId?: string>] }

                // Format nonce for submission
                char nonceHex[25];  // Sufficient for 12-byte suffix
                if (!extranonce.empty()) {
                  // Format only the 12-byte suffix of the nonce if extranonce is provided
                  snprintf(nonceHex, sizeof(nonceHex), "%012llx", (unsigned long long)nonce);
                } else {
                  // Format the full 16-byte nonce if no extranonce is provided
                  snprintf(nonceHex, sizeof(nonceHex), "%016llx", (unsigned long long)nonce);
                }

                // Construct the JSON payload
                std::string strTemp;
                if (!extranonce.empty()) {
                  // Extranonce exists, send only the nonce suffix
                  strTemp = std::string("{\"method\": \"found\", \"job_id\": \"") + jobid
                            + std::string("\", \"nonce\": \"") + nonceHex + "\"}";
                } else {
                  // No extranonce, send the full nonce
                  char fullNonce[33];  // Sufficient for 16 bytes
                  snprintf(fullNonce, sizeof(fullNonce), "%016llx", (unsigned long long)nonce);
                  strTemp = std::string("{\"method\": \"found\", \"job_id\": \"") + jobid
                            + std::string("\", \"nonce\": \"") + fullNonce + "\"}";
                }

                send_data(strTemp);

                printf("[MINER] Success, Found %s Nonce: %llu (%s) %s\n", blockHashStr.data(), (unsigned long long)nonce, nonceHex, blockHashStr.data());
              }
            }


            uiHashesPerSec++;

            // Stats, report best distance for this extranonce2 job
            if (time(0) - uiTick >= STATS_INTERVAL) {
              std::string strPayload = std::string("{\"method\": \"stats\", \"combinations_per_sec\": ") + std::to_string(uiHashesPerSec / STATS_INTERVAL) + ", \"distance\": " + std::to_string(0) + ", \"ident\":\"" + m_strIdent + "\",\"version\":\"" + MINER_VERSION + "\",\"besthash\":\"" + blockHashStr + "\", \"current_nonce\": " + std::to_string(nonce) + "}\r\n";


              send_data(strPayload);
              printf("[MINER] HashRate: %s BestScore: %i Tgt: %s\n",
                     std::to_string(uiHashesPerSec / STATS_INTERVAL).data(),
                     iCurrentJobScoreBest,
                     targetBlobStr.data());
              uiHashesPerSec = 0;
              uiTick = time(0);
            }
          }

        } else if (strJobName == "DSHA256") {

          uint32_t uiNonce = 0;
          uint64_t uiExtraNonce2 = 0;
          int extranonce2_size;
          std::vector<uint8_t> vHeader;
          uint8_t block_hash[32];
          uint8_t target[32];
          job_id = rootNode.get("job_id").toString();
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
          std::vector<std::string> merkle_branch;
          const Jzon::Node jzMerkeleBranch = rootNode.get("merkle_branch");
          for (int a = 0; a < jzMerkeleBranch.getCount(); a++) {
            Jzon::Node _Node = jzMerkeleBranch.get(a);
            std::string strMerkle = _Node.toString();
            merkle_branch.push_back(strMerkle);
          }

          uiNonce = nonce_start;


          // Calculate target this never changes
          nbitsToBETarget(nbits, target);
          std::string targetStr = byteArrayToHexString(target, 32);

          // Generate coinbase and calculate its hash + merkle
          std::string coinbase = coinb1 + extranonce1 + extranonce2 + coinb2;
          size_t str_len = coinbase.length() / 2;
          uint8_t coinbase_bytes[str_len];
          to_byte_array(coinbase.c_str(), str_len * 2, coinbase_bytes);
          uint8_t final_hash[32];
          sha256_double(coinbase_bytes, str_len, final_hash);
          std::string coinbase_hash_hex = bin_to_hex(final_hash, sizeof(final_hash));
          std::string merkle_root = calculate_merkle_root_to_hex(final_hash, 32, merkle_branch);

          // BLOCK CONSTRUCTION (dummy nonce 000000 at the end)
          // https://en.bitcoin.it/wiki/Block_hashing_algorithm
          std::string blockheader = version + prevhash + merkle_root + ntime + nbits + "00000000";

          str_len = blockheader.length() / 2;
          uint8_t bytearray_blockheader[str_len];
          to_byte_array(blockheader.c_str(), str_len * 2, bytearray_blockheader);

          // Reverse specific sections of the block header
          reverse_bytes(bytearray_blockheader, 36, 32);  // Reverse merkle root (32 bytes)

          // Clear stats
          std::string blockHashStr = "";

          for (uiNonce = nonce_start; uiNonce < nonce_end; uiNonce++) {
            //memcpy(bytearray_blockheader+76,&uiNonce,sizeof(uiNonce));
            //sha256_double(bytearray_blockheader, sizeof(bytearray_blockheader), block_hash);

            // Initialize SHA256 state with intermediate state (excluding nonce)
            crypto_hash_sha256_state sha256_state;
            sha256_init_with_intermediate_state(bytearray_blockheader, 76 /*without nonce*/, &sha256_state);
            sha256_double_with_intermediate_state(&sha256_state, uiNonce, block_hash);

            // Calculates zeroes from left to right (BIG ENDIAN 00000ffffff) on our generated hash.
            iCurrentJobScore = calculateDistanceToTarget(block_hash, sizeof(block_hash));

            // Update if the miner found a closer distance to the target in this job
            if (iCurrentJobScore >= iCurrentJobScoreBest) {
              iCurrentJobScoreBest = iCurrentJobScore;
              blockHashStr = byteArrayToHexString(block_hash, 32);

              // Only compare if we're progressing towards the target
              if (checkValid(block_hash, target)) {
                std::ostringstream oss;                                           // Create an ostringstream object
                oss << std::setw(8) << std::setfill('0') << std::hex << uiNonce;  // Format the value
                std::string strNonceHex = oss.str();                              // Retrieve the formatted string

                std::string strTemp = std::string("{\"method\": \"found\", \"job_id\": \"") + job_id
                                      + std::string("\", \"extranonce1\": \"") + extranonce1
                                      + std::string("\", \"extranonce2\": \"") + extranonce2
                                      + std::string("\", \"nonce\": \"") + strNonceHex
                                      + std::string("\", \"ntime\": \"") + ntime + "\"}";


                send_data(strTemp);

                printf("[MINER] Success, Found %s Nonce: %u\n", blockHashStr.data(), uiNonce);
                //printf("[MINER] Success, Found %s\n", strTemp.data());
                //printf("[SUCCESS] ");
                //for (size_t i = 0; i < sizeof(bytearray_blockheader); i++) printf("%02x", bytearray_blockheader[i]);
                //printf("\n");
                break;
              }
            }

            uiHashesPerSec++;

            // Stats, report best distance for this extranonce2 job
            if (time(0) - uiTick >= STATS_INTERVAL) {
              std::string strPayload = std::string("{\"method\": \"stats\", \"combinations_per_sec\": ") + std::to_string(uiHashesPerSec / STATS_INTERVAL) + ", \"distance\": " + std::to_string(iCurrentJobScoreBest) + ", \"ident\":\"" + m_strIdent + "\",\"version\":\"" + MINER_VERSION + "\",\"besthash\":\"" + blockHashStr + "\", \"current_nonce\": " + std::to_string(uiNonce) + "}\r\n";
              send_data(strPayload);
              printf("[MINER] HashRate: %s BestScore: %i Target: %s\n",
                     std::to_string(uiHashesPerSec / STATS_INTERVAL).data(), iCurrentJobScoreBest, targetStr.data());
              uiHashesPerSec = 0;
              uiTick = time(0);
            }
          }
        } else {
          printf("[MINER] Unknown job type, skipping\n");
        }




        // Nonce check completed
        // Report progress and request the next chunk
        std::string progressPayload = std::string("{\"method\": \"completed\", \"job_id\": \"") + job_id + "\", \"nonce_start\": " + std::to_string(nonce_start) + ", \"nonce_end\": " + std::to_string(nonce_end) + "}\r\n";
        send_data(progressPayload);
      }
    }
  }
}
CMiner::~CMiner() {
}
