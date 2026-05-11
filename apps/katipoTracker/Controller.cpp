
#ifdef _MSC_VER
#define _WINSOCKAPI_    // stops windows.h including winsock.h
#define NOMINMAX
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <windows.h>
#endif

#include "Controller.h"
#include "Server.h"
#include "TuiFileUtils.h"
#include "MJVersion.h"
#include "sodium.h"
#include "Tracker.h"

void Controller::init(int argc, const char * argv[])
{
    if(sodium_init() < 0) //this is safe to call multiple times
    {
        MJError("Sodium initialization failed. Exiting.");
        abort();
    }
    

    rootTable = Tui::initRootTable();
    
    std::string basePath = Tui::pathByRemovingLastPathComponent(argv[0]);
    
    katipoTable = new TuiTable(rootTable);
    rootTable->set("katipo", katipoTable);
    katipoTable->release();
    
    katipoTable->setString("version", KATIPO_VERSION);
    katipoTable->setString("hostPort", "3470");
    katipoTable->setString("clientPort", "3471");
    
    TuiTable* launchArgsTable = new TuiTable(rootTable);
    rootTable->set("launchArgs", launchArgsTable);
    launchArgsTable->release();
    
    for(int i = 1; i < argc; i++)
    {
        launchArgsTable->arrayObjects.push_back(new TuiString(argv[i]));

        std::string arg = argv[i];
        if(arg == "--basePath")
        {
            if(i+1 >= argc)
            {
                MJError("missing basePath. usage example: ./katipoTracker --basePath %s", basePath.c_str());
                exit(1);
            }
            basePath = argv[++i];
            launchArgsTable->arrayObjects.push_back(new TuiString(argv[i]));
        }
        
    }
    
    if(basePath.back() != '/' && basePath.back() != '\\')
    {
        basePath = basePath + "/";
    }
    katipoTable->setString("basePath", basePath);
    
    tracker = new Tracker(katipoTable);
    
    scriptState = (TuiTable*)TuiRef::runScriptFile(katipoTable->getString("basePath") + "/scripts/code.tui", katipoTable);
    
}


Controller::Controller()
{
    
}

Controller::~Controller()
{
    delete tracker;
    delete scriptState;
    delete rootTable;
}
