#pragma once
#include <openxr/openxr.h>
#include "OpenXRInput.h"

struct ID3D11Device;
struct ID3D11Texture2D;
struct _LUID;
enum D3D_FEATURE_LEVEL;

class OpenXRRuntime
{
public:
	bool Init();
	void Shutdown();
	void GetD3D11Requirements(_LUID* adapterLuid, D3D_FEATURE_LEVEL* minRequiredLevel);
	void CreateSession(ID3D11Device* device);
	void AwaitFrame();
	void FinishFrame();

	void Update();

	Matrix34 GetRenderEyeTransform(int eye) const;
	void GetFov(int eye, float& tanl, float& tanr, float& tant, float& tanb) const;
	Vec2i GetRecommendedRenderSize() const;

	void SubmitEyes(ID3D11Texture2D* leftEyeTex, const RectF& leftArea, ID3D11Texture2D* rightEyeTex, const RectF& rightArea);
	void SubmitHud(ID3D11Texture2D* hudTex);

	XrTime GetNextFrameDisplayTime() const { return m_predictedNextFrameDisplayTime; }

	OpenXRInput* GetInput() { return &m_input; }

	XrPosef GetHudPose() const;
	float GetHudWidth() const { return 2.f; }
	float GetHudHeight() const { return 2.f; }

private:
	OpenXRInput m_input;
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
	std::vector<ID3D11Texture2D*> m_stereoImages;
	XrSwapchain m_hudSwapchain = nullptr;
	int m_hudWidth = 0;
	int m_hudHeight = 0;
	std::vector<ID3D11Texture2D*> m_hudImages;

	bool CreateInstance();
	void HandleSessionStateChange(XrEventDataSessionStateChanged* event);
	void CreateStereoSwapchain(int width, int height);
	void CreateHudSwapchain(int width, int height);
};

extern OpenXRRuntime *gXR;
