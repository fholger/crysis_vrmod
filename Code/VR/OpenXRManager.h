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

private:
	XrInstance m_instance = nullptr;
	XrSystemId m_system = 0;
	XrSession m_session = nullptr;

	bool CreateInstance();
};

extern OpenXRManager *gXR;
