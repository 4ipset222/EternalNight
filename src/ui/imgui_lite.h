#ifndef __IMGUI_LITE_H__
#define __IMGUI_LITE_H__

#include "eternal_ui.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef EternalUIContext ImGuiLiteContext;

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

