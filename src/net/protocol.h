#ifndef __NET_PROTOCOL_H__
#define __NET_PROTOCOL_H__

#include <stdint.h>

#define NET_PROTOCOL_VERSION 2
#define NET_MAX_PLAYERS 8
#define NET_MAX_MOBS 64

typedef enum NetMsgType
{
    MSG_HELLO = 1,
    MSG_WELCOME = 2,
    MSG_INPUT = 3,
    MSG_SNAPSHOT = 4,
    MSG_MOBS = 5
} NetMsgType;

typedef struct NetPlayerState
{
    uint8_t id;
    float x;
    float y;
    float hp;
    uint8_t isDead;
    float respawnTimer;
    uint8_t isAttacking;
    float attackProgress;
    float attackDirX;
    float attackDirY;
    float attackBaseAngle;
    uint16_t lastInputSeq;
} NetPlayerState;

typedef struct NetInputState
{
    uint16_t seq;
    float moveX;
    float moveY;
    uint8_t attack;
    float attackDirX;
    float attackDirY;
} NetInputState;

typedef struct NetMobState
{
    uint8_t type;
    float x;
    float y;
    float hp;
} NetMobState;

#endif // __NET_PROTOCOL_H__
