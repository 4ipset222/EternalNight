#include "main_menu.h"
#include "../engine/forge.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>

MainMenu::MainMenu()
    : gameState(STATE_MENU), buttonTexture(nullptr), sliderTexture(nullptr),
      backPressed(false), sliderWidth(200), draggingSlider(-1), background(nullptr)
{
    config.seed = 12345;
    config.waterAmount = 0.5f;
    config.stoneAmount = 0.5f;
    config.caveAmount = 0.5f;
    tempWaterAmount = 0.5f;
    tempStoneAmount = 0.5f;
    tempCaveAmount = 0.5f;
    snprintf(seedBuffer, sizeof(seedBuffer), "%d", config.seed);
    background = new MenuBackground();
    LoadTextures();
}

MainMenu::~MainMenu()
{
    if (buttonTexture) 
    {
        UnloadTexture(buttonTexture);
    }
    if (sliderTexture)
    {
        UnloadTexture(sliderTexture);
    }
    if (background)
    {
        delete background;
        background = nullptr;
    }
}

void MainMenu::LoadTextures()
{
    buttonTexture = LoadTexture("gui_button.png");
    sliderTexture = LoadTexture("gui_slider.png");
}

void MainMenu::DrawButton9Slice(float x, float y, float w, float h, const std::string& label, bool& outPressed)
{
    outPressed = false;
    
    int mx = Input_GetMouseX();
    int my = Input_GetMouseY();
    bool hovered = mx >= x && mx <= x + w && my >= y && my <= y + h;
    bool clicked = hovered && Input_IsMousePressed(MOUSE_LEFT);
    
    if (buttonTexture && buttonTexture->width > 0)
    {
        float cornerSize = 16.0f;
        float srcW = (float)buttonTexture->width;
        float srcH = (float)buttonTexture->height;
        
        Rect srcTL = {0, 0, cornerSize, cornerSize};
        Rect dstTL = {x, y, cornerSize, cornerSize};
        Renderer_DrawTexturePro(*buttonTexture, srcTL, dstTL, Vec2{0, 0}, 0.0f, 
                               Color{1, 1, 1, hovered ? 1.2f : 1.0f});
        
        Rect srcTC = {cornerSize, 0, srcW - cornerSize * 2, cornerSize};
        Rect dstTC = {x + cornerSize, y, w - cornerSize * 2, cornerSize};
        Renderer_DrawTexturePro(*buttonTexture, srcTC, dstTC, Vec2{0, 0}, 0.0f,
                               Color{1, 1, 1, hovered ? 1.2f : 1.0f});
        
        Rect srcTR = {srcW - cornerSize, 0, cornerSize, cornerSize};
        Rect dstTR = {x + w - cornerSize, y, cornerSize, cornerSize};
        Renderer_DrawTexturePro(*buttonTexture, srcTR, dstTR, Vec2{0, 0}, 0.0f,
                               Color{1, 1, 1, hovered ? 1.2f : 1.0f});
        
        Rect srcML = {0, cornerSize, cornerSize, srcH - cornerSize * 2};
        Rect dstML = {x, y + cornerSize, cornerSize, h - cornerSize * 2};
        Renderer_DrawTexturePro(*buttonTexture, srcML, dstML, Vec2{0, 0}, 0.0f,
                               Color{1, 1, 1, hovered ? 1.2f : 1.0f});
        
        Rect srcMC = {cornerSize, cornerSize, srcW - cornerSize * 2, srcH - cornerSize * 2};
        Rect dstMC = {x + cornerSize, y + cornerSize, w - cornerSize * 2, h - cornerSize * 2};
        Renderer_DrawTexturePro(*buttonTexture, srcMC, dstMC, Vec2{0, 0}, 0.0f,
                               Color{1, 1, 1, hovered ? 1.2f : 1.0f});
        
        Rect srcMR = {srcW - cornerSize, cornerSize, cornerSize, srcH - cornerSize * 2};
        Rect dstMR = {x + w - cornerSize, y + cornerSize, cornerSize, h - cornerSize * 2};
        Renderer_DrawTexturePro(*buttonTexture, srcMR, dstMR, Vec2{0, 0}, 0.0f,
                               Color{1, 1, 1, hovered ? 1.2f : 1.0f});
        
        Rect srcBL = {0, srcH - cornerSize, cornerSize, cornerSize};
        Rect dstBL = {x, y + h - cornerSize, cornerSize, cornerSize};
        Renderer_DrawTexturePro(*buttonTexture, srcBL, dstBL, Vec2{0, 0}, 0.0f,
                               Color{1, 1, 1, hovered ? 1.2f : 1.0f});
        
        Rect srcBC = {cornerSize, srcH - cornerSize, srcW - cornerSize * 2, cornerSize};
        Rect dstBC = {x + cornerSize, y + h - cornerSize, w - cornerSize * 2, cornerSize};
        Renderer_DrawTexturePro(*buttonTexture, srcBC, dstBC, Vec2{0, 0}, 0.0f,
                               Color{1, 1, 1, hovered ? 1.2f : 1.0f});
        
        Rect srcBR = {srcW - cornerSize, srcH - cornerSize, cornerSize, cornerSize};
        Rect dstBR = {x + w - cornerSize, y + h - cornerSize, cornerSize, cornerSize};
        Renderer_DrawTexturePro(*buttonTexture, srcBR, dstBR, Vec2{0, 0}, 0.0f,
                               Color{1, 1, 1, hovered ? 1.2f : 1.0f});
    }
    else
    {
        Color bgColor = hovered ? Color{0.4f, 0.6f, 0.8f, 1.0f} : Color{0.2f, 0.4f, 0.6f, 1.0f};
        Rect buttonRect = {x, y, w, h};
        Renderer_DrawRectangle(buttonRect, bgColor);
        Renderer_DrawRectangleLines(buttonRect, 2.0f, Color{0.8f, 0.8f, 0.8f, 1.0f});
    }
    
    float textX = x + w * 0.5f - (float)label.length() * 3.5f;
    float textY = y + h * 0.5f - 8.0f;
    Renderer_DrawText(label.c_str(), textX, textY, 16, Color{1, 1, 1, 1});
    
    outPressed = clicked;
}

void MainMenu::DrawSlider(float x, float y, float w, float h, const std::string& label, float& value)
{
    Rect bgRect = {x, y, w, h};
    Renderer_DrawRectangle(bgRect, Color{0.2f, 0.2f, 0.2f, 1.0f});
    Renderer_DrawRectangleLines(bgRect, 2.0f, Color{0.5f, 0.5f, 0.5f, 1.0f});
    
    float handleX = x + value * (w - 20.0f);
    Rect handleRect = {handleX, y, 20.0f, h};
    
    if (sliderTexture && sliderTexture->width > 0)
    {
        Rect srcRect = {0, 0, (float)sliderTexture->width, (float)sliderTexture->height};
        Renderer_DrawTexturePro(*sliderTexture, srcRect, handleRect, Vec2{0, 0}, 0.0f, Color{1, 1, 1, 1});
    }
    else
    {
        Renderer_DrawRectangle(handleRect, Color{0.8f, 0.8f, 0.2f, 1.0f});
    }
    
    int mx = Input_GetMouseX();
    int my = Input_GetMouseY();
    bool isOverHandle = mx >= handleX && mx <= handleX + 20.0f && my >= y && my <= y + h;
    bool isOverTrack = mx >= x && mx <= x + w && my >= y && my <= y + h;
    
    int sliderId = (label[0] == 'W') ? 0 : (label[0] == 'S') ? 1 : 2;
    
    if (Input_IsMousePressed(MOUSE_LEFT))
    {
        if (isOverHandle || isOverTrack)
            draggingSlider = sliderId;
    }
    
    if (!Input_IsMouseDown(MOUSE_LEFT))
        draggingSlider = -1;
    
    if (draggingSlider == sliderId)
    {
        float normalized = (float)(mx - (int)x) / (w - 20.0f);
        value = std::max(0.0f, std::min(1.0f, normalized));
    }
    
    char valueStr[32];
    snprintf(valueStr, sizeof(valueStr), "%s: %.0f%%", label.c_str(), value * 100.0f);
    Renderer_DrawText(valueStr, x, y - 25.0f, 14, Color{1, 1, 1, 1});
}

void MainMenu::DrawMainMenu()
{
    Renderer* renderer = GetGlobalRenderer();
    if (!renderer) return;
    
    if (background)
    {
        background->Draw();
    }
    
    float rightX = renderer->width - 280.0f;
    float centerY = renderer->height * 0.5f;
    
    const char* title = "Eternal Night";
    Renderer_DrawText(title, 40.0f, 50.0f, 48, {0.2f, 1.0f, 0.8f, 1.0f});
    
    bool playPressed = false;
    DrawButton9Slice(rightX, centerY - 100.0f, 220.0f, 70.0f, "Play", playPressed);
    if (playPressed)
    {
        gameState = STATE_NEW_GAME;
    }
    
    bool exitPressed = false;
    DrawButton9Slice(rightX, centerY + 30.0f, 220.0f, 70.0f, "Exit", exitPressed);
    if (exitPressed)
    {
        gameState = STATE_EXITING;
    }
}

void MainMenu::DrawNewGameScreen()
{
    Renderer* renderer = GetGlobalRenderer();
    if (!renderer) return;
    
    if (background)
    {
        background->Draw();
    }
    
    float leftX = 40.0f;
    float rightX = renderer->width - 280.0f;
    float topY = 80.0f;
    
    Renderer_DrawText("New Game", leftX, topY, 36, Color{0.2f, 1.0f, 0.8f, 1.0f});
    
    Renderer_DrawText("World Seed:", leftX, topY + 80.0f, 16, Color{1, 1, 1, 1});
    
    Rect seedBoxRect = {leftX, topY + 110.0f, 280.0f, 40.0f};
    Renderer_DrawRectangle(seedBoxRect, Color{0.2f, 0.2f, 0.2f, 1.0f});
    Renderer_DrawRectangleLines(seedBoxRect, 2.0f, Color{0.5f, 0.5f, 0.5f, 1.0f});
    Renderer_DrawText(seedBuffer, leftX + 10.0f, topY + 122.0f, 14, Color{1, 1, 1, 1});
    
    int pressedKey = 0;
    for (int key = KEY_0; key <= KEY_9; key++)
    {
        if (Input_IsKeyPressed((KeyCode)key))
        {
            pressedKey = key;
            break;
        }
    }
    
    if (pressedKey > 0)
    {
        size_t len = strlen(seedBuffer);
        if (len < sizeof(seedBuffer) - 1)
        {
            seedBuffer[len] = (char)('0' + (pressedKey - (int)KEY_0));
            seedBuffer[len + 1] = '\0';
        }
    }
    
    if (Input_IsKeyPressed(KEY_BACKSPACE))
    {
        size_t len = strlen(seedBuffer);
        if (len > 0)
            seedBuffer[len - 1] = '\0';
    }
    
    DrawSlider(leftX, topY + 170.0f, 280.0f, 30.0f, "Water", tempWaterAmount);
    DrawSlider(leftX, topY + 230.0f, 280.0f, 30.0f, "Stone", tempStoneAmount);
    DrawSlider(leftX, topY + 290.0f, 280.0f, 30.0f, "Caves", tempCaveAmount);
    
    bool playPressed = false;
    DrawButton9Slice(rightX, topY + 150.0f, 220.0f, 70.0f, "Play", playPressed);
    if (playPressed)
    {
        config.seed = atoi(seedBuffer);
        config.waterAmount = tempWaterAmount;
        config.stoneAmount = tempStoneAmount;
        config.caveAmount = tempCaveAmount;
        gameState = STATE_PLAYING;
    }
    
    bool backPressed = false;
    DrawButton9Slice(rightX, topY + 250.0f, 220.0f, 70.0f, "Back", backPressed);
    if (backPressed)
    {
        gameState = STATE_MENU;
    }
}

void MainMenu::Update()
{
    float dt = (float)GetDeltaTime();
    
    if (background)
    {
        background->Update(dt);
    }
    
    if (gameState == STATE_MENU)
    {
        DrawMainMenu();
    }
    else if (gameState == STATE_NEW_GAME)
    {
        DrawNewGameScreen();
    }
}

void MainMenu::Draw()
{
    
}
