#include "game_input.h"
#include "engine/input.h"
#include <algorithm>
#include <cmath>

void HandleBlockPlacement(const Player& player, const Camera2D& camera, 
                         World* world, Inventory& inventory, bool uiBlockInput)
{
    Item* selected = inventory.GetSelectedItem();
    if (selected && selected->type == ITEM_BLOCK && !uiBlockInput && Input_IsMousePressed(MOUSE_LEFT))
    {
        Vec2 mouse = { (float)Input_GetMouseX(), (float)Input_GetMouseY() };
        Vec2 worldMouse = { camera.x + mouse.x / camera.zoom, camera.y + mouse.y / camera.zoom };
        
        int tileX = (int)floorf(worldMouse.x / 16.0f);
        int tileY = (int)floorf(worldMouse.y / 16.0f);
        
        if (selected->tileType >= 0)
        {
            World_SetTile(world->GetRaw(), tileX, tileY, (TileType)selected->tileType);
            inventory.RemoveItemQuantity(inventory.GetSelectedIndex(), 1);
        }
    }
}

void HandlePlayerAttack(Player& player, const Camera2D& camera, 
                       World* world, Texture2D* weaponSprite, bool uiBlockInput)
{
    if (weaponSprite && !uiBlockInput && Input_IsMousePressed(MOUSE_LEFT))
    {
        Vec2 mouse = { (float)Input_GetMouseX(), (float)Input_GetMouseY() };
        Vec2 worldMouse = { camera.x + mouse.x / camera.zoom, camera.y + mouse.y / camera.zoom };
        Vec2 playerPos = player.GetPosition();
        Vec2 dir = vec2_sub(worldMouse, playerPos);

        if (player.Attack(dir))
        {
            World_PlayerAttack(
                world->GetRaw(),
                playerPos.x, playerPos.y,
                dir.x, dir.y,
                player.GetAttackRange(),
                player.GetAttackArcCos(),
                player.GetAttackDamage()
            );
        }
    }
}

NetInputState GetNetInput()
{
    NetInputState netInput = {};
    if (Input_IsKeyDown(KEY_W)) netInput.moveY -= 1.0f;
    if (Input_IsKeyDown(KEY_S)) netInput.moveY += 1.0f;
    if (Input_IsKeyDown(KEY_A)) netInput.moveX -= 1.0f;
    if (Input_IsKeyDown(KEY_D)) netInput.moveX += 1.0f;
    return netInput;
}

void UpdateCameraZoom(Camera2D& camera, float dt)
{
    if (Input_IsKeyDown(KEY_Q)) camera.zoom += 0.5f * dt;
    if (Input_IsKeyDown(KEY_E)) camera.zoom -= 0.5f * dt;
    camera.zoom = std::clamp(camera.zoom, 0.1f, 5.0f);
}
