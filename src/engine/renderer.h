#ifndef __FORGE_RENDERER_H__
#define __FORGE_RENDERER_H__

#include "shader.h"
#include "texture.h"
#include "vmath.h"
#include "window.h"
#include "camera.h"
#include <GL/glew.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Renderer
{
    Window* window;
    int width;
    int height;
    
    Shader primitive_shader;
    Shader texture_shader;
    Shader text_shader;
    
    GLuint primitive_vao;
    GLuint primitive_vbo;
    GLuint primitive_color_vbo;
    GLuint primitive_ibo;
    
    GLuint texture_vao;
    GLuint texture_vbo;
    GLuint texture_ibo;

    GLuint text_vao;
    GLuint text_vbo;
    
    GLint proj_uniform;
    GLint transform_uniform;
    GLint text_proj_uniform;
    GLint text_transform_uniform;
    GLint text_sampler_uniform;
} Renderer;

extern Renderer* g_renderer;

typedef struct Glyph
{
    int x;
    int y;
    int width;
    int height;
    int advance;
    int bearingX;
    int bearingY;
} Glyph;

typedef struct Font
{
    Texture2D atlas;
    int size;
    int ascent;
    int descent;
    int line_height;
    Glyph glyphs[128];
} Font;

typedef enum TextStyle
{
    TEXT_STYLE_NORMAL = 0,
    TEXT_STYLE_SHADOW = 1,
    TEXT_STYLE_OUTLINE = 2,
    TEXT_STYLE_OUTLINE_SHADOW = 3
} TextStyle;

Texture2D* LoadTexture(const char* path);

void SetGlobalRenderer(Renderer* r);
Renderer* GetGlobalRenderer(void);

Renderer* Renderer_Create(Window* window);
void      Renderer_Destroy(Renderer* r);

void Renderer_BeginFrame(Renderer* r);
void Renderer_EndFrame(Renderer* r);

void Renderer_Clear(Color color);

void DrawRectangle(Rect rect, Color color);
void DrawRectangleLines(Rect rect, int line_thick, Color color);

void DrawTrianglesColored(const float* positions, const float* colors, int vertexCount);

void DrawCircle(Vec2 center, float radius, Color color);
void DrawCircleLines(Vec2 center, float radius, Color color);

void DrawLine(Vec2 start, Vec2 end, Color color);
void DrawLineEx(Vec2 start, Vec2 end, float thick, Color color);

void DrawTriangle(Vec2 v1, Vec2 v2, Vec2 v3, Color color);
void DrawTriangleLines(Vec2 v1, Vec2 v2, Vec2 v3, Color color);

void DrawTexture(Texture2D texture, Vec2 position, Color tint);
void DrawTextureRec(Texture2D texture, Rect source, Vec2 position, Color tint);
void DrawTextureEx(Texture2D texture, Vec2 position, float rotation, float scale, Color tint);
void DrawTexturePro(Texture2D texture, Rect source, Rect dest, Vec2 origin, float rotation, Color tint);

Font* LoadFontTTF(const char* path, int pixel_size);
void  UnloadFont(Font* font);
void  SetDefaultFont(Font* font);
void  Renderer_DrawText(const char* text, float x, float y, float fontSize, Color color);
void  Renderer_DrawTextEx(const char* text, float x, float y, float fontSize, Color color, TextStyle style);

#ifdef __cplusplus
}
#endif

#endif // __FORGE_RENDERER_H__
