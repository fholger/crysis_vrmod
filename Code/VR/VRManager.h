#pragma once
#include <d3d10_1.h>
#include <dxgi.h>
#include <d3d11.h>
#include <wrl/client.h>

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

	IDXGISwapChain* GetSwapChain() const { return m_swapchain.Get(); }
	void SetSwapChain(IDXGISwapChain *swapchain);
	void FinishFrame(bool didRenderThisFrame);

	Vec2i GetRenderSize() const;

	void ModifyViewCamera(int eye, CCamera& cam);

	RectF GetEffectiveRenderLimits(int eye);

private:
	bool m_initialized = false;
	ComPtr<ID3D10Device1> m_device;
	ComPtr<IDXGISwapChain> m_swapchain;
	ComPtr<ID3D10Texture2D> m_eyeTextures[2];
	ComPtr<ID3D10Texture2D> m_hudTexture;

	// D3D11 resources for OpenXR submission
	ComPtr<ID3D11Device> m_device11;
	ComPtr<ID3D11DeviceContext> m_context11;
	ComPtr<ID3D11Texture2D> m_eyeTextures11[2];
	ComPtr<ID3D11Texture2D> m_hudTexture11;

	float m_prevViewYaw = 0;

	void InitDevice(IDXGISwapChain* swapchain);
	void CreateEyeTexture(int eye);
	void CreateHUDTexture();
	void CreateSharedTexture(ComPtr<ID3D10Texture2D>& texture, ComPtr<ID3D11Texture2D>& texture11, int width, int height);
	void CopyBackbufferToTexture(ID3D10Texture2D *target);
};

extern VRManager* gVR;
