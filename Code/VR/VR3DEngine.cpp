#include "StdAfx.h"
#include "VR3DEngine.h"

#include "Cry_Camera.h"
#include "D3D10Hooks.h"
#include "Game.h"
#include "Hooks.h"
#include "VRManager.h"
#include "Menus/FlashMenuObject.h"

void VR_ISystem_Render(ISystem* pSelf);


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
	CCamera origCam = pSelf->GetViewCamera();

	gVR->AwaitFrame();

	VR_RenderSingleEye(pSelf, 0, origCam);
	// need to call RenderBegin to reset state, otherwise we get messed up object culling and other issues
	pSelf->RenderBegin();
	VR_RenderSingleEye(pSelf, 1, origCam);

	VR_RestrictScissor(false);
	Vec2i renderSize = gVR->GetRenderSize();
	gEnv->pRenderer->SetScissor(0, 0, renderSize.x, renderSize.y);
	// clear render target to fully transparent for HUD render
	ColorF transparent(0, 0, 0, 0);
	gEnv->pRenderer->ClearBuffer(FRT_CLEAR_COLOR | FRT_CLEAR_IMMEDIATE, &transparent);
}

void VR_InitRendererHooks()
{
	hooks::InstallVirtualFunctionHook("ISystem::Render", gEnv->pSystem, &ISystem::Render, &VR_ISystem_Render);
}
