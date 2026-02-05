#ifndef __WORLD_H__
#define __WORLD_H__

#include "engine/forge.h"

class World
{
public:
    World(int loadRadiusChunks, int seed);
    ~World();

    void Update(float focusX, float focusY);
    void Draw(const Camera2D& camera, int screenW, int screenH) const;

    float GetTileSize() const { return tileSize; }
    ForgeWorld* GetRaw() const { return forgeWorld; }

private:
    ForgeWorld* forgeWorld;
    float tileSize;
};

#endif // __WORLD_H__
