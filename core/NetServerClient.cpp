
#include "NetServerClient.h"
#include "ServerNetInterface.h"

NetServerClient::NetServerClient(TuiTable* joinRequest_,
                                 ServerNetInterface* netInterface_,
                                 ENetPeer* enetPeer_)
{
    enetPeer = enetPeer_;
    netInterface = netInterface_;
    joinRequest = joinRequest_;
    
    joinRequest->retain();
    
    TuiTable* clientInfo = joinRequest->getTable("clientInfo");
    
    if(!clientInfo)
    {
        MJError("Invalid join request, missing clientInfo");
        clientID = "invalid";
        return;
    }
    
    clientID = clientInfo->getString("clientID");
    if(clientID.length() != 16)
    {
        MJError("Invalid clientID in join request of length:%d", clientID.length());
        clientID = "invalid";
        return;
    }
    
    valid = true;
}


NetServerClient::~NetServerClient()
{
    joinRequest->release();
}

void NetServerClient::sendDataToClient(const ServerData& serverData, bool reliable)
{
    if(!valid)
    {
        return;
    }
#if LOG_NETWORK
    MJLog("server send: %d - to %s - %zu bytes", serverData.type, playerIDString.c_str(), serverData.length);
#endif
    netInterface->sendData(serverData.type, serverData.data, serverData.length, enetPeer, reliable);
}

void NetServerClient::sendLargeDataToClient(const ServerData& serverData)
{
    int freeChannel = 0;
    for(freeChannel = 0; freeChannel < MAX_SIMULTANEOUS_DOWNLOADS; freeChannel++)
    {
        if(inUseChannels.count(freeChannel) == 0)
        {
            break;
        }
    }
    
    if(freeChannel < MAX_SIMULTANEOUS_DOWNLOADS)
    {
        inUseChannels.insert(freeChannel);
        netInterface->sendLargeData(serverData.type, serverData.data, serverData.length, enetPeer, freeChannel);
    }
    else
    {
        MJError("todo");
        //queuedDownloads.push(serverData); // hmmmmm serverData.data is not valid once we exit this function
    }
}

double NetServerClient::getPingDelay()
{
    return pingDelay;
}
