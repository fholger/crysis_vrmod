#include "StdAfx.h"
#include "VRRenderer.h"

#include "Cry_Camera.h"
#include "Game.h"
#include "GameCVars.h"
#include "Hooks.h"
#include "IRenderAuxGeom.h"
#include "IronSight.h"
#include "IVehicleSystem.h"
#include "OpenXRRuntime.h"
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

extern ID3D10Device1 *g_latestCreatedDevice;
extern IDXGISwapChain *g_latestCreatedSwapChain;

const D3DCOLOR EvtCol = D3DCOLOR_ARGB(255, 255, 0, 0);

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

void VR_ID3D10Device_OMSetRenderTargets(ID3D10Device *pSelf, UINT NumViews, ID3D10RenderTargetView *const *ppRenderTargetViews, ID3D10DepthStencilView *pDepthStencilView)
{
	gVRRenderer->OnRenderTargetChanged(NumViews > 0 ? ppRenderTargetViews[0] : nullptr, pDepthStencilView);
	hooks::CallOriginal(VR_ID3D10Device_OMSetRenderTargets)(pSelf, NumViews, ppRenderTargetViews, pDepthStencilView);
}

void VR_ID3D10Device_RSSetState(ID3D10Device *pSelf, ID3D10RasterizerState *state)
{
	gVRRenderer->OnSetRasterizerState(state);
	hooks::CallOriginal(VR_ID3D10Device_RSSetState)(pSelf, state);
}

void VR_ISystem_Render(ISystem *pSelf)
{
	gVRRenderer->Render(hooks::CallOriginal(VR_ISystem_Render), pSelf);
}

void VR_ISystem_Quit(ISystem *pSelf)
{
	gVRRenderer->Shutdown();
	gVR->Shutdown();
	gXR->Shutdown();
	hooks::CallOriginal(VR_ISystem_Quit)(pSelf);
}

void VRRenderer::Init()
{
	hooks::InstallVirtualFunctionHook("ISystem::Render", gEnv->pSystem, &ISystem::Render, &VR_ISystem_Render);
	hooks::InstallHook("SetWindowPos", &SetWindowPos, &Hook_SetWindowPos);
	hooks::InstallVirtualFunctionHook("ISystem::Quit", gEnv->pSystem, &ISystem::Quit, &VR_ISystem_Quit);

	m_lastPresentCallTime = gEnv->pTimer->GetAsyncTime().GetMilliSecondsAsInt64();

	IDXGISwapChain *swapChain = g_latestCreatedSwapChain;
	CryLogAlways("Retrieved swap chain: %ul", (uintptr_t)swapChain);

	if (!swapChain)
	{
		CryLogAlways("Error: no swapchain found");
		return;
	}

	swapChain->GetDevice(__uuidof(ID3D10Device), (void**)m_device.ReleaseAndGetAddressOf());
	hooks::InstallVirtualFunctionHook("IDXGISwapChain::Present", swapChain, 8, &IDXGISwapChain_Present);
	hooks::InstallVirtualFunctionHook("IDXGISwapChain::ResizeBuffers", swapChain, 13, &IDXGISwapChain_ResizeBuffers);
	hooks::InstallVirtualFunctionHook("IDXGISwapChain::ResizeTarget", swapChain, 14, &IDXGISwapChain_ResizeTarget);
	hooks::InstallVirtualFunctionHook("ID3D10Device::OMSetRenderTargets", m_device.Get(), &ID3D10Device::OMSetRenderTargets, &VR_ID3D10Device_OMSetRenderTargets);
	hooks::InstallVirtualFunctionHook("ID3D10Device::RSSetState", m_device.Get(), &ID3D10Device::RSSetState, &VR_ID3D10Device_RSSetState);

	gVR->SetSwapChain(swapChain);
}

void VRRenderer::Shutdown()
{
}

void VRRenderer::Render(SystemRenderFunc renderFunc, ISystem* pSystem)
{
	D3DPERF_BeginEvent(EvtCol, L"StereoRender");
	IDXGISwapChain *currentSwapChain = g_latestCreatedSwapChain;
	int64 milliSecsSinceLastPresentCall = gEnv->pTimer->GetAsyncTime().GetMilliSecondsAsInt64() - m_lastPresentCallTime;
	if (currentSwapChain != gVR->GetSwapChain() || milliSecsSinceLastPresentCall > 1000)
	{
		CryLogAlways("May have lost our Present hook, recreating!");
		hooks::RemoveHook(&IDXGISwapChain_Present);
		hooks::RemoveHook(&IDXGISwapChain_ResizeTarget);
		hooks::RemoveHook(&IDXGISwapChain_ResizeBuffers);
		hooks::InstallVirtualFunctionHook("IDXGISwapChain::Present", currentSwapChain, 8, &IDXGISwapChain_Present);
		hooks::InstallVirtualFunctionHook("IDXGISwapChain::ResizeBuffers", currentSwapChain, 13, &IDXGISwapChain_ResizeBuffers);
		hooks::InstallVirtualFunctionHook("IDXGISwapChain::ResizeTarget", currentSwapChain, 14, &IDXGISwapChain_ResizeTarget);
		gVR->SetSwapChain(currentSwapChain);
	}
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

	m_didRenderThisFrame = true;
	D3DPERF_EndEvent();
}

bool VRRenderer::OnPrePresent(IDXGISwapChain *swapChain)
{
	m_lastPresentCallTime = gEnv->pTimer->GetAsyncTime().GetMilliSecondsAsInt64();

	gVR->SetSwapChain(swapChain);
	D3DPERF_SetMarker(EvtCol, L"CaptureHUD");
	gVR->CaptureHUD();
	return true;
}

void VRRenderer::OnPostPresent()
{
	gVR->FinishFrame(m_didRenderThisFrame);
	m_didRenderThisFrame = false;
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

	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if (pPlayer)
	{
		if (CWeapon* weapon = pPlayer->GetWeapon(pPlayer->GetCurrentItemId(true)))
		{
			IZoomMode* zoom = weapon->GetZoomMode(weapon->GetCurrentZoomMode());
			if (CIronSight *sight = dynamic_cast<CIronSight*>(zoom))
			{
				if (sight->IsScope() && (sight->IsZoomed() || sight->IsZooming()))
					return false;
			}
		}
	}

	return !m_binocularsActive;
}

void VRRenderer::OnRenderTargetChanged(ID3D10RenderTargetView* rtv, ID3D10DepthStencilView* depth)
{
	m_renderTargetIsBackBuffer = false;

	if (!g_latestCreatedSwapChain || !rtv)
		return;

	// check if the render target is the current backbuffer
	ComPtr<ID3D10Texture2D> tex;
	rtv->GetResource((ID3D10Resource**)tex.GetAddressOf());

	Vec2i renderSize = gVR->GetRenderSize();
	D3D10_TEXTURE2D_DESC desc;
	tex->GetDesc(&desc);
	if (desc.Width != renderSize.x || desc.Height != renderSize.y)
		return;

	m_renderTargetIsBackBuffer = true;

	if (m_renderingEye == -1)
		return;

	// make sure scissor is enabled and set scissor
	ComPtr<ID3D10RasterizerState> currentState;
	m_device->RSGetState(currentState.GetAddressOf());
	ID3D10RasterizerState* desiredState = currentState.Get();
	OnSetRasterizerState(desiredState);
	if (desiredState != currentState.Get())
		hooks::CallOriginal(VR_ID3D10Device_RSSetState)(m_device.Get(), desiredState);
	SetScissorForCurrentEye();
}

void VRRenderer::OnSetRasterizerState(ID3D10RasterizerState*& state)
{
	if (!m_renderTargetIsBackBuffer || m_renderingEye == -1 || !state || !g_pGameCVars->vr_render_use_scissor)
		return;

	D3D10_RASTERIZER_DESC desc;
	state->GetDesc(&desc);
	if (desc.ScissorEnable) // scissor already enabled?
		return;

	auto it = m_replacementStates.find(state);
	if (it == m_replacementStates.end())
	{
		CryLogAlways("Creating replacement rasterizer state with scissor enabled for %ul", (uintptr_t)state);
		desc.ScissorEnable = TRUE;
		m_device->CreateRasterizerState(&desc, m_replacementStates[state].ReleaseAndGetAddressOf());
	}

	state = m_replacementStates[state].Get();
}

extern void DrawHUDFaders();

void VRRenderer::RenderSingleEye(int eye, SystemRenderFunc renderFunc, ISystem* pSystem)
{
	D3DPERF_BeginEvent(EvtCol, L"SingleEye");
	m_renderingEye = eye;
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

	D3DPERF_SetMarker(EvtCol, L"CaptureEye");
	gVR->CaptureEye(eye);
	m_renderingEye = -1;

	D3DPERF_EndEvent();
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

	D3DPERF_BeginEvent(EvtCol, L"DrawCrosshair");
	// for the moment, draw something primitive with the debug tools. Maybe later we can find something more elegant...
	SAuxGeomRenderFlags geomMode;
	geomMode.SetDepthTestFlag(e_DepthTestOff);
	geomMode.SetMode2D3DFlag(e_Mode3D);
	geomMode.SetDrawInFrontMode(e_DrawInFrontOn);
	gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(geomMode);
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(crosshairPos, 0.03f, ColorB(240, 240, 240));
	gEnv->pRenderer->GetIRenderAuxGeom()->Flush();
	D3DPERF_EndEvent();
}

void VRRenderer::SetScissorForCurrentEye()
{
	if (m_renderingEye == -1 || !g_pGameCVars->vr_render_use_scissor)
		return;

	// since we can't easily set the projection matrix properly for asymmetric FOV rendering, we will instead set a scissor
	// to recoup the GPU performance lost.
	RectF limits = gVR->GetEffectiveRenderLimits(m_renderingEye);
	Vec2i size = gVR->GetRenderSize();

	D3D10_RECT scissor;
	scissor.left = size.x * limits.x;
	scissor.top = size.y * limits.y;
	scissor.right = size.x * (limits.x + limits.w);
	scissor.bottom = size.y * (limits.y + limits.h);
	m_device->RSSetScissorRects(1, &scissor);
}
