#ifndef __FORGE_H__
#define __FORGE_H__

#include "camera.h"
#include "forgesystem.h"
#include "input.h"
#include "perlin.h"
#include "renderer.h"
#include "rendertexture.h"
#include "timer.h"
#include "window.h"
#include "worldgen.h"

#ifdef __cplusplus
extern "C"
{
#endif

bool Forge_Init();
void Forge_Shutdown();

#ifdef __cplusplus
}
#endif

#endif // __FORGE_H__