
#ifndef __NetConstants_h
#define __NetConstants_h

#include "enet/enet.h"
#include <inttypes.h>
#include <string>
#define SODIUM_STATIC

#include "MJLog.h"
#include "sodium.h"

#define LOG_NETWORK 0

#define CLIENT_MAX_SIMULTANEOUS_DOWNLOADS 8 //I think absolute max is 256, this value needs to be experimented with

static const uint32_t MJMultipartChunkSize = 1024*1024; //1MB chunks. Absolute maximum for this is slightly under 8MB, before we hit a limit in enet

static char clientIDBuffer[128];
static inline std::string readableKeyForPublicKey(const std::string& publicKey)
{
    return sodium_bin2hex(clientIDBuffer, 128,
                         (unsigned char*)&(publicKey[0]), publicKey.length());
}

static inline std::string publicKeyForReadableKey(const std::string& readableKey) //untested
{
     if(sodium_hex2bin((unsigned char*)clientIDBuffer, 128,
                         (char*)&(readableKey[0]), readableKey.length(), NULL, NULL, NULL) >= 0)
     {
         return clientIDBuffer;
     }
    return "";
}

struct ServerData {
    uint8_t type;
    void* data;
    size_t length;
};

enum {
    KATIPO_NET_TYPE_SERVER_JOIN_RESPONSE_REJECT = 1,
    KATIPO_NET_TYPE_FUNCTION_CALL_RESPONSE_TO_CLIENT_FROM_TRACKER,
    KATIPO_NET_TYPE_GET_RESPONSE_TO_CLIENT_FROM_HOST,
    
    KATIPO_NET_TYPE_CLIENT_JOIN_REQUEST,
    KATIPO_NET_TYPE_CLIENT_SERVER_FUNCTION_CALL_REQUEST,
    KATIPO_NET_TYPE_REMOTE_HOST_REQUEST,
    
    KATIPO_NET_TYPE_INITIAL_HANDSHAKE
};

#endif // !__NetConstants_h
