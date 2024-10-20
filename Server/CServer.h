/*
    HashNet X Server
    https://github.com/invpe/HashNet
*/
#ifndef __CSERVER__
#define __CSERVER__
#include <chrono> // For time-based seed
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sched.h>
#include <sys/time.h>
#include <stdint.h>
#include <math.h>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <cmath>
#include <string>
#include <limits>
#include <iterator>
#include <numeric>
#include <functional>
#include <algorithm>
#include <map>
#include <set>
#include <thread>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h> 

#include "CSystemUtil.h"
#include "json11.hpp"
#include "CSerializer.h"
#include "CClient.h"

#define BTC_WALLET "1KNbJNBmoQCDsSfgc2ZU8gif85Q4YSuRDX"
#define SERVER_LOG_INFO         0               // Info log level
#define SERVER_LOG_WARRNING     1               // Warrning log level
#define SERVER_LOG_ERROR        2               // Error log level
#define SERVER_VERSION          "1.3"           // Server version
#define SERVER_MINOR_VERSION    __DATE__" "__TIME__ 
#define SERVER_PORT             5000
#define SESSION_TIMEOUT         15              // Ping timeout
#define ITERATIONS_PER_MINER    5000000

class CServer
{
public:
 
    struct tStatistics
    {
        void Clear()
        {
            m_uiCompletedChunksTotal = 0;
            m_uiCompletedChunksForThisJob = 0;
            m_uiTotalBlocksFound = 0;
            m_uiTotalJobs = 0;
            m_uiCombinationsPerSecond=0;
        }


        uint32_t m_uiCompletedChunksTotal;
        uint32_t m_uiCompletedChunksForThisJob;
        uint32_t m_uiTotalBlocksFound;
        uint32_t m_uiTotalJobs;
        uint32_t m_uiCombinationsPerSecond;
        std::map<int,uint32_t> m_mBestDistancesEpoch;
    };

    struct tCurrentJob
    { 
        tCurrentJob(): m_RandomEngine(std::random_device{}())
        {

        }
        void Clear()
        {
            m_mMapping.clear();
            m_vMerkleBranch.clear();
            m_mBestExtranonce2.clear();
            m_mExtranonce2LastEndNonce.clear(); 
        }
        int GetBestDistance()
        {
            if (m_mBestExtranonce2.empty()) {
                return 0;
            }

            int bestDistance = std::numeric_limits<int>::min();

            for (const auto& entry : m_mBestExtranonce2) {
                int distance = entry.second;

                if (distance > bestDistance) {
                    bestDistance = distance;
                }
            }

            return bestDistance;
        }

        // Generates a random nonce2 of a pool provided size
        std::string GenerateRandomExtranonce2()
        {
            int size = std::stoi(m_mMapping["extranonce2_size"]);
  
            // Define the characters we want to use (0-9 and a-f)
            const char hex_chars[] = "0123456789abcdef";

            std::stringstream ss;            
            std::uniform_int_distribution<> dist(0, 15); // Generates a random number between 0 and 15
            for (int i = 0; i < size * 2; ++i) {  // size * 2 because each byte is 2 hex characters
                ss << hex_chars[dist(m_RandomEngine)];        // Pick a random character from hex_chars
            }

            return ss.str();  
        } 

        // Return next chunk of nonces for a given extranonce2
        std::pair<uint32_t, uint32_t> GetNextWorkChunk(const std::string& extranonce2)
        {
            
            uint32_t startNonce = 0;
            uint32_t endNonce = 0;
  
            // Check if we have a last endnonce for the given extranonce2
            if (m_mExtranonce2LastEndNonce.find(extranonce2) != m_mExtranonce2LastEndNonce.end()) {
                startNonce = m_mExtranonce2LastEndNonce[extranonce2]; // Use the last endnonce as the new startNonce
            }

            // Calculate the new endNonce
            endNonce = startNonce + ITERATIONS_PER_MINER - 1; 

            // Store the updated endNonce for this extranonce2
            // Set the next startNonce to be endNonce + 1
            m_mExtranonce2LastEndNonce[extranonce2] = endNonce + 1;  


            return {startNonce, endNonce};   

/*          ABSOLUTE RANDOMNESS - NO LOGIC AT ALL -CHECK AFTER TESTING WITH THE BEST EN2
            uint32_t max_nonce_start = UINT32_MAX - ITERATIONS_PER_MINER + 1;

            // Create the distribution with the given range
            std::uniform_int_distribution<uint32_t> dist(0, max_nonce_start);

            // Use the member variable m_RandomEngine to generate the random number
            uint32_t uiStartChunk = dist(m_RandomEngine);
            uint32_t uiEndChunk = uiStartChunk + ITERATIONS_PER_MINER - 1;

            return {uiStartChunk, uiEndChunk};*/


        }

        // Core logic, roll the dice to either use random extranonce2 or choose
        // one of the best extranonce2's we've got so far.
        std::string GetNextExtranonce2()
        {
            std::string strExtranonce2 = GenerateRandomExtranonce2();
 
            
            // 60% chance to pick from the best extranonce2 if available
            if (rand() % 100 < 60 && !m_mBestExtranonce2.empty()) {
 
                std::vector<std::string> bestExtranonce2List;
                for (const auto& entry : m_mBestExtranonce2) {
                    bestExtranonce2List.push_back(entry.first);
                }

                // Generate a random index and select a random extranonce2 
                std::uniform_int_distribution<> dist(0, bestExtranonce2List.size() - 1);
                strExtranonce2 = bestExtranonce2List[dist(m_RandomEngine)];
            }
            


            return strExtranonce2;
        }

        std::map<std::string, std::string> m_mMapping;
        std::vector<std::string> m_vMerkleBranch; 
        std::map<std::string,int> m_mBestExtranonce2;  
        std::map<std::string, uint32_t> m_mExtranonce2LastEndNonce;   

        std::mt19937 m_RandomEngine; 
  
    };

    CServer();
    ~CServer();
    
    bool Start(const int16_t& ruiPort, const uint16_t& ruiMaxSessions);
    void Tick();
    void Stop(); 

private: 

    bool POOL_Connected();
    bool POOL_Connect();
    void POOL_Disconnect();
    bool POOL_Send(const std::string& rstrData);
    std::string POOL_Receive();
    
    CClient *GetClientByIdent(const std::string& strMAC); 
    bool SendTo(CClient* pClient, const std::string& strData );   
    void SaveLog(const int& iType, const std::string& strText);   
    void DisconnectMiner(CClient *pMiner);
    void DisconnectIP(const std::string& strIP);  
    void DisconnectAll();

    void NbitsToTarget(const std::string &nbits, uint8_t target[32]);
    std::string ByteArrayToHexString(const uint8_t *byteArray, size_t length);    
 
    uint16_t m_uiMaxSessions; 
    uint16_t m_uiPort;
    uint32_t m_uiStatsEpoch;
    uint32_t m_uiStartTime;       
    int32_t m_Socket;   
    int32_t m_PoolSocket; 


    bool m_bActive;  

    struct sockaddr_in servaddr, cliaddr;
        
    // Miners Map 
    std::vector<CClient*> m_vpSessions; 
    std::vector<CClient*>::iterator itClient;
    

    CServer::tStatistics m_tStatistics;
    CServer::tCurrentJob m_tCurrentJob;  

};
#endif
 
