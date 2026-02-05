#ifndef __FORGE_PERLIN_H__
#define __FORGE_PERLIN_H__

#include <math.h>
#include <stdlib.h>

static int p[512];

static void Perlin_Init(int seed)
{
    srand(seed);

    int permutation[256];
    for (int i = 0; i < 256; i++)
        permutation[i] = i;

    for (int i = 255; i > 0; i--)
    {
        int j = rand() % (i + 1);
        int tmp = permutation[i];
        permutation[i] = permutation[j];
        permutation[j] = tmp;
    }

    for (int i = 0; i < 256; i++)
    {
        p[i] = permutation[i];
        p[256 + i] = permutation[i];
    }
}

static float Fade(float t)
{
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static float Lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

static float Grad(int hash, float x, float y)
{
    switch (hash & 3)
    {
        case 0: return  x + y;
        case 1: return -x + y;
        case 2: return  x - y;
        case 3: return -x - y;
        default: return 0;
    }
}

static float Perlin2D(float x, float y)
{
    int X = (int)floorf(x) & 255;
    int Y = (int)floorf(y) & 255;

    x -= floorf(x);
    y -= floorf(y);

    float u = Fade(x);
    float v = Fade(y);

    int aa = p[p[X] + Y];
    int ab = p[p[X] + Y + 1];
    int ba = p[p[X + 1] + Y];
    int bb = p[p[X + 1] + Y + 1];

    float res = Lerp(
        Lerp(Grad(aa, x, y),     Grad(ba, x - 1, y),     u),
        Lerp(Grad(ab, x, y - 1), Grad(bb, x - 1, y - 1), u),
        v
    );

    return (res + 1.0f) * 0.5f; // 0..1
}

static inline float FractalPerlin2D(float x, float y, int octaves, float persistence)
{
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; i++)
    {
        total += Perlin2D(x * frequency, y * frequency) * amplitude;
        maxValue += amplitude;

        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / maxValue;
}

#endif // __FORGE_PERLIN_H__