
#include "Server.h"
#include "ServerNetInterface.h"
//#include "DatabaseEnvironment.h"
//#include "Database.h"
//#include "FileUtils.h"
#include "TuiStringUtils.h"

Server::Server(std::string hostName_,
               std::string port_,
               int maxConnections_,
               TuiTable* rootTable)
{
    hostName = hostName_;
    port = port_;
    maxConnections = maxConnections_;
    if(rootTable)
    {
        bindTui(rootTable);
    }
}

void Server::bindTui(TuiTable* rootTable)
{
    TuiTable* serverTable = new TuiTable(rootTable);
    rootTable->setTable(hostName, serverTable);
    serverTable->release();
    
    serverTable->onSet = [this](TuiRef* table, const std::string& key, TuiRef* value) {
        if(key == "clientConnected")
        {
            if(value->type() == Tui_ref_type_FUNCTION)
            {
                clientConnectedFunction = (TuiFunction*)value;
            }
            else if(value->type() == Tui_ref_type_NIL)
            {
                clientConnectedFunction = nullptr;
            }
        }
    };
    
    serverTable->onSet = [this](TuiRef* table, const std::string& key, TuiRef* value) {
        if(key == "clientDisconnected")
        {
            if(value->type() == Tui_ref_type_FUNCTION)
            {
                clientDisconnectedFunction = (TuiFunction*)value;
            }
            else if(value->type() == Tui_ref_type_NIL)
            {
                clientDisconnectedFunction = nullptr;
            }
        }
    };
    
    serverTable->setFunction("register", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(args->arrayObjects.size() >= 2)
        {
            TuiRef* functionNameRef = args->arrayObjects[0];
            TuiRef* functionRef = args->arrayObjects[1];
            if(functionNameRef->type() == Tui_ref_type_STRING && functionRef->type() == Tui_ref_type_FUNCTION)
            {
                registeredFunctions[((TuiString*)functionNameRef)->value] = (TuiFunction*)functionRef;
            }
            else
            {
                TuiParseError(callingDebugInfo->fileName.c_str(), callingDebugInfo->lineNumber, "Incorrect argument type");
            }
        }
        else
        {
            TuiParseError(callingDebugInfo->fileName.c_str(), callingDebugInfo->lineNumber, "Missing args");
        }
        return nullptr;
    });
    
    // server.call(clientID, "playlists", testPlaylists)
    serverTable->setFunction("callClientFunction", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(args->arrayObjects.size() >= 2)
        {
            TuiRef* clientIDRef = args->arrayObjects[0];
            TuiRef* functionNameRef = args->arrayObjects[1];
            if(clientIDRef->type() == Tui_ref_type_STRING && functionNameRef->type() == Tui_ref_type_STRING)
            {
                if(clients.count(((TuiString*)clientIDRef)->value) == 0)
                {
                    MJWarn("Attempt to call client function:%s for non connected client:%s", ((TuiString*)functionNameRef)->value.c_str(), ((TuiString*)clientIDRef)->value.c_str());
                    return nullptr;
                }
                TuiTable* sendTable = new TuiTable(nullptr);
                sendTable->set("name", functionNameRef);
                for(int i = 2; i < args->arrayObjects.size(); i++)
                {
                    if(i == args->arrayObjects.size() - 1 && args->arrayObjects[i]->type() == Tui_ref_type_FUNCTION)
                    {
                        callbacksByID[functionCallbackIDCounter] = ((TuiFunction*)args->arrayObjects[i]->retain());
                        sendTable->setDouble("callbackID", functionCallbackIDCounter++);
                    }
                    else
                    {
                        TuiRef* arg = args->arrayObjects[i];
                        arg->retain();
                        sendTable->arrayObjects.push_back(arg);
                    }
                }
                
                std::string dataSerialized = sendTable->serializeBinary();
                sendTable->release();
                
                ServerData serverData;
                serverData.type = SERVER_DATA_TYPE_SERVER_CLIENT_FUNCTION_CALL_REQUEST;
                serverData.data = (void*)dataSerialized.data();
                serverData.length = dataSerialized.length();
                
                clients[((TuiString*)clientIDRef)->value]->sendDataToClient(serverData, true);
                
            }
            else
            {
                TuiParseError(callingDebugInfo->fileName.c_str(), callingDebugInfo->lineNumber, "Incorrect argument type");
            }
        }
        else
        {
            TuiParseError(callingDebugInfo->fileName.c_str(), callingDebugInfo->lineNumber, "Missing args");
        }
        return nullptr;
    });
    
    serverTable->setFunction("sendFile", [this](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        
        if(args->arrayObjects.size() >= 2 && args->arrayObjects[0]->type() == Tui_ref_type_STRING && args->arrayObjects[1]->type() == Tui_ref_type_STRING)
        {
            TuiRef* clientIDRef = args->arrayObjects[0];
            TuiRef* fileNameRef = args->arrayObjects[1];
            
            std::string fileData = Tui::getFileContents(((TuiString*)fileNameRef)->value);
            if(!fileData.empty())
            {
                ServerData serverData;
                serverData.type = SERVER_DATA_TYPE_SERVER_CLIENT_FUNCTION_CALL_REQUEST;
                serverData.data = (void*)fileData.data();
                serverData.length = fileData.length();
                
                clients[((TuiString*)clientIDRef)->value]->sendLargeDataToClient(serverData);
            }
        }
        
        return nullptr;
    });
}

void Server::loadDatabase()
{
    //serverDatabaseEnvironment = new DatabaseEnvironment(getWorldSavePath(hostPlayerID, worldID, "serverdb"),
                                                      //  32,
                                                      //  2);
   // serverDatabase = new Database(serverDatabaseEnvironment, "serverInternal");
}

Server::~Server()
{
    if(serverNetInterface)
    {
        delete serverNetInterface;
    }

    //if(serverDatabase)
    //{
	//	MJLog("Closing database");
    //    delete serverDatabase;
    //    delete serverDatabaseEnvironment;
   // }
}


bool Server::start()
{
    if(running)
    {
        return true;
    }
    
	if(!serverNetInterface)
	{
        
        MJLog("Starting server \"%s\" on UDP port:%s max connections:%d", hostName.c_str(), port.c_str(), maxConnections);
        
        int portNumber = 7121;
        
        if(!port.empty())
        {
            try
            {
                portNumber = std::stoi(port);
            }
            catch(const std::runtime_error& ex)
            {
                MJError("invalid port specificed:%s - %s", port.c_str(), ex.what());
                return false;
            }
        }
        else
        {
            MJError("No port specified in ServerNetInterface");
            return false;
        }
        
        std::string logPath = hostName + "_serverLog.log";
		serverNetInterface = new ServerNetInterface(this, portNumber, maxConnections, logPath);
        
        if(!serverNetInterface->valid)
        {
            delete serverNetInterface;
            serverNetInterface = nullptr;
            return false;
        }

	}
    
    running = true;
    return true;
}

void Server::stop()
{
	if(running)
	{
        running = false;
        if(serverNetInterface)
        {
            MJLog("Stopping server");
            if(serverNetInterface)
            {
                delete serverNetInterface;
                serverNetInterface = nullptr;
            }
        }
	}
}

void Server::update(double dt)
{
    if(running)
    {
        if(serverNetInterface)
        {
            serverNetInterface->update(dt);
        }
    }
}

void Server::addClient(NetServerClient* client)
{
    clients[client->clientID] = client;
    
    if(clientConnectedFunction)
    {
        clientConnectedFunction->call("Server::addClient::clientConnectedFunction", new TuiString(client->clientID), client->joinRequest);
    }
    
}

void Server::removeClient(std::string clientID)
{
    if(clientDisconnectedFunction)
    {
        clientDisconnectedFunction->call("Server::addClient::clientDisconnectedFunction", new TuiString(clientID));
    }
    clients.erase(clientID);
}

void Server::clientDataReceived(NetServerClient* client, const ServerData& serverData)
{
    if(serverData.type == SERVER_DATA_TYPE_CLIENT_SERVER_FUNCTION_CALL_REQUEST)
    {
        TuiTable* tuiData = (TuiTable*)TuiRef::loadBinaryString(std::string((const char*)serverData.data, serverData.length)); //todo memcpys
        TuiString* functionName = (TuiString*)tuiData->objectsByStringKey["name"];
        if(registeredFunctions.count(functionName->value) != 0)
        {
            tuiData->retain();
            TuiTable* sendArgs = new TuiTable(nullptr);
            
            TuiDebugInfo debugInfo;
            debugInfo.fileName = "FUNCTION_CALL_REQUEST";
            
            sendArgs->arrayObjects.push_back(new TuiString(client->clientID));
            
            for(TuiRef* arg : tuiData->arrayObjects)
            {
                sendArgs->push(arg);
            }
            
            TuiFunction* callbackFunction = nullptr;
            if(tuiData->hasKey("callbackID"))
            {
                callbackFunction = new TuiFunction([this, tuiData, client](TuiTable* args, TuiRef* existingResult, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
                    TuiTable* sendTable = new TuiTable(nullptr);
                    sendTable->set("callbackID", tuiData->objectsByStringKey["callbackID"]); //todo don't capture
                    if(args && args->arrayObjects.size() > 0)
                    {
                        sendTable->set("data", args->arrayObjects[0]);
                    }
                    
                    std::string dataSerialized = sendTable->serializeBinary();
                    sendTable->release();
                    
                    ServerData serverData;
                    serverData.type = SERVER_DATA_TYPE_SERVER_FUNCTION_CALL_RESPONSE;
                    serverData.data = (void*)dataSerialized.data();
                    serverData.length = dataSerialized.length();
                    
                    client->sendDataToClient(serverData, true); //todo don't capture client here, find it again in case it has disconnected
                    return nullptr;
                });
                sendArgs->push(callbackFunction);
            }
            
            TuiRef* result = registeredFunctions[functionName->value]->call(sendArgs, nullptr, &debugInfo);
            
            if(callbackFunction && result && result->type() != Tui_ref_type_NIL)
            {
                
                TuiTable* funcCallArgs = new TuiTable(nullptr);
                
                funcCallArgs->push(result);
                
                callbackFunction->call(funcCallArgs, nullptr, &debugInfo);
                
                funcCallArgs->release();
                result->release();
            }
            
            if(callbackFunction)
            {
                callbackFunction->release();
            }
            
            sendArgs->release();
        }
        else
        {
            MJError("attempt to call unregistered function:%s", functionName->value.c_str());
        }
        tuiData->release();
            
        if(serverData.data)
        {
            free(serverData.data);
        }
    }
    else if(serverData.type == SERVER_DATA_TYPE_CLIENT_FUNCTION_CALL_RESPONSE)
    {
        TuiTable* tuiData = (TuiTable*)TuiRef::loadBinaryString(std::string((const char*)serverData.data, serverData.length)); //todo memcpys
        uint32_t callbackID = ((TuiNumber*)tuiData->objectsByStringKey["callbackID"])->value;
        
        if(callbacksByID.count(callbackID) == 0)
        {
            MJError("no callback");
            abort();
        }
        
        callbacksByID[callbackID]->call("CLIENT_FUNCTION_CALL_RESPONSE", tuiData->get("data"));
    }
    else if(serverData.type == SERVER_DATA_TYPE_CLIENT_SERVER_DOWNLOAD_FILE_REQUEST)
    {
        TuiTable* tuiData = (TuiTable*)TuiRef::loadBinaryString(std::string((const char*)serverData.data, serverData.length)); //todo memcpys
        TuiString* functionName = (TuiString*)tuiData->objectsByStringKey["name"];
        if(registeredFunctions.count(functionName->value) != 0)
        {
            TuiDebugInfo debugInfo;
            debugInfo.fileName = "CLIENT_DOWNLOAD_FILE_REQUEST";
            TuiTable* args = new TuiTable(nullptr);
            
            args->arrayObjects.push_back(new TuiString(client->clientID));
            for(TuiRef* arg : tuiData->arrayObjects)
            {
                args->arrayObjects.push_back(arg->retain());
            }
            
            TuiRef* result = registeredFunctions[functionName->value]->call(args, nullptr, &debugInfo);
            
            args->release();
            
            if(tuiData->hasKey("callbackID"))
            {
                TuiString* dataString = nullptr;
                if(result && result->type() == Tui_ref_type_STRING)
                {
                    dataString = new TuiString("");
                    std::ifstream in((((TuiString*)result)->value).c_str(), std::ios::in | std::ios::binary);
                    if(in)
                    {
                        in.seekg(0, std::ios::end);
                        (dataString->value).resize(in.tellg());
                        in.seekg(0, std::ios::beg);
                        in.read(&(dataString->value)[0], (dataString->value).size());
                        in.close();
                    }
                }
                
                if(result)
                {
                    result->release();
                }
            
                TuiTable* sendTable = new TuiTable(nullptr);
                sendTable->set("callbackID", tuiData->objectsByStringKey["callbackID"]);
                if(dataString)
                {
                    sendTable->set("data", dataString);
                    dataString->release();
                }
                
                std::string dataSerialized = sendTable->serializeBinary();
                sendTable->release();
                
                ServerData serverData;
                serverData.type = SERVER_DATA_TYPE_SERVER_DOWNLOAD_FILE_RESPONSE;
                serverData.data = (void*)dataSerialized.data();
                serverData.length = dataSerialized.length();
                
                client->sendLargeDataToClient(serverData);
            }
        }
        else
        {
            MJError("attempt to call unregistered function:%s", functionName->value.c_str());
        }
        tuiData->release();
            
        if(serverData.data)
        {
            free(serverData.data);
        }
        
    }
}

void Server::sendDataToAllClients(uint8_t type,
                                  std::string& serializedData,
                                  bool reliable)
{
    ServerData serverData;
    serverData.type = type;
    serverData.data = (void*)serializedData.data();
    serverData.length = serializedData.size();
    
    for(auto& idAndClient : clients)
    {
        idAndClient.second->sendDataToClient(serverData, reliable);
    }
}


void Server::sendDataToClient(NetServerClient* client,
                                  uint8_t type,
                                  std::string& serializedData,
                              bool reliable)
{
    ServerData serverData;
    serverData.type = type;
    serverData.data = (void*)serializedData.data();
    serverData.length = serializedData.size();

    client->sendDataToClient(serverData, reliable);
}

void Server::registerNetFunction(std::string name, std::function<TuiTable*(TuiTable*)>& func)
{
    MJError("todo");
    //registeredFunctions[name] = func;
}

void Server::callClientFunctionForAllClientsInternal(std::string functionName, TuiTable* userData, std::function<void(TuiTable*)>& callback, bool reliable)
{
    MJError("unimplemented callClientFunctionForAllClientsInternal");
    /*MJLuaRef* mjCallback = nullptr;
    if(callback)
    {
        mjCallback = new MJLuaRef(callback);
    }
    std::string userDataString;
    if(userData)
    {
        userDataString = serializeObject(userData);
    }
    CallFunctionRequest* request = new CallFunctionRequest(functionName, userDataString, mjCallback, FUNCTION_REQUEST_ORIGIN_SERVER);

    CallFunctionRequestNetData netData = CallFunctionRequestNetData(request);
    std::string dataSerialized = serializeObject(netData);

    ServerData serverData;
    serverData.type = SERVER_DATA_TYPE_SERVER_CLIENT_FUNCTION_CALL_REQUEST;
    serverData.data = (void*)dataSerialized.data();
    serverData.length = dataSerialized.length();

    for(auto& idAndClient : clients)
    {
        idAndClient.second->sendDataToClient(serverData, reliable);
    }

    delete request;*/
}

void Server::clientJoinGetInfoReceived(ENetPeer* enetPeer, uint32_t steamConnectionHandle, const ServerData& serverData)
{
    MJError("unimplemented clientJoinGetInfoReceived");
    /*ClientPreJoinGetInfoRequest request;
    if(unserializeObject(&request, std::string((const char*)serverData.data, serverData.length)))
    {
        
        LuaRef joinInfoTable = luabridge::newTable(luaEnvironment->state);
        
        bool found = false;
        for(int sessionIndex = 0;;sessionIndex++)
        {
            std::string playerSessionID = playerSessionIDForPlayerIDWithSessionIndex(request.playerID, sessionIndex);
            uint64_t clientID = clientIDFromPlayerIDStrings(playerSessionID, request.playerID);
            LuaRef returnValueRef = luaModule->callFunction("getSessionInfoForConnectingClient", clientID);
            if(returnValueRef.isNil())
            {
                break;
            }
            
            joinInfoTable[sessionIndex + 1] = returnValueRef;
            found = true;
        }
        
        if(!found)
        {
            joinInfoTable = LuaRef(luaEnvironment->state);
        }
        
        
        std::string returnValueString = serializeObject(joinInfoTable);
        
        serverNetInterface->sendData(SERVER_DATA_TYPE_SERVER_JOIN_INFO_RESPONSE,
                                     (void*)returnValueString.data(),
                                     returnValueString.size(),
                                     enetPeer,
                                     steamConnectionHandle,
                                     true);
        
    }*/
}

void Server::callClientFunctionForAllClients(std::string functionName, TuiTable* userData, std::function<void(TuiTable*)>& callback)
{
    callClientFunctionForAllClientsInternal(functionName, userData, callback, true);
}

void Server::callUnreliableClientFunctionForAllClients(std::string functionName, TuiTable* userData, std::function<void(TuiTable*)>& callback)
{
    callClientFunctionForAllClientsInternal(functionName, userData, callback, false);
}


void Server::callClientFunctionInternal(std::string functionName, std::string clientID, TuiTable* userData, std::function<void(TuiTable*)>& callback, bool reliable)
{
    MJError("unimplemented callUnreliableClientFunctionForAllClients");
    //MJLog("callClientFunction:%s clientID:%llx", functionName, clientID);
	if(clients.count(clientID) == 0)
	{
		MJWarn("Attempt to call client function:%s for non connected client:%s", functionName.c_str(), clientID.c_str());
		return;
	}
    /*MJLuaRef* mjCallback = nullptr;
    if(callback)
    {
        mjCallback = new MJLuaRef(callback);
    }
    std::string userDataString;
    if(userData)
    {
        userDataString = serializeObject(userData);
    }
    CallFunctionRequest* request = new CallFunctionRequest(functionName, userDataString, mjCallback, FUNCTION_REQUEST_ORIGIN_SERVER);
    
    CallFunctionRequestNetData netData = CallFunctionRequestNetData(request);
    std::string dataSerialized = serializeObject(netData);
  
    ServerData serverData;
    serverData.type = SERVER_DATA_TYPE_SERVER_CLIENT_FUNCTION_CALL_REQUEST;
    serverData.data = (void*)dataSerialized.data();
    serverData.length = dataSerialized.length();
    
    clients[clientID]->sendDataToClient(serverData, reliable);
    delete request;*/
}

void Server::callClientFunction(std::string functionName, std::string clientID, TuiTable* userData, std::function<void(TuiTable*)>& callback)
{
    callClientFunctionInternal(functionName, clientID, userData, callback, true);
}

void Server::callUnreliableClientFunction(std::string functionName, std::string clientID, TuiTable* userData, std::function<void(TuiTable*)>& callback)
{
    callClientFunctionInternal(functionName, clientID, userData, callback, false);
}
