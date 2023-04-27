#pragma once
#include <openxr/openxr.h>

class OpenXRInput
{
public:
	void Init(XrInstance instance, XrSession session);
	void Shutdown();

	void Update();

private:
	XrInstance m_instance = nullptr;
	XrSession m_session = nullptr;

	XrActionSet m_ingameSet = nullptr;
	XrAction m_controller[2] = {};
	XrAction m_stick[2] = {};
	XrAction m_primaryFire = nullptr;

	void CreateInputActions();
	void CreateBooleanAction(XrActionSet actionSet, XrAction* action, const char* name, const char* description);
};