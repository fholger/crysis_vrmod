#include "StdAfx.h"
#include "VRManager.h"
#include <openvr.h>
#include "Cry_Camera.h"
#include "GameCVars.h"
#include <d3d9_interfaces.h>

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
	transform.m[1][3] = 0;
	transform.m[2][3] = -2.f;
	vr::VROverlay()->SetOverlayTransformAbsolute(m_hudOverlay, vr::TrackingUniverseSeated, &transform);
	vr::VROverlay()->ShowOverlay(m_hudOverlay);

	float ll, lr, lt, lb, rl, rr, rt, rb;
	vr::VRSystem()->GetProjectionRaw(vr::Eye_Left, &ll, &lr, &lt, &lb);
	vr::VRSystem()->GetProjectionRaw(vr::Eye_Right, &rl, &rr, &rt, &rb);
	CryLogAlways(" Left eye - l: %.2f  r: %.2f  t: %.2f  b: %.2f", ll, lr, lt, lb);
	CryLogAlways("Right eye - l: %.2f  r: %.2f  t: %.2f  b: %.2f", rl, rr, rt, rb);
	m_verticalFov = max(max(fabsf(lt), fabsf(lb)), max(fabsf(rt), fabsf(rb)));
	m_horizontalFov = max(max(fabsf(ll), fabsf(lr)), max(fabsf(rl), fabsf(rr)));
	m_vertRenderScale = 2.f * m_verticalFov / min(fabsf(lt) + fabsf(lb), fabsf(rt) + fabsf(rb));
	CryLogAlways("VR vert fov: %.2f  horz fov: %.2f  vert scale: %.2f", m_verticalFov, m_horizontalFov, m_vertRenderScale);

	m_initialized = true;
	return true;
}

void VRManager::Shutdown()
{
	m_device.Reset();

	if (!m_initialized)
		return;

	vr::VROverlay()->DestroyOverlay(m_hudOverlay);
	vr::VR_Shutdown();
	m_initialized = false;
}

void VRManager::AwaitFrame()
{
	if (!m_initialized || !m_device)
		return;

	ComPtr<ID3D9VkInteropDevice> vkDevice;
	m_device->QueryInterface(__uuidof(ID3D9VkInteropDevice), (void**)vkDevice.GetAddressOf());
	vkDevice->LockSubmissionQueue();
	vr::VRCompositor()->WaitGetPoses(&m_headPose, 1, nullptr, 0);
	vkDevice->ReleaseSubmissionQueue();
}

void VRManager::CaptureEye(int eye)
{
	if (!m_device)
		return;

	if (!m_eyeTextures[eye])
	{
		CreateEyeTexture(eye);
		if (!m_eyeTextures[eye])
			return;
	}

	D3DSURFACE_DESC desc;
	m_eyeTextures[eye]->GetLevelDesc(0, &desc);
	Vec2i expectedSize = GetRenderSize();
	if (desc.Width != expectedSize.x || desc.Height != expectedSize.y)
	{
		// recreate with new resolution
		CreateEyeTexture(eye);
		if (!m_eyeTextures[eye])
			return;
	}

	// acquire and copy the current swap chain buffer to the eye texture
	ComPtr<IDirect3DSurface9> backBuffer;
	m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, backBuffer.GetAddressOf());
	ComPtr<IDirect3DSurface9> texSurface;
	m_eyeTextures[eye]->GetSurfaceLevel(0, texSurface.GetAddressOf());
	HRESULT hr = m_device->StretchRect(backBuffer.Get(), nullptr, texSurface.Get(), nullptr, D3DTEXF_POINT);
	if (hr != S_OK)
	{
		CryLogAlways("ERROR: Capturing HUD failed: %i", hr);
	}
}

void VRManager::CaptureHUD()
{
	if (!m_device)
		return;

	if (!m_hudTexture)
	{
		CreateHUDTexture();
		if (!m_hudTexture)
			return;
	}

	D3DSURFACE_DESC desc;
	m_hudTexture->GetLevelDesc(0, &desc);
	Vec2i expectedSize = GetRenderSize();
	if (desc.Width != expectedSize.x || desc.Height != expectedSize.y)
	{
		// recreate with new resolution
		CreateHUDTexture();
		if (!m_hudTexture)
			return;
	}

	// acquire and copy the current back buffer to the HUD texture
	ComPtr<IDirect3DSurface9> backBuffer;
	m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, backBuffer.GetAddressOf());
	ComPtr<IDirect3DSurface9> texSurface;
	m_hudTexture->GetSurfaceLevel(0, texSurface.GetAddressOf());
	HRESULT hr = m_device->StretchRect(backBuffer.Get(), nullptr, texSurface.Get(), nullptr, D3DTEXF_POINT);
	if (hr != S_OK)
	{
		CryLogAlways("ERROR: Capturing HUD failed: %i", hr);
	}

	if (!m_initialized)
		return;

	vr::Texture_t texInfo;
	texInfo.eColorSpace = vr::ColorSpace_Auto;
	texInfo.eType = vr::TextureType_DirectX;
	texInfo.handle = (void*)m_hudTexture.Get();
	//vr::VROverlay()->SetOverlayTexture(m_hudOverlay, &texInfo);
}

void VRManager::SetDevice(IDirect3DDevice9Ex *device)
{
	if (device != m_device.Get())
		InitDevice(device);
}

void VRManager::FinishFrame()
{
	if (!m_initialized || !m_device || !m_eyeTextures[0] || !m_eyeTextures[1])
		return;

	ComPtr<ID3D9VkInteropDevice> vkDevice;
	m_device->QueryInterface(__uuidof(ID3D9VkInteropDevice), (void**)vkDevice.GetAddressOf());

  vr::VRVulkanTextureData_t vkTexData[2];
	VkImageLayout origLayout[2];
	ComPtr<ID3D9VkInteropTexture> vkTex[2];
	VkImageSubresourceRange range;
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseMipLevel = 0;
	range.levelCount = 1;
	range.baseArrayLayer = 0;
	range.layerCount = 1;

	for (int eye = 0; eye < 2; ++eye)
	{
		m_eyeTextures[eye]->QueryInterface(__uuidof(ID3D9VkInteropTexture), (void**)vkTex[eye].GetAddressOf());
		vr::VRVulkanTextureData_t vkData;
		VkImage image;
		VkImageCreateInfo createInfo;
		vkTex[eye]->GetVulkanImageInfo(&image, &origLayout[eye], &createInfo);
		vkDevice->TransitionTextureLayout(vkTex[eye].Get(), &range, origLayout[eye], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		vkTexData[eye].m_nFormat = createInfo.format;
		vkTexData[eye].m_nWidth = createInfo.extent.width;
		vkTexData[eye].m_nHeight = createInfo.extent.height;
		vkTexData[eye].m_nImage = (uint64_t)image;
		vkTexData[eye].m_nSampleCount = 1;
		vkDevice->GetSubmissionQueue(&vkTexData[eye].m_pQueue, nullptr, &vkTexData[eye].m_nQueueFamilyIndex);
		vkDevice->GetVulkanHandles(&vkTexData[eye].m_pInstance, &vkTexData[eye].m_pPhysicalDevice, &vkTexData[eye].m_pDevice);
  }

	vkDevice->FlushRenderingCommands();
	vkDevice->LockSubmissionQueue();

  for (int eye = 0; eye < 2; ++eye) 
	{
		// game is currently using symmetric projection, we need to cut off the texture accordingly
		vr::VRTextureBounds_t bounds;
		GetEffectiveRenderLimits(eye, &bounds.uMin, &bounds.uMax, &bounds.vMin, &bounds.vMax);

		vr::Texture_t vrTexData;
		vrTexData.eColorSpace = vr::ColorSpace_Auto;
		vrTexData.eType = vr::TextureType_Vulkan;
		vrTexData.handle = &vkTexData[eye];

		auto error = vr::VRCompositor()->Submit(eye == 0 ? vr::Eye_Left : vr::Eye_Right, &vrTexData, &bounds);
		if (error != vr::VRCompositorError_None)
		{
			CryLogAlways("Submitting eye texture failed: %i", error);
		}

	}

	vr::VRCompositor()->PostPresentHandoff();
	vkDevice->ReleaseSubmissionQueue();

	for (int eye = 0; eye < 2; ++eye)
	{
		vkDevice->TransitionTextureLayout(vkTex[eye].Get(), &range, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, origLayout[eye]);
	}
}

Vec2i VRManager::GetRenderSize() const
{
	if (!m_initialized)
		return Vec2i(1280, 800);

	uint32_t width, height;
	vr::VRSystem()->GetRecommendedRenderTargetSize(&width, &height);
	height *= m_vertRenderScale;
	width = height * m_horizontalFov / m_verticalFov;
	return Vec2i(width, height);
}

void VRManager::ModifyViewCamera(int eye, CCamera& cam)
{
	if (IsEquivalent(cam.GetPosition(), Vec3(0, 0, 0), VEC_EPSILON))
	{
		// no valid camera set, leave it
		return;
	}

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

	if (eye == 0)
	{
		// manage the aiming deadzone in which the camera should not be rotated
		float yawDiff = angles.z - m_prevViewYaw;
		if (yawDiff < -g_PI)
			yawDiff += 2 * g_PI;
		else if (yawDiff > g_PI)
			yawDiff -= 2 * g_PI;

		float maxDiff = g_pGameCVars->vr_yaw_deadzone_angle * g_PI / 180.f;
		if (yawDiff > maxDiff)
			m_prevViewYaw += yawDiff - maxDiff;
		if (yawDiff < -maxDiff)
			m_prevViewYaw += yawDiff + maxDiff;
		if (m_prevViewYaw > g_PI)
			m_prevViewYaw -= 2*g_PI;
		if (m_prevViewYaw < -g_PI)
			m_prevViewYaw += 2*g_PI;

		CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
		if (pPlayer && pPlayer->GetLinkedVehicle())
		{
			// don't use this while in a vehicle, it feels off
			m_prevViewYaw = angles.z;
		}
	}
	angles.z = m_prevViewYaw;

	Matrix34 viewMat;
	viewMat.SetRotationXYZ(angles, position);

	vr::HmdMatrix34_t eyeMatVR = vr::VRSystem()->GetEyeToHeadTransform(eye == 0 ? vr::Eye_Left : vr::Eye_Right);
	Matrix34 eyeMat = OpenVRToCrysis(eyeMatVR);
	Matrix34 headMat = OpenVRToCrysis(m_headPose.mDeviceToAbsoluteTracking);
	viewMat = viewMat * headMat * eyeMat;

	cam.SetMatrix(viewMat);

	// we don't have obvious access to the projection matrix, and the camera code is written with symmetric projection in mind
	// for now, set up a symmetric FOV and cut off parts of the image during submission
	float vertFov = atanf(m_verticalFov) * 2;
	Vec2i renderSize = GetRenderSize();
	cam.SetFrustum(renderSize.x, renderSize.y, vertFov, cam.GetNearPlane(), cam.GetFarPlane());

	// but we can set up frustum planes for our asymmetric projection, which should help culling accuracy.
	float tanl, tanr, tant, tanb;
	vr::VRSystem()->GetProjectionRaw(eye == 0 ? vr::Eye_Left : vr::Eye_Right, &tanl, &tanr, &tant, &tanb);
	cam.UpdateFrustumFromVRRaw(tanl, tanr, -tanb, -tant);
}

void VRManager::GetEffectiveRenderLimits(int eye, float* left, float* right, float* top, float* bottom)
{
	float l, r, t, b;
	vr::VRSystem()->GetProjectionRaw(eye == 0 ? vr::Eye_Left : vr::Eye_Right, &l, &r, &t, &b);
	*left = 0.5f + 0.5f * l / m_horizontalFov;
	*right = 0.5f + 0.5f * r / m_horizontalFov;
	*top = 0.5f - 0.5f * b / m_verticalFov;
	*bottom = 0.5f - 0.5f * t / m_verticalFov;
}

void VRManager::InitDevice(IDirect3DDevice9Ex* device)
{
	m_hudTexture.Reset();
	m_eyeTextures[0].Reset();
	m_eyeTextures[1].Reset();

	CryLogAlways("Acquiring device...");
	m_device = device;

	//VR_InitD3D10DeviceHooks(m_device.Get());
}

void VRManager::CreateEyeTexture(int eye)
{
	if (!m_device)
		return;

	Vec2i size = GetRenderSize();
	CryLogAlways("Creating eye texture %i: %i x %i", eye, size.x, size.y);
	HRESULT hr = m_device->CreateTexture(size.x, size.y, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, m_eyeTextures[eye].ReleaseAndGetAddressOf(), nullptr);
	CryLogAlways("CreateTexture2D return code: %i", hr);
}

void VRManager::CreateHUDTexture()
{
	if (!m_device)
		return;

	Vec2i size = GetRenderSize();
	CryLogAlways("Creating HUD texture: %i x %i", size.x, size.y);
	HRESULT hr = m_device->CreateTexture(size.x, size.y, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, m_hudTexture.ReleaseAndGetAddressOf(), nullptr);
	CryLogAlways("CreateRenderTarget return code: %i", hr);
}
