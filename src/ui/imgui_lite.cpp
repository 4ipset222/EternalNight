// Wrapper implementation that delegates to EternalUI
// This maintains backward compatibility while using the modern UI system

#include "imgui_lite.h"
#include "eternal_ui.h"

void ImGuiLite_BeginFrame(ImGuiLiteContext* ctx)
{
    if (!ctx) return;
    
    if (ctx->theme.accent.r < 0.01f && ctx->theme.accent.g < 0.01f)
    {
        EternalUI_Init(ctx);
    }
    
    EternalUI_BeginFrame(ctx, 0.016f);
}

void ImGuiLite_EndFrame(ImGuiLiteContext* ctx)
{
    EternalUI_EndFrame(ctx);
}

void ImGuiLite_BeginWindow(ImGuiLiteContext* ctx, const char* title, float* x, float* y, float w, float h)
{
    EternalUI_BeginWindow(ctx, title, x, y, w, h);
}

void ImGuiLite_EndWindow(ImGuiLiteContext* ctx)
{
    EternalUI_EndWindow(ctx);
}

void ImGuiLite_Text(ImGuiLiteContext* ctx, const char* text)
{
    EternalUI_Text(ctx, text);
}

bool ImGuiLite_Button(ImGuiLiteContext* ctx, const char* label, float w, float h)
{
    return EternalUI_Button(ctx, label, w, h);
}

bool ImGuiLite_Checkbox(ImGuiLiteContext* ctx, const char* label, bool* value)
{
    return EternalUI_Checkbox(ctx, label, value);
}

bool ImGuiLite_InputText(ImGuiLiteContext* ctx, const char* label, char* buffer, int bufferSize)
{
    return EternalUI_InputText(ctx, label, buffer, bufferSize);
}

