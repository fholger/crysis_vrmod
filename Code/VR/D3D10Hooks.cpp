#include "StdAfx.h"
#include "Hooks.h"
#include "VRManager.h"
#include <d3d10misc.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <WinUser.h>

using Microsoft::WRL::ComPtr;

namespace
{
	bool presentEnabled = true;
	bool ignoreWindowSizeChange = false;
	int currentWindowWidth = 0;
	int currentWindowHeight = 0;

	bool scissorRestricted = false;
	int scissorX1Limit = 0;
	int scissorY1Limit = 0;
	int scissorX2Limit = 0;
	int scissorY2Limit = 0;
}

HRESULT IDXGISwapChain_Present(IDXGISwapChain *pSelf, UINT SyncInterval, UINT Flags)
{
	HRESULT hr = 0;

	if (presentEnabled)
	{
		gVR->CaptureHUD();
		hr = hooks::CallOriginal(IDXGISwapChain_Present)(pSelf, SyncInterval, Flags);
		gVR->FinishFrame(pSelf);
	}

	return hr;
}

HRESULT IDXGISwapChain_ResizeTarget(IDXGISwapChain *pSelf, const DXGI_MODE_DESC *pNewTargetParameters)
{
	if (!ignoreWindowSizeChange)
	{
		currentWindowWidth = pNewTargetParameters->Width;
		currentWindowHeight = pNewTargetParameters->Height;
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
	if (!ignoreWindowSizeChange)
	{
		return hooks::CallOriginal(Hook_SetWindowPos)(hWnd, hWndInsertAfter, 0, 0, cx, cy, uFlags);
	}

	return TRUE;
}

void ID3D10Device_RSSetScissorRects(ID3D10Device *pSelf, UINT NumRects, const D3D10_RECT *pRects)
{
	CryLogAlways("RSSetScissorRects called");
	ComPtr<ID3D10RasterizerState> state;
	pSelf->RSGetState(state.GetAddressOf());
	D3D10_RASTERIZER_DESC stateDesc;
	state->GetDesc(&stateDesc);
	CryLogAlways("Scissor test enabled: %i", stateDesc.ScissorEnable);

	if (scissorRestricted)
	{
		ID3D10RenderTargetView* rts[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		pSelf->OMGetRenderTargets(NumRects, rts, nullptr);
		static D3D10_RECT scissors[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		Vec2i renderSize = gVR->GetRenderSize();
		for (UINT i = 0; i < NumRects; ++i)
		{
			scissors[i] = pRects[i];
			if (!rts[i])
				continue;

			ComPtr<ID3D10Texture2D> texture;
			rts[i]->GetResource((ID3D10Resource**)texture.GetAddressOf());
			rts[i]->Release();

			D3D10_TEXTURE2D_DESC desc;
			texture->GetDesc(&desc);
			if (desc.Width != renderSize.x || desc.Height != renderSize.y)
				continue;

			scissors[i].left = max(scissors[i].left, (LONG)scissorX1Limit);
			scissors[i].right = min(scissors[i].right, (LONG)scissorX2Limit);
			scissors[i].top = max(scissors[i].top, (LONG)scissorY1Limit);
			scissors[i].bottom = min(scissors[i].bottom, (LONG)scissorY2Limit);
			CryLogAlways("Modified scissor rect %i to (%i, %i, %i, %i)", i, scissors[i].left, scissors[i].top, scissors[i].right, scissors[i].bottom);
		}

		pRects = scissors;
	}

	hooks::CallOriginal(ID3D10Device_RSSetScissorRects)(pSelf, NumRects, pRects);
}

void VR_InitD3D10Hooks()
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

	hooks::InstallVirtualFunctionHook("IDXGISwapChain::Present", swapchain.Get(), 8, &IDXGISwapChain_Present);
	hooks::InstallVirtualFunctionHook("IDXGISwapChain::ResizeBuffers", swapchain.Get(), 13, &IDXGISwapChain_ResizeBuffers);
	hooks::InstallVirtualFunctionHook("IDXGISwapChain::ResizeTarget", swapchain.Get(), 14, &IDXGISwapChain_ResizeTarget);
	hooks::InstallHook("SetWindowPos", &SetWindowPos, &Hook_SetWindowPos);
}

void VR_InitD3D10DeviceHooks(ID3D10Device *device)
{
	hooks::InstallVirtualFunctionHook("ID3D10Device::RSSetScissorRects", device, &ID3D10Device::RSSetScissorRects, &ID3D10Device_RSSetScissorRects);
}

void VR_EnablePresent(bool enabled)
{
	presentEnabled = enabled;
}

void VR_IgnoreWindowSizeChange(bool ignore)
{
	ignoreWindowSizeChange = ignore;
}

int VR_GetCurrentWindowWidth()
{
	return currentWindowWidth;
}

int VR_GetCurrentWindowHeight()
{
	return currentWindowHeight;
}

void VR_SetCurrentWindowSize(int width, int height)
{
	currentWindowWidth = width;
	currentWindowHeight = height;
}

void VR_RestrictScissor(bool restrict, int x1 = 0, int y1 = 0, int x2 = 0, int y2 = 0)
{
	scissorRestricted = restrict;
	scissorX1Limit = x1;
	scissorY1Limit = y1;
	scissorX2Limit = x2;
	scissorY2Limit = y2;
}
