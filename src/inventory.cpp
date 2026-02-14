#include "Inventory.h"
#include <cstdio>
#include <algorithm>

Inventory::Inventory()
{
    selectedSlot = 0;
    isExpanded = false;
    hasHeldItem = false;
    for (int i = 0; i < MAX_SLOTS; i++)
        slots[i].used = false;
}

bool Inventory::AddItem(const std::string& name, Color color, Texture2D* sprite, ItemType type, int tileType, int quantity)
{
    if (type == ITEM_BLOCK)
    {
        for (int i = 0; i < MAX_SLOTS; i++)
        {
            if (slots[i].used && slots[i].type == ITEM_BLOCK && slots[i].tileType == tileType)
            {
                int canAdd = 99 - slots[i].quantity;
                if (canAdd > 0)
                {
                    int toAdd = std::min(canAdd, quantity);
                    slots[i].quantity += toAdd;
                    if (toAdd < quantity)
                    {
                        return AddItem(name, color, sprite, type, tileType, quantity - toAdd);
                    }
                    return true;
                }
            }
        }
    }
    
    for (int i = 0; i < MAX_SLOTS; i++)
    {
        if (!slots[i].used)
        {
            slots[i].name = name;
            slots[i].color = color;
            slots[i].sprite = sprite;
            slots[i].type = type;
            slots[i].tileType = tileType;
            slots[i].quantity = (type == ITEM_BLOCK) ? quantity : 1;
            slots[i].used = true;
            return true;
        }
    }
    return false;
}

bool Inventory::RemoveItemQuantity(int index, int amount)
{
    if (index < 0 || index >= MAX_SLOTS) return false;
    if (!slots[index].used) return false;
    
    if (slots[index].type == ITEM_BLOCK)
    {
        slots[index].quantity -= amount;
        if (slots[index].quantity <= 0)
        {
            slots[index].used = false;
            return true;
        }
    }
    return false;
}

void Inventory::RemoveItem(int index)
{
    if (index < 0 || index >= MAX_SLOTS) return;
    slots[index].used = false;
}

void Inventory::Draw(float startX, float startY, float slotSize)
{
    const int visibleSlots = isExpanded ? MAX_SLOTS : HOTBAR_SLOTS;
    const int columns = HOTBAR_SLOTS;

    for (int i = 0; i < visibleSlots; i++)
    {
        int col = i % columns;
        int row = i / columns;
        Rect slotRect = { startX + col * (slotSize + 4), startY - row * (slotSize + 4), slotSize, slotSize };
        Renderer_DrawRectangle(slotRect, Color{0.2f, 0.2f, 0.2f, 1.0f});
        Renderer_DrawRectangleLines(slotRect, 2.0f, Color{1.0f, 1.0f, 1.0f, 1.0f});

        if (slots[i].used)
        {
            Rect itemRect = { slotRect.x + 4, slotRect.y + 4, slotSize - 8, slotSize - 8 };
            if (slots[i].sprite)
            {
                Rect src = { 0, 0, (float)slots[i].sprite->width, (float)slots[i].sprite->height };
                Renderer_DrawTexturePro(*slots[i].sprite, src, itemRect, Vec2{0, 0}, 0.0f, Color{1, 1, 1, 1});
            }
            else
            {
                Renderer_DrawRectangle(itemRect, slots[i].color);
            }

            if (slots[i].type == ITEM_BLOCK && slots[i].quantity > 0)
            {
                char qtyStr[16];
                snprintf(qtyStr, sizeof(qtyStr), "%d", slots[i].quantity);
                Renderer_DrawText(qtyStr, slotRect.x + slotSize - 20, slotRect.y + slotSize - 18, 10, Color{1, 1, 1, 1});
            }
            //Renderer_DrawText(slots[i].name.c_str(), itemRect.x - 6, itemRect.y - 16, 12, Color{1,1,1,1});
        }

        if (i == selectedSlot)
        {
            Renderer_DrawRectangleLines(slotRect, 3.0f, Color{1, 1, 0, 1});
            if(!isExpanded)
            {
                char buffer[256];
                snprintf(buffer, sizeof(buffer), "Selected: %s", slots[i].used ? slots[i].name.c_str() : "Empty");
                Renderer_DrawText(buffer, startX, startY - 20, 14, Color{1, 1, 1, 1});
            }
        }
    }

    if (hasHeldItem)
    {
        int mx = Input_GetMouseX();
        int my = Input_GetMouseY();
        Rect heldRect = { (float)mx - slotSize * 0.5f + 4.0f, (float)my - slotSize * 0.5f + 4.0f, slotSize - 8, slotSize - 8 };
        if (heldItem.sprite)
        {
            Rect src = { 0, 0, (float)heldItem.sprite->width, (float)heldItem.sprite->height };
            Renderer_DrawTexturePro(*heldItem.sprite, src, heldRect, Vec2{0, 0}, 0.0f, Color{1, 1, 1, 1});
        }
        else
        {
            Renderer_DrawRectangle(heldRect, heldItem.color);
        }
    }
}

void Inventory::Update()
{
    if (Input_IsKeyPressed(KEY_TAB))
        isExpanded = !isExpanded;

    for (int i = 0; i < HOTBAR_SLOTS; i++)
    {
        if (Input_IsKeyDown(KeyCode(KEY_1 + i)))
        {
            selectedSlot = i;
        }
    }

    int mx = Input_GetMouseX();
    int my = Input_GetMouseY();
    float startX = 10.0f;
    float startY = (float)GetGlobalRenderer()->height - 50.0f;
    float slotSize = 32.0f;

    if (Input_IsMousePressed(MOUSE_LEFT))
    {
        const int visibleSlots = isExpanded ? MAX_SLOTS : HOTBAR_SLOTS;
        const int columns = HOTBAR_SLOTS;

        for (int i = 0; i < visibleSlots; i++)
        {
            int col = i % columns;
            int row = i / columns;
            Rect slotRect = { startX + col * (slotSize + 4), startY - row * (slotSize + 4), slotSize, slotSize };
            if (mx >= slotRect.x && mx <= slotRect.x + slotRect.width &&
                my >= slotRect.y && my <= slotRect.y + slotRect.height)
            {
                if (i < HOTBAR_SLOTS)
                    selectedSlot = i;
                else if (selectedSlot >= HOTBAR_SLOTS)
                    selectedSlot = 0;
                if (hasHeldItem)
                {
                    if (slots[i].used)
                    {
                        Item temp = slots[i];
                        slots[i] = heldItem;
                        heldItem = temp;
                        heldItem.used = true;
                    }
                    else
                    {
                        slots[i] = heldItem;
                        slots[i].used = true;
                        hasHeldItem = false;
                    }
                }
                else if (slots[i].used)
                {
                    heldItem = slots[i];
                    heldItem.used = true;
                    slots[i].used = false;
                    hasHeldItem = true;
                }
                break;
            }
        }
    }
}
