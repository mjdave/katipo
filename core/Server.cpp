
#include "Server.h"
#include "ServerNetInterface.h"
//#include "DatabaseEnvironment.h"
//#include "Database.h"
//#include "FileUtils.h"
#include "TuiStringUtils.h"
#include "sodium.h"

Server::Server(const std::string& publicKey_,
               const std::string& secretKey_,
               std::string hostName_,
               std::string port_,
               int maxConnections_,
               TuiTable* rootTable)
{
    hostName = hostName_;
    port = port_;
    maxConnections = maxConnections_;
    publicKey = publicKey_;
    secretKey = secretKey_;
    
    if(rootTable)
    {
        bindTui(rootTable);
    }
}

void Server::bindTui(TuiTable* parentTable)
{
    TuiTable* serverTable = new TuiTable(parentTable);
    parentTable->setTable(hostName, serverTable);
    serverTable->release();
    
    serverTable->onSet = [this](TuiRef* table, const std::string& key, TuiRef* value) {
        if(key == "clientConnected")
        {
            if(clientConnectedFunction)
            {
                clientConnectedFunction->release();
            }
            if(value->type() == Tui_ref_type_FUNCTION)
            {
                clientConnectedFunction = (TuiFunction*)value;
                clientConnectedFunction->retain();
            }
            else if(value->type() == Tui_ref_type_NIL)
            {
                clientConnectedFunction = nullptr;
            }
        }
        else if(key == "clientDisconnected")
        {
            if(clientDisconnectedFunction)
            {
                clientDisconnectedFunction->release();
            }
            if(value->type() == Tui_ref_type_FUNCTION)
            {
                clientDisconnectedFunction = (TuiFunction*)value;
                clientDisconnectedFunction->retain();
            }
            else if(value->type() == Tui_ref_type_NIL)
            {
                clientDisconnectedFunction = nullptr;
            }
        }
    };
    
    serverTable->setFunction("register", [this](TuiTable* args, TuiRef* existingResult, TuiFunctionCallData* incomingCallData, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
        if(args->arrayObjects.size() >= 2)
        {
            TuiRef* functionNameRef = args->arrayObjects[0];
            TuiRef* functionRef = args->arrayObjects[1];
            if(functionNameRef->type() == Tui_ref_type_STRING && functionRef->type() == Tui_ref_type_FUNCTION)
            {
                if(registeredFunctions.count(((TuiString*)functionNameRef)->value) != 0)
                {
                    registeredFunctions[((TuiString*)functionNameRef)->value]->release();
                }
                registeredFunctions[((TuiString*)functionNameRef)->value] = (TuiFunction*)(functionRef->retain());
                return TUI_TRUE;
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
        return TUI_FALSE;
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
		serverNetInterface = new ServerNetInterface(publicKey, secretKey, this, portNumber, maxConnections, logPath);
        
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
        TuiRef* clientIDRef = new TuiString(client->clientID);
        clientConnectedFunction->call("Server::addClient::clientConnectedFunction", clientIDRef, client->initialData);
        clientIDRef->release();
    }
    
}

void Server::removeClient(std::string clientID)
{
    if(clientDisconnectedFunction)
    {
        TuiRef* clientIDRef = new TuiString(clientID);
        clientDisconnectedFunction->call("Server::addClient::clientDisconnectedFunction", clientIDRef);
        clientIDRef->release();
    }
    clients.erase(clientID);
}

void Server::relayHostResponseToClient(TuiTable* decryptedDataTable) //called by host server, we are now on the client server
{
    //bool sendSuccess = false;
    
    if(decryptedDataTable)
    {
        std::string requestID = decryptedDataTable->getString("requestID");
        if(clientsByHostRequestIDs.count(requestID) != 0)
        {
            std::string clientClientID = clientsByHostRequestIDs[requestID];
            
            if(!clientClientID.empty() && clients.count(clientClientID) != 0 && decryptedDataTable->hasKey("clientData"))
            {
                const std::string& dataSerialized = ((TuiString*)decryptedDataTable->objectsByStringKey["clientData"])->value;
                
                ServerData sendToHostServerData;
                sendToHostServerData.type = KATIPO_NET_TYPE_GET_RESPONSE_TO_CLIENT_FROM_HOST;
                sendToHostServerData.data = (void*)dataSerialized.data();
                sendToHostServerData.length = dataSerialized.length();
                
                clients[clientClientID]->sendDataToClient(sendToHostServerData, true);
                //sendSuccess = true;
            }
            else
            {
                MJError("failed to relay host response to client, bad request");
            }
        }
        else
        {
            MJError("failed to relay host response to client, bad request");
        }
    }
    else
    {
        MJError("failed to relay host response to client, unable to decrypt");
    }
    
    /*if(!sendSuccess && decryptedDataTable->hasKey("callbackID")) //todo something like this maybe, inform the host if the client didn't get the message
    {
        TuiTable* toSecureTable = new TuiTable(nullptr);
        toSecureTable->set("callbackID", decryptedDataTable->get("callbackID"));
        toSecureTable->setString("status", "error");
        toSecureTable->setString("message", "failed to call remote function");
        
        TuiTable* sendTable = client->getEncryptedDataTable(toSecureTable, publicKey, secretKey);
        toSecureTable->release();
        std::string dataSerialized = sendTable->serializeBinary();
        sendTable->release();
        
        ServerData serverData;
        serverData.type = KATIPO_NET_TYPE_FUNCTION_CALL_RESPONSE_TO_CLIENT_FROM_TRACKER;
        serverData.data = (void*)dataSerialized.data();
        serverData.length = dataSerialized.length();
        
        client->sendDataToClient(serverData, true);
    }*/
}

void Server::clientDataReceived(NetServerClient* client, const ServerData& serverData)
{
    if(serverData.type == KATIPO_NET_TYPE_CLIENT_SERVER_FUNCTION_CALL_REQUEST)
    {
        TuiTable* tuiDataWrapper = (TuiTable*)TuiRef::loadBinaryString(std::string((const char*)serverData.data, serverData.length));
        TuiTable* decryptedDataTable = serverNetInterface->getDecryptedDataTable(tuiDataWrapper);
        if(tuiDataWrapper)
        {
            tuiDataWrapper->release();
            tuiDataWrapper = nullptr;
        }
            
        if(decryptedDataTable && !decryptedDataTable->arrayObjects.empty() && decryptedDataTable->arrayObjects[0]->type() == Tui_ref_type_STRING)
        {
            TuiString* functionName = (TuiString*)decryptedDataTable->arrayObjects[0];
            if(registeredFunctions.count(functionName->value) != 0)
            {
                TuiRef* callbackID = decryptedDataTable->objectsByStringKey["callbackID"];
                //bool hasCallback = !callbackID.empty();
                
                TuiTable* sendArgs = new TuiTable(nullptr);
                
                TuiDebugInfo debugInfo;
                debugInfo.fileName = "FUNCTION_CALL_REQUEST";
                
                sendArgs->arrayObjects.push_back(new TuiString(client->clientID));
                
                for(int i = 1; i < decryptedDataTable->arrayObjects.size(); i++)
                {
                    sendArgs->push(decryptedDataTable->arrayObjects[i]);
                }
                
                TuiFunction* callbackFunction = nullptr;
                if(callbackID)
                {
                    callbackID->retain();
                    callbackFunction = new TuiFunction([this, callbackID, client](TuiTable* args, TuiRef* existingResult, TuiFunctionCallData* incomingCallData, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
                        TuiTable* toSecureTable = nullptr;
                        if(args && args->arrayObjects.size() > 0)
                        {
                            TuiRef* argObject = args->arrayObjects[0];
                            if(argObject->type() == Tui_ref_type_TABLE)
                            {
                                toSecureTable = (TuiTable*)argObject;
                                toSecureTable->retain();
                            }
                            else
                            {
                                MJError("callbackFunction expected table eg {status='' data=''}");
                            }
                        }
                        
                        if(!toSecureTable)
                        {
                            toSecureTable = new TuiTable(nullptr);
                        }
                        toSecureTable->set("callbackID", callbackID);
                        
                        TuiTable* sendTable = client->getEncryptedDataTable(toSecureTable, publicKey, secretKey);
                        toSecureTable->release();
                        std::string dataSerialized = sendTable->serializeBinary();
                        sendTable->release();
                        
                        ServerData serverData;
                        serverData.type = KATIPO_NET_TYPE_FUNCTION_CALL_RESPONSE_TO_CLIENT_FROM_TRACKER;
                        serverData.data = (void*)dataSerialized.data();
                        serverData.length = dataSerialized.length();
                        
                        client->sendDataToClient(serverData, true); //todo don't capture client here, find it again in case it has disconnected
                        callbackID->release();
                        return TUI_NIL;
                    });
                    sendArgs->push(callbackFunction);
                }
                
                TuiRef* result = registeredFunctions[functionName->value]->call(sendArgs, nullptr, nullptr, &debugInfo);
                
                if(callbackFunction && result && result->type() != Tui_ref_type_NIL)
                {
                    
                    TuiTable* funcCallArgs = new TuiTable(nullptr);
                    
                    funcCallArgs->push(result);
                    
                    callbackFunction->call(funcCallArgs, nullptr, nullptr, &debugInfo);
                    
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
        }
        else
        {
            MJError("failed to call function");
        }
        
            
        if(serverData.data)
        {
            free(serverData.data);
        }
        
        if(decryptedDataTable)
        {
            decryptedDataTable->release();
        }
    }
    else if(serverData.type == KATIPO_NET_TYPE_REMOTE_HOST_REQUEST)
    {
        TuiTable* tuiDataWrapper = (TuiTable*)TuiRef::loadBinaryString(std::string((const char*)serverData.data, serverData.length));
        TuiTable* decryptedDataTable = serverNetInterface->getDecryptedDataTable(tuiDataWrapper);
        if(tuiDataWrapper)
        {
            tuiDataWrapper->release();
            tuiDataWrapper = nullptr;
        }
        
        bool sendSuccess = false;
            
        if(decryptedDataTable)
        {
            std::string hostPublicKey = decryptedDataTable->getString("hostPublicKey");
            std::string hostClientID = readableKeyForPublicKey(hostPublicKey);
            
            if(!hostClientID.empty() && hostServer->clients.count(hostClientID) != 0 && decryptedDataTable->hasKey("data"))
            {
                TuiTable* toSecureTable = new TuiTable(nullptr);
                std::string requestID;
                requestID.resize(16);
                randombytes_buf(&requestID[0], 16);
                
                clientsByHostRequestIDs[requestID] = client->clientID; //todo remove requestID from clientsByHostRequestIDs when client disconnects or after a timeout
                toSecureTable->setString("requestID", requestID);
                toSecureTable->set("data", decryptedDataTable->objectsByStringKey["data"]);
                
                TuiTable* sendTable = hostServer->clients[hostClientID]->getEncryptedDataTable(toSecureTable, publicKey, secretKey);
                toSecureTable->release();
                std::string dataSerialized = sendTable->serializeBinary();
                sendTable->release();
                
                ServerData sendToHostServerData;
                sendToHostServerData.type = KATIPO_NET_TYPE_REMOTE_HOST_REQUEST;
                sendToHostServerData.data = (void*)dataSerialized.data();
                sendToHostServerData.length = dataSerialized.length();
                
                hostServer->clients[hostClientID]->sendDataToClient(sendToHostServerData, true);
                sendSuccess = true;
            }
            else
            {
                MJError("failed to call remote host function, bad request");
            }
        }
        else
        {
            MJError("failed to call remote host function, unable to decrypt");
        }
        
        if(!sendSuccess && decryptedDataTable && decryptedDataTable->hasKey("callbackID"))
        {
            TuiTable* toSecureTable = new TuiTable(nullptr);
            toSecureTable->set("callbackID", decryptedDataTable->get("callbackID"));
            toSecureTable->setString("status", "error");
            toSecureTable->setString("message", "failed to call remote function");
            
            TuiTable* sendTable = client->getEncryptedDataTable(toSecureTable, publicKey, secretKey);
            toSecureTable->release();
            std::string dataSerialized = sendTable->serializeBinary();
            sendTable->release();
            
            ServerData serverData;
            serverData.type = KATIPO_NET_TYPE_FUNCTION_CALL_RESPONSE_TO_CLIENT_FROM_TRACKER;
            serverData.data = (void*)dataSerialized.data();
            serverData.length = dataSerialized.length();
            
            client->sendDataToClient(serverData, true);
        }
        
            
        if(serverData.data)
        {
            free(serverData.data);
        }
        if(decryptedDataTable)
        {
            decryptedDataTable->release();
        }
    }
    else if(serverData.type == KATIPO_NET_TYPE_GET_RESPONSE_TO_CLIENT_FROM_HOST)
    {
        TuiTable* tuiDataWrapper = (TuiTable*)TuiRef::loadBinaryString(std::string((const char*)serverData.data, serverData.length));
        TuiTable* decryptedDataTable = serverNetInterface->getDecryptedDataTable(tuiDataWrapper);
        if(tuiDataWrapper)
        {
            tuiDataWrapper->release();
            tuiDataWrapper = nullptr;
        }
        
        clientServer->relayHostResponseToClient(decryptedDataTable);
        
            
        if(serverData.data)
        {
            free(serverData.data);
        }
        if(decryptedDataTable)
        {
            decryptedDataTable->release();
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
