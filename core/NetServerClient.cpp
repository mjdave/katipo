
#include "NetServerClient.h"
#include "ServerNetInterface.h"
#include "sodium.h"

NetServerClient::NetServerClient(std::string publicKey_,
                                 ServerNetInterface* netInterface_,
                                 ENetPeer* enetPeer_,
                                 TuiTable* initialData_)
{
    enetPeer = enetPeer_;
    netInterface = netInterface_;
    publicKey = publicKey_;
    
    if(initialData_)
    {
        initialData = initialData_;
        initialData->retain();
    }
    
    
    if(publicKey.length() != 32)
    {
        MJError("Invalid publicKey in credentials of join request. Length:%d", (int)publicKey.length());
        clientID = "invalid";
        return;
    }
    
    
    clientID = clientIDForPublicKey(publicKey);

    
    valid = true;
}


NetServerClient::~NetServerClient()
{
    if(initialData)
    {
        initialData->release();
    }
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

TuiTable* NetServerClient::getEncryptedDataTable(TuiTable* dataToSecureTable, const std::string& serverPublicKey, const std::string& serverSecretKey)
{
    std::string nonce;
    nonce.resize(crypto_box_NONCEBYTES);
    randombytes_buf(&nonce[0], crypto_box_NONCEBYTES);
    
    std::string dataToSecureSerialized = dataToSecureTable->serializeBinary();
    std::string cipherText;
    cipherText.resize(crypto_box_MACBYTES + dataToSecureSerialized.length());
    
    if (crypto_box_easy((unsigned char*)&(cipherText[0]),
                        (unsigned char*)&(dataToSecureSerialized[0]),
                        dataToSecureSerialized.length(),
                        (unsigned char*)nonce.c_str(),
                        (unsigned char*)publicKey.c_str(),
                        (unsigned char*)serverSecretKey.c_str()) != 0)
    {
        return nullptr;
    }
    
    TuiTable* sendTable = new TuiTable(nullptr);
    
    sendTable->setString("nonce", nonce);
    sendTable->setString("publicKey", serverPublicKey);
    sendTable->setString("data", cipherText);
    
    return sendTable;
}
