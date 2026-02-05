#ifndef FORGE_VMATH_H
#define FORGE_VMATH_H

#include <math.h>
#include <stdbool.h>

typedef struct Vec2
{
    float x;
    float y;
} Vec2;

static inline Vec2 vec2_add(Vec2 a, Vec2 b)
{
    Vec2 r = { a.x + b.x, a.y + b.y };
    return r;
}

static inline Vec2 vec2_sub(Vec2 a, Vec2 b)
{
    Vec2 r = { a.x - b.x, a.y - b.y };
    return r;
}

static inline Vec2 vec2_mul(Vec2 v, float s)
{
    Vec2 r = { v.x * s, v.y * s };
    return r;
}

static inline Vec2 vec2_div(Vec2 v, float s)
{
    Vec2 r = { v.x / s, v.y / s };
    return r;
}

static inline bool vec2_equal(Vec2 a, Vec2 b)
{
    return a.x == b.x && a.y == b.y;
}

static inline float vec2_length(Vec2 v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
}

static inline Vec2 vec2_normalize(Vec2 v)
{
    float len = vec2_length(v);
    Vec2 r = { v.x / len, v.y / len };
    return r;
}

static inline Vec2 vec2_lerp(Vec2 a, Vec2 b, float t)
{
    Vec2 r = { a.x * (1.0f - t) + b.x * t, a.y * (1.0f - t) + b.y * t };
    return r;
}

typedef struct Vec3
{
    float x;
    float y;
    float z;
} Vec3;

static inline Vec3 vec3_add(Vec3 a, Vec3 b)
{
    Vec3 r = { a.x + b.x, a.y + b.y, a.z + b.z };
    return r;
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b)
{
    Vec3 r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

static inline Vec3 vec3_mul(Vec3 v, float s)
{
    Vec3 r = { v.x * s, v.y * s, v.z * s };
    return r;
}

static inline Vec3 vec3_div(Vec3 v, float s)
{
    Vec3 r = { v.x / s, v.y / s, v.z / s };
    return r;
}

static inline bool vec3_equal(Vec3 a, Vec3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

static inline float vec3_length(Vec3 v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline Vec3 vec3_normalize(Vec3 v)
{
    float len = vec3_length(v);
    Vec3 r = { v.x / len, v.y / len, v.z / len };
    return r;
}

static inline Vec3 vec3_lerp(Vec3 a, Vec3 b, float t)
{
    Vec3 r = { a.x * (1.0f - t) + b.x * t,
               a.y * (1.0f - t) + b.y * t,
               a.z * (1.0f - t) + b.z * t };
    return r;
}

typedef struct Vec4
{
    float x;
    float y;
    float z;
    float w;
} Vec4;

static inline Vec4 vec4_add(Vec4 a, Vec4 b)
{
    Vec4 r = { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w };
    return r;
}

static inline Vec4 vec4_sub(Vec4 a, Vec4 b)
{
    Vec4 r = { a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w };
    return r;
}

static inline Vec4 vec4_mul(Vec4 v, float s)
{
    Vec4 r = { v.x * s, v.y * s, v.z * s, v.w * s };
    return r;
}

static inline Vec4 vec4_div(Vec4 v, float s)
{
    Vec4 r = { v.x / s, v.y / s, v.z / s, v.w / s };
    return r;
}

static inline bool vec4_equal(Vec4 a, Vec4 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

static inline float vec4_length(Vec4 v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}

static inline Vec4 vec4_normalize(Vec4 v)
{
    float len = vec4_length(v);
    Vec4 r = { v.x / len, v.y / len, v.z / len, v.w / len };
    return r;
}

static inline Vec4 vec4_lerp(Vec4 a, Vec4 b, float t)
{
    Vec4 r = { a.x * (1.0f - t) + b.x * t,
               a.y * (1.0f - t) + b.y * t,
               a.z * (1.0f - t) + b.z * t,
               a.w * (1.0f - t) + b.w * t };
    return r;
}

typedef struct Rect
{
    float x;
    float y;
    float width;
    float height;
} Rect;

static inline bool rect_contains(Rect r, float px, float py)
{
    return px >= r.x && px <= r.x + r.width &&
           py >= r.y && py <= r.y + r.height;
}

static inline bool rect_equal(Rect a, Rect b)
{
    return a.x == b.x && a.y == b.y &&
           a.width == b.width && a.height == b.height;
}

typedef struct Color
{
    float r;
    float g;
    float b;
    float a;
} Color;

static inline bool color_equal(Color a, Color b)
{
    return a.r == b.r && a.g == b.g &&
           a.b == b.b && a.a == b.a;
}

typedef struct Mat4
{
    float e[16];
} Mat4;

static inline Mat4 mat4_identity(void)
{
    Mat4 m = {0};
    m.e[0]  = 1.0f;
    m.e[5]  = 1.0f;
    m.e[10] = 1.0f;
    m.e[15] = 1.0f;
    return m;
}

static inline Mat4 mat4_translation(Vec3 t)
{
    Mat4 m = mat4_identity();
    m.e[12] = t.x;
    m.e[13] = t.y;
    m.e[14] = t.z;
    return m;
}

static inline Mat4 mat4_scale(Vec3 s)
{
    Mat4 m = mat4_identity();
    m.e[0]  = s.x;
    m.e[5]  = s.y;
    m.e[10] = s.z;
    return m;
}

static inline Mat4 mat4_mul(Mat4 a, Mat4 b)
{
    Mat4 r = {0};
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            r.e[row + col * 4] =
                a.e[row + 0 * 4] * b.e[0 + col * 4] +
                a.e[row + 1 * 4] * b.e[1 + col * 4] +
                a.e[row + 2 * 4] * b.e[2 + col * 4] +
                a.e[row + 3 * 4] * b.e[3 + col * 4];
        }
    }
    return r;
}

static inline bool mat4_equal(Mat4 a, Mat4 b)
{
    for (int i = 0; i < 16; ++i)
    {
        if (a.e[i] != b.e[i])
        {
            return false;
        }
    }
    return true;
}

#endif // FORGE_VMATH_H
