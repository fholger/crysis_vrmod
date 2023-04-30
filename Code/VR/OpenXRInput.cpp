#include "StdAfx.h"
#include "OpenXRInput.h"

#include "GameActions.h"
#include "IPlayerInput.h"
#include "Player.h"

extern bool XR_CheckResult(XrResult result, const char* description, XrInstance instance = nullptr);

class SuggestedProfileBinding
{
public:
	SuggestedProfileBinding(XrInstance instance) : m_instance(instance) {}

	void AddBinding(XrAction action, const char* binding)
	{
		XrPath bindingPath;
		XR_CheckResult(xrStringToPath(m_instance, binding, &bindingPath), "string to path");
		m_bindings.push_back({ action, bindingPath });
	}

	void SuggestBindings(const char* profile)
	{
		XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
		XR_CheckResult(xrStringToPath(m_instance, profile, &suggestedBindings.interactionProfile), "profile string to path");
		suggestedBindings.suggestedBindings = m_bindings.data();
		suggestedBindings.countSuggestedBindings = m_bindings.size();
		XR_CheckResult(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings), "suggesting bindings");
	}

private:
	XrInstance m_instance;
	std::vector<XrActionSuggestedBinding> m_bindings;
};

void OpenXRInput::Init(XrInstance instance, XrSession session)
{
	m_instance = instance;
	m_session = session;

	XrActionSetCreateInfo setCreateInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };
	strcpy(setCreateInfo.actionSetName, "ingame");
	strcpy(setCreateInfo.localizedActionSetName, "Ingame");
	XR_CheckResult(xrCreateActionSet(m_instance, &setCreateInfo, &m_ingameSet), "creating ingame action set", m_instance);

	CreateInputActions();
	SuggestBindings();

	XrSessionActionSetsAttachInfo attachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
	attachInfo.actionSets = &m_ingameSet;
	attachInfo.countActionSets = 1;
	XR_CheckResult(xrAttachSessionActionSets(m_session, &attachInfo), "attaching action set", m_instance);
}

void OpenXRInput::Shutdown()
{
	xrDestroyAction(m_primaryFire);
	xrDestroyAction(m_moveX);
	xrDestroyAction(m_moveY);
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

	UpdatePlayerMovement();
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

	createInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
	strcpy(createInfo.actionName, "move_x");
	strcpy(createInfo.localizedActionName, "Move X");
	XR_CheckResult(xrCreateAction(m_ingameSet, &createInfo, &m_moveX), "creating movement action");
	strcpy(createInfo.actionName, "move_y");
	strcpy(createInfo.localizedActionName, "Move Y");
	XR_CheckResult(xrCreateAction(m_ingameSet, &createInfo, &m_moveY), "creating movement action");
	strcpy(createInfo.actionName, "rotate_yaw");
	strcpy(createInfo.localizedActionName, "Rotate Yaw");
	XR_CheckResult(xrCreateAction(m_ingameSet, &createInfo, &m_rotateYaw), "creating movement action");
	strcpy(createInfo.actionName, "rotate_pitch");
	strcpy(createInfo.localizedActionName, "Rotate Pitch");
	XR_CheckResult(xrCreateAction(m_ingameSet, &createInfo, &m_rotatePitch), "creating movement action");
}

void OpenXRInput::SuggestBindings()
{
	SuggestedProfileBinding knuckles(m_instance);
	knuckles.AddBinding(m_primaryFire, "/user/hand/right/input/trigger");
	knuckles.AddBinding(m_controller[0], "/user/hand/left/input/aim");
	knuckles.AddBinding(m_controller[1], "/user/hand/right/input/aim");
	knuckles.AddBinding(m_moveX, "/user/hand/left/input/thumbstick/x");
	knuckles.AddBinding(m_moveY, "/user/hand/left/input/thumbstick/y");
	knuckles.AddBinding(m_rotateYaw, "/user/hand/right/input/thumbstick/x");
	knuckles.AddBinding(m_rotatePitch, "/user/hand/right/input/thumbstick/y");
	knuckles.SuggestBindings("/interaction_profiles/valve/index_controller");

	SuggestedProfileBinding touch(m_instance);
	touch.AddBinding(m_primaryFire, "/user/hand/right/input/trigger");
	touch.AddBinding(m_controller[0], "/user/hand/left/input/aim");
	touch.AddBinding(m_controller[1], "/user/hand/right/input/aim");
	touch.AddBinding(m_moveX, "/user/hand/left/input/thumbstick/x");
	touch.AddBinding(m_moveY, "/user/hand/left/input/thumbstick/y");
	touch.AddBinding(m_rotateYaw, "/user/hand/right/input/thumbstick/x");
	touch.AddBinding(m_rotatePitch, "/user/hand/right/input/thumbstick/y");
	touch.SuggestBindings("/interaction_profiles/oculus/touch_controller");
}

void OpenXRInput::CreateBooleanAction(XrActionSet actionSet, XrAction* action, const char* name, const char* description)
{
	XrActionCreateInfo createInfo{ XR_TYPE_ACTION_CREATE_INFO };
	createInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
	strcpy(createInfo.actionName, name);
	strcpy(createInfo.localizedActionName, description);
	XR_CheckResult(xrCreateAction(actionSet, &createInfo, action), "creating boolean action");
}

void OpenXRInput::UpdatePlayerMovement()
{
	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if (!pPlayer)
		return;
	IPlayerInput* input = pPlayer->GetPlayerInput();
	if (!input)
		return;

	XrActionStateFloat state{ XR_TYPE_ACTION_STATE_FLOAT };
	XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
	getInfo.subactionPath = XR_NULL_PATH;

	getInfo.action = m_moveX;
	xrGetActionStateFloat(m_session, &getInfo, &state);
	if (state.isActive)
	{
		input->OnAction(g_pGameActions->xi_movex, eAAM_Always, state.currentState);
	}

	getInfo.action = m_moveY;
	xrGetActionStateFloat(m_session, &getInfo, &state);
	if (state.isActive)
	{
		input->OnAction(g_pGameActions->xi_movey, eAAM_Always, state.currentState);
	}

	getInfo.action = m_rotateYaw;
	xrGetActionStateFloat(m_session, &getInfo, &state);
	if (state.isActive)
	{
		input->OnAction(g_pGameActions->xi_rotateyaw, eAAM_Always, state.currentState);
	}

	getInfo.action = m_rotatePitch;
	xrGetActionStateFloat(m_session, &getInfo, &state);
	if (state.isActive)
	{
		input->OnAction(g_pGameActions->xi_rotatepitch, eAAM_Always, state.currentState);
	}
}
