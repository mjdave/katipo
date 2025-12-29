#include "Controller.h"
#include "Timer.h"
#include "ClientNetInterface.h"
#include "TuiFileUtils.h"

#define TRACKER_IP "127.0.0.1"
#define TRACKER_PORT "3471" //clients connect to 3471, servers to 3470

void Controller::init(std::string basePath)
{
    rootTable = Tui::createRootTable();
    
    /*MJLog("run from path:%s", basePath.c_str())*/
    
    TuiTable* katipoTable = new TuiTable(rootTable);
    rootTable->set("katipo", katipoTable);
    katipoTable->release();
    
    clientInfo = new TuiTable(katipoTable);
    katipoTable->setTable("clientInfo", clientInfo);
    
    //todo generate and save/load unique names and ids
    clientInfo->setString("name", "Client");
    clientInfo->setString("clientID", "3234567812345678"); //should be the public key

    //katipo.get("127.0.0.1/example", sendData, function(result){ print("got result:", result)})
    katipoTable->setFunction("get", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(args->arrayObjects.size() >= 1)
        {
            TuiRef* urlRef = args->arrayObjects[0];
            if(urlRef->type() == Tui_ref_type_STRING)
            {
                std::string remoteURL = urlRef->getStringValue();
                std::vector<std::string> split = Tui::splitString(remoteURL, '/');
                
                std::string trackerURL = TRACKER_IP;
                std::string trackerPort = TRACKER_PORT;
                
                if(split[0].find(".") != -1)
                {
                    std::vector<std::string> portSplit = Tui::splitString(split[0], ':');
                    trackerURL = portSplit[0];
                    if(portSplit.size() > 1)
                    {
                        trackerPort = portSplit[1];
                    }
                    remoteURL = remoteURL.substr(split[0].length() + 1, -1);
                }
                
                std::string trackerKey = trackerURL + "/" + trackerPort;
                ClientNetInterface* netInterface = nullptr;
                if(netInterfaces.count(trackerKey) != 0)
                {
                    netInterface = netInterfaces[trackerKey];
                }
                else
                {
                    netInterface = new ClientNetInterface(trackerURL,
                                                          trackerPort,
                                                        clientInfo);
                    netInterfaces[trackerKey] = netInterface;
                    
                    netInterface->bindTui(rootTable);
                }
                
                TuiTable* remoteFuncCallArgs = new TuiTable(nullptr);
                
                TuiString* remoteURLString = new TuiString(remoteURL);
                remoteFuncCallArgs->arrayObjects.push_back(remoteURLString);
                
                TuiFunction* callbackFunction = nullptr;
                
                for(int i = 1; i < args->arrayObjects.size(); i++)
                {
                    if(i == args->arrayObjects.size() - 1 && args->arrayObjects[i]->type() == Tui_ref_type_FUNCTION)
                    {
                        callbackFunction = ((TuiFunction*)args->arrayObjects[i]);
                    }
                    else
                    {
                        TuiRef* arg = args->arrayObjects[i];
                        arg->retain();
                        remoteFuncCallArgs->arrayObjects.push_back(arg);
                    }
                }
                
                netInterface->callServerFunction("callHostFunction", remoteFuncCallArgs, callbackFunction);
                
                remoteFuncCallArgs->release();
            }
            else
            {
                MJError("get expected url string");
            }
        }
        return nullptr;
    });
    
    
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
        for(auto& idAndRequestInterface : netInterfaces)
        {
            idAndRequestInterface.second->pollNetEvents();
        }
        
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
    clientInfo->release();
    for(auto& idAndRequestInterface : netInterfaces)
    {
        delete idAndRequestInterface.second;
    }
}
