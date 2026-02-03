#ifdef _MSC_VER
#define _WINSOCKAPI_    // stops windows.h including winsock.h
#define NOMINMAX
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <windows.h>
#endif

#include "Controller.h"
#include "Timer.h"
#include "ClientNetInterface.h"
#include "TuiFileUtils.h"
#include "MJVersion.h"
#include "sodium.h"

//#define TRACKER_IP "127.0.0.1"
//#define TRACKER_PORT "3471" //clients connect to 3471, servers to 3470
/*
 
     std::string remoteURL = urlRef->getStringValue();
     std::vector<std::string> split = Tui::splitString(remoteURL, '/');
     
     std::string trackerURL = "127.0.0.1";
     std::string trackerPort = "3471";
     std::string hostName = split[0];
 */

void doGet(ClientNetInterface* netInterface, const std::string& remoteURL, const std::string& hostName, TuiTable* args)
{
    TuiFunction* mainGetCallbackFunction = nullptr;
    if(!args->arrayObjects.empty() && args->arrayObjects[args->arrayObjects.size() - 1]->type() == Tui_ref_type_FUNCTION)
    {
        mainGetCallbackFunction = ((TuiFunction*)args->arrayObjects[args->arrayObjects.size() - 1]);
        mainGetCallbackFunction->retain();
    }
    
    TuiTable* remoteFuncCallArgs = new TuiTable(nullptr); //todo leaks?
    
    TuiString* remoteURLString = new TuiString(remoteURL);
    remoteFuncCallArgs->arrayObjects.push_back(remoteURLString);
    
    for(int i = 1; i < args->arrayObjects.size(); i++)
    {
        if(args->arrayObjects[i]->type() != Tui_ref_type_FUNCTION)
        {
            TuiRef* arg = args->arrayObjects[i];
            arg->retain();
            remoteFuncCallArgs->arrayObjects.push_back(arg);
        }
    }
    
    MJLog("fetching from remote hostName:%s", hostName.c_str());
    
    
    TuiFunction* callHostFunctionCallbackFunction = new TuiFunction([mainGetCallbackFunction](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        
        if(args && !args->arrayObjects.empty())
        {
            TuiRef* result = args->arrayObjects[0];
            mainGetCallbackFunction->call("mainGetCallbackFunction", result);
        }
        else
        {
            mainGetCallbackFunction->call("mainGetCallbackFunction", TUI_NIL);
        }
        
        mainGetCallbackFunction->release();
        return TUI_NIL;
    });
    
    remoteFuncCallArgs->push(callHostFunctionCallbackFunction);
    callHostFunctionCallbackFunction->release();
    
    TuiFunction* getSiteKeyCallbackFunction = new TuiFunction([mainGetCallbackFunction, remoteFuncCallArgs, netInterface](TuiTable* incomingCallbackResponseData, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        
        if(incomingCallbackResponseData && !incomingCallbackResponseData->arrayObjects.empty())
        {
            TuiRef* result = incomingCallbackResponseData->arrayObjects[0];
            
            if(result->type() == Tui_ref_type_TABLE && (((TuiTable*)result)->hasKey("data") || ((TuiTable*)result)->getString("status") == "ok"))
            {
                //TODO check for host key locally tracker_hostname
                //abort on mismatch
                //save new host key
                
                TuiRef* hostPublicKeyRef = ((TuiTable*)result)->get("publicKey");
                if(hostPublicKeyRef && hostPublicKeyRef->type() == Tui_ref_type_STRING)
                {
                    netInterface->callRemoteHostFunction(((TuiString*)hostPublicKeyRef)->value, remoteFuncCallArgs);
                }
                else
                {
                    MJError("missing public key");
                    mainGetCallbackFunction->call("mainGetCallbackFunction", new TuiTable("{status='error',message='missing public key'}"));
                }
                
            }
            else
            {
                mainGetCallbackFunction->call("mainGetCallbackFunction", result->retain()); //status not ok. retain for all call() args
            }
        }
        else
        {
            MJError("missing args");
            mainGetCallbackFunction->call("mainGetCallbackFunction", TUI_NIL);
        }
        
        mainGetCallbackFunction->release();
        return TUI_NIL;
    });
    
    TuiTable* remoteHostKeyFuncCallArgs = new TuiTable(nullptr);
    remoteHostKeyFuncCallArgs->pushString("getSiteKey");
    remoteHostKeyFuncCallArgs->pushString(hostName);
    remoteHostKeyFuncCallArgs->push(getSiteKeyCallbackFunction);
    getSiteKeyCallbackFunction->release();
    
    netInterface->callTrackerFunction(remoteHostKeyFuncCallArgs);
    
    remoteHostKeyFuncCallArgs->release();
}

void Controller::init(int argc, const char * argv[])
{
    
    if(sodium_init() < 0) //this is safe to call multiple times
    {
        MJError("Sodium initialization failed. Exiting.");
        abort();
    }
    
    std::string basePath = Tui::pathByRemovingLastPathComponent(argv[0]);
    rootTable = Tui::getRootTable();

    TuiTable* launchArgsTable = new TuiTable(rootTable);
    rootTable->set("launchArgs", launchArgsTable);
    launchArgsTable->release();
    
    katipoTable = new TuiTable(rootTable);
    rootTable->set("katipo", katipoTable);
    katipoTable->release();

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

    //katipo.get("127.0.0.1/example", sendData, function(result){ print("got result:", result)})
    katipoTable->setFunction("get", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(args->arrayObjects.size() >= 1)
        {
            TuiRef* urlRef = args->arrayObjects[0];
            if(urlRef->type() == Tui_ref_type_STRING)
            {
                std::string remoteURL = urlRef->getStringValue();
                std::vector<std::string> split = Tui::splitString(remoteURL, '/');
                
                std::string trackerURL = "127.0.0.1";
                std::string trackerPort = "3471";
                std::string hostName = split[0];
                
                if(split[0].find(".") != -1)
                {
                    std::vector<std::string> portSplit = Tui::splitString(split[0], ':');
                    trackerURL = portSplit[0];
                    if(portSplit.size() > 1)
                    {
                        trackerPort = portSplit[1];
                    }
                    remoteURL = remoteURL.substr(split[0].length() + 1, -1);
                    hostName = split[1];
                }
                
                std::string trackerKey = trackerURL + ":" + trackerPort;
                ClientNetInterface* netInterface = nullptr;
                if(netInterfaces.count(trackerKey) != 0)
                {
                    doGet(netInterfaces[trackerKey], remoteURL, hostName, args);
                }
                else
                {
                    std::string publicKey = "";
                    std::string secretKey = "";
                    
                    std::string clientKeyPath = "client_privateKey.tuib"; //todo these should be saved in the database, not files
                    
                    if(Tui::fileExistsAtPath(clientKeyPath))
                    {
                        TuiTable* saveData = (TuiTable*)TuiRef::loadBinary(clientKeyPath);
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
                        
                        saveData->saveBinary(clientKeyPath);
                        saveData->release();
                        MJLog("Generated and saved new private key:\n%s.\nPlease backup this file and keep it safe and secure!", Tui::getAbsolutePath(clientKeyPath).c_str());
                    }
                    else
                    {
                        MJLog("loaded private key:\n%s", Tui::getAbsolutePath(clientKeyPath).c_str());
                    }
                    
                    TuiTable* getArgs = args;
                    getArgs->retain();
                    TuiFunction* onConnect = new TuiFunction([this, trackerKey,remoteURL, hostName, getArgs](TuiTable* innerFuncArgs, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
                        //todo check for connection success
                        doGet(netInterfaces[trackerKey], remoteURL, hostName, getArgs);
                        getArgs->release();
                        return TUI_NIL;
                    });
                    
                    katipoTable->set("connected", onConnect);
                    
                    onConnect->release();
                    
                    netInterface = new ClientNetInterface(trackerURL,
                                                          trackerPort,
                                                          publicKey,
                                                          secretKey);
                    netInterfaces[trackerKey] = netInterface;
                    
                    netInterface->bindTui(katipoTable);
                }
                
                
                /*TuiTable* remoteFuncCallArgs = new TuiTable(nullptr);
                
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
                
                netInterface->callTrackerFunction("callHostFunction", remoteFuncCallArgs, callbackFunction);
                
                remoteFuncCallArgs->release();*/
            }
            else
            {
                MJError("get expected url string");
            }
        }
        return TUI_NIL;
    });
    
    thread = new std::thread(&Controller::serverEventLoop, this);
    
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
    for(auto& idAndRequestInterface : netInterfaces)
    {
        delete idAndRequestInterface.second;
    }
}
