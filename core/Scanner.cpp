
#include "Scanner.h"
#include "ClientNetInterface.h"


// Source - https://stackoverflow.com/a/10838854
// Posted by kgriffs, modified by community. See post 'Timeline' for change history
// Retrieved 2026-04-16, License - CC BY-SA 4.0

#ifdef _MSC_VER

#include <winsock2.h> 
#include <iphlpapi.h> 
#include <stdio.h> 
#include <ws2tcpip.h>

static inline std::vector<std::string> getLocalIPs()
{
  IP_ADAPTER_ADDRESSES* adapter_addresses(NULL);
  IP_ADAPTER_ADDRESSES* adapter(NULL);

  std::vector<std::string> result;

  // Start with a 16 KB buffer and resize if needed -
  // multiple attempts in case interfaces change while
  // we are in the middle of querying them.
  DWORD adapter_addresses_buffer_size = 16 * 1024;
  for (int attempts = 0; attempts != 3; ++attempts)
  {
    adapter_addresses = (IP_ADAPTER_ADDRESSES*)malloc(adapter_addresses_buffer_size);
    assert(adapter_addresses);

    DWORD error = ::GetAdaptersAddresses(
      AF_UNSPEC,
      GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST |
        GAA_FLAG_SKIP_DNS_SERVER |
        GAA_FLAG_SKIP_FRIENDLY_NAME,
      NULL,
      adapter_addresses,
      &adapter_addresses_buffer_size);

    if (ERROR_SUCCESS == error)
    {
      // We're done here, people!
      break;
    }
    else if (ERROR_BUFFER_OVERFLOW == error)
    {
      // Try again with the new size
      free(adapter_addresses);
      adapter_addresses = NULL;

      continue;
    }
    else
    {
      // Unexpected error code - log and throw
      free(adapter_addresses);
      adapter_addresses = NULL;
        MJError("error finding local IP address");
        return result;
    }
  }

  // Iterate through all of the adapters
  for (adapter = adapter_addresses; NULL != adapter; adapter = adapter->Next)
  {
      MJLog("adapter");
    // Skip loopback adapters
    if (IF_TYPE_SOFTWARE_LOOPBACK == adapter->IfType)
    {
      continue;
    }

    // Parse all IPv4 and IPv6 addresses
    for (
      IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress;
      NULL != address;
      address = address->Next)
    {
      auto family = address->Address.lpSockaddr->sa_family;
      if (AF_INET == family)
      {
        // IPv4
        SOCKADDR_IN* ipv4 = reinterpret_cast<SOCKADDR_IN*>(address->Address.lpSockaddr);

        char str_buffer[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &(ipv4->sin_addr), str_buffer, INET_ADDRSTRLEN);
        result.push_back(str_buffer);
      }
      else
      {
        // Skip all other types of addresses
        continue;
      }
    }
  }

  // Cleanup
  free(adapter_addresses);
  adapter_addresses = NULL;

  return result;
}

#else


#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include "NetConstants.h"


static inline std::vector<std::string> getLocalIPs()
{
    std::vector<std::string> result;
    struct ifaddrs *ipAddrStruct = NULL;
    struct ifaddrs *ipPtr = NULL;
    getifaddrs(&ipAddrStruct);
    
    char ipBuf[INET_ADDRSTRLEN];

    struct in_addr *ipv4Sockaddr;
    for(ipPtr = ipAddrStruct; ipPtr != NULL; ipPtr = ipPtr->ifa_next) {
        if(ipPtr->ifa_addr->sa_family ==  AF_INET) {
            ipv4Sockaddr = &((struct sockaddr_in *)ipPtr->ifa_addr)->sin_addr;
            inet_ntop(AF_INET,ipv4Sockaddr,ipBuf,INET_ADDRSTRLEN);
            result.push_back(ipBuf);
        }
    }

    free(ipAddrStruct);
    
    return result;
}

#endif


static inline std::vector<std::string> getScanIPs()
{
    std::vector<std::string> scanIPs;
    scanIPs.push_back("127.0.0.1");
    std::vector<std::string> localIPs = getLocalIPs();
    for(const std::string& localIP : localIPs)
    {
        if(localIP.find("127.") != 0)
        {
            std::string baseString = "";
            int dotCount = 0;
            for(int c = 0;; c++)
            {
                baseString += localIP[c];
                if(localIP[c] == '.')
                {
                    dotCount++;
                    if(dotCount == 3)
                    {
                        break;
                    }
                }
            }

            for(int i = 0; i < 256; i++)
            {
                std::string thisIP = baseString + Tui::string_format("%d", i);
                if(thisIP != localIP)
                {
                    scanIPs.push_back(thisIP);
                }
            }
        }
    }

    return scanIPs;
}

void Scanner::handleReceivedData(std::string ip, ENetEvent& event)
{
    if(connectionsByIP.count(ip) == 0)
    {
        return;
    }
    
    
    bool success = false;
    
    ScannerConnection& connection = connectionsByIP[ip];
    
    if(event.packet->dataLength < 1)
    {
        MJLog("packet too small");
    }
    else
    {
        ServerData incoming;
        incoming.type = ((uint8_t*)(event.packet->data))[0];
        if(event.packet->dataLength > 1)
        {
            incoming.data = &(((uint8_t*)(event.packet->data))[1]);
            incoming.length = event.packet->dataLength - 1;
        }
        else
        {
            incoming.data = NULL;
            incoming.length = 0;
        }
        
        if(incoming.type == KATIPO_NET_TYPE_SERVER_JOIN_RESPONSE_REJECT)
        {
            MJLog("rejected");
        }
        else if(incoming.type == KATIPO_NET_TYPE_INITIAL_HANDSHAKE)
        {
            connection.trackerPublicKey = std::string((const char*)incoming.data, incoming.length);
            success = true;
            
            std::string trackerPort = "3471";
            std::string trackerKey = ip + ":" + trackerPort;
            
            TuiFunction* getSitesCallbackFunction = new TuiFunction([this, ip, trackerKey](TuiTable* incomingCallbackResponseData, TuiRef* existingResult, TuiFunctionCallData* incomingCallData, TuiDebugInfo* callingDebugInfo) -> TuiRef* {
                
                if(incomingCallbackResponseData && !incomingCallbackResponseData->arrayObjects.empty() && incomingCallbackResponseData->arrayObjects[0]->type() == Tui_ref_type_TABLE)
                {
                    TuiTable* result = (TuiTable*)(incomingCallbackResponseData->arrayObjects[0]);
                    
                    if(callbackFunction)
                    {
                        TuiString* statusString = new TuiString("connected");
                        TuiTable* trackerResult = new TuiTable();
                        
                        trackerResult->setString("ip", ip);
                        trackerResult->setString("trackerKey", trackerKey);
                        trackerResult->set("sites", result->get("sites"));
                        
                        TuiRef* callbackResult = callbackFunction->call("Scanner.cpp connected to tracker", statusString, trackerResult);
                        statusString->release();
                        trackerResult->release();
                        
                        if(!(callbackResult && callbackResult->boolValue()))
                        {
                            ScannerConnection& connection = connectionsByIP[ip];
                            enet_peer_disconnect(connection.enetPeer, 0);
                            enet_peer_reset(connection.enetPeer);
                            connection.enetPeer = nullptr;
                            enet_host_destroy(connection.enetClient);
                            connection.enetClient = nullptr;
                            
                            //MJLog("erase due to no sites:%s", ip.c_str());
                            completedIPsToRemove.insert(ip);
                        }
                        
                    }
                }
                return TUI_NIL;
            });
            
            connection.netInterface = new ClientNetInterface(ip,
                                                             trackerPort,
                                                             publicKey,
                                                             secretKey,
                                                             connection.trackerPublicKey,
                                                             connection.enetClient,
                                                             connection.enetPeer);
            
            connection.netInterface->bindTui(katipoTable);
            
            TuiTable* remoteGetSitesFuncCallArgs = new TuiTable(nullptr);
            remoteGetSitesFuncCallArgs->pushString("getSitesForBroadcastKey");
            remoteGetSitesFuncCallArgs->pushString(broadcastKey);
            remoteGetSitesFuncCallArgs->push(getSitesCallbackFunction);
            
            getSitesCallbackFunction->release();
            
            connection.netInterface->callTrackerFunction(remoteGetSitesFuncCallArgs);
            //MJLog("calling getSitesForBroadcastKey:%s", trackerKey.c_str());
            
            remoteGetSitesFuncCallArgs->release();
            
            //todo simple passphrase protection, basically just like anonymous wifi?
            
        }
    }
    
    
    if(!success)
    {
        enet_peer_disconnect(connection.enetPeer, 0);
        enet_peer_reset(connection.enetPeer);
        connection.enetPeer = nullptr;
        enet_host_destroy(connection.enetClient);
        connection.enetClient = nullptr;
        
        //MJLog("erase:%s", ip.c_str());
        completedIPsToRemove.insert(ip);
    }
}

void Scanner::update()
{
    if(!complete)
    {
        for(auto& ipAndConnection : connectionsByIP)
        {
            if(ipAndConnection.second.netInterface)
            {
                ipAndConnection.second.netInterface->pollNetEvents();
            }
        }
        
        int maxCount = 64;
        for(int i = 0; i < maxCount && scanIndex < scanIPs.size(); i++)
        {
            const std::string& scanIP = scanIPs[scanIndex++];
            //MJLog("scanIP:%s", scanIP.c_str());
            
            ScannerConnection& connection = connectionsByIP[scanIP];
            if(!connection.enetClient && !connection.netInterface)
            {
                connection.enetClient = enet_host_create (nullptr, // create a client host
                                                          1,
                                                          CLIENT_MAX_SIMULTANEOUS_DOWNLOADS, //channels
                                                          0,
                                                          0);
                if(connection.enetClient)
                {
                    ENetAddress address;
                    enet_address_set_host (&address, scanIP.c_str());
                    address.port = 3471;
                    connection.enetPeer = enet_host_connect(connection.enetClient, &address, CLIENT_MAX_SIMULTANEOUS_DOWNLOADS, 0);
                    enet_peer_timeout(connection.enetPeer, 0, 2000, 3000);
                }
                else
                {
                    connectionsByIP.erase(scanIP);
                }
            }
        }
    }
    
    for(auto& ipAndConnection : connectionsByIP)
    {
        ScannerConnection& connection = ipAndConnection.second;
        if(!connection.netInterface) //otherwise netInterface will handle it in pollNetEvents
        {
            ENetEvent event;
            while(connection.enetClient && !connection.netInterface && enet_host_service(connection.enetClient, &event, 0) > 0)
            {
                switch (event.type)
                {
                    case ENET_EVENT_TYPE_CONNECT:
                    {
                        MJLog("Scanner Initial connection established:%s", ipAndConnection.first.c_str());
                    }
                        break;
                    case ENET_EVENT_TYPE_RECEIVE:
                    {
                         MJLog("ENET_EVENT_TYPE_RECEIVE:%s", ipAndConnection.first.c_str());
                        handleReceivedData(ipAndConnection.first, event);
                        enet_packet_destroy (event.packet);
                    }
                        break;
                    case ENET_EVENT_TYPE_DISCONNECT:
                    {
                         MJLog("ENET_EVENT_TYPE_DISCONNECT:%s", ipAndConnection.first.c_str());
                        enet_peer_disconnect(connection.enetPeer, 0);
                        enet_peer_reset(connection.enetPeer);
                        connection.enetPeer = nullptr;
                        enet_host_destroy(connection.enetClient);
                        connection.enetClient = nullptr;
                        
                        completedIPsToRemove.insert(ipAndConnection.first);
                    }
                        break;
                    default:
                        break;
                }
                
            }
        }
    }
    
    for(auto& completedIP : completedIPsToRemove)
    {
        MJLog("completedIPsToRemove:%s", completedIP.c_str());
        connectionsByIP.erase(completedIP);
    }
    completedIPsToRemove.clear();
    
    if(!complete && scanIndex >= scanIPs.size() && connectionsByIP.empty())
    {
        if(!hasTriedAgain) //OS security features will block the first attempt while prompting the user to allow access
        {
            MJLog("Found no results, trying again...");
            hasTriedAgain = true;
            scanIndex = 0;
        }
        else
        {
            complete = true;
            if(callbackFunction)
            {
                TuiString* statusString = new TuiString("complete");
                callbackFunction->call("Scanner.cpp scan complete", statusString);
                statusString->release();
                callbackFunction->release();
                callbackFunction = nullptr;
            }
        }
    }
}

void Scanner::cleanupPreviousScan()
{
    if(callbackFunction)
    {
        callbackFunction->release();
        callbackFunction = nullptr;
    }
    
    for(auto& ipAndConnection : connectionsByIP)
    {
        if(ipAndConnection.second.netInterface)
        {
            delete ipAndConnection.second.netInterface;
        }
        else
        {
            ENetHost* enetClient = ipAndConnection.second.enetClient;
            ENetPeer* enetPeer = ipAndConnection.second.enetPeer;
            MJLog("cleanupPreviousScan");
            
            if(enetPeer)
            {
                enet_peer_disconnect(enetPeer, 0);
                enet_peer_reset(enetPeer);
            }
            if(enetClient)
            {
                enet_host_destroy(enetClient);
            }
        }
    }
    
    completedIPsToRemove.clear();
    connectionsByIP.clear();
}

void Scanner::startScan(std::string broadcastKey_, TuiFunction* callbackFunction_)
{
    enet_initialize();
    cleanupPreviousScan();
    callbackFunction = callbackFunction_;
    broadcastKey = broadcastKey_;
    complete = false;
    scanIndex = 0;
    hasTriedAgain = false;
    
    if(callbackFunction)
    {
        callbackFunction->retain();
    }
    
    scanIPs = getScanIPs();
    
    if(!scanIPs.empty())
    {
        scanIndex = 0;
    }
    else
    {
        scanIndex = -1;
        complete = true;
        
        if(callbackFunction)
        {
            TuiString* statusString = new TuiString("error");
            callbackFunction->call("Scanner.cpp scanIPs.empty() callback", statusString);
            statusString->release();
            callbackFunction->release();
            callbackFunction = nullptr;
        }
    }
}

Scanner::Scanner(std::string& publicKey_, std::string& secretKey_, TuiTable* katipoTable_)
{
    publicKey = publicKey_;
    secretKey = secretKey_;
    katipoTable = katipoTable_;
}

Scanner::~Scanner()
{
    cleanupPreviousScan();
}


ScannerConnection Scanner::getConnection(std::string ip) //caller is responsible for closing the returned connection
{
    if(connectionsByIP.count(ip) == 0)
    {
        ScannerConnection connection;
        return connection;
    }
    ScannerConnection connection = connectionsByIP[ip];
    connectionsByIP.erase(ip);
    return connection;
}
