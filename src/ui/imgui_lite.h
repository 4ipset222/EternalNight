#ifndef __IMGUI_LITE_H__
#define __IMGUI_LITE_H__

#include "engine/forge.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ImGuiLiteContext
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
} ImGuiLiteContext;

void ImGuiLite_BeginFrame(ImGuiLiteContext* ctx);
void ImGuiLite_EndFrame(ImGuiLiteContext* ctx);

void ImGuiLite_BeginWindow(ImGuiLiteContext* ctx, const char* title, float* x, float* y, float w, float h);
void ImGuiLite_EndWindow(ImGuiLiteContext* ctx);

void ImGuiLite_Text(ImGuiLiteContext* ctx, const char* text);
bool ImGuiLite_Button(ImGuiLiteContext* ctx, const char* label, float w, float h);
bool ImGuiLite_Checkbox(ImGuiLiteContext* ctx, const char* label, bool* value);
bool ImGuiLite_InputText(ImGuiLiteContext* ctx, const char* label, char* buffer, int bufferSize);

#ifdef __cplusplus
}
#endif

#endif // __IMGUI_LITE_H__

