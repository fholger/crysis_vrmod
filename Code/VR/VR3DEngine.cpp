#include "StdAfx.h"
#include "VR3DEngine.h"

#include "Cry_Camera.h"
#include "D3D10Hooks.h"
#include "Game.h"
#include "Hooks.h"
#include "VRManager.h"
#include "Menus/FlashMenuObject.h"

namespace
{
	bool viewCamOverridden = false;
	CCamera originalViewCamera;
}

void VR_ISystem_Render(ISystem* pSelf);

const CCamera &VR_GetCurrentViewCamera()
{
	if (viewCamOverridden)
		return originalViewCamera;

	return gEnv->pSystem->GetViewCamera();
}


void VR_RenderSingleEye(ISystem *pSystem, int eye, const CCamera &originalCam)
{
	CCamera eyeCam = originalCam;
	gVR->ModifyViewCamera(eye, eyeCam);
	pSystem->SetViewCamera(eyeCam);
	float fov = eyeCam.GetFov();
	gEnv->pRenderer->EF_Query(EFQ_DrawNearFov, (INT_PTR)&fov);

	ColorF black(0, 0, 0, 1);
	gEnv->pRenderer->ClearBuffer(FRT_CLEAR_COLOR | FRT_CLEAR_IMMEDIATE, &black);

	//gVR->GetEffectiveRenderLimits(0, &left, &right, &top, &bottom);
	//VR_RestrictScissor(true, renderSize.x * left, renderSize.y * top, renderSize.x * right, renderSize.y * bottom);
	//gEnv->pRenderer->SetScissor(0, 0, renderSize.x, renderSize.y);

	CFlashMenuObject* menu = static_cast<CGame*>(gEnv->pGame)->GetMenu();
	// do not render while in menu, as it shows a rotating game world that is disorienting
	if (!menu->IsMenuActive())
	{
		hooks::CallOriginal(VR_ISystem_Render)(pSystem);
	}

	gVR->CaptureEye(eye);
}


void VR_ISystem_Render(ISystem *pSelf)
{
	originalViewCamera = pSelf->GetViewCamera();
	viewCamOverridden = true;

	gVR->AwaitFrame();

	VR_RenderSingleEye(pSelf, 0, originalViewCamera);
	// need to call RenderBegin to reset state, otherwise we get messed up object culling and other issues
	pSelf->RenderBegin();
	VR_RenderSingleEye(pSelf, 1, originalViewCamera);

	VR_RestrictScissor(false);
	Vec2i renderSize = gVR->GetRenderSize();
	gEnv->pRenderer->SetScissor(0, 0, renderSize.x, renderSize.y);
	// clear render target to fully transparent for HUD render
	ColorF transparent(0, 0, 0, 0);
	gEnv->pRenderer->ClearBuffer(FRT_CLEAR_COLOR | FRT_CLEAR_IMMEDIATE, &transparent);

	pSelf->SetViewCamera(originalViewCamera);
	viewCamOverridden = false;
}

void VR_InitRendererHooks()
{
	hooks::InstallVirtualFunctionHook("ISystem::Render", gEnv->pSystem, &ISystem::Render, &VR_ISystem_Render);
}
