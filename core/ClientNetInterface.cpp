
#include "ClientNetInterface.h"
#include "Timer.h"
#include "TuiFileUtils.h"

ClientNetInterface::ClientNetInterface(std::string host_,
                                       std::string port_,
                                       TuiTable* clientInfo_)
{
    clientInfo = clientInfo_;
    clientInfo->retain();
    host = host_;
    port = port_;
    
    inputQueue = new ThreadSafeQueue<ClientNetInterfaceInput>();
    outputQueue = new ThreadSafeQueue<ClientNetInterfaceOutput>();
    
    connect();
}


ClientNetInterface::~ClientNetInterface()
{
    disconnect();
    delete inputQueue;
    delete outputQueue;
    delete stateTable;
}


void ClientNetInterface::connect()
{
    if(thread)
    {
        return;
    }
    
    enet_initialize();
    
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
        MJLog("enet disconnected");
    }
    else if(!disconnected)
    {
        MJLog("unable to connect");
    }
    
    needsToExit = true;
    connected = false;
    disconnected = true;
    thread->join();
    delete thread;
    thread = nullptr;
    
    for(auto& idAndCallback : callbacksByID)
    {
        idAndCallback.second->call("SERVER_FUNCTION_CALL_RESPONSE", nullptr);
    }
    callbacksByID.clear();
    
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
    
    enet_deinitialize();
}

void ClientNetInterface::callServerFunction(std::string functionName, TuiTable* args, TuiFunction* callback)
{
    TuiTable* sendTable = new TuiTable(nullptr);
    sendTable->setString("name", functionName);
    for(int i = 0; i < args->arrayObjects.size(); i++)
    {
        TuiRef* arg = args->arrayObjects[i];
        arg->retain();
        sendTable->arrayObjects.push_back(arg);
    }
    
    if(callback)
    {
        callbacksByID[functionCallbackIDCounter] = ((TuiFunction*)callback->retain());
        sendTable->setDouble("callbackID", functionCallbackIDCounter++);
    }
    
    std::string dataSerialized = sendTable->serializeBinary();
    sendTable->release();
        
    sendData(SERVER_DATA_TYPE_CLIENT_SERVER_FUNCTION_CALL_REQUEST, (void*)dataSerialized.data(), dataSerialized.length(), true);
        
}

TuiTable* ClientNetInterface::bindTui(TuiTable* rootTable)
{
    stateTable = new TuiTable(rootTable);
    
    
    
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
    
    stateTable->setFunction("register", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
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
    });
    
    // client.callServerFunction(clientID, "playlists", testPlaylists)
    stateTable->setFunction("callServerFunction", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(disconnected)
        {
            TuiParseError(callingDebugInfo->fileName.c_str(), callingDebugInfo->lineNumber, "attempted to callServerFunction, but we have been disconnected");
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
                
                sendData(SERVER_DATA_TYPE_CLIENT_SERVER_FUNCTION_CALL_REQUEST, (void*)dataSerialized.data(), dataSerialized.length(), true);
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
    });
    
    
    // client.downloadFromServer(clientID, "song", arg1, ... , callbackFunction)
    stateTable->setFunction("downloadFromServer", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(disconnected)
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
                
                sendData(SERVER_DATA_TYPE_CLIENT_SERVER_DOWNLOAD_FILE_REQUEST, (void*)dataSerialized.data(), dataSerialized.length(), true);
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
    });
    
    
    // client.disconnect()
    stateTable->setFunction("disconnect", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(!disconnected)
        {
            disconnect();
            if(registeredFunctions.count("disconnected") != 0)
            {
                registeredFunctions["disconnected"]->call("disconnect");
            }
        }
        return nullptr;
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
                MJLog("enet connected\n");
                
                TuiTable* dataTable = new TuiTable(nullptr);
                dataTable->set("clientInfo", clientInfo);
                
                std::string data = dataTable->serializeBinary();
                
                int dataSize = data.length() + sizeof(uint8_t);
                uint8_t* netData = (uint8_t*)malloc(dataSize);
                netData[0] = SERVER_DATA_TYPE_CLIENT_JOIN_REQUEST;
                memcpy(&(netData[1]), data.data(), data.length());
                
                ENetPacket * packet = enet_packet_create(netData,
                                                          dataSize,
                                                          ENET_PACKET_FLAG_RELIABLE);
                
                
                enet_peer_send(enetPeer, 0, packet);
                free(netData);
                dataTable->release();
                
                connected = true;
                
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
                    
                    if(incoming.type == SERVER_DATA_TYPE_SERVER_JOIN_RESPONSE_REJECT)
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
                    else if(incoming.type == SERVER_DATA_TYPE_SERVER_DELAY_PING)
                    {
                        ENetPacket * packet = enet_packet_create(event.packet->data,
                                                                 event.packet->dataLength,
                                                                  ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(enetPeer, 0, packet);
                    }
                    else
                    {
                        bool sendToOutput = true;
                        bool sendDownloadAcknowledge = false;
                        
                        if(incoming.type == SERVER_DATA_TYPE_SERVER_DOWNLOAD_FILE_RESPONSE)
                        {
                            sendDownloadAcknowledge = true;
                        }
                        else if(incoming.type == SERVER_DATA_TYPE_SERVER_MULTIPART_DOWNLOAD_RESPONSE)
                        {
                            sendToOutput = false;
                            
                            uint32_t additionalHeaderSize = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t);
                            
                            uint32_t totalSize = *((uint32_t*)(((uint8_t*)incoming.data) + 1));
                            uint32_t dataStartOffset = *((uint32_t*)(((uint8_t*)incoming.data) + 5));
                            
                            uint32_t recievedPayloadSize = incoming.length - additionalHeaderSize;
                            
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
                                sendDownloadAcknowledge = true;
                                
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
                        }
                        
                        if(sendDownloadAcknowledge)
                        {
                            uint8_t data[2] = {
                                SERVER_DATA_TYPE_CLIENT_SERVER_DOWNLOAD_FILE_COMPLETE_NOTIFICATION,
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

void ClientNetInterface::pollNetEvents()
{
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
                    if(registeredFunctions.count("disconnected") != 0)
                    {
                        registeredFunctions["disconnected"]->call("CLIENT_NET_INTERFACE_OUTPUT_CLIENT_DISCONNECTED");
                    }
                    return;
                }
            }
                break;
            case CLIENT_NET_INTERFACE_OUTPUT_CLIENT_DISCONNECTED:
            {
                disconnect();
                if(registeredFunctions.count("disconnected") != 0)
                {
                    registeredFunctions["disconnected"]->call("CLIENT_NET_INTERFACE_OUTPUT_CLIENT_DISCONNECTED");
                }
                return;
            }
                break;
            case CLIENT_NET_INTERFACE_OUTPUT_DATA_RECEIEVED:
            {
                switch (output.serverData.type) {
                    case SERVER_DATA_TYPE_SERVER_CLIENT_FUNCTION_CALL_REQUEST:
                    {
                        TuiTable* tuiData = (TuiTable*)TuiRef::loadBinaryString(std::string((const char*)output.serverData.data, output.serverData.length)); //todo memcpys
                         TuiString* functionName = (TuiString*)tuiData->objectsByStringKey["name"];
                         if(registeredFunctions.count(functionName->value) != 0)
                         {
                             tuiData->retain();
                             TuiTable* sendArgs = new TuiTable(nullptr);
                             
                             TuiDebugInfo debugInfo;
                             debugInfo.fileName = "FUNCTION_CALL_REQUEST";
                             
                             for(TuiRef* arg : tuiData->arrayObjects)
                             {
                                 sendArgs->push(arg);
                             }
                             
                             TuiFunction* callbackFunction = nullptr;
                             if(tuiData->hasKey("callbackID"))
                             {
                                 callbackFunction = new TuiFunction([this, tuiData](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
                                     TuiTable* sendTable = new TuiTable(nullptr);
                                     sendTable->set("callbackID", tuiData->objectsByStringKey["callbackID"]); //todo don't capture
                                     if(args && args->arrayObjects.size() > 0)
                                     {
                                         sendTable->set("data", args->arrayObjects[0]);
                                     }
                                     
                                     std::string dataSerialized = sendTable->serializeBinary();
                                     sendTable->release();
                                     
                                     sendData(SERVER_DATA_TYPE_CLIENT_FUNCTION_CALL_RESPONSE, dataSerialized.data(), dataSerialized.length(), true);
                                     
                                     return nullptr;
                                 });
                                 sendArgs->push(callbackFunction);
                             }
                             
                             TuiRef* result = registeredFunctions[functionName->value]->call(sendArgs, nullptr, &debugInfo);
                             
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
                             MJError("attempt to call unregistered function:%s", functionName->value.c_str());
                         }
                         tuiData->release();
                    }
                        break;
                    case SERVER_DATA_TYPE_SERVER_FUNCTION_CALL_RESPONSE:
                    {
                        TuiTable* tuiData = (TuiTable*)TuiRef::loadBinaryString(std::string((const char*)output.serverData.data, output.serverData.length)); //todo memcpys
                        uint32_t callbackID = ((TuiNumber*)tuiData->objectsByStringKey["callbackID"])->value;
                        
                        
                        if(callbacksByID.count(callbackID) != 0)
                        {
                            callbacksByID[callbackID]->call("SERVER_FUNCTION_CALL_RESPONSE", tuiData->get("data"));
                        }
                        
                    }
                        break;
                    case SERVER_DATA_TYPE_SERVER_DOWNLOAD_FILE_RESPONSE:
                    {
                        TuiTable* tuiData = (TuiTable*)TuiRef::loadBinaryString(std::string((const char*)output.serverData.data, output.serverData.length)); //todo memcpys
                        uint32_t callbackID = ((TuiNumber*)tuiData->objectsByStringKey["callbackID"])->value;
                        if(callbacksByID.count(callbackID) != 0)
                        {
                            callbacksByID[callbackID]->call("SERVER_DOWNLOAD_FILE_RESPONSE", tuiData->objectsByStringKey["data"]);
                        }
                        
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

void ClientNetInterface::sendData(uint8_t type, const void * data, size_t dataLength, bool reliable)
{
    if(!enetPeer || !enetClient || disconnected)
    {
        MJLog("Attempt to send data with no connection.");
        return;
    }
    
    uint8_t* netData = (uint8_t*)malloc(dataLength + sizeof(uint8_t));
    netData[0] = type;
    if(dataLength > 0)
    {
        memcpy(&(netData[1]), data, dataLength);
    }
    
    //MJLog("send data type:%d length:%d", type, dataLength);
    
    ClientNetInterfaceInput input;
    input.data = netData;
    input.dataLength = dataLength;
    input.reliable = reliable;
    inputQueue->push(input);
}
