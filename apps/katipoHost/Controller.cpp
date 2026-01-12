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

//#define TRACKER_IP "127.0.0.1"
//#define TRACKER_PORT "3470"

void Controller::init(int argc, const char * argv[])
{
    std::string basePath = Tui::pathByRemovingLastPathComponent(argv[0]);

    rootTable = Tui::createRootTable();
    TuiTable* launchArgsTable = new TuiTable(rootTable);
    rootTable->set("launchArgs", launchArgsTable);
    launchArgsTable->release();
    
    katipoTable = new TuiTable(rootTable);
    rootTable->set("katipo", katipoTable);
    katipoTable->release();

    /*katipoTable->setString("trackerIP", TRACKER_IP);
    katipoTable->setString("trackerPort", TRACKER_PORT);
    katipoTable->setString("basePath", basePath);
    
    if(argc < 2)
    {
        MJError("katipoHost requires a site path, pointing to a directory which contains a site.tui file");
        MJLog("Usage: ./katipoHost exampleSites/basicSite");
        MJLog("       ./katipoHost --help");
    }
    
    std::string sitePath = argv[1];*/
    
    for(int i = 1; i < argc; i++)
    {
        launchArgsTable->arrayObjects.push_back(new TuiString(argv[i]));

        std::string arg = argv[i];
        if(arg == "--basePath") //base path is where we find scripts/code.tui. Allows the same executable to run different environments
        {
            if(i+1 >= argc)
            {
                MJError("missing basePath path argument. usage example: ./katipoHost --basePath BASE_PATH");
                exit(1);
            }
            basePath = argv[++i];
            launchArgsTable->arrayObjects.push_back(new TuiString(argv[i]));
        }
    }
    
    //katipoTable->setString("sitePath", sitePath);
    
    //todo generate and save/load unique names and ids
    /*TuiTable* clientInfo = new TuiTable(nullptr);
    katipoTable->setTable("clientInfo", clientInfo);
    clientInfo->setString("name", "Host");
    clientInfo->setString("clientID", "2234567812345678"); //should be the public key
    clientInfo->release();*/

    katipoTable->setFunction("init", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(!trackerNetInterface)
        {
            trackerNetInterface = new ClientNetInterface(katipoTable->get("trackerIP")->getStringValue(),
                                                    katipoTable->get("trackerPort")->getStringValue(),
                                                 katipoTable->getTable("clientInfo"));
            trackerNetInterface->bindTui(katipoTable);
            katipoTable->set("tracker", trackerNetInterface->stateTable);

    
            thread = new std::thread(&Controller::serverEventLoop, this);

            return TUI_TRUE;
        }
        return nullptr;
    });
    
    scriptState = (TuiTable*)TuiRef::runScriptFile(Tui::pathByAppendingPathComponent(basePath,"scripts/code.tui"), rootTable);
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

