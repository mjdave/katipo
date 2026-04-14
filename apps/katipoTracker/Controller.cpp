
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
#include "sodium.h"

void Controller::init(int argc, const char * argv[])
{
    if(sodium_init() < 0) //this is safe to call multiple times
    {
        MJError("Sodium initialization failed. Exiting.");
        abort();
    }
    
    std::string basePath = Tui::pathByRemovingLastPathComponent(argv[0]);

    TuiTable* rootTable = Tui::getRootTable();
    TuiTable* launchArgsTable = new TuiTable(rootTable);
    rootTable->set("launchArgs", launchArgsTable);
    launchArgsTable->release();
    
    katipoTable = new TuiTable(rootTable);
    rootTable->set("katipo", katipoTable);
    katipoTable->release();
    
    katipoTable->setString("version", KATIPO_VERSION);
    katipoTable->setString("hostPort", "3470");
    katipoTable->setString("clientPort", "3471");
    katipoTable->setString("basePath", basePath);
    
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

    katipoTable->setFunction("init", [this](TuiTable* args, TuiRef* existingResult, TuiFunctionCallData* incomingCallData, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(!hostServer)
        {
            //todo could have separate keys for hosts/clients
            std::string publicKey = "";
            std::string secretKey = "";
            
            std::string trackerKeyPath = "tracker_privateKey.tuib";
            
            if(Tui::fileExistsAtPath(trackerKeyPath)) //todo these should be saved in the database, not files
            {
                TuiTable* saveData = (TuiTable*)TuiRef::loadBinary(trackerKeyPath);
                if(saveData)
                {
                    publicKey = saveData->getString("publicKey");
                    secretKey = saveData->getString("secretKey");
                }
            }
            
            if(publicKey.empty())
            {
                publicKey.resize(crypto_box_PUBLICKEYBYTES);
                secretKey.resize(crypto_box_SECRETKEYBYTES);
                crypto_box_keypair((unsigned char*)&(publicKey[0]), (unsigned char*)&(secretKey[0]));
                
                TuiTable* saveData = new TuiTable(nullptr);
                
                saveData->setString("publicKey", publicKey);
                saveData->setString("secretKey", secretKey);
                
                saveData->saveBinary(trackerKeyPath);
                saveData->release();
                
                MJLog("Generated and saved new private key:\n%s.\nPlease backup this file and keep it safe and secure!", Tui::getAbsolutePath(trackerKeyPath).c_str());
            }
            else
            {
                MJLog("loaded private key:\n%s", Tui::getAbsolutePath(trackerKeyPath).c_str());
            }
            
            hostServer = new Server(publicKey, secretKey, "hostServer", katipoTable->get("hostPort")->getStringValue(), 4095, katipoTable);
            clientServer = new Server(publicKey, secretKey, "clientServer", katipoTable->get("clientPort")->getStringValue(), 4095, katipoTable);
            clientServer->hostServer = hostServer;
            hostServer->clientServer = clientServer;
            return TUI_TRUE;
        }
        return TUI_FALSE;
    });



    katipoTable->setFunction("start", [this](TuiTable* args, TuiRef* existingResult, TuiFunctionCallData* incomingCallData, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(hostServer && !hostServer->running)
        {
            if(hostServer->start() && clientServer->start())
            {
                thread = new std::thread(&Controller::serverEventLoop, this);
                return TUI_TRUE;
            }
        }
        return TUI_FALSE;
    });

    
    TuiRef::runScriptFile(basePath + "scripts/code.tui", rootTable);
    
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
