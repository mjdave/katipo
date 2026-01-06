
#ifndef NetServerClient_h
#define NetServerClient_h


#include <stdio.h>
#include <queue>
#include <set>
#include "NetConstants.h"
#include "TuiScript.h"

#define MAX_SIMULTANEOUS_DOWNLOADS 4

class ServerNetInterface;

class NetServerClient {
public:
    ENetPeer* enetPeer = nullptr;
    bool valid = false;
    std::string clientID;
    
    TuiTable* joinRequest;
    
    double pingDelay = 0.0;
    
    std::set<uint8_t> inUseChannels; //uploading to remote
    std::queue<ServerData> queuedDownloads; //uploading to remote
    
    std::map<uint8_t, std::string> inProgressMultiPartDownloadsByChannel; //downloading from remote
    

public:
    NetServerClient(TuiTable* joinRequest_,
                    ServerNetInterface* netInterface_,
                    ENetPeer* enetPeer_);
    ~NetServerClient();
    
    virtual void sendDataToClient(const ServerData& serverData, bool reliable);
    virtual void sendLargeDataToClient(const ServerData& serverData);
    
    
    virtual double getPingDelay();
    
protected:
    
    ServerNetInterface* netInterface;
    

};

#endif /* NetServerClient_h */
