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
		m_device->CopySubresourceRegion(m_eyeTextures[eye].Get(), 0, 0, 0, 0, rt.Get(), 0, nullptr);
	}
	m_device->Flush();
	m_context->Flush();

	CryLogAlways("Submitting VR eye texture...");
	vr::Texture_t vrTexData;
	vrTexData.eType = vr::TextureType_DirectX;
	vrTexData.eColorSpace = vr::ColorSpace_Auto;
	vrTexData.handle = m_eyeTextures11[eye].Get();
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
		swapchain->GetDevice(__uuidof(ID3D10Device1), (void**)m_device.GetAddressOf());
		if (m_device)
		{
			CryLogAlways("Successfully acquired a D3D 10.1 interface");
		}

		ComPtr<IDXGIDevice> dxgiDevice;
		m_device->QueryInterface(_uuidof(IDXGIDevice), (void**)dxgiDevice.GetAddressOf());
		if (dxgiDevice)
		{
			CryLogAlways("Found DXGI device interface");
			ComPtr<IDXGIAdapter> dxgiAdapter;
			dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf());
			if (!dxgiAdapter)
				CryLogAlways("Could not acquire DXGI adapter");
			HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, m_device11.ReleaseAndGetAddressOf(), nullptr, m_context.ReleaseAndGetAddressOf());
			CryLogAlways("D3D11 creation result: %i", hr);
		}
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

	D3D11_TEXTURE2D_DESC desc11 = {};
	desc11.Width = width;
	desc11.Height = height;
	desc11.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc11.SampleDesc.Count = 1;
	desc11.ArraySize = 1;
	desc11.MipLevels = 1;
	desc11.Usage = D3D11_USAGE_DEFAULT;
	desc11.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	desc11.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
	HRESULT hr = m_device11->CreateTexture2D(&desc11, nullptr, m_eyeTextures11[eye].ReleaseAndGetAddressOf());
	CryLogAlways("D3D11 CreateTexture2D return code: %i", hr);

	ComPtr<IDXGIResource> dxgiRes;
	m_eyeTextures11[eye]->QueryInterface(__uuidof(IDXGIResource), (void**)dxgiRes.GetAddressOf());

	HANDLE handle;
	dxgiRes->GetSharedHandle(&handle);
	hr = m_device->OpenSharedResource(handle, __uuidof(ID3D10Texture2D), (void**)m_eyeTextures[eye].ReleaseAndGetAddressOf());
	CryLogAlways("CreateTexture2D return code: %i", hr);
}
