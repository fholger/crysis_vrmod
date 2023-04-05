#pragma once

const CCamera& VR_GetCurrentViewCamera();
void VR_ProjectToScreenPlayerCam( float ptx, float pty, float ptz, float *sx, float *sy, float *sz );
void VR_InitRendererHooks();
