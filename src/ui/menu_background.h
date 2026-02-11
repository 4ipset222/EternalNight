#ifndef __MENU_BACKGROUND_H__
#define __MENU_BACKGROUND_H__

#include "../engine/forge.h"
#include "../engine/rendertexture.h"
#include "../world.h"
#include <vector>

struct BackgroundFrameState
{
    World* world;
    RenderTexture renderTexture;
    float animationTimer;
    float fadeInDuration;
    float moveDuration;
    float fadeOutDuration;
    float x, y;
    float startX, startY;
    float endX, endY;
    float opacity;
    int stage;            // 0 = fade in, 1 = moving, 2 = fade out
    float cameraX, cameraY;
    int seed;
};

class MenuBackground
{
public:
    MenuBackground();
    ~MenuBackground();
    
    void Update(float dt);
    void Draw();
    
private:
    void GenerateNextFrame();
    void UpdateCurrentFrame(float dt);
    
    std::vector<BackgroundFrameState> frames;
    int currentFrameIndex;
    int frameWidth, frameHeight;
    int nextSeed;
};

#endif
