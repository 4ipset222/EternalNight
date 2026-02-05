#include <algorithm>
#include "engine/forge.h"
#include "inventory.h"
#include "player.h"
#include "world.h"

int main(int argc, char** argv)
{
    Forge_Init();

    Window* window = Window_Create(800, 600, "Eternal Night");
    Renderer* renderer = Renderer_Create(window);
    SetGlobalRenderer(renderer);

    World world(WORLD_LOAD_RADIUS_CHUNKS, 12345);

    Camera2D camera = { 0.0f, 0.0f, 1.0f };
    Player player(0.0f, 0.0f);

    Inventory inventory;
    Texture2D* itemSprite = LoadTexture("test.png");
    Texture2D* swordSprite = LoadTexture("kuzne4ik_sword.png");
    inventory.AddItem("fimoz", Color{1,0,0,1}, itemSprite, ITEM_MISC);
    inventory.AddItem("giga fimoz", Color{0,1,0,1}, itemSprite, ITEM_MISC);
    inventory.AddItem("kuzne4ik sword", Color{1,1,1,1}, swordSprite, ITEM_WEAPON);

    bool showDebug = false;
    bool prevF3 = false;

    float fogRadius = 200.0f;
    float fogStrength = 0.0f;
    const float DAY_DURATION = 5.0f;
    const float NIGHT_DURATION = 180.0f;
    const float NIGHT_FADE = 10.0f;
    bool isNight = false;
    float cycleTimer = 0.0f;

    while (!Window_ShouldClose(window))
    {
        Window_PollEvents(window);
        float dt = (float)GetDeltaTime();

        player.Update(dt);
        inventory.Update();

        float halfW = (float)renderer->width * 0.5f;
        float halfH = (float)renderer->height * 0.5f;
        camera.x = player.GetX() - halfW / camera.zoom;
        camera.y = player.GetY() - halfH / camera.zoom;

        if (Input_IsKeyDown(KEY_Q)) camera.zoom += 0.5f * dt;
        if (Input_IsKeyDown(KEY_E)) camera.zoom -= 0.5f * dt;
        camera.zoom = std::clamp(camera.zoom, 0.1f, 5.0f);

        Item* selected = inventory.GetSelectedItem();
        Texture2D* weaponSprite = (selected && selected->type == ITEM_WEAPON) ? selected->sprite : nullptr;
        player.SetWeaponSprite(weaponSprite);

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

        float playerHP = player.GetHP();
        world.Update(dt, isNight ? 1 : 0, player.GetX(), player.GetY(), player.GetX(), player.GetY(), player.GetSize(), &playerHP);
        player.SetHP(playerHP);

        bool f3 = Input_IsKeyDown(KEY_F3);
        if (f3 && !prevF3)
            showDebug = !showDebug;
        prevF3 = f3;

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
        world.Draw(camera, 800, 600);
        if (showDebug)
        {
            const float tileSize = world.GetTileSize();
            const float chunkWorldSize = CHUNK_SIZE * tileSize;

            auto floorDiv = [](int v, int d) -> int {
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

        inventory.Draw(10.0f, (float)renderer->height - 50.0f, 32.0f);
        player.DrawHP();

        Renderer_EndFrame(renderer);

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Eternal Night - FPS: %.2f", GetFPS());
        Window_SetTitle(window, buffer);
    }

    Window_Destroy(window);
    Renderer_Destroy(renderer);
    Forge_Shutdown();
    return 0;
}
