#include "Controller.h"
#include "Timer.h"
#include "Server.h"
#include "TuiFileUtils.h"

void Controller::init(std::string basePath)
{
    TuiTable* rootTable = Tui::createRootTable();
    
    MJLog("run from path:%s", basePath.c_str());
    hostServer = new Server("hostServer", "3470", 4095, rootTable);
    clientServer = new Server("clientServer", "3471", 4095, rootTable);
    
    if(hostServer->start() && clientServer->start())
    {
        thread = new std::thread(&Controller::serverEventLoop, this);
        
        TuiRef::runScriptFile(Tui::pathByRemovingLastPathComponent(basePath) + "scripts/code.tui", rootTable);
    }
    else
    {
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
