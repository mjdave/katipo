
#ifdef _MSC_VER
#define _WINSOCKAPI_    // stops windows.h including winsock.h
#define NOMINMAX
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <windows.h>
#endif

#include "Controller.h"
#include "Timer.h"
#include "Server.h"
#include "TuiFileUtils.h"
#include "MJVersion.h"

void Controller::init(int argc, const char * argv[])
{
    TuiTable* rootTable = Tui::createRootTable();
    
    std::string hostPortString = "3470";
    std::string clientPortString = "3471";
    
    for(int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if(arg == "--hostPort")
        {
            if(i+1 >= argc)
            {
                MJError("missing host port. usage example: ./katipoTracker --hostPort 3470");
                exit(1);
            }
            hostPortString = argv[++i];
        }
        else if(arg == "--clientPort")
        {
            if(i+1 >= argc)
            {
                MJError("missing client port. usage example: ./katipoTracker --clientPort 3471");
                exit(1);
            }
            clientPortString = argv[++i];
        }
        else if(arg == "--help")
        {
            MJLog("Katipo Tracker version:%s\n\
usage: ./katipoTracker --hostPort 3470 --clientPort 3471", KATIPO_VERSION);
            exit(0);
        }
    }
    
    hostServer = new Server("hostServer", hostPortString, 4095, rootTable);
    clientServer = new Server("clientServer", clientPortString, 4095, rootTable);
    
    if(hostServer->start() && clientServer->start())
    {
        thread = new std::thread(&Controller::serverEventLoop, this);
        
        std::string basePath = argv[0];
        
        TuiRef::runScriptFile(Tui::pathByRemovingLastPathComponent(basePath) + "scripts/code.tui", rootTable);
    }
    else
    {
        MJError("Failed to start.");
        exit(1);
    }
    
}

static const double SERVER_FIXED_TIME_STEP = 1.0 / 60.0;

void Controller::serverEventLoop()
{
    Timer* timer = new Timer();
    Timer* deltaTimer = new Timer();
    
    while(1)
    {
        //checkInput();
        
        double dt = glm::clamp(deltaTimer->getDt(), 0.0, 4.0);
        
        hostServer->update(dt);
        clientServer->update(dt);
        
        if(needsToExit)
        {
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
}
