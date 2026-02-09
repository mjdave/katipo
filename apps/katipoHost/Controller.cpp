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
#include "sodium.h"

#if COMPILE_WITH_HTTP_INTERFACE
#include "ClientHttpInterface.h"
#endif

//#define TRACKER_IP "127.0.0.1"
//#define TRACKER_PORT "3470"

void Controller::init(int argc, const char * argv[])
{
    std::string basePath = Tui::pathByRemovingLastPathComponent(argv[0]);
    rootTable = Tui::getRootTable();

    TuiTable* launchArgsTable = new TuiTable(rootTable);
    rootTable->set("launchArgs", launchArgsTable);
    launchArgsTable->release();
    
    katipoTable = new TuiTable(rootTable);
    rootTable->set("katipo", katipoTable);
    katipoTable->release();
    
#if COMPILE_WITH_HTTP_INTERFACE
    httpInterface = new ClientHttpInterface();
    httpInterface->bindTui(rootTable);
#endif
    
    for(int i = 1; i < argc; i++)
    {
        launchArgsTable->arrayObjects.push_back(new TuiString(argv[i]));

        std::string arg = argv[i];
        if(arg == "--basePath") //base path is where we find scripts/code.tui. Allows the same executable to run different environments
        {
            if(i+1 >= argc)
            {
                MJError("missing basePath path argument. usage example: ./katipoHost --basePath BASE_PATH");
                abort();
            }
            basePath = argv[++i];
            launchArgsTable->arrayObjects.push_back(new TuiString(argv[i]));
        }
    }

    katipoTable->setFunction("init", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(!trackerNetInterface)
        {
            //TuiTable* clientInfo = katipoTable->getTable("clientInfo");
            TuiTable* siteInfo = katipoTable->getTable("siteInfo");
            if(!siteInfo)
            {
                MJError("Invalid or missing siteInfo table");
                abort();
            }
            if(!siteInfo->hasKey("nameKey"))
            {
                MJError("siteInfo is missing a 'nameKey' entry ");
                abort();
            }
            
            std::string publicKey = "";
            std::string secretKey = "";
            
            std::string hostKeyPath = siteInfo->getString("nameKey") + "_privateKey.tuib";
            
            if(Tui::fileExistsAtPath(hostKeyPath)) //todo these should be saved in the database, not files
            {
                TuiTable* saveData = (TuiTable*)TuiRef::loadBinary(hostKeyPath);
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
                
                saveData->saveBinary(hostKeyPath);
                saveData->release();
            
                MJLog("Generated and saved new private key:\n%s.\nPlease backup this file and keep it safe and secure!", Tui::getAbsolutePath(hostKeyPath).c_str());
            }
            else
            {
                MJLog("loaded private key:\n%s", Tui::getAbsolutePath(hostKeyPath).c_str());
            }
            
            siteInfo->setString("publicKey", publicKey);
            
            trackerNetInterface = new ClientNetInterface(katipoTable->get("trackerIP")->getStringValue(),
                                                    katipoTable->get("trackerPort")->getStringValue(),
                                                         publicKey, secretKey, siteInfo);
            trackerNetInterface->bindTui(katipoTable);

    
            thread = new std::thread(&Controller::serverEventLoop, this);

            return TUI_TRUE;
        }
        return nullptr;
    });
    
    
    scriptState = (TuiTable*)TuiRef::runScriptFile(Tui::pathByAppendingPathComponent(basePath,"scripts/code.tui"), rootTable);
}

static const double SERVER_FIXED_TIME_STEP = 1.0 / 10.0;

void Controller::serverEventLoop()
{
    Timer* timer = new Timer();
    //Timer* deltaTimer = new Timer();
    
    while(1)
    {
        //checkInput();
        
        //double dt = std::clamp(deltaTimer->getDt(), 0.0, 4.0);
        
        trackerNetInterface->pollNetEvents();
#if COMPILE_WITH_HTTP_INTERFACE
        httpInterface->update();
#endif
        
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

