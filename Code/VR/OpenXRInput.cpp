#include "StdAfx.h"
#include "OpenXRInput.h"

#include "GameActions.h"
#include "GameCVars.h"
#include "IPlayerInput.h"
#include "OpenXRRuntime.h"
#include "Player.h"

extern bool XR_CheckResult(XrResult result, const char* description, XrInstance instance = nullptr);
extern Matrix34 OpenXRToCrysis(const XrQuaternionf& orientation, const XrVector3f& position);

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

void OpenXRInput::Init(XrInstance instance, XrSession session, XrSpace space)
{
	m_instance = instance;
	m_session = session;
	m_trackingSpace = space;

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
	xrDestroySpace(m_gripSpace[0]);
	xrDestroySpace(m_gripSpace[1]);
	xrDestroyAction(m_primaryFire.handle);
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
	if (!m_session || !g_pGameCVars->vr_enable_motion_controllers)
		return;

	XrActiveActionSet activeSet{ m_ingameSet, XR_NULL_PATH };
	XrActionsSyncInfo syncInfo{ XR_TYPE_ACTIONS_SYNC_INFO };
	syncInfo.countActiveActionSets = 1;
	syncInfo.activeActionSets = &activeSet;
	XR_CheckResult(xrSyncActions(m_session, &syncInfo), "syncing actions");

	UpdatePlayerMovement();
	UpdateBooleanAction(m_sprint);
	UpdateBooleanAction(m_menu);
	UpdateBooleanAction(m_suitMenu);
	UpdateBooleanAction(m_primaryFire);
	UpdateBooleanAction(m_nextWeapon);
	UpdateBooleanAction(m_reload);
	UpdateBooleanAction(m_use);
	UpdateBooleanAction(m_binoculars);
	UpdateBooleanAction(m_nightvision);
	UpdateBooleanAction(m_melee);
}

Matrix34 OpenXRInput::GetControllerTransform(int hand)
{
	XrSpaceLocation location{ XR_TYPE_SPACE_LOCATION };
	XR_CheckResult(xrLocateSpace(m_gripSpace[hand], m_trackingSpace, gXR->GetNextFrameDisplayTime(), &location), "locating grip space", m_instance);
	// the grip pose has a peculiar orientation. This brings it in line with what we need
	Matrix33 correction = Matrix33::CreateRotationXYZ(Ang3(gf_PI, gf_PI/2, 0));
	return OpenXRToCrysis(location.pose.orientation, location.pose.position) * correction;
}

void OpenXRInput::CreateInputActions()
{
	CreateBooleanAction(m_ingameSet, m_primaryFire, "primary_fire", "Primary Fire", &g_pGameActions->attack1);
	CreateBooleanAction(m_ingameSet, m_suitMenu, "suit_menu", "Suit Menu", &g_pGameActions->hud_suit_menu);
	CreateBooleanAction(m_ingameSet, m_menu, "menu_toggle", "Open menu / show objectives", &g_pGameActions->xi_hud_back);
	CreateBooleanAction(m_ingameSet, m_sprint, "sprint", "Sprint", &g_pGameActions->sprint);
	CreateBooleanAction(m_ingameSet, m_reload, "reload", "Reload", &g_pGameActions->reload, &g_pGameActions->firemode);
	CreateBooleanAction(m_ingameSet, m_nextWeapon, "next_weapon", "Next Weapon", &g_pGameActions->nextitem, nullptr, false);
	CreateBooleanAction(m_ingameSet, m_use, "use", "Use", &g_pGameActions->use);
	CreateBooleanAction(m_ingameSet, m_binoculars, "binoculars", "Binoculars", &g_pGameActions->binoculars);
	CreateBooleanAction(m_ingameSet, m_nightvision, "nightvision", "Nightvision", &g_pGameActions->hud_night_vision);
	CreateBooleanAction(m_ingameSet, m_melee, "melee", "Melee Attack", &g_pGameActions->special);

	XrActionCreateInfo createInfo{ XR_TYPE_ACTION_CREATE_INFO };
	createInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
	strcpy(createInfo.actionName, "left_controller");
	strcpy(createInfo.localizedActionName, "Left Controller");
	XR_CheckResult(xrCreateAction(m_ingameSet, &createInfo, &m_controller[0]), "creating controller pose action");
	strcpy(createInfo.actionName, "right_controller");
	strcpy(createInfo.localizedActionName, "Right Controller");
	XR_CheckResult(xrCreateAction(m_ingameSet, &createInfo, &m_controller[1]), "creating controller pose action");

	XrActionSpaceCreateInfo spaceCreateInfo{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
	spaceCreateInfo.action = m_controller[0];
	spaceCreateInfo.subactionPath = XR_NULL_PATH;
	spaceCreateInfo.poseInActionSpace.orientation.w = 1;
	XR_CheckResult(xrCreateActionSpace(m_session, &spaceCreateInfo, &m_gripSpace[0]), "creating controller tracking space", m_instance);
	spaceCreateInfo.action = m_controller[1];
	XR_CheckResult(xrCreateActionSpace(m_session, &spaceCreateInfo, &m_gripSpace[1]), "creating controller tracking space", m_instance);

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
	strcpy(createInfo.actionName, "jump_crouch");
	strcpy(createInfo.localizedActionName, "Jump / Crouch / Prone");
	XR_CheckResult(xrCreateAction(m_ingameSet, &createInfo, &m_jumpCrouch), "creating movement action");
}

void OpenXRInput::SuggestBindings()
{
	SuggestedProfileBinding knuckles(m_instance);
	knuckles.AddBinding(m_primaryFire.handle, "/user/hand/right/input/trigger/click");
	knuckles.AddBinding(m_controller[0], "/user/hand/left/input/grip");
	knuckles.AddBinding(m_controller[1], "/user/hand/right/input/grip");
	knuckles.AddBinding(m_moveX, "/user/hand/left/input/thumbstick/x");
	knuckles.AddBinding(m_moveY, "/user/hand/left/input/thumbstick/y");
	knuckles.AddBinding(m_rotateYaw, "/user/hand/right/input/thumbstick/x");
	knuckles.AddBinding(m_jumpCrouch, "/user/hand/right/input/thumbstick/y");
	knuckles.AddBinding(m_sprint.handle, "/user/hand/left/input/trigger");
	knuckles.AddBinding(m_menu.handle, "/user/hand/left/input/b/click");
	knuckles.AddBinding(m_reload.handle, "/user/hand/right/input/a/click");
	knuckles.AddBinding(m_suitMenu.handle, "/user/hand/left/input/trigger/click");
	knuckles.AddBinding(m_nextWeapon.handle, "/user/hand/right/input/b");
	knuckles.AddBinding(m_use.handle, "/user/hand/left/input/squeeze/force");
	knuckles.AddBinding(m_binoculars.handle, "/user/hand/left/input/a");
	knuckles.AddBinding(m_nightvision.handle, "/user/hand/left/input/thumbstick/click");
	knuckles.AddBinding(m_melee.handle, "/user/hand/right/input/thumbstick/click");
	knuckles.SuggestBindings("/interaction_profiles/valve/index_controller");

	SuggestedProfileBinding touch(m_instance);
	touch.AddBinding(m_primaryFire.handle, "/user/hand/right/input/trigger");
	touch.AddBinding(m_controller[0], "/user/hand/left/input/grip");
	touch.AddBinding(m_controller[1], "/user/hand/right/input/grip");
	touch.AddBinding(m_moveX, "/user/hand/left/input/thumbstick/x");
	touch.AddBinding(m_moveY, "/user/hand/left/input/thumbstick/y");
	touch.AddBinding(m_rotateYaw, "/user/hand/right/input/thumbstick/x");
	touch.AddBinding(m_rotatePitch, "/user/hand/right/input/thumbstick/y");
	touch.SuggestBindings("/interaction_profiles/oculus/touch_controller");
}


void OpenXRInput::CreateBooleanAction(XrActionSet actionSet, BooleanAction& action, const char* name,
	const char* description, ActionId* onPress, ActionId* onLongPress, bool sendRelease)
{
	if (action.handle)
	{
		xrDestroyAction(action.handle);
	}

	XrActionCreateInfo createInfo{ XR_TYPE_ACTION_CREATE_INFO };
	createInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
	strcpy(createInfo.actionName, name);
	strcpy(createInfo.localizedActionName, description);
	XR_CheckResult(xrCreateAction(actionSet, &createInfo, &action.handle), "creating boolean action");

	action.onPress = onPress;
	action.onLongPress = onLongPress;
	action.sendRelease = sendRelease;
	action.longPressActive = false;
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

	getInfo.action = m_jumpCrouch;
	xrGetActionStateFloat(m_session, &getInfo, &state);
	float jumpCrouch = state.isActive ? state.currentState : 0;

	getInfo.action = m_rotateYaw;
	xrGetActionStateFloat(m_session, &getInfo, &state);
	float yaw = state.isActive ? state.currentState : 0;
	if (state.isActive && fabsf(jumpCrouch) < g_pGameCVars->vr_controller_stick_zone_cutoff)
	{
		float deadzone = FClamp(g_pGameCVars->vr_controller_yaw_deadzone, 0, 0.99f);
		float value = 0;
		if (yaw < -deadzone)
			value = (yaw + deadzone) / (1.f - deadzone);
		else if (yaw > deadzone)
			value = (yaw - deadzone) / (1.f - deadzone);
		input->OnAction(g_pGameActions->xi_rotateyaw, eAAM_Always, value);
	}

	if (jumpCrouch < g_pGameCVars->vr_controller_stick_action_threshold)
	{
		if (m_wasJumpActive)
			input->OnAction(g_pGameActions->jump, eAAM_OnRelease, 0);
		m_wasJumpActive = false;
	}
	else if (fabsf(yaw) < g_pGameCVars->vr_controller_stick_zone_cutoff)
	{
		if (!m_wasJumpActive)
			input->OnAction(g_pGameActions->jump, eAAM_OnPress, 1);
		m_wasJumpActive = true;
	}
	if (jumpCrouch > -g_pGameCVars->vr_controller_stick_action_threshold)
	{
		m_wasCrouchActive = false;
	}
	else if (fabsf(yaw) < g_pGameCVars->vr_controller_stick_zone_cutoff)
	{
		if (!m_wasCrouchActive)
		{
			if (pPlayer->GetStance() == STANCE_CROUCH)
			{
				input->OnAction(g_pGameActions->crouch, eAAM_OnRelease, 0);
				input->OnAction(g_pGameActions->prone, eAAM_OnPress, 1);
			}
			else
			{
				// as prone is a toggle, might need to deactivate it
				if (pPlayer->GetStance() == STANCE_PRONE)
					input->OnAction(g_pGameActions->prone, eAAM_OnPress, 1);
				input->OnAction(g_pGameActions->crouch, eAAM_OnPress, 1);
			}
		}
		m_wasCrouchActive = true;
	}

	getInfo.action = m_rotatePitch;
	xrGetActionStateFloat(m_session, &getInfo, &state);
	if (state.isActive)
	{
		input->OnAction(g_pGameActions->xi_rotatepitch, eAAM_Always, state.currentState);
	}
}

void OpenXRInput::UpdateBooleanAction(BooleanAction& action)
{
	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if (!pPlayer)
		return;
	IPlayerInput* input = pPlayer->GetPlayerInput();
	if (!input)
		return;

	XrActionStateBoolean state{ XR_TYPE_ACTION_STATE_BOOLEAN };
	XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
	getInfo.subactionPath = XR_NULL_PATH;
	getInfo.action = action.handle;
	XR_CheckResult(xrGetActionStateBoolean(m_session, &getInfo, &state), "getting boolean action state", m_instance);

	if (!state.isActive || !action.onPress)
		return;

	if (action.onLongPress == nullptr)
	{
		if (state.changedSinceLastSync && (state.currentState || action.sendRelease))
		{
			input->OnAction(*action.onPress, state.currentState ? eAAM_OnPress : eAAM_OnRelease, state.currentState ? 1.f : 0.f);
		}
		return;
	}

	if (state.changedSinceLastSync && state.currentState == XR_TRUE)
	{
		action.timePressed = gEnv->pTimer->GetAsyncCurTime();
	}
	else if (state.changedSinceLastSync && state.currentState == XR_FALSE)
	{
		if (action.longPressActive && action.onLongPress && action.sendRelease)
		{
			input->OnAction(*action.onLongPress, eAAM_OnRelease, 0);
		}
		else if (action.onPress)
		{
			// we are only activating on release to distinguish from a long press
			input->OnAction(*action.onPress, eAAM_OnPress, 1);
			if (action.sendRelease)
			{
				input->OnAction(*action.onPress, eAAM_OnRelease, 0);
			}
		}
		action.timePressed = -1;
		action.longPressActive = false;
	}

	if (state.currentState == XR_TRUE && action.timePressed >= 0)
	{
		float curTime = gEnv->pTimer->GetAsyncCurTime();
		if (curTime - action.timePressed >= 0.25f)
		{
			action.longPressActive = true;
			input->OnAction(*action.onLongPress, eAAM_OnPress, 1);
		}
	}
}
