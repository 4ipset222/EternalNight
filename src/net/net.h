#ifndef __NET_H__
#define __NET_H__

#include <stdint.h>
#include <stdbool.h>

#if defined(_WIN32) || defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct NetSocket
{
#ifdef _WIN32
    SOCKET handle;
#else
    int handle;
#endif
} NetSocket;

#define NET_MAX_MESSAGE 2048
#define NET_BUFFER_SIZE 8192

typedef struct NetRecvBuffer
{
    uint8_t data[NET_BUFFER_SIZE];
    int len;
} NetRecvBuffer;

typedef struct NetSendBuffer
{
    uint8_t data[NET_BUFFER_SIZE];
    int len;
    int offset;
} NetSendBuffer;

bool Net_Init(void);
void Net_Shutdown(void);

NetSocket Net_CreateTCP(void);
bool Net_IsValid(NetSocket s);
void Net_Close(NetSocket* s);
bool Net_SetNonBlocking(NetSocket* s, bool nonBlocking);

bool Net_Bind(NetSocket* s, uint16_t port);
bool Net_Listen(NetSocket* s);
NetSocket Net_Accept(NetSocket* s, uint32_t* outAddr, uint16_t* outPort);

bool Net_Connect(NetSocket* s, const char* host, uint16_t port);
int  Net_Send(NetSocket* s, const void* data, int len);
int  Net_Recv(NetSocket* s, void* data, int len);
bool Net_WouldBlock(void);

void NetRecvBuffer_Init(NetRecvBuffer* b);
bool NetRecvBuffer_Push(NetRecvBuffer* b, const uint8_t* data, int len);
bool NetRecvBuffer_PopMessage(NetRecvBuffer* b, uint8_t* outType, uint8_t* outPayload, int* outPayloadLen);

void NetSendBuffer_Init(NetSendBuffer* b);
bool NetSendBuffer_Append(NetSendBuffer* b, const uint8_t* data, int len);
bool NetSendBuffer_Flush(NetSendBuffer* b, NetSocket* s);

#ifdef __cplusplus
}
#endif

#endif // __NET_H__
