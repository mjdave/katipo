
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



ClientNetInterface::ClientNetInterface(std::string host_, //init from a scanner connection
                                       std::string port_,
                                       const std::string& savedPermanentPublicKey_,
                                       const std::string& savedPermanentSecretKey_,
                                       std::string trackerPublicKey_,
                                       ENetHost* enetClient_,
                                       ENetPeer* enetPeer_)
{
    host = host_;
    port = port_;
    savedPermanentPublicKey = savedPermanentPublicKey_;
    savedPermanentSecretKey = savedPermanentSecretKey_;
    
    inputQueue = new ThreadSafeQueue<ClientNetInterfaceInput>();
    outputQueue = new ThreadSafeQueue<ClientNetInterfaceOutput>();
    
    //todo this key pair is not used on the host, doesn't need to be generated there
    sessionTransientPublicKey.resize(crypto_box_PUBLICKEYBYTES);
    sessionTransientSecretKey.resize(crypto_box_SECRETKEYBYTES);
    crypto_box_keypair((unsigned char*)&(sessionTransientPublicKey[0]), (unsigned char*)&(sessionTransientSecretKey[0]));
    
    trackerPublicKey = trackerPublicKey_;
    enetClient = enetClient_;
    enetPeer = enetPeer_;
    
    disconnected = false;
    needsToExit = false;
    
    sendInitialData();
    
    thread = new std::thread(&ClientNetInterface::startThread, this);
    
}


ClientNetInterface::~ClientNetInterface()
{
    disconnect();
    enet_deinitialize();
    delete inputQueue;
    delete outputQueue;
    if(initialData)
    {
        initialData->release();
    }
}

bool ClientNetInterface::connectedOrConnecting()
{
    return (thread != nullptr);
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
    
    //enet_host_compress_with_range_coder(enetClient); //NO!, lets save CPU and use the bandwidth instead.
    
    ENetAddress address;
    
    enet_address_set_host (&address, host.c_str());
    address.port = atoi(port.c_str());
    
    MJLog("Connecting to tracker...");
    enetPeer = enet_host_connect (enetClient, &address, ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT, 0);
    
    enet_peer_timeout(enetPeer, 0, 2000, 3000);
    
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
    
    if(!callbacksByID.empty())
    {
        TuiRef* statusResult = new TuiTable("{status='error',message='not connected'}");
        for(auto& idAndCallback : callbacksByID)
        {
            idAndCallback.second.func->call("SERVER_FUNCTION_CALL_RESPONSE", statusResult);
            idAndCallback.second.func->release();
            if(idAndCallback.second.progressCallback)
            {
                idAndCallback.second.progressCallback->release();
            }
        }
        statusResult->release();
        callbacksByID.clear();
    }
    
    if(connected)
    {
        if(katipoTable && katipoTable->hasKey("onDisconnected"))
        {
            TuiFunction* disconnectedFunction = ((TuiFunction*)katipoTable->get("onDisconnected"));
            disconnectedFunction->call("onDisconnected");
        }
    }
    else
    {
        if(katipoTable && katipoTable->hasKey("onConnectionFailed"))
        {
            TuiFunction* disconnectedFunction = ((TuiFunction*)katipoTable->get("onConnectionFailed"));
            disconnectedFunction->call("onConnectionFailed");
        }
    }
    
    needsToExit = true;
    connected = false;
    disconnected = true;
    thread->join();
    delete thread;
    thread = nullptr;
    
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
            callbacksByID[callbackID].func = callback;
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

void ClientNetInterface::callRemoteHostFunction(std::string hostSiteKey, std::string hostPublicKey, TuiTable* args)
{
    if(args->arrayObjects.empty() || args->arrayObjects[0]->type() != Tui_ref_type_STRING)
    {
        MJError("callRemoteHostFunction expects an in initial string argument, a function name to call.");
        return;
    }
    
    TuiFunction* callback = nullptr;
    TuiFunction* progressCallback = nullptr;
    TuiTable* hostDataToSecureTable = new TuiTable(nullptr);
    
    for(int i = 0; i < args->arrayObjects.size(); i++)
    {
        TuiRef* arg = args->arrayObjects[i];
        if(i == args->arrayObjects.size() - 2 && arg->type() == Tui_ref_type_FUNCTION)
        {
            progressCallback = (TuiFunction*)arg;
        }
        else if(i == args->arrayObjects.size() - 1 && arg->type() == Tui_ref_type_FUNCTION)
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
        callbacksByID[callbackID].func = ((TuiFunction*)callback->retain());
        if(progressCallback)
        {
            callbacksByID[callbackID].progressCallback = ((TuiFunction*)progressCallback->retain());
        }
        callbacksByID[callbackID].hostSiteKey = hostSiteKey;
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

void ClientNetInterface::bindTui(TuiTable* katipoTable_)
{
    katipoTable = katipoTable_;
}



#define GOAL_TIME_PER_UPDATE 0.01

void ClientNetInterface::startThread()
{
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
                
                
                enet_peer_send(enetPeer, input.channelID, packet);
                
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

void ClientNetInterface::sendInitialData()
{
    TuiTable* dataToSecureTable = initialData;
    if(dataToSecureTable)
    {
        dataToSecureTable->retain();
    }
    else
    {
        //We just want to send through the clientID, but that is already public in sendTable, so lets just send an empty table
        dataToSecureTable = new TuiTable(nullptr);
    }
    
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
    
    if(katipoTable && katipoTable->hasKey("onConnected")) //hmmm not thread safe?
    {
        TuiFunction* connectedFunction = ((TuiFunction*)katipoTable->get("onConnected"));
        connectedFunction->call("onConnected");
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
                MJLog("Initial connection established");
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
                    else if(incoming.type == KATIPO_NET_TYPE_INITIAL_HANDSHAKE)
                    {
                        trackerPublicKey = std::string((const char*)incoming.data, incoming.length);
                        
                        sendInitialData();
                    }
                    else
                    {
                        bool sendToOutput = true;
                        
                        /*if(incoming.type == KATIPO_NET_TYPE_SERVER_DOWNLOAD_FILE_RESPONSE)
                        {
                            sendDownloadAcknowledge = true;
                        }*/
                        /*if(incoming.type == KATIPO_NET_TYPE_SERVER_MULTIPART_DOWNLOAD_RESPONSE) //hmm KATIPO_NET_TYPE_GET_RESPONSE_TO_CLIENT_FROM_HOST
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
                            }
                        }*/
                        
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
            TuiDebugInfoPush(&debugInfo, "processGetRequest", 1);
            
            for(int i = 0; i < clientData->arrayObjects.size(); i++)
            {
                sendArgs->push(clientData->arrayObjects[i]);
            }
            
            TuiFunction* callbackFunction = nullptr;
            if(clientData->hasKey("callbackID"))
            {
                uint32_t callbackID = clientData->getDouble("callbackID");
                std::string requestID = trackerData->getString("requestID");
                
                callbackFunction = new TuiFunction([this, callbackID, requestID, clientPublicKey](TuiTable* args, TuiRef* existingResult, TuiFunctionCallData* incomingCallData, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
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
                                    
                                    fileDataRef->release();
                                    
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
                    std::string clientDataToSecureTableFullSerialized = clientDataToSecureTable->serializeBinary();
                    clientDataToSecureTable->release();
                    
                    sendMultipartTuiData(requestID, callbackID, clientPublicKey, clientDataToSecureTableFullSerialized);
                    
                    return TUI_NIL;
                });
                sendArgs->push(callbackFunction);
            }
            
            TuiRef* result = ((TuiFunction*)(katipoTable->objectsByStringKey["get"]))->call(sendArgs, nullptr, nullptr, &debugInfo);
            
            if(callbackFunction && result && result->type() != Tui_ref_type_NIL)
            {
                TuiTable* funcCallArgs = new TuiTable(nullptr);
                
                funcCallArgs->push(result);
                
                callbackFunction->call(funcCallArgs, nullptr, nullptr, &debugInfo);
                
                funcCallArgs->release();
            }
            
            if(result)
            {
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
    
    if(clientData)
    {
        clientData->release();
    }
}

void ClientNetInterface::pollNetEvents()
{
    if(disconnected)
    {
        if(!reconnectTimer)
        {
            if(katipoTable->getBool("retry"))
            {
                timeBetweenReconnects = 5.0;
                if(katipoTable->hasKey("retryDelay"))
                {
                    timeBetweenReconnects = katipoTable->getDouble("retryDelay");
                }
                MJLog("Couldn't connect to tracker. Trying again in %.2f seconds", timeBetweenReconnects);
                reconnectTimer = new Timer();
            }
            /*else //commented out as probably handled elsewhere now
            {
                if(katipoTable->hasKey("onConnectionFailed"))
                {
                    TuiFunction* connectionFailedFunction = ((TuiFunction*)katipoTable->get("onConnectionFailed"));
                    connectionFailedFunction->call("onConnectionFailed");
                }
            }*/
        }
        else
        {
            if(reconnectTimer->getElapsed() > timeBetweenReconnects)
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
                    return;
                }
            }
                break;
            case CLIENT_NET_INTERFACE_OUTPUT_CLIENT_DISCONNECTED:
            {
                disconnect();
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
                            if(callbacksByID[callbackID].progressCallback)
                            {
                                callbacksByID[callbackID].progressCallback->release();
                                callbacksByID[callbackID].progressCallback = nullptr;
                            }
                            
                            callbacksByID[callbackID].func->call("SERVER_FUNCTION_CALL_RESPONSE", tuiData);
                            callbacksByID[callbackID].func->release();
                            callbacksByID.erase(callbackID);
                        }
                        
                        tuiData->release();
                        
                    }
                        break;
                    case KATIPO_NET_TYPE_GET_RESPONSE_TO_CLIENT_FROM_HOST: //client gets this after calling get, response is from host, data but no status means ok
                    {
                        int length = 0;
                        TuiTable* hostEncryptedData = (TuiTable*)TuiRef::loadBinaryString((const char*)output.serverData.data, &length, nullptr);
                        TuiTable* hostData = getDecryptedDataTable((TuiTable*)hostEncryptedData, true);
                        hostEncryptedData->release();
                           
                        if(hostData)
                        {
                            /*if(hostData->hasKey("callbackID")) //todo does this actually happen now?
                            {
                                uint32_t callbackID = ((TuiNumber*)hostData->objectsByStringKey["callbackID"])->value;
                                TuiRef* responseData = hostData->get("data");
                                //todo test handling of status/message responses

                                if(callbacksByID.count(callbackID) != 0)
                                {
                                    ClientNetCallback& callback = callbacksByID[callbackID];
                                    TuiRef* hostSiteKeyReadableRef = new TuiString(callback.hostSiteKey);
                                    callback.func->call("KATIPO_NET_TYPE_GET_RESPONSE_TO_CLIENT_FROM_HOST", responseData, hostSiteKeyReadableRef);
                                    callback.func->release();
                                    if(callbacksByID[callbackID].progressCallback)
                                    {
                                        callbacksByID[callbackID].progressCallback->release();
                                        callbacksByID[callbackID].progressCallback = nullptr;
                                    }
                                    callbacksByID.erase(callbackID);
                                    hostSiteKeyReadableRef->release();
                                }
                            }
                            else*/
                                
                            if(hostData->hasKey("total") && hostData->hasKey("requestID") && hostData->hasKey("clientData")) //multipart
                            {
                                std::string requestID = hostData->getString("requestID");
                                if(!requestID.empty())
                                {
                                    if(inProgressMultiPartDownloadsByRequestID.count(requestID) == 0)
                                    {
                                        inProgressMultiPartDownloadsByRequestID[requestID].data.resize(hostData->getDouble("total"));
                                    }

                                    ClientNetMultipartDownload& download = inProgressMultiPartDownloadsByRequestID[requestID];

                                    const std::string& clientData = ((TuiString*)hostData->objectsByStringKey["clientData"])->value;
                                    memcpy((void*)(&download.data[hostData->getDouble("offset")]),
                                        &(clientData[0]), clientData.length());

                                    download.recievedBytes += clientData.length();

                                    if(download.recievedBytes >= (uint32_t)hostData->getDouble("total"))
                                    {
                                        MJLog("recieved full download bytes:%d", download.recievedBytes);
                                        length = 0;
                                        TuiTable* combinedHostData = (TuiTable*)TuiRef::loadBinaryString(&download.data[0], &length, nullptr);

                                        if(combinedHostData->hasKey("callbackID"))
                                        {
                                            uint32_t callbackID = ((TuiNumber*)combinedHostData->objectsByStringKey["callbackID"])->value;
                                            TuiRef* responseData = combinedHostData->get("data");

                                            if(callbacksByID.count(callbackID) != 0)
                                            {
                                                ClientNetCallback& callback = callbacksByID[callbackID];
                                                TuiRef* hostSiteKeyReadableRef = new TuiString(callback.hostSiteKey);
                                                callback.func->call("KATIPO_NET_TYPE_GET_RESPONSE_TO_CLIENT_FROM_HOST", responseData, hostSiteKeyReadableRef);
                                                callback.func->release();
                                                if(callbacksByID[callbackID].progressCallback)
                                                {
                                                    callbacksByID[callbackID].progressCallback->release();
                                                    callbacksByID[callbackID].progressCallback = nullptr;
                                                }
                                                callbacksByID.erase(callbackID);
                                                hostSiteKeyReadableRef->release();
                                            }
                                        }

                                        combinedHostData->release();
                                        inProgressMultiPartDownloadsByRequestID.erase(requestID);
                                    }
                                    else
                                    {
                                        if(hostData->hasKey("callbackID"))
                                        {
                                            uint32_t callbackID = ((TuiNumber*)hostData->objectsByStringKey["callbackID"])->value;
                                            if(callbacksByID[callbackID].progressCallback)
                                            {
                                                TuiNumber* fractionNumber = new TuiNumber(((double)download.recievedBytes) / hostData->getDouble("total"));
                                                callbacksByID[callbackID].progressCallback->call("progress callback", fractionNumber);
                                                fractionNumber->release();
                                            }
                                        }
                                        MJLog("recieved download bytes:(%d/%d)", download.recievedBytes, (uint32_t)hostData->getDouble("total"));
                                    }
                                }
                            }
                            else
                            {
                                MJError("Expected callbackID");
                            }
                            hostData->release();
                        }
                        else
                        {
                           // MJError(""); this can happen if we reconnect, probably OK
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

void ClientNetInterface::sendMultipartTuiData(const std::string& requestID, double callbackID, const std::string& clientPublicKey, const std::string& clientDataToSecureTableFullSerialized) //todo pretty sure clientDataToSecureTableFullSerialized is double encrypted
{
    if(!enetPeer || !enetClient || disconnected)
    {
        MJLog("Attempt to send data with no connection.");
        return;
    }
    
    uint32_t dataLength = (uint32_t)clientDataToSecureTableFullSerialized.length();
    uint32_t bytesToSend = dataLength;
    uint32_t dataStartOffset = 0;
    
    TuiTable* trackerDataToSecureTable = new TuiTable();
    trackerDataToSecureTable->setString("requestID", requestID);
    
    while(bytesToSend > 0)
    {
        TuiTable* clientDataToSecureTable = new TuiTable(nullptr);
        clientDataToSecureTable->setString("requestID", requestID);
        clientDataToSecureTable->setDouble("callbackID", callbackID);
        clientDataToSecureTable->setDouble("total", dataLength);
        
        uint32_t thisPacketLoadBytesToSend = min(bytesToSend, MJMultipartChunkSize);
        clientDataToSecureTable->setString("clientData", clientDataToSecureTableFullSerialized.substr(dataStartOffset, thisPacketLoadBytesToSend));
        clientDataToSecureTable->setDouble("offset", dataStartOffset);
        
        TuiTable* clientSendTable = getHostOrClientEncryptedDataTable(clientPublicKey, clientDataToSecureTable);
        std::string clientSendTableSerialized = clientSendTable->serializeBinary();
        clientSendTable->release();
        
        dataStartOffset += thisPacketLoadBytesToSend;
        bytesToSend -= thisPacketLoadBytesToSend;
        
        trackerDataToSecureTable->setString("clientData", clientSendTableSerialized); //todo this probably doesn't need to be serialized
        TuiTable* trackerSendTable = getTrackerEncryptedDataTable(trackerDataToSecureTable);
        std::string dataSerialized = trackerSendTable->serializeBinary();
        trackerSendTable->release();
        clientDataToSecureTable->release();
        
        uint32_t thisPacketTotalSize = dataSerialized.length() + sizeof(uint8_t);
        uint8_t* netData = (uint8_t*)malloc(thisPacketTotalSize);
        netData[0] = KATIPO_NET_TYPE_GET_RESPONSE_TO_CLIENT_FROM_HOST;
        memcpy(&(netData[1]), ((uint8_t*)(&dataSerialized[0])), dataSerialized.length());
        
        ClientNetInterfaceInput input;
        input.data = netData;
        input.dataLength = thisPacketTotalSize;
        input.reliable = true;
        input.channelID = ((++sendChannelIndex) % CLIENT_MAX_SIMULTANEOUS_DOWNLOADS);
        inputQueue->push(input);
        
    }
    
    trackerDataToSecureTable->release();
}

void ClientNetInterface::sendData(uint8_t type, const void * data, size_t dataLength, bool reliable)
{
    if(!enetPeer || !enetClient || disconnected)
    {
        MJLog("Attempt to send data with no connection.");
        return;
    }
    
    uint8_t* netData = (uint8_t*)malloc(dataLength + sizeof(uint8_t));
    netData[0] = type;
    
    memcpy(&(netData[1]), data, dataLength);
    
    ClientNetInterfaceInput input;
    input.data = netData;
    input.dataLength = dataLength;
    input.reliable = true;
    input.channelID = ((++sendChannelIndex) % CLIENT_MAX_SIMULTANEOUS_DOWNLOADS);
    inputQueue->push(input);
}
