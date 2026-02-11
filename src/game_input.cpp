#include "game_input.h"
#include "engine/input.h"
#include <algorithm>
#include <cmath>

void HandleBlockPlacement(const Player& player, const Camera2D& camera, 
                         const Renderer* renderer, World* world, 
                         Inventory& inventory, bool uiBlockInput)
{
    Item* selected = inventory.GetSelectedItem();
    if (selected && selected->type == ITEM_BLOCK && !uiBlockInput && Input_IsMousePressed(MOUSE_LEFT))
    {
        float mx = (float)Input_GetMouseX();
        float my = (float)Input_GetMouseY();
        float worldMx = camera.x + mx / camera.zoom;
        float worldMy = camera.y + my / camera.zoom;
        
        int tileX = (int)floorf(worldMx / 16.0f);
        int tileY = (int)floorf(worldMy / 16.0f);
        
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
        float mx = (float)Input_GetMouseX();
        float my = (float)Input_GetMouseY();
        float worldMx = camera.x + mx / camera.zoom;
        float worldMy = camera.y + my / camera.zoom;
        float dirX = worldMx - player.GetX();
        float dirY = worldMy - player.GetY();

        if (player.Attack(dirX, dirY))
        {
            World_PlayerAttack(
                world->GetRaw(),
                player.GetX(), player.GetY(),
                dirX, dirY,
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
