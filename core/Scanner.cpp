
#include "Scanner.h"


#ifdef _MSC_VER

#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <ws2tcpip.h>


// Source - https://stackoverflow.com/a/10838854
// Posted by kgriffs, modified by community. See post 'Timeline' for change history
// Retrieved 2026-04-16, License - CC BY-SA 4.0

void ListIpAddresses(IpAddresses& ipAddrs)
{
  IP_ADAPTER_ADDRESSES* adapter_addresses(NULL);
  IP_ADAPTER_ADDRESSES* adapter(NULL);

  // Start with a 16 KB buffer and resize if needed -
  // multiple attempts in case interfaces change while
  // we are in the middle of querying them.
  DWORD adapter_addresses_buffer_size = 16 * KB;
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
        return;//error
    }
  }

  // Iterate through all of the adapters
  for (adapter = adapter_addresses; NULL != adapter; adapter = adapter->Next)
  {
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
        ipAddrs.mIpv4.push_back(str_buffer);
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

  // Cheers!
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

static inline std::vector<std::string> getScanIPs()
{
    std::vector<std::string> scanIPs;
    std::vector<std::string> localIPs = getLocalIPs();
    for(const std::string& localIP : localIPs)
    {
        if(localIP.find("127.") != 0)
        {
            std::string baseString = "";
            int dotCount = 0;
            for(int c = 0;;c++)
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

#endif


void Scanner::update()
{
    if(scanIndex >= 0 && scanIndex < scanIPs.size())
    {
        const std::string& scanIP = scanIPs[scanIndex++];
        MJLog("scanIP:%s", scanIP.c_str());
        
        ScannerConnection& connection = currentlyTestingConnectionsByIP[scanIP];
        if(!connection.enetClient)
        {
            connection.enetClient = enet_host_create (nullptr, // create a client host
                                                      1,
                                                      0, //channels
                                                      0,
                                                      0);
            ENetAddress address;
            enet_address_set_host (&address, scanIP.c_str());
            address.port = 3471;
            connection.enetPeer = enet_host_connect(connection.enetClient, &address, 1, 0);
            enet_peer_timeout(connection.enetPeer, 0, 2000, 3000);
        }
        //
    }
    
    std::set<std::string> completedIPs;
    
    for(auto& ipAndConnection : currentlyTestingConnectionsByIP)
    {
        ScannerConnection& connection = ipAndConnection.second;
        ENetEvent event;
        while(connection.enetClient && enet_host_service(connection.enetClient, &event, 0) > 0)
        {
            bool foundResult = false;
            switch (event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                {
                    foundResult = true;
                    MJLog("Scanner Initial connection established");
                }
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    MJLog("Warning: Scanner got ENET_EVENT_TYPE_RECEIVE");
                    enet_packet_destroy (event.packet);
                }
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    MJLog("Scanner ENET_EVENT_TYPE_DISCONNECT");
                }
                    break;
                default:
                    break;
            }
            
            if(foundResult)
            {
                validConnectionsByIP[ipAndConnection.first] = connection;
            }
            else
            {
                
                enet_peer_disconnect(connection.enetPeer, 0);
                enet_peer_reset(connection.enetPeer);
                connection.enetPeer = nullptr;
                enet_host_destroy(connection.enetClient);
                connection.enetClient = nullptr;
                
                completedIPs.insert(ipAndConnection.first);
            }
        }
    }
    
    for(auto& completedIP : completedIPs)
    {
        currentlyTestingConnectionsByIP.erase(completedIP);
    }
    /*enetClient = enet_host_create (nullptr, // create a client host
                                   1,
                                   0, //channels
                                   0,
                                   0);
    
    //enet_host_compress_with_range_coder(enetClient); //NO!, lets save CPU and use the bandwidth instead.
    
    ENetAddress address;
    
    enet_address_set_host (&address, host.c_str());
    address.port = atoi(port.c_str());
    
    MJLog("Connecting to tracker...");
    enetPeer = enet_host_connect (enetClient, &address, ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT, 0);
    
    enet_peer_timeout(enetPeer, 0, 2000, 3000);*/
}


Scanner::Scanner()
{
    scanIPs = getScanIPs();
    if(!scanIPs.empty())
    {
        scanIndex = 0;
    }
    else
    {
        scanIndex = -1;
    }
}

Scanner::~Scanner()
{
    
}
