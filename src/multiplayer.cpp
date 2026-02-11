#include "multiplayer.h"
#include "game_input.h"
#include <algorithm>
#include <cstring>
#include <cmath>

Color ColorForId(uint8_t id)
{
    static Color palette[] =
    {
        {1.0f, 0.2f, 0.2f, 1.0f},
        {0.2f, 0.8f, 1.0f, 1.0f},
        {0.4f, 1.0f, 0.4f, 1.0f},
        {1.0f, 0.8f, 0.3f, 1.0f},
        {0.8f, 0.4f, 1.0f, 1.0f},
        {1.0f, 0.4f, 0.7f, 1.0f},
        {0.6f, 0.6f, 1.0f, 1.0f},
        {0.9f, 0.9f, 0.4f, 1.0f}
    };
    return palette[id % (sizeof(palette) / sizeof(palette[0]))];
}

RemotePlayer* FindRemote(std::vector<RemotePlayer>& list, uint8_t id)
{
    for (auto& rp : list)
    {
        if (rp.id == id)
            return &rp;
    }
    return nullptr;
}

void UpdateMultiplayerInput(Player& player, World* world, NetInputState& netInput,
                           Texture2D* weaponSprite, bool uiBlockInput,
                           float& attackBufferTimer, float& lastAttackDirX, float& lastAttackDirY,
                           const Camera2D& camera, float dt)
{
    if (attackBufferTimer > 0.0f)
        attackBufferTimer -= dt;
    if (weaponSprite && !uiBlockInput && Input_IsMousePressed(MOUSE_LEFT))
    {
        float mx = (float)Input_GetMouseX();
        float my = (float)Input_GetMouseY();
        float worldMx = camera.x + mx / camera.zoom;
        float worldMy = camera.y + my / camera.zoom;
        lastAttackDirX = worldMx - player.GetX();
        lastAttackDirY = worldMy - player.GetY();
        attackBufferTimer = 0.20f;
    }
    if (attackBufferTimer > 0.0f)
    {
        netInput.attack = 1;
        netInput.attackDirX = lastAttackDirX;
        netInput.attackDirY = lastAttackDirY;
    }
}

void UpdateClientMovement(Player& player, World* world, NetInputState& netInput,
                         uint16_t& inputSeq, std::vector<PendingInput>& pendingInputs, float dt)
{
    float mx = netInput.moveX;
    float my = netInput.moveY;
    float lenSq = mx * mx + my * my;
    if (lenSq > 1.0f)
    {
        float inv = 1.0f / sqrtf(lenSq);
        mx *= inv;
        my *= inv;
    }
    float px = player.GetX();
    float py = player.GetY();
    float radius = player.GetSize() * 0.45f;
    const float NET_PLAYER_SPEED = 200.0f;
    World_MoveWithCollision(world->GetRaw(), world->GetTileSize(), radius, &px, &py, 
                           mx * NET_PLAYER_SPEED * dt, my * NET_PLAYER_SPEED * dt);
    player.SetPosition(px, py);

    inputSeq++;
    netInput.seq = inputSeq;
    pendingInputs.push_back({ inputSeq, dt, mx, my });
}

void ProcessNetworkSnapshot(const NetPlayerState* snap, int snapCount, uint8_t localId,
                           Player& player, std::vector<RemotePlayer>& remotePlayers,
                           std::vector<PendingInput>& pendingInputs, World* world,
                           Texture2D* swordSprite, MpMode mpMode, float dt)
{
    bool seen[NET_MAX_PLAYERS] = {};
    float smooth = std::clamp(dt * 10.0f, 0.0f, 1.0f);
    const float NET_PLAYER_SPEED = 200.0f;
    
    for (int i = 0; i < snapCount; ++i)
    {
        const NetPlayerState& ps = snap[i];
        seen[ps.id] = true;
        if (ps.id == localId)
        {
            player.SetPosition(ps.x, ps.y);
            player.SetHP(ps.hp);
            player.SetAttackState(ps.isAttacking != 0, ps.attackProgress, ps.attackDirX, ps.attackDirY, ps.attackBaseAngle);
            player.SetColor(ColorForId(ps.id));

            if (mpMode == MpMode::Client)
            {
                while (!pendingInputs.empty() && pendingInputs.front().seq <= ps.lastInputSeq)
                {
                    pendingInputs.erase(pendingInputs.begin());
                }
                for (const auto& inp : pendingInputs)
                {
                    float nx = player.GetX();
                    float ny = player.GetY();
                    float radius = player.GetSize() * 0.45f;
                    World_MoveWithCollision(world->GetRaw(), world->GetTileSize(), radius, &nx, &ny, 
                                           inp.moveX * NET_PLAYER_SPEED * inp.dt, inp.moveY * NET_PLAYER_SPEED * inp.dt);
                    player.SetPosition(nx, ny);
                }
            }
        }
        else
        {
            RemotePlayer* rp = FindRemote(remotePlayers, ps.id);
            if (!rp)
            {
                remotePlayers.push_back({ ps.id, Player(ps.x, ps.y) });
                rp = &remotePlayers.back();
                rp->player.SetColor(ColorForId(ps.id));
                rp->player.SetWeaponSprite(swordSprite);
            }
            if (mpMode == MpMode::Client)
            {
                float rx = rp->player.GetX() + (ps.x - rp->player.GetX()) * smooth;
                float ry = rp->player.GetY() + (ps.y - rp->player.GetY()) * smooth;
                rp->player.SetPosition(rx, ry);
            }
            else
            {
                rp->player.SetPosition(ps.x, ps.y);
            }
            rp->player.SetHP(ps.hp);
            rp->player.SetAttackState(ps.isAttacking != 0, ps.attackProgress, ps.attackDirX, ps.attackDirY, ps.attackBaseAngle);
            rp->player.SetWeaponSprite(swordSprite);
        }
    }

    for (size_t i = 0; i < remotePlayers.size();)
    {
        if (!seen[remotePlayers[i].id])
            remotePlayers.erase(remotePlayers.begin() + (int)i);
        else
            ++i;
    }
}

void UpdateHostMobs(Player& player, World* world, ServerState& server, 
                   float& mobSyncTimer, bool isNight, float dt)
{
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
        World_UpdateMobsMulti(world->GetRaw(), dt, isNight ? 1 : 0, px, py, pcount, player.GetSize(), hp);
        for (int p = 0; p < pcount; ++p)
        {
            int idx = indexMap[p];
            server.clients[idx].hp = hp[p];
            if (idx == 0)
                player.SetHP(hp[p]);
        }
    }

    for (int i = 0; i < NET_MAX_PLAYERS; ++i)
    {
        if (!server.clients[i].connected)
            continue;
        if (server.clients[i].attackQueued)
        {
            World_PlayerAttack(
                world->GetRaw(),
                server.clients[i].x, server.clients[i].y,
                server.clients[i].attackDirX, server.clients[i].attackDirY,
                player.GetAttackRange(),
                player.GetAttackArcCos(),
                player.GetAttackDamage()
            );
            server.clients[i].attackQueued = false;
        }
    }

    mobSyncTimer += dt;
    if (mobSyncTimer >= 0.05f)
    {
        mobSyncTimer = 0.0f;
        int mobCount = 0;
        const Mob* mobs = World_GetMobs(world->GetRaw(), &mobCount);
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
}
