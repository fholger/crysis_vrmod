#pragma once
#include <wrl/client.h>
#include <openvr.h>
#include <d3d9.h>

#include "HUD/HUD.h"

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

	void SetDevice(IDirect3DDevice9Ex *device);
	void FinishFrame();

	Vec2i GetRenderSize() const;

	void ModifyViewCamera(int eye, CCamera& cam);

	void GetEffectiveRenderLimits(int eye, float* left, float* right, float* top, float* bottom);

private:
	bool m_initialized = false;
	ComPtr<IDirect3DDevice9Ex> m_device;
	ComPtr<IDirect3DTexture9> m_hudTexture;
	ComPtr<IDirect3DTexture9> m_eyeTextures[2];
	vr::TrackedDevicePose_t m_headPose;
	vr::VROverlayHandle_t m_hudOverlay;
	float m_verticalFov;
	float m_horizontalFov;
	float m_vertRenderScale;
	float m_horzRenderScale;
	float m_prevViewYaw = 0;

	void InitDevice(IDirect3DDevice9Ex* device);
	void CreateEyeTexture(int eye);
	void CreateHUDTexture();
};

extern VRManager* gVR;
