/*
    HashNet X Server
    https://github.com/invpe/HashNet
*/
#include "CServer.h"
 
CServer::CServer()
{    
    m_bActive = true;   
    m_tCurrentJob.Clear();
    m_tStatistics.Clear();
    
}
bool CServer::Start(const int16_t& ruiPort, const uint16_t& ruiMaxSessions)
{  
        
    SaveLog(SERVER_LOG_INFO,"Version: "SERVER_VERSION);
    SaveLog(SERVER_LOG_INFO,"Compilation: "__TIME__" "__DATE__);
    SaveLog(SERVER_LOG_INFO,"Iterations per miner: "+std::to_string(ITERATIONS_PER_MINER));
    SaveLog(SERVER_LOG_INFO,"Starting CORE Server (TCP)");

    
    //
    if ((m_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        return false;
     
    
    struct sockaddr_in sin;
    
    int32_t  opt = 1;
    sin.sin_port = htons(ruiPort);
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_family = AF_INET;
    
    
    if (setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        return false;
    
    if (bind(m_Socket, (struct sockaddr*) &sin, sizeof(sin)) == -1)
        return false;
    
    if (listen(m_Socket, ruiMaxSessions) == -1)
        return false;
    
    m_uiPort           = ruiPort;
    m_uiMaxSessions    = ruiMaxSessions; 

    //
    SaveLog(SERVER_LOG_INFO,"Starting Pool Connection");
    

    SaveLog(SERVER_LOG_INFO,"Rolling"); 
    m_uiStartTime = time(0);  

    return true;
} 
void CServer::Tick()
{

    // Check for Session Timeout
    for (itClient = m_vpSessions.begin();  itClient!=m_vpSessions.end(); )  
    {
        CClient *pClient = *itClient;
        
        if(time(0) - pClient->GetHeartBeat() >= SESSION_TIMEOUT )
        {
            DisconnectMiner(pClient);
            itClient = m_vpSessions.erase(itClient);
            delete pClient;      
        }  
        else
        {
          ++itClient;
        }
    }

    // Check if up and running
    for (itClient = m_vpSessions.begin();  itClient!=m_vpSessions.end(); )  
    {
        CClient *pClient = *itClient;
        
        if(pClient->Tick() == false)
        {
            SaveLog(SERVER_LOG_INFO,pClient->GetIP()+" Disconnected");
            DisconnectMiner(pClient);
            itClient = m_vpSessions.erase(itClient);
            delete pClient;
        }
        else
        {  
            ++itClient;
        } 
    }

 
    fd_set readfds;
    timeval timeout;    
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;


    // POOL
    if(POOL_Connected())
    { 
        // POOL
        FD_ZERO(&readfds);
        FD_SET(m_PoolSocket, &readfds);
        int32_t iRet = select(m_PoolSocket + 1, &readfds, NULL, NULL, &timeout);
        
        if(iRet>0)
        {
            if (FD_ISSET(m_PoolSocket, &readfds))
            {
                std::string response = POOL_Receive();
                SaveLog(SERVER_LOG_INFO,"Pool received:\n"+response+"\n");

                if (response.find("mining.notify") != std::string::npos)
                { 
                  std::string err;
                  json11::Json jsondata = json11::Json::parse(response, err); 


                    if (err.empty()) {
                        // Ensure the JSON contains the required data
                        if (jsondata.is_object() && jsondata["params"].is_array()) {
                            const auto& params = jsondata["params"].array_items();

                            // We update current_work only when clean job is received
                            // As the pool will send notifications without clean_job changing parameters here and there
                            // We need to ensure our miners dig on a single current_job, not on everchanging updates of mining notify
                         
                            if (params.size() > 8) {
                                bool clean_jobs = params[8].bool_value();
                                m_tCurrentJob.m_mMapping["clean_jobs"] = clean_jobs ? "true" : "false";
                            }

                            // Update current_job only when new jobs come in.
                            if(m_tCurrentJob.m_mMapping["clean_jobs"]=="true")
                            {
                              
                                // Clear the best extranonce2's for the job - as we've got new job coming in
                                // Rest is already remapped
                                m_tCurrentJob.m_mBestExtranonce2.clear();
                                m_tCurrentJob.m_mExtranonce2LastEndNonce.clear();                                    
                                                                
                                // And statistics
                                m_tStatistics.m_uiCombinationsPerSecond=0;
                                m_tStatistics.m_uiCompletedChunksForThisJob = 0;
                                m_tStatistics.m_uiTotalJobs++;

                                // Define the job keys and extract the relevant parameters
                                std::vector<std::string> job_keys = {"job_id", "prevhash", "coinb1", "coinb2", "merkle_branch", "version", "nbits", "ntime"};

                                for (size_t i = 0; i < job_keys.size() && i < params.size(); ++i) {
                                    if (job_keys[i] != "merkle_branch") {
                                            m_tCurrentJob.m_mMapping[job_keys[i]] = params[i].string_value();
                                    }
                                } 

                                // Extract the merkle_branch array (5th element in params)
                                if (params.size() > 4 && params[4].is_array()) {
                                    const auto& merkle_branch_array = params[4].array_items();
                                    m_tCurrentJob.m_vMerkleBranch.clear();

                                    for (const auto& item : merkle_branch_array) {
                                        m_tCurrentJob.m_vMerkleBranch.push_back(item.string_value());
                                    }

                                } else {
                                    SaveLog(SERVER_LOG_ERROR, "Merkle not found");
                                    exit(0);
                                } 
                                
                                //

                                SaveLog(SERVER_LOG_INFO, "Clean jobs flag detected. Resetting nonce chunks, best_extranonce2.");

                                // Don't drop them, simply don't care
                                // DisconnectAll();

                                uint8_t target[32];
                                NbitsToBETarget(m_tCurrentJob.m_mMapping["nbits"],target);

                                m_tCurrentJob.m_mMapping["targetdistance"] = std::to_string(TargetDistance(target,32));
                                m_tCurrentJob.m_mMapping["target"] = ByteArrayToHexString(target, 32);
                                m_tCurrentJob.m_mMapping["ready"] = "true";           
                            }
                        }
                    }
                }

            }//FD_ISSET
        }//ret>0
    }
    else
    {
       // Clear whatever job we were working on
       m_tCurrentJob.Clear();

       // Connect and obtain initial variables
       bool bConnected =  POOL_Connect();
       SaveLog(SERVER_LOG_INFO,"Pool reconnection: "+std::to_string(bConnected));
       if(bConnected)
       {

            // Nothing can fail here, we exit if any failure as this is critical.

            // 1
            SaveLog(SERVER_LOG_INFO,"Pool subscribe");
            POOL_Send("{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": []}\n");

            std::string response = POOL_Receive();
            SaveLog(SERVER_LOG_INFO,"Pool received:\n"+response+"\n");

              // 2. Parse only the "result" JSON from the concatenated response
            std::string err;
            size_t result_pos = response.find("\"result\":");
            if (result_pos == std::string::npos) {SaveLog(SERVER_LOG_ERROR, "Can't find results json object");exit(0);}

            // Find the end of this JSON object (it ends at the first '}' after "result")
            size_t end_pos = response.find("}", result_pos);
            if (end_pos == std::string::npos) {SaveLog(SERVER_LOG_ERROR, "Can't find end of results json object");exit(0);}


            // Extract the valid JSON substring
            std::string valid_json = response.substr(0, end_pos + 1);
            json11::Json jsondata = json11::Json::parse(valid_json, err);

            // Check for parsing errors
            if (!err.empty()) {
                SaveLog(SERVER_LOG_ERROR, "Error parsing JSON: " + err);
                exit(0);;
            }

            // 3. Check for "result" and extract extranonce1 and extranonce2_size
            if (jsondata.is_object() && jsondata["result"].is_array()) {
                const auto& result = jsondata["result"].array_items();

                // Ensure that the result has the necessary elements
                if (result.size() >= 3) 
                {
                    m_tCurrentJob.m_mMapping["extranonce1"] = result[1].string_value();
                    m_tCurrentJob.m_mMapping["extranonce2_size"] = std::to_string(result[2].int_value()); 


                } else {
                    SaveLog(SERVER_LOG_ERROR, "Invalid result format in response");
                    exit(0);
                }
            } else {
                SaveLog(SERVER_LOG_ERROR, "Result key missing or not an array");
                exit(0);
            }
                         

            // 4
            SaveLog(SERVER_LOG_INFO,"Pool authorization"); 
            POOL_Send("{\"params\": [\"" BTC_WALLET "\", \"password\"], \"id\": 2, \"method\": \"mining.authorize\"}\n");
       }
    }
    

    // MINERS connections
    FD_ZERO(&readfds);
    FD_SET(m_Socket, &readfds);
    int32_t iRet = select(m_Socket + 1, &readfds, NULL, NULL, &timeout);
    
    // We're not accepting new connections in maintenance mode
    if(m_bActive == true)
    { 
        if (iRet > 0)
        {
            typedef uint32_t socklen_t;
            socklen_t        addrlen;
            sockaddr_in     address;
            
            addrlen = sizeof(struct sockaddr_in);
                
            int32_t new_socket = accept(m_Socket, (struct sockaddr *)&address, &addrlen);
    
            //  
            std::string strIP = std::string(inet_ntoa(address.sin_addr));

            //
            if(IsThrottled(strIP)) 
            {
                SaveLog(SERVER_LOG_WARRNING,"Throttled IP coming "+strIP);
                DisconnectIP(strIP); 
            }

            // Nothing new
            if (new_socket == -1)
            {
                
            }
            else
            {       
                  
                // Create new connection socket if not FULL
                if(m_vpSessions.size() >= m_uiMaxSessions)
                { 
                    SaveLog(SERVER_LOG_WARRNING,"Server Full: "+std::to_string(m_vpSessions.size())+"/"+std::to_string(m_uiMaxSessions));
                    close(new_socket);
                }
                else
                {
                    CClient *pNewClient = new CClient();
                    pNewClient->SetProgress(0);
                    pNewClient->SetSocket(new_socket);
                    pNewClient->SetIP(std::string(inet_ntoa(address.sin_addr)));
                    pNewClient->Pong();
                    m_vpSessions.push_back(pNewClient);                    
                }   
            }
      
        }
    }


    //  Handle miners logic
    for(size_t x = 0; x < m_vpSessions.size(); x++)
    {
        CClient *pClient = m_vpSessions[x];

        // Commands
        std::vector<std::string> vCommands = pClient->GetCommands();
       
        for (const auto& command : vCommands) 
        {
            std::string err;
            json11::Json jsondata = json11::Json::parse(command, err);

            if (!err.empty()) {
                SaveLog(SERVER_LOG_ERROR, "Error parsing JSON command: " + err);
                ThrottleIP(pClient->GetIP());
            }

            std::string method = jsondata["method"].string_value();

            if (method == "found") { 

                // Get details from the miner
                std::string job_id      = jsondata["job_id"].string_value();
                std::string extranonce2 = jsondata["extranonce2"].string_value();
                uint32_t nonce          = std::stoul(jsondata["nonce"].string_value());

                // Log the found block info
                SaveLog(SERVER_LOG_WARRNING, "Block found by client " + pClient->GetIdent() + ": job_id = " + job_id + ", extranonce2 = " + extranonce2 + ", nonce = " + std::to_string(nonce));

                // Build the JSON message for nonce submission using current_work
                std::stringstream ss;
                ss << "{"
                   << "\"params\": ["
                   << "\"" << BTC_WALLET << "\","
                   << "\"" << jsondata["job_id"].string_value() << "\","
                   << "\"" << jsondata["extranonce1"].string_value() << "\"," 
                   << "\"" << jsondata["extranonce2"].string_value() << "\","
                   << "\"" << jsondata["ntime"].string_value() << "\"," 
                   << "\"" << jsondata["nonce"].string_value() << "\"],"
                   << "\"id\": 3,"
                   << "\"method\": \"mining.submit\""
                   << "}";

                std::string nonce_data = ss.str();
 
                SaveLog(SERVER_LOG_WARRNING, "Submitting found nonce to pool: " + nonce_data);

                POOL_Send(nonce_data);

                std::string pool_response = POOL_Receive();

                SaveLog(SERVER_LOG_WARRNING, "Response: "+ pool_response);
                m_tStatistics.m_uiTotalBlocksFound++;

            } else if (method == "stats") {
                uint32_t combinations_per_sec = jsondata["combinations_per_sec"].int_value();
                int distance = jsondata["distance"].int_value();
                std::string ident = jsondata["ident"].string_value();
                std::string version = jsondata["version"].string_value();
                std::string besthash = jsondata["besthash"].string_value();
                uint32_t uiCurrentNonce = jsondata["current_nonce"].int_value();

                if(uiCurrentNonce>0)
                { 
                    float totalNonces = (float)(pClient->GetEndNonce() - pClient->GetStartNonce());
                    float fPercentage = (uiCurrentNonce / totalNonces) * 100.0f;
                    pClient->SetProgress(fPercentage);
                }
                else 
                    pClient->SetProgress(0);

                m_tCurrentJob.m_mBestExtranonce2[pClient->GetExtranonce2()] = distance;                    
                m_tStatistics.m_mBestDistancesEpoch[distance] = time(0);

                pClient->SetIdent(ident);
                pClient->SetVersion(version);
                pClient->SetCombinationsPerSecond(combinations_per_sec);

            } else if (method == "completed") {
                std::string job_id = jsondata["job_id"].string_value();
                uint32_t nonce_start = jsondata["nonce_start"].int_value();
                uint32_t nonce_end = jsondata["nonce_end"].int_value();

                // Difficulty hop check
                uint32_t uiTimeTaken = time(0) - pClient->GetStart();

                if(uiTimeTaken < DIFFICULTY_INCREASE_THRESHOLD)
                    pClient->IncreaseDifficulty();

                SaveLog(SERVER_LOG_INFO, "Job: "+m_tCurrentJob.m_mMapping["job_id"]+" "+pClient->GetIdent()+" DONE "+pClient->GetExtranonce2()+" <"+std::to_string(pClient->GetStartNonce())+","+std::to_string(pClient->GetEndNonce())+"> in "+std::to_string(uiTimeTaken));

                // Free
                pClient->SetExtranonce2("");
                pClient->AddCompleted();                
                m_tStatistics.m_uiCompletedChunksForThisJob++;
                m_tStatistics.m_uiCompletedChunksTotal++;

            } else {
                SaveLog(SERVER_LOG_ERROR, "Unknown method received from client: " + method);                
                ThrottleIP(pClient->GetIP());
            }
            pClient->Pong();
        }

        pClient->ClearCommands();

        // See if ready for distribution
        if(pClient->GetExtranonce2().empty() && m_tCurrentJob.m_mMapping["ready"] == "true")
        {
            // Obtain work chunk from current task and assign to the miner
            std::string strExtranonce2 = m_tCurrentJob.GetNextExtranonce2();  
            std::pair<uint32_t, uint32_t> ppWorkChunk = m_tCurrentJob.GetNextWorkChunk(strExtranonce2, pClient->GetDifficulty());
            pClient->SetStartNonce(ppWorkChunk.first);
            pClient->SetEndNonce(ppWorkChunk.second);
            pClient->SetExtranonce2(strExtranonce2);
            pClient->SetJobID(m_tCurrentJob.m_mMapping["job_id"]);            
            pClient->SetProgress(0);
            pClient->Start();

            //  
            std::stringstream ss;
            ss << "{"
               << "\"method\":\"work\","
               << "\"nonce_start\":" << pClient->GetStartNonce() << ","
               << "\"nonce_end\":" << pClient->GetEndNonce() << ",";

            //  
            ss << "\"merkle_branch\":[";
            for (size_t i = 0; i < m_tCurrentJob.m_vMerkleBranch.size(); ++i) {
                ss << "\"" << m_tCurrentJob.m_vMerkleBranch[i] << "\"";
                if (i < m_tCurrentJob.m_vMerkleBranch.size() - 1) {
                    ss << ",";  // Add a comma between elements
                }
            }
            ss << "],";  

            //  
            ss << "\"extranonce2\":\"" << pClient->GetExtranonce2() << "\","

               << "\"sversion\":\"" << SERVER_VERSION << "\","
               << "\"extranonce1\":\"" << m_tCurrentJob.m_mMapping["extranonce1"] << "\","
               << "\"job_id\":\"" << m_tCurrentJob.m_mMapping["job_id"] << "\","
               << "\"prevhash\":\"" << m_tCurrentJob.m_mMapping["prevhash"] << "\","
               << "\"coinb1\":\"" << m_tCurrentJob.m_mMapping["coinb1"] << "\","
               << "\"coinb2\":\"" << m_tCurrentJob.m_mMapping["coinb2"] << "\","
               << "\"version\":\"" << m_tCurrentJob.m_mMapping["version"] << "\","
               << "\"nbits\":\"" << m_tCurrentJob.m_mMapping["nbits"] << "\","
               << "\"ntime\":\"" << m_tCurrentJob.m_mMapping["ntime"] << "\""
               << "}\r\n";


            //  
            std::string work_message_str = ss.str();

            //  
            SendTo(pClient, work_message_str);
            SaveLog(SERVER_LOG_INFO, "Job: "+m_tCurrentJob.m_mMapping["job_id"]+" "+pClient->GetIdent()+" SENT "+pClient->GetExtranonce2()+" <"+std::to_string(pClient->GetStartNonce())+","+std::to_string(pClient->GetEndNonce())+"> Diff "+std::to_string(pClient->GetDifficulty()));
        }
        
    }
    
    if(time(0) - m_uiStatsEpoch>=1)
    {   

        m_tStatistics.m_uiCombinationsPerSecond = 0;

        for(size_t x = 0; x < m_vpSessions.size(); x++)
            m_tStatistics.m_uiCombinationsPerSecond +=m_vpSessions[x]->GetCombinationsPerSecond();        

        SaveLog(SERVER_LOG_INFO, "Job: "+m_tCurrentJob.m_mMapping["job_id"]+" Miners: "+std::to_string(m_vpSessions.size())+" C/S: "+std::to_string(m_tStatistics.m_uiCombinationsPerSecond)+" Top: "+std::to_string(m_tCurrentJob.m_mBestExtranonce2.size())+" Dst: "+std::to_string(m_tCurrentJob.GetBestDistance())+" Thr: "+std::to_string(m_mThrottleIP.size()));


        // Adds entry to the statistics time graph
        CServer::tTimeEntry _Entry;
        _Entry.m_uiEpoch = time(0);
        _Entry.m_uiTotalHash = m_tStatistics.m_uiCombinationsPerSecond;
        _Entry.m_uiTotalMiners = m_vpSessions.size();
        _Entry.m_uiBestDistance = m_tCurrentJob.GetBestDistance();
        m_tStatistics.AddTimeEntry(_Entry);

        // Dump stats.json 
        // MINING
        std::string strJSON = "{\n";
        strJSON += "\"Mining\": [{\n";
        strJSON += "\"Time\": "+std::to_string(time(0))+",\n";
        strJSON += "\"Throttled\": "+std::to_string(m_mThrottleIP.size())+",\n";
        strJSON += "\"JobID\": \""+m_tCurrentJob.m_mMapping["job_id"]+"\",\n"; 


        if(m_tCurrentJob.m_mMapping["targetdistance"].empty())
        {
            strJSON += "\"TargetDistance\": 9999,\n";
        }else
            strJSON += "\"TargetDistance\": "+m_tCurrentJob.m_mMapping["targetdistance"]+",\n";

        strJSON += "\"BestDistance\": "+std::to_string(m_tCurrentJob.GetBestDistance())+",\n";
        strJSON += "\"TotalChunks\": "+std::to_string(m_tStatistics.m_uiCompletedChunksTotal)+",\n";
        strJSON += "\"JobChunks\": "+std::to_string(m_tStatistics.m_uiCompletedChunksForThisJob)+",\n";
        strJSON += "\"BlocksFound\": "+std::to_string(m_tStatistics.m_uiTotalBlocksFound)+",\n";
        strJSON += "\"TotalJobs\": "+std::to_string(m_tStatistics.m_uiTotalJobs)+",\n";
        strJSON += "\"Target\": \""+m_tCurrentJob.m_mMapping["target"]+"\",\n"; 
        strJSON += "\"HashRate\": "+std::to_string(m_tStatistics.m_uiCombinationsPerSecond)+"\n";
 
        strJSON += "}],\n";


        // Add TimeStats
        strJSON += "\"TimeStats\": [\n";

        if (!m_tStatistics.m_vTimeStats.empty()) {
            for (auto it = m_tStatistics.m_vTimeStats.begin(); it != m_tStatistics.m_vTimeStats.end(); ++it) {
                strJSON += "{\n";
                strJSON += "\"Epoch\": "+std::to_string(it->m_uiEpoch)+",\n";
                strJSON += "\"TotalMiners\": "+std::to_string(it->m_uiTotalMiners)+",\n";
                strJSON += "\"TotalHash\": "+std::to_string(it->m_uiTotalHash)+",\n";
                strJSON += "\"BestDistance\": "+std::to_string(it->m_uiBestDistance)+"\n";
                
                if (std::next(it) != m_tStatistics.m_vTimeStats.end()) {
                    strJSON += "},\n";
                } else {
                    strJSON += "}\n";  // Last entry, no comma
                }
            }
        }

        strJSON += "],\n";  // End of TimeStats array
        
        // Best Distances history 
        strJSON += "\"BestDistances\": [\n";
 
        if (!m_tStatistics.m_mBestDistancesEpoch.empty()) {
 
            for (auto it = m_tStatistics.m_mBestDistancesEpoch.begin(); it != m_tStatistics.m_mBestDistancesEpoch.end(); ++it) {
                int epoch = it->second;
                uint32_t distance = it->first;
 
                strJSON += "{\n";
                strJSON += "\"Epoch\": " + std::to_string(epoch) + ",\n";
                strJSON += "\"Distance\": " + std::to_string(distance) + "\n";
 
                if (std::next(it) != m_tStatistics.m_mBestDistancesEpoch.end()) {
                    strJSON += "},\n";
                } else {
                    strJSON += "}\n";
                }
            }
        }

        strJSON += "],\n";

        // NODES
        strJSON += "\"Nodes\": [\n";
 
        if (!m_vpSessions.empty()) {
 
            for (auto it = m_vpSessions.begin(); it != m_vpSessions.end(); ++it) {
                CClient* pClient = *it;

                strJSON += "{\n";
                strJSON += "\"ident\": \""+pClient->GetIdent()+"\",\n";

                // Mask the IP                 
                CSHA1 m_SHA1;  
                m_SHA1.update(pClient->GetIP());    
                strJSON += "\"address\": \""+m_SHA1.final()+"\",\n";
                strJSON += "\"combinations_per_sec\": "+std::to_string(pClient->GetCombinationsPerSecond())+",\n";
                strJSON += "\"difficulty\": \""+std::to_string(pClient->GetDifficulty())+"\",\n";
                strJSON += "\"jobid\": \""+pClient->GetJobID()+"\",\n";
                strJSON += "\"start_nonce\": "+std::to_string(pClient->GetStartNonce())+",\n";
                strJSON += "\"end_nonce\": "+std::to_string(pClient->GetEndNonce())+",\n";
                strJSON += "\"extranonce2\": \""+pClient->GetExtranonce2()+"\",\n";
                strJSON += "\"completed\": "+std::to_string(pClient->GetCompleted())+",\n"; 
                strJSON += "\"progress\": "+std::to_string(pClient->GetProgress())+"\n"; 
                 
                if (std::next(it) != m_vpSessions.end()) {
                    strJSON += "},\n";
                } else {
                    strJSON += "}\n";
                }
            }
        }
        strJSON += "]\n";
        strJSON += "}\n";
 
        std::ofstream outFile("stats.json");
  
        if (outFile.is_open()) { 
            outFile << strJSON; 
            outFile.close(); 
        } else {
            SaveLog(SERVER_LOG_ERROR, "Cant store stats.json");
        }
       
        m_uiStatsEpoch = time(0);
        
    }

    usleep(10000);
}   
bool CServer::SendTo(CClient* pClient, const std::string& strData )
{
    assert(pClient);  
    std::string strDataNew = strData; 

    int32_t iBytesSent = 0;
    int iLength        = strDataNew.size();
    const char *p    = reinterpret_cast<const char*>(strDataNew.data());
    
  
    // NO_SIGNAL added, to prevent raising SIGPIPE when writing to disconnected socket
    // http://stackoverflow.com/questions/108183/how-to-prevent-sigpipes-or-handle-them-properly
    iBytesSent = send(pClient->GetSocket(), p, iLength, 0);
  
    //
    if (iBytesSent <= 0)
    { 
        return false;
    }
         
    return true;
}  
void CServer::DisconnectIP(const std::string& strIP)
{
    for(int a = 0; a < m_vpSessions.size(); a++)
    {
        if(m_vpSessions[a]->GetIP() == strIP)
        {
            DisconnectMiner(m_vpSessions[a]);
        }
    }
}  
void CServer::SaveLog(const int& iType, const std::string& strText)
{
    std::string strLogType = "";

     
    switch(iType)
    {
        case SERVER_LOG_INFO:
        {
            strLogType = "[INF]";
        }
        break;

        case SERVER_LOG_WARRNING:
        {
            strLogType = "[WARN]";
        }
        break;


        case SERVER_LOG_ERROR:
        {
            strLogType = "[ERR]";
        }
        break;

        default:
        {
            strLogType = "[???]";
        }
    }

    std::string strTime = CSystemUtil::GetTimeDate();
    std::string strOutput = strTime+" "+strLogType+" "+strText+"\n";
    std::string strFilename = std::to_string(CSystemUtil::GetYear())+"_"+std::to_string(CSystemUtil::GetMonth())+"_"+std::to_string(CSystemUtil::GetDay())+".txt";
    
   
    FILE *fp = fopen(strFilename.c_str(),"a");
    if(fp)
    {
        fprintf(fp,"%s", strOutput.data());
        fclose(fp);
        
    }  
    
    printf("%s", strOutput.data());
}
void CServer::DisconnectMiner(CClient *pMiner)
{
    assert(pMiner);  

    // Disconnect the socket
    pMiner->Disconnect();
}  
void CServer::Stop()
{
 
    SaveLog(SERVER_LOG_WARRNING,"Server going shutdown");
 
     m_bActive = false;
 
    for(int b = 0 ; b < m_vpSessions.size(); b++)
        DisconnectMiner(m_vpSessions[b]); 

    exit(0);
}
 
CClient *CServer::GetClientByIdent(const std::string& strMAC)
{
    for(int a = 0; a < m_vpSessions.size(); a++)
    {
        if(m_vpSessions[a]->GetIdent() == strMAC)
            return m_vpSessions[a];
    }

    return NULL;
}
bool CServer::POOL_Connect()
{
    const char* host = "solo.ckpool.org";
    hostent *he;
    if ((he = gethostbyname(host)) == 0) { 
        return false;
    }
    m_PoolSocket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(3333);
    server_addr.sin_addr = *((in_addr *)he->h_addr);

    if (connect(m_PoolSocket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {        
        return false;
    }

    return true;
}     
void CServer::POOL_Disconnect()
{
    close(m_PoolSocket);
    m_PoolSocket = -1;
}
bool CServer::POOL_Connected()
{
    if (m_PoolSocket < 0) {
        // Invalid socket
        return false;
    }

    // Check if the socket is still connected
    char buffer;
    int error = 0;
    socklen_t len = sizeof(error);
    
    // Use getsockopt to check for errors on the socket
    int retval = getsockopt(m_PoolSocket, SOL_SOCKET, SO_ERROR, &error, &len);
    
    if (retval != 0 || error != 0) {
        // Either getsockopt failed or there is an error on the socket
        return false;
    }

    // The socket is valid and no errors occurred
    return true;
}
bool CServer::POOL_Send(const std::string& rstrData)
{
    send(m_PoolSocket, rstrData.c_str(), rstrData.size(), 0);
}

std::string CServer::POOL_Receive() {
    char buffer[5024] = {0};
    size_t bytes_read = recv(m_PoolSocket, buffer, sizeof(buffer), 0);
    return std::string(buffer, bytes_read);
}
void CServer::DisconnectAll()
{
    for(int a = 0; a < m_vpSessions.size(); a++)
    {
        DisconnectMiner(m_vpSessions[a]);
    }
}  
std::string CServer::ByteArrayToHexString(const uint8_t *byteArray, size_t length)
{
     std::ostringstream oss;
      for (size_t i = 0; i < length; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byteArray[i]);
      }
      return oss.str();
}
void CServer::NbitsToBETarget(const std::string &nbits, uint8_t target[32]) {

    static const uint32_t EXPONENT_SHIFT = 24;
    static const uint32_t MANTISSA_MASK = 0xffffff;
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
int CServer::TargetDistance(const uint8_t *target, size_t target_size)
{
    int required_zero_bytes = 0;

    // Iterate from the end of block_hash towards the beginning
      for (size_t i = 0; i < target_size; i++) {
        if (target[i] == 0x00) {
          required_zero_bytes++;
        } else {
          break;  // Stop counting once a non-zero byte is encountered
        }
      }

    return required_zero_bytes;
}
bool CServer::IsThrottled(const std::string& rstrIP)
{
    std::map<std::string, uint32_t>::iterator it = m_mThrottleIP.find(rstrIP);

    // Found
    if (it != m_mThrottleIP.end())
        return true;
    
    return false;
}
void CServer::ThrottleIP(const std::string& rstrIP)
{
    // Local tests dont get throttled
    if(rstrIP != "127.0.0.1")
        m_mThrottleIP[rstrIP] = time(0);    

    SaveLog(SERVER_LOG_WARRNING,"Throttling enabled for "+rstrIP);
    DisconnectIP(rstrIP);
}
CServer::~CServer()
{  
}
