#ifndef __FORGE_RENDERTEXTURE_H__
#define __FORGE_RENDERTEXTURE_H__

#include "texture.h"
#include <GL/glew.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct RenderTexture
{
    GLuint framebuffer;
    GLuint renderbuffer;
    Texture2D texture;
    int width;
    int height;
} RenderTexture;

RenderTexture LoadRenderTexture(int width, int height);
void UnloadRenderTexture(RenderTexture rt);

void BeginTextureMode(RenderTexture rt);
void EndTextureMode(void);

#ifdef __cplusplus
}
#endif

#endif // __FORGE_RENDERTEXTURE_H__
