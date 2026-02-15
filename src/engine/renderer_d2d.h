#ifndef __FORGE_RENDERER_D2D_H__
#define __FORGE_RENDERER_D2D_H__

#include "vmath.h"
#include "renderer.h"
#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct D2DRenderer D2DRenderer;

D2DRenderer* D2DRenderer_Create(SDL_Window* window, int width, int height);
void D2DRenderer_Destroy(D2DRenderer* ctx);
void D2DRenderer_Resize(D2DRenderer* ctx, int width, int height);
int  D2DRenderer_BeginFrame(D2DRenderer* ctx);
void D2DRenderer_EndFrame(D2DRenderer* ctx);
void D2DRenderer_Clear(D2DRenderer* ctx, Color color);
void D2DRenderer_DrawRectangle(D2DRenderer* ctx, Rect rect, Color color);
void D2DRenderer_DrawRectangleLines(D2DRenderer* ctx, Rect rect, float line_thick, Color color);
void D2DRenderer_DrawLine(D2DRenderer* ctx, Vec2 start, Vec2 end, float thick, Color color);
void D2DRenderer_DrawCircle(D2DRenderer* ctx, Vec2 center, float radius, Color color, int filled);
void D2DRenderer_DrawTriangle(D2DRenderer* ctx, Vec2 v1, Vec2 v2, Vec2 v3, Color color, int filled);
void D2DRenderer_DrawTrianglesColored(D2DRenderer* ctx, const float* positions, const float* colors, int vertexCount);
void D2DRenderer_DrawText(D2DRenderer* ctx, const char* text, float x, float y, float fontSize, Color color, TextStyle style);
void* D2DRenderer_LoadBitmap(D2DRenderer* ctx, const char* path, int* width, int* height);
void D2DRenderer_FreeBitmap(void* bitmap);
void D2DRenderer_DrawBitmap(D2DRenderer* ctx, void* bitmap, Rect source, Rect dest, Vec2 origin, float rotation, Color tint);

#ifdef __cplusplus
}
#endif

#endif // __FORGE_RENDERER_D2D_H__
