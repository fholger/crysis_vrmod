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
	xrDestroySession(m_session);
	m_session = nullptr;
	xrDestroyInstance(m_instance);
	m_instance = nullptr;
}

void OpenXRManager::GetD3D11Requirements(LUID* adapterLuid, D3D_FEATURE_LEVEL* minRequiredLevel)
{
	XrGraphicsRequirementsD3D11KHR d3dReqs{};
	d3dReqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR;
	XR_CheckResult(xrGetD3D11GraphicsRequirementsKHR(m_instance, m_system, &d3dReqs), "getting D3D11 requirements", m_instance);
	if (adapterLuid)
		*adapterLuid = d3dReqs.adapterLuid;
	if (minRequiredLevel)
		*minRequiredLevel = d3dReqs.minFeatureLevel;
}

void OpenXRManager::CreateSession(ID3D11Device* device)
{
	if (m_session)
	{
		xrDestroySession(m_session);
		m_session = nullptr;
	}
	XrGraphicsBindingD3D11KHR graphicsBinding{};
	graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
	graphicsBinding.device = device;
	XrSessionCreateInfo createInfo{};
	createInfo.type = XR_TYPE_SESSION_CREATE_INFO;
	createInfo.next = &graphicsBinding;
	createInfo.systemId = m_system;
	XrResult result = xrCreateSession(m_instance, &createInfo, &m_session);
	XR_CheckResult(result, "creating session", m_instance);
	m_sessionActive = false;

	XrReferenceSpaceCreateInfo spaceCreateInfo{};
	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	result = xrCreateReferenceSpace(m_session, &spaceCreateInfo, &m_space);
	XR_CheckResult(result, "creating seated reference space", m_instance);
}

void OpenXRManager::AwaitFrame()
{
	if (!m_session)
		return;

	XrEventDataBuffer eventData{};
	eventData.type = XR_TYPE_EVENT_DATA_BUFFER;
	XrResult result = xrPollEvent(m_instance, &eventData);
	if (!XR_CheckResult(result, "polling event", m_instance))
		return;

	if (eventData.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED)
		HandleSessionStateChange(reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData));

	if (!m_sessionActive)
		return;

	XrFrameState frameState{};
	frameState.type = XR_TYPE_FRAME_STATE;
	xrWaitFrame(m_session, nullptr, &frameState);
	m_shouldRender = frameState.shouldRender;
	m_predictedDisplayTime = frameState.predictedDisplayTime;
	m_predictedNextFrameDisplayTime = m_predictedDisplayTime + frameState.predictedDisplayPeriod;

	result = xrBeginFrame(m_session, nullptr);
	XR_CheckResult(result, "begin frame", m_instance);
	m_frameStarted = true;

	XrViewLocateInfo viewLocateInfo{};
	viewLocateInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
	viewLocateInfo.space = m_space;
	viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	viewLocateInfo.displayTime = m_predictedDisplayTime;
	XrViewState viewState{XR_TYPE_VIEW_STATE};
	uint32_t viewCount = 0;
	result = xrLocateViews(m_session, &viewLocateInfo, &viewState, 2, &viewCount, m_renderViews);
	XR_CheckResult(result, "getting eye views", m_instance);
}

bool OpenXRManager::CreateInstance()
{
	CryLogAlways("Creating OpenXR instance...");
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

	XrSystemGetInfo systemInfo{};
	systemInfo.type = XR_TYPE_SYSTEM_GET_INFO;
	systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	result = xrGetSystem(m_instance, &systemInfo, &m_system);
	return XR_CheckResult(result, "getting system", m_instance);
}

void OpenXRManager::HandleSessionStateChange(XrEventDataSessionStateChanged* event)
{
	XrSessionBeginInfo beginInfo = {
		XR_TYPE_SESSION_BEGIN_INFO,
		nullptr,
		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
	};
	XrResult result;

	switch (event->state)
	{
	case XR_SESSION_STATE_READY:
		CryLogAlways("XR session is ready, beginning session...");
		result = xrBeginSession(m_session, &beginInfo);
		XR_CheckResult(result, "beginning session", m_instance);
		m_sessionActive = true;
		break;

	case XR_SESSION_STATE_SYNCHRONIZED:
	case XR_SESSION_STATE_VISIBLE:
	case XR_SESSION_STATE_FOCUSED:
		CryLogAlways("XR session restored");
		m_sessionActive = true;
		break;

	case XR_SESSION_STATE_IDLE:
		m_sessionActive = false;
		break;

	case XR_SESSION_STATE_STOPPING:
		m_sessionActive = false;
		CryLogAlways("XR session lost or stopped");
		result = xrEndSession(m_session);
		XR_CheckResult(result, "ending session", m_instance);
		break;
	case XR_SESSION_STATE_LOSS_PENDING:
		CryLogAlways("Error: XR session lost");
		Shutdown();
		break;
	}
}

