
#include "ServerNetInterface.h"
#include "ThreadSafeQueue.h"
//#include "sha1.h"
#include "Server.h"
#include "Timer.h"
#include "MJVersion.h"

ServerNetInterface::ServerNetInterface(Server* server_,
                                       int portNumber,
                                       int maxConnections,
                                       std::string logPath_) //todo logPath not implemented
{
    server = server_;
    logPath = logPath_;
    
    inputQueue = new ThreadSafeQueue<ServerNetInterfaceInput>();
    outputQueue = new ThreadSafeQueue<ServerNetInterfaceOutput>();
    
    enet_initialize(); //todo once per process?
    
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    
    address.port = portNumber;
    
    
    enetServer = enet_host_create (& address /* the address to bind the server host to */,
                                   maxConnections      /* allow up to 32 clients and/or outgoing connections */,
                                   0      /* allow up to 2 channels to be used, 0 and 1 */,
                                   0      /* assume any amount of incoming bandwidth */,
                                   0      /* assume any amount of outgoing bandwidth */);
    
    if(!enetServer)
    {
        MJError("Failed to start server. Port %d may be in use?", portNumber);
        return;
    }
    
    
    enet_host_compress_with_range_coder(enetServer);
    
    thread = new std::thread(&ServerNetInterface::startThread, this);
    valid = true;
}

ServerNetInterface::~ServerNetInterface()
{
    needsToExit = true;
    if(valid)
    {
        thread->join();
        disconnect();
    }
}

#define GOAL_TIME_PER_UPDATE 0.01

void ServerNetInterface::startThread()
{
    Timer* timer = new Timer();
    Timer* pingTimer = new Timer();
    double pingTimerAccumulation = 0.0;
    double timeBetweenPings = 1.0;
    
    while(1)
    {
        while(!inputQueue->empty())
        {
            ServerNetInterfaceInput input;
            inputQueue->pop(input);
            
            ENetPacket * packet = enet_packet_create (input.data,
                                                      input.dataLength + sizeof(uint8_t),
                                                      (input.reliable ? ENET_PACKET_FLAG_RELIABLE : 0));
            enet_peer_send(input.peer, input.channelID, packet);
            
            free(input.data);
        }
        
        checkEnetEvents();
        
        if(needsToExit)
        {
            return;
        }
        
        double pingTimeElapsed = pingTimer->getDt();
        pingTimerAccumulation = pingTimerAccumulation + pingTimeElapsed;
        if(pingTimerAccumulation >= timeBetweenPings)
        {
            pingTimerAccumulation = 0.0;
            /*for(auto& peerAndClient : connectedClientsByEnetPeer)
            {
                ClientPingTimer* pingTimer = &pingTimersByClientID[peerAndClient.second->clientID];
                uint32_t pingIndex = pingTimer->pingIndex++;
                pingTimer->timers[pingIndex] = new Timer();
                
                int dataSize = sizeof(uint32_t) + sizeof(uint8_t);
                uint8_t* netData = (uint8_t*)malloc(dataSize);
                netData[0] = SERVER_DATA_TYPE_SERVER_DELAY_PING;
                memcpy(&(netData[1]), &(pingIndex), sizeof(uint32_t));
                
                ENetPacket * packet = enet_packet_create(netData,
                                                          dataSize,
                                                          ENET_PACKET_FLAG_RELIABLE);
                //MJLog("send ping")
                enet_peer_send(peerAndClient.first, 0, packet);
                free(netData);
                
                pingTimer->smoothedPingDelay = mix(pingTimer->smoothedPingDelay, pingTimer->pingDelay, 0.2);
                pingTimer->pingDelay = pingTimer->pingDelay + 1.0;
                
                ServerNetInterfaceOutput output;
                output.outputType = SERVER_NET_INTERFACE_OUTPUT_PING_UPDATE;
                output.client = peerAndClient.second;
                
                //output.serverData.type = SERVER_NET_INTERFACE_OUTPUT_PING_UPDATE;
                output.serverData.length = sizeof(double);
                output.serverData.data = malloc(output.serverData.length);
                memcpy(output.serverData.data, &pingTimer->smoothedPingDelay, output.serverData.length);
                
                outputQueue->push(output);
                
            }*/
            
         //   uint32_t pingIndex;
        }
        
        double timeElapsed = timer->getDt();
        if(timeElapsed < GOAL_TIME_PER_UPDATE)
        {
            std::this_thread::sleep_for(std::chrono::duration<double>(GOAL_TIME_PER_UPDATE - timeElapsed));
        }
    }
    
}

void ServerNetInterface::disconnect()
{
    for(auto& peerAndClient : connectedClientsByEnetPeer)
    {
        enet_peer_disconnect_later(peerAndClient.first, 0);
    }
    
    ENetEvent event;
    while (!connectedClientsByEnetPeer.empty() && enet_host_service (enetServer, &event, 3000) > 0)
    {
        switch (event.type)
        {
            case ENET_EVENT_TYPE_RECEIVE:
                enet_packet_destroy (event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
            {
                ENetPeer* peer = event.peer;
                if(connectedClientsByEnetPeer.count(peer) != 0)
                {
                    NetServerClient* client = connectedClientsByEnetPeer[peer];
                    
                    server->removeClient(client->clientID);
                    connectedClientsByClientID.erase(client->clientID);
                    connectedClientsByEnetPeer.erase(peer);
                    
                    delete client;
                }
            }
                break;
            default:
                break;
        }
    }
    if(!connectedClientsByEnetPeer.empty())
    {
        for(auto& peerAndClient : connectedClientsByEnetPeer)
        {
            enet_peer_reset(peerAndClient.first);
            NetServerClient* client = connectedClientsByEnetPeer[peerAndClient.first];
            server->removeClient(client->clientID);
        }
    }
    
    connectedClientsByEnetPeer.clear();
    connectedClientsByClientID.clear();
    
    
    for(auto& peerAndTimerInfo : pingTimersByClientID)
    {
        for(auto& timerIDAndTimer : peerAndTimerInfo.second.timers)
        {
            delete timerIDAndTimer.second;
        }
    }
    pingTimersByClientID.clear();
    
    enet_host_destroy(enetServer);
    enetServer = nullptr;
    enet_deinitialize(); //todo once per process?
}

bool ServerNetInterface::checkClientAuthorized(uint16_t clientVersion,
                                               std::string* rejectionReason,
                                               std::string* rejectionContext)
{
    if(clientVersion != NETWORK_COMPATIBILITY_VERSION)
    {
        *rejectionContext = KATIPO_VERSION;
        if(clientVersion < NETWORK_COMPATIBILITY_VERSION)
        {
            *rejectionReason = "client_too_old";
        }
        else
        {
            *rejectionReason = "client_too_new";
        }
        MJLog("Connecting client version mismatch:%s. (client:%d server:%d)", rejectionReason->c_str(), clientVersion, NETWORK_COMPATIBILITY_VERSION);
        
        return false;
    }
    
    return true;
}

void ServerNetInterface::sendJoinRejectionAndDisconnect(ENetPeer* peer,
                                                        std::string rejectionReason,
                                                        std::string rejectionContext)
{
    MJError("todo");
    /*JoinRejection rejection;
    rejection.reason = rejectionReason;
    rejection.context = rejectionContext;
    std::string outData = serializeObject(rejection);
    
    uint8_t* netData = (uint8_t*)malloc(outData.size() + sizeof(uint8_t));
    netData[0] = SERVER_DATA_TYPE_SERVER_JOIN_RESPONSE_REJECT;
    memcpy(&(netData[1]), outData.data(), outData.size());*/
    
    
    if(peer)
    {
        /*ENetPacket * packet = enet_packet_create (netData,
                                                  outData.size() + sizeof(uint8_t),
                                                  ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer, 0, packet);
        free(netData);*/
        
        enet_peer_disconnect_later(peer, 0);
    }
}

void ServerNetInterface::checkEnetEvents()
{
    ENetEvent event;
    while(enetServer && enet_host_service(enetServer, &event, 0) > 0)
    {
        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                MJLog("enet peer connected");

                //enet_peer_throttle_configure(event.peer, 5000, 2, 1);
            }
                break;
            case ENET_EVENT_TYPE_RECEIVE:
            {
                if(event.packet->dataLength < 1)
                {
                    MJError("packet too small");
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
                    
                    //MJLog("server receive: %d -  %zu bytes", incoming.type, incoming.length);
                    
                    if(incoming.type == SERVER_DATA_TYPE_CLIENT_JOIN_REQUEST)
                    {
                        int length = 0;
                        TuiTable* joinRequest = (TuiTable*)TuiRef::loadBinaryString((const char*)incoming.data, &length, nullptr);
                        
                        MJLog("Got connection request:");
                        joinRequest->debugLog();
                        
                        if(!joinRequest->hasKey("clientInfo"))
                        {
                            MJError("joinRequest missing clientInfo");
                            continue;
                        }
                        
                        
                        bool rejected = false;
                        std::string rejectionReason = "unknown error";
                        std::string rejectionContext = "";
                        
                        if(!rejected)
                        {
                            NetServerClient* client = new NetServerClient(joinRequest,
                                                                          this,
                                                                          event.peer);
                            
                            if(client->valid)
                            {
                                connectedClientsByEnetPeer[event.peer] = client;
                                connectedClientsByClientID[client->clientID] = client;
                                
                                ServerNetInterfaceOutput output;
                                output.outputType = SERVER_NET_INTERFACE_OUTPUT_ADD_CLIENT;
                                output.client = client;
                                output.enetPeer = event.peer;
                                outputQueue->push(output);
                            }
                            else
                            {
                                rejected = true;
                                rejectionReason = "invalid_clientID";
                                MJLog("Client rejected.");
                            }
                        }
                        
                        if(rejected && !needsToExit)
                        {
                            sendJoinRejectionAndDisconnect(event.peer,
                                                           rejectionReason,
                                                           rejectionContext);
                        }
                    }
                    else if(incoming.type == SERVER_DATA_TYPE_SERVER_DELAY_PING)
                    {
                        if(connectedClientsByEnetPeer.count(event.peer) != 0)
                        {
                            NetServerClient* client = connectedClientsByEnetPeer[event.peer];
                            ClientPingTimer* pingTimer = &pingTimersByClientID[client->clientID];
                            uint32_t pingIndex = 0;
                            memcpy(&pingIndex, incoming.data, sizeof(uint32_t));
                            if(pingTimer->timers.count(pingIndex) != 0)
                            {
                                Timer* timer = pingTimer->timers[pingIndex];
                                double dt = timer->getElapsed();
                                
                                pingTimer->pingDelay = dt;
                                
                                //MJLog("got ping:%.2f", pingTimer->pingDelay)
                                
                                delete timer;
                                pingTimer->timers.erase(pingIndex);
                            }
                        }
                    }
                    else if(incoming.type == SERVER_DATA_TYPE_CLIENT_SERVER_DOWNLOAD_FILE_COMPLETE_NOTIFICATION)
                    {
                        if(connectedClientsByEnetPeer.count(event.peer) != 0)
                        {
                            NetServerClient* client = connectedClientsByEnetPeer[event.peer];
                            uint8_t channelIndex = *((uint8_t*)incoming.data);
                            client->inUseChannels.erase(channelIndex);
                        }
                    }
                    else if(connectedClientsByEnetPeer.count(event.peer) != 0)
                    {
                        NetServerClient* client = connectedClientsByEnetPeer[event.peer];
                        ServerNetInterfaceOutput output;
                        
                        output.outputType = SERVER_NET_INTERFACE_OUTPUT_DATA_RECEIEVED;
                        output.client = client;
                        
                        output.serverData.type = incoming.type;
                        output.serverData.data = malloc(incoming.length);
                        memcpy(output.serverData.data, incoming.data, incoming.length);
                        output.serverData.length = incoming.length;
                        
                        outputQueue->push(output);
                    }
                }
                enet_packet_destroy (event.packet);
            }
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
            {
                ENetPeer* peer = event.peer;
                peer->data = NULL;
                
                if(connectedClientsByEnetPeer.count(peer) != 0)
                {
                    NetServerClient* client = connectedClientsByEnetPeer[peer];
                    
                    connectedClientsByClientID.erase(client->clientID);
                    connectedClientsByEnetPeer.erase(peer);
                    
                    if(pingTimersByClientID.count(client->clientID) != 0)
                    {
                        for(auto& timerIDAndTimer : pingTimersByClientID[client->clientID].timers)
                        {
                            delete timerIDAndTimer.second;
                        }
                        pingTimersByClientID.erase(client->clientID);
                    }
                    
                    ServerNetInterfaceOutput output;
                    output.outputType = SERVER_NET_INTERFACE_OUTPUT_REMOVE_CLIENT;
                    output.client = client;
                    outputQueue->push(output);
                }
                
                MJLog("enet peer disconnect");
            }
                break;
            default:
                MJLog("hmm");
                break;
        }
    }
}


void ServerNetInterface::update(double dt)
{
    while(!outputQueue->empty())
    {
        ServerNetInterfaceOutput output;
        outputQueue->pop(output);
#if LOG_NETWORK
        if(output.client && output.outputType != SERVER_NET_INTERFACE_OUTPUT_ADD_CLIENT)
        {
            MJLog("server receive: %d - to %s - %zu bytes", output.outputType, output.client->clientID.c_str(), output.serverData.length);
        }
#endif
        
        switch(output.outputType)
        {
            case SERVER_NET_INTERFACE_OUTPUT_ADD_CLIENT:
            {
                server->addClient(output.client);
            }
                break;
            case SERVER_NET_INTERFACE_OUTPUT_REMOVE_CLIENT:
            {
                server->removeClient(output.client->clientID);
                delete output.client;
            }
                break;
            case SERVER_NET_INTERFACE_OUTPUT_DATA_RECEIEVED:
            {
                server->clientDataReceived(output.client, output.serverData);
                //free(output.serverData.data); //no, handled in clientDataReceived
            }
                break;
            case SERVER_NET_INTERFACE_OUTPUT_GET_JOIN_INFO:
            {
                server->clientJoinGetInfoReceived(output.enetPeer, 0, output.serverData);
                free(output.serverData.data);
            }
                break;
            case SERVER_NET_INTERFACE_OUTPUT_PING_UPDATE:
            {
                double pingDelay = 0.0;
                memcpy(&pingDelay, output.serverData.data, sizeof(double));
                
                //MJLog("setting final ping delay:%.2f", pingDelay)
                
                output.client->pingDelay = clamp(pingDelay, 0.0, 60.0);
                free(output.serverData.data);
            }
                break;
        }
    }
    
}

void ServerNetInterface::sendData(uint8_t type,
                                  const void * data,
                                  size_t dataLength,
                                  ENetPeer* peer,
                                  bool reliable)
{
    uint8_t* netData = (uint8_t*)malloc(dataLength + sizeof(uint8_t));
    netData[0] = type;
    memcpy(&(netData[1]), data, dataLength);
    
    if(peer)
    {
        ServerNetInterfaceInput input;
        input.data = netData;
        input.peer = peer;
        input.dataLength = dataLength;
        input.reliable = reliable;
        inputQueue->push(input);
    }
    
}

//todo
// keep per client sets of in use channels
// when client returns a packet with a new type TYPE, clear the in use state


void ServerNetInterface::sendLargeData(uint8_t type,
    const void * data,
    size_t dataLength,
    ENetPeer* peer,
    uint8_t channel)
{
    if(dataLength > MJMaxPacketSize)
    {
        uint32_t bytesToSend = dataLength;
        uint32_t dataStartOffset = 0;
        while(bytesToSend > 0)
        {
            uint32_t additionalHeaderSize = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t);
            uint32_t thisPacketLoadBytesToSend = min(bytesToSend, MJMaxPacketSize);
            uint32_t thisPacketTotalSize = thisPacketLoadBytesToSend + sizeof(uint8_t) + additionalHeaderSize;
            uint8_t* netData = (uint8_t*)malloc(thisPacketTotalSize);
            netData[0] = SERVER_DATA_TYPE_SERVER_MULTIPART_DOWNLOAD_RESPONSE;
            netData[1] = type;
            
            memcpy(&(netData[2]), &dataLength, sizeof(uint32_t));
            memcpy(&(netData[6]), &dataStartOffset, sizeof(uint32_t));
            
            memcpy(&(netData[10]), ((uint8_t*)data + dataStartOffset), thisPacketLoadBytesToSend);
            
            dataStartOffset += thisPacketLoadBytesToSend;
            bytesToSend -= thisPacketLoadBytesToSend;
            
            ServerNetInterfaceInput input;
            input.data = netData;
            input.peer = peer;
            input.dataLength = thisPacketLoadBytesToSend + additionalHeaderSize;
            input.reliable = true;
            input.channelID = channel;
            inputQueue->push(input);
        }
    }
    else
    {
        uint8_t* netData = (uint8_t*)malloc(dataLength + sizeof(uint8_t));
        netData[0] = type;
        
        memcpy(&(netData[1]), data, dataLength);
        
        ServerNetInterfaceInput input;
        input.data = netData;
        input.peer = peer;
        input.dataLength = dataLength;
        input.reliable = true;
        input.channelID = channel;
        inputQueue->push(input);
    }
}
