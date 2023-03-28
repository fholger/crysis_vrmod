#pragma once
#include <d3d10.h>
#include <dxgi.h>
#include <wrl/client.h>

#undef GetUserName

using Microsoft::WRL::ComPtr;

class VRManager
{
public:
	bool Init();
	void Shutdown();

	void AwaitFrame();

	void CaptureEye(int eye);

	void FinishFrame(IDXGISwapChain* swapchain);

	Vec2i GetRenderSize() const;

private:
	bool m_initialized = false;
	ComPtr<ID3D10Device> m_device;
	ComPtr<ID3D10Texture2D> m_eyeTextures[2];

	void CreateEyeTexture(int eye);
};

extern VRManager* gVR;
