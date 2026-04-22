
#ifndef __Scanner__
#define __Scanner__

#include <vector>
#include <string>
#include "TuiScript.h"
#include "NetConstants.h"

struct ScannerConnection {
    ENetHost* enetClient = nullptr;
    ENetPeer* enetPeer = nullptr;
};

class Scanner {
    
public:
    
    std::vector<std::string> scanIPs;
    int scanIndex = 0;
    std::map<std::string, ScannerConnection> currentlyTestingConnectionsByIP;
    
    std::map<std::string, ScannerConnection> validConnectionsByIP;
    
public:
    Scanner();
    ~Scanner();
    
    void update();
    
};


#endif
