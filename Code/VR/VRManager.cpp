#include "StdAfx.h"
#include "VRManager.h"
#include <openvr.h>

#include "Cry_Camera.h"
#include "GameCVars.h"
#include "OpenXRManager.h"

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
		m_eyeTextures11[eye].Detach();
	}
	m_hudTexture.Detach();
	m_hudTexture11.Detach();
	m_swapchain.Detach();
	m_device.Detach();
	m_context11.Detach();
	m_device11.Detach();
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
	CryLogAlways("Shutting down VRManager...");

	for (int eye = 0; eye < 2; ++eye)
	{
		m_eyeTextures[eye].Reset();
		m_eyeTextures11[eye].Reset();
	}
	m_hudTexture.Reset();
	m_hudTexture11.Reset();
	m_swapchain.Reset();
	m_device.Reset();
	m_context11.Reset();
	m_device11.Reset();

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

	gXR->AwaitFrame();
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
	CopyBackbufferToTexture(m_eyeTextures[eye].Get());
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
	CopyBackbufferToTexture(m_hudTexture.Get());
}

void VRManager::SetSwapChain(IDXGISwapChain *swapchain)
{
	if (swapchain != m_swapchain.Get())
	{
		m_device.Reset();
	}

	m_swapchain = swapchain;
	if (!m_device)
	  InitDevice(swapchain);
}

void VRManager::FinishFrame(bool didRenderThisFrame)
{
	if (!m_initialized || !m_device || !m_device11)
		return;

	vr::Texture_t texInfo;
	texInfo.eColorSpace = vr::ColorSpace_Auto;
	texInfo.eType = vr::TextureType_DirectX;
	texInfo.handle = (void*)m_hudTexture11.Get();

	ComPtr<IDXGIKeyedMutex> mutex;
	m_hudTexture11->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)mutex.GetAddressOf());
	HRESULT hr = mutex->AcquireSync(1, 1000);
	if (hr != S_OK)
	{
		CryLogAlways("Error while waiting for HUD texture mutex in FinishFrame: %i", hr);
	}
	vr::VROverlay()->SetOverlayTexture(m_hudOverlay, &texInfo);
	mutex->ReleaseSync(0);

	if (!didRenderThisFrame)
		return;

	for (int eye = 0; eye < 2; ++eye)
	{
		vr::Texture_t vrTexData;
		vrTexData.eType = vr::TextureType_DirectX;
		vrTexData.eColorSpace = vr::ColorSpace_Auto;
		vrTexData.handle = m_eyeTextures11[eye].Get();

		// game is currently using symmetric projection, we need to cut off the texture accordingly
		vr::VRTextureBounds_t bounds;
		GetEffectiveRenderLimits(eye, &bounds.uMin, &bounds.uMax, &bounds.vMin, &bounds.vMax);

		ComPtr<IDXGIKeyedMutex> mutex;
		m_eyeTextures11[eye]->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)mutex.GetAddressOf());
		mutex->AcquireSync(1, 1000);

		auto error = vr::VRCompositor()->Submit(eye == 0 ? vr::Eye_Left : vr::Eye_Right, &vrTexData, &bounds);
		if (error != vr::VRCompositorError_None)
		{
			CryLogAlways("Submitting eye texture failed: %i", error);
		}

		mutex->ReleaseSync(0);
	}

	vr::VRCompositor()->PostPresentHandoff();
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

	Matrix34 eyeMat = gXR->GetRenderEyeTransform(eye);
	viewMat = viewMat * eyeMat;

	cam.SetMatrix(viewMat);

	// we don't have obvious access to the projection matrix, and the camera code is written with symmetric projection in mind
	// for now, set up a symmetric FOV and cut off parts of the image during submission
	float vertFov = atanf(m_verticalFov) * 2;
	Vec2i renderSize = GetRenderSize();
	cam.SetFrustum(renderSize.x, renderSize.y, vertFov, cam.GetNearPlane(), cam.GetFarPlane());

	// but we can set up frustum planes for our asymmetric projection, which should help culling accuracy.
	float tanl, tanr, tant, tanb;
	gXR->GetFov(eye, tanl, tanr, tant, tanb);
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

void VRManager::InitDevice(IDXGISwapChain* swapchain)
{
	m_hudTexture.Reset();
	m_eyeTextures[0].Reset();
	m_eyeTextures[1].Reset();

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

	ComPtr<IDXGIDevice> dxgiDevice;
	m_device->QueryInterface(__uuidof(IDXGIDevice), (void**)dxgiDevice.GetAddressOf());
	if (dxgiDevice)
	{
		ComPtr<IDXGIAdapter> d3d10Adapter;
		dxgiDevice->GetAdapter(d3d10Adapter.GetAddressOf());
		DXGI_ADAPTER_DESC desc;
		d3d10Adapter->GetDesc(&desc);
		CryLogAlways("Game is rendering to device %ls", desc.Description);
	}

	CryLogAlways("Creating D3D11 device");
	LUID requiredAdapterLuid;
	D3D_FEATURE_LEVEL requiredLevel;
	gXR->GetD3D11Requirements(&requiredAdapterLuid, &requiredLevel);
	ComPtr<IDXGIFactory1> factory;
	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)factory.GetAddressOf());
	if (hr != S_OK)
	{
		CryLogAlways("Failed to create DXGI factory: %i", hr);
		return;
	}
	ComPtr<IDXGIAdapter1> adapter;
	for (UINT idx = 0; factory->EnumAdapters1(idx, adapter.ReleaseAndGetAddressOf()) == S_OK; ++idx)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		if (desc.AdapterLuid.HighPart == requiredAdapterLuid.HighPart && desc.AdapterLuid.LowPart == requiredAdapterLuid.LowPart)
		{
			CryLogAlways("Found adapter for XR rendering: %ls", desc.Description);
			break;
		}
	}

	hr = D3D11CreateDevice(adapter.Get(), adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr,
		D3D11_CREATE_DEVICE_SINGLETHREADED, &requiredLevel, 1, D3D11_SDK_VERSION,
		m_device11.ReleaseAndGetAddressOf(), nullptr, m_context11.ReleaseAndGetAddressOf());
	if (hr != S_OK)
	{
		CryLogAlways("Failed to create D3D11 device: %i", hr);
	}

	gXR->CreateSession(m_device11.Get());
}

void VRManager::CreateEyeTexture(int eye)
{
	Vec2i size = GetRenderSize();
	CryLogAlways("Creating eye texture %i: %i x %i", eye, size.x, size.y);
	CreateSharedTexture(m_eyeTextures[eye], m_eyeTextures11[eye], size.x, size.y);
}

void VRManager::CreateHUDTexture()
{
	Vec2i size = GetRenderSize();
	CryLogAlways("Creating HUD texture: %i x %i", size.x, size.y);
	CreateSharedTexture(m_hudTexture, m_hudTexture11, size.x, size.y);
}

void VRManager::CreateSharedTexture(ComPtr<ID3D10Texture2D> &texture, ComPtr<ID3D11Texture2D> &texture11, int width, int height)
{
	if (!m_device || !m_device11)
		return;

	D3D10_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.ArraySize = 1;
	desc.MipLevels = 1;
	desc.Usage = D3D10_USAGE_DEFAULT;
	desc.BindFlags = 0;
	desc.MiscFlags = D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX;
	HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, texture.ReleaseAndGetAddressOf());
	if (hr != S_OK)
	{
		CryLogAlways("D3D10 CreateTexture2D failed: %i", hr);
		return;
	}

	CryLogAlways("Sharing texture with D3D11...");
	ComPtr<IDXGIResource> dxgiRes;
	texture->QueryInterface(__uuidof(IDXGIResource), (void**)dxgiRes.GetAddressOf());
	HANDLE handle;
	dxgiRes->GetSharedHandle(&handle);
	hr = m_device11->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), (void**)texture11.ReleaseAndGetAddressOf());
	if (hr != S_OK)
	{
		CryLogAlways("D3D11 OpenSharedResource failed: %i", hr);
	}
}

void VRManager::CopyBackbufferToTexture(ID3D10Texture2D *target)
{
	// acquire and copy the current swap chain buffer to the HUD texture
	ComPtr<ID3D10Texture2D> backbuffer;
	m_swapchain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void**)backbuffer.GetAddressOf());
	if (!backbuffer)
	{
		CryLogAlways("Error: failed to acquire current swapchain buffer");
		return;
	}

	ComPtr<IDXGIKeyedMutex> mutex;
	target->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)mutex.GetAddressOf());
	HRESULT hr = mutex->AcquireSync(0, 1000);
	if (hr != S_OK)
	{
		CryLogAlways("Error while waiting for mutex during copy: %i", hr);
	}

	D3D10_TEXTURE2D_DESC rtDesc;
	backbuffer->GetDesc(&rtDesc);
	if (rtDesc.SampleDesc.Count > 1)
	{
		m_device->ResolveSubresource(target, 0, backbuffer.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
	}
	else
	{
		m_device->CopyResource(target, backbuffer.Get());
	}

	mutex->ReleaseSync(1);
}
