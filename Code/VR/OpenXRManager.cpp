#include "StdAfx.h"
#define XR_USE_PLATFORM_WIN32 1
#define XR_USE_GRAPHICS_API_D3D11 1
#include <d3d11.h>
#include <openxr/openxr_platform.h>
#include "OpenXRManager.h"

OpenXRManager g_xrManager;
OpenXRManager *gXR = &g_xrManager;

namespace
{
	bool XR_KHR_D3D11_enable_available = false;
	bool XR_EXT_debug_utils_available = false;
	bool XR_EXT_hp_mixed_reality_controller_available = false;

#define XR_DECLARE_FN_PTR(name) PFN_##name name = nullptr
	XR_DECLARE_FN_PTR(xrGetD3D11GraphicsRequirementsKHR);

	bool XR_CheckResult(XrResult result, const char *description, XrInstance instance = nullptr)
	{
		if ( XR_SUCCEEDED( result ) ) {
			return true;
		}

		char resultString[XR_MAX_RESULT_STRING_SIZE];
		xrResultToString( instance, result, resultString );
		CryLogAlways("XR %s failed: %s", description, resultString);
		return false;
	}

	void XR_CheckAvailableExtensions()
	{
		XR_KHR_D3D11_enable_available = false;
		XR_EXT_debug_utils_available = false;
		XR_EXT_hp_mixed_reality_controller_available = false;

		uint32_t extensionsCount = 0;
		XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionsCount, nullptr);
		if (!XR_CheckResult(result, "querying number of available extensions"))
			return;

		std::vector<XrExtensionProperties> extensionProperties (extensionsCount);
		for (auto &ext : extensionProperties)
		{
			ext.type = XR_TYPE_EXTENSION_PROPERTIES;
			ext.next = nullptr;
		}
		result = xrEnumerateInstanceExtensionProperties( nullptr, extensionProperties.size(), &extensionsCount, extensionProperties.data() );
		if (!XR_CheckResult( result, "querying available extensions" ))
			return;

		CryLogAlways("XR supported extensions:");
		for ( auto ext : extensionProperties )
		{
			CryLogAlways("  %s", ext.extensionName);

			if (strcmp(ext.extensionName, XR_KHR_D3D11_ENABLE_EXTENSION_NAME) == 0)
			{
				XR_KHR_D3D11_enable_available = true;
			}
			if (strcmp(ext.extensionName, XR_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
			{
				XR_EXT_debug_utils_available = true;
			}
			if (strcmp(ext.extensionName, XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME) == 0)
			{
				XR_EXT_hp_mixed_reality_controller_available = true;
			}
		}
	}

	void XR_CheckAvailableApiLayers()
	{
		uint32_t layersCount = 0;
		XrResult result = xrEnumerateApiLayerProperties(0, &layersCount, nullptr);
		if (!XR_CheckResult(result, "enumerating API layers", nullptr))
			return;

		std::vector<XrApiLayerProperties> layerProperties (layersCount);
		for (auto &layer : layerProperties)
		{
			layer.type = XR_TYPE_API_LAYER_PROPERTIES;
			layer.next = nullptr;
		}
		result = xrEnumerateApiLayerProperties(layerProperties.size(), &layersCount, layerProperties.data());
		if (!XR_CheckResult(result, "enumerating API layers", nullptr))
			return;

		CryLogAlways("XR supported API layers:");
		for ( auto layer : layerProperties )
		{
			CryLogAlways("  %s", layer.layerName);
		}
	}

	void XR_LoadExtensionFunctions(XrInstance instance)
	{
#define XR_LOAD_FN_PTR(name) XR_CheckResult(xrGetInstanceProcAddr(instance, #name, (PFN_xrVoidFunction*)& name), "loading extension function " #name, instance)
		XR_LOAD_FN_PTR(xrGetD3D11GraphicsRequirementsKHR);
	}
}

bool OpenXRManager::Init()
{
	if (!CreateInstance())
		return false;

	return true;
}

void OpenXRManager::Shutdown()
{
	xrDestroyInstance(m_instance);
	m_instance = nullptr;
}

bool OpenXRManager::CreateInstance()
{
	XR_CheckAvailableExtensions();
	XR_CheckAvailableApiLayers();

	if (!XR_KHR_D3D11_enable_available)
	{
		CryLogAlways("Error: XR runtime does not support D3D11");
		return false;
	}

	std::vector<const char*> enabledExtensions;
	enabledExtensions.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
	if (XR_EXT_hp_mixed_reality_controller_available)
		enabledExtensions.push_back(XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME);

	XrInstanceCreateInfo createInfo {};
	createInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
	createInfo.applicationInfo = {
		"Crysis VR",
		1,
		"CryEngine 2",
		0,
		XR_CURRENT_API_VERSION,
	};
	createInfo.enabledExtensionCount = enabledExtensions.size();
	createInfo.enabledExtensionNames = enabledExtensions.data();

	XrResult result = xrCreateInstance(&createInfo, &m_instance);
	if (!XR_CheckResult(result, "creating the instance"))
		return false;

	XrInstanceProperties instanceProperties = {
		XR_TYPE_INSTANCE_PROPERTIES,
		nullptr,
	};
	xrGetInstanceProperties(m_instance, &instanceProperties);
	CryLogAlways("OpenXR runtime: %s (v%d.%d.%d)", instanceProperties.runtimeName,
		XR_VERSION_MAJOR(instanceProperties.runtimeVersion),
		XR_VERSION_MINOR(instanceProperties.runtimeVersion),
		XR_VERSION_PATCH(instanceProperties.runtimeVersion));

	XR_LoadExtensionFunctions(m_instance);

	return true;
}

