#ifndef __FORGE_INPUT_H__
#define __FORGE_INPUT_H__

#include <SDL2/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    KEY_UNKNOWN = 0,
    
    // Letters
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    
    // Numbers
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    
    // Function Keys
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    
    // Special Keys
    KEY_ESCAPE, KEY_SPACE, KEY_ENTER, KEY_BACKSPACE, KEY_TAB,
    KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT, KEY_LEFT_CTRL, KEY_RIGHT_CTRL,
    KEY_LEFT_ALT, KEY_RIGHT_ALT,
    KEY_LEFT_SUPER, KEY_RIGHT_SUPER,
    
    // Arrow Keys
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    
    // Navigation Keys
    KEY_INSERT, KEY_DELETE, KEY_HOME, KEY_END, KEY_PAGE_UP, KEY_PAGE_DOWN,
    
    // Punctuation/Symbols
    KEY_GRAVE, KEY_MINUS, KEY_EQUALS, KEY_SLASH, KEY_BACKSLASH,
    KEY_SEMICOLON, KEY_APOSTROPHE, KEY_COMMA, KEY_PERIOD,
    KEY_LEFT_BRACKET, KEY_RIGHT_BRACKET,
    
    // Numeric Keypad
    KEY_KP_0, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4,
    KEY_KP_5, KEY_KP_6, KEY_KP_7, KEY_KP_8, KEY_KP_9,
    KEY_KP_PLUS, KEY_KP_MINUS, KEY_KP_MULTIPLY, KEY_KP_DIVIDE, KEY_KP_DECIMAL,
    KEY_KP_ENTER, KEY_NUM_LOCK, KEY_CAPS_LOCK, KEY_SCROLL_LOCK,
    
    // Print/Pause
    KEY_PRINT_SCREEN, KEY_PAUSE,

    KEY_COUNT
} KeyCode;

typedef enum
{
    MOUSE_LEFT = 0,
    MOUSE_RIGHT,
    MOUSE_MIDDLE,
    MOUSE_BUTTON_4,
    MOUSE_BUTTON_5,
    MOUSE_BUTTON_6,
    MOUSE_BUTTON_7,
    MOUSE_BUTTON_8,
    
    MOUSE_BUTTON_COUNT
} MouseButton;

void Input_Init(void);
void Input_Update(void);
void Input_ProcessEvent(const SDL_Event* e);

bool Input_IsKeyDown(KeyCode key);
bool Input_IsKeyPressed(KeyCode key);
bool Input_IsKeyReleased(KeyCode key);

bool Input_IsMouseDown(MouseButton button);
bool Input_IsMousePressed(MouseButton button);
bool Input_IsMouseReleased(MouseButton button);

int  Input_GetMouseX(void);
int  Input_GetMouseY(void);

#ifdef __cplusplus
}
#endif

#endif // __FORGE_INPUT_H__
