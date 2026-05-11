
#ifndef Controller_h
#define Controller_h

#include "TuiScript.h"
#include "Tracker.h"
//#include "ThreadSafeQueue.h"


class Controller {
public:
    bool needsToExit = false;
    
    Tracker* tracker = nullptr;
    
    TuiTable* rootTable = nullptr;
    TuiTable* katipoTable = nullptr;
    TuiTable* scriptState = nullptr;
    
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
    
    
private:

};

#endif
