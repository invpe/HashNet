/*
    HashNet X Server
    https://github.com/invpe/HashNet
*/
#include "CClient.h"

CClient::CClient()
{
    m_uiStart = 0;
    m_fProgress = 0;    
    m_uiCompleted=0;
    m_Socket = -1; 
    m_iLastHB = 0;  
    m_strBuffer = "";
    m_strIP = ""; 
    m_strVersion = ""; 
    m_uiStartNonce = 0;
    m_uiEndNonce = 0;
    m_strExtranonce2 = "";
    m_strJobID = ""; 
    m_uiCombinationsPerSecond=0;
    m_uiDifficulty = 0;
}        
void CClient::Disconnect()
{  
    close(m_Socket);
    m_Socket = -1; 
}
void CClient::AddCompleted()
{
    m_uiCompleted++;
}
uint32_t CClient::GetCompleted()
{
    return m_uiCompleted;
}
void CClient::SetVersion(const std::string& strVersion)
{
    m_strVersion = strVersion;
}
void CClient::SetIdent(const std::string& strMAC)
{
    m_strMAC = strMAC;
} 
void CClient::SetSocket(const int32_t& ruiSocket)
{
    m_Socket = ruiSocket;
}
void CClient::SetIP(const std::string& rstrIP)
{
    m_strIP = rstrIP;
}   
// IP 
std::string CClient::GetIP()
{
    return m_strIP;
}
std::string CClient::GetVersion()
{
    return m_strVersion;
}
std::string CClient::GetIdent()
{
    return m_strMAC;
} 
bool CClient::Tick()
{
    if(m_Socket == -1)
        return false;

    // Receiving in non blocking mode - by select'ing the descriptor
    fd_set readfds;
    fd_set errorfds;
    timeval timeout;
    
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    FD_ZERO(&readfds);
    FD_ZERO(&errorfds);
    
    
    FD_SET(m_Socket, &readfds);
    FD_SET(m_Socket, &errorfds);
    
    // Get the max socket number we have (for select()+1)
    int32_t iMaxSocketDescriptor = m_Socket;
    
    // Check if any+1 of set descriptors has something to say
    int32_t iRet = select(iMaxSocketDescriptor + 1, &readfds, NULL, &errorfds, &timeout);
    
    
    // No error, data arrived
    if (iRet > 0)
    {
        
        // This socket is set with Error descriptor, drop session
        if (FD_ISSET(m_Socket, &errorfds))
        {
            //
            Disconnect();

            //
            return false;
        }
        
        // This socket is set with read descriptor, data coming in
        if (FD_ISSET(m_Socket, &readfds))
        {
            
            // Read the incoming data
            int32_t     iBytesRead;
            char        cBuffer[CLIENT_MAX_BUFFER];
            
            //
            iBytesRead = (int)recv(m_Socket, (char *)cBuffer, CLIENT_MAX_BUFFER, 0);
            
            //
            if (iBytesRead <= 0)
            {
                //
                Disconnect();
                
                //
                return false;
            }
            else
            {
                
                // Do not allow for buffer to grow
                if(m_strBuffer.size() > CLIENT_MAX_BUFFER)
                    return false;

                // append data to buffer
                m_strBuffer.append(cBuffer, iBytesRead); 

                //
                std::string::size_type index;
                
                while ((index = m_strBuffer.find("\r\n")) != std::string::npos)
                {
                    // we got a full message, grab it (without the \r\n)
                    std::string msg = m_strBuffer.substr(0, index); 
                    
                    // remove it from the buffer (+2 so we remove the \r\n aswell)
                    m_strBuffer.erase(0, index + 2);
                    
                     //
                    m_vCommands.push_back(msg);
                }
                
                //printf("%i\n",m_strBuffer.size() );
            }
            
        } 
    }
    
    return true;
} 
int CClient::GetHeartBeat()
{
    return m_iLastHB;
}
void CClient::Pong()
{
    m_iLastHB = time(0);
}
void CClient::ClearCommands()
{
    m_vCommands.clear();
} 
int32_t CClient::GetSocket()
{
    return m_Socket;
}
std::vector<std::string> CClient::GetCommands()
{
    return m_vCommands;
} 
void CClient::SetStartNonce(const uint32_t& ruiChunk)
{
    m_uiStartNonce = ruiChunk;
}
void CClient::SetEndNonce(const uint32_t& ruiChunk)
{
    m_uiEndNonce = ruiChunk;
}
void CClient::SetExtranonce2(const std::string& rstrNonce)
{
    m_strExtranonce2 = rstrNonce;
}
uint32_t CClient::GetStartNonce()
{
    return m_uiStartNonce;
}
std::string CClient::GetExtranonce2()
{
    return m_strExtranonce2;
}
uint32_t CClient::GetEndNonce()
{
    return m_uiEndNonce;
}
void CClient::SetCombinationsPerSecond(const uint32_t& ruiCombinations)
{
    m_uiCombinationsPerSecond = ruiCombinations;
}
uint32_t CClient::GetCombinationsPerSecond()
{
    return m_uiCombinationsPerSecond;
}
void CClient::SetJobID(const std::string& rstrJobId)
{
    m_strJobID = rstrJobId;
}
std::string CClient::GetJobID()
{
    return m_strJobID;
}
void CClient::SetProgress(const float& fValue)
{
    m_fProgress = fValue;
}
float CClient::GetProgress()
{
    return m_fProgress;
}
void CClient::Start()
{
    m_uiStart = time(0);
}
uint32_t CClient::GetStart()
{
    return m_uiStart;
}
uint32_t CClient::GetDifficulty()
{
    return m_uiDifficulty;
}
void CClient::IncreaseDifficulty()
{
    m_uiDifficulty += 100000;
}
CClient::~CClient()
{
    m_vCommands.clear();
}
