
#include "ClientNetInterface.h"
#include "Timer.h"
#include "TuiFileUtils.h"
#include "sodium.h"

ClientNetInterface::ClientNetInterface(std::string host_,
                                       std::string port_,
                                       const std::string& savedPermanentPublicKey_,
                                       const std::string& savedPermanentSecretKey_,
                                       TuiTable* initialData_)
{
    host = host_;
    port = port_;
    savedPermanentPublicKey = savedPermanentPublicKey_;
    savedPermanentSecretKey = savedPermanentSecretKey_;
    initialData = initialData_;
    if(initialData)
    {
        initialData->retain();
    }
    
    inputQueue = new ThreadSafeQueue<ClientNetInterfaceInput>();
    outputQueue = new ThreadSafeQueue<ClientNetInterfaceOutput>();
    
    //todo this key pair is not used on the host, doesn't need to be generated there
    sessionTransientPublicKey.resize(crypto_box_PUBLICKEYBYTES);
    sessionTransientSecretKey.resize(crypto_box_SECRETKEYBYTES);
    crypto_box_keypair((unsigned char*)&(sessionTransientPublicKey[0]), (unsigned char*)&(sessionTransientSecretKey[0]));
    
    enet_initialize();
    connect();
}


ClientNetInterface::~ClientNetInterface()
{
    disconnect();
    enet_deinitialize();
    delete inputQueue;
    delete outputQueue;
    stateTable->release();
    if(initialData)
    {
        initialData->release();
    }
}


void ClientNetInterface::connect()
{
    if(thread)
    {
        return;
    }
    
    disconnected = false;
    needsToExit = false;
    
    enetClient = enet_host_create (nullptr, // create a client host
                                   1,
                                   0, //channels
                                   0,
                                   0);
    
    enet_host_compress_with_range_coder(enetClient);
    
    ENetAddress address;
    
    enet_address_set_host (&address, host.c_str());
    address.port = atoi(port.c_str());
    
    enetPeer = enet_host_connect (enetClient, &address, ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT, 0);
    
    enet_peer_timeout(enetPeer, 0, 5000, 10000);
    
    thread = new std::thread(&ClientNetInterface::startThread, this);
}

void ClientNetInterface::disconnect()
{
    if(!thread)
    {
        return;
    }
    
    if(connected)
    {
        MJLog("Disconnected from tracker.");
    }
    else if(!disconnected)
    {
        MJLog("Couldn't connect to tracker. Trying again in %d seconds", (int)timeBetweenReconnects);
    }
    
    needsToExit = true;
    connected = false;
    disconnected = true;
    thread->join();
    delete thread;
    thread = nullptr;
    
    if(!callbacksByID.empty())
    {
        TuiRef* statusResult = new TuiTable("{status='error',message='not connected'}");
        for(auto& idAndCallback : callbacksByID)
        {
            idAndCallback.second->call("SERVER_FUNCTION_CALL_RESPONSE", statusResult);
        }
        statusResult->release();
        callbacksByID.clear();
    }
    
    if(enetPeer)
    {
        enet_peer_disconnect(enetPeer, 0);
        ENetEvent event;
        bool success = false;
        while (!success && enet_host_service (enetClient, &event, 3000) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_RECEIVE:
                    enet_packet_destroy (event.packet);
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    success = true;
                    break;
                default:
                    break;
            }
        }
        if(!success)
        {
            enet_peer_reset(enetPeer);
        }
        enetPeer = nullptr;
    }
    
    if(enetClient)
    {
        enet_host_destroy(enetClient);
        enetClient = nullptr;
    }
    
}

TuiTable* ClientNetInterface::getTrackerEncryptedDataTable(TuiTable* dataToSecureTable)
{
    std::string nonce;
    nonce.resize(crypto_box_NONCEBYTES);
    randombytes_buf(&nonce[0], crypto_box_NONCEBYTES);
    
    
    std::string dataToSecureSerialized = dataToSecureTable->serializeBinary();
    std::string cipherText;
    cipherText.resize(crypto_box_MACBYTES + dataToSecureSerialized.length());
    
    if (crypto_box_easy((unsigned char*)&(cipherText[0]),
                        (unsigned char*)&(dataToSecureSerialized[0]),
                        dataToSecureSerialized.length(), (unsigned char*)nonce.c_str(),
                        (unsigned char*)trackerPublicKey.c_str(), (unsigned char*)savedPermanentSecretKey.c_str()) != 0)
    {
        return nullptr;
    }
    
    TuiTable* sendTable = new TuiTable(nullptr);
    
    sendTable->setString("nonce", nonce);
    sendTable->setString("publicKey", savedPermanentPublicKey);
    sendTable->setString("data", cipherText);
    
    return sendTable;
}

TuiTable* ClientNetInterface::getHostOrClientEncryptedDataTable(std::string hostOrClientPublicKey, TuiTable* dataToSecureTable)
{
    std::string nonce;
    nonce.resize(crypto_box_NONCEBYTES);
    randombytes_buf(&nonce[0], crypto_box_NONCEBYTES);
    
    
    std::string dataToSecureSerialized = dataToSecureTable->serializeBinary();
    std::string cipherText;
    cipherText.resize(crypto_box_MACBYTES + dataToSecureSerialized.length());
    
    if (crypto_box_easy((unsigned char*)&(cipherText[0]),
                        (unsigned char*)&(dataToSecureSerialized[0]),
                        dataToSecureSerialized.length(), (unsigned char*)nonce.c_str(),
                        (unsigned char*)hostOrClientPublicKey.c_str(), (unsigned char*)sessionTransientSecretKey.c_str()) != 0)
    {
        return nullptr;
    }
    
    TuiTable* sendTable = new TuiTable(nullptr);
    
    sendTable->setString("nonce", nonce);
    sendTable->setString("publicKey", sessionTransientPublicKey);
    sendTable->setString("data", cipherText);
    
    return sendTable;
}

void ClientNetInterface::callTrackerFunction(TuiTable* args) //function name is assumed first arg, callback is last
{
    if(args->arrayObjects.empty() || args->arrayObjects[0]->type() != Tui_ref_type_STRING)
    {
        MJError("callTrackerFunction expects an in initial string argument, a function name to call.");
        return;
    }
    
    TuiFunction* callback = nullptr;
    uint32_t callbackID = 0;
    TuiTable* dataToSecureTable = new TuiTable(nullptr);
    
    for(int i = 0; i < args->arrayObjects.size(); i++)
    {
        TuiRef* arg = args->arrayObjects[i];
        if(i == args->arrayObjects.size() - 1 && arg->type() == Tui_ref_type_FUNCTION)
        {
            callback = (TuiFunction*)(arg->retain());
            callbackID = functionCallbackIDCounter++;
            callbacksByID[callbackID] = callback;
            dataToSecureTable->setDouble("callbackID", callbackID);
        }
        else
        {
            arg->retain();
            dataToSecureTable->arrayObjects.push_back(arg);
        }
    }
    
    TuiTable* sendTable = getTrackerEncryptedDataTable(dataToSecureTable);
    dataToSecureTable->release();
    
    if (!sendTable)
    {
        MJError("Failed to encode");
        if(callback)
        {
            callback->call("encode error");
            callback->release();
            callbacksByID.erase(callbackID);
        }
        return;
    }
    
    std::string dataSerialized = sendTable->serializeBinary();
    sendTable->release();
    
    sendData(KATIPO_NET_TYPE_CLIENT_SERVER_FUNCTION_CALL_REQUEST, (void*)dataSerialized.data(), dataSerialized.length(), true);
        
}

void ClientNetInterface::callRemoteHostFunction(std::string hostPublicKey, TuiTable* args)
{
    if(args->arrayObjects.empty() || args->arrayObjects[0]->type() != Tui_ref_type_STRING)
    {
        MJError("callRemoteHostFunction expects an in initial string argument, a function name to call.");
        return;
    }
    
    TuiFunction* callback = nullptr;
    TuiTable* hostDataToSecureTable = new TuiTable(nullptr);
    
    for(int i = 0; i < args->arrayObjects.size(); i++)
    {
        TuiRef* arg = args->arrayObjects[i];
        if(i == args->arrayObjects.size() - 1 && arg->type() == Tui_ref_type_FUNCTION)
        {
            callback = (TuiFunction*)arg;
        }
        else
        {
            arg->retain();
            hostDataToSecureTable->arrayObjects.push_back(arg);
        }
    }
    
    uint32_t callbackID = 0;
    
    if(callback)
    {
        callbackID = functionCallbackIDCounter++;
        callbacksByID[callbackID] = ((TuiFunction*)callback->retain());
        hostDataToSecureTable->setDouble("callbackID", callbackID);
    }
    
    //encrypt the args with the host key, so only the host can decrypt
    TuiTable* hostSendTable = getHostOrClientEncryptedDataTable(hostPublicKey, hostDataToSecureTable);
    hostDataToSecureTable->release();
    
    if (!hostSendTable)
    {
        MJError("Failed to encode host data");
        if(callback)
        {
            callback->call("host encode error");
        }
        return;
    }
    
    std::string hostDataSerialized = hostSendTable->serializeBinary();
    hostSendTable->release();
    
    
    TuiTable* trackerDataToSecureTable = new TuiTable(nullptr);
    trackerDataToSecureTable->setString("hostPublicKey", hostPublicKey);
    trackerDataToSecureTable->setString("data", hostDataSerialized);
    if(callback) //in case it fails at the tracker level, we need the callback id here as hostSendTable is encrypted for the host only
    {
        trackerDataToSecureTable->setDouble("callbackID", callbackID);
    }
    
    //encrypt the hostPublicKey and data again with the tracker key, so only the tracker can decrypt this, find the host public key, and send it to the host
    // double encryption of hostSendTable probably isn't really be necessary, might get a performance boost for minimal security impact to split it out?
    TuiTable* trackerSendTable = getTrackerEncryptedDataTable(trackerDataToSecureTable);
    trackerDataToSecureTable->release();
    
    if (!trackerSendTable)
    {
        MJError("Failed to encode tracker data");
        if(callback)
        {
            callback->call("tracker encode error");
        }
        return;
    }
    
    std::string trackerDataSerialized = trackerSendTable->serializeBinary();
    trackerSendTable->release();
    
    sendData(KATIPO_NET_TYPE_REMOTE_HOST_REQUEST, (void*)trackerDataSerialized.data(), trackerDataSerialized.length(), true);
}

TuiTable* ClientNetInterface::bindTui(TuiTable* katipoTable_)
{
    katipoTable = katipoTable_;
    stateTable = new TuiTable(katipoTable);
    
    
    stateTable->onSet = [this](TuiRef* table, const std::string& key, TuiRef* value) {
        /*if(key == "clientConnected")
        {
            if(value->type() == Tui_ref_type_FUNCTION)
            {
                clientConnectedFunction = (TuiFunction*)value;
            }
            else if(value->type() == Tui_ref_type_NIL)
            {
                clientConnectedFunction = nullptr;
            }
        }*/
        
    };
    
    /*stateTable->setFunction("register", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(args->arrayObjects.size() >= 2)
        {
            TuiRef* functionNameRef = args->arrayObjects[0];
            TuiRef* functionRef = args->arrayObjects[1];
            if(functionNameRef->type() == Tui_ref_type_STRING && functionRef->type() == Tui_ref_type_FUNCTION)
            {
                registeredFunctions[((TuiString*)functionNameRef)->value] = (TuiFunction*)functionRef;
                return TUI_TRUE;
            }
            else
            {
                TuiParseError(callingDebugInfo->fileName.c_str(), callingDebugInfo->lineNumber, "Incorrect argument type");
            }
        }
        else
        {
            TuiParseError(callingDebugInfo->fileName.c_str(), callingDebugInfo->lineNumber, "Missing args");
        }
        return TUI_FALSE;
    });*/
    
    // client.callTrackerFunction(clientID, "playlists", testPlaylists)
    stateTable->setFunction("callTrackerFunction", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(disconnected || !connected)
        {
            TuiParseError(callingDebugInfo->fileName.c_str(), callingDebugInfo->lineNumber, "attempted to callTrackerFunction, but we are not connected");
            return TUI_NIL;
        }
        
        callTrackerFunction(args);
        
        return TUI_NIL;
    });
    
    
    // client.downloadFromServer(clientID, "song", arg1, ... , callbackFunction)
    /*stateTable->setFunction("downloadFromServer", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(disconnected || !connected)
        {
            TuiParseError(callingDebugInfo->fileName.c_str(), callingDebugInfo->lineNumber, "attempted to downloadFromServer, but we have been disconnected");
            return TUI_FALSE;
        }
        if(args->arrayObjects.size() >= 1)
        {
            TuiRef* functionNameRef = args->arrayObjects[0];
            if(functionNameRef->type() == Tui_ref_type_STRING)
            {
                TuiTable* sendTable = new TuiTable(nullptr);
                sendTable->set("name", functionNameRef);
                for(int i = 1; i < args->arrayObjects.size(); i++)
                {
                    if(i == args->arrayObjects.size() - 1 && args->arrayObjects[i]->type() == Tui_ref_type_FUNCTION)
                    {
                        callbacksByID[functionCallbackIDCounter] = ((TuiFunction*)args->arrayObjects[i]->retain());
                        sendTable->setDouble("callbackID", functionCallbackIDCounter++);
                    }
                    else
                    {
                        TuiRef* arg = args->arrayObjects[i];
                        arg->retain();
                        sendTable->arrayObjects.push_back(arg);
                    }
                }
                
                std::string dataSerialized = sendTable->serializeBinary();
                sendTable->release();
                
                sendData(KATIPO_NET_TYPE_CLIENT_SERVER_DOWNLOAD_FILE_REQUEST, (void*)dataSerialized.data(), dataSerialized.length(), true);
                return TUI_TRUE;
                
            }
            else
            {
                TuiParseError(callingDebugInfo->fileName.c_str(), callingDebugInfo->lineNumber, "Incorrect argument type");
            }
        }
        else
        {
            TuiParseError(callingDebugInfo->fileName.c_str(), callingDebugInfo->lineNumber, "Missing args");
        }
        return TUI_FALSE;
    });*/
    
    
    
    
    // client.disconnect()
    stateTable->setFunction("disconnect", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(!disconnected)
        {
            disconnect();
            //todo call a disconnect function
            /*if(registeredFunctions.count("disconnected") != 0)
            {
                registeredFunctions["disconnected"]->call("disconnect");
            }*/
        }
        return TUI_NIL;
    });
    
    return stateTable;
}



#define GOAL_TIME_PER_UPDATE 0.01

void ClientNetInterface::startThread()
{
    //std::string logPath = Tui::getSavePath("enetClientLog.log"); //todo hmm
    
    Timer* timer = new Timer();
    
    while(1)
    {
        if(connected)
        {
            while(!inputQueue->empty())
            {
                ClientNetInterfaceInput input;
                inputQueue->pop(input);
                
#if LOG_NETWORK
                if(input.dataLength > 1024)
                {
                    MJLog("send large packet:%.2fkb type:%d", ((double)input.dataLength) / 1024, ((uint8_t*)input.data)[0]);
                }
#endif
                
                ENetPacket * packet = enet_packet_create (input.data,
                                                          input.dataLength + sizeof(uint8_t),
                                                          (input.reliable ? ENET_PACKET_FLAG_RELIABLE : 0));
                
                
                enet_peer_send(enetPeer, 0, packet);
                
                free(input.data);
            }
        }
        
        checkEnetEvents();
        
        if(needsToExit)
        {
            return;
        }
        
        double timeElapsed = timer->getDt();
        if(timeElapsed < GOAL_TIME_PER_UPDATE)
        {
            std::this_thread::sleep_for(std::chrono::duration<double>(GOAL_TIME_PER_UPDATE - timeElapsed));
        }
    }
}

void ClientNetInterface::checkEnetEvents()
{
    ENetEvent event;
    while(enetClient && enet_host_service(enetClient, &event, 0) > 0)
    {
        if(needsToExit)
        {
            return;
        }
        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                MJLog("initial connection established.\n");
                
                /**/
                
            }
                break;
            case ENET_EVENT_TYPE_RECEIVE:
            {
                if(event.packet->dataLength < 1)
                {
                    MJLog("packet too small");
                }
                else
                {
                    ServerData incoming;
                    incoming.type = ((uint8_t*)(event.packet->data))[0];
                    if(event.packet->dataLength > 1)
                    {
                        incoming.data = &(((uint8_t*)(event.packet->data))[1]);
                        incoming.length = event.packet->dataLength - 1;
                    }
                    else
                    {
                        incoming.data = NULL;
                        incoming.length = 0;
                    }
                    
                    if(incoming.type == KATIPO_NET_TYPE_SERVER_JOIN_RESPONSE_REJECT)
                    {
                        ClientNetInterfaceOutput output;
                        
                        output.outputType = CLIENT_NET_INTERFACE_OUTPUT_CLIENT_REJECTED;
                        
                        output.serverData.type = incoming.type;
                        
                        if(incoming.length > 0)
                        {
                            output.serverData.data = malloc(incoming.length);
                            memcpy(output.serverData.data, incoming.data, incoming.length);
                        }
                        else
                        {
                            output.serverData.data = nullptr;
                        }
                        output.serverData.length = incoming.length;
                        
                        outputQueue->push(output);
                        
                    }
                    else if(incoming.type == KATIPO_NET_TYPE_CLIENT_SERVER_DOWNLOAD_FILE_COMPLETE_NOTIFICATION)
                    {
                        uint8_t channelIndex = *((uint8_t*)incoming.data);
                        inUseChannels.erase(channelIndex);
                    }
                    else if(incoming.type == KATIPO_NET_TYPE_INITIAL_HANDSHAKE)
                    {
                        trackerPublicKey = std::string((const char*)incoming.data, incoming.length);
                        
                        //We just want to send through the clientID, but that is already public in sendTable, so lets just send an empty table for now, we will need stuff later for sure
                        TuiTable* dataToSecureTable = initialData;
                        if(dataToSecureTable)
                        {
                            dataToSecureTable->retain();
                        }
                        else
                        {
                            dataToSecureTable = new TuiTable(nullptr);
                        }
                        //todo add initial data
                        
                        TuiTable* sendTable = getTrackerEncryptedDataTable(dataToSecureTable);
                        dataToSecureTable->release();
                        
                        if (!sendTable)
                        {
                            MJError("Failed to encode");
                            abort();
                        }
                        
                        std::string data = sendTable->serializeBinary();
                        
                        int dataSize = (int)data.length() + (int)sizeof(uint8_t);
                        uint8_t* netData = (uint8_t*)malloc(dataSize);
                        netData[0] = KATIPO_NET_TYPE_CLIENT_JOIN_REQUEST;
                        memcpy(&(netData[1]), data.data(), data.length());
                        
                        ENetPacket * packet = enet_packet_create(netData,
                                                                  dataSize,
                                                                  ENET_PACKET_FLAG_RELIABLE);
                        
                        
                        enet_peer_send(enetPeer, 0, packet);
                        free(netData);
                        sendTable->release();
                        
                        connected = true;
                        
                        if(katipoTable->hasKey("connected"))
                        {
                            TuiFunction* connectedFunction = ((TuiFunction*)katipoTable->get("connected"));
                            connectedFunction->call("connected");
                        }
                    }
                    else
                    {
                        bool sendToOutput = true;
                        bool sendDownloadAcknowledge = true;
                        
                        /*if(incoming.type == KATIPO_NET_TYPE_SERVER_DOWNLOAD_FILE_RESPONSE)
                        {
                            sendDownloadAcknowledge = true;
                        }*/
                        if(incoming.type == KATIPO_NET_TYPE_SERVER_MULTIPART_DOWNLOAD_RESPONSE)
                        {
                            sendToOutput = false;
                            
                            uint32_t additionalHeaderSize = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t);
                            
                            uint32_t totalSize = *((uint32_t*)(((uint8_t*)incoming.data) + 1));
                            uint32_t dataStartOffset = *((uint32_t*)(((uint8_t*)incoming.data) + 5));
                            
                            uint32_t recievedPayloadSize = (uint32_t)incoming.length - additionalHeaderSize;
                            
                            MJLog("multipart download: %d/%d", dataStartOffset + recievedPayloadSize, totalSize);
                            
                            if(recievedPayloadSize + dataStartOffset == totalSize)
                            {
                                MJLog("multipart download complete");
                                if(inProgressMultiPartDownloadsByChannel.count(event.channelID) == 0)
                                {
                                    MJError("Got unexpected final multpart download packet");
                                    abort();
                                }
                                
                                inProgressMultiPartDownloadsByChannel[event.channelID].append((((const char*)incoming.data) + additionalHeaderSize), recievedPayloadSize);
                               
                                
                                ClientNetInterfaceOutput output;
                                
                                output.outputType = CLIENT_NET_INTERFACE_OUTPUT_DATA_RECEIEVED;
                                
                                output.serverData.type = *(((uint8_t*)incoming.data) + 0);
                                
                                output.serverData.data = malloc(totalSize);
                                memcpy(output.serverData.data, inProgressMultiPartDownloadsByChannel[event.channelID].data(), inProgressMultiPartDownloadsByChannel[event.channelID].size());
                                output.serverData.length = totalSize;
                                
                                outputQueue->push(output);
                                inProgressMultiPartDownloadsByChannel.erase(event.channelID);
                            }
                            else
                            {
                                inProgressMultiPartDownloadsByChannel[event.channelID].append((((const char*)incoming.data) + additionalHeaderSize), recievedPayloadSize);
                                sendDownloadAcknowledge = false;
                            }
                        }
                        
                        if(sendDownloadAcknowledge)
                        {
                            uint8_t data[2] = {
                                KATIPO_NET_TYPE_CLIENT_SERVER_DOWNLOAD_FILE_COMPLETE_NOTIFICATION,
                                event.channelID
                            };
                            ENetPacket * packet = enet_packet_create (data,
                                                                      sizeof(uint8_t) * 2,
                                                                      ENET_PACKET_FLAG_RELIABLE);
                            
                            
                            enet_peer_send(enetPeer, 0, packet);
                        }
                        
                        if(sendToOutput)
                        {
                            ClientNetInterfaceOutput output;
                            
                            output.outputType = CLIENT_NET_INTERFACE_OUTPUT_DATA_RECEIEVED;
                            
                            output.serverData.type = incoming.type;
                            if(incoming.length > 0)
                            {
                                output.serverData.data = malloc(incoming.length);
                                memcpy(output.serverData.data, incoming.data, incoming.length);
                            }
                            else
                            {
                                output.serverData.data = nullptr;
                            }
                            output.serverData.length = incoming.length;
                            
                            outputQueue->push(output);
                        }
                        
                    }
                }
                enet_packet_destroy (event.packet);
            }
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
            {
                
                enetPeer = nullptr;
                if(enetClient)
                {
                    ClientNetInterfaceOutput output;
                    output.outputType = CLIENT_NET_INTERFACE_OUTPUT_CLIENT_DISCONNECTED;
                    outputQueue->push(output);
                }
            }
                break;
            default:
                break;
        }
        if(needsToExit)
        {
            return;
        }
    }
}


TuiTable* ClientNetInterface::getDecryptedDataTable(TuiTable* tuiDataWrapper, bool useTransientSessionKey)
{
    if(tuiDataWrapper && tuiDataWrapper->hasKey("nonce") && tuiDataWrapper->hasKey("data") && tuiDataWrapper->hasKey("publicKey"))
    {
        TuiRef* dataRef = tuiDataWrapper->get("data");
        TuiRef* nonceRef = tuiDataWrapper->get("nonce");
        TuiRef* publicKeyRef = tuiDataWrapper->get("publicKey");
        
        TuiTable* decryptedDataTable = nullptr;
        
        if(dataRef->type() == Tui_ref_type_STRING &&
           nonceRef->type() == Tui_ref_type_STRING &&
           publicKeyRef->type() == Tui_ref_type_STRING)
        {
            std::string decrypted;
            unsigned long encryptedLength = (((TuiString*)dataRef)->value).length();
            decrypted.resize(encryptedLength - crypto_box_MACBYTES);
            
            if (crypto_box_open_easy((unsigned char*)&(decrypted[0]),
                                     (unsigned char*)&((((TuiString*)dataRef)->value)[0]),
                                     encryptedLength,
                                     (unsigned char*)(((TuiString*)nonceRef)->value).c_str(),
                                     (unsigned char*)(((TuiString*)publicKeyRef)->value).c_str(),
                                     (unsigned char*)&((useTransientSessionKey ? sessionTransientSecretKey : savedPermanentSecretKey)[0])) != 0)
            {
                MJError("attempt failed to decrypt in ClientNetInterface::getDecryptedDataTable");
            }
            else
            {
                decryptedDataTable = (TuiTable*)TuiRef::loadBinaryString(std::string((const char*)decrypted.data(), decrypted.length())); //todo memcpys
                return decryptedDataTable;
            }
        }
    }
    
    return nullptr;
}

void ClientNetInterface::processGetRequest(TuiTable* trackerData) //we are on a host, a client has sent us this request via tracker
{
    if(!(trackerData && trackerData->hasKey("requestID") && trackerData->hasKey("data")))
    {
        MJError("bad request");
        return;
    }
    
    int length = 0;
    TuiTable* tuiDataWrapper = (TuiTable*)TuiRef::loadBinaryString((const char*)((TuiString*)trackerData->objectsByStringKey["data"])->value.c_str(), &length, nullptr);
    TuiTable* clientData = getDecryptedDataTable(tuiDataWrapper, false); //use our permanent key, as that is what is provided by the tracker and what the client uses to encrypt this
    std::string clientPublicKey = tuiDataWrapper->getString("publicKey");
    tuiDataWrapper->release();
    
    if(clientData && !clientData->arrayObjects.empty() && clientData->arrayObjects[0]->type() == Tui_ref_type_STRING)
    {
        if(katipoTable->hasKey("get"))
        {
            clientData->retain();
            TuiTable* sendArgs = new TuiTable(nullptr);
            
            TuiDebugInfo debugInfo;
            debugInfo.fileName = "FUNCTION_CALL_REQUEST";
            
            for(int i = 0; i < clientData->arrayObjects.size(); i++)
            {
                sendArgs->push(clientData->arrayObjects[i]);
            }
            
            TuiFunction* callbackFunction = nullptr;
            if(clientData->hasKey("callbackID"))
            {
                uint32_t callbackID = clientData->getDouble("callbackID");
                std::string requestID = trackerData->getString("requestID");
                
                callbackFunction = new TuiFunction([this, callbackID, requestID, clientPublicKey](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
                    TuiTable* clientDataToSecureTable = new TuiTable(nullptr);
                    clientDataToSecureTable->setDouble("callbackID", callbackID);
                    bool sendFile = false;
                    
                    if(args && args->arrayObjects.size() > 0)
                    {
                        if(args->arrayObjects[0]->type() == Tui_ref_type_TABLE)
                        {
                            TuiTable* resultTable = (TuiTable*)args->arrayObjects[0];
                            
                            if(resultTable->objectsByStringKey.count("status") != 0 && resultTable->objectsByStringKey.count("filePath") != 0)
                            {
                                if(((TuiString*)resultTable->objectsByStringKey["status"])->value == "ok")
                                {
                                    TuiRef* filePathRef = resultTable->objectsByStringKey["filePath"];
                                    std::string filePath = ((TuiString*)filePathRef)->value;
                                    resultTable->set("filePath", TUI_NIL);
                                    resultTable->setString("fileName", Tui::fileNameFromPath(filePath));
                                    
                                    TuiString* fileDataRef = new TuiString("");
                                    
                                    Tui::getFileContents(filePath, &(fileDataRef->value));
                                    if(!fileDataRef->value.empty())
                                    {
                                        sendFile = true;
                                        resultTable->set("fileData", fileDataRef);
                                        clientDataToSecureTable->set("data", resultTable);
                                    }
                                    else
                                    {
                                        clientDataToSecureTable->setString("status", "error");
                                        clientDataToSecureTable->setString("message", "unable to load file not found");
                                        MJError("Unable to load file not found:\n%s", filePath.c_str());
                                    }
                                    
                                }
                                else
                                {
                                    clientDataToSecureTable->set("filePath", TUI_NIL);
                                }
                            }
                        }
                        
                        if(!sendFile)
                        {
                            clientDataToSecureTable->set("data", args->arrayObjects[0]);
                        }
                        
                    }
                    //clientDataToSecureTable needs to be serialized, and then broken up into smaller packets if needed, send in multiple chunks
                    TuiTable* clientSendTable = getHostOrClientEncryptedDataTable(clientPublicKey, clientDataToSecureTable);
                    clientDataToSecureTable->release();
                    
                    if (!clientSendTable)
                    {
                        MJError("Failed to encode");
                        abort(); //todo shouldn't abort here
                    }
                    
                    TuiTable* trackerDataToSecureTable = new TuiTable();
                    trackerDataToSecureTable->setString("requestID", requestID);
                    trackerDataToSecureTable->set("clientData", clientSendTable);
                    clientSendTable->release();
                    
                    TuiTable* trackerSendTable = getTrackerEncryptedDataTable(trackerDataToSecureTable);
                    trackerDataToSecureTable->release();
                    std::string dataSerialized = trackerSendTable->serializeBinary();
                    trackerSendTable->release();
                    
                    
                    sendData(KATIPO_NET_TYPE_FUNCTION_CALL_RESPONSE_TO_CLIENT_FROM_HOST, dataSerialized.data(), dataSerialized.length());
                    
                    //if(sendFile) //todo should use sendLargeData if data size above threshold, not based on whether it was loaded from disk
                  //  {
                   //     sendLargeData(KATIPO_NET_TYPE_SERVER_DOWNLOAD_FILE_RESPONSE, dataSerialized.data(), dataSerialized.length());
                   // }
                   // else
                   // {
                        
                   //     sendData(KATIPO_NET_TYPE_CLIENT_FUNCTION_CALL_RESPONSE, dataSerialized.data(), dataSerialized.length(), true);
                   // }
                    
                    return TUI_NIL;
                });
                sendArgs->push(callbackFunction);
            }
            
            TuiRef* result = ((TuiFunction*)(katipoTable->objectsByStringKey["get"]))->call(sendArgs, nullptr, &debugInfo);
            
            if(callbackFunction && result && result->type() != Tui_ref_type_NIL)
            {
                TuiTable* funcCallArgs = new TuiTable(nullptr);
                
                funcCallArgs->push(result);
                
                callbackFunction->call(funcCallArgs, nullptr, &debugInfo);
                
                funcCallArgs->release();
                result->release();
            }
            
            if(callbackFunction)
            {
                callbackFunction->release();
            }
            
            sendArgs->release();
        }
        else
        {
            MJError("katipo.get callback function not set.");
        }
    }
}

void ClientNetInterface::pollNetEvents()
{
    if(disconnected)
    {
        if(!reconnectTimer)
        {
            reconnectTimer = new Timer();
        }
        else
        {
            if(reconnectTimer->getElapsed() > 5.0)
            {
                MJLog("Attempting to reconnect...");
                delete reconnectTimer;
                reconnectTimer = nullptr;
                connect();
            }
        }
        return;
    }
    
    while(!outputQueue->empty())
    {
        ClientNetInterfaceOutput output;
        outputQueue->pop(output);
        
        switch(output.outputType)
        {
            case CLIENT_NET_INTERFACE_OUTPUT_CLIENT_REJECTED:
            {
                if(output.serverData.data)
                {
                    disconnect();
                    /*if(registeredFunctions.count("disconnected") != 0)
                    {
                        registeredFunctions["disconnected"]->call("CLIENT_NET_INTERFACE_OUTPUT_CLIENT_DISCONNECTED");
                    }*/
                    return;
                }
            }
                break;
            case CLIENT_NET_INTERFACE_OUTPUT_CLIENT_DISCONNECTED:
            {
                disconnect();
                /*if(registeredFunctions.count("disconnected") != 0)
                {
                    registeredFunctions["disconnected"]->call("CLIENT_NET_INTERFACE_OUTPUT_CLIENT_DISCONNECTED");
                }*/
                return;
            }
                break;
            case CLIENT_NET_INTERFACE_OUTPUT_DATA_RECEIEVED:
            {
                switch (output.serverData.type) {
                    case KATIPO_NET_TYPE_FUNCTION_CALL_RESPONSE_TO_CLIENT_FROM_TRACKER: //client gets this after calling get, response is from tracker, data but no status means ok
                        // {
                        //  data = "data"
                        // }
                        // or
                        // {
                        //  status = "error"
                        //  message = "error message"
                        // }
                    {
                        int length = 0;
                        TuiTable* tuiDataWrapper = (TuiTable*)TuiRef::loadBinaryString((const char*)output.serverData.data, &length, nullptr);
                        TuiTable* tuiData = getDecryptedDataTable(tuiDataWrapper, false);
                        tuiDataWrapper->release();
                        
                        uint32_t callbackID = ((TuiNumber*)tuiData->objectsByStringKey["callbackID"])->value;
                        
                        if(callbacksByID.count(callbackID) != 0)
                        {
                            callbacksByID[callbackID]->call("SERVER_FUNCTION_CALL_RESPONSE", tuiData);
                        }
                        
                        tuiData->release();
                        
                    }
                        break;
                    case KATIPO_NET_TYPE_FUNCTION_CALL_RESPONSE_TO_CLIENT_FROM_HOST: //client gets this after calling get, response is from host, data but no status means ok
                    {
                        int length = 0;
                        TuiTable* trackerDataWrapper = (TuiTable*)TuiRef::loadBinaryString((const char*)output.serverData.data, &length, nullptr);
                        TuiTable* trackerData = getDecryptedDataTable(trackerDataWrapper, false);
                        trackerDataWrapper->release();
                        
                        if(trackerData)
                        {
                            if(trackerData->hasKey("data"))
                            {
                                TuiRef* hostEncryptedData = trackerData->get("data");
                                if(hostEncryptedData->type() != Tui_ref_type_TABLE)
                                {
                                    MJError("Expected table");
                                }
                                else
                                {
                                    TuiTable* hostData = getDecryptedDataTable((TuiTable*)hostEncryptedData, true);
                                    
                                    if(hostData->hasKey("callbackID"))
                                    {
                                        uint32_t callbackID = ((TuiNumber*)hostData->objectsByStringKey["callbackID"])->value;
                                        TuiRef* responseData = hostData->get("data");
                                        //todo test handling of status/message responses
                                        
                                        if(callbacksByID.count(callbackID) != 0)
                                        {
                                            //MJLog("calling func in KATIPO_NET_TYPE_FUNCTION_CALL_RESPONSE_TO_CLIENT_FROM_TRACKER");
                                            callbacksByID[callbackID]->call("KATIPO_NET_TYPE_FUNCTION_CALL_RESPONSE_TO_CLIENT_FROM_HOST", responseData);
                                        }
                                    }
                                    else
                                    {
                                        MJError("Expected callbackID");
                                    }
                                    
                                    hostData->release();
                                }
                            }
                            
                            trackerData->release();
                        }
                        
                    }
                        break;
                    case KATIPO_NET_TYPE_REMOTE_HOST_REQUEST: //we must be a host /todo if we are a client we shouldn't listen for this
                    {
                        int length = 0;
                        TuiTable* tuiDataWrapper = (TuiTable*)TuiRef::loadBinaryString((const char*)output.serverData.data, &length, nullptr);
                        TuiTable* tuiData = getDecryptedDataTable(tuiDataWrapper, false);
                        tuiDataWrapper->release();
                        processGetRequest(tuiData);
                        tuiData->release();
                        
                    }
                        break;
                    default:
                    {
                        MJError("unhandled data type");
                    }
                        break;
                }
                    
                if(output.serverData.data)
                {
                    free(output.serverData.data);
                }
            }
                break;
        }
    }
}

#define CLIENT_MAX_SIMULTANEOUS_DOWNLOADS 4


void ClientNetInterface::sendLargeDataInternal(uint8_t type,
    const void * data,
    size_t dataLength,
    uint8_t channel)
{
    if(!enetPeer || !enetClient || disconnected)
    {
        MJLog("Attempt to send data with no connection.");
        return;
    }
    
    if(dataLength > MJMaxPacketSize)
    {
        uint32_t bytesToSend = (uint32_t)dataLength;
        //uint32_t dataStartOffset = 0;
        while(bytesToSend > 0)
        {
            MJError("Todo ClientNetInterface::sendLargeDataInternal. Katipo does not currently support data larger than 32MB.");
            abort();
            //todo IMPORTANT we need to send this in chunks with the request ID so that it can be passed on to the client immediately
            /*uint32_t additionalHeaderSize = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t);
            uint32_t thisPacketLoadBytesToSend = min(bytesToSend, MJMaxPacketSize);
            uint32_t thisPacketTotalSize = thisPacketLoadBytesToSend + sizeof(uint8_t) + additionalHeaderSize;
            uint8_t* netData = (uint8_t*)malloc(thisPacketTotalSize);
            netData[0] = KATIPO_NET_TYPE_SERVER_MULTIPART_DOWNLOAD_RESPONSE;
            netData[1] = type;
            
            memcpy(&(netData[2]), &dataLength, sizeof(uint32_t));
            memcpy(&(netData[6]), &dataStartOffset, sizeof(uint32_t));
            
            memcpy(&(netData[10]), ((uint8_t*)data + dataStartOffset), thisPacketLoadBytesToSend);
            
            dataStartOffset += thisPacketLoadBytesToSend;
            bytesToSend -= thisPacketLoadBytesToSend;
            
            ClientNetInterfaceInput input;
            input.data = netData;
            input.dataLength = thisPacketLoadBytesToSend + additionalHeaderSize;
            input.reliable = true;
            input.channelID = channel;
            inputQueue->push(input);*/
        }
    }
    else
    {
        uint8_t* netData = (uint8_t*)malloc(dataLength + sizeof(uint8_t));
        netData[0] = type;
        
        memcpy(&(netData[1]), data, dataLength);
        
        ClientNetInterfaceInput input;
        input.data = netData;
        input.dataLength = dataLength;
        input.reliable = true;
        input.channelID = channel;
        inputQueue->push(input);
    }
}

void ClientNetInterface::sendData(uint8_t type, const void * data, size_t dataLength, bool reliable)
{
    int freeChannel = 0;
    for(freeChannel = 0; freeChannel < CLIENT_MAX_SIMULTANEOUS_DOWNLOADS; freeChannel++)
    {
        if(inUseChannels.count(freeChannel) == 0)
        {
            break;
        }
    }
    
    if(freeChannel < CLIENT_MAX_SIMULTANEOUS_DOWNLOADS)
    {
        inUseChannels.insert(freeChannel);
        sendLargeDataInternal(type, data, dataLength, freeChannel);
    }
    else
    {
        MJError("todo MAX_SIMULTANEOUS_DOWNLOADS reached");
        //queuedDownloads.push(serverData); // hmmmmm serverData.data is not valid once we exit this function
    }
}
