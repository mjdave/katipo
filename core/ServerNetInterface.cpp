
#include "ServerNetInterface.h"
#include "ThreadSafeQueue.h"
//#include "sha1.h"
#include "Server.h"
#include "Timer.h"
#include "sodium.h"

ServerNetInterface::ServerNetInterface(const std::string& publicKey_,
                                       const std::string& secretKey_,
                                       Server* server_,
                                       int portNumber,
                                       int maxConnections,
                                       std::string logPath_) //todo logPath not implemented
{
    publicKey = publicKey_;
    secretKey = secretKey_;
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
    
    
    //enet_host_compress_with_range_coder(enetServer);
    
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
    
    
    enet_host_destroy(enetServer);
    enetServer = nullptr;
    enet_deinitialize(); //todo once per process?
}

void ServerNetInterface::sendJoinRejectionAndDisconnect(ENetPeer* peer,
                                                        std::string rejectionReason,
                                                        std::string rejectionContext)
{
    MJError("todo sendJoinRejectionAndDisconnect");
    /*JoinRejection rejection;
    rejection.reason = rejectionReason;
    rejection.context = rejectionContext;
    std::string outData = serializeObject(rejection);
    
    uint8_t* netData = (uint8_t*)malloc(outData.size() + sizeof(uint8_t));
    netData[0] = KATIPO_NET_TYPE_SERVER_JOIN_RESPONSE_REJECT;
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


void ServerNetInterface::sendInitialHandshake(ENetPeer* peer)
{
    if(peer)
    {
        const std::string& outData = publicKey;
        uint8_t* netData = (uint8_t*)malloc(outData.size() + sizeof(uint8_t));
        netData[0] = KATIPO_NET_TYPE_INITIAL_HANDSHAKE;
        memcpy(&(netData[1]), outData.data(), outData.size());
        
        ENetPacket * packet = enet_packet_create (netData,
                                                  outData.size() + sizeof(uint8_t),
                                                  ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer, 0, packet);
        free(netData);
    }
}


TuiTable* ServerNetInterface::getDecryptedDataTable(TuiTable* tuiDataWrapper)
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
                                     (unsigned char*)&(secretKey[0])) != 0)
            {
                MJError("attempt failed to decrypt in KATIPO_NET_TYPE_CLIENT_SERVER_FUNCTION_CALL_REQUEST");
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

void ServerNetInterface::checkEnetEvents()
{
    ENetEvent event;
    while(enetServer && enet_host_service(enetServer, &event, 0) > 0)
    {
        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                MJLog("Tracker: initial connection established.\n");
                
                sendInitialHandshake(event.peer);

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
                    
                    if(incoming.type == KATIPO_NET_TYPE_CLIENT_JOIN_REQUEST)
                    {
                        int length = 0;
                        TuiTable* tuiDataWrapper = (TuiTable*)TuiRef::loadBinaryString((const char*)incoming.data, &length, nullptr);
                        TuiTable* initalData = getDecryptedDataTable(tuiDataWrapper);
                        
                        NetServerClient* client = new NetServerClient(tuiDataWrapper->getString("publicKey"),
                                                                      this,
                                                                      event.peer,
                                                                      initalData);
                        
                        tuiDataWrapper->release();
                        if(initalData)
                        {
                            initalData->release();
                        }
                        
                        if(client->valid)
                        {
                            if(connectedClientsByClientID.count(client->clientID) != 0)
                            {
                                NetServerClient* clientToRemove = connectedClientsByClientID[client->clientID];
                                
                                connectedClientsByClientID.erase(clientToRemove->clientID);
                                enet_peer_reset(clientToRemove->enetPeer);
                                connectedClientsByEnetPeer.erase(clientToRemove->enetPeer);
                                
                                ServerNetInterfaceOutput output;
                                output.outputType = SERVER_NET_INTERFACE_OUTPUT_REMOVE_CLIENT;
                                output.client = clientToRemove;
                                outputQueue->push(output);
                            }
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
                            MJLog("Client rejected.");
                            sendJoinRejectionAndDisconnect(event.peer,
                                                           "invalid_clientID",
                                                           "");
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
