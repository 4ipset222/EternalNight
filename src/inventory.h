#ifndef __INVENTORY_H__
#define __INVENTORY_H__

#include "engine/forge.h"
#include <string>

struct Item
{
    std::string name;
    Color color;
    Texture2D* sprite = nullptr;
    bool used = false;
};

class Inventory
{
public:
    static const int HOTBAR_SLOTS = 5;
    static const int MAX_SLOTS = 20;

    Inventory();

    bool AddItem(const std::string& name, Color color, Texture2D* sprite = nullptr);
    void RemoveItem(int index);
    void Draw(float startX, float startY, float slotSize = 32.0f);

    void Update();

    int GetSelectedIndex() const { return selectedSlot; }
    Item* GetSelectedItem()
    { 
        if (selectedSlot >= 0 && selectedSlot < HOTBAR_SLOTS && slots[selectedSlot].used)
            return &slots[selectedSlot]; 
        return nullptr;
    }

private:
    Item slots[MAX_SLOTS];
    int selectedSlot;
    bool isExpanded;

    bool hasHeldItem;
    Item heldItem;
};

#endif // __INVENTORY_H__
