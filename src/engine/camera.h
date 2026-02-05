#ifndef __FORGE_CAMERA_H__
#define __FORGE_CAMERA_H__

#include "vmath.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Camera2D
{
    float x;
    float y;
    float zoom;
} Camera2D;

void BeginCameraMode(Camera2D camera);
void EndCameraMode(void);

#ifdef __cplusplus
}
#endif

#endif // __FORGE_CAMERA_H__
