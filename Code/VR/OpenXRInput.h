#pragma once
#include <openxr/openxr.h>

class OpenXRInput
{
public:
	void Init(XrInstance instance, XrSession session, XrSpace space);
	void Shutdown();

	void Update();

	Matrix34 GetControllerTransform(int hand);

private:
	XrInstance m_instance = nullptr;
	XrSession m_session = nullptr;
	XrSpace m_trackingSpace = nullptr;

	XrActionSet m_ingameSet = nullptr;
	XrAction m_controller[2] = {};
	XrAction m_moveX = nullptr;
	XrAction m_moveY = nullptr;
	XrAction m_rotateYaw = nullptr;
	XrAction m_rotatePitch = nullptr;
	XrAction m_primaryFire = nullptr;
	XrSpace m_gripSpace[2] = {};

	void CreateInputActions();
	void SuggestBindings();
	void CreateBooleanAction(XrActionSet actionSet, XrAction* action, const char* name, const char* description);
	void UpdatePlayerMovement();
};