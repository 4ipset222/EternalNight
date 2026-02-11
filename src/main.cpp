#include <algorithm>
#include <cstdlib>
#include <vector>
#include "engine/forge.h"
#include "inventory.h"
#include "net/client.h"
#include "net/server.h"
#include "player.h"
#include "ui/imgui_lite.h"
#include "ui/main_menu.h"
#include "world.h"
#include "multiplayer_types.h"
#include "ui/multiplayer_ui.h"
#include "game_input.h"
#include "game_logic.h"
#include "game_render.h"
#include "multiplayer.h"

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

int main(int argc, char** argv)
{
    Forge_Init();

    Window* window = Window_Create(800, 600, "Eternal Night");
    Renderer* renderer = Renderer_Create(window);
    SetGlobalRenderer(renderer);

    MainMenu mainMenu;
    GameState currentGameState = STATE_MENU;
    World* world = nullptr;
    
    Camera2D camera = { 0.0f, 0.0f, 1.0f };
    Player player(0.0f, 0.0f);

    Inventory inventory;
    Texture2D* itemSprite = LoadTexture("test.png");
    Texture2D* swordSprite = LoadTexture("kuzne4ik_sword.png");
    inventory.AddItem("fimoz", Color{1,0,0,1}, itemSprite, ITEM_MISC);
    inventory.AddItem("giga fimoz", Color{0,1,0,1}, itemSprite, ITEM_MISC);
    inventory.AddItem("kuzne4ik sword", Color{1,1,1,1}, swordSprite, ITEM_WEAPON);
    
    inventory.AddItem("Stone", Color{0.5f, 0.5f, 0.5f, 1.0f}, nullptr, ITEM_BLOCK, TILE_STONE, 99);
    inventory.AddItem("Dirt", Color{0.6f, 0.4f, 0.2f, 1.0f}, nullptr, ITEM_BLOCK, TILE_DIRT, 64);
    inventory.AddItem("Grass", Color{0.2f, 0.8f, 0.2f, 1.0f}, nullptr, ITEM_BLOCK, TILE_GRASS, 64);
    inventory.AddItem("Sand", Color{0.9f, 0.8f, 0.4f, 1.0f}, nullptr, ITEM_BLOCK, TILE_SAND, 50);

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

    bool inCave = false;
    float caveEntranceX = 0.0f;
    float caveEntranceY = 0.0f;

    float fogStrength = 0.0f;
    const float DAY_DURATION = 300.0f;
    const float NIGHT_DURATION = 180.0f;
    const float NIGHT_FADE = 10.0f;
    bool isNight = false;
    float cycleTimer = 0.0f;

    static float uiX = 10.0f;
    static float uiY = 10.0f;
    const float uiW = 260.0f;
    const float uiH = 230.0f;

    while (!Window_ShouldClose(window))
    {
        Window_PollEvents(window);
        float dt = (float)GetDeltaTime();

        ImGuiLite_BeginFrame(&ui);

        if (currentGameState == STATE_MENU || currentGameState == STATE_NEW_GAME)
        {
            mainMenu.Update();
            GameState newState = mainMenu.GetGameState();
            
            if (newState == STATE_PLAYING)
            {
                const WorldConfig& cfg = mainMenu.GetWorldConfig();
                if (world)
                    delete world;
                world = new World(WORLD_LOAD_RADIUS_CHUNKS, cfg.seed);
                ForgeWorld* raw = world->GetRaw();
                raw->waterAmount = cfg.waterAmount;
                raw->stoneAmount = cfg.stoneAmount;
                raw->caveAmount = cfg.caveAmount;
                World_ReloadChunks(raw);
                currentGameState = STATE_PLAYING;
                player.SetX(0.0f);
                player.SetY(0.0f);
            }
            else if (newState == STATE_EXITING)
                break;
            else
                currentGameState = newState;
            
            Renderer_EndFrame(renderer);
            continue;
        }
        else if (currentGameState == STATE_EXITING)
            break;
        
        if (!world) continue;

        inventory.Update();
        bool multiplayerActive = (mpMode != MpMode::None);

        Item* selected = inventory.GetSelectedItem();
        Texture2D* weaponSprite = (selected && selected->type == ITEM_WEAPON) ? selected->sprite : nullptr;
        player.SetWeaponSprite(weaponSprite);
        
        float uiMouseX = (float)Input_GetMouseX();
        float uiMouseY = (float)Input_GetMouseY();
        bool mouseInUi = uiMouseX >= uiX && uiMouseX <= uiX + uiW &&
                         uiMouseY >= uiY && uiMouseY <= uiY + uiH;
        bool uiBlockInput = mouseInUi || ui.kbdId != 0 || ui.activeId != 0;

        HandleBlockPlacement(player, camera, renderer, world, inventory, uiBlockInput);

        NetInputState netInput = GetNetInput();

        float halfW = (float)renderer->width * 0.5f;
        float halfH = (float)renderer->height * 0.5f;
        camera.x = player.GetX() - halfW / camera.zoom;
        camera.y = player.GetY() - halfH / camera.zoom;

        UpdateCameraZoom(camera, dt);

        if (!multiplayerActive)
        {
            UpdateSingleplayerGame(player, world, camera, inventory, weaponSprite, uiBlockInput, dt);
        }
        else
        {
            UpdateMultiplayerInput(player, world, netInput, weaponSprite, uiBlockInput,
                                  attackBufferTimer, lastAttackDirX, lastAttackDirY, camera, dt);

            if (mpMode == MpMode::Client && clientReady)
                UpdateClientMovement(player, world, netInput, inputSeq, pendingInputs, dt);

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
                }
            }

            NetPlayerState snap[NET_MAX_PLAYERS];
            int snapCount = 0;
            uint8_t localId = 0;
            bool snapNight = isNight;
            float snapCycle = cycleTimer;
            
            if (mpMode == MpMode::Host && serverReady)
            {
                localId = 0;
                snapCount = Server_GetSnapshot(&server, snap, NET_MAX_PLAYERS, &snapNight, &snapCycle);
            }
            else if (mpMode == MpMode::Client && clientReady)
            {
                localId = client.playerId;
                snapCount = Client_GetSnapshot(&client, snap, NET_MAX_PLAYERS, &snapNight, &snapCycle);
            }

            ProcessNetworkSnapshot(snap, snapCount, localId, player, remotePlayers, 
                                  pendingInputs, world, swordSprite, mpMode, dt);

            isNight = snapNight;
            cycleTimer = snapCycle;
        }

        camera.x = player.GetX() - halfW / camera.zoom;
        camera.y = player.GetY() - halfH / camera.zoom;

        float playerHP = player.GetHP();
        if (multiplayerActive)
        {
            float dummyHP = playerHP;
            world->Update(dt, isNight ? 1 : 0, player.GetX(), player.GetY(), 
                         player.GetX(), player.GetY(), player.GetSize(), &dummyHP, false);
        }
        else
        {
            world->Update(dt, isNight ? 1 : 0, player.GetX(), player.GetY(),
                         player.GetX(), player.GetY(), player.GetSize(), &playerHP, true);
            player.SetHP(playerHP);
        }

        UpdateCaveState(player, world, inCave, caveEntranceX, caveEntranceY, multiplayerActive);

        if (mpMode == MpMode::Host && serverReady)
            UpdateHostMobs(player, world, server, mobSyncTimer, isNight, dt);

        if (!multiplayerActive)
            UpdateDayNightCycle(isNight, cycleTimer, DAY_DURATION, NIGHT_DURATION, NIGHT_FADE, fogStrength, dt);

        bool f3 = Input_IsKeyDown(KEY_F3);
        if (f3 && !prevF3)
            showDebug = !showDebug;
        prevF3 = f3;

        Renderer_BeginFrame(renderer);
        Renderer_Clear(Color{0.5f, 0.8f, 1.0f, 1.0f});

        BeginCameraMode(camera);
        world->Draw(camera, 800, 600, mpMode != MpMode::Client);
        
        if (showDebug)
            DrawChunkDebugLines(camera, renderer);

        player.Draw();
        if (multiplayerActive)
        {
            for (const auto& rp : remotePlayers)
                rp.player.Draw();
        }
        
        if (mpMode == MpMode::Client && clientReady)
        {
            int typeCount = 0;
            const MobArchetype* types = World_GetMobArchetypes(world->GetRaw(), &typeCount);
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
            DrawDebugInfo(player, world, renderer, camera, inCave, isNight, cycleTimer,
                         DAY_DURATION, NIGHT_DURATION, fogStrength);

        MpUiResult uiResult = DrawMultiplayerUI(&ui, &uiX, &uiY, uiW, uiH, 
                                               mpMode, 1 + (int)remotePlayers.size(), 
                                               ipBuffer, (int)sizeof(ipBuffer), 
                                               portBuffer, (int)sizeof(portBuffer));

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
                remotePlayers.clear();
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

    if (world)
        delete world;

    Window_Destroy(window);
    Renderer_Destroy(renderer);
    Forge_Shutdown();
    return 0;
}
