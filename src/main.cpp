#include <algorithm>
#include <cstdlib>
#include <vector>
#include "engine/forge.h"
#include "inventory.h"
#include "net/client.h"
#include "net/server.h"
#include "player.h"
#include "ui/imgui_lite.h"
#include "world.h"
#include "multiplayer_types.h"
#include "ui/multiplayer_ui.h"
#ifdef _WIN32
#include <windows.h>
#endif

static bool LaunchServerProcess(uint16_t port)
{
#ifdef _WIN32
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "EternalNight-srv.exe %u", (unsigned int)port);
    STARTUPINFOA si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    BOOL ok = CreateProcessA(
        NULL,
        cmd,
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );
    if (!ok)
        return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#else
    (void)port;
    return false;
#endif
}

struct RemotePlayer
{
    uint8_t id;
    Player player;
};

struct PendingInput
{
    uint16_t seq;
    float dt;
    float moveX;
    float moveY;
};

static Color ColorForId(uint8_t id)
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

static RemotePlayer* FindRemote(std::vector<RemotePlayer>& list, uint8_t id)
{
    for (auto& rp : list)
    {
        if (rp.id == id)
            return &rp;
    }
    return nullptr;
}
int main(int argc, char** argv)
{
    Forge_Init();

    Window* window = Window_Create(800, 600, "Eternal Night");
    Renderer* renderer = Renderer_Create(window);
    SetGlobalRenderer(renderer);

    const int WORLD_SEED = 12345;
    World world(WORLD_LOAD_RADIUS_CHUNKS, WORLD_SEED);

    Camera2D camera = { 0.0f, 0.0f, 1.0f };
    Player player(0.0f, 0.0f);

    Inventory inventory;
    Texture2D* itemSprite = LoadTexture("test.png");
    Texture2D* swordSprite = LoadTexture("kuzne4ik_sword.png");
    inventory.AddItem("fimoz", Color{1,0,0,1}, itemSprite, ITEM_MISC);
    inventory.AddItem("giga fimoz", Color{0,1,0,1}, itemSprite, ITEM_MISC);
    inventory.AddItem("kuzne4ik sword", Color{1,1,1,1}, swordSprite, ITEM_WEAPON);

    ImGuiLiteContext ui = {};
    MpMode mpMode = MpMode::None;
    ServerState server = {};
    ClientState client = {};
    bool serverReady = false;
    bool clientReady = false;
    char ipBuffer[64] = "127.0.0.1";
    char portBuffer[16] = "7777";
    std::vector<RemotePlayer> remotePlayers;
    float mobSyncTimer = 0.0f;
    std::vector<PendingInput> pendingInputs;
    uint16_t inputSeq = 0;
    float attackBufferTimer = 0.0f;
    float lastAttackDirX = 1.0f;
    float lastAttackDirY = 0.0f;

    bool showDebug = false;
    bool prevF3 = false;

    float fogRadius = 200.0f;
    float fogStrength = 0.0f;
    const float DAY_DURATION = 300.0f;
    const float NIGHT_DURATION = 180.0f;
    const float NIGHT_FADE = 10.0f;
    bool isNight = false;
    float cycleTimer = 0.0f;

    while (!Window_ShouldClose(window))
    {
        Window_PollEvents(window);
        float dt = (float)GetDeltaTime();

        ImGuiLite_BeginFrame(&ui);

        inventory.Update();

        bool multiplayerActive = (mpMode != MpMode::None);

        Item* selected = inventory.GetSelectedItem();
        Texture2D* weaponSprite = (selected && selected->type == ITEM_WEAPON) ? selected->sprite : nullptr;
        player.SetWeaponSprite(weaponSprite);

        static float uiX = 10.0f;
        static float uiY = 10.0f;
        const float uiW = 260.0f;
        const float uiH = 230.0f;
        float uiMouseX = (float)Input_GetMouseX();
        float uiMouseY = (float)Input_GetMouseY();
        bool mouseInUi = uiMouseX >= uiX && uiMouseX <= uiX + uiW &&
                         uiMouseY >= uiY && uiMouseY <= uiY + uiH;
        bool uiBlockInput = mouseInUi || ui.kbdId != 0 || ui.activeId != 0;

        NetInputState netInput = {};
        if (!uiBlockInput)
        {
            if (Input_IsKeyDown(KEY_W)) netInput.moveY -= 1.0f;
            if (Input_IsKeyDown(KEY_S)) netInput.moveY += 1.0f;
            if (Input_IsKeyDown(KEY_A)) netInput.moveX -= 1.0f;
            if (Input_IsKeyDown(KEY_D)) netInput.moveX += 1.0f;
        }

        float halfW = (float)renderer->width * 0.5f;
        float halfH = (float)renderer->height * 0.5f;
        camera.x = player.GetX() - halfW / camera.zoom;
        camera.y = player.GetY() - halfH / camera.zoom;

        if (!uiBlockInput && Input_IsKeyDown(KEY_Q)) camera.zoom += 0.5f * dt;
        if (!uiBlockInput && Input_IsKeyDown(KEY_E)) camera.zoom -= 0.5f * dt;
        camera.zoom = std::clamp(camera.zoom, 0.1f, 5.0f);

        const float NET_PLAYER_SPEED = 200.0f;

        if (!multiplayerActive)
        {
            player.Update(dt, netInput.moveX, netInput.moveY, world.GetRaw(), world.GetTileSize());

            if (weaponSprite && Input_IsMousePressed(MOUSE_LEFT))
            {
                float mx = (float)Input_GetMouseX();
                float my = (float)Input_GetMouseY();
                float worldMx = camera.x + mx / camera.zoom;
                float worldMy = camera.y + my / camera.zoom;
                float dirX = worldMx - player.GetX();
                float dirY = worldMy - player.GetY();

                if (player.Attack(dirX, dirY))
                {
                    World_PlayerAttack(
                        world.GetRaw(),
                        player.GetX(), player.GetY(),
                        dirX, dirY,
                        player.GetAttackRange(),
                        player.GetAttackArcCos(),
                        player.GetAttackDamage()
                    );
                }
            }
        }
        else
        {
            bool skipSnapshot = false;
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

            if (mpMode == MpMode::Client && clientReady)
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
                World_MoveWithCollision(world.GetRaw(), world.GetTileSize(), radius, &px, &py, mx * NET_PLAYER_SPEED * dt, my * NET_PLAYER_SPEED * dt);
                player.SetPosition(px, py);

                inputSeq++;
                netInput.seq = inputSeq;
                pendingInputs.push_back({ inputSeq, dt, mx, my });
            }

            if (mpMode == MpMode::Host && serverReady)
            {
                Server_Update(&server, dt);
            }
            else if (mpMode == MpMode::Client && clientReady)
            {
                Client_SendInput(&client, &netInput);
                Client_Update(&client, dt);
                if (!Client_IsConnected(&client))
                {
                    mpMode = MpMode::None;
                    remotePlayers.clear();
                    clientReady = false;
                    pendingInputs.clear();
                    inputSeq = 0;
                    skipSnapshot = true;
                }
            }

            NetPlayerState snap[NET_MAX_PLAYERS];
            bool snapNight = isNight;
            float snapCycle = cycleTimer;
            int snapCount = 0;
            uint8_t localId = 0;
            if (!skipSnapshot && mpMode == MpMode::Host && serverReady)
            {
                localId = 0;
                snapCount = Server_GetSnapshot(&server, snap, NET_MAX_PLAYERS, &snapNight, &snapCycle);
            }
            else if (!skipSnapshot && mpMode == MpMode::Client && clientReady)
            {
                localId = client.playerId;
                snapCount = Client_GetSnapshot(&client, snap, NET_MAX_PLAYERS, &snapNight, &snapCycle);
            }

            bool seen[NET_MAX_PLAYERS] = {};
            float smooth = std::clamp(dt * 10.0f, 0.0f, 1.0f);
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
                            World_MoveWithCollision(world.GetRaw(), world.GetTileSize(), radius, &nx, &ny, inp.moveX * NET_PLAYER_SPEED * inp.dt, inp.moveY * NET_PLAYER_SPEED * inp.dt);
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

            isNight = snapNight;
            cycleTimer = snapCycle;
        }

        float followHalfW = (float)renderer->width * 0.5f;
        float followHalfH = (float)renderer->height * 0.5f;
        camera.x = player.GetX() - followHalfW / camera.zoom;
        camera.y = player.GetY() - followHalfH / camera.zoom;

        float playerHP = player.GetHP();
        if (multiplayerActive)
        {
            float dummyHP = playerHP;
            world.Update(dt, isNight ? 1 : 0, player.GetX(), player.GetY(), player.GetX(), player.GetY(), player.GetSize(), &dummyHP, false);
        }
        else
        {
            world.Update(dt, isNight ? 1 : 0, player.GetX(), player.GetY(), player.GetX(), player.GetY(), player.GetSize(), &playerHP, true);
            player.SetHP(playerHP);
        }

        if (mpMode == MpMode::Host && serverReady)
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
                World_UpdateMobsMulti(world.GetRaw(), dt, isNight ? 1 : 0, px, py, pcount, player.GetSize(), hp);
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
                        world.GetRaw(),
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
        }

        bool f3 = Input_IsKeyDown(KEY_F3);
        if (f3 && !prevF3)
            showDebug = !showDebug;
        prevF3 = f3;

        if (!multiplayerActive)
        {
            cycleTimer += dt;
            if (!isNight && cycleTimer >= DAY_DURATION)
            {
                isNight = true;
                cycleTimer = 0.0f;
            }
            else if (isNight && cycleTimer >= NIGHT_DURATION)
            {
                isNight = false;
                cycleTimer = 0.0f;
            }
        }

        if (isNight)
        {
            float t = cycleTimer;
            float maxFog = 0.8f;
            if (t < NIGHT_FADE)
                fogStrength = std::clamp(t / NIGHT_FADE * maxFog, 0.0f, maxFog);
            else if (t > NIGHT_DURATION - NIGHT_FADE)
                fogStrength = std::clamp((NIGHT_DURATION - t) / NIGHT_FADE * maxFog, 0.0f, maxFog);
            else
                fogStrength = maxFog;
        }
        else
        {
            fogStrength = 0.0f;
        }

        Renderer_BeginFrame(renderer);
        Renderer_Clear(Color{0.5f, 0.8f, 1.0f, 1.0f});

        BeginCameraMode(camera);
        world.Draw(camera, 800, 600, mpMode != MpMode::Client);
        if (showDebug)
        {
            const float tileSize = world.GetTileSize();
            const float chunkWorldSize = CHUNK_SIZE * tileSize;

            auto floorDiv = [](int v, int d) -> int
            {
                return v >= 0 ? v / d : (v - d + 1) / d;
            };

            int tilesXStart = (int)floorf(camera.x / tileSize);
            int tilesYStart = (int)floorf(camera.y / tileSize);
            int tilesXEnd = tilesXStart + (int)ceilf(800 / tileSize / camera.zoom) + 2;
            int tilesYEnd = tilesYStart + (int)ceilf(600 / tileSize / camera.zoom) + 2;

            int chunkXStart = floorDiv(tilesXStart, CHUNK_SIZE);
            int chunkYStart = floorDiv(tilesYStart, CHUNK_SIZE);
            int chunkXEnd = floorDiv(tilesXEnd - 1, CHUNK_SIZE);
            int chunkYEnd = floorDiv(tilesYEnd - 1, CHUNK_SIZE);

            Color chunkColor = {1.0f, 0.2f, 0.2f, 0.6f};
            for (int cy = chunkYStart; cy <= chunkYEnd; cy++)
            {
                for (int cx = chunkXStart; cx <= chunkXEnd; cx++)
                {
                    float x0 = cx * chunkWorldSize;
                    float y0 = cy * chunkWorldSize;
                    Renderer_DrawRectangleLines(Rect{x0, y0, chunkWorldSize, chunkWorldSize}, 1, chunkColor);
                }
            }
        }
        player.Draw();
        if (multiplayerActive)
        {
            for (const auto& rp : remotePlayers)
            {
                rp.player.Draw();
            }
        }
        if (mpMode == MpMode::Client && clientReady)
        {
            int typeCount = 0;
            const MobArchetype* types = World_GetMobArchetypes(world.GetRaw(), &typeCount);
            if (types)
            {
                for (int i = 0; i < client.mobCount; ++i)
                {
                    const NetMobState& m = client.mobs[i];
                    if (m.type < 0 || m.type >= typeCount)
                        continue;
                    const MobArchetype& arch = types[m.type];
                    Renderer_DrawRectangle(
                        Rect{ m.x - arch.size * 0.5f, m.y - arch.size * 0.5f, arch.size, arch.size },
                        Color{ arch.color.x, arch.color.y, arch.color.z, arch.color.w }
                    );
                }
            }
        }
        EndCameraMode();

        if (fogStrength > 0.0f)
        {
            Color nightOverlay = {0.0f, 0.0f, 0.0f, fogStrength};
            Renderer_DrawRectangle(Rect{0, 0, (float)renderer->width, (float)renderer->height}, nightOverlay);
        }

        if (showDebug)
        {
            float px = player.GetX();
            float py = player.GetY();
            float tileSize = 16.0f;

            int tileX = (int)floorf(px / tileSize);
            int tileY = (int)floorf(py / tileSize);

            int chunkX = tileX >= 0
                ? tileX / CHUNK_SIZE
                : (tileX - CHUNK_SIZE + 1) / CHUNK_SIZE;

            int chunkY = tileY >= 0
                ? tileY / CHUNK_SIZE
                : (tileY - CHUNK_SIZE + 1) / CHUNK_SIZE;

            int tileId = World_GetTile(world.GetRaw(), tileX, tileY);

            char dbg[256];
            snprintf(dbg, sizeof(dbg),
                "FPS: %.1f\n"
                "Player: %.1f, %.1f\n"
                "Tile: %d, %d (id %d)\n"
                "Chunk: %d, %d\n"
                "Zoom: %.2f\n"
                "Time: %s\n"
                "Cycle: %.2f / %.2f\n"
                "Night Fog: %.2f",
                GetFPS(),
                px, py,
                tileX, tileY, tileId,
                chunkX, chunkY,
                camera.zoom,
                isNight ? "Night" : "Day",
                cycleTimer, isNight ? NIGHT_DURATION : DAY_DURATION,
                fogStrength
            );

            Renderer_DrawTextEx(dbg, 10, 10, 16, Color{1, 1, 0, 1}, TEXT_STYLE_OUTLINE_SHADOW);

            Renderer_DrawTextEx(" WASD - move\n LMB - attack(sword)\n 1-5 - select hotbar\n Esc - toggle inventory\n Q/E - zoom\n F3 - debug info", (float)renderer->width - 140, 10, 14, Color{1, 1, 1, 1}, TEXT_STYLE_OUTLINE_SHADOW);
        }

        MpUiResult uiResult = DrawMultiplayerUI(&ui, &uiX, &uiY, uiW, uiH, mpMode, 1 + (int)remotePlayers.size(), ipBuffer, (int)sizeof(ipBuffer), portBuffer, (int)sizeof(portBuffer));

        inventory.Draw(10.0f, (float)renderer->height - 50.0f, 32.0f);
        player.DrawStamina();
        player.DrawHP();

        if (uiResult.hostClicked)
        {
            int port = atoi(portBuffer);
            if (port <= 0 || port > 65535) port = 7777;
            if (LaunchServerProcess((uint16_t)port))
            {
                if (!clientReady)
                    clientReady = Client_Init(&client);
                if (clientReady && Client_Connect(&client, "127.0.0.1", (uint16_t)port))
                {
                    mpMode = MpMode::Client;
                    remotePlayers.clear();
                    pendingInputs.clear();
                    inputSeq = 0;
                }
            }
            else
            {
                remotePlayers.clear();
            }
        }
        if (uiResult.joinClicked)
        {
            int port = atoi(portBuffer);
            if (port <= 0 || port > 65535) port = 7777;
            if (!clientReady)
                clientReady = Client_Init(&client);
            if (clientReady && Client_Connect(&client, ipBuffer, (uint16_t)port))
            {
                mpMode = MpMode::Client;
                remotePlayers.clear();
                pendingInputs.clear();
                inputSeq = 0;
            }
        }
        if (uiResult.disconnectClicked)
        {
            if (mpMode == MpMode::Host && serverReady)
                Server_Shutdown(&server);
            if (mpMode == MpMode::Client && clientReady)
                Client_Disconnect(&client);
            mpMode = MpMode::None;
            serverReady = false;
            clientReady = false;
            remotePlayers.clear();
            pendingInputs.clear();
            inputSeq = 0;
        }

        Renderer_EndFrame(renderer);

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Eternal Night - FPS: %.2f", GetFPS());
        Window_SetTitle(window, buffer);
    }

    if (clientReady)
        Client_Disconnect(&client);
    if (serverReady)
        Server_Shutdown(&server);
    if (!serverReady)
        Net_Shutdown();

    Window_Destroy(window);
    Renderer_Destroy(renderer);
    Forge_Shutdown();
    return 0;
}
