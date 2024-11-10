/*
    HashNet X Server
    https://github.com/invpe/HashNet
*/
#ifndef __CCLIENT__
#define __CCLIENT__

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sched.h>
#include <sys/time.h>

#include "CSystemUtil.h";

#define CLIENT_MAX_BUFFER 2048 
class CClient
{
public: 

    CClient();
    ~CClient();
    

    void Disconnect();
    void SetSocket(const int32_t& ruiSocket);
    void SetIP(const std::string& rstrIP);   
    void SetVersion(const std::string& strVersion); 
    void SetIdent(const std::string& strMAC);       
    void SetStartNonce(const uint32_t& ruiChunk);
    void SetEndNonce(const uint32_t& ruiChunk);
    void SetExtranonce2(const std::string& rstrNonce);
    void SetJobID(const std::string& rstrJobId);
    void SetCombinationsPerSecond(const uint32_t& ruiCombinations);
    void SetProgress(const float& fValue);

 
    
    float GetProgress();
    uint32_t GetCombinationsPerSecond();
    uint32_t GetStartNonce();
    uint32_t GetEndNonce();
    uint32_t GetCompleted(); 
    uint32_t GetDifficulty();
    uint32_t GetStart();
    std::string GetExtranonce2();
    std::string GetJobID();
    int GetSocket();   
    int GetHeartBeat();  
    std::vector<std::string> GetCommands();  
    std::string GetIP(); 
    std::string GetVersion(); 
    std::string GetIdent();   

    void Start();
    void AddCompleted();
    void Pong();
    void ClearCommands();
    bool Tick();  
    void End();
    void IncreaseDifficulty();
private:       
    int m_Socket;     
    int m_iLastHB;
    std::string m_strVersion; 
    std::string m_strMAC;
    std::string m_strIP;  
    std::string m_strBuffer; 
    std::vector<std::string> m_vCommands;  

    // Work
    uint32_t m_uiDifficulty;
    uint32_t m_uiCombinationsPerSecond;
    uint32_t m_uiStartNonce;
    uint32_t m_uiEndNonce; 
    uint32_t m_uiCompleted; 
    uint32_t m_uiStart;
    float m_fProgress;
    std::string m_strExtranonce2;
    std::string m_strJobID;
};
#endif
