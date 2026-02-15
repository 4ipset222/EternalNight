#include "camera.h"
#include "renderer.h"
#include <GL/glew.h>
#include <math.h>

static int g_in_camera_mode = 0;

void BeginCameraMode(Camera2D camera)
{
    if (g_in_camera_mode) return;
    g_in_camera_mode = 1;
    if (g_renderer)
    {
        g_renderer->camera_mode = 1;
        g_renderer->camera_x = camera.x;
        g_renderer->camera_y = camera.y;
        g_renderer->camera_zoom = camera.zoom;
    }

    if (!g_renderer || Renderer_GetBackend(g_renderer) != RENDERER_BACKEND_OPENGL)
    {
        return;
    }

    float zoom = camera.zoom;
    float width = (float)g_renderer->width / zoom;
    float height = (float)g_renderer->height / zoom;
    
    float left = camera.x;
    float right = camera.x + width;
    float bottom = camera.y + height;
    float top = camera.y;
    
    float projection[16] =
    {
        2.0f / (right - left), 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -(right + left) / (right - left), -(top + bottom) / (top - bottom), 0.0f, 1.0f
    };

    glUseProgram(g_renderer->primitive_shader.id);
    GLint proj_uniform = glGetUniformLocation(g_renderer->primitive_shader.id, "projection");
    glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, projection);

    glUseProgram(g_renderer->texture_shader.id);
    proj_uniform = glGetUniformLocation(g_renderer->texture_shader.id, "projection");
    glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, projection);
}

void EndCameraMode(void)
{
    if (!g_in_camera_mode) return;
    g_in_camera_mode = 0;
    if (g_renderer)
    {
        g_renderer->camera_mode = 0;
        g_renderer->camera_x = 0.0f;
        g_renderer->camera_y = 0.0f;
        g_renderer->camera_zoom = 1.0f;
    }

    if (!g_renderer || Renderer_GetBackend(g_renderer) != RENDERER_BACKEND_OPENGL)
    {
        return;
    }

    float left = 0.0f;
    float right = (float)g_renderer->width;
    float bottom = (float)g_renderer->height;
    float top = 0.0f;
    
    float projection[16] =
    {
        2.0f / (right - left), 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -(right + left) / (right - left), -(top + bottom) / (top - bottom), 0.0f, 1.0f
    };

    glUseProgram(g_renderer->primitive_shader.id);
    GLint proj_uniform = glGetUniformLocation(g_renderer->primitive_shader.id, "projection");
    glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, projection);

    glUseProgram(g_renderer->texture_shader.id);
    proj_uniform = glGetUniformLocation(g_renderer->texture_shader.id, "projection");
    glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, projection);
}
