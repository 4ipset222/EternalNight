#include "imgui_lite.h"
#include "engine/input.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL2/SDL.h>

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

static int ClampInt(int v, int minv, int maxv)
{
    if (v < minv) return minv;
    if (v > maxv) return maxv;
    return v;
}

static int MinInt(int a, int b) { return (a < b) ? a : b; }
static int MaxInt(int a, int b) { return (a > b) ? a : b; }

static void DeleteRange(char* buffer, int start, int end)
{
    if (!buffer) return;
    int len = (int)strlen(buffer);
    start = ClampInt(start, 0, len);
    end = ClampInt(end, 0, len);
    if (start >= end) return;
    memmove(buffer + start, buffer + end, (size_t)(len - end + 1));
}

static int InsertTextAt(char* buffer, int bufferSize, int pos, const char* text, int textLen)
{
    if (!buffer || !text || bufferSize <= 0) return 0;
    int len = (int)strlen(buffer);
    pos = ClampInt(pos, 0, len);
    if (textLen < 0) textLen = (int)strlen(text);
    int available = bufferSize - 1 - len;
    if (available <= 0 || textLen <= 0) return 0;
    if (textLen > available) textLen = available;
    memmove(buffer + pos + textLen, buffer + pos, (size_t)(len - pos + 1));
    memcpy(buffer + pos, text, (size_t)textLen);
    return textLen;
}

static int GetCaretFromMouse(float mouseX, float textStartX, float charW, int len)
{
    if (charW <= 0.0f) return len;
    float rel = mouseX - textStartX;
    int caret = (int)floorf(rel / charW + 0.5f);
    return ClampInt(caret, 0, len);
}

static int CopySelectionToClipboard(const char* buffer, int start, int end)
{
    if (!buffer) return 0;
    int len = (int)strlen(buffer);
    start = ClampInt(start, 0, len);
    end = ClampInt(end, 0, len);
    if (start >= end) return 0;
    int selLen = end - start;
    char* tmp = (char*)malloc((size_t)selLen + 1);
    if (!tmp) return 0;
    memcpy(tmp, buffer + start, (size_t)selLen);
    tmp[selLen] = '\0';
    SDL_SetClipboardText(tmp);
    free(tmp);
    return selLen;
}

static int PasteFromClipboard(char* buffer, int bufferSize, int caret)
{
    if (!SDL_HasClipboardText()) return 0;
    char* clip = SDL_GetClipboardText();
    if (!clip) return 0;
    char paste[512];
    int p = 0;
    for (const char* s = clip; *s && p < (int)sizeof(paste) - 1; ++s)
    {
        if (*s == '\r' || *s == '\n')
            continue;
        paste[p++] = *s;
    }
    paste[p] = '\0';
    SDL_free(clip);
    if (p == 0) return 0;
    return InsertTextAt(buffer, bufferSize, caret, paste, p);
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

    Renderer_DrawRectangle(Rect{wx, wy, w, h}, Color{0.08f, 0.08f, 0.10f, 0.92f});
    Renderer_DrawRectangleLines(Rect{wx, wy, w, h}, 2, Color{0.3f, 0.3f, 0.35f, 1.0f});
    Renderer_DrawRectangle(Rect{wx, wy, w, titleH}, Color{0.12f, 0.12f, 0.16f, 1.0f});
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

    Renderer_DrawRectangle(Rect{x, y, bw, bh}, base);
    Renderer_DrawRectangleLines(Rect{x, y, bw, bh}, 1, Color{0.6f, 0.6f, 0.7f, 1.0f});
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

    Renderer_DrawRectangle(Rect{x, y, size, size}, Color{0.12f, 0.12f, 0.16f, 1.0f});
    Renderer_DrawRectangleLines(Rect{x, y, size, size}, 1, Color{0.6f, 0.6f, 0.7f, 1.0f});
    if (value && *value)
        Renderer_DrawRectangle(Rect{x + 4.0f, y + 4.0f, size - 8.0f, size - 8.0f}, Color{0.3f, 0.8f, 0.4f, 1.0f});

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
    float fontSize = 13.0f;
    float charW = fontSize * 0.50f;
    int labelLen = (int)strlen(label);
    float labelStartX = x + 6.0f;
    float textStartX = labelStartX + (float)labelLen * charW + 2.0f * charW - 2.0f;
    float caretOffset = -1.0f;

    bool hovered = PointInRect(ctx->mouseX, ctx->mouseY, x, y, bw, bh);
    if (hovered)
    {
        ctx->hotId = id;
        ctx->wantsMouse = true;
    }

    if (ctx->mousePressed)
    {
        if (hovered)
        {
            ctx->activeId = id;
            ctx->kbdId = id;
            int len = (int)strlen(buffer);
            int caret = GetCaretFromMouse(ctx->mouseX, textStartX, charW, len);
            ctx->caret = caret;
            ctx->textSelStart = caret;
            ctx->textSelEnd = caret;
            ctx->selecting = true;
        }
        else if (ctx->kbdId == id)
        {
            ctx->kbdId = 0;
            ctx->selecting = false;
        }
    }

    bool focused = (ctx->kbdId == id);
    if (focused)
    {
        ctx->wantsKeyboard = true;
        int len = (int)strlen(buffer);
        ctx->caret = ClampInt(ctx->caret, 0, len);
        ctx->textSelStart = ClampInt(ctx->textSelStart, 0, len);
        ctx->textSelEnd = ClampInt(ctx->textSelEnd, 0, len);

        if (ctx->mouseDown && ctx->selecting)
        {
            int caret = GetCaretFromMouse(ctx->mouseX, textStartX, charW, len);
            ctx->caret = caret;
            ctx->textSelEnd = caret;
        }
        if (ctx->mouseReleased)
        {
            ctx->selecting = false;
        }

        int selStart = MinInt(ctx->textSelStart, ctx->textSelEnd);
        int selEnd = MaxInt(ctx->textSelStart, ctx->textSelEnd);
        bool hasSelection = (selStart != selEnd);

        bool ctrlDown = Input_IsKeyDown(KEY_LEFT_CTRL) || Input_IsKeyDown(KEY_RIGHT_CTRL);
        if (ctrlDown && Input_IsKeyPressed(KEY_C))
        {
            CopySelectionToClipboard(buffer, selStart, selEnd);
        }
        if (ctrlDown && Input_IsKeyPressed(KEY_X))
        {
            if (CopySelectionToClipboard(buffer, selStart, selEnd) > 0)
            {
                DeleteRange(buffer, selStart, selEnd);
                ctx->caret = selStart;
                ctx->textSelStart = ctx->caret;
                ctx->textSelEnd = ctx->caret;
                len = (int)strlen(buffer);
            }
        }
        if (ctrlDown && Input_IsKeyPressed(KEY_V))
        {
            if (hasSelection)
            {
                DeleteRange(buffer, selStart, selEnd);
                ctx->caret = selStart;
                ctx->textSelStart = ctx->caret;
                ctx->textSelEnd = ctx->caret;
                len = (int)strlen(buffer);
                hasSelection = false;
            }
            int inserted = PasteFromClipboard(buffer, bufferSize, ctx->caret);
            if (inserted > 0)
            {
                ctx->caret += inserted;
                ctx->textSelStart = ctx->caret;
                ctx->textSelEnd = ctx->caret;
                len = (int)strlen(buffer);
            }
        }

        if (!ctrlDown && ctx->textInputLen > 0)
        {
            if (hasSelection)
            {
                DeleteRange(buffer, selStart, selEnd);
                ctx->caret = selStart;
                ctx->textSelStart = ctx->caret;
                ctx->textSelEnd = ctx->caret;
                len = (int)strlen(buffer);
            }
            int inserted = InsertTextAt(buffer, bufferSize, ctx->caret, ctx->textInput, ctx->textInputLen);
            if (inserted > 0)
            {
                ctx->caret += inserted;
                ctx->textSelStart = ctx->caret;
                ctx->textSelEnd = ctx->caret;
            }
        }
        if (ctx->keyBackspace)
        {
            if (hasSelection)
            {
                DeleteRange(buffer, selStart, selEnd);
                ctx->caret = selStart;
                ctx->textSelStart = ctx->caret;
                ctx->textSelEnd = ctx->caret;
            }
            else if (ctx->caret > 0)
            {
                DeleteRange(buffer, ctx->caret - 1, ctx->caret);
                ctx->caret -= 1;
                ctx->textSelStart = ctx->caret;
                ctx->textSelEnd = ctx->caret;
            }
        }
        if (ctx->keyEnter)
        {
            ctx->kbdId = 0;
            ctx->selecting = false;
        }
    }

    Color base = hovered ? Color{0.22f, 0.22f, 0.28f, 1.0f} : Color{0.16f, 0.16f, 0.20f, 1.0f};
    if (focused)
        base = Color{0.25f, 0.25f, 0.32f, 1.0f};

    Renderer_DrawRectangle(Rect{x, y, bw, bh}, base);
    Renderer_DrawRectangleLines(Rect{x, y, bw, bh}, 1, Color{0.6f, 0.6f, 0.7f, 1.0f});

    if (focused)
    {
        int selStart = MinInt(ctx->textSelStart, ctx->textSelEnd);
        int selEnd = MaxInt(ctx->textSelStart, ctx->textSelEnd);
        if (selStart != selEnd)
        {
            float selX = textStartX + (float)selStart * charW + caretOffset;
            float selW = (float)(selEnd - selStart) * charW;
            float maxW = (x + bw - 4.0f) - selX;
            if (selW > maxW) selW = maxW;
            if (selW > 0.0f)
            {
                Renderer_DrawRectangle(Rect{selX, y + 4.0f, selW, 14.0f}, Color{0.20f, 0.45f, 0.90f, 0.45f});
            }
        }
    }

    Renderer_DrawTextEx(label, labelStartX, y + 4.0f, fontSize, Color{1, 1, 1, 1}, TEXT_STYLE_OUTLINE_SHADOW);
    Renderer_DrawTextEx(": ", labelStartX + (float)labelLen * charW, y + 4.0f, fontSize, Color{1, 1, 1, 1}, TEXT_STYLE_OUTLINE_SHADOW);
    Renderer_DrawTextEx(buffer, textStartX, y + 4.0f, fontSize, Color{1, 1, 1, 1}, TEXT_STYLE_OUTLINE_SHADOW);

    if (focused)
    {
        float caretX = textStartX + (float)ctx->caret * charW + caretOffset;
        if (caretX < x + bw - 3.0f)
        {
            Renderer_DrawLineEx(Vec2{caretX, y + 4.0f}, Vec2{caretX, y + 18.0f}, 1.0f, Color{1.0f, 1.0f, 1.0f, 1.0f});
        }
    }

    ctx->cursorY += bh + ctx->padding;
    return focused;
}

