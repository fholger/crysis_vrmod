#include "StdAfx.h"
#define XR_USE_PLATFORM_WIN32 1
#define XR_USE_GRAPHICS_API_D3D11 1
#include <d3d11.h>
#include <wrl/client.h>
#include <openxr/openxr_platform.h>
#include "OpenXRRuntime.h"

#include "GameCVars.h"
#include "optick.h"
#include "VRManager.h"
#include "VRRenderer.h"
#include "Menus/FlashMenuObject.h"

OpenXRRuntime g_xrRuntime;
OpenXRRuntime *gXR = &g_xrRuntime;

using Microsoft::WRL::ComPtr;

const int OpenXRInput::MOUSE_SAMPLE_COUNT;


bool XR_CheckResult(XrResult result, const char *description, XrInstance instance = XR_NULL_HANDLE)
{
	if ( XR_SUCCEEDED( result ) ) {
		return true;
	}

	char resultString[XR_MAX_RESULT_STRING_SIZE];
	xrResultToString( instance, result, resultString );
	CryLogAlways("XR %s failed: %s (%i)", description, resultString, result);
	return false;
}

// OpenXR: x = right, y = up, -z = forward
// Crysis: x = right, y = forward, z = up
// both use the metric system, i.e. 1 unit = 1m
Matrix34 OpenXRToCrysis(const XrQuaternionf& orientation, const XrVector3f& position)
{
	Vec3 pos(position.x, position.y, position.z);
	Quat rot(orientation.w, orientation.x, orientation.y, orientation.z);
	Matrix34 xr(Vec3(1, 1, 1), rot, pos);

	Matrix34 m;
	m.m00 = xr.m00;
	m.m01 = -xr.m02;
	m.m02 = xr.m01;
	m.m03 = xr.m03;
	m.m10 = -xr.m20;
	m.m11 = xr.m22;
	m.m12 = -xr.m21;
	m.m13 = -xr.m23;
	m.m20 = xr.m10;
	m.m21 = -xr.m12;
	m.m22 = xr.m11;
	m.m23 = xr.m13;
	return m;
}

Vec3 OpenXRToCrysis(const XrVector3f& position)
{
	Vec3 pos(position.x, -position.z, position.y);
	return pos;
}

XrPosef CrysisToOpenXR(const Matrix34& transform)
{
	Matrix34 m;
	m.m00 = transform.m00;
	m.m01 = transform.m02;
	m.m02 = -transform.m01;
	m.m03 = transform.m03;
	m.m10 = transform.m20;
	m.m11 = transform.m22;
	m.m12 = -transform.m21;
	m.m13 = transform.m23;
	m.m20 = -transform.m10;
	m.m21 = -transform.m12;
	m.m22 = transform.m11;
	m.m23 = -transform.m13;

	Vec3 pos = m.GetTranslation();
	Quat rot = GetQuatFromMat33((Matrix33)m);
	XrPosef pose;
	pose.position.x = pos.x;
	pose.position.y = pos.y;
	pose.position.z = pos.z;
	pose.orientation.w = rot.w;
	pose.orientation.x = rot.v.x;
	pose.orientation.y = rot.v.y;
	pose.orientation.z = rot.v.z;
	return pose;
}

namespace
{

	bool XR_KHR_D3D11_enable_available = false;
	bool XR_EXT_debug_utils_available = false;
	bool XR_EXT_hp_mixed_reality_controller_available = false;

#define XR_DECLARE_FN_PTR(name) PFN_##name name = nullptr
	XR_DECLARE_FN_PTR(xrGetD3D11GraphicsRequirementsKHR);
	XR_DECLARE_FN_PTR(xrCreateDebugUtilsMessengerEXT);

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
		if (!XR_CheckResult(result, "enumerating API layers", XR_NULL_HANDLE))
			return;

		std::vector<XrApiLayerProperties> layerProperties (layersCount);
		for (auto &layer : layerProperties)
		{
			layer.type = XR_TYPE_API_LAYER_PROPERTIES;
			layer.next = nullptr;
		}
		result = xrEnumerateApiLayerProperties(layerProperties.size(), &layersCount, layerProperties.data());
		if (!XR_CheckResult(result, "enumerating API layers", XR_NULL_HANDLE))
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
		//XR_LOAD_FN_PTR(xrCreateDebugUtilsMessengerEXT);
	}

	XrBool32 XRAPI_PTR XR_DebugMessengerCallback(
			XrDebugUtilsMessageSeverityFlagsEXT              messageSeverity,
			XrDebugUtilsMessageTypeFlagsEXT                  messageTypes,
			const XrDebugUtilsMessengerCallbackDataEXT*      callbackData,
			void*                                            userData) {
		CryLogAlways("XR in %s: %s", callbackData->functionName, callbackData->message);
		return XR_TRUE;
	}

	void XR_SetupDebugMessenger(XrInstance instance)
	{
		static XrDebugUtilsMessengerEXT debugMessenger = XR_NULL_HANDLE;

		XrDebugUtilsMessengerCreateInfoEXT createInfo{ XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
		createInfo.messageSeverities = 0xffffffff;
		createInfo.messageTypes = 0xffffffff;
		createInfo.userCallback = &XR_DebugMessengerCallback;
		xrCreateDebugUtilsMessengerEXT(instance, &createInfo, &debugMessenger);
	}
}

bool OpenXRRuntime::Init()
{
	if (!CreateInstance())
		return false;

	memset(&m_hudPose, 0, sizeof(m_hudPose));
	m_hudPose.orientation.w = 1;
	m_hudPose.position.z = -2.f;

	return true;
}

void OpenXRRuntime::Shutdown()
{
	m_input.Shutdown();

	xrDestroySwapchain(m_stereoSwapchain);
	m_stereoSwapchain = XR_NULL_HANDLE;
	m_stereoImages.clear();
	xrDestroySwapchain(m_hudSwapchain);
	m_hudSwapchain = XR_NULL_HANDLE;
	xrDestroySpace(m_space);
	m_space = XR_NULL_HANDLE;
	xrDestroySession(m_session);
	m_session = XR_NULL_HANDLE;
	xrDestroyInstance(m_instance);
	m_instance = XR_NULL_HANDLE;
}

void OpenXRRuntime::GetD3D11Requirements(LUID* adapterLuid, D3D_FEATURE_LEVEL* minRequiredLevel)
{
	XrGraphicsRequirementsD3D11KHR d3dReqs{};
	d3dReqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR;
	XR_CheckResult(xrGetD3D11GraphicsRequirementsKHR(m_instance, m_system, &d3dReqs), "getting D3D11 requirements", m_instance);
	if (adapterLuid)
		*adapterLuid = d3dReqs.adapterLuid;
	if (minRequiredLevel)
		*minRequiredLevel = d3dReqs.minFeatureLevel;
}

void OpenXRRuntime::CreateSession(ID3D11Device* device)
{
	if (m_session)
	{
		m_input.Shutdown();
		xrDestroySession(m_session);
		m_session = XR_NULL_HANDLE;
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
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1;
	result = xrCreateReferenceSpace(m_session, &spaceCreateInfo, &m_space);
	XR_CheckResult(result, "creating roomscale reference space", m_instance);

	m_input.Init(m_instance, m_session, m_space);
}

void OpenXRRuntime::AwaitFrame()
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
	if (eventData.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING)
		HandleSpaceRecalibration(reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&eventData));

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
	for (int i = 0; i < 2; ++i)
	{
		m_renderViews[i].type = XR_TYPE_VIEW;
		m_renderViews[i].next = nullptr;
	}
	result = xrLocateViews(m_session, &viewLocateInfo, &viewState, 2, &viewCount, m_renderViews);
	XR_CheckResult(result, "getting eye views", m_instance);

	m_posesValid = viewState.viewStateFlags & (XR_VIEW_STATE_POSITION_TRACKED_BIT | XR_VIEW_STATE_ORIENTATION_TRACKED_BIT);
}

void OpenXRRuntime::FinishFrame()
{
	if (!m_frameStarted || !m_sessionActive)
		return;

	XrCompositionLayerProjectionView views[2] = {};
	views[0].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
	views[0].pose = m_renderViews[0].pose;
	views[0].fov = m_renderViews[0].fov;
	views[0].subImage.swapchain = m_stereoSwapchain;
	views[0].subImage.imageRect.offset.x = 0;
	views[0].subImage.imageRect.offset.y = 0;
	views[0].subImage.imageRect.extent.width = m_submittedEyeWidth[0];
	views[0].subImage.imageRect.extent.height = m_submittedEyeHeight[0];
	views[1].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
	views[1].pose = m_renderViews[1].pose;
	views[1].fov = m_renderViews[1].fov;
	views[1].subImage.swapchain = m_stereoSwapchain;
	views[1].subImage.imageRect.offset.x = m_stereoWidth;
	views[1].subImage.imageRect.offset.y = 0;
	views[1].subImage.imageRect.extent.width = m_submittedEyeWidth[1];
	views[1].subImage.imageRect.extent.height = m_submittedEyeHeight[1];

	XrCompositionLayerProjection stereoLayer{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
	stereoLayer.space = m_space;
	stereoLayer.viewCount = 2;
	stereoLayer.views = views;

	XrCompositionLayerQuad cinema3DLayerLeft{ XR_TYPE_COMPOSITION_LAYER_QUAD };
	cinema3DLayerLeft.eyeVisibility = XR_EYE_VISIBILITY_LEFT;
	cinema3DLayerLeft.space = m_space;
	cinema3DLayerLeft.subImage.swapchain = m_stereoSwapchain;
	cinema3DLayerLeft.subImage.imageRect.extent.width = m_stereoWidth;
	cinema3DLayerLeft.subImage.imageRect.extent.height = m_stereoHeight;
	cinema3DLayerLeft.pose = GetHudPose();
	cinema3DLayerLeft.size.width = GetHudWidth();
	cinema3DLayerLeft.size.height = GetHudHeight();
	cinema3DLayerLeft.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
	XrCompositionLayerQuad cinema3DLayerRight{ XR_TYPE_COMPOSITION_LAYER_QUAD };
	cinema3DLayerRight.eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
	cinema3DLayerRight.space = m_space;
	cinema3DLayerRight.subImage.swapchain = m_stereoSwapchain;
	cinema3DLayerRight.subImage.imageRect.offset.x = m_stereoWidth;
	cinema3DLayerRight.subImage.imageRect.extent.width = m_stereoWidth;
	cinema3DLayerRight.subImage.imageRect.extent.height = m_stereoHeight;
	cinema3DLayerRight.pose = GetHudPose();
	cinema3DLayerRight.size.width = GetHudWidth();
	cinema3DLayerRight.size.height = GetHudHeight();
	cinema3DLayerRight.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;

	XrCompositionLayerQuad hudLayer{ XR_TYPE_COMPOSITION_LAYER_QUAD };
	hudLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
	hudLayer.space = m_space;
	hudLayer.subImage.swapchain = m_hudSwapchain;
	hudLayer.subImage.imageRect.extent.width = m_hudWidth;
	hudLayer.subImage.imageRect.extent.height = m_hudHeight;
	hudLayer.pose = GetHudPose();
	hudLayer.size.width = GetHudWidth();
	hudLayer.size.height = GetHudHeight();
	hudLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;

	std::vector<XrCompositionLayerBaseHeader*> layers;
	VRRenderMode renderMode = gVRRenderer->GetRenderMode();
	if (renderMode == RM_VR)
	{
		layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&stereoLayer));
	}
	else if (renderMode == RM_3D)
	{
		layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&cinema3DLayerLeft));
		layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&cinema3DLayerRight));
	}
	if (m_hudVisible)
		layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&hudLayer));

	XrFrameEndInfo endInfo{ XR_TYPE_FRAME_END_INFO };
	endInfo.displayTime = m_predictedDisplayTime;
	endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endInfo.layerCount = layers.size();
	endInfo.layers = layers.data();

	XrResult result = xrEndFrame(m_session, &endInfo);
	XR_CheckResult(result, "submitting frame", m_instance);

	if (m_recalibrationPending && m_recalibrationTime < m_predictedDisplayTime)
	{
		CryLogAlways("XR runtime space recalibration - reset view\n");
		gVR->RecalibrateView();
		m_recalibrationPending = false;
	}
}

void OpenXRRuntime::Update()
{
	m_input.Update();
}

Matrix34 OpenXRRuntime::GetRenderEyeTransform(int eye) const
{
	return OpenXRToCrysis(m_renderViews[eye].pose.orientation, m_renderViews[eye].pose.position);
}

Matrix44 OpenXRRuntime::GetHmdTransform() const
{
	Matrix44 hmdTransform = GetRenderEyeTransform(0);
	hmdTransform = (hmdTransform + GetRenderEyeTransform(1)) * .5f;
	return hmdTransform;
}

void OpenXRRuntime::GetFov(int eye, float& tanl, float& tanr, float& tant, float& tanb) const
{
	tanl = tanf(m_renderViews[eye].fov.angleLeft);
	tanr = tanf(m_renderViews[eye].fov.angleRight);
	tant = tanf(m_renderViews[eye].fov.angleUp);
	tanb = tanf(m_renderViews[eye].fov.angleDown);
}

Vec2i OpenXRRuntime::GetRecommendedRenderSize() const
{
	uint32_t viewCount = 0;
	XrViewConfigurationView views[2] = {};
	views[0].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
	views[1].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
	xrEnumerateViewConfigurationViews(m_instance, m_system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewCount, views);
	float scale = FClamp(g_pGameCVars->vr_resolution_scale, 0.25f, 4.f);
	return Vec2i(views[0].recommendedImageRectWidth * scale, views[0].recommendedImageRectHeight * scale);
}

void OpenXRRuntime::SubmitEyes(ID3D11Texture2D* leftEyeTex, const RectF& leftArea, ID3D11Texture2D* rightEyeTex, const RectF& rightArea)
{
	OPTICK_EVENT();
	if (!m_sessionActive)
		return;

	D3D11_TEXTURE2D_DESC lDesc, rDesc;
	leftEyeTex->GetDesc(&lDesc);
	rightEyeTex->GetDesc(&rDesc);
	m_submittedEyeWidth[0] = lDesc.Width * leftArea.w;
	m_submittedEyeHeight[0] = lDesc.Height * leftArea.h;
	m_submittedEyeWidth[1] = rDesc.Width * rightArea.w;
	m_submittedEyeHeight[1] = rDesc.Height * rightArea.h;
	int width = max(m_submittedEyeWidth[0], m_submittedEyeWidth[1]);
	int height = max(m_submittedEyeHeight[0], m_submittedEyeHeight[1]);

	if (!m_stereoSwapchain || m_stereoWidth != width || m_stereoHeight != height)
	{
		CreateStereoSwapchain(width, height);
		if (!m_stereoSwapchain)
			return;
	}

	ComPtr<ID3D11Device> device;
	leftEyeTex->GetDevice(device.GetAddressOf());
	ComPtr<ID3D11DeviceContext> context;
	device->GetImmediateContext(context.GetAddressOf());

	uint32_t imageIdx;
	XR_CheckResult(xrAcquireSwapchainImage(m_stereoSwapchain, nullptr, &imageIdx), "acquiring swapchain image", m_instance);
	XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
	waitInfo.timeout = 1000000000;
	XR_CheckResult(xrWaitSwapchainImage(m_stereoSwapchain, &waitInfo), "waiting for swapchain image", m_instance);

	bool isVR = gVRRenderer->GetRenderMode() == RM_VR;
	D3D11_BOX rect;
	rect.left = lDesc.Width * leftArea.x;
	rect.top = lDesc.Height * leftArea.y;
	rect.right = rect.left + m_submittedEyeWidth[0];
	rect.bottom = rect.top + m_submittedEyeHeight[0];
	rect.front = 0;
	rect.back = 1;
	context->CopySubresourceRegion(m_stereoImages[imageIdx], 0, 0, 0, 0, leftEyeTex, 0, isVR ? &rect : nullptr);
	rect.left = rDesc.Width * rightArea.x;
	rect.top = rDesc.Height * rightArea.y;
	rect.right = rect.left + m_submittedEyeWidth[1];
	rect.bottom = rect.top + m_submittedEyeHeight[1];
	context->CopySubresourceRegion(m_stereoImages[imageIdx], 0, width, 0, 0, rightEyeTex, 0, isVR ? &rect : nullptr);

	XR_CheckResult(xrReleaseSwapchainImage(m_stereoSwapchain, nullptr), "releasing swapchain image", m_instance);
}

void OpenXRRuntime::SubmitHud(ID3D11Texture2D* hudTex)
{
	OPTICK_EVENT();
	if (!m_sessionActive)
		return;

	D3D11_TEXTURE2D_DESC desc;
	hudTex->GetDesc(&desc);

	if (!m_hudSwapchain || m_hudWidth != desc.Width || m_hudHeight != desc.Height)
	{
		CreateHudSwapchain(desc.Width, desc.Height);
		if (!m_hudSwapchain)
			return;
	}

	ComPtr<ID3D11Device> device;
	hudTex->GetDevice(device.GetAddressOf());
	ComPtr<ID3D11DeviceContext> context;
	device->GetImmediateContext(context.GetAddressOf());

	uint32_t imageIdx;
	XR_CheckResult(xrAcquireSwapchainImage(m_hudSwapchain, nullptr, &imageIdx), "acquiring swapchain image", m_instance);
	XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
	waitInfo.timeout = 1000000000;
	XR_CheckResult(xrWaitSwapchainImage(m_hudSwapchain, &waitInfo), "waiting for swapchain image", m_instance);

	context->CopyResource(m_hudImages[imageIdx], hudTex);

	XR_CheckResult(xrReleaseSwapchainImage(m_hudSwapchain, nullptr), "releasing swapchain image", m_instance);
}

XrPosef OpenXRRuntime::GetHudPose() const
{
	return m_hudPose;
}

void OpenXRRuntime::SetHudPose(const XrPosef& pose)
{
	m_hudPose = pose;
}

void OpenXRRuntime::SetHudSize(float width, float height)
{
	m_hudDisplayWidth = width;
	m_hudDisplayHeight = height;
}

bool OpenXRRuntime::HasReverbG2BindingsExtension() const
{
	return XR_EXT_hp_mixed_reality_controller_available;
}

bool OpenXRRuntime::CreateInstance()
{
	CryLogAlways("Creating OpenXR instance...");
	XR_CheckAvailableExtensions();
	XR_CheckAvailableApiLayers();

	if (!XR_KHR_D3D11_enable_available)
	{
		CryError("Error: XR runtime does not support D3D11");
		return false;
	}

	std::vector<const char*> enabledExtensions;
	enabledExtensions.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
	if (XR_EXT_hp_mixed_reality_controller_available)
		enabledExtensions.push_back(XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME);

	XrInstanceCreateInfo createInfo{ XR_TYPE_INSTANCE_CREATE_INFO };
	createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
	createInfo.applicationInfo.applicationVersion = 1;
	createInfo.applicationInfo.engineVersion = 1;
	strcpy(createInfo.applicationInfo.applicationName, "Crysis VR");
	strcpy(createInfo.applicationInfo.engineName, "CryEngine 2");
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
	if (!XR_CheckResult(result, "getting system", m_instance))
	{
		char message[1024];
		snprintf(message, sizeof(message), "Could not find your headset. Make sure it is running and connected and that your active OpenXR runtime is set correctly. Your current runtime is %s", instanceProperties.runtimeName);
		MessageBoxA(nullptr, message, "No VR headset found", MB_OK);
		CryError(message);
		return false;
	}

	return true;
}

void OpenXRRuntime::HandleSessionStateChange(XrEventDataSessionStateChanged* event)
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
		CryLogAlways("XR session state changed to synchronized");
		m_sessionActive = true;
		break;
	case XR_SESSION_STATE_VISIBLE:
		CryLogAlways("XR session state changed to visible");
		m_sessionActive = true;
		if (m_lastSessionState == XR_SESSION_STATE_FOCUSED)
		{
			// happens when e.g. the SteamVR system menu is opened. In this case we want to pause the game and go to the menu
			SAFE_MENU_FUNC(ShowInGameMenu(true));
		}
		break;
	case XR_SESSION_STATE_FOCUSED:
		CryLogAlways("XR session state changed to focused");
		m_sessionActive = true;
		break;

	case XR_SESSION_STATE_IDLE:
		m_sessionActive = false;
		break;

	case XR_SESSION_STATE_STOPPING:
	case XR_SESSION_STATE_LOSS_PENDING:
		m_sessionActive = false;
		CryLogAlways("XR session lost or stopped");
		result = xrEndSession(m_session);
		XR_CheckResult(result, "ending session", m_instance);
		break;

	case XR_SESSION_STATE_EXITING:
		CryLogAlways("Received quit request from XR runtime, shutting down...");
		gEnv->pSystem->Quit();
		break;
	}

	m_lastSessionState = event->state;
}

void OpenXRRuntime::HandleSpaceRecalibration(XrEventDataReferenceSpaceChangePending* event)
{
	m_recalibrationPending = true;
	m_recalibrationTime = event->changeTime;
}

void OpenXRRuntime::CreateStereoSwapchain(int width, int height)
{
	if (m_stereoSwapchain)
	{
		xrDestroySwapchain(m_stereoSwapchain);
		m_stereoSwapchain = XR_NULL_HANDLE;
		m_stereoWidth = 0;
		m_stereoHeight = 0;
	}

	CryLogAlways("XR: Creating stereo swapchain of size %i x %i", width, height);

	XrSwapchainCreateInfo createInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
	createInfo.width = width * 2;
	createInfo.height = height;
	createInfo.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	createInfo.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
	createInfo.sampleCount = 1;
	createInfo.faceCount = 1;
	createInfo.arraySize = 1;
	createInfo.mipCount = 1;
	XrResult result = xrCreateSwapchain(m_session, &createInfo, &m_stereoSwapchain);
	if (!XR_CheckResult(result, "creating stereo swapchain", m_instance))
		return;
	m_stereoWidth = width;
	m_stereoHeight = height;

	uint32_t imageCount;
	xrEnumerateSwapchainImages(m_stereoSwapchain, 0, &imageCount, nullptr);
	std::vector<XrSwapchainImageD3D11KHR> images (imageCount);
	for (auto &image : images)
	{
		image.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
		image.next = nullptr;
	}
	result = xrEnumerateSwapchainImages(m_stereoSwapchain, images.size(), &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
	XR_CheckResult(result, "getting swapchain images", m_instance);
	m_stereoImages.clear();

	for (const auto& image: images)
	{
		m_stereoImages.push_back(image.texture);
	}
}

void OpenXRRuntime::CreateHudSwapchain(int width, int height)
{
	if (m_hudSwapchain)
	{
		xrDestroySwapchain(m_hudSwapchain);
		m_hudSwapchain = XR_NULL_HANDLE;
		m_hudWidth = 0;
		m_hudHeight = 0;
	}

	CryLogAlways("XR: Creating HUD swapchain of size %i x %i", width, height);

	XrSwapchainCreateInfo createInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
	createInfo.width = width;
	createInfo.height = height;
	createInfo.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	createInfo.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
	createInfo.sampleCount = 1;
	createInfo.faceCount = 1;
	createInfo.arraySize = 1;
	createInfo.mipCount = 1;
	XrResult result = xrCreateSwapchain(m_session, &createInfo, &m_hudSwapchain);
	if (!XR_CheckResult(result, "creating hud swapchain", m_instance))
		return;
	m_hudWidth = width;
	m_hudHeight = height;

	uint32_t imageCount;
	xrEnumerateSwapchainImages(m_hudSwapchain, 0, &imageCount, nullptr);
	std::vector<XrSwapchainImageD3D11KHR> images (imageCount);
	for (auto &image : images)
	{
		image.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
		image.next = nullptr;
	}
	result = xrEnumerateSwapchainImages(m_hudSwapchain, images.size(), &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
	XR_CheckResult(result, "getting swapchain images", m_instance);
	m_hudImages.clear();

	for (const auto& image: images)
	{
		m_hudImages.push_back(image.texture);
	}
}
