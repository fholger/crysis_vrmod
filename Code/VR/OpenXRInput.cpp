#include "StdAfx.h"
#include "OpenXRInput.h"

extern bool XR_CheckResult(XrResult result, const char* description, XrInstance instance = nullptr);

void OpenXRInput::Init(XrInstance instance, XrSession session)
{
	m_instance = instance;
	m_session = session;

	XrActionSetCreateInfo setCreateInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };
	strcpy(setCreateInfo.actionSetName, "ingame");
	strcpy(setCreateInfo.localizedActionSetName, "Ingame");
	XR_CheckResult(xrCreateActionSet(m_instance, &setCreateInfo, &m_ingameSet), "creating ingame action set", m_instance);

	CreateInputActions();
}

void OpenXRInput::Shutdown()
{
	xrDestroyAction(m_primaryFire);
	xrDestroyAction(m_stick[0]);
	xrDestroyAction(m_stick[1]);
	xrDestroyAction(m_controller[0]);
	xrDestroyAction(m_controller[1]);
	xrDestroyActionSet(m_ingameSet);

	m_session = nullptr;
	m_instance = nullptr;
}

void OpenXRInput::Update()
{
	if (!m_session)
		return;

	XrActiveActionSet activeSet{ m_ingameSet, XR_NULL_PATH };
	XrActionsSyncInfo syncInfo{ XR_TYPE_ACTIONS_SYNC_INFO };
	syncInfo.countActiveActionSets = 1;
	syncInfo.activeActionSets = &activeSet;
	XR_CheckResult(xrSyncActions(m_session, &syncInfo), "syncing actions");
}

void OpenXRInput::CreateInputActions()
{
	CreateBooleanAction(m_ingameSet, &m_primaryFire, "primary_fire", "Primary Fire");

	XrActionCreateInfo createInfo{ XR_TYPE_ACTION_CREATE_INFO };
	createInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
	strcpy(createInfo.actionName, "left_controller");
	strcpy(createInfo.localizedActionName, "Left Controller");
	XR_CheckResult(xrCreateAction(m_ingameSet, &createInfo, &m_controller[0]), "creating controller pose action");
	strcpy(createInfo.actionName, "right_controller");
	strcpy(createInfo.localizedActionName, "Right Controller");
	XR_CheckResult(xrCreateAction(m_ingameSet, &createInfo, &m_controller[1]), "creating controller pose action");

	createInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
	strcpy(createInfo.actionName, "left_joystick");
	strcpy(createInfo.localizedActionName, "Left Joystick");
	XR_CheckResult(xrCreateAction(m_ingameSet, &createInfo, &m_stick[0]), "creating thumbstick action");
	strcpy(createInfo.actionName, "right_joystick");
	strcpy(createInfo.localizedActionName, "Right Joystick");
	XR_CheckResult(xrCreateAction(m_ingameSet, &createInfo, &m_stick[1]), "creating thumbstick action");
}

void OpenXRInput::CreateBooleanAction(XrActionSet actionSet, XrAction* action, const char* name, const char* description)
{
	XrActionCreateInfo createInfo{ XR_TYPE_ACTION_CREATE_INFO };
	createInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
	strcpy(createInfo.actionName, name);
	strcpy(createInfo.localizedActionName, description);
	XR_CheckResult(xrCreateAction(actionSet, &createInfo, action), "creating boolean action");
}
