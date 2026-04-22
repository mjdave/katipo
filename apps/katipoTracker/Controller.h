
#ifndef Controller_h
#define Controller_h

#include "TuiScript.h"
#include "Tracker.h"
//#include "ThreadSafeQueue.h"


class Controller {
public:
    bool needsToExit = false;
    
    Tracker* tracker;

    TuiTable* katipoTable;
    
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
