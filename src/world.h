#ifndef __WORLD_H__
#define __WORLD_H__

#include "engine/forge.h"
#include <vector>

class World
{
public:
    World(int loadRadiusChunks, int seed);
    ~World();

    void Update(float dt, int isNight, Vec2 focusPos, Vec2 playerPos, float playerRadius, float* ioPlayerHP, bool updateMobs);
    void Draw(const Camera2D& camera, int screenW, int screenH, bool drawMobs) const;

    float GetTileSize() const { return tileSize; }
    ForgeWorld* GetRaw() const { return forgeWorld; }

private:
    ForgeWorld* forgeWorld;
    float tileSize;
    mutable std::vector<float> tileTriPos;
    mutable std::vector<float> tileTriCol;
};

#endif // __WORLD_H__
