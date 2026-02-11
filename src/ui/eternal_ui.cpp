#include "eternal_ui.h"
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

static float ClampFloat(float v, float minv, float maxv)
{
    if (v < minv) return minv;
    if (v > maxv) return maxv;
    return v;
}

static float LerpFloat(float a, float b, float t)
{
    return a + (b - a) * t;
}

static int MinInt(int a, int b) { return (a < b) ? a : b; }
static int MaxInt(int a, int b) { return (a > b) ? a : b; }

static Color LerpColor(Color a, Color b, float t)
{
    return Color{
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t
    };
}

static void DrawRoundedRect(float x, float y, float w, float h, float radius, Color color)
{
    Renderer_DrawRectangle(Rect{x, y, w, h}, color);
}

static void DrawShadow(float x, float y, float w, float h, float blur)
{
    float shadowAlpha = 0.15f;
    Color shadow = Color{0, 0, 0, shadowAlpha};
    
    Renderer_DrawRectangle(Rect{x + blur, y + blur, w, h}, shadow);
}

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

EternalUITheme EternalUI_GetDefaultTheme()
{
    return EternalUITheme{
        Color{0.0f, 0.0f, 0.0f, 1.0f},
        Color{0.05f, 0.05f, 0.05f, 1.0f},
        Color{0.1f, 0.1f, 0.1f, 1.0f},
        
        Color{0.2f, 0.6f, 1.0f, 1.0f},
        Color{0.3f, 0.7f, 1.0f, 1.0f},
        
        Color{0.95f, 0.95f, 0.98f, 1.0f},
        Color{0.65f, 0.65f, 0.7f, 1.0f},
        
        Color{0.3f, 0.5f, 0.8f, 0.6f},
        Color{0.5f, 0.8f, 1.0f, 0.3f}
    };
}

void EternalUI_Init(EternalUIContext* ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(EternalUIContext));
    ctx->theme = EternalUI_GetDefaultTheme();
    ctx->rowHeight = 20.0f;
    ctx->padding = 3.0f;
}

void EternalUI_BeginFrame(EternalUIContext* ctx, float dt)
{
    if (!ctx) return;
    
    ctx->deltaTime = dt;
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
    
    if (ctx->lastHoveredId != ctx->hotId)
    {
        ctx->hoverAlpha = LerpFloat(ctx->hoverAlpha, 0.0f, ClampFloat(dt * 5.0f, 0.0f, 1.0f));
        if (ctx->hoverAlpha < 0.01f)
        {
            ctx->lastHoveredId = ctx->hotId;
            ctx->hoverAlpha = 0.0f;
        }
    }
    else if (ctx->hotId != 0)
    {
        ctx->hoverAlpha = LerpFloat(ctx->hoverAlpha, 1.0f, ClampFloat(dt * 8.0f, 0.0f, 1.0f));
    }
}

void EternalUI_EndFrame(EternalUIContext* ctx)
{
    if (!ctx) return;
    if (!ctx->mouseDown)
        ctx->activeId = 0;
}

void EternalUI_BeginWindow(EternalUIContext* ctx, const char* title, float* x, float* y, float w, float h)
{
    if (!ctx || !x || !y) return;
    
    float wx = *x;
    float wy = *y;
    
    float titleH = 28.0f;
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
    ctx->cursorX = wx + 10.0f;
    ctx->cursorY = wy + 35.0f;
    ctx->rowHeight = 22.0f;
    ctx->padding = 4.0f;
    
    Renderer_DrawRectangle(Rect{wx, wy, w, h}, ctx->theme.bgDark);
    
    Renderer_DrawRectangleLines(Rect{wx, wy, w, h}, 1, ctx->theme.border);
    
    Renderer_DrawRectangle(Rect{wx, wy, w, titleH}, ctx->theme.bgMedium);
    Renderer_DrawRectangle(Rect{wx, wy + titleH - 1, w, 1}, ctx->theme.accent);
    
    Renderer_DrawTextEx(title, wx + 10.0f, wy + 6.0f, 13.0f, ctx->theme.textPrimary, TEXT_STYLE_NORMAL);
}

void EternalUI_EndWindow(EternalUIContext* ctx)
{
    if (!ctx) return;
    ctx->inWindow = false;
}

void EternalUI_Text(EternalUIContext* ctx, const char* text)
{
    EternalUI_TextEx(ctx, text, 12.0f);
}

void EternalUI_TextEx(EternalUIContext* ctx, const char* text, float fontSize)
{
    if (!ctx || !ctx->inWindow) return;
    Renderer_DrawTextEx(text, ctx->cursorX, ctx->cursorY, fontSize, ctx->theme.textPrimary, TEXT_STYLE_NORMAL);
    ctx->cursorY += ctx->rowHeight;
}

bool EternalUI_Button(EternalUIContext* ctx, const char* label, float w, float h)
{
    return EternalUI_ButtonEx(ctx, label, w, h, Color{0.4f, 0.4f, 0.45f, 1.0f});
}

bool EternalUI_ButtonEx(EternalUIContext* ctx, const char* label, float w, float h, Color color)
{
    if (!ctx || !ctx->inWindow) return false;
    
    int id = HashId(label);
    float x = ctx->cursorX;
    float y = ctx->cursorY;
    float bw = (w > 0.0f) ? w : (ctx->winW - 20.0f);
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
    
    Color btnColor = color;
    if (ctx->activeId == id)
    {
        btnColor = Color{
            color.r * 0.6f,
            color.g * 0.6f,
            color.b * 0.6f,
            color.a
        };
    }
    else if (hovered)
    {
        btnColor = Color{
            color.r + (1.0f - color.r) * 0.2f,
            color.g + (1.0f - color.g) * 0.2f,
            color.b + (1.0f - color.b) * 0.2f,
            color.a
        };
    }
    
    Renderer_DrawRectangle(Rect{x, y, bw, bh}, btnColor);
    
    Renderer_DrawRectangleLines(Rect{x, y, bw, bh}, 1, Color{0.25f, 0.25f, 0.25f, 1.0f});
    
    Renderer_DrawTextEx(label, x + 6.0f, y + 4.0f, 12.0f, Color{0.9f, 0.9f, 0.9f, 1.0f}, TEXT_STYLE_NORMAL);
    
    ctx->cursorY += bh + ctx->padding;
    return clicked;
}

bool EternalUI_Checkbox(EternalUIContext* ctx, const char* label, bool* value)
{
    if (!ctx || !ctx->inWindow) return false;
    
    int id = HashId(label);
    float x = ctx->cursorX;
    float y = ctx->cursorY;
    float size = 16.0f;
    
    bool checkboxHovered = PointInRect(ctx->mouseX, ctx->mouseY, x, y, size, size);
    bool labelHovered = PointInRect(ctx->mouseX, ctx->mouseY, x + size + 8.0f, y, ctx->winW - 30.0f, size);
    bool hovered = checkboxHovered || labelHovered;
    
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
    
    Color cbBg = hovered ? ctx->theme.bgLight : ctx->theme.bgMedium;
    Renderer_DrawRectangle(Rect{x, y, size, size}, cbBg);
    Renderer_DrawRectangleLines(Rect{x, y, size, size}, 1, ctx->theme.border);
    
    if (value && *value)
    {
        Renderer_DrawRectangle(Rect{x + 3.0f, y + 3.0f, size - 6.0f, size - 6.0f}, ctx->theme.accent);
    }
    
    Renderer_DrawTextEx(label, x + size + 8.0f, y + 1.0f, 12.0f, ctx->theme.textPrimary, TEXT_STYLE_NORMAL);
    ctx->cursorY += size + ctx->padding;
    
    return changed;
}

bool EternalUI_InputText(EternalUIContext* ctx, const char* label, char* buffer, int bufferSize)
{
    if (!ctx || !ctx->inWindow || !buffer || bufferSize <= 0) return false;
    
    int id = HashId(label);
    float x = ctx->cursorX;
    float y = ctx->cursorY;
    float bw = ctx->winW - 20.0f;
    float bh = 20.0f;
    float fontSize = 11.0f;
    
    float labelWidth = 45.0f;
    float inputX = x + labelWidth;
    float inputW = bw - labelWidth;
    
    bool hovered = PointInRect(ctx->mouseX, ctx->mouseY, inputX, y, inputW, bh);
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
            ctx->caret = len;
            ctx->textSelStart = 0;
            ctx->textSelEnd = 0;
        }
        else if (ctx->kbdId == id)
        {
            ctx->kbdId = 0;
        }
    }
    
    bool focused = (ctx->kbdId == id);
    if (focused)
    {
        ctx->wantsKeyboard = true;
        int len = (int)strlen(buffer);
        ctx->caret = ClampInt(ctx->caret, 0, len);
        
        if (ctx->mouseReleased)
        {
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
                len = (int)strlen(buffer);
            }
        }
        if (ctrlDown && Input_IsKeyPressed(KEY_V))
        {
            int inserted = PasteFromClipboard(buffer, bufferSize, 0);
            if (inserted > 0)
            {
                ctx->caret = inserted;
                len = (int)strlen(buffer);
            }
        }
        
        if (!ctrlDown && ctx->textInputLen > 0)
        {
            int inserted = InsertTextAt(buffer, bufferSize, ctx->caret, ctx->textInput, ctx->textInputLen);
            if (inserted > 0)
            {
                ctx->caret += inserted;
            }
        }
        if (ctx->keyBackspace)
        {
            if (ctx->caret > 0)
            {
                DeleteRange(buffer, ctx->caret - 1, ctx->caret);
                ctx->caret -= 1;
            }
        }
        if (ctx->keyEnter)
        {
            ctx->kbdId = 0;
        }
    }
    
    Renderer_DrawRectangle(Rect{x, y, labelWidth, bh}, Color{0.1f, 0.1f, 0.1f, 1.0f});
    Renderer_DrawRectangleLines(Rect{x, y, labelWidth, bh}, 1, Color{0.25f, 0.25f, 0.25f, 1.0f});
    Renderer_DrawTextEx(label, x + 4.0f, y + 4.0f, fontSize, Color{0.7f, 0.7f, 0.75f, 1.0f}, TEXT_STYLE_NORMAL);
    
    Color inputBg = focused ? Color{0.08f, 0.08f, 0.08f, 1.0f} : Color{0.05f, 0.05f, 0.05f, 1.0f};
    Renderer_DrawRectangle(Rect{inputX, y, inputW, bh}, inputBg);
    
    Color borderColor = focused ? Color{0.2f, 0.6f, 1.0f, 1.0f} : Color{0.25f, 0.25f, 0.25f, 1.0f};
    Renderer_DrawRectangleLines(Rect{inputX, y, inputW, bh}, 1, borderColor);
    
    float textX = inputX + 3.0f;
    Renderer_DrawTextEx(buffer, textX, y + 4.0f, fontSize, Color{0.9f, 0.9f, 0.9f, 1.0f}, TEXT_STYLE_NORMAL);
    
    if (focused)
    {
        int bufLen = (int)strlen(buffer);
        float cursorX = textX + (bufLen * 6.5f);
        if (cursorX < inputX + inputW - 4.0f)
        {
            Renderer_DrawLineEx(Vec2{cursorX, y + 3.0f}, Vec2{cursorX, y + bh - 3.0f}, 1.0f, Color{0.2f, 0.6f, 1.0f, 1.0f});
        }
    }
    
    ctx->cursorY += bh + ctx->padding;
    return focused;
}

void EternalUI_Separator(EternalUIContext* ctx)
{
    if (!ctx || !ctx->inWindow) return;
    
    float x = ctx->cursorX;
    float y = ctx->cursorY + ctx->rowHeight * 0.3f;
    float w = ctx->winW - 20.0f;
    
    Renderer_DrawRectangle(Rect{x, y, w, 1}, ctx->theme.border);
    ctx->cursorY += ctx->rowHeight * 0.6f;
}

void EternalUI_Spacing(EternalUIContext* ctx, float height)
{
    if (!ctx || !ctx->inWindow) return;
    ctx->cursorY += height;
}

void EternalUI_SetTheme(EternalUIContext* ctx, const EternalUITheme* theme)
{
    if (!ctx || !theme) return;
    ctx->theme = *theme;
}
