#pragma once
#include <openxr/openxr.h>

class OpenXRManager
{
public:
	bool Init();
	void Shutdown();

private:
	XrInstance m_instance = nullptr;

	bool CreateInstance();
};

extern OpenXRManager *gXR;
