#pragma once

void VR_InitD3D10Hooks();
void VR_EnablePresent(bool enable);

void VR_IgnoreWindowSizeChange(bool ignore);
int VR_GetCurrentWindowWidth();
int VR_GetCurrentWindowHeight();
void VR_SetCurrentWindowSize(int width, int height);
