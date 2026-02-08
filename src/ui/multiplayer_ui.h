#ifndef __MULTIPLAYER_UI_H__
#define __MULTIPLAYER_UI_H__

#include "multiplayer_types.h"
#include "ui/imgui_lite.h"

typedef struct MpUiResult
{
    bool hostClicked;
    bool joinClicked;
    bool disconnectClicked;
} MpUiResult;

MpUiResult DrawMultiplayerUI(ImGuiLiteContext* ui,
                             float* uiX,
                             float* uiY,
                             float uiW,
                             float uiH,
                             MpMode mode,
                             int playerCount,
                             char* ipBuffer,
                             int ipBufSize,
                             char* portBuffer,
                             int portBufSize);

#endif // __MULTIPLAYER_UI_H__
