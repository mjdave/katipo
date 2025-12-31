
#ifndef Controller_h
#define Controller_h

#include "TuiScript.h"
#include <thread>
//#include "ThreadSafeQueue.h"

class Server;

class Controller {
public:
    bool needsToExit = false;
    std::thread* thread = nullptr;
    
    Server* hostServer;
    Server* clientServer;
    
    //ThreadSafeQueue<ControllerInput>* inputQueue;
    //ThreadSafeQueue<ServerAppControllerOutput>* outputQueue;

public:
    
    static Controller* getInstance() {
        static Controller* instance = new Controller();
        return instance;
    }
    
    void init(int argc, const char * argv[]);
    
    Controller();
    ~Controller();
    
private:
    
    void serverEventLoop();
    
private:

};

#endif
