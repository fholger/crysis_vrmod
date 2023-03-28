#include "StdAfx.h"
#include "Hooks.h"
#include "VRManager.h"
#include <d3d10misc.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <WinUser.h>

using Microsoft::WRL::ComPtr;

HRESULT IDXGISwapChain_Present(IDXGISwapChain *pSelf, UINT SyncInterval, UINT Flags)
{
	HRESULT hr = hooks::CallOriginal(IDXGISwapChain_Present)(pSelf, SyncInterval, Flags);
	gVR->FinishFrame(pSelf);
	return hr;
}

HRESULT IDXGISwapChain_ResizeTarget(IDXGISwapChain *pSelf, const DXGI_MODE_DESC *pNewTargetParameters)
{
	CryLogAlways("ResizeTarget called for %ul", pSelf);
	return hooks::CallOriginal(IDXGISwapChain_ResizeTarget)(pSelf, pNewTargetParameters);
}

HRESULT IDXGISwapChain_ResizeBuffers(IDXGISwapChain *pSelf, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	CryLogAlways("ResizeBuffers called for %ul: %d x %d", pSelf, Width, Height);
	return hooks::CallOriginal(IDXGISwapChain_ResizeBuffers)(pSelf, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

BOOL Hook_SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int  X, int  Y, int  cx, int  cy, UINT uFlags)
{
	CryLogAlways("SetWindowPos called for %ul: (%i, %i) -> (%i, %i)", hWnd, X, Y, cx, cy);
	return hooks::CallOriginal(Hook_SetWindowPos)(hWnd, hWndInsertAfter, 0, 0, cx, cy, uFlags);
}

void InitD3D10Hooks()
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