
#ifndef __ClientNetInterface__
#define __ClientNetInterface__

#include <stdio.h>
#include <vector>
#include <string>
#include "NetConstants.h"
#include "ThreadSafeQueue.h"

struct ClientNetInterfaceInput {
    uint8_t*  data;
    size_t dataLength;
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

private:
    ENetHost* enetClient;
    ENetPeer* enetPeer;
    
    ThreadSafeQueue<ClientNetInterfaceInput>* inputQueue;
    ThreadSafeQueue<ClientNetInterfaceOutput>* outputQueue;
    
    TuiTable* clientInfo;
    
    uint32_t functionCallbackIDCounter = 0;
    std::map<uint32_t, TuiFunction*> callbacksByID;
    
    std::map<uint8_t, std::string> inProgressMultiPartDownloadsByChannel;
    
    std::thread* thread;
    
    bool needsToExit = false;
    bool connected = false;
    
    std::map<std::string, TuiFunction*> registeredFunctions;
    
public:
    ClientNetInterface(std::string host_,
                       std::string port_,
                       TuiTable* clientInfo_);
    ~ClientNetInterface();
    
    void connect();
    void disconnect();
    
    void callServerFunction(std::string functionName, TuiTable* args, TuiFunction* callback);
    TuiTable* bindTui(TuiTable* rootTable);
    
    void pollNetEvents();
    
    void sendData(uint8_t type, const void * data = NULL, size_t dataLength = 0, bool reliable = true);
    
protected:
    
private:
    void startThread();
    void checkEnetEvents();
    
};


#endif // !__ClientNetInterface__
