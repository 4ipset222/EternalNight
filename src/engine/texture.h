#ifndef __FORGE_TEXTURE_H__
#define __FORGE_TEXTURE_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Texture2D
{
    unsigned int id;
    int width;
    int height;
    void* native_handle;
    char path[260];
} Texture2D;

#ifdef __cplusplus
}
#endif

#endif // __FORGE_TEXTURE_H__
