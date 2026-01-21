#ifndef __ServerNetInterface__
#define __ServerNetInterface__

#include <stdio.h>
#include "NetConstants.h"
#include <vector>
#include <queue>
#include <map>
#include <iostream>
#include <unordered_set>
#include "Timer.h"
#include "NetServerClient.h"
#include "ThreadSafeQueue.h"

struct ServerNetInterfaceInput {
    ENetPeer* peer;
    uint8_t*  data;
    size_t dataLength;
    uint8_t channelID = 0;
    bool reliable;
};

enum {
    SERVER_NET_INTERFACE_OUTPUT_ADD_CLIENT,
    SERVER_NET_INTERFACE_OUTPUT_REMOVE_CLIENT,
    SERVER_NET_INTERFACE_OUTPUT_DATA_RECEIEVED,
};

struct ServerNetInterfaceOutput {
    uint8_t outputType;
    NetServerClient* client; //might be null, in the case of a join get-info request
    ENetPeer* enetPeer;
    ServerData serverData;
};

class Server;

class ServerNetInterface {
    ENetHost* enetServer;
    
    ThreadSafeQueue<ServerNetInterfaceInput>* inputQueue;
    ThreadSafeQueue<ServerNetInterfaceOutput>* outputQueue;
    
    std::map<ENetPeer*, NetServerClient*> connectedClientsByEnetPeer;
    std::map<std::string, NetServerClient*> connectedClientsByClientID;
    
    std::string logPath;
    
    
    
public:
    ServerNetInterface(const std::string& publicKey_,
                       const std::string& secretKey_,
                       Server* server,
                       int portNumber,
                       int maxConnections,
                       std::string logPath_);
    ~ServerNetInterface();
    
    
    void update(double dt);
    
    void sendData(uint8_t type,
                  const void * data,
                  size_t dataLength,
                  ENetPeer* peer,
                  bool reliable);
    
    void sendLargeData(uint8_t type,
                  const void * data,
                  size_t dataLength,
                  ENetPeer* peer,
                  uint8_t channel);
    
    TuiTable* getDecryptedDataTable(TuiTable* tuiDataWrapper);
    
public:
    bool valid = false;
    
private:
    Server* server;
    std::thread* thread;
    std::string publicKey;
    std::string secretKey;
    
    bool needsToExit = false;
    void sendInitialHandshake(ENetPeer* peer);
    
protected:
    void disconnect();
    
private:
    void startThread();
    void checkEnetEvents();
    bool checkClientAuthorized(uint16_t clientVersion,
                               std::string* rejectionReason,
                               std::string* rejectionContext);
    void sendJoinRejectionAndDisconnect(ENetPeer* peer,
                                        std::string rejectionReason,
                                        std::string rejectionContext);
    
    
};

#endif // !__ServerNetInterface__
