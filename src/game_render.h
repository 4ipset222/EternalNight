#ifndef __GAME_RENDER_H__
#define __GAME_RENDER_H__

#include "world.h"
#include "player.h"
#include "engine/forge.h"

void DrawDebugInfo(const Player& player, World* world, const Renderer* renderer,
                  const Camera2D& camera, bool inCave, bool isNight, float cycleTimer,
                  float DAY_DURATION, float NIGHT_DURATION, float fogStrength);

void DrawChunkDebugLines(const Camera2D& camera, const Renderer* renderer);

#endif // __GAME_RENDER_H__
