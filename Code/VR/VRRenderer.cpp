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

namespace
{
	VRRenderer g_vrRendererImpl;
}

VRRenderer* gVRRenderer = &g_vrRendererImpl;

HRESULT IDXGISwapChain_Present(IDXGISwapChain *pSelf, UINT SyncInterval, UINT Flags)
{
	HRESULT hr = 0;

	if (gVRRenderer->OnPrePresent(pSelf))
	{
		hr = hooks::CallOriginal(IDXGISwapChain_Present)(pSelf, SyncInterval, Flags);
		gVRRenderer->OnPostPresent();
	}

	return hr;
}

HRESULT IDXGISwapChain_ResizeTarget(IDXGISwapChain *pSelf, const DXGI_MODE_DESC *pNewTargetParameters)
{
	if (!gVRRenderer->ShouldIgnoreWindowSizeChanges())
	{
		gVRRenderer->SetDesiredWindowSize(pNewTargetParameters->Width, pNewTargetParameters->Height);
		return hooks::CallOriginal(IDXGISwapChain_ResizeTarget)(pSelf, pNewTargetParameters);
	}

	return 0;
}

HRESULT IDXGISwapChain_ResizeBuffers(IDXGISwapChain *pSelf, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	return hooks::CallOriginal(IDXGISwapChain_ResizeBuffers)(pSelf, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

BOOL Hook_SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int  X, int  Y, int  cx, int  cy, UINT uFlags)
{
	if (!gVRRenderer->ShouldIgnoreWindowSizeChanges())
	{
		return hooks::CallOriginal(Hook_SetWindowPos)(hWnd, hWndInsertAfter, 0, 0, cx, cy, uFlags);
	}

	return TRUE;
}

void VR_ISystem_Render(ISystem *pSelf)
{
	gVRRenderer->Render(hooks::CallOriginal(VR_ISystem_Render), pSelf);
}

void VRRenderer::Init()
{
	ComPtr<ID3D10Device> device;
	HRESULT hr = D3D10CreateDevice(nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr, 0, D3D10_SDK_VERSION, device.GetAddressOf());
	if (!device)
	{
		CryLogAlways("Failed to create D3D10 device to hook: %i", hr);
		return;
	}

	ComPtr<IDXGIFactory> dxgiFactory;
	CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)dxgiFactory.GetAddressOf());
	if (!dxgiFactory)
	{
		CryError("Failed to create DXGI factory");
		return;
	}

	DXGI_SWAP_CHAIN_DESC desc = {};
	desc.BufferCount = 2;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	desc.BufferDesc.Width = gEnv->pRenderer->GetWidth();
	desc.BufferDesc.Height = gEnv->pRenderer->GetHeight();
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.SampleDesc.Count = 1;
	desc.Windowed = TRUE;
	desc.OutputWindow = (HWND)gEnv->pRenderer->GetHWND();
	ComPtr<IDXGISwapChain> swapchain;
	dxgiFactory->CreateSwapChain(device.Get(), &desc, swapchain.GetAddressOf());
	if (!swapchain)
	{
		CryError("Failed to create swapchain");
		return;
	}

	hooks::InstallVirtualFunctionHook("ISystem::Render", gEnv->pSystem, &ISystem::Render, &VR_ISystem_Render);
	hooks::InstallVirtualFunctionHook("IDXGISwapChain::Present", swapchain.Get(), 8, &IDXGISwapChain_Present);
	hooks::InstallVirtualFunctionHook("IDXGISwapChain::ResizeBuffers", swapchain.Get(), 13, &IDXGISwapChain_ResizeBuffers);
	hooks::InstallVirtualFunctionHook("IDXGISwapChain::ResizeTarget", swapchain.Get(), 14, &IDXGISwapChain_ResizeTarget);
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
	gEnv->pRenderer->ClearBuffer(FRT_CLEAR_COLOR | FRT_CLEAR_IMMEDIATE, &transparent);

	if (!ShouldRenderVR())
	{
		// for things like the binoculars, we skip the stereo rendering and instead render to the 2D screen
		renderFunc(pSystem);
	}
}

bool VRRenderer::OnPrePresent(IDXGISwapChain *swapChain)
{
	gVR->SetSwapChain(swapChain);
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
	m_ignoreWindowSizeChanges = true;
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
