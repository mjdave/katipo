
#ifndef __ClientNetInterface__
#define __ClientNetInterface__

#include <stdio.h>
#include <vector>
#include <string>
#include "NetConstants.h"
#include "ThreadSafeQueue.h"
#include "TuiScript.h"

struct ClientNetInterfaceInput {
    uint8_t*  data;
    size_t dataLength;
    uint8_t channelID = 0;
    bool reliable;
};

enum {
    CLIENT_NET_INTERFACE_OUTPUT_CLIENT_REJECTED,
    CLIENT_NET_INTERFACE_OUTPUT_CLIENT_DISCONNECTED,
    CLIENT_NET_INTERFACE_OUTPUT_DATA_RECEIEVED,
};

struct ClientNetInterfaceOutput {
    uint8_t outputType;
    ServerData serverData;
};

class ClientNetInterface {
public:
	std::string host;
    std::string port;
    
    TuiTable* stateTable;
    TuiTable* katipoTable;
    bool connected = false;
    bool disconnected = false; //true after a disconnect event

private:
    ENetHost* enetClient;
    ENetPeer* enetPeer;
    
    ThreadSafeQueue<ClientNetInterfaceInput>* inputQueue;
    ThreadSafeQueue<ClientNetInterfaceOutput>* outputQueue;
    
    std::string savedPermanentPublicKey;
    std::string savedPermanentSecretKey;
    
    std::string sessionTransientPublicKey;
    std::string sessionTransientSecretKey;
    
    std::string trackerPublicKey;
    
    TuiTable* initialData = nullptr;
    
    uint32_t functionCallbackIDCounter = 0;
    std::map<uint32_t, TuiFunction*> callbacksByID;
    
    std::map<uint8_t, std::string> inProgressMultiPartDownloadsByChannel; //downloading from remote
    
    std::set<uint8_t> inUseChannels; //uploading to remote
    
    std::thread* thread;
    
    bool needsToExit = false;
    
    //std::map<std::string, TuiFunction*> registeredFunctions;
    
public:
    ClientNetInterface(std::string host_,
                       std::string port_,
                       const std::string& savedPermanentPublicKey_,
                       const std::string& savedPermanentSecretKey_,
                       TuiTable* initialData_ = nullptr);
    ~ClientNetInterface();
    
    void connect();
    void disconnect();
    
    void callTrackerFunction(TuiTable* args);  //function name is assumed first arg, callback is last
    void callRemoteHostFunction(std::string hostPublicKey, TuiTable* args); //function name is assumed first arg, callback is last
    
    TuiTable* bindTui(TuiTable* katipoTable);
    
    void pollNetEvents();
    
    void sendData(uint8_t type, const void * data = NULL, size_t dataLength = 0, bool reliable = true);
    //void sendLargeData(uint8_t type, const void * data, size_t dataLength = 0, bool reliable = true);
    
protected:
    
private:
    void startThread();
    void checkEnetEvents();
    
    void sendLargeDataInternal(uint8_t type,
                               const void * data,
                               size_t dataLength,
                               uint8_t channel);
    
    
    TuiTable* getTrackerEncryptedDataTable(TuiTable* dataToSecure);
    TuiTable* getHostOrClientEncryptedDataTable(std::string hostPublicKey, TuiTable* dataToSecure);
    
    TuiTable* getDecryptedDataTable(TuiTable* tuiDataWrapper, bool useTransientSessionKey);
    
    
    void processGetRequest(TuiTable* tuiData);
    
};


#endif // !__ClientNetInterface__
