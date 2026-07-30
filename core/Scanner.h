
#ifndef __Scanner__
#define __Scanner__

#ifdef _MSC_VER
#define _WINSOCKAPI_    // stops windows.h including winsock.h
#include <windows.h>
#include <direct.h>
#include <cstdint>
#endif

#include <vector>
#include <string>
#include <set>
#include "TuiScript.h"
#include "NetConstants.h"

class ClientNetInterface;

struct ScannerConnection {
    ClientNetInterface* netInterface = nullptr;
    std::string trackerPublicKey;
    ENetHost* enetClient = nullptr;
    ENetPeer* enetPeer = nullptr;
};

class Scanner {
    
public:
    
    std::string publicKey = "";
    std::string secretKey = "";
    TuiTable* katipoTable;
    
    std::string broadcastKey;
    TuiFunction* callbackFunction = nullptr;
    
    std::vector<std::string> scanIPs;
    bool complete = false;
    bool hasTriedAgain = false;
    int scanIndex = 0;
    
    std::map<std::string, ScannerConnection> connectionsByIP;
    
public:
    Scanner(std::string& publicKey, std::string& secretKey, TuiTable* katipoTable_);
    ~Scanner();
    
    void startScan(std::string broadcastKey_, TuiFunction* callbackFunction_);
    void cleanupPreviousScan();
    
    void update();
    ScannerConnection getConnection(std::string ip); //caller is responsible for closing the returned connection
    
private:
    
    std::set<std::string> completedIPsToRemove;
    
    void handleReceivedData(std::string ip, ENetEvent& event);
    
};


#endif
