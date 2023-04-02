#pragma once

struct ID3D10Device;

void VR_InitD3D10Hooks();
void VR_InitD3D10DeviceHooks(ID3D10Device* device);
void VR_EnablePresent(bool enable);

void VR_IgnoreWindowSizeChange(bool ignore);
int VR_GetCurrentWindowWidth();
int VR_GetCurrentWindowHeight();
void VR_SetCurrentWindowSize(int width, int height);

void VR_RestrictScissor(bool restrict, int x1 = 0, int y1 = 0, int x2 = 0, int y2 = 0);
