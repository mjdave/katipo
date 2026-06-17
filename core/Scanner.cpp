
#include "Scanner.h"


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
    if(validConnectionsByIP.count(ip) == 0)
    {
        return;
    }
    
    completedIPsToRemove.insert(ip);
    
    bool success = false;
    
    ScannerConnection& connection = validConnectionsByIP[ip];
    
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
            
            //todo we need a tracker function to query for a public waraki host
            //but for now, we can assume. This is enough to show the tracker to the user, they can then attempt to connect to a host with the name 'waraki'
            //todo simple passphrase protection, basically just like anonymous wifi
            
            
        }
    }
    
    
    if(success)
    {
        if(callbackFunction)
        {
            TuiString* statusString = new TuiString("connected");
            TuiString* ipString = new TuiString(ip);
            callbackFunction->call("Scanner.cpp connected to tracker", statusString, ipString);
            statusString->release();
            ipString->release();
        }
    }
    else
    {
        enet_peer_disconnect(connection.enetPeer, 0);
        enet_peer_reset(connection.enetPeer);
        connection.enetPeer = nullptr;
        enet_host_destroy(connection.enetClient);
        connection.enetClient = nullptr;
        
        validConnectionsByIP.erase(ip);
    }
}

void Scanner::update()
{
    if(!complete)
    {
        int maxCount = 64;
        for(int i = 0; i < maxCount && scanIndex < scanIPs.size(); i++)
        {
            const std::string& scanIP = scanIPs[scanIndex++];
            //MJLog("scanIP:%s", scanIP.c_str());
            
            ScannerConnection& connection = currentlyTestingConnectionsByIP[scanIP];
            if(!connection.enetClient)
            {
                connection.enetClient = enet_host_create (nullptr, // create a client host
                                                          1,
                                                          0, //channels
                                                          0,
                                                          0);
                if(connection.enetClient)
                {
                    ENetAddress address;
                    enet_address_set_host (&address, scanIP.c_str());
                    address.port = 3471;
                    connection.enetPeer = enet_host_connect(connection.enetClient, &address, 1, 0);
                    enet_peer_timeout(connection.enetPeer, 0, 2000, 3000);
                }
                else
                {
                    currentlyTestingConnectionsByIP.erase(scanIP);
                }
            }
        }
    }
    
    for(auto& ipAndConnection : currentlyTestingConnectionsByIP)
    {
        ScannerConnection& connection = ipAndConnection.second;
        ENetEvent event;
        while(connection.enetClient && enet_host_service(connection.enetClient, &event, 0) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    validConnectionsByIP[ipAndConnection.first] = connection;
                    MJLog("Scanner Initial connection established:%s", ipAndConnection.first.c_str());
                }
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    handleReceivedData(ipAndConnection.first, event);
                    enet_packet_destroy (event.packet);
                }
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                {
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
    
    for(auto& completedIP : completedIPsToRemove)
    {
        currentlyTestingConnectionsByIP.erase(completedIP);
    }
    completedIPsToRemove.clear();
    
    if(!complete && scanIndex >= scanIPs.size() && currentlyTestingConnectionsByIP.empty())
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

void Scanner::cleanupPreviousScan()
{
    if(callbackFunction)
    {
        callbackFunction->release();
        callbackFunction = nullptr;
    }
    
    for(auto& ipAndConnection : validConnectionsByIP)
    {
        ENetHost* enetClient = ipAndConnection.second.enetClient;
        ENetPeer* enetPeer = ipAndConnection.second.enetPeer;
        
        if(enetPeer)
        {
            enet_peer_disconnect(enetPeer, 0);
            enet_peer_reset(enetPeer);
        }
        if(enetClient)
        {
            enet_host_destroy(enetClient);
        }
            
        
        /*if(enetPeer)
        {
            enet_peer_disconnect(enetPeer, 0);
            ENetEvent event;
            bool success = false;
            while (!success && enet_host_service (enetClient, &event, 3000) > 0)
            {
                switch (event.type)
                {
                    case ENET_EVENT_TYPE_RECEIVE:
                        enet_packet_destroy (event.packet);
                        break;
                    case ENET_EVENT_TYPE_DISCONNECT:
                        success = true;
                        break;
                    default:
                        break;
                }
            }
            if(!success)
            {
                enet_peer_reset(enetPeer);
            }
            enetPeer = nullptr;
        }
        
        if(enetClient)
        {
            enet_host_destroy(enetClient);
            enetClient = nullptr;
        }*/
    }
    
    validConnectionsByIP.clear();
    completedIPsToRemove.clear();
    currentlyTestingConnectionsByIP.clear();
}

void Scanner::startScan(TuiFunction* callbackFunction_)
{
    enet_initialize();
    cleanupPreviousScan();
    callbackFunction = callbackFunction_;
    complete = false;
    scanIndex = 0;
    
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

Scanner::Scanner()
{
}

Scanner::~Scanner()
{
    cleanupPreviousScan();
}


ScannerConnection Scanner::getConnection(std::string ip) //caller is responsible for closing the returned connection
{
    if(validConnectionsByIP.count(ip) == 0)
    {
        ScannerConnection connection;
        return connection;
    }
    ScannerConnection connection = validConnectionsByIP[ip];
    validConnectionsByIP.erase(ip);
    currentlyTestingConnectionsByIP.erase(ip);
    return connection;
}
