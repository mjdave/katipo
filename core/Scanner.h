
#ifndef __Scanner__
#define __Scanner__

#include <vector>
#include <string>
#include "TuiScript.h"
#include "NetConstants.h"

struct ScannerConnection {
    std::string trackerPublicKey;
    ENetHost* enetClient = nullptr;
    ENetPeer* enetPeer = nullptr;
};

class Scanner {
    
public:
    
    TuiFunction* callbackFunction = nullptr;
    
    std::vector<std::string> scanIPs;
    bool complete = false;
    int scanIndex = 0;
    std::map<std::string, ScannerConnection> currentlyTestingConnectionsByIP;
    
    std::map<std::string, ScannerConnection> validConnectionsByIP;
    
public:
    Scanner();
    ~Scanner();
    
    void startScan(TuiFunction* callbackFunction_);
    void cleanupPreviousScan();
    
    void update();
    ScannerConnection getConnection(std::string ip); //caller is responsible for closing the returned connection
    
private:
    
    std::set<std::string> completedIPsToRemove;
    
    void handleReceivedData(std::string ip, ENetEvent& event);
    
};


#endif
