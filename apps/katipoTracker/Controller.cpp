
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
    TuiTable* launchArgsTable = new TuiTable(rootTable);
    rootTable->set("launchArgs", launchArgsTable);
    launchArgsTable->release();
    
    katipoTable = new TuiTable(rootTable);
    rootTable->set("katipo", katipoTable);
    katipoTable->release();

    katipoTable->setString("hostPort", "3470");
    katipoTable->setString("clientPort", "3471");
    
    
    for(int i = 1; i < argc; i++)
    {
        launchArgsTable->arrayObjects.push_back(new TuiString(argv[i]));
    }

    katipoTable->setFunction("init", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(!hostServer)
        {
            hostServer = new Server("hostServer", katipoTable->get("hostPort")->getStringValue(), 4095, katipoTable);
            clientServer = new Server("clientServer", katipoTable->get("clientPort")->getStringValue(), 4095, katipoTable);
            return TUI_TRUE;
        }
        return nullptr;
    });



    katipoTable->setFunction("start", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(hostServer && !hostServer->running)
        {
            if(hostServer->start() && clientServer->start())
            {
                thread = new std::thread(&Controller::serverEventLoop, this);
                return TUI_TRUE;
            }
        }
        return nullptr;
    });

    
    std::string basePath = argv[0];
    TuiRef::runScriptFile(Tui::pathByRemovingLastPathComponent(basePath) + "scripts/code.tui", rootTable);
    
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
