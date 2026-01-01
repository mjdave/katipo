#ifdef _MSC_VER
#define _WINSOCKAPI_    // stops windows.h including winsock.h
#define NOMINMAX
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <windows.h>
#endif

#include "Controller.h"
#include "MJVersion.h"
#include "Timer.h"
#include "ClientNetInterface.h"
#include "TuiFileUtils.h"

#define TRACKER_IP "127.0.0.1"
#define TRACKER_PORT "3470"

void Controller::init(int argc, const char * argv[])
{
    rootTable = Tui::createRootTable();
    TuiTable* clientInfo = new TuiTable(nullptr);
    
    std::string trackerIP = TRACKER_IP;
    std::string trackerPort = TRACKER_PORT;
    
    for(int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if(arg == "--trackerIP")
        {
            if(i+1 >= argc)
            {
                MJError("missing tracker IP. usage example: ./katipoTracker --trackerIP %s", TRACKER_IP);
                exit(1);
            }
            trackerIP = argv[++i];
        }
        else if(arg == "--trackerPort")
        {
            if(i+1 >= argc)
            {
                MJError("missing tracker port. usage example: ./katipoTracker --trackerPort %s", TRACKER_PORT);
                exit(1);
            }
            trackerPort = argv[++i];
        }
        else if(arg == "--help")
        {
            MJLog("Katipo Host version:%s\n\
usage: ./katipoTracker --trackerIP %s --trackerPort %s", KATIPO_VERSION, TRACKER_IP, TRACKER_PORT);
            exit(0);
        }
    }

    //todo generate and save/load unique names and ids
    clientInfo->setString("name", "Host");
    clientInfo->setString("clientID", "2234567812345678"); //should be the public key
    
    trackerNetInterface = new ClientNetInterface(trackerIP,
                                                 trackerPort,
                                                 clientInfo);
    
    clientInfo->release();
    
    trackerNetInterface->bindTui(rootTable);
    rootTable->set("tracker", trackerNetInterface->stateTable);
    
    thread = new std::thread(&Controller::serverEventLoop, this);
    
    scriptState = (TuiTable*)TuiRef::runScriptFile(Tui::getResourcePath("scripts/code.tui"), rootTable);
}

static const double SERVER_FIXED_TIME_STEP = 1.0 / 60.0;

void Controller::serverEventLoop()
{
    Timer* timer = new Timer();
    //Timer* deltaTimer = new Timer();
    
    while(1)
    {
        //checkInput();
        
        //double dt = std::clamp(deltaTimer->getDt(), 0.0, 4.0);
        
        trackerNetInterface->pollNetEvents();
        
        if(needsToExit)
        {
            delete timer;
            return;
        }
        
        double timeElapsed = timer->getDt();
        if(timeElapsed < SERVER_FIXED_TIME_STEP)
        {
            std::this_thread::sleep_for(std::chrono::duration<double>(SERVER_FIXED_TIME_STEP - timeElapsed));
        }
    }
}

Controller::Controller()
{
    
}

Controller::~Controller()
{
    rootTable->release();
    scriptState->release();
    delete trackerNetInterface;
}
