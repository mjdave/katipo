
#ifndef Controller_h
#define Controller_h

//COMPILE_WITH_HTTP_INTERFACE allows hosts to send http get requests, but requires curl. Also needs to be defined in katipoHost/CMakeLists.txt
#define COMPILE_WITH_HTTP_INTERFACE 0

#include "TuiScript.h"
#include <thread>

class ClientNetInterface;
class DatabaseEnvironment;
class Database;

#if COMPILE_WITH_HTTP_INTERFACE
#include "ClientHttpInterface.h"
#endif

class Controller {
public:
    bool needsToExit = false;
    std::thread* thread = nullptr;
    
    DatabaseEnvironment* databaseEnvironment = nullptr;
    Database* database = nullptr;
    
    ClientNetInterface* trackerNetInterface = nullptr;
    std::string currentHostNameKey;
    
#if COMPILE_WITH_HTTP_INTERFACE
    ClientHttpInterface* httpInterface = nullptr;
#endif
    
    TuiTable* rootTable = nullptr;
    TuiTable* katipoTable = nullptr;
    TuiTable* scriptState = nullptr;

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
