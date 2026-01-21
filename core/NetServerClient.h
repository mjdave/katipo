
#ifndef NetServerClient_h
#define NetServerClient_h


#include <stdio.h>
#include <queue>
#include <set>
#include "NetConstants.h"
#include "TuiScript.h"
#include "sodium.h"

#define MAX_SIMULTANEOUS_DOWNLOADS 4

static char clientIDBuffer[128];
static inline std::string clientIDForPublicKey(const std::string& publicKey)
{
    return sodium_bin2hex(clientIDBuffer, 128,
                         (unsigned char*)&(publicKey[0]), publicKey.length());
}

class ServerNetInterface;

class NetServerClient {
public:
    ENetPeer* enetPeer = nullptr;
    bool valid = false;
    std::string publicKey;
    std::string clientID;
    
    TuiTable* initialData = nullptr;
    
    double pingDelay = 0.0;
    
    std::set<uint8_t> inUseChannels; //uploading to remote
    std::queue<ServerData> queuedDownloads; //uploading to remote
    
    std::map<uint8_t, std::string> inProgressMultiPartDownloadsByChannel; //downloading from remote
    

public:
    NetServerClient(std::string publicKey_,
                    ServerNetInterface* netInterface_,
                    ENetPeer* enetPeer_,
                    TuiTable* initialData_ = nullptr);
    ~NetServerClient();
    
    virtual void sendDataToClient(const ServerData& serverData, bool reliable);
    virtual void sendLargeDataToClient(const ServerData& serverData);
    
    TuiTable* getEncryptedDataTable(TuiTable* dataToSecure, const std::string& serverPublicKey, const std::string& serverSecretKey);
    
    virtual double getPingDelay();
    
protected:
    
    ServerNetInterface* netInterface;
    

};

#endif /* NetServerClient_h */
