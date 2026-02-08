#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>
#include <cstdlib>
#include <cstring>
#include "world.h"
#include "net/server.h"

static const int WORLD_SEED = 12345;
static const float PLAYER_RADIUS = 16.0f * 0.45f;
static const float PLAYER_ATTACK_RANGE = 55.0f;
static const float PLAYER_ATTACK_ARC_COS = 0.35f;
static const float PLAYER_ATTACK_DAMAGE = 12.0f;

int main(int argc, char** argv)
{
    uint16_t port = 7777;
    if (argc > 1)
    {
        int p = atoi(argv[1]);
        if (p > 0 && p <= 65535) port = (uint16_t)p;
    }

    ServerState server = {};
    if (!Server_Init(&server, port, WORLD_SEED))
        return 1;

    World world(WORLD_LOAD_RADIUS_CHUNKS, WORLD_SEED);
    Server_SetWorld(&server, world.GetRaw(), world.GetTileSize(), PLAYER_RADIUS);

    float mobSyncTimer = 0.0f;
    auto last = std::chrono::steady_clock::now();

    while (server.running)
    {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = now - last;
        last = now;
        float dt = std::clamp(elapsed.count(), 0.0f, 0.1f);

        Server_Update(&server, dt);

        float px[NET_MAX_PLAYERS];
        float py[NET_MAX_PLAYERS];
        float hp[NET_MAX_PLAYERS];
        int indexMap[NET_MAX_PLAYERS];
        int pcount = 0;

        for (int i = 0; i < NET_MAX_PLAYERS; ++i)
        {
            if (!server.clients[i].connected)
                continue;
            px[pcount] = server.clients[i].x;
            py[pcount] = server.clients[i].y;
            hp[pcount] = server.clients[i].hp;
            indexMap[pcount] = i;
            pcount++;
        }

        if (pcount > 0)
        {
            World_UpdateMobsMulti(world.GetRaw(), dt, server.isNight ? 1 : 0, px, py, pcount, 16.0f, hp);
            for (int p = 0; p < pcount; ++p)
            {
                int idx = indexMap[p];
                server.clients[idx].hp = hp[p];
            }
        }

        for (int i = 0; i < NET_MAX_PLAYERS; ++i)
        {
            if (!server.clients[i].connected)
                continue;
            if (server.clients[i].attackQueued)
            {
                World_PlayerAttack(
                    world.GetRaw(),
                    server.clients[i].x, server.clients[i].y,
                    server.clients[i].attackDirX, server.clients[i].attackDirY,
                    PLAYER_ATTACK_RANGE,
                    PLAYER_ATTACK_ARC_COS,
                    PLAYER_ATTACK_DAMAGE
                );
                server.clients[i].attackQueued = false;
            }
        }

        mobSyncTimer += dt;
        if (mobSyncTimer >= 0.05f)
        {
            mobSyncTimer = 0.0f;
            int mobCount = 0;
            const Mob* mobs = World_GetMobs(world.GetRaw(), &mobCount);
            if (mobs)
            {
                uint8_t buffer[NET_MAX_MESSAGE];
                int offset = 0;
                offset += 2;
                buffer[offset++] = (uint8_t)MSG_MOBS;
                int countPos = offset++;
                uint8_t count = 0;

                for (int i = 0; i < mobCount && count < NET_MAX_MOBS; ++i)
                {
                    if (offset + 1 + (int)(sizeof(float) * 3) >= NET_MAX_MESSAGE)
                        break;
                    buffer[offset++] = (uint8_t)mobs[i].type;
                    memcpy(buffer + offset, &mobs[i].x, sizeof(float)); offset += sizeof(float);
                    memcpy(buffer + offset, &mobs[i].y, sizeof(float)); offset += sizeof(float);
                    memcpy(buffer + offset, &mobs[i].hp, sizeof(float)); offset += sizeof(float);
                    count++;
                }

                buffer[countPos] = count;
                uint16_t totalLen = (uint16_t)(offset - 2);
                buffer[0] = (uint8_t)(totalLen & 0xFF);
                buffer[1] = (uint8_t)((totalLen >> 8) & 0xFF);
                Server_Broadcast(&server, buffer, offset);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    Server_Shutdown(&server);
    return 0;
}
