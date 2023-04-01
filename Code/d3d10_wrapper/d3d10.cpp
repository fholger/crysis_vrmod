#include <string>
#include <windows.h>
#include <d3d10_1.h>
#include <MinHook.h>
#include <wrl/client.h>

HINSTANCE mHinst = 0, mHinstDLL = 0;

extern "C" UINT_PTR mProcs[29] = {0};

LPCSTR mImportNames[] = {
  "D3D10CompileEffectFromMemory",
  "D3D10CompileShader",
  "D3D10CreateBlob",
  "D3D10CreateDevice",
  "D3D10CreateDeviceAndSwapChain",
  "D3D10CreateEffectFromMemory",
  "D3D10CreateEffectPoolFromMemory",
  "D3D10CreateStateBlock",
  "D3D10DisassembleEffect",
  "D3D10DisassembleShader",
  "D3D10GetGeometryShaderProfile",
  "D3D10GetInputAndOutputSignatureBlob",
  "D3D10GetInputSignatureBlob",
  "D3D10GetOutputSignatureBlob",
  "D3D10GetPixelShaderProfile",
  "D3D10GetShaderDebugInfo",
  "D3D10GetVersion",
  "D3D10GetVertexShaderProfile",
  "D3D10PreprocessShader",
  "D3D10ReflectShader",
  "D3D10RegisterLayers",
  "D3D10StateBlockMaskDifference",
  "D3D10StateBlockMaskDisableAll",
  "D3D10StateBlockMaskDisableCapture",
  "D3D10StateBlockMaskEnableAll",
  "D3D10StateBlockMaskEnableCapture",
  "D3D10StateBlockMaskGetSetting",
  "D3D10StateBlockMaskIntersect",
  "D3D10StateBlockMaskUnion",
};

LPVOID d3d10CreateDevice_orig;
LPVOID d3d10CreateDeviceAndSwapChain_orig;

void FindAdapter(IDXGIAdapter *adapter, IDXGIAdapter1 **replacement)
{
    if (!adapter)
    {
        *replacement = nullptr;
        return;
    }

	DXGI_ADAPTER_DESC refDesc;
	adapter->GetDesc(&refDesc);

	Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
	CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)factory.GetAddressOf());
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

extern "C" HRESULT WINAPI D3D10CreateDevice_wrapper(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, ID3D10Device **ppDevice)
{
    ID3D10Device1* device;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    FindAdapter(pAdapter, adapter.GetAddressOf());
    HRESULT hr = D3D10CreateDevice1(adapter.Get(), D3D10_DRIVER_TYPE_HARDWARE, nullptr, Flags, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &device);
    auto featureLevel = device->GetFeatureLevel();
    *ppDevice = device;
    return hr;
}

extern "C" HRESULT WINAPI D3D10CreateDeviceAndSwapChain_wrapper(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device **ppDevice)
{
    ID3D10Device1* device;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    FindAdapter(pAdapter, adapter.GetAddressOf());
    HRESULT hr = D3D10CreateDeviceAndSwapChain1(adapter.Get(), D3D10_DRIVER_TYPE_HARDWARE, nullptr, Flags, D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, pSwapChainDesc, ppSwapChain, &device);
    *ppDevice = device;
    return hr;
}

extern "C" void D3D10CompileEffectFromMemory_wrapper();
extern "C" void D3D10CompileShader_wrapper();
extern "C" void D3D10CreateBlob_wrapper();
extern "C" void D3D10CreateEffectFromMemory_wrapper();
extern "C" void D3D10CreateEffectPoolFromMemory_wrapper();
extern "C" void D3D10CreateStateBlock_wrapper();
extern "C" void D3D10DisassembleEffect_wrapper();
extern "C" void D3D10DisassembleShader_wrapper();
extern "C" void D3D10GetGeometryShaderProfile_wrapper();
extern "C" void D3D10GetInputAndOutputSignatureBlob_wrapper();
extern "C" void D3D10GetInputSignatureBlob_wrapper();
extern "C" void D3D10GetOutputSignatureBlob_wrapper();
extern "C" void D3D10GetPixelShaderProfile_wrapper();
extern "C" void D3D10GetShaderDebugInfo_wrapper();
extern "C" void D3D10GetVersion_wrapper();
extern "C" void D3D10GetVertexShaderProfile_wrapper();
extern "C" void D3D10PreprocessShader_wrapper();
extern "C" void D3D10ReflectShader_wrapper();
extern "C" void D3D10RegisterLayers_wrapper();
extern "C" void D3D10StateBlockMaskDifference_wrapper();
extern "C" void D3D10StateBlockMaskDisableAll_wrapper();
extern "C" void D3D10StateBlockMaskDisableCapture_wrapper();
extern "C" void D3D10StateBlockMaskEnableAll_wrapper();
extern "C" void D3D10StateBlockMaskEnableCapture_wrapper();
extern "C" void D3D10StateBlockMaskGetSetting_wrapper();
extern "C" void D3D10StateBlockMaskIntersect_wrapper();
extern "C" void D3D10StateBlockMaskUnion_wrapper();

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  mHinst = hinstDLL;
  if (fdwReason == DLL_PROCESS_ATTACH) {
	wchar_t buf[MAX_PATH + 1];
	GetSystemDirectory(buf, sizeof(buf));
	std::wstring dllPath = buf;
	dllPath += L"\\d3d10.dll";
    mHinstDLL = LoadLibrary(dllPath.c_str());
    if (!mHinstDLL) {
      return FALSE;
    }
    for (int i = 0; i < 29; ++i) {
      mProcs[i] = (UINT_PTR)GetProcAddress(mHinstDLL, mImportNames[i]);
    }
    MH_Initialize();
    MH_CreateHook((LPVOID)mProcs[3], &D3D10CreateDevice_wrapper, &d3d10CreateDevice_orig);
    MH_EnableHook((LPVOID)mProcs[3]);
    MH_CreateHook((LPVOID)mProcs[4], &D3D10CreateDeviceAndSwapChain_wrapper, &d3d10CreateDeviceAndSwapChain_orig);
    MH_EnableHook((LPVOID)mProcs[4]);
  } else if (fdwReason == DLL_PROCESS_DETACH) {
    FreeLibrary(mHinstDLL);
  }
  return TRUE;
}
