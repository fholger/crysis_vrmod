#include "StdAfx.h"
#include "VRManager.h"
#include <openvr.h>

#include "Cry_Camera.h"

VRManager s_VRManager;
VRManager* gVR = &s_VRManager;

// OpenVR: x = right, y = up, -z = forward
// Crysis: x = right, y = forward, z = up
Matrix34 OpenVRToCrysis(const vr::HmdMatrix34_t &mat)
{
	Matrix34 m;
	m.m00 = mat.m[0][0];
	m.m01 = -mat.m[0][2];
	m.m02 = mat.m[0][1];
	m.m03 = mat.m[0][3];
	m.m10 = -mat.m[2][0];
	m.m11 = mat.m[2][2];
	m.m12 = -mat.m[2][1];
	m.m13 = -mat.m[2][3];
	m.m20 = mat.m[1][0];
	m.m21 = -mat.m[1][2];
	m.m22 = mat.m[1][1];
	m.m23 = mat.m[1][3];
	return m;
}

VRManager::~VRManager()
{
	// if Shutdown isn't properly called, we will get an infinite hang when trying to dispose of our D3D resources after
	// the game already shut down. So just let go here to avoid that
	for (int eye = 0; eye < 2; ++eye)
	{
		m_eyeTextures[eye].Detach();
	}
	m_hudTexture.Detach();
	m_swapchain.Detach();
	m_device.Detach();
}

bool VRManager::Init()
{
	if (m_initialized)
		return true;

	vr::EVRInitError error;
	vr::VR_Init(&error, vr::VRApplication_Scene);
	if (error != vr::VRInitError_None)
	{
		CryError("Failed to initialize OpenVR: %s", vr::VR_GetVRInitErrorAsEnglishDescription(error));
		return false;
	}

	vr::VRCompositor()->SetTrackingSpace(vr::TrackingUniverseSeated);

	vr::VROverlay()->CreateOverlay("CrysisHud", "Crysis HUD", &m_hudOverlay);
	vr::VROverlay()->SetOverlayWidthInMeters(m_hudOverlay, 2.f);
	vr::HmdMatrix34_t transform;
	memset(&transform, 0, sizeof(vr::HmdMatrix34_t));
	transform.m[0][0] = transform.m[1][1] = transform.m[2][2] = 1;
	transform.m[0][3] = 0;
	transform.m[1][3] = 0.5f;
	transform.m[2][3] = -2.f;
	vr::VROverlay()->SetOverlayTransformAbsolute(m_hudOverlay, vr::TrackingUniverseSeated, &transform);
	vr::VROverlay()->ShowOverlay(m_hudOverlay);

	m_initialized = true;
	return true;
}

void VRManager::Shutdown()
{
	for (int eye = 0; eye < 2; ++eye)
	{
		m_eyeTextures[eye].Reset();
	}
	m_hudTexture.Reset();
	m_swapchain.Reset();
	m_device.Reset();

	if (!m_initialized)
		return;

	vr::VROverlay()->DestroyOverlay(m_hudOverlay);
	vr::VR_Shutdown();
	m_initialized = false;
}

void VRManager::AwaitFrame()
{
	if (!m_initialized)
		return;

	vr::VRCompositor()->WaitGetPoses(&m_headPose, 1, nullptr, 0);
}

void VRManager::CaptureEye(int eye)
{
	if (!m_swapchain)
		return;

	if (!m_device)
		InitDevice(m_swapchain.Get());

	if (!m_eyeTextures[eye])
	{
		CreateEyeTexture(eye);
		if (!m_eyeTextures[eye])
			return;
	}

	D3D10_TEXTURE2D_DESC desc;
	m_eyeTextures[eye]->GetDesc(&desc);
	Vec2i expectedSize = GetRenderSize();
	if (desc.Width != expectedSize.x || desc.Height != expectedSize.y)
	{
		// recreate with new resolution
		CreateEyeTexture(eye);
		if (!m_eyeTextures[eye])
			return;
	}

	// acquire and copy the current swap chain buffer to the eye texture
	ComPtr<ID3D10Texture2D> texture;
	m_swapchain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void**)texture.GetAddressOf());
	if (!texture)
	{
		CryLogAlways("Error: failed to acquire current swapchain buffer");
		return;
	}

	D3D10_TEXTURE2D_DESC rtDesc;
	texture->GetDesc(&rtDesc);
	if (rtDesc.SampleDesc.Count > 1)
	{
		m_device->ResolveSubresource(m_eyeTextures[eye].Get(), 0, texture.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
	}
	else
	{
		//m_device->CopySubresourceRegion(m_eyeTextures[eye].Get(), 0, 0, 0, 0, texture.Get(), 0, nullptr);
		m_device->CopyResource(m_eyeTextures[eye].Get(), texture.Get());
	}
}

void VRManager::CaptureHUD()
{
	if (!m_swapchain)
		return;

	if (!m_device)
		InitDevice(m_swapchain.Get());

	if (!m_hudTexture)
	{
		CreateHUDTexture();
		if (!m_hudTexture)
			return;
	}

	D3D10_TEXTURE2D_DESC desc;
	m_hudTexture->GetDesc(&desc);
	Vec2i expectedSize = GetRenderSize();
	if (desc.Width != expectedSize.x || desc.Height != expectedSize.y)
	{
		// recreate with new resolution
		CreateHUDTexture();
		if (!m_hudTexture)
			return;
	}

	// acquire and copy the current swap chain buffer to the HUD texture
	ComPtr<ID3D10Texture2D> texture;
	m_swapchain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void**)texture.GetAddressOf());
	if (!texture)
	{
		CryLogAlways("Error: failed to acquire current swapchain buffer");
		return;
	}

	D3D10_TEXTURE2D_DESC rtDesc;
	texture->GetDesc(&rtDesc);
	if (rtDesc.SampleDesc.Count > 1)
	{
		m_device->ResolveSubresource(m_hudTexture.Get(), 0, texture.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
	}
	else
	{
		m_device->CopySubresourceRegion(m_hudTexture.Get(), 0, 0, 0, 0, texture.Get(), 0, nullptr);
	}

	if (!m_initialized)
		return;

	vr::Texture_t texInfo;
	texInfo.eColorSpace = vr::ColorSpace_Auto;
	texInfo.eType = vr::TextureType_DirectX;
	texInfo.handle = (void*)m_hudTexture.Get();
	vr::VROverlay()->SetOverlayTexture(m_hudOverlay, &texInfo);
}

void VRManager::FinishFrame(IDXGISwapChain *swapchain)
{
	m_swapchain = swapchain;
	if (!m_initialized || !m_device)
		return;

	Vec2i renderSize = GetRenderSize();

	for (int eye = 0; eye < 2; ++eye)
	{
		vr::Texture_t vrTexData;
		vrTexData.eType = vr::TextureType_DirectX;
		vrTexData.eColorSpace = vr::ColorSpace_Auto;
		vrTexData.handle = m_eyeTextures[eye].Get();

		// game is currently using symmetric projection, we need to cut off the texture accordingly
		float left, right, top, bottom;
		vr::VRSystem()->GetProjectionRaw(eye == 0 ? vr::Eye_Left : vr::Eye_Right, &left, &right, &top, &bottom);
		float vertFov = max(fabsf(top), fabsf(bottom));
		float horzFov = vertFov * renderSize.x / renderSize.y;
		vr::VRTextureBounds_t bounds;
		bounds.uMin = 0.5f + 0.5f * left / horzFov;
		bounds.uMax = 0.5f + 0.5f * right / horzFov;
		bounds.vMin = 0.5f + 0.5f * top / vertFov;
		bounds.vMax = 0.5f + 0.5f * bottom / vertFov;

		auto error = vr::VRCompositor()->Submit(eye == 0 ? vr::Eye_Left : vr::Eye_Right, &vrTexData, &bounds);
		if (error != vr::VRCompositorError_None)
		{
			CryLogAlways("Submitting eye texture failed: %i", error);
		}
	}

	vr::VRCompositor()->PostPresentHandoff();
}

Vec2i VRManager::GetRenderSize() const
{
	if (!m_initialized)
		return Vec2i(1280, 800);

	uint32_t width, height;
	vr::VRSystem()->GetRecommendedRenderTargetSize(&width, &height);
	width *= 1.2;
	return Vec2i(width, height);
}

void VRManager::ModifyViewCamera(int eye, CCamera& cam)
{
	if (!m_initialized)
	{
		if (eye == 1)
		{
			Vec3 pos = cam.GetPosition();
			pos.x += 0.1f;
			cam.SetPosition(pos);
		}
		return;
	}

	Ang3 angles = cam.GetAngles();
	Vec3 position = cam.GetPosition();

	// eliminate pitch and roll
	// switch around, because these functions do not agree on which angle is what...
	angles.z = angles.x;
	angles.y = 0;
	angles.x = 0;

	Matrix34 viewMat;
	viewMat.SetRotationXYZ(angles, position);

	vr::HmdMatrix34_t eyeMatVR = vr::VRSystem()->GetEyeToHeadTransform(eye == 0 ? vr::Eye_Left : vr::Eye_Right);
	Matrix34 eyeMat = OpenVRToCrysis(eyeMatVR);
	Matrix34 headMat = OpenVRToCrysis(m_headPose.mDeviceToAbsoluteTracking);
	viewMat = viewMat * headMat * eyeMat;

	cam.SetMatrix(viewMat);

	// we don't have obvious access to the projection matrix, and the camera code is written with symmetric projection in mind
	// it does set up frustum planes that we could calculate properly for our asymmetric projection, but it is unclear if that
	// would result in the correct projection matrix to be calculated.
	// for now, set up a symmetric FOV and cut off parts of the image during submission
	float left, right, top, bottom;
	vr::VRSystem()->GetProjectionRaw(eye == 0 ? vr::Eye_Left : vr::Eye_Right, &left, &right, &top, &bottom);
	float vertFov = atanf(max(fabsf(top), fabsf(bottom))) * 2;
	cam.SetFrustum(cam.GetViewSurfaceX(), cam.GetViewSurfaceZ(), vertFov, cam.GetNearPlane(), cam.GetFarPlane());
}

void VRManager::InitDevice(IDXGISwapChain* swapchain)
{
	CryLogAlways("Acquiring device...");
	swapchain->GetDevice(__uuidof(ID3D10Device1), (void**)m_device.ReleaseAndGetAddressOf());
	if (!m_device)
	{
		CryLogAlways("Failed to get game's D3D10.1 device!");
		return;
	}
	if (m_device->GetFeatureLevel() != D3D10_FEATURE_LEVEL_10_1)
	{
		CryLogAlways("Device only has feature level %i", m_device->GetFeatureLevel());
	}
}

void VRManager::CreateEyeTexture(int eye)
{
	if (!m_device)
		return;

	Vec2i size = GetRenderSize();
	CryLogAlways("Creating eye texture %i: %i x %i", eye, size.x, size.y);

	D3D10_TEXTURE2D_DESC desc = {};
	desc.Width = size.x;
	desc.Height = size.y;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.ArraySize = 1;
	desc.MipLevels = 1;
	desc.Usage = D3D10_USAGE_DEFAULT;
	desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
	HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, m_eyeTextures[eye].ReleaseAndGetAddressOf());
	CryLogAlways("CreateTexture2D return code: %i", hr);
}

void VRManager::CreateHUDTexture()
{
	if (!m_device)
		return;

	Vec2i size = GetRenderSize();
	CryLogAlways("Creating HUD texture: %i x %i", size.x, size.y);

	D3D10_TEXTURE2D_DESC desc = {};
	desc.Width = size.x;
	desc.Height = size.y;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.ArraySize = 1;
	desc.MipLevels = 1;
	desc.Usage = D3D10_USAGE_DEFAULT;
	desc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
	desc.MiscFlags = D3D10_RESOURCE_MISC_SHARED;
	HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, m_hudTexture.ReleaseAndGetAddressOf());
	CryLogAlways("CreateTexture2D return code: %i", hr);
}
