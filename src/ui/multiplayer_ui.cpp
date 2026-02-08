#include "multiplayer_ui.h"
#include <stdio.h>

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
                             int portBufSize)
{
    MpUiResult result = { false, false, false };
    char statusText[128];

    if (mode == MpMode::Host)
        snprintf(statusText, sizeof(statusText), "Status: Host (players %d)", playerCount);
    else if (mode == MpMode::Client)
        snprintf(statusText, sizeof(statusText), "Status: Client (players %d)", playerCount);
    else
        snprintf(statusText, sizeof(statusText), "Status: Offline");

    ImGuiLite_BeginWindow(ui, "Multiplayer", uiX, uiY, uiW, uiH);
    ImGuiLite_Text(ui, statusText);
    ImGuiLite_InputText(ui, "IP", ipBuffer, ipBufSize);
    ImGuiLite_InputText(ui, "Port", portBuffer, portBufSize);

    if (mode == MpMode::None)
    {
        if (ImGuiLite_Button(ui, "Host", 0.0f, 0.0f)) result.hostClicked = true;
        if (ImGuiLite_Button(ui, "Join", 0.0f, 0.0f)) result.joinClicked = true;
    }
    else
    {
        if (ImGuiLite_Button(ui, "Disconnect", 0.0f, 0.0f)) result.disconnectClicked = true;
    }

    ImGuiLite_EndWindow(ui);
    ImGuiLite_EndFrame(ui);

    return result;
}
