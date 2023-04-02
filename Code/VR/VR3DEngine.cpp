#include "StdAfx.h"
#include "VR3DEngine.h"

#include "Cry_Camera.h"
#include "D3D10Hooks.h"
#include "Hooks.h"
#include "VRManager.h"



void VR_ISystem_Render(ISystem *pSelf)
{
	CCamera origCam = pSelf->GetViewCamera();
	CCamera leftEyeCam = pSelf->GetViewCamera();
	CCamera rightEyeCam = pSelf->GetViewCamera();

	gVR->AwaitFrame();

	ColorF black(0, 0, 0, 1);
	Vec2i renderSize = gVR->GetRenderSize();
	//float left, right, top, bottom;

	gVR->ModifyViewCamera(0, leftEyeCam);
	pSelf->SetViewCamera(leftEyeCam);
	float fov = leftEyeCam.GetFov();
	gEnv->pRenderer->EF_Query(EFQ_DrawNearFov, (INT_PTR)&fov);
	gEnv->pRenderer->ClearBuffer(FRT_CLEAR_COLOR | FRT_CLEAR_IMMEDIATE, &black);
	//gVR->GetEffectiveRenderLimits(0, &left, &right, &top, &bottom);
	//VR_RestrictScissor(true, renderSize.x * left, renderSize.y * top, renderSize.x * right, renderSize.y * bottom);
	//gEnv->pRenderer->SetScissor(0, 0, renderSize.x, renderSize.y);
	hooks::CallOriginal(VR_ISystem_Render)(pSelf);
	gVR->CaptureEye(0);

	pSelf->RenderBegin();
	gVR->ModifyViewCamera(1, rightEyeCam);
	pSelf->SetViewCamera(rightEyeCam);
	fov = rightEyeCam.GetFov();
	gEnv->pRenderer->EF_Query(EFQ_DrawNearFov, (INT_PTR)&fov);
	gEnv->pRenderer->ClearBuffer(FRT_CLEAR_COLOR | FRT_CLEAR_IMMEDIATE, &black);
	//gVR->GetEffectiveRenderLimits(1, &left, &right, &top, &bottom);
	//VR_RestrictScissor(true, renderSize.x * left, renderSize.y * top, renderSize.x * right, renderSize.y * bottom);
	//gEnv->pRenderer->SetScissor(0, 0, renderSize.x, renderSize.y);
	hooks::CallOriginal(VR_ISystem_Render)(pSelf);
	gVR->CaptureEye(1);

	VR_RestrictScissor(false);
	gEnv->pRenderer->SetScissor(0, 0, renderSize.x, renderSize.y);
	// clear render target to fully transparent for HUD render
	ColorF transparent(0, 0, 0, 0);
	gEnv->pRenderer->ClearBuffer(FRT_CLEAR_COLOR | FRT_CLEAR_IMMEDIATE, &transparent);
}

void VR_InitRendererHooks()
{
	hooks::InstallVirtualFunctionHook("ISystem::Render", gEnv->pSystem, &ISystem::Render, &VR_ISystem_Render);
}
