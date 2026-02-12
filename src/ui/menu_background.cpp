#include "menu_background.h"
#include "../engine/renderer.h"
#include "../engine/camera.h"
#include <cmath>
#include <cstdlib>
#include <ctime>

MenuBackground::MenuBackground()
    : currentFrameIndex(0), frameWidth(2000), frameHeight(1600), nextSeed(54321)
{
    srand((unsigned)time(nullptr));
    if (frames.empty())
    {
        for (int i = 0; i < 3; ++i)
        {
            GenerateNextFrame();
        }
    }
    if (!frames.empty())
    {
        frames[0].stage = 0;
        frames[0].animationTimer = 0.0f;
    }
}

MenuBackground::~MenuBackground()
{
    for (auto& frame : frames)
    {
        if (frame.world)
        {
            delete frame.world;
            frame.world = nullptr;
        }
        UnloadRenderTexture(frame.renderTexture);
    }
    frames.clear();
}

void MenuBackground::GenerateNextFrame()
{
    BackgroundFrameState newFrame = {};
    
    newFrame.seed = nextSeed;
    newFrame.world = new World(8, nextSeed);
    ForgeWorld* raw = newFrame.world->GetRaw();
    raw->waterAmount = 0.2f + (rand() % 40) * 0.01f;
    raw->stoneAmount = 0.2f + (rand() % 50) * 0.01f;
    raw->caveAmount = 0.1f + (rand() % 30) * 0.01f;
    World_ReloadChunks(raw);
    
    newFrame.renderTexture = LoadRenderTexture(frameWidth, frameHeight);
    
    newFrame.cameraX = (float)(rand() % 20000 - 10000);
    newFrame.cameraY = (float)(rand() % 20000 - 10000);
    
    newFrame.x = 0.0f;
    newFrame.y = 0.0f;
    newFrame.opacity = 0.0f;
    newFrame.stage = 0;
    newFrame.animationTimer = 0.0f;
    newFrame.fadeInDuration = 2.0f;
    newFrame.moveDuration = 4.0f;
    newFrame.fadeOutDuration = 1.5f;
    
    if (frames.size() % 2 == 0)
    {
        newFrame.startX = frameWidth * 0.1f;
        newFrame.startY = frameHeight * 0.05f;
        newFrame.endX = -frameWidth * 0.3f;
        newFrame.endY = -frameHeight * 0.2f;
    }
    else
    {
        newFrame.startX = -frameWidth * 0.2f;
        newFrame.startY = frameHeight * 0.1f;
        newFrame.endX = frameWidth * 0.6f;
        newFrame.endY = frameHeight * 0.1f;
    }
    
    newFrame.x = newFrame.startX;
    newFrame.y = newFrame.startY;
    
    frames.push_back(newFrame);
    nextSeed += 1000 + (rand() % 5000);
}

void MenuBackground::UpdateCurrentFrame(float dt)
{
    if (frames.empty())
        return;
    
    BackgroundFrameState& frame = frames[currentFrameIndex];
    frame.animationTimer += dt;
    
    float totalDuration = frame.fadeInDuration + frame.moveDuration + frame.fadeOutDuration;
    
    if (frame.animationTimer >= totalDuration)
    {
        currentFrameIndex = (currentFrameIndex + 1) % frames.size();
        frames[currentFrameIndex].stage = 0;
        frames[currentFrameIndex].animationTimer = 0.0f;
        
        if (frames.size() < 4)
            GenerateNextFrame();
        
        return;
    }
    
    if (frame.animationTimer < frame.fadeInDuration)
    {
        frame.opacity = frame.animationTimer / frame.fadeInDuration;
        frame.x = frame.startX;
        frame.y = frame.startY;
        frame.stage = 0;
    }
    else if (frame.animationTimer < frame.fadeInDuration + frame.moveDuration)
    {
        frame.opacity = 1.0f;
        float moveProgress = (frame.animationTimer - frame.fadeInDuration) / frame.moveDuration;
        frame.x = frame.startX + (frame.endX - frame.startX) * moveProgress;
        frame.y = frame.startY + (frame.endY - frame.startY) * moveProgress;
        frame.stage = 1;
    }
    else
    {
        float fadeOutProgress = (frame.animationTimer - frame.fadeInDuration - frame.moveDuration) / frame.fadeOutDuration;
        frame.opacity = 1.0f - fadeOutProgress;
        frame.x = frame.endX;
        frame.y = frame.endY;
        frame.stage = 2;
    }
}

void MenuBackground::Update(float dt)
{
    UpdateCurrentFrame(dt);
}

void MenuBackground::Draw()
{
    if (frames.empty())
        return;
    
    Renderer* renderer = GetGlobalRenderer();
    if (!renderer)
        return;

    Renderer_Clear(Color{0.0f, 0.0f, 0.0f, 1.0f});
    
    const BackgroundFrameState& frame = frames[currentFrameIndex];
    
    bool needsRender = true;
    
    if (needsRender && frame.world && frame.renderTexture.texture.id > 0)
    {
        BeginTextureMode(frame.renderTexture);
        Renderer_Clear(Color{0.5f, 0.8f, 1.0f, 1.0f});
        
        Camera2D camera = {frame.cameraX, frame.cameraY, 0.8f};
        BeginCameraMode(camera);
        
        frame.world->Draw(camera, frameWidth, frameHeight, true);
        
        EndCameraMode();
        EndTextureMode();
    }
    
    if (frame.renderTexture.texture.id > 0)
    {
        Rect src = {0, 0, (float)frameWidth, (float)frameHeight};
        Rect dst = {0, 0, (float)renderer->width, (float)renderer->height};
        Color tint = {1.0f, 1.0f, 1.0f, frame.opacity};
        Renderer_DrawTexturePro(frame.renderTexture.texture, src, dst, Vec2{0, 0}, 0.0f, tint);
    }
    
    Color overlay = {0.0f, 0.0f, 0.0f, 0.5f};
    Renderer_DrawRectangle(Rect{0, 0, (float)renderer->width, (float)renderer->height}, overlay);
}
