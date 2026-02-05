#include "rendertexture.h"
#include "renderer.h"
#include <stdlib.h>
#include <stdio.h>

static GLint g_default_framebuffer = 0;
static GLint g_default_viewport[4];

RenderTexture LoadRenderTexture(int width, int height)
{
    RenderTexture rt = {0};
    rt.width = width;
    rt.height = height;

    glGenTextures(1, &rt.texture.id);
    glBindTexture(GL_TEXTURE_2D, rt.texture.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    rt.texture.width = width;
    rt.texture.height = height;

    glGenFramebuffers(1, &rt.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, rt.framebuffer);
    
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.texture.id, 0);

    glGenRenderbuffers(1, &rt.renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, rt.renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt.renderbuffer);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        dbg_msg("RenderTexture", "Framebuffer not complete: 0x%X", status);
        UnloadRenderTexture(rt);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    dbg_msg("RenderTexture", "RenderTexture created: %dx%d", width, height);

    return rt;
}

void UnloadRenderTexture(RenderTexture rt)
{
    glDeleteFramebuffers(1, &rt.framebuffer);
    glDeleteRenderbuffers(1, &rt.renderbuffer);
    glDeleteTextures(1, &rt.texture.id);

    dbg_msg("RenderTexture", "RenderTexture unloaded");
}

void BeginTextureMode(RenderTexture rt)
{
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &g_default_framebuffer);
    glGetIntegerv(GL_VIEWPORT, g_default_viewport);

    glBindFramebuffer(GL_FRAMEBUFFER, rt.framebuffer);
    glViewport(0, 0, rt.width, rt.height);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void EndTextureMode(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, g_default_framebuffer);
    glViewport(g_default_viewport[0], g_default_viewport[1], g_default_viewport[2], g_default_viewport[3]);
}
