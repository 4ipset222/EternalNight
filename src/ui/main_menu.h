#ifndef __MAIN_MENU_H__
#define __MAIN_MENU_H__

#include "../engine/forge.h"
#include "menu_background.h"
#include <string>

enum GameState
{
    STATE_MENU = 0,
    STATE_NEW_GAME = 1,
    STATE_LOAD_GAME = 2,
    STATE_PLAYING = 3,
    STATE_EXITING = 4
};

struct WorldConfig
{
    int seed;
    float waterAmount;
    float stoneAmount;
    float caveAmount;
};

class MainMenu
{
public:
    MainMenu();
    ~MainMenu();
    
    void Update();
    void Draw();
    
    GameState GetGameState() const { return gameState; }
    void SetGameState(GameState state) { gameState = state; }
    
    const WorldConfig& GetWorldConfig() const { return config; }
    
private:
    void DrawMainMenu();
    void DrawNewGameScreen();
    void DrawButton9Slice(float x, float y, float w, float h, const std::string& label, bool& outPressed);
    void DrawSlider(float x, float y, float w, float h, const std::string& label, float& value);
    
    void LoadTextures();
    
    GameState gameState;
    WorldConfig config;
    
    Texture2D* buttonTexture;
    Texture2D* sliderTexture;
    MenuBackground* background;
    
    bool backPressed;
    char seedBuffer[32];
    float tempWaterAmount;
    float tempStoneAmount;
    float tempCaveAmount;
    int sliderWidth;
    
    int draggingSlider;  // -1 = none, 0 = water, 1 = stone, 2 = cave
};

#endif // __MAIN_MENU_H__
