#include "net.h"
#include <string.h>

static bool g_net_inited = false;

bool Net_Init(void)
{
#ifdef _WIN32
    if (g_net_inited)
        return true;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return false;
#endif
    g_net_inited = true;
    return true;
}

void Net_Shutdown(void)
{
#ifdef _WIN32
    if (g_net_inited)
    {
        WSACleanup();
    }
#endif
    g_net_inited = false;
}

NetSocket Net_CreateTCP(void)
{
    NetSocket s;
#ifdef _WIN32
    s.handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    s.handle = socket(AF_INET, SOCK_STREAM, 0);
#endif
    return s;
}

bool Net_IsValid(NetSocket s)
{
#ifdef _WIN32
    return s.handle != INVALID_SOCKET;
#else
    return s.handle >= 0;
#endif
}

void Net_Close(NetSocket* s)
{
    if (!s) return;
    if (!Net_IsValid(*s)) return;
#ifdef _WIN32
    closesocket(s->handle);
    s->handle = INVALID_SOCKET;
#else
    close(s->handle);
    s->handle = -1;
#endif
}

bool Net_SetNonBlocking(NetSocket* s, bool nonBlocking)
{
    if (!s || !Net_IsValid(*s)) return false;
#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    return ioctlsocket(s->handle, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(s->handle, F_GETFL, 0);
    if (flags < 0) return false;
    if (nonBlocking)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    return fcntl(s->handle, F_SETFL, flags) == 0;
#endif
}

bool Net_Bind(NetSocket* s, uint16_t port)
{
    if (!s || !Net_IsValid(*s)) return false;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    return bind(s->handle, (struct sockaddr*)&addr, sizeof(addr)) == 0;
}

bool Net_Listen(NetSocket* s)
{
    if (!s || !Net_IsValid(*s)) return false;
    return listen(s->handle, SOMAXCONN) == 0;
}

NetSocket Net_Accept(NetSocket* s, uint32_t* outAddr, uint16_t* outPort)
{
    NetSocket client;
#ifdef _WIN32
    client.handle = INVALID_SOCKET;
#else
    client.handle = -1;
#endif

    if (!s || !Net_IsValid(*s)) return client;

    struct sockaddr_in addr;
#ifdef _WIN32
    int addrlen = (int)sizeof(addr);
#else
    socklen_t addrlen = (socklen_t)sizeof(addr);
#endif
#ifdef _WIN32
    SOCKET c = accept(s->handle, (struct sockaddr*)&addr, &addrlen);
    if (c == INVALID_SOCKET) return client;
    client.handle = c;
#else
    int c = accept(s->handle, (struct sockaddr*)&addr, &addrlen);
    if (c < 0) return client;
    client.handle = c;
#endif
    if (outAddr) *outAddr = ntohl(addr.sin_addr.s_addr);
    if (outPort) *outPort = ntohs(addr.sin_port);
    return client;
}

bool Net_Connect(NetSocket* s, const char* host, uint16_t port)
{
    if (!s || !Net_IsValid(*s)) return false;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (host == NULL || host[0] == '\0')
        host = "127.0.0.1";

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1)
        return false;

    return connect(s->handle, (struct sockaddr*)&addr, sizeof(addr)) == 0;
}

int Net_Send(NetSocket* s, const void* data, int len)
{
    if (!s || !Net_IsValid(*s)) return -1;
    if (!data || len <= 0) return 0;
#ifdef _WIN32
    return send(s->handle, (const char*)data, len, 0);
#else
    return (int)send(s->handle, data, (size_t)len, 0);
#endif
}

int Net_Recv(NetSocket* s, void* data, int len)
{
    if (!s || !Net_IsValid(*s)) return -1;
#ifdef _WIN32
    return recv(s->handle, (char*)data, len, 0);
#else
    return (int)recv(s->handle, data, (size_t)len, 0);
#endif
}

bool Net_WouldBlock(void)
{
#ifdef _WIN32
    int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
#else
    return false;
#endif
}

void NetRecvBuffer_Init(NetRecvBuffer* b)
{
    if (!b) return;
    b->len = 0;
}

bool NetRecvBuffer_Push(NetRecvBuffer* b, const uint8_t* data, int len)
{
    if (!b || !data || len <= 0) return false;
    if (b->len + len > NET_BUFFER_SIZE) return false;
    memcpy(b->data + b->len, data, (size_t)len);
    b->len += len;
    return true;
}

bool NetRecvBuffer_PopMessage(NetRecvBuffer* b, uint8_t* outType, uint8_t* outPayload, int* outPayloadLen)
{
    if (!b || b->len < 3) return false;

    uint16_t msgLen = (uint16_t)(b->data[0] | (b->data[1] << 8));
    if (msgLen > NET_MAX_MESSAGE) return false;
    if (b->len < (int)(2 + msgLen)) return false;

    uint8_t type = b->data[2];
    int payloadLen = (int)msgLen - 1;

    if (outType) *outType = type;
    if (outPayload && payloadLen > 0)
        memcpy(outPayload, b->data + 3, (size_t)payloadLen);
    if (outPayloadLen) *outPayloadLen = payloadLen;

    int remaining = b->len - (2 + (int)msgLen);
    if (remaining > 0)
        memmove(b->data, b->data + 2 + msgLen, (size_t)remaining);
    b->len = remaining;

    return true;
}

void NetSendBuffer_Init(NetSendBuffer* b)
{
    if (!b) return;
    b->len = 0;
    b->offset = 0;
}

bool NetSendBuffer_Append(NetSendBuffer* b, const uint8_t* data, int len)
{
    if (!b || !data || len <= 0) return false;
    if (b->len + len > NET_BUFFER_SIZE) return false;
    memcpy(b->data + b->len, data, (size_t)len);
    b->len += len;
    return true;
}

bool NetSendBuffer_Flush(NetSendBuffer* b, NetSocket* s)
{
    if (!b || !s || !Net_IsValid(*s)) return false;
    if (b->offset >= b->len) return true;

    int toSend = b->len - b->offset;
    int sent = Net_Send(s, b->data + b->offset, toSend);
    if (sent < 0)
    {
        if (Net_WouldBlock())
            return false;
        return false;
    }
    b->offset += sent;
    if (b->offset >= b->len)
    {
        b->len = 0;
        b->offset = 0;
    }
    return true;
}
