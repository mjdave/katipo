
#ifndef __NetConstants_h
#define __NetConstants_h

#include "enet/enet.h"
#include <inttypes.h>
#include <string>
#define SODIUM_STATIC

#include "MJLog.h"
#include "sodium.h"

#define LOG_NETWORK 0

static const uint32_t MJMaxPacketSize = 1024*1024*sizeof(uint32_t) - (sizeof(uint32_t) * 4); //we use an 8 bit header, and two 32 bit header values for multipart downloads, enet maximum appears to be 1024*1024*sizeof(uint32_t)

static char clientIDBuffer[128];
static inline std::string clientIDForPublicKey(const std::string& publicKey)
{
    return sodium_bin2hex(clientIDBuffer, 128,
                         (unsigned char*)&(publicKey[0]), publicKey.length());
}

struct ServerData {
    uint8_t type;
    void* data;
    size_t length;
};

enum {
    KATIPO_NET_TYPE_SERVER_JOIN_RESPONSE_ACCEPT = 1,
    KATIPO_NET_TYPE_SERVER_JOIN_RESPONSE_REJECT,//2
    KATIPO_NET_TYPE_SERVER_HEARTBEAT,//3
    KATIPO_NET_TYPE_FUNCTION_CALL_RESPONSE_TO_CLIENT_FROM_TRACKER,
    KATIPO_NET_TYPE_FUNCTION_CALL_RESPONSE_TO_CLIENT_FROM_HOST,
    KATIPO_NET_TYPE_SERVER_JOIN_INFO_RESPONSE,
    KATIPO_NET_TYPE_SERVER_MULTIPART_DOWNLOAD_RESPONSE,
    
    KATIPO_NET_TYPE_CLIENT_JOIN_REQUEST,
    KATIPO_NET_TYPE_CLIENT_PLAYER_UPDATE,
    KATIPO_NET_TYPE_CLIENT_SERVER_FUNCTION_CALL_REQUEST,
    KATIPO_NET_TYPE_CLIENT_FUNCTION_CALL_RESPONSE,
    KATIPO_NET_TYPE_CLIENT_SERVER_DOWNLOAD_FILE_REQUEST,
    KATIPO_NET_TYPE_REMOTE_HOST_REQUEST,
    
    KATIPO_NET_TYPE_INITIAL_HANDSHAKE
};

#endif // !__NetConstants_h
