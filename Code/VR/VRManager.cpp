#include "StdAfx.h"
#include "VRManager.h"
#include <openvr.h>

VRManager s_VRManager;
VRManager* gVR = &s_VRManager;

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

	m_initialized = true;
	return true;
}

void VRManager::Shutdown()
{
	if (!m_initialized)
		return;

	m_eyeTextures[0].Reset();
	m_eyeTextures[1].Reset();
	m_device.Reset();

	vr::VR_Shutdown();
	m_initialized = false;
}

void VRManager::AwaitFrame()
{
	vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);
}

void VRManager::CaptureEye(int eye)
{
	if (!m_initialized || !m_device)
		return;

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

	// acquire and copy the current render target to the eye texture
	ComPtr<ID3D10RenderTargetView> rtv;
	m_device->OMGetRenderTargets(1, rtv.GetAddressOf(), nullptr);
	if (!rtv)
	{
		CryError("Failed getting current render target view");
		return;
	}
	ComPtr<ID3D10Resource> rt;
	rtv->GetResource(rt.GetAddressOf());
	if (!rt)
	{
		CryError("Failed getting current render target texture");
		return;
	}
	D3D10_TEXTURE2D_DESC rtDesc;
	((ID3D10Texture2D*)rt.Get())->GetDesc(&rtDesc);
	if (rtDesc.SampleDesc.Count > 1)
	{
		CryLogAlways("Resolving back buffer...");
		m_device->ResolveSubresource(m_eyeTextures[eye].Get(), 0, rt.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
	}
	else
	{
		CryLogAlways("Copying back buffer...");
		m_device->CopyResource(m_eyeTextures[eye].Get(), rt.Get());
	}

	CryLogAlways("Submitting VR eye texture...");
	vr::Texture_t vrTexData;
	vrTexData.eType = vr::TextureType_DirectX;
	vrTexData.eColorSpace = vr::ColorSpace_Auto;
	vrTexData.handle = m_eyeTextures[eye].Get();
	auto error = vr::VRCompositor()->Submit(eye == 0 ? vr::Eye_Left : vr::Eye_Right, &vrTexData);
	if (error != vr::VRCompositorError_None)
	{
		CryLogAlways("Submitting eye texture failed: %i", error);
	}
}

void VRManager::FinishFrame(IDXGISwapChain* swapchain)
{
	if (!m_initialized)
		return;

	if (!m_device)
	{
		CryLogAlways("Acquiring device...");
		swapchain->GetDevice(__uuidof(ID3D10Device), (void**)m_device.GetAddressOf());
	}

	vr::VRCompositor()->PostPresentHandoff();
}

Vec2i VRManager::GetRenderSize() const
{
	if (!m_initialized)
		return Vec2i(0, 0);

	uint32_t width, height;
	vr::VRSystem()->GetRecommendedRenderTargetSize(&width, &height);
	return Vec2i(width, height);
}

void VRManager::CreateEyeTexture(int eye)
{
	if (!m_initialized || !m_device)
		return;

	uint32_t width, height;
	vr::VRSystem()->GetRecommendedRenderTargetSize(&width, &height);
	CryLogAlways("Creating eye texture %i: %d x %d", eye, width, height);

	D3D10_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.ArraySize = 1;
	desc.MipLevels = 1;
	desc.Usage = D3D10_USAGE_DEFAULT;
	desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	desc.MiscFlags = D3D10_RESOURCE_MISC_SHARED_KEYEDMUTEX;
	HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, m_eyeTextures[eye].ReleaseAndGetAddressOf());
	CryLogAlways("CreateTexture2D return code: %i", hr);
}
