#include "StdAfx.h"
#include "VRRenderer.h"

#include "Cry_Camera.h"
#include "Game.h"
#include "GameCVars.h"
#include "Hooks.h"
#include "IRenderAuxGeom.h"
#include "IVehicleSystem.h"
#include "Player.h"
#include "VRManager.h"
#include "Weapon.h"
#include "Menus/FlashMenuObject.h"
#include <d3d9.h>

namespace
{
	VRRenderer g_vrRendererImpl;
}

VRRenderer* gVRRenderer = &g_vrRendererImpl;

HRESULT D3D9_Present(IDirect3DDevice9Ex *pSelf, const RECT* pSourceRect, const RECT* pDestRect,HWND hDestWindowOverride, const RGNDATA* pDirtyRegion)
{
	HRESULT hr = S_OK;

	if (gVRRenderer->OnPrePresent(pSelf))
	{
		hr = hooks::CallOriginal(D3D9_Present)(pSelf, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
		gVRRenderer->OnPostPresent();
	}

	return hr;
}

BOOL Hook_SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int  X, int  Y, int  cx, int  cy, UINT uFlags)
{
	if (!gVRRenderer->ShouldIgnoreWindowSizeChanges())
	{
		gVRRenderer->SetDesiredWindowSize(gEnv->pRenderer->GetWidth(), gEnv->pRenderer->GetHeight());
		return hooks::CallOriginal(Hook_SetWindowPos)(hWnd, hWndInsertAfter, 0, 0, cx, cy, uFlags);
	}

	return TRUE;
}

void VR_ISystem_Render(ISystem *pSelf)
{
	gVRRenderer->Render(hooks::CallOriginal(VR_ISystem_Render), pSelf);
}

extern "C" {
  __declspec(dllimport) IDirect3DDevice9Ex* dxvkGetCreatedDevice();
}

	/*CryLogAlways("Creating d3d9 device for function hooking...");
	ComPtr<IDirect3D9Ex> d3d9;
	Direct3DCreate9Ex(D3D_SDK_VERSION, d3d9.GetAddressOf());
	if (!d3d9)
	{
		CryLogAlways("ERROR: Failed to create D3D9 adapter to hook!");
		return;
	}

	ComPtr<IDirect3DDevice9Ex> device;
	D3DPRESENT_PARAMETERS params {};
	params.BackBufferWidth = 1280;
	params.BackBufferHeight = 720;
	params.BackBufferFormat = D3DFMT_A8R8G8B8;
	params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	params.Windowed = TRUE;
	params.hDeviceWindow = (HWND)gEnv->pRenderer->GetHWND();
	params.PresentationInterval = 1;
	HRESULT hr = d3d9->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, (HWND)gEnv->pRenderer->GetHWND(), D3DCREATE_NOWINDOWCHANGES, &params, nullptr, device.GetAddressOf());
	if (hr != S_OK)
	{
		CryLogAlways("ERROR: Failed to create D3D9 device to hook: %i", hr);
		return;
	}*/

	IDirect3DDevice9Ex *device = dxvkGetCreatedDevice();
  if (!device)
  {
	  CryLogAlways("Could not get d3d9 device from dxvk");
		return;
  }

	CryLogAlways("Initializing rendering function hooks");
	hooks::InstallVirtualFunctionHook("ISystem::Render", gEnv->pSystem, &ISystem::Render, &VR_ISystem_Render);
	hooks::InstallVirtualFunctionHook("IDirect3DDevice9Ex::Present", device, &IDirect3DDevice9Ex::Present, &D3D9_Present);
	hooks::InstallHook("SetWindowPos", &SetWindowPos, &Hook_SetWindowPos);
}

void VRRenderer::Shutdown()
{
}

void VRRenderer::Render(SystemRenderFunc renderFunc, ISystem* pSystem)
{
	m_originalViewCamera = pSystem->GetViewCamera();

	gVR->AwaitFrame();

	RenderSingleEye(0, renderFunc, pSystem);
	// need to call RenderBegin to reset state, otherwise we get messed up object culling and other issues
	pSystem->RenderBegin();
	RenderSingleEye(1, renderFunc, pSystem);

	Vec2i renderSize = gVR->GetRenderSize();
	gEnv->pRenderer->SetScissor(0, 0, renderSize.x, renderSize.y);
	// clear render target to fully transparent for HUD render
	ColorF transparent(0, 0, 0, 0);
	//gEnv->pRenderer->ClearBuffer(FRT_CLEAR_COLOR | FRT_CLEAR_IMMEDIATE, &transparent);

	if (!ShouldRenderVR())
	{
		// for things like the binoculars, we skip the stereo rendering and instead render to the 2D screen
		renderFunc(pSystem);
	}
}

bool VRRenderer::OnPrePresent(IDirect3DDevice9Ex *device)
{
	gVR->SetDevice(device);
	gVR->CaptureHUD();
	return true;
}

void VRRenderer::OnPostPresent()
{
	gVR->FinishFrame();
}

const CCamera& VRRenderer::GetCurrentViewCamera() const
{
	if (m_viewCamOverridden)
		return m_originalViewCamera;

	return gEnv->pSystem->GetViewCamera();
}

void VRRenderer::ProjectToScreenPlayerCam(float ptx, float pty, float ptz, float* sx, float* sy, float* sz)
{
	const CCamera &currentCam = gEnv->pRenderer->GetCamera();
	gEnv->pRenderer->SetCamera(GetCurrentViewCamera());
	gEnv->pRenderer->ProjectToScreen(ptx, pty, ptz, sx, sy, sz);
	gEnv->pRenderer->SetCamera(currentCam);
}

void VRRenderer::SetDesiredWindowSize(int width, int height)
{
	m_windowWidth = width;
	m_windowHeight = height;
}

Vec2i VRRenderer::GetWindowSize() const
{
	return Vec2i(m_windowWidth, m_windowHeight);
}

void VRRenderer::ChangeRenderResolution(int width, int height)
{
	CryLogAlways("Changing render resolution to %i x %i", width, height);
	CryFixedStringT<32> cmd;
	m_ignoreWindowSizeChanges = true;
	cmd.Format("r_width %i", width);
	gEnv->pConsole->ExecuteString(cmd);
	cmd.Format("r_height %i", height);
	gEnv->pConsole->ExecuteString(cmd);
	gEnv->pRenderer->ChangeResolution(width, height, 8, 0, false);
	gEnv->pRenderer->EnableVSync(false);
	m_ignoreWindowSizeChanges = false;
}

bool VRRenderer::ShouldRenderVR() const
{
	if (g_pGameCVars->vr_cutscenes_2d && g_pGame->GetIGameFramework()->GetIViewSystem()->IsPlayingCutScene())
		return false;

	return !m_binocularsActive;
}

extern void DrawHUDFaders();

void VRRenderer::RenderSingleEye(int eye, SystemRenderFunc renderFunc, ISystem* pSystem)
{
	CCamera eyeCam = m_originalViewCamera;
	gVR->ModifyViewCamera(eye, eyeCam);
	pSystem->SetViewCamera(eyeCam);
	m_viewCamOverridden = true;
	float fov = eyeCam.GetFov();
	gEnv->pRenderer->EF_Query(EFQ_DrawNearFov, (INT_PTR)&fov);

	ColorF black(0, 0, 0, 1);
	gEnv->pRenderer->ClearBuffer(FRT_CLEAR_COLOR | FRT_CLEAR_IMMEDIATE, &black);

	CFlashMenuObject* menu = static_cast<CGame*>(gEnv->pGame)->GetMenu();
	// do not render while in menu, as it shows a rotating game world that is disorienting
	if (!menu->IsMenuActive() && ShouldRenderVR())
	{
		renderFunc(pSystem);
		DrawCrosshair();
	}

	pSystem->SetViewCamera(m_originalViewCamera);
	m_viewCamOverridden = false;

	DrawHUDFaders();

	gVR->CaptureEye(eye);
}

void VRRenderer::DrawCrosshair()
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

	const CCamera& cam = m_originalViewCamera;
	Vec3 crosshairPos = cam.GetPosition();
	Vec3 dir = cam.GetViewdir();
	dir.Normalize();
	float maxDistance = 10.f;

	std::vector<IPhysicalEntity*> skipEntities;
	skipEntities.push_back(pPlayer->GetEntity()->GetPhysics());
	if (pPlayer->GetLinkedVehicle())
	{
		skipEntities.push_back(pPlayer->GetLinkedVehicle()->GetEntity()->GetPhysics());
		IPhysicalEntity* vecSkipEnts[8];
		int numSkips = pPlayer->GetLinkedVehicle()->GetSkipEntities(vecSkipEnts, 8);
		for (int i = 0; i < numSkips; ++i)
			skipEntities.push_back(vecSkipEnts[i]);
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
