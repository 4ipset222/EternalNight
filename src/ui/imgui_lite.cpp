#include "imgui_lite.h"
#include "engine/input.h"
#include <string.h>
#include <stdio.h>

static bool PointInRect(float x, float y, float rx, float ry, float rw, float rh)
{
    return x >= rx && x <= (rx + rw) && y >= ry && y <= (ry + rh);
}

static int HashId(const char* label)
{
    int h = 5381;
    for (const char* p = label; *p; ++p)
        h = ((h << 5) + h) + *p;
    return h;
}

void ImGuiLite_BeginFrame(ImGuiLiteContext* ctx)
{
    if (!ctx) return;

    ctx->mouseX = (float)Input_GetMouseX();
    ctx->mouseY = (float)Input_GetMouseY();
    ctx->mouseDown = Input_IsMouseDown(MOUSE_LEFT);
    ctx->mousePressed = Input_IsMousePressed(MOUSE_LEFT);
    ctx->mouseReleased = Input_IsMouseReleased(MOUSE_LEFT);

    ctx->textInput = Input_GetTextInput();
    ctx->textInputLen = Input_GetTextInputLen();
    ctx->keyBackspace = Input_IsKeyPressed(KEY_BACKSPACE);
    ctx->keyEnter = Input_IsKeyPressed(KEY_ENTER);

    ctx->wantsMouse = false;
    ctx->wantsKeyboard = false;
    ctx->hotId = 0;

    ctx->inWindow = false;
}

void ImGuiLite_EndFrame(ImGuiLiteContext* ctx)
{
    if (!ctx) return;
    if (!ctx->mouseDown)
        ctx->activeId = 0;
}

void ImGuiLite_BeginWindow(ImGuiLiteContext* ctx, const char* title, float* x, float* y, float w, float h)
{
    if (!ctx || !x || !y) return;
    float wx = *x;
    float wy = *y;

    float titleH = 22.0f;
    int titleId = HashId(title);
    bool inTitle = PointInRect(ctx->mouseX, ctx->mouseY, wx, wy, w, titleH);
    if (ctx->mousePressed && inTitle)
    {
        ctx->dragging = true;
        ctx->dragOffsetX = ctx->mouseX - wx;
        ctx->dragOffsetY = ctx->mouseY - wy;
        ctx->activeId = titleId;
    }
    if (!ctx->mouseDown)
    {
        ctx->dragging = false;
    }
    if (ctx->dragging && ctx->activeId == titleId)
    {
        wx = ctx->mouseX - ctx->dragOffsetX;
        wy = ctx->mouseY - ctx->dragOffsetY;
        *x = wx;
        *y = wy;
        ctx->wantsMouse = true;
    }

    ctx->inWindow = true;
    ctx->winX = wx;
    ctx->winY = wy;
    ctx->winW = w;
    ctx->winH = h;
    ctx->cursorX = wx + 12.0f;
    ctx->cursorY = wy + 30.0f;
    ctx->rowHeight = 24.0f;
    ctx->padding = 6.0f;

    DrawRectangle(Rect{wx, wy, w, h}, Color{0.08f, 0.08f, 0.10f, 0.92f});
    DrawRectangleLines(Rect{wx, wy, w, h}, 2, Color{0.3f, 0.3f, 0.35f, 1.0f});
    DrawRectangle(Rect{wx, wy, w, titleH}, Color{0.12f, 0.12f, 0.16f, 1.0f});
    Renderer_DrawTextEx(title, wx + 10.0f, wy + 5.0f, 15.0f, Color{1, 1, 1, 1}, TEXT_STYLE_OUTLINE_SHADOW);
}

void ImGuiLite_EndWindow(ImGuiLiteContext* ctx)
{
    if (!ctx) return;
    ctx->inWindow = false;
}

void ImGuiLite_Text(ImGuiLiteContext* ctx, const char* text)
{
    if (!ctx || !ctx->inWindow) return;
    Renderer_DrawTextEx(text, ctx->cursorX, ctx->cursorY, 14.0f, Color{0.9f, 0.9f, 0.9f, 1.0f}, TEXT_STYLE_OUTLINE_SHADOW);
    ctx->cursorY += ctx->rowHeight;
}

bool ImGuiLite_Button(ImGuiLiteContext* ctx, const char* label, float w, float h)
{
    if (!ctx || !ctx->inWindow) return false;
    int id = HashId(label);
    float x = ctx->cursorX;
    float y = ctx->cursorY;
    float bw = (w > 0.0f) ? w : (ctx->winW - 24.0f);
    float bh = (h > 0.0f) ? h : 22.0f;

    bool hovered = PointInRect(ctx->mouseX, ctx->mouseY, x, y, bw, bh);
    if (hovered)
    {
        ctx->hotId = id;
        ctx->wantsMouse = true;
    }

    if (hovered && ctx->mousePressed)
    {
        ctx->activeId = id;
    }

    bool clicked = false;
    if (ctx->mouseReleased && ctx->activeId == id && hovered)
    {
        clicked = true;
    }

    Color base = hovered ? Color{0.25f, 0.25f, 0.32f, 1.0f} : Color{0.18f, 0.18f, 0.24f, 1.0f};
    if (ctx->activeId == id)
        base = Color{0.30f, 0.30f, 0.38f, 1.0f};

    DrawRectangle(Rect{x, y, bw, bh}, base);
    DrawRectangleLines(Rect{x, y, bw, bh}, 1, Color{0.6f, 0.6f, 0.7f, 1.0f});
    Renderer_DrawTextEx(label, x + 8.0f, y + 4.0f, 14.0f, Color{1, 1, 1, 1}, TEXT_STYLE_OUTLINE_SHADOW);

    ctx->cursorY += bh + ctx->padding;
    return clicked;
}

bool ImGuiLite_Checkbox(ImGuiLiteContext* ctx, const char* label, bool* value)
{
    if (!ctx || !ctx->inWindow) return false;
    int id = HashId(label);
    float x = ctx->cursorX;
    float y = ctx->cursorY;
    float size = 18.0f;

    bool hovered = PointInRect(ctx->mouseX, ctx->mouseY, x, y, size, size) ||
                   PointInRect(ctx->mouseX, ctx->mouseY, x + size + 8.0f, y, ctx->winW - 40.0f, size);

    if (hovered)
    {
        ctx->hotId = id;
        ctx->wantsMouse = true;
    }

    if (hovered && ctx->mousePressed)
    {
        ctx->activeId = id;
    }

    bool changed = false;
    if (ctx->mouseReleased && ctx->activeId == id && hovered)
    {
        if (value) *value = !(*value);
        changed = true;
    }

    DrawRectangle(Rect{x, y, size, size}, Color{0.12f, 0.12f, 0.16f, 1.0f});
    DrawRectangleLines(Rect{x, y, size, size}, 1, Color{0.6f, 0.6f, 0.7f, 1.0f});
    if (value && *value)
        DrawRectangle(Rect{x + 4.0f, y + 4.0f, size - 8.0f, size - 8.0f}, Color{0.3f, 0.8f, 0.4f, 1.0f});

    Renderer_DrawTextEx(label, x + size + 8.0f, y + 2.0f, 14.0f, Color{1, 1, 1, 1}, TEXT_STYLE_OUTLINE_SHADOW);
    ctx->cursorY += size + ctx->padding;

    return changed;
}

bool ImGuiLite_InputText(ImGuiLiteContext* ctx, const char* label, char* buffer, int bufferSize)
{
    if (!ctx || !ctx->inWindow || !buffer || bufferSize <= 0) return false;
    int id = HashId(label);
    float x = ctx->cursorX;
    float y = ctx->cursorY;
    float bw = ctx->winW - 24.0f;
    float bh = 22.0f;

    bool hovered = PointInRect(ctx->mouseX, ctx->mouseY, x, y, bw, bh);
    if (hovered)
    {
        ctx->hotId = id;
        ctx->wantsMouse = true;
    }

    if (hovered && ctx->mousePressed)
    {
        ctx->activeId = id;
        ctx->kbdId = id;
    }

    bool focused = (ctx->kbdId == id);
    if (focused)
    {
        ctx->wantsKeyboard = true;
        if (ctx->textInputLen > 0)
        {
            int len = (int)strlen(buffer);
            int available = bufferSize - 1 - len;
            if (available > 0)
            {
                int copyCount = ctx->textInputLen;
                if (copyCount > available) copyCount = available;
                memcpy(buffer + len, ctx->textInput, (size_t)copyCount);
                buffer[len + copyCount] = '\0';
            }
        }
        if (ctx->keyBackspace)
        {
            int len = (int)strlen(buffer);
            if (len > 0)
                buffer[len - 1] = '\0';
        }
        if (ctx->keyEnter)
        {
            ctx->kbdId = 0;
        }
    }

    Color base = hovered ? Color{0.22f, 0.22f, 0.28f, 1.0f} : Color{0.16f, 0.16f, 0.20f, 1.0f};
    if (focused)
        base = Color{0.25f, 0.25f, 0.32f, 1.0f};

    DrawRectangle(Rect{x, y, bw, bh}, base);
    DrawRectangleLines(Rect{x, y, bw, bh}, 1, Color{0.6f, 0.6f, 0.7f, 1.0f});

    char labelText[128];
    snprintf(labelText, sizeof(labelText), "%s: %s", label, buffer);
    Renderer_DrawTextEx(labelText, x + 6.0f, y + 4.0f, 13.0f, Color{1, 1, 1, 1}, TEXT_STYLE_OUTLINE_SHADOW);

    ctx->cursorY += bh + ctx->padding;
    return focused;
}
