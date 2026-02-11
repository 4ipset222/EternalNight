#ifndef __ETERNAL_UI_H__
#define __ETERNAL_UI_H__

#include "engine/forge.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct EternalUITheme
{
    Color bgDark;
    Color bgMedium;
    Color bgLight;
    Color accent;
    Color accentHover;
    Color textPrimary;
    Color textSecondary;
    Color border;
    Color highlight;
} EternalUITheme;

typedef struct EternalUIContext
{
    float mouseX;
    float mouseY;
    bool mouseDown;
    bool mousePressed;
    bool mouseReleased;
    
    bool wantsMouse;
    bool wantsKeyboard;
    
    int hotId;
    int activeId;
    int kbdId;
    
    float cursorX;
    float cursorY;
    float winX;
    float winY;
    float winW;
    float winH;
    float rowHeight;
    float padding;
    bool inWindow;
    
    bool dragging;
    float dragOffsetX;
    float dragOffsetY;
    
    int caret;
    int textSelStart;
    int textSelEnd;
    bool selecting;
    
    const char* textInput;
    int textInputLen;
    bool keyBackspace;
    bool keyEnter;
    
    float deltaTime;
    int lastHoveredId;
    float hoverAlpha;
    
    EternalUITheme theme;
} EternalUIContext;

void EternalUI_Init(EternalUIContext* ctx);

void EternalUI_BeginFrame(EternalUIContext* ctx, float dt);
void EternalUI_EndFrame(EternalUIContext* ctx);

void EternalUI_BeginWindow(EternalUIContext* ctx, const char* title, float* x, float* y, float w, float h);
void EternalUI_EndWindow(EternalUIContext* ctx);

void EternalUI_Text(EternalUIContext* ctx, const char* text);
void EternalUI_TextEx(EternalUIContext* ctx, const char* text, float fontSize);
bool EternalUI_Button(EternalUIContext* ctx, const char* label, float w, float h);
bool EternalUI_ButtonEx(EternalUIContext* ctx, const char* label, float w, float h, Color color);
bool EternalUI_Checkbox(EternalUIContext* ctx, const char* label, bool* value);
bool EternalUI_InputText(EternalUIContext* ctx, const char* label, char* buffer, int bufferSize);
void EternalUI_Separator(EternalUIContext* ctx);
void EternalUI_Spacing(EternalUIContext* ctx, float height);

void EternalUI_SetTheme(EternalUIContext* ctx, const EternalUITheme* theme);
EternalUITheme EternalUI_GetDefaultTheme();

#ifdef __cplusplus
}
#endif

#endif // __ETERNAL_UI_H__
