
#ifndef Controller_h
#define Controller_h

#include "TuiScript.h"
#include <thread>
//#include "ThreadSafeQueue.h"

class ClientNetInterface;

class Controller {
public:
    bool needsToExit = false;
    std::thread* thread = nullptr;
    
    ClientNetInterface* trackerNetInterface;
    
    TuiTable* rootTable;
    TuiTable* scriptState;
    
    //ThreadSafeQueue<ControllerInput>* inputQueue;
    //ThreadSafeQueue<ServerAppControllerOutput>* outputQueue;

public:
    
    static Controller* getInstance() {
        static Controller* instance = new Controller();
        return instance;
    }
    
    void init(std::string basePath);
    
    Controller();
    ~Controller();
    
private:
    
    void serverEventLoop();
    
private:

};

#endif
