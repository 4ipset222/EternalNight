#include "renderer.h"
#include "forgesystem.h"
#include "renderer_d2d.h"
#include <stdlib.h>
#include <GL/glew.h>
#include <png.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <ft2build.h>
#include FT_FREETYPE_H

Renderer* g_renderer = NULL;

static FT_Library g_ft_lib = NULL;
static bool g_ft_inited = false;
static Font* g_default_font = NULL;
static bool g_default_font_attempted = false;

static int IsD2DBackend(const Renderer* r)
{
    return r && r->backend == RENDERER_BACKEND_DIRECT2D && r->backend_ctx != NULL;
}

static Vec2 TransformPointForBackend(Vec2 p)
{
    if (!g_renderer || !g_renderer->camera_mode)
    {
        return p;
    }

    float zoom = g_renderer->camera_zoom;
    Vec2 out = {
        (p.x - g_renderer->camera_x) * zoom,
        (p.y - g_renderer->camera_y) * zoom
    };
    return out;
}

static Rect TransformRectForBackend(Rect r)
{
    if (!g_renderer || !g_renderer->camera_mode)
    {
        return r;
    }

    float zoom = g_renderer->camera_zoom;
    Rect out = {
        (r.x - g_renderer->camera_x) * zoom,
        (r.y - g_renderer->camera_y) * zoom,
        r.width * zoom,
        r.height * zoom
    };
    return out;
}

static float TransformLengthForBackend(float v)
{
    if (!g_renderer || !g_renderer->camera_mode)
    {
        return v;
    }
    return v * g_renderer->camera_zoom;
}

typedef struct D2DTextureCacheEntry
{
    char path[260];
    void* bitmap;
} D2DTextureCacheEntry;

static D2DTextureCacheEntry g_d2d_tex_cache[256];
static int g_d2d_tex_cache_count = 0;

static void ClearD2DTextureCache(void)
{
    for (int i = 0; i < g_d2d_tex_cache_count; ++i)
    {
        if (g_d2d_tex_cache[i].bitmap)
        {
            D2DRenderer_FreeBitmap(g_d2d_tex_cache[i].bitmap);
            g_d2d_tex_cache[i].bitmap = NULL;
        }
    }
    g_d2d_tex_cache_count = 0;
}

static void* GetOrLoadD2DBitmap(const Texture2D* texture)
{
    if (!texture || !IsD2DBackend(g_renderer) || texture->path[0] == '\0')
    {
        return NULL;
    }

    for (int i = 0; i < g_d2d_tex_cache_count; ++i)
    {
        if (strcmp(g_d2d_tex_cache[i].path, texture->path) == 0)
        {
            return g_d2d_tex_cache[i].bitmap;
        }
    }

    int w = 0;
    int h = 0;
    void* bmp = D2DRenderer_LoadBitmap((D2DRenderer*)g_renderer->backend_ctx, texture->path, &w, &h);
    if (!bmp)
    {
        return NULL;
    }

    if (g_d2d_tex_cache_count < (int)(sizeof(g_d2d_tex_cache) / sizeof(g_d2d_tex_cache[0])))
    {
        D2DTextureCacheEntry* e = &g_d2d_tex_cache[g_d2d_tex_cache_count++];
        strncpy(e->path, texture->path, sizeof(e->path) - 1);
        e->path[sizeof(e->path) - 1] = '\0';
        e->bitmap = bmp;
    }

    return bmp;
}

void SetGlobalRenderer(Renderer* r)
{
    g_renderer = r;
}

Renderer* GetGlobalRenderer(void)
{
    return g_renderer;
}

static GLuint CompileShader(const char* source, GLenum type)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint success;
    GLchar info_log[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        dbg_msg("Renderer", "Shader compilation failed: %s", info_log);
    }
    
    return shader;
}

static Shader CreateShader(const char* vert_src, const char* frag_src)
{
    GLuint vert = CompileShader(vert_src, GL_VERTEX_SHADER);
    GLuint frag = CompileShader(frag_src, GL_FRAGMENT_SHADER);
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    
    GLint success;
    GLchar info_log[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 512, NULL, info_log);
        dbg_msg("Renderer", "Shader linking failed: %s", info_log);
    }
    
    glDeleteShader(vert);
    glDeleteShader(frag);
    
    Shader shader;
    shader.id = program;
    return shader;
}

const char* PRIMITIVE_VERT = 
    "#version 460 core\n"
    "layout(location = 0) in vec2 position;\n"
    "layout(location = 1) in vec4 color;\n"
    "out VS_OUT {\n"
    "    vec4 color;\n"
    "} vs_out;\n"
    "uniform mat4 projection;\n"
    "void main() {\n"
    "    gl_Position = projection * vec4(position, 0.0, 1.0);\n"
    "    vs_out.color = color;\n"
    "}\n";

const char* PRIMITIVE_FRAG = 
    "#version 460 core\n"
    "in VS_OUT {\n"
    "    vec4 color;\n"
    "} fs_in;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    FragColor = fs_in.color;\n"
    "}\n";

const char* TEXTURE_VERT = 
    "#version 460 core\n"
    "layout(location = 0) in vec2 position;\n"
    "layout(location = 1) in vec2 texCoord;\n"
    "layout(location = 2) in vec4 color;\n"
    "out VS_OUT {\n"
    "    vec2 texCoord;\n"
    "    vec4 color;\n"
    "} vs_out;\n"
    "uniform mat4 projection;\n"
    "uniform mat4 transform;\n"
    "void main() {\n"
    "    gl_Position = projection * transform * vec4(position, 0.0, 1.0);\n"
    "    vs_out.texCoord = texCoord;\n"
    "    vs_out.color = color;\n"
    "}\n";

const char* TEXTURE_FRAG = 
    "#version 460 core\n"
    "in VS_OUT {\n"
    "    vec2 texCoord;\n"
    "    vec4 color;\n"
    "} fs_in;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D tex_sampler;\n"
    "void main() {\n"
    "    vec4 tex_color = texture(tex_sampler, fs_in.texCoord);\n"
    "    FragColor = tex_color * fs_in.color;\n"
    "}\n";

const char* TEXT_VERT =
    "#version 460 core\n"
    "layout(location = 0) in vec2 position;\n"
    "layout(location = 1) in vec2 texCoord;\n"
    "layout(location = 2) in vec4 color;\n"
    "out VS_OUT {\n"
    "    vec2 texCoord;\n"
    "    vec4 color;\n"
    "} vs_out;\n"
    "uniform mat4 projection;\n"
    "uniform mat4 transform;\n"
    "void main() {\n"
    "    gl_Position = projection * transform * vec4(position, 0.0, 1.0);\n"
    "    vs_out.texCoord = texCoord;\n"
    "    vs_out.color = color;\n"
    "}\n";

const char* TEXT_FRAG =
    "#version 460 core\n"
    "in VS_OUT {\n"
    "    vec2 texCoord;\n"
    "    vec4 color;\n"
    "} fs_in;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D tex_sampler;\n"
    "void main() {\n"
    "    float a = texture(tex_sampler, fs_in.texCoord).a;\n"
    "    FragColor = vec4(fs_in.color.rgb, fs_in.color.a * a);\n"
    "}\n";

static void OrthoMatrix(float left, float right, float bottom, float top, float* out)
{
    memset(out, 0, 16 * sizeof(float));
    out[0] = 2.0f / (right - left);
    out[5] = 2.0f / (top - bottom);
    out[10] = -1.0f;
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[15] = 1.0f;
}

static void IdentityMatrix(float* out)
{
    memset(out, 0, 16 * sizeof(float));
    out[0] = out[5] = out[10] = out[15] = 1.0f;
}

Texture2D* LoadTexture(const char* path)
{
    FILE* fp = fopen(path, "rb");
    if (fp == NULL)
    {
        dbg_msg("Renderer", "Failed to open PNG file: %s", path);
        return NULL;
    }

    png_byte header[8];
    fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8))
    {
        dbg_msg("Renderer", "File is not recognized as PNG: %s", path);
        fclose(fp);
        return NULL;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        return NULL;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return NULL;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    png_uint_32 width  = png_get_image_width(png_ptr, info_ptr);
    png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth  = png_get_bit_depth(png_ptr, info_ptr);

    if (bit_depth == 16)
    {
        png_set_strip_16(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_PALETTE)
    {
        png_set_palette_to_rgb(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    {
        png_set_tRNS_to_alpha(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
    {
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    {
        png_set_gray_to_rgb(png_ptr);
    }

    png_read_update_info(png_ptr, info_ptr);

    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for (png_uint_32 y = 0; y < height; ++y)
    {
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png_ptr, info_ptr));
    }

    png_read_image(png_ptr, row_pointers);
    fclose(fp);

    png_byte* data = (png_byte*)malloc(width * height * 4);
    for (png_uint_32 y = 0; y < height; ++y)
    {
        memcpy(data + y * width * 4, row_pointers[y], width * 4);
        free(row_pointers[y]);
    }
    free(row_pointers);

    GLuint tex_id = 0;
    if (!g_renderer || Renderer_GetBackend(g_renderer) == RENDERER_BACKEND_OPENGL)
    {
        glGenTextures(1, &tex_id);
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, data);
    }
    free(data);

    Texture2D* texture = (Texture2D*)malloc(sizeof(Texture2D));
    texture->id = tex_id;
    texture->width  = width;
    texture->height = height;
    texture->native_handle = NULL;
    texture->path[0] = '\0';
    if (path)
    {
        strncpy(texture->path, path, sizeof(texture->path) - 1);
        texture->path[sizeof(texture->path) - 1] = '\0';
    }

    return texture;
}

void UnloadTexture(Texture2D* texture)
{
    if (texture)
    {
        if (texture->id != 0)
        {
            glDeleteTextures(1, &texture->id);
        }
        free(texture);
    }
}

Renderer* Renderer_Create(Window* window)
{
    Renderer* r = malloc(sizeof(Renderer));
    r->window = window;
    r->width  = window->width;
    r->height = window->height;
    r->backend = RENDERER_BACKEND_OPENGL;
    r->backend_ctx = NULL;
    r->camera_mode = 0;
    r->camera_x = 0.0f;
    r->camera_y = 0.0f;
    r->camera_zoom = 1.0f;

    glViewport(0, 0, r->width, r->height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    r->primitive_shader = CreateShader(PRIMITIVE_VERT, PRIMITIVE_FRAG);
    r->texture_shader = CreateShader(TEXTURE_VERT, TEXTURE_FRAG);
    r->text_shader = CreateShader(TEXT_VERT, TEXT_FRAG);

    float left = 0.0f;
    float right = (float)r->width;
    float bottom = (float)r->height;
    float top = 0.0f;
    
    float projection[16] =
    {
        2.0f / (right - left), 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -(right + left) / (right - left), -(top + bottom) / (top - bottom), 0.0f, 1.0f
    };

    glUseProgram(r->primitive_shader.id);
    r->proj_uniform = glGetUniformLocation(r->primitive_shader.id, "projection");
    glUniformMatrix4fv(r->proj_uniform, 1, GL_FALSE, projection);
    
    glUseProgram(r->texture_shader.id);
    r->proj_uniform = glGetUniformLocation(r->texture_shader.id, "projection");
    glUniformMatrix4fv(r->proj_uniform, 1, GL_FALSE, projection);
    r->transform_uniform = glGetUniformLocation(r->texture_shader.id, "transform");

    glUseProgram(r->text_shader.id);
    r->text_proj_uniform = glGetUniformLocation(r->text_shader.id, "projection");
    glUniformMatrix4fv(r->text_proj_uniform, 1, GL_FALSE, projection);
    r->text_transform_uniform = glGetUniformLocation(r->text_shader.id, "transform");
    r->text_sampler_uniform = glGetUniformLocation(r->text_shader.id, "tex_sampler");

    glGenVertexArrays(1, &r->primitive_vao);
    glGenBuffers(1, &r->primitive_vbo);
    glGenBuffers(1, &r->primitive_color_vbo);
    glGenBuffers(1, &r->primitive_ibo);

    glGenVertexArrays(1, &r->texture_vao);
    glGenBuffers(1, &r->texture_vbo);
    glGenBuffers(1, &r->texture_ibo);

    glGenVertexArrays(1, &r->text_vao);
    glGenBuffers(1, &r->text_vbo);

    glBindVertexArray(r->primitive_vao);

    glBindBuffer(GL_ARRAY_BUFFER, r->primitive_vbo);
    glBufferData(GL_ARRAY_BUFFER, 4 * 2 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, r->primitive_color_vbo);
    glBufferData(GL_ARRAY_BUFFER, 4 * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

    unsigned int quad_indices[] = { 0, 1, 2, 2, 3, 0 };
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->primitive_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_indices), quad_indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    glBindVertexArray(r->text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->text_vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    dbg_msg("Renderer", "Renderer created successfully");

    return r;
}

int Renderer_SetBackend(Renderer* r, RendererBackend backend)
{
    if (!r)
    {
        return 0;
    }

    if ((RendererBackend)r->backend == backend)
    {
        return 1;
    }

    if (backend == RENDERER_BACKEND_DIRECT2D)
    {
        ClearD2DTextureCache();
        D2DRenderer* d2d = D2DRenderer_Create(r->window->handle, r->width, r->height);
        if (!d2d)
        {
            dbg_msg("Renderer", "Failed to create Direct2D backend");
            return 0;
        }
        if (r->backend_ctx)
        {
            D2DRenderer_Destroy((D2DRenderer*)r->backend_ctx);
        }
        r->backend_ctx = d2d;
        r->backend = RENDERER_BACKEND_DIRECT2D;
        dbg_msg("Renderer", "Switched backend to Direct2D");
        return 1;
    }

    if (backend == RENDERER_BACKEND_OPENGL)
    {
        if (r->backend_ctx)
        {
            D2DRenderer_Destroy((D2DRenderer*)r->backend_ctx);
            ClearD2DTextureCache();
            r->backend_ctx = NULL;
        }
        r->backend = RENDERER_BACKEND_OPENGL;
        dbg_msg("Renderer", "Switched backend to OpenGL");
        return 1;
    }

    return 0;
}

RendererBackend Renderer_GetBackend(const Renderer* r)
{
    if (!r)
    {
        return RENDERER_BACKEND_OPENGL;
    }
    return (RendererBackend)r->backend;
}

const char* Renderer_GetBackendName(RendererBackend backend)
{
    switch (backend)
    {
        case RENDERER_BACKEND_DIRECT2D: return "Direct2D";
        case RENDERER_BACKEND_OPENGL:
        default: return "OpenGL";
    }
}

void Renderer_Destroy(Renderer* r)
{
    if (!r)
    {
        return;
    }

    if (r->backend_ctx)
    {
        D2DRenderer_Destroy((D2DRenderer*)r->backend_ctx);
        ClearD2DTextureCache();
        r->backend_ctx = NULL;
    }

    if (g_default_font)
    {
        UnloadFont(g_default_font);
        g_default_font = NULL;
    }

    glDeleteProgram(r->primitive_shader.id);
    glDeleteProgram(r->texture_shader.id);
    glDeleteProgram(r->text_shader.id);
    
    glDeleteBuffers(1, &r->primitive_vbo);
    glDeleteBuffers(1, &r->primitive_color_vbo);
    glDeleteBuffers(1, &r->primitive_ibo);
    glDeleteVertexArrays(1, &r->primitive_vao);
    
    glDeleteBuffers(1, &r->texture_vbo);
    glDeleteBuffers(1, &r->texture_ibo);
    glDeleteVertexArrays(1, &r->texture_vao);

    glDeleteBuffers(1, &r->text_vbo);
    glDeleteVertexArrays(1, &r->text_vao);
    
    dbg_msg("Renderer", "Renderer destroyed");

    free(r);
}

void Renderer_BeginFrame(Renderer* r)
{
    if (IsD2DBackend(r))
    {
        D2DRenderer_BeginFrame((D2DRenderer*)r->backend_ctx);
        D2DRenderer_Clear((D2DRenderer*)r->backend_ctx, (Color){0.0f, 0.0f, 0.0f, 1.0f});
        return;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer_EndFrame(Renderer* r)
{
    if (IsD2DBackend(r))
    {
        D2DRenderer_EndFrame((D2DRenderer*)r->backend_ctx);
        return;
    }
    SDL_GL_SwapWindow(r->window->handle);
}

void Renderer_Clear(Color color)
{
    if (IsD2DBackend(g_renderer))
    {
        D2DRenderer_Clear((D2DRenderer*)g_renderer->backend_ctx, color);
        return;
    }
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer_DrawRectangle(Rect rect, Color color)
{
    if (IsD2DBackend(g_renderer))
    {
        Rect r = TransformRectForBackend(rect);
        D2DRenderer_DrawRectangle((D2DRenderer*)g_renderer->backend_ctx, r, color);
        return;
    }

    float vertices[] =
    {
        rect.x, rect.y,
        rect.x + rect.width, rect.y,
        rect.x + rect.width, rect.y + rect.height,
        rect.x, rect.y + rect.height
    };
    float colors[] =
    {
        color.r, color.g, color.b, color.a,
        color.r, color.g, color.b, color.a,
        color.r, color.g, color.b, color.a,
        color.r, color.g, color.b, color.a
    };
    if (!g_renderer) return;
    glBindVertexArray(g_renderer->primitive_vao);

    glBindBuffer(GL_ARRAY_BUFFER, g_renderer->primitive_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    glBindBuffer(GL_ARRAY_BUFFER, g_renderer->primitive_color_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(colors), colors);

    glUseProgram(g_renderer->primitive_shader.id);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Renderer_DrawTrianglesColored(const float* positions, const float* colors, int vertexCount)
{
    if (IsD2DBackend(g_renderer))
    {
        if (!positions || !colors || vertexCount <= 0) return;
        float* transformed = (float*)malloc((size_t)vertexCount * 2 * sizeof(float));
        if (!transformed) return;
        for (int i = 0; i < vertexCount; ++i)
        {
            Vec2 p = { positions[i * 2 + 0], positions[i * 2 + 1] };
            p = TransformPointForBackend(p);
            transformed[i * 2 + 0] = p.x;
            transformed[i * 2 + 1] = p.y;
        }
        D2DRenderer_DrawTrianglesColored((D2DRenderer*)g_renderer->backend_ctx, transformed, colors, vertexCount);
        free(transformed);
        return;
    }

    if (!g_renderer) return;
    if (!positions || !colors || vertexCount <= 0) return;

    glBindVertexArray(g_renderer->primitive_vao);

    glBindBuffer(GL_ARRAY_BUFFER, g_renderer->primitive_vbo);
    glBufferData(GL_ARRAY_BUFFER, (size_t)vertexCount * 2 * sizeof(float), positions, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, g_renderer->primitive_color_vbo);
    glBufferData(GL_ARRAY_BUFFER, (size_t)vertexCount * 4 * sizeof(float), colors, GL_DYNAMIC_DRAW);

    glUseProgram(g_renderer->primitive_shader.id);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);

    glBindVertexArray(0);
}

void Renderer_DrawRectangleLines(Rect rect, int line_thick, Color color)
{
    if (IsD2DBackend(g_renderer))
    {
        Rect r = TransformRectForBackend(rect);
        float thick = TransformLengthForBackend((float)line_thick);
        D2DRenderer_DrawRectangleLines((D2DRenderer*)g_renderer->backend_ctx, r, thick, color);
        return;
    }

    float vertices[] =
    {
        rect.x, rect.y,
        rect.x + rect.width, rect.y,
        rect.x + rect.width, rect.y + rect.height,
        rect.x, rect.y + rect.height
    };
    float colors[] =
    {
        color.r, color.g, color.b, color.a,
        color.r, color.g, color.b, color.a,
        color.r, color.g, color.b, color.a,
        color.r, color.g, color.b, color.a
    };
    unsigned int indices[] = { 0, 1, 1, 2, 2, 3, 3, 0 };
    GLuint vao, vbo, vbo_color, ibo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &vbo_color);
    glGenBuffers(1, &ibo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_color);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glUseProgram(g_renderer->primitive_shader.id);
    glLineWidth((float)line_thick);
    glDrawElements(GL_LINES, 8, GL_UNSIGNED_INT, 0);
    glLineWidth(1.0f);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &vbo_color);
    glDeleteBuffers(1, &ibo);
    glDeleteVertexArrays(1, &vao);
}

void Renderer_DrawCircle(Vec2 center, float radius, Color color)
{
    if (IsD2DBackend(g_renderer))
    {
        Vec2 c = TransformPointForBackend(center);
        float r = TransformLengthForBackend(radius);
        D2DRenderer_DrawCircle((D2DRenderer*)g_renderer->backend_ctx, c, r, color, 1);
        return;
    }

    const int segments = 32;
    float* vertices = (float*)malloc((segments + 2) * 2 * sizeof(float));
    float* colors = (float*)malloc((segments + 2) * 4 * sizeof(float));
    unsigned int* indices = (unsigned int*)malloc(segments * 3 * sizeof(unsigned int));
    
    vertices[0] = center.x;
    vertices[1] = center.y;
    colors[0] = color.r;
    colors[1] = color.g;
    colors[2] = color.b;
    colors[3] = color.a;
    
    for (int i = 0; i <= segments; i++)
    {
        float angle = (2.0f * 3.14159f * i) / segments;
        vertices[(i + 1) * 2] = center.x + radius * cosf(angle);
        vertices[(i + 1) * 2 + 1] = center.y + radius * sinf(angle);
        colors[(i + 1) * 4] = color.r;
        colors[(i + 1) * 4 + 1] = color.g;
        colors[(i + 1) * 4 + 2] = color.b;
        colors[(i + 1) * 4 + 3] = color.a;
    }
    
    for (int i = 0; i < segments; i++)
    {
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = i + 2;
    }
    
    GLuint vao, vbo, vbo_color, ibo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &vbo_color);
    glGenBuffers(1, &ibo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (segments + 2) * 2 * sizeof(float), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_color);
    glBufferData(GL_ARRAY_BUFFER, (segments + 2) * 4 * sizeof(float), colors, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, segments * 3 * sizeof(unsigned int), indices, GL_STATIC_DRAW);
    glUseProgram(g_renderer->primitive_shader.id);
    glDrawElements(GL_TRIANGLES, segments * 3, GL_UNSIGNED_INT, 0);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &vbo_color);
    glDeleteBuffers(1, &ibo);
    glDeleteVertexArrays(1, &vao);
    
    free(vertices);
    free(colors);
    free(indices);
}

void Renderer_DrawCircleLines(Vec2 center, float radius, Color color)
{
    if (IsD2DBackend(g_renderer))
    {
        Vec2 c = TransformPointForBackend(center);
        float r = TransformLengthForBackend(radius);
        D2DRenderer_DrawCircle((D2DRenderer*)g_renderer->backend_ctx, c, r, color, 0);
        return;
    }

    const int segments = 32;
    float* vertices = (float*)malloc(segments * 2 * sizeof(float));
    float* colors = (float*)malloc(segments * 4 * sizeof(float));
    unsigned int* indices = (unsigned int*)malloc(segments * 2 * sizeof(unsigned int));
    
    for (int i = 0; i < segments; i++)
    {
        float angle = (2.0f * 3.14159f * i) / segments;
        vertices[i * 2] = center.x + radius * cosf(angle);
        vertices[i * 2 + 1] = center.y + radius * sinf(angle);
        colors[i * 4] = color.r;
        colors[i * 4 + 1] = color.g;
        colors[i * 4 + 2] = color.b;
        colors[i * 4 + 3] = color.a;
        indices[i * 2] = i;
        indices[i * 2 + 1] = (i + 1) % segments;
    }
    
    GLuint vao, vbo, vbo_color, ibo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &vbo_color);
    glGenBuffers(1, &ibo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, segments * 2 * sizeof(float), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_color);
    glBufferData(GL_ARRAY_BUFFER, segments * 4 * sizeof(float), colors, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, segments * 2 * sizeof(unsigned int), indices, GL_STATIC_DRAW);
    glUseProgram(g_renderer->primitive_shader.id);
    glLineWidth(1.0f);
    glDrawElements(GL_LINES, segments * 2, GL_UNSIGNED_INT, 0);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &vbo_color);
    glDeleteBuffers(1, &ibo);
    glDeleteVertexArrays(1, &vao);
    
    free(vertices);
    free(colors);
    free(indices);
}

void Renderer_DrawLine(Vec2 start, Vec2 end, Color color)
{
    if (IsD2DBackend(g_renderer))
    {
        Vec2 s = TransformPointForBackend(start);
        Vec2 e = TransformPointForBackend(end);
        D2DRenderer_DrawLine((D2DRenderer*)g_renderer->backend_ctx, s, e, TransformLengthForBackend(1.0f), color);
        return;
    }

    float vertices[] =
    {
        start.x, start.y,
        end.x, end.y
    };
    float colors[] =
    {
        color.r, color.g, color.b, color.a,
        color.r, color.g, color.b, color.a
    };
    if (!g_renderer) return;
    glBindVertexArray(g_renderer->primitive_vao);

    glBindBuffer(GL_ARRAY_BUFFER, g_renderer->primitive_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    glBindBuffer(GL_ARRAY_BUFFER, g_renderer->primitive_color_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(colors), colors);

    glUseProgram(g_renderer->primitive_shader.id);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
}

void Renderer_DrawLineEx(Vec2 start, Vec2 end, float thick, Color color)
{
    if (IsD2DBackend(g_renderer))
    {
        Vec2 s = TransformPointForBackend(start);
        Vec2 e = TransformPointForBackend(end);
        D2DRenderer_DrawLine((D2DRenderer*)g_renderer->backend_ctx, s, e, TransformLengthForBackend(thick), color);
        return;
    }

    float vertices[] =
    {
        start.x, start.y,
        end.x, end.y
    };
    float colors[] =
    {
        color.r, color.g, color.b, color.a,
        color.r, color.g, color.b, color.a
    };
    if (!g_renderer) return;
    glBindVertexArray(g_renderer->primitive_vao);

    glBindBuffer(GL_ARRAY_BUFFER, g_renderer->primitive_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    glBindBuffer(GL_ARRAY_BUFFER, g_renderer->primitive_color_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(colors), colors);

    glUseProgram(g_renderer->primitive_shader.id);
    glLineWidth(thick);
    glDrawArrays(GL_LINES, 0, 2);
    glLineWidth(1.0f);
    glBindVertexArray(0);
}

void Renderer_DrawTriangle(Vec2 v1, Vec2 v2, Vec2 v3, Color color)
{
    if (IsD2DBackend(g_renderer))
    {
        Vec2 a = TransformPointForBackend(v1);
        Vec2 b = TransformPointForBackend(v2);
        Vec2 c = TransformPointForBackend(v3);
        D2DRenderer_DrawTriangle((D2DRenderer*)g_renderer->backend_ctx, a, b, c, color, 1);
        return;
    }

    float vertices[] =
    {
        v1.x, v1.y,
        v2.x, v2.y,
        v3.x, v3.y
    };
    float colors[] =
    {
        color.r, color.g, color.b, color.a,
        color.r, color.g, color.b, color.a,
        color.r, color.g, color.b, color.a
    };
    GLuint vao, vbo, vbo_color;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &vbo_color);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_color);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glUseProgram(g_renderer->primitive_shader.id);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &vbo_color);
    glDeleteVertexArrays(1, &vao);
}

void Renderer_DrawTriangleLines(Vec2 v1, Vec2 v2, Vec2 v3, Color color)
{
    if (IsD2DBackend(g_renderer))
    {
        Vec2 a = TransformPointForBackend(v1);
        Vec2 b = TransformPointForBackend(v2);
        Vec2 c = TransformPointForBackend(v3);
        D2DRenderer_DrawTriangle((D2DRenderer*)g_renderer->backend_ctx, a, b, c, color, 0);
        return;
    }

    float vertices[] =
    {
        v1.x, v1.y,
        v2.x, v2.y,
        v3.x, v3.y
    };
    float colors[] =
    {
        color.r, color.g, color.b, color.a,
        color.r, color.g, color.b, color.a,
        color.r, color.g, color.b, color.a
    };
    unsigned int indices[] = { 0, 1, 1, 2, 2, 0 };
    GLuint vao, vbo, vbo_color, ibo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &vbo_color);
    glGenBuffers(1, &ibo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_color);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glUseProgram(g_renderer->primitive_shader.id);
    glDrawElements(GL_LINES, 6, GL_UNSIGNED_INT, 0);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &vbo_color);
    glDeleteBuffers(1, &ibo);
    glDeleteVertexArrays(1, &vao);
}

void Renderer_DrawTexture(Texture2D texture, Vec2 position, Color tint)
{
    if (IsD2DBackend(g_renderer))
    {
        Rect src = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
        Rect dst = { position.x, position.y, (float)texture.width, (float)texture.height };
        Rect dst_t = TransformRectForBackend(dst);
        void* bmp = GetOrLoadD2DBitmap(&texture);
        if (bmp)
        {
            D2DRenderer_DrawBitmap((D2DRenderer*)g_renderer->backend_ctx, bmp, src, dst_t, (Vec2){0.0f, 0.0f}, 0.0f, tint);
        }
        else
        {
            Renderer_DrawRectangle(dst, tint);
        }
        return;
    }

    float vertices[] =
    {
        position.x, position.y,
        position.x + texture.width, position.y,
        position.x + texture.width, position.y + texture.height,
        position.x, position.y + texture.height
    };
    
    float texcoords[] =
    {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };
    
    float colors[] =
    {
        tint.r, tint.g, tint.b, tint.a,
        tint.r, tint.g, tint.b, tint.a,
        tint.r, tint.g, tint.b, tint.a,
        tint.r, tint.g, tint.b, tint.a
    };
    
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };
    
    GLuint vao, vbo_pos, vbo_tex, vbo_col, ibo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo_pos);
    glGenBuffers(1, &vbo_tex);
    glGenBuffers(1, &vbo_col);
    glGenBuffers(1, &ibo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_tex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_col);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glUseProgram(g_renderer->texture_shader.id);
    GLint tex_uniform = glGetUniformLocation(g_renderer->texture_shader.id, "tex_sampler");
    glUniform1i(tex_uniform, 0);
    float identity[16] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    GLint transform_uniform = glGetUniformLocation(g_renderer->texture_shader.id, "transform");
    glUniformMatrix4fv(transform_uniform, 1, GL_FALSE, identity);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo_pos);
    glDeleteBuffers(1, &vbo_tex);
    glDeleteBuffers(1, &vbo_col);
    glDeleteBuffers(1, &ibo);
}

void Renderer_DrawTextureRec(Texture2D texture, Rect source, Vec2 position, Color tint)
{
    if (IsD2DBackend(g_renderer))
    {
        Rect dst = { position.x, position.y, source.width, source.height };
        Rect dst_t = TransformRectForBackend(dst);
        void* bmp = GetOrLoadD2DBitmap(&texture);
        if (bmp)
        {
            D2DRenderer_DrawBitmap((D2DRenderer*)g_renderer->backend_ctx, bmp, source, dst_t, (Vec2){0.0f, 0.0f}, 0.0f, tint);
        }
        else
        {
            Renderer_DrawRectangle(dst, tint);
        }
        return;
    }

    if (!g_renderer) return;
    
    float u1 = source.x / texture.width;
    float v1 = source.y / texture.height;
    float u2 = (source.x + source.width) / texture.width;
    float v2 = (source.y + source.height) / texture.height;
    
    float vertices[] =
    {
        position.x, position.y,
        position.x + source.width, position.y,
        position.x + source.width, position.y + source.height,
        position.x, position.y + source.height
    };
    
    float texcoords[] =
    {
        u1, v1,
        u2, v1,
        u2, v2,
        u1, v2
    };
    
    float colors[] =
    {
        tint.r, tint.g, tint.b, tint.a,
        tint.r, tint.g, tint.b, tint.a,
        tint.r, tint.g, tint.b, tint.a,
        tint.r, tint.g, tint.b, tint.a
    };
    
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };
    
    GLuint vao, vbo_pos, vbo_tex, vbo_col, ibo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo_pos);
    glGenBuffers(1, &vbo_tex);
    glGenBuffers(1, &vbo_col);
    glGenBuffers(1, &ibo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_tex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_col);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glUseProgram(g_renderer->texture_shader.id);
    GLint tex_uniform = glGetUniformLocation(g_renderer->texture_shader.id, "tex_sampler");
    glUniform1i(tex_uniform, 0);
    float identity[16] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    GLint transform_uniform = glGetUniformLocation(g_renderer->texture_shader.id, "transform");
    glUniformMatrix4fv(transform_uniform, 1, GL_FALSE, identity);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo_pos);
    glDeleteBuffers(1, &vbo_tex);
    glDeleteBuffers(1, &vbo_col);
    glDeleteBuffers(1, &ibo);
}

void Renderer_DrawTextureEx(Texture2D texture, Vec2 position, float rotation, float scale, Color tint)
{
    if (IsD2DBackend(g_renderer))
    {
        Rect src = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
        Rect dst = { position.x, position.y, (float)texture.width * scale, (float)texture.height * scale };
        Rect dst_t = TransformRectForBackend(dst);
        Vec2 origin = { dst_t.width * 0.5f, dst_t.height * 0.5f };
        void* bmp = GetOrLoadD2DBitmap(&texture);
        if (bmp)
        {
            D2DRenderer_DrawBitmap((D2DRenderer*)g_renderer->backend_ctx, bmp, src, dst_t, origin, rotation, tint);
        }
        else
        {
            Renderer_DrawRectangle(dst, tint);
        }
        return;
    }

    if (!g_renderer) return;
    
    float width = texture.width * scale;
    float height = texture.height * scale;
    
    float vertices[] =
    {
        position.x, position.y,
        position.x + width, position.y,
        position.x + width, position.y + height,
        position.x, position.y + height
    };
    
    float texcoords[] =
    {
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };
    
    float colors[] =
    {
        tint.r, tint.g, tint.b, tint.a,
        tint.r, tint.g, tint.b, tint.a,
        tint.r, tint.g, tint.b, tint.a,
        tint.r, tint.g, tint.b, tint.a
    };
    
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };
    
    GLuint vao, vbo_pos, vbo_tex, vbo_col, ibo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo_pos);
    glGenBuffers(1, &vbo_tex);
    glGenBuffers(1, &vbo_col);
    glGenBuffers(1, &ibo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_tex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_col);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glUseProgram(g_renderer->texture_shader.id);
    GLint tex_uniform = glGetUniformLocation(g_renderer->texture_shader.id, "tex_sampler");
    glUniform1i(tex_uniform, 0);
    float transform[16] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    if (rotation != 0.0f)
    {
        float cos_a = cosf(rotation);
        float sin_a = sinf(rotation);
        float cx = position.x + width / 2.0f;
        float cy = position.y + height / 2.0f;
        
        transform[0] = cos_a;
        transform[1] = sin_a;
        transform[4] = -sin_a;
        transform[5] = cos_a;
        transform[12] = cx - (cos_a * cx - sin_a * cy);
        transform[13] = cy - (sin_a * cx + cos_a * cy);
    }
    GLint transform_uniform = glGetUniformLocation(g_renderer->texture_shader.id, "transform");
    glUniformMatrix4fv(transform_uniform, 1, GL_FALSE, transform);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo_pos);
    glDeleteBuffers(1, &vbo_tex);
    glDeleteBuffers(1, &vbo_col);
    glDeleteBuffers(1, &ibo);
}

void Renderer_DrawTexturePro(Texture2D texture, Rect source, Rect dest, Vec2 origin, float rotation, Color tint)
{
    if (IsD2DBackend(g_renderer))
    {
        Rect dst_t = TransformRectForBackend(dest);
        Vec2 o_t = { TransformLengthForBackend(origin.x), TransformLengthForBackend(origin.y) };
        void* bmp = GetOrLoadD2DBitmap(&texture);
        if (bmp)
        {
            D2DRenderer_DrawBitmap((D2DRenderer*)g_renderer->backend_ctx, bmp, source, dst_t, o_t, rotation, tint);
        }
        else
        {
            Renderer_DrawRectangle(dest, tint);
        }
        return;
    }

    if (!g_renderer) return;
    
    float u1 = source.x / texture.width;
    float v1 = source.y / texture.height;
    float u2 = (source.x + source.width) / texture.width;
    float v2 = (source.y + source.height) / texture.height;
    
    float vertices[] =
    {
        dest.x, dest.y,
        dest.x + dest.width, dest.y,
        dest.x + dest.width, dest.y + dest.height,
        dest.x, dest.y + dest.height
    };
    
    float texcoords[] =
    {
        u1, v1,
        u2, v1,
        u2, v2,
        u1, v2
    };
    
    float colors[] =
    {
        tint.r, tint.g, tint.b, tint.a,
        tint.r, tint.g, tint.b, tint.a,
        tint.r, tint.g, tint.b, tint.a,
        tint.r, tint.g, tint.b, tint.a
    };
    
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };
    
    GLuint vao, vbo_pos, vbo_tex, vbo_col, ibo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo_pos);
    glGenBuffers(1, &vbo_tex);
    glGenBuffers(1, &vbo_col);
    glGenBuffers(1, &ibo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_tex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_col);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glUseProgram(g_renderer->texture_shader.id);
    GLint tex_uniform = glGetUniformLocation(g_renderer->texture_shader.id, "tex_sampler");
    glUniform1i(tex_uniform, 0);
    float transform[16] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    if (rotation != 0.0f)
    {
        float cos_a = cosf(rotation);
        float sin_a = sinf(rotation);
        float cx = dest.x + origin.x;
        float cy = dest.y + origin.y;
        transform[0] = cos_a;
        transform[1] = sin_a;
        transform[4] = -sin_a;
        transform[5] = cos_a;
        transform[12] = cx - (cos_a * cx - sin_a * cy);
        transform[13] = cy - (sin_a * cx + cos_a * cy);
    }
    else if (origin.x != 0.0f || origin.y != 0.0f)
    {
        transform[12] = -origin.x;
        transform[13] = -origin.y;
    }
    GLint transform_uniform = glGetUniformLocation(g_renderer->texture_shader.id, "transform");
    glUniformMatrix4fv(transform_uniform, 1, GL_FALSE, transform);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo_pos);
    glDeleteBuffers(1, &vbo_tex);
    glDeleteBuffers(1, &vbo_col);
    glDeleteBuffers(1, &ibo);
}

static void DrawTextBatch(Texture2D texture, const float* verts, int vertCount)
{
    if (!g_renderer) return;

    glBindVertexArray(g_renderer->text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_renderer->text_vbo);
    glBufferData(GL_ARRAY_BUFFER, (size_t)vertCount * 8 * sizeof(float), verts, GL_DYNAMIC_DRAW);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glUseProgram(g_renderer->text_shader.id);
    glUniform1i(g_renderer->text_sampler_uniform, 0);
    float identity[16] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    glUniformMatrix4fv(g_renderer->text_transform_uniform, 1, GL_FALSE, identity);
    glDrawArrays(GL_TRIANGLES, 0, vertCount);
    glBindVertexArray(0);
}

static bool EnsureFreeType(void)
{
    if (g_ft_inited)
    {
        return true;
    }

    if (FT_Init_FreeType(&g_ft_lib))
    {
        dbg_msg("Renderer", "FreeType init failed");
        return false;
    }

    g_ft_inited = true;
    return true;
}

static int NextPow2(int v)
{
    int p = 1;
    while (p < v)
    {
        p <<= 1;
    }
    return p;
}

Font* LoadFontTTF(const char* path, int pixel_size)
{
    if (!EnsureFreeType())
    {
        return NULL;
    }

    FT_Face face;
    if (FT_New_Face(g_ft_lib, path, 0, &face))
    {
        dbg_msg("Renderer", "Failed to load font: %s", path);
        return NULL;
    }

    FT_Set_Pixel_Sizes(face, 0, pixel_size);

    int max_h = 0;
    int total_w = 0;

    for (int c = 32; c < 127; ++c)
    {
        if (FT_Load_Char(face, (FT_ULong)c, FT_LOAD_RENDER))
        {
            continue;
        }
        total_w += face->glyph->bitmap.width + 1;
        if ((int)face->glyph->bitmap.rows > max_h)
        {
            max_h = (int)face->glyph->bitmap.rows;
        }
    }

    if (total_w <= 0 || max_h <= 0)
    {
        FT_Done_Face(face);
        dbg_msg("Renderer", "Font has no renderable glyphs: %s", path);
        return NULL;
    }

    int atlas_w = NextPow2(total_w);
    int atlas_h = NextPow2(max_h);
    size_t atlas_size = (size_t)atlas_w * (size_t)atlas_h * 4;
    unsigned char* atlas = (unsigned char*)calloc(atlas_size, 1);
    if (!atlas)
    {
        FT_Done_Face(face);
        dbg_msg("Renderer", "Font atlas alloc failed: %s", path);
        return NULL;
    }

    Font* font = (Font*)calloc(1, sizeof(Font));
    if (!font)
    {
        free(atlas);
        FT_Done_Face(face);
        return NULL;
    }

    int x = 0;
    for (int c = 32; c < 127; ++c)
    {
        if (FT_Load_Char(face, (FT_ULong)c, FT_LOAD_RENDER))
        {
            continue;
        }

        FT_GlyphSlot g = face->glyph;
        int w = (int)g->bitmap.width;
        int h = (int)g->bitmap.rows;

        Glyph* glyph = &font->glyphs[c];
        glyph->x = x;
        glyph->y = 0;
        glyph->width = w;
        glyph->height = h;
        glyph->bearingX = g->bitmap_left;
        glyph->bearingY = g->bitmap_top;
        glyph->advance = (int)(g->advance.x >> 6);

        for (int row = 0; row < h; ++row)
        {
            for (int col = 0; col < w; ++col)
            {
                unsigned char alpha = g->bitmap.buffer[row * g->bitmap.pitch + col];
                size_t idx = ((size_t)(glyph->y + row) * (size_t)atlas_w + (size_t)(glyph->x + col)) * 4;
                atlas[idx + 0] = 255;
                atlas[idx + 1] = 255;
                atlas[idx + 2] = 255;
                atlas[idx + 3] = alpha;
            }
        }

        x += w + 1;
    }

    GLuint tex_id;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas_w, atlas_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas);
    free(atlas);

    font->atlas.id = tex_id;
    font->atlas.width = atlas_w;
    font->atlas.height = atlas_h;
    font->size = pixel_size;
    font->ascent = (int)(face->size->metrics.ascender >> 6);
    font->descent = (int)(face->size->metrics.descender >> 6);
    font->line_height = (int)(face->size->metrics.height >> 6);
    if (font->line_height <= 0)
    {
        font->line_height = pixel_size;
    }

    FT_Done_Face(face);

    return font;
}

void UnloadFont(Font* font)
{
    if (!font)
    {
        return;
    }

    if (font->atlas.id != 0)
    {
        glDeleteTextures(1, &font->atlas.id);
    }

    free(font);
}

void SetDefaultFont(Font* font)
{
    if (g_default_font && g_default_font != font)
    {
        UnloadFont(g_default_font);
    }
    g_default_font = font;
}

static Font* GetDefaultFont(void)
{
    if (!g_default_font && !g_default_font_attempted)
    {
        g_default_font_attempted = true;
        g_default_font = LoadFontTTF("assets/arial.ttf", 56);
        if (!g_default_font)
        {
            dbg_msg("Renderer", "Default font load failed (arial.ttf)");
        }
    }
    return g_default_font;
}

void Renderer_DrawText(const char* text, float x, float y, float fontSize, Color color)
{
    Renderer_DrawTextEx(text, x, y, fontSize, color, TEXT_STYLE_NORMAL);
}

static void DrawTextPass(const char* text, float x, float y, float fontSize, Color color)
{
    if (!g_renderer || !text)
    {
        return;
    }

    Font* font = GetDefaultFont();
    if (!font)
    {
        return;
    }

    float scale = fontSize / (float)font->size;
    float pen_x = x;
    float pen_y = y;
    float baseline = floorf(pen_y + (float)font->ascent * scale + 0.5f);
    float line_step = (float)font->line_height * scale;

    int glyph_count = 0;
    for (const char* p = text; *p; ++p)
    {
        unsigned char c = (unsigned char)(*p);
        if (c == '\n' || c == '\t')
        {
            continue;
        }
        if (c < 32 || c >= 127)
        {
            continue;
        }
        Glyph* g = &font->glyphs[c];
        if (g->width > 0 && g->height > 0)
        {
            glyph_count++;
        }
    }

    if (glyph_count == 0)
    {
        return;
    }

    int vert_count = glyph_count * 6;
    float* verts = (float*)malloc((size_t)vert_count * 8 * sizeof(float));
    if (!verts)
    {
        return;
    }

    int v = 0;
    for (const char* p = text; *p; ++p)
    {
        unsigned char c = (unsigned char)(*p);

        if (c == '\n')
        {
            pen_x = x;
            pen_y += line_step;
            baseline = floorf(pen_y + (float)font->ascent * scale + 0.5f);
            continue;
        }

        if (c == '\t')
        {
            pen_x += (float)(font->glyphs[' '].advance * 4) * scale;
            continue;
        }

        if (c < 32 || c >= 127)
        {
            continue;
        }

        Glyph* g = &font->glyphs[c];
        if (g->width > 0 && g->height > 0)
        {
            float xpos = pen_x + (float)g->bearingX * scale;
            float ypos = baseline - (float)g->bearingY * scale;
            xpos = floorf(xpos + 0.5f);
            float w = (float)g->width * scale;
            float h = (float)g->height * scale;

            float u1 = (float)g->x / font->atlas.width;
            float v1 = (float)g->y / font->atlas.height;
            float u2 = (float)(g->x + g->width) / font->atlas.width;
            float v2 = (float)(g->y + g->height) / font->atlas.height;

            float x0 = xpos;
            float y0 = ypos;
            float x1 = xpos + w;
            float y1 = ypos + h;

            float r = color.r;
            float gcol = color.g;
            float b = color.b;
            float a = color.a;

            float quad[] =
            {
                x0, y0, u1, v1, r, gcol, b, a,
                x1, y0, u2, v1, r, gcol, b, a,
                x1, y1, u2, v2, r, gcol, b, a,

                x1, y1, u2, v2, r, gcol, b, a,
                x0, y1, u1, v2, r, gcol, b, a,
                x0, y0, u1, v1, r, gcol, b, a
            };

            memcpy(&verts[v * 8], quad, sizeof(quad));
            v += 6;
        }

        pen_x += (float)g->advance * scale;
    }

    if (v > 0)
    {
        DrawTextBatch(font->atlas, verts, v);
    }

    free(verts);
}

void Renderer_DrawTextEx(const char* text, float x, float y, float fontSize, Color color, TextStyle style)
{
    if (IsD2DBackend(g_renderer))
    {
        Vec2 p = TransformPointForBackend((Vec2){x, y});
        float size = TransformLengthForBackend(fontSize);
        D2DRenderer_DrawText((D2DRenderer*)g_renderer->backend_ctx, text, p.x, p.y, size, color, style);
        return;
    }

    if (!text)
    {
        return;
    }

    int outline_px = (int)fmaxf(1.0f, floorf(fontSize / 12.0f));
    Vec2 shadow_offset = { (float)outline_px, (float)outline_px };
    Color shadow = { 0.0f, 0.0f, 0.0f, 0.65f };
    Color outline = { 0.0f, 0.0f, 0.0f, 0.75f };

    if (style == TEXT_STYLE_SHADOW || style == TEXT_STYLE_OUTLINE_SHADOW)
    {
        DrawTextPass(text, x + shadow_offset.x, y + shadow_offset.y, fontSize, shadow);
    }

    if (style == TEXT_STYLE_OUTLINE || style == TEXT_STYLE_OUTLINE_SHADOW)
    {
        for (int oy = -outline_px; oy <= outline_px; ++oy)
        {
            for (int ox = -outline_px; ox <= outline_px; ++ox)
            {
                if (ox == 0 && oy == 0)
                {
                    continue;
                }
                DrawTextPass(text, x + (float)ox, y + (float)oy, fontSize, outline);
            }
        }
    }

    DrawTextPass(text, x, y, fontSize, color);
}
