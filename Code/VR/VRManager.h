#pragma once
#include <d3d10_1.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#undef GetUserName

using Microsoft::WRL::ComPtr;

class VRManager
{
public:
	~VRManager();

	bool Init();
	void Shutdown();

	void AwaitFrame();

	void CaptureEye(IDXGISwapChain *swapchain);

	void FinishFrame();

	Vec2i GetRenderSize() const;

	void SetCurrentEyeTarget(int eye);

private:
	bool m_initialized = false;
	ComPtr<ID3D10Device1> m_device;
	ComPtr<ID3D11Device> m_device11;
	ComPtr<ID3D11DeviceContext> m_context;
	ComPtr<ID3D10Texture2D> m_eyeTextures[2];
	ComPtr<ID3D11Texture2D> m_eyeTextures11[2];
	int m_currentEye = 0;

	void InitDevice(IDXGISwapChain* swapchain);
	void CreateEyeTexture(int eye);
};

extern VRManager* gVR;
