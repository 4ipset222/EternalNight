#include "input.h"
#include <string.h>

static bool currentKeys[KEY_COUNT];
static bool previousKeys[KEY_COUNT];

static bool currentMouse[MOUSE_BUTTON_COUNT];
static bool previousMouse[MOUSE_BUTTON_COUNT];

static int mouseX, mouseY;


static KeyCode SDLKeyToKeyCode(SDL_Keycode key)
{
    switch (key)
    {
        case SDLK_a: return KEY_A;
        case SDLK_b: return KEY_B;
        case SDLK_c: return KEY_C;
        case SDLK_d: return KEY_D;
        case SDLK_e: return KEY_E;
        case SDLK_f: return KEY_F;
        case SDLK_g: return KEY_G;
        case SDLK_h: return KEY_H;
        case SDLK_i: return KEY_I;
        case SDLK_j: return KEY_J;
        case SDLK_k: return KEY_K;
        case SDLK_l: return KEY_L;
        case SDLK_m: return KEY_M;
        case SDLK_n: return KEY_N;
        case SDLK_o: return KEY_O;
        case SDLK_p: return KEY_P;
        case SDLK_q: return KEY_Q;
        case SDLK_r: return KEY_R;
        case SDLK_s: return KEY_S;
        case SDLK_t: return KEY_T;
        case SDLK_u: return KEY_U;
        case SDLK_v: return KEY_V;
        case SDLK_w: return KEY_W;
        case SDLK_x: return KEY_X;
        case SDLK_y: return KEY_Y;
        case SDLK_z: return KEY_Z;
        
        case SDLK_0: return KEY_0;
        case SDLK_1: return KEY_1;
        case SDLK_2: return KEY_2;
        case SDLK_3: return KEY_3;
        case SDLK_4: return KEY_4;
        case SDLK_5: return KEY_5;
        case SDLK_6: return KEY_6;
        case SDLK_7: return KEY_7;
        case SDLK_8: return KEY_8;
        case SDLK_9: return KEY_9;
        
        case SDLK_F1: return KEY_F1;
        case SDLK_F2: return KEY_F2;
        case SDLK_F3: return KEY_F3;
        case SDLK_F4: return KEY_F4;
        case SDLK_F5: return KEY_F5;
        case SDLK_F6: return KEY_F6;
        case SDLK_F7: return KEY_F7;
        case SDLK_F8: return KEY_F8;
        case SDLK_F9: return KEY_F9;
        case SDLK_F10: return KEY_F10;
        case SDLK_F11: return KEY_F11;
        case SDLK_F12: return KEY_F12;
        
        case SDLK_ESCAPE: return KEY_ESCAPE;
        case SDLK_SPACE: return KEY_SPACE;
        case SDLK_RETURN: return KEY_ENTER;
        case SDLK_BACKSPACE: return KEY_BACKSPACE;
        case SDLK_TAB: return KEY_TAB;
        case SDLK_LSHIFT: return KEY_LEFT_SHIFT;
        case SDLK_RSHIFT: return KEY_RIGHT_SHIFT;
        case SDLK_LCTRL: return KEY_LEFT_CTRL;
        case SDLK_RCTRL: return KEY_RIGHT_CTRL;
        case SDLK_LALT: return KEY_LEFT_ALT;
        case SDLK_RALT: return KEY_RIGHT_ALT;
        case SDLK_LGUI: return KEY_LEFT_SUPER;
        case SDLK_RGUI: return KEY_RIGHT_SUPER;
        
        case SDLK_UP: return KEY_UP;
        case SDLK_DOWN: return KEY_DOWN;
        case SDLK_LEFT: return KEY_LEFT;
        case SDLK_RIGHT: return KEY_RIGHT;
        
        case SDLK_INSERT: return KEY_INSERT;
        case SDLK_DELETE: return KEY_DELETE;
        case SDLK_HOME: return KEY_HOME;
        case SDLK_END: return KEY_END;
        case SDLK_PAGEUP: return KEY_PAGE_UP;
        case SDLK_PAGEDOWN: return KEY_PAGE_DOWN;
        
        case SDLK_BACKQUOTE: return KEY_GRAVE;
        case SDLK_MINUS: return KEY_MINUS;
        case SDLK_EQUALS: return KEY_EQUALS;
        case SDLK_SLASH: return KEY_SLASH;
        case SDLK_BACKSLASH: return KEY_BACKSLASH;
        case SDLK_SEMICOLON: return KEY_SEMICOLON;
        case SDLK_QUOTE: return KEY_APOSTROPHE;
        case SDLK_COMMA: return KEY_COMMA;
        case SDLK_PERIOD: return KEY_PERIOD;
        case SDLK_LEFTBRACKET: return KEY_LEFT_BRACKET;
        case SDLK_RIGHTBRACKET: return KEY_RIGHT_BRACKET;
        
        case SDLK_KP_0: return KEY_KP_0;
        case SDLK_KP_1: return KEY_KP_1;
        case SDLK_KP_2: return KEY_KP_2;
        case SDLK_KP_3: return KEY_KP_3;
        case SDLK_KP_4: return KEY_KP_4;
        case SDLK_KP_5: return KEY_KP_5;
        case SDLK_KP_6: return KEY_KP_6;
        case SDLK_KP_7: return KEY_KP_7;
        case SDLK_KP_8: return KEY_KP_8;
        case SDLK_KP_9: return KEY_KP_9;
        case SDLK_KP_PLUS: return KEY_KP_PLUS;
        case SDLK_KP_MINUS: return KEY_KP_MINUS;
        case SDLK_KP_MULTIPLY: return KEY_KP_MULTIPLY;
        case SDLK_KP_DIVIDE: return KEY_KP_DIVIDE;
        case SDLK_KP_PERIOD: return KEY_KP_DECIMAL;
        case SDLK_KP_ENTER: return KEY_KP_ENTER;
        case SDLK_NUMLOCKCLEAR: return KEY_NUM_LOCK;
        case SDLK_CAPSLOCK: return KEY_CAPS_LOCK;
        case SDLK_SCROLLLOCK: return KEY_SCROLL_LOCK;
        
        case SDLK_PRINTSCREEN: return KEY_PRINT_SCREEN;
        case SDLK_PAUSE: return KEY_PAUSE;
        
        default: return KEY_UNKNOWN;
    }
}

void Input_Init(void)
{
    memset(currentKeys, 0, sizeof(currentKeys));
    memset(previousKeys, 0, sizeof(previousKeys));

    memset(currentMouse, 0, sizeof(currentMouse));
    memset(previousMouse, 0, sizeof(previousMouse));
}

void Input_Update(void)
{
    memcpy(previousKeys, currentKeys, sizeof(currentKeys));
    memcpy(previousMouse, currentMouse, sizeof(currentMouse));
}

void Input_ProcessEvent(const SDL_Event* e)
{
    switch (e->type)
    {
        case SDL_KEYDOWN:
        {
            KeyCode key = SDLKeyToKeyCode(e->key.keysym.sym);
            if (key != KEY_UNKNOWN)
                currentKeys[key] = true;
            break;
        }

        case SDL_KEYUP:
        {
            KeyCode key = SDLKeyToKeyCode(e->key.keysym.sym);
            if (key != KEY_UNKNOWN)
                currentKeys[key] = false;
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
            if (e->button.button == SDL_BUTTON_LEFT)
                currentMouse[MOUSE_LEFT] = true;
            if (e->button.button == SDL_BUTTON_RIGHT)
                currentMouse[MOUSE_RIGHT] = true;
            if (e->button.button == SDL_BUTTON_MIDDLE)
                currentMouse[MOUSE_MIDDLE] = true;
            if (e->button.button == SDL_BUTTON_X1)
                currentMouse[MOUSE_BUTTON_4] = true;
            if (e->button.button == SDL_BUTTON_X2)
                currentMouse[MOUSE_BUTTON_5] = true;
            break;

        case SDL_MOUSEBUTTONUP:
            if (e->button.button == SDL_BUTTON_LEFT)
                currentMouse[MOUSE_LEFT] = false;
            if (e->button.button == SDL_BUTTON_RIGHT)
                currentMouse[MOUSE_RIGHT] = false;
            if (e->button.button == SDL_BUTTON_MIDDLE)
                currentMouse[MOUSE_MIDDLE] = false;
            if (e->button.button == SDL_BUTTON_X1)
                currentMouse[MOUSE_BUTTON_4] = false;
            if (e->button.button == SDL_BUTTON_X2)
                currentMouse[MOUSE_BUTTON_5] = false;
            break;

        case SDL_MOUSEMOTION:
            mouseX = e->motion.x;
            mouseY = e->motion.y;
            break;
    }
}

bool Input_IsKeyDown(KeyCode key)
{
    return currentKeys[key];
}

bool Input_IsKeyPressed(KeyCode key)
{
    return currentKeys[key] && !previousKeys[key];
}

bool Input_IsKeyReleased(KeyCode key)
{
    return !currentKeys[key] && previousKeys[key];
}

bool Input_IsMouseDown(MouseButton button)
{
    return currentMouse[button];
}

bool Input_IsMousePressed(MouseButton button)
{
    return currentMouse[button] && !previousMouse[button];
}

bool Input_IsMouseReleased(MouseButton button)
{
    return !currentMouse[button] && previousMouse[button];
}

int Input_GetMouseX(void) { return mouseX; }
int Input_GetMouseY(void) { return mouseY; }
