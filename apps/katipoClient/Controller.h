
#ifndef Controller_h
#define Controller_h

#include "TuiScript.h"
#include <thread>

class ClientNetInterface;

class Controller {
public:
    bool needsToExit = false;
    std::thread* thread = nullptr;
    
    TuiTable* rootTable;
    TuiTable* scriptState;
    
    TuiTable* clientInfo;
    std::map<std::string, ClientNetInterface*> netInterfaces;

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
