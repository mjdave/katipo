
#ifndef __Tracker__
#define __Tracker__

#include "TuiScript.h"
#include <thread>


class Server;



class Tracker {
    
    bool needsToExit = false;
    
public:
    
    std::thread* thread = nullptr;
    
    Server* hostServer = nullptr;
    Server* clientServer = nullptr;
    
public:
    Tracker(TuiTable* katipoTable);
    ~Tracker();
    
private:
    void serverEventLoop();
    
};


#endif
