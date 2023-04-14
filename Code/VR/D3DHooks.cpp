#include "StdAfx.h"

#include <d3d10_1.h>
#include <wrl/client.h>

#include "Hooks.h"

using Microsoft::WRL::ComPtr;

ID3D10Device1 *g_latestCreatedDevice = nullptr;
IDXGISwapChain *g_latestCreatedSwapChain = nullptr;


HRESULT IDXGIFactory_CreateSwapChain_wrapper(IDXGIFactory *pSelf, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
	CryLogAlways("DXGI swapchain created");
	HRESULT hr = hooks::CallOriginal(IDXGIFactory_CreateSwapChain_wrapper)(pSelf, pDevice, pDesc, ppSwapChain);
	g_latestCreatedSwapChain = *ppSwapChain;
	return hr;
}

void FindAdapter(IDXGIAdapter *adapter, IDXGIAdapter1 **replacement)
{
	ComPtr<IDXGIFactory1> factory;
	CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)factory.GetAddressOf());

	if (!adapter)
	{
		*replacement = nullptr;
		return;
	}

	DXGI_ADAPTER_DESC refDesc;
	adapter->GetDesc(&refDesc);

	UINT idx = 0;
	while (factory->EnumAdapters1(idx, replacement) != DXGI_ERROR_NOT_FOUND)
	{
		++idx;
		DXGI_ADAPTER_DESC desc;
		(*replacement)->GetDesc(&desc);
		if (desc.AdapterLuid.HighPart == refDesc.AdapterLuid.HighPart && desc.AdapterLuid.LowPart == refDesc.AdapterLuid.LowPart)
		{
			return;
		}
		(*replacement)->Release();
		*replacement = nullptr;
	}
}

HRESULT WINAPI D3D10CreateDevice_wrapper(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, ID3D10Device **ppDevice)
{
	// redirect to D3D10.1
	ID3D10Device1* device;
	ComPtr<IDXGIAdapter1> adapter;
	FindAdapter(pAdapter, adapter.GetAddressOf());
	HRESULT hr = D3D10CreateDevice1(adapter.Get(), D3D10_DRIVER_TYPE_HARDWARE, nullptr, Flags, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &device);
	*ppDevice = device;
	g_latestCreatedDevice = device;
	return hr;
}

HRESULT WINAPI D3D10CreateDeviceAndSwapChain_wrapper(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device **ppDevice)
{
	// redirect to D3D10.1
	ID3D10Device1* device;
	ComPtr<IDXGIAdapter1> adapter;
	FindAdapter(pAdapter, adapter.GetAddressOf());
	HRESULT hr = D3D10CreateDeviceAndSwapChain1(adapter.Get(), D3D10_DRIVER_TYPE_HARDWARE, nullptr, Flags, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, pSwapChainDesc, ppSwapChain, &device);
	*ppDevice = device;
	g_latestCreatedDevice = device;
	g_latestCreatedSwapChain = *ppSwapChain;
	return hr;
}

HRESULT WINAPI CreateDXGIFactory_wrapper(const IID &riid, void **ppFactory)
{
	// redirect to DXGI 1.1
	return CreateDXGIFactory1(__uuidof(IDXGIFactory1), ppFactory);
}

extern "C" bool __declspec(dllexport) CryVRInitD3DHooks()
{
	if (!hooks::Init())
		return false;

	// load system d3d10.dll
	wchar_t buf[MAX_PATH + 1];
	GetSystemDirectoryW(buf, sizeof(buf));
	std::wstring systemPath = buf;
	std::wstring d3d10DllPath = systemPath + L"\\d3d10.dll";
	HMODULE d3d10Dll = LoadLibraryW(d3d10DllPath.c_str());
	if (!d3d10Dll) {
		return false;
	}
	// install hooks to redirect calls to D3D10.1
	hooks::InstallHook("D3D10CreateDevice", GetProcAddress(d3d10Dll, "D3D10CreateDevice"), &D3D10CreateDevice_wrapper);
	hooks::InstallHook("D3D10CreateDeviceAndSwapChain", GetProcAddress(d3d10Dll, "D3D10CreateDeviceAndSwapChain"), &D3D10CreateDeviceAndSwapChain_wrapper);

	// load system dxgi.dll
	std::wstring dxgiDllPath = systemPath + L"\\dxgi.dll";
	HMODULE dxgiDll = LoadLibraryW(dxgiDllPath.c_str());
	if (!dxgiDll) {
		return false;
	}
	hooks::InstallHook("CreateDXGIFactory", GetProcAddress(dxgiDll, "CreateDXGIFactory"), &CreateDXGIFactory_wrapper);

	// install DXGI factory hooks to capture created swapchains
	ComPtr<IDXGIFactory1> factory;
	CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)factory.GetAddressOf());
	hooks::InstallVirtualFunctionHook("IDXGIFactory::CreateSwapChain", (IDXGIFactory*)factory.Get(), &IDXGIFactory::CreateSwapChain, &IDXGIFactory_CreateSwapChain_wrapper);

	return true;
}
