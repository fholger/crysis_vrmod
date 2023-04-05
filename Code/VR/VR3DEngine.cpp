#include "StdAfx.h"
#include "VR3DEngine.h"

#include "Cry_Camera.h"
#include "D3D10Hooks.h"
#include "Game.h"
#include "Hooks.h"
#include "IRenderAuxGeom.h"
#include "IVehicleSystem.h"
#include "Player.h"
#include "VRManager.h"
#include "Weapon.h"
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

void VR_ProjectToScreenPlayerCam(float ptx, float pty, float ptz, float* sx, float* sy, float* sz)
{
	const CCamera &currentCam = gEnv->pRenderer->GetCamera();
	gEnv->pRenderer->SetCamera(originalViewCamera);
	gEnv->pRenderer->ProjectToScreen(ptx, pty, ptz, sx, sy, sz);
	gEnv->pRenderer->SetCamera(currentCam);
}


void VR_DrawCrosshair()
{
	// don't show crosshair during cutscenes
	if (gEnv->pGame->GetIGameFramework()->GetIViewSystem()->IsPlayingCutScene())
		return;

	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if (!pPlayer)
		return;
	if (CWeapon *weapon = pPlayer->GetWeapon(pPlayer->GetCurrentItemId(true)))
	{
		// don't draw a crosshair if the weapon laser is active
		if (weapon->IsLamLaserActivated())
			return;
	}

	const CCamera& cam = VR_GetCurrentViewCamera();
	Vec3 crosshairPos = cam.GetPosition();
	Vec3 dir = cam.GetViewdir();
	dir.Normalize();
	float maxDistance = 10.f;

	std::vector<IPhysicalEntity*> skipEntities;
	skipEntities.push_back(pPlayer->GetEntity()->GetPhysics());
	if (pPlayer->GetLinkedVehicle())
	{
		skipEntities.push_back(pPlayer->GetLinkedVehicle()->GetEntity()->GetPhysics());
		maxDistance = 16.f;
	}
	const int objects = ent_all;
	const int flags = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (10 & rwi_pierceability_mask) | (geom_colltype14 << rwi_colltype_bit);

	ray_hit hit;
	if (gEnv->pPhysicalWorld->RayWorldIntersection(crosshairPos, dir*maxDistance, objects, flags, &hit, 1, skipEntities.data(), skipEntities.size()))
	{
		crosshairPos = hit.pt;
	}
	else
	{
		crosshairPos += dir * maxDistance;
	}

	// for the moment, draw something primitive with the debug tools. Maybe later we can find something more elegant...
	SAuxGeomRenderFlags geomMode;
	geomMode.SetDepthTestFlag(e_DepthTestOff);
	geomMode.SetMode2D3DFlag(e_Mode3D);
	geomMode.SetDrawInFrontMode(e_DrawInFrontOn);
	gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(geomMode);
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(crosshairPos, 0.03f, ColorB(240, 240, 240));
	gEnv->pRenderer->GetIRenderAuxGeom()->Flush();
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
	if (!menu->IsMenuActive() && gVR->ShouldRenderVR())
	{
		hooks::CallOriginal(VR_ISystem_Render)(pSystem);
		VR_DrawCrosshair();
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

	if (!gVR->ShouldRenderVR())
	{
		// for things like the binoculars, we skip the stereo rendering and instead render to the 2D screen
		hooks::CallOriginal(VR_ISystem_Render)(pSelf);
	}
}

void VR_InitRendererHooks()
{
	hooks::InstallVirtualFunctionHook("ISystem::Render", gEnv->pSystem, &ISystem::Render, &VR_ISystem_Render);
}
