#pragma once
#include <d3d11.h>
#include <openxr/openxr.h>

class OpenXRManager
{
public:
	bool Init();
	void Shutdown();
	void GetD3D11Requirements(LUID* adapterLuid, D3D_FEATURE_LEVEL* minRequiredLevel);
	void CreateSession(ID3D11Device* device);
	void AwaitFrame();

	Matrix34 GetRenderEyeTransform(int eye);
	void GetFov(int eye, float& tanl, float& tanr, float& tant, float& tanb);

	void SubmitEyes(int width, int height, ID3D11Texture2D* leftEyeTex, int leftX, int leftY,
		ID3D11Texture2D* rightEyTex, int rightX, int rightY);

private:
	XrInstance m_instance = nullptr;
	XrSystemId m_system = 0;
	XrSession m_session = nullptr;
	XrSpace m_space = nullptr;
	bool m_sessionActive = false;
	bool m_frameStarted = false;
	bool m_shouldRender = false;
	XrTime m_predictedDisplayTime = 0;
	XrTime m_predictedNextFrameDisplayTime = 0;
	XrView m_renderViews[2] = {};
	XrSwapchain m_stereoSwapchain = nullptr;
	int m_stereoWidth = 0;
	int m_stereoHeight = 0;
	XrSwapchain m_hudSwapchain = nullptr;
	std::vector<ID3D11Texture2D*> m_stereoImages;
	uint32_t m_currentStereoIndex = 0;

	bool CreateInstance();
	void HandleSessionStateChange(XrEventDataSessionStateChanged* event);
	void CreateStereoSwapchain(int width, int height);
};

extern OpenXRManager *gXR;
