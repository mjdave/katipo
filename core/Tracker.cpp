
#include "Tracker.h"
#include "Server.h"
#include "Timer.h"
#include "TuiScript.h"

Tracker::Tracker(TuiTable* katipoTable)
{
    katipoTable->setFunction("init", [this, katipoTable](TuiTable* args, TuiRef* existingResult, TuiFunctionCallData* incomingCallData, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
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
                thread = new std::thread(&Tracker::serverEventLoop, this);
                return TUI_TRUE;
            }
        }
        return TUI_FALSE;
    });
    
    
    TuiRef::runScriptFile(katipoTable->getString("basePath") + "/scripts/code.tui", katipoTable);
}

Tracker::~Tracker()
{
    
}


static const double SERVER_FIXED_TIME_STEP = 1.0 / 20.0;

void Tracker::serverEventLoop()
{
    Timer* timer = new Timer();
    Timer* deltaTimer = new Timer();
    
    while(1)
    {
        //checkInput();
        
        double dt = glm::clamp(deltaTimer->getDt(), 0.0, 1.0);
        
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
