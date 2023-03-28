#include <dxgi.h>
#include <string>

namespace
{
	HMODULE dxgiHandle = nullptr;

	typedef HRESULT(*LP_CreateDXGIFactory1)(REFIID, void**);
	typedef HRESULT(*LP_CreateDXGIFactory2)(UINT, REFIID, void**);
	typedef HRESULT(*LP_DXGIDeclareAdapterRemovalSupport)();
	typedef HRESULT(*LP_DXGIGetDebugInterface1)(UINT, REFIID, void**);

	LP_CreateDXGIFactory1 g_pfnCreateDXGIFactory1 = nullptr;
	LP_CreateDXGIFactory2 g_pfnCreateDXGIFactory2 = nullptr;
	LP_DXGIDeclareAdapterRemovalSupport g_pfnDXGIDeclareAdapterRemovalSupport = nullptr;
	LP_DXGIGetDebugInterface1 g_pfnDXGIGetDebugInterface1 = nullptr;

	bool LoadSystemDXGI()
	{
		if (dxgiHandle)
			return true;

		wchar_t buf[MAX_PATH + 1];
		GetSystemDirectory(buf, sizeof(buf));
		std::wstring dllPath = buf;
		dllPath += L"\\dxgi.dll";

		dxgiHandle = LoadLibrary(dllPath.c_str());
		if (!dxgiHandle)
			return false;

		g_pfnCreateDXGIFactory1 = (LP_CreateDXGIFactory1)GetProcAddress(dxgiHandle, "CreateDXGIFactory1");
		g_pfnCreateDXGIFactory2 = (LP_CreateDXGIFactory2)GetProcAddress(dxgiHandle, "CreateDXGIFactory2");
		g_pfnDXGIDeclareAdapterRemovalSupport = (LP_DXGIDeclareAdapterRemovalSupport)GetProcAddress(dxgiHandle, "DXGIDeclareAdapterRemovalSupport");
		g_pfnDXGIGetDebugInterface1 = (LP_DXGIGetDebugInterface1)GetProcAddress(dxgiHandle, "DXGIGetDebugInterface1");

		return true;
	}
}

extern "C" {
	HRESULT __stdcall CreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory)
	{
		if (!LoadSystemDXGI())
		{
			return E_FAIL;
		}

		return g_pfnCreateDXGIFactory2(Flags, riid, ppFactory);
	}

	HRESULT __stdcall CreateDXGIFactory1(REFIID riid, void** ppFactory)
	{
		if (!LoadSystemDXGI())
		{
			return E_FAIL;
		}

		return g_pfnCreateDXGIFactory1(riid, ppFactory);
	}

	HRESULT __stdcall CreateDXGIFactory(REFIID riid, void **ppFactory)
	{
		// redirect to CreateDXGIFactory1
		return CreateDXGIFactory1(__uuidof(IDXGIFactory1), ppFactory);
	}

	HRESULT __stdcall DXGIDeclareAdapterRemovalSupport()
	{
		if (!LoadSystemDXGI())
		{
			return E_FAIL;
		}

		return g_pfnDXGIDeclareAdapterRemovalSupport();
	}

	HRESULT __stdcall DXGIGetDebugInterface1(UINT Flags, REFIID riid, void **pDebug)
	{
		if (!LoadSystemDXGI())
		{
			return E_FAIL;
		}

		return g_pfnDXGIGetDebugInterface1(Flags, riid, pDebug);
	}
}