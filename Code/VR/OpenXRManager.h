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

	bool CreateInstance();
	void HandleSessionStateChange(XrEventDataSessionStateChanged* event);
};

extern OpenXRManager *gXR;
