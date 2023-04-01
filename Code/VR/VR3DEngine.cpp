#include "StdAfx.h"
#include "VR3DEngine.h"

#include "Cry_Camera.h"
#include "Hooks.h"
#include "VRManager.h"



void VR_ISystem_Render(ISystem *pSelf)
{
	CCamera origCam = pSelf->GetViewCamera();
	CCamera leftEyeCam = pSelf->GetViewCamera();
	CCamera rightEyeCam = pSelf->GetViewCamera();
	Vec2i renderSize = gVR->GetRenderSize();

	gVR->AwaitFrame();

	ColorF black(0, 0, 0, 1);
	gVR->ModifyViewCamera(0, leftEyeCam);
	pSelf->SetViewCamera(leftEyeCam);
	hooks::CallOriginal(VR_ISystem_Render)(pSelf);
	gVR->CaptureEye(0);

	pSelf->RenderBegin();
	gVR->ModifyViewCamera(1, rightEyeCam);
	pSelf->SetViewCamera(rightEyeCam);
	hooks::CallOriginal(VR_ISystem_Render)(pSelf);
	gVR->CaptureEye(1);

	// clear render target to fully transparent for HUD render
	ColorF transparent(0, 0, 0, 0);
	gEnv->pRenderer->ClearBuffer(FRT_CLEAR_COLOR | FRT_CLEAR_IMMEDIATE, &transparent);
}

void VR_InitRendererHooks()
{
	hooks::InstallVirtualFunctionHook("ISystem::Render", gEnv->pSystem, &ISystem::Render, &VR_ISystem_Render);
}
