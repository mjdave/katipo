
#ifndef __NetConstants_h
#define __NetConstants_h

#include "enet/enet.h"
#include <inttypes.h>
#include "string.h"

#include "MJLog.h"

#define LOG_NETWORK 0

static const uint32_t MJMaxPacketSize = 1024*1024*sizeof(uint32_t) - (sizeof(uint32_t) * 4); //we use an 8 bit header, and two 32 bit header values for multipart downloads, enet maxiomum appears to be 1024*1024*sizeof(uint32_t)

struct ServerData {
    uint8_t type;
    void* data;
    size_t length;
};

enum {
    SERVER_DATA_TYPE_SERVER_JOIN_RESPONSE_ACCEPT = 1,
    SERVER_DATA_TYPE_SERVER_JOIN_RESPONSE_REJECT,//2
    SERVER_DATA_TYPE_SERVER_HEARTBEAT,//3
    SERVER_DATA_TYPE_SERVER_CLIENT_FUNCTION_CALL_REQUEST,//4
    SERVER_DATA_TYPE_SERVER_FUNCTION_CALL_RESPONSE,//5
    SERVER_DATA_TYPE_SERVER_JOIN_INFO_RESPONSE,//6
    SERVER_DATA_TYPE_SERVER_DELAY_PING,//7
    SERVER_DATA_TYPE_SERVER_DOWNLOAD_FILE_RESPONSE,//8
    SERVER_DATA_TYPE_SERVER_MULTIPART_DOWNLOAD_RESPONSE,//9
    
    SERVER_DATA_TYPE_CLIENT_JOIN_REQUEST,//10
    SERVER_DATA_TYPE_CLIENT_PLAYER_UPDATE,//11
    SERVER_DATA_TYPE_CLIENT_SERVER_FUNCTION_CALL_REQUEST,//12
    SERVER_DATA_TYPE_CLIENT_FUNCTION_CALL_RESPONSE, //13
    SERVER_DATA_TYPE_CLIENT_SERVER_DOWNLOAD_FILE_REQUEST,//14
    SERVER_DATA_TYPE_CLIENT_SERVER_DOWNLOAD_FILE_COMPLETE_NOTIFICATION,//15
    //NOTE! Always add new types below old, as reordering may break the version check
};

#endif // !__NetConstants_h
