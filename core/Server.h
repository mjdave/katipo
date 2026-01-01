
#ifndef Server_h
#define Server_h

#include <stdio.h>
#include "ThreadSafeQueue.h"
#include "NetConstants.h"
#include <map>
#include <set>
#include <string>
#include "TuiScript.h"

class ServerNetInterface;
class NetServerClient;
//class DatabaseEnvironment;
//class Database;


class Server {
public:
    //DatabaseEnvironment* serverDatabaseEnvironment;
    //Database* serverDatabase;
    
    std::map<std::string, NetServerClient*> clients;

	std::string hostName;
    
	bool running = false;

public:
    Server(std::string hostName_ = "localhost", std::string port_ = "7121", int maxConnections_ = 16, TuiTable* rootTable = nullptr);
    ~Server();
    
	void stop();
    bool start();
    
    void update(double dt);
    
    void addClient(NetServerClient* client);
    void removeClient(std::string clientID);
    void clientJoinGetInfoReceived(ENetPeer* enetPeer, uint32_t steamConnectionHandle, const ServerData& serverData);
    void clientDataReceived(NetServerClient* client, const ServerData& data);
    void sendDataToAllClients(uint8_t type,
                              std::string& serializedData,
                              bool reliable);
    void sendDataToClient(NetServerClient* client,
                           uint8_t type,
                           std::string& serializedData,
                          bool reliable);
    
    void sendHeartbeatToClients(bool reliable);
    
    void registerNetFunction(std::string name, std::function<TuiTable*(TuiTable*)>& func);
    
    void callClientFunctionForAllClientsInternal(std::string functionName, TuiTable* userData, std::function<void(TuiTable*)>& callback, bool reliable);
    void callClientFunctionForAllClients(std::string functionName, TuiTable* userData, std::function<void(TuiTable*)>& callback);
    void callUnreliableClientFunctionForAllClients(std::string functionName, TuiTable* userData, std::function<void(TuiTable*)>& callback);
    
    void callClientFunctionInternal(std::string functionName, std::string clientID, TuiTable* userData, std::function<void(TuiTable*)>& callback, bool reliable);
    void callClientFunction(std::string functionName, std::string clientID, TuiTable* userData, std::function<void(TuiTable*)>& callback);
    void callUnreliableClientFunction(std::string functionName, std::string clientID, TuiTable* userData, std::function<void(TuiTable*)>& callback);
    
    
protected:
    ServerNetInterface* serverNetInterface = nullptr;
    
    TuiTable* serverConfig;
    
    std::map<std::string, TuiFunction*> registeredFunctions;
    TuiFunction* clientConnectedFunction = nullptr;
    TuiFunction* clientDisconnectedFunction = nullptr;
    
	std::string port;
    int maxConnections;
    
    
    uint32_t functionCallbackIDCounter = 0;
    std::map<uint32_t, TuiFunction*> callbacksByID;
    
private:
    void loadDatabase();
    
    void bindTui(TuiTable* parentTable);
    
    void loadList(TuiTable* serverConfigRef, std::string listKey, std::set<uint64_t>& list);
    void loadWorldConfig();

};

#endif /* Server_h */
