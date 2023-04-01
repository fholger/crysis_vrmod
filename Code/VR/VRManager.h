#pragma once
#include <d3d10_1.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <openvr.h>

#undef GetUserName

using Microsoft::WRL::ComPtr;

class VRManager
{
public:
	~VRManager();

	bool Init();
	void Shutdown();

	void AwaitFrame();

	void CaptureEye(int eye);
	void CaptureHUD();

	void FinishFrame(IDXGISwapChain *swapchain);

	Vec2i GetRenderSize() const;

	void ModifyViewCamera(int eye, CCamera& cam);

private:
	bool m_initialized = false;
	ComPtr<ID3D10Device1> m_device;
	ComPtr<ID3D10Texture2D> m_eyeTextures[2];
	ComPtr<ID3D10Texture2D> m_hudTexture;
	ComPtr<IDXGISwapChain> m_swapchain;
	vr::TrackedDevicePose_t m_headPose;
	vr::VROverlayHandle_t m_hudOverlay;
	float m_verticalFov;
	float m_horizontalFov;
	float m_vertRenderScale;
	float m_horzRenderScale;

	void InitDevice(IDXGISwapChain* swapchain);
	void CreateEyeTexture(int eye);
	void CreateHUDTexture();
};

extern VRManager* gVR;
