#include "StdAfx.h"
#include "OpenXRInput.h"

#include "GameActions.h"
#include "GameCVars.h"
#include "IPlayerInput.h"
#include "OpenXRRuntime.h"
#include "Player.h"
#include "VRManager.h"
#include "VRRenderer.h"
#include "HUD/HUD.h"
#include "Menus/FlashMenuObject.h"

extern bool XR_CheckResult(XrResult result, const char* description, XrInstance instance = nullptr);
extern Matrix34 OpenXRToCrysis(const XrQuaternionf& orientation, const XrVector3f& position);

namespace
{
	std::string replace_all(std::string input, const std::string& search, const std::string& replace)
	{
		if (search.empty())
			return input;

		size_t pos = 0;
		while ((pos = input.find(search, pos)) != std::string::npos)
		{
			input.replace(pos, search.length(), replace);
			pos += replace.length();
		}
		return input;
	}
}

class SuggestedProfileBinding
{
public:
	SuggestedProfileBinding(XrInstance instance, bool hasXYButtonsOnLeft) : m_instance(instance), m_hasXYButtonsOnLeft(hasXYButtonsOnLeft) {}

	void AddBinding(XrAction action, const char* binding)
	{
		const char* weaponHand = g_pGameCVars->vr_weapon_hand == 0 ? "left" : "right";
		const char* nonWeaponHand = g_pGameCVars->vr_weapon_hand == 1 ? "left" : "right";
		const char* movementHand = g_pGameCVars->vr_movement_hand == 0 ? "left" : "right";
		const char* nonMovementHand = g_pGameCVars->vr_movement_hand == 1 ? "left" : "right";
		std::string finalBinding = binding;
		finalBinding = replace_all(finalBinding, "<weapon>", weaponHand);
		finalBinding = replace_all(finalBinding, "<!weapon>", nonWeaponHand);
		finalBinding = replace_all(finalBinding, "<movement>", movementHand);
		finalBinding = replace_all(finalBinding, "<!movement>", nonMovementHand);

		if (m_hasXYButtonsOnLeft && finalBinding.find("/user/hand/left") == 0)
		{
			size_t pos = finalBinding.size() - 2;
			if (finalBinding.substr(pos) == "/a")
				finalBinding.replace(pos, 2, "/x");
			if (finalBinding.substr(pos) == "/b")
				finalBinding.replace(pos, 2, "/y");
			pos = finalBinding.size() - strlen("/a/click");
			if (finalBinding.substr(pos) == "/a/click" || finalBinding.substr(pos) == "/a/touch")
				finalBinding.replace(pos, 2, "/x");
			if (finalBinding.substr(pos) == "/b/click" || finalBinding.substr(pos) == "/b/touch")
				finalBinding.replace(pos, 2, "/y");
		}

		CryLogAlways("Final binding path: %s", finalBinding.c_str());
		XrPath bindingPath;
		XR_CheckResult(xrStringToPath(m_instance, finalBinding.c_str(), &bindingPath), "string to path");
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
	bool m_hasXYButtonsOnLeft;
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
	strcpy(setCreateInfo.actionSetName, "menu");
	strcpy(setCreateInfo.localizedActionSetName, "Menu");
	XR_CheckResult(xrCreateActionSet(m_instance, &setCreateInfo, &m_menuSet), "creating menu action set", m_instance);
	strcpy(setCreateInfo.actionSetName, "vehicles");
	strcpy(setCreateInfo.localizedActionSetName, "Vehicles");
	XR_CheckResult(xrCreateActionSet(m_instance, &setCreateInfo, &m_vehicleSet), "creating vehicle action set", m_instance);

	CreateInputActions();
	SuggestBindings();

	XrSessionActionSetsAttachInfo attachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
	std::vector<XrActionSet> actionSets;
	actionSets.push_back(m_ingameSet);
	actionSets.push_back(m_menuSet);
	actionSets.push_back(m_vehicleSet);
	attachInfo.actionSets = actionSets.data();
	attachInfo.countActionSets = actionSets.size();
	XR_CheckResult(xrAttachSessionActionSets(m_session, &attachInfo), "attaching action set", m_instance);
}

void OpenXRInput::Shutdown()
{
	xrDestroySpace(m_gripSpace[0]);
	xrDestroySpace(m_gripSpace[1]);
	xrDestroyActionSet(m_ingameSet);
	xrDestroyActionSet(m_menuSet);
	xrDestroyActionSet(m_vehicleSet);

	m_session = nullptr;
	m_instance = nullptr;
}

void OpenXRInput::Update()
{
	if (!m_session || !g_pGameCVars->vr_enable_motion_controllers)
		return;

	std::vector<XrActiveActionSet> activeSets;
	activeSets.push_back({ m_ingameSet, XR_NULL_PATH });
	activeSets.push_back({ m_menuSet, XR_NULL_PATH });
	activeSets.push_back({ m_vehicleSet, XR_NULL_PATH });
	XrActionsSyncInfo syncInfo{ XR_TYPE_ACTIONS_SYNC_INFO };
	syncInfo.countActiveActionSets = activeSets.size();
	syncInfo.activeActionSets = activeSets.data();
	XR_CheckResult(xrSyncActions(m_session, &syncInfo), "syncing actions");

	float pointerX, pointerY;
	if (CalcControllerHudIntersection(g_pGameCVars->vr_weapon_hand, pointerX, pointerY))
	{
		m_hudMousePosSamples[m_curMouseSampleIdx].x = pointerX;
		m_hudMousePosSamples[m_curMouseSampleIdx].y = pointerY;
		m_curMouseSampleIdx = (m_curMouseSampleIdx + 1) % MOUSE_SAMPLE_COUNT;

		Vec2 smoothedMousePos;
		for (int i = 0; i < MOUSE_SAMPLE_COUNT; ++i)
		{
			smoothedMousePos += m_hudMousePosSamples[i];
		}
		smoothedMousePos /= MOUSE_SAMPLE_COUNT;

		Vec2i windowSize = gVRRenderer->GetWindowSize();
		gEnv->pHardwareMouse->SetHardwareMouseClientPosition(smoothedMousePos.x * windowSize.x, smoothedMousePos.y * windowSize.y);
	}

	if (g_pGame->GetMenu()->IsMenuActive())
	{
		UpdateMenuActions();
		return;
	}
	if (g_pGame->GetHUD() && g_pGame->GetHUD()->GetModalHUD())
	{
		UpdateHUDActions();
		return;
	}

	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if (pPlayer && pPlayer->GetLinkedVehicle())
	{
		UpdateVehicleActions();
	}
	else
	{
		UpdateIngameActions();
	}
}

Matrix34 OpenXRInput::GetControllerTransform(int hand)
{
	hand = clamp_tpl(hand, 0, 1);

	XrSpaceLocation location{ XR_TYPE_SPACE_LOCATION };
	XR_CheckResult(xrLocateSpace(m_gripSpace[hand], m_trackingSpace, gXR->GetNextFrameDisplayTime(), &location), "locating grip space", m_instance);

	// the grip pose has a peculiar orientation that we need to fix
	Matrix33 correction = Matrix33::CreateRotationX(-gf_PI/2);
	return OpenXRToCrysis(location.pose.orientation, location.pose.position) * correction;
}

Matrix34 OpenXRInput::GetControllerWeaponTransform(int hand)
{
	// Weapon bones are offset to what our grip pose is, so we need to rotate the pose a bit
	Matrix33 correction = Matrix33::CreateRotationX(-gf_PI/2) * Matrix33::CreateRotationY(-gf_PI/2);
	Matrix33 gripAngleAdjust = Matrix33::CreateRotationX(DEG2RAD(g_pGameCVars->vr_weapon_angle_offset));
	return GetControllerTransform(hand) * gripAngleAdjust * correction;
}

void OpenXRInput::EnableHandMovementForQuickMenu()
{
	m_quickMenuActive = true;
	m_quickMenuInitialHandPosition = GetControllerTransform(g_pGameCVars->vr_weapon_hand).GetTranslation();
}

void OpenXRInput::DisableHandMovementForQuickMenu()
{
	m_quickMenuActive = false;
}

void OpenXRInput::CreateInputActions()
{
	CreateBooleanAction(m_ingameSet, m_primaryFire, "primary_fire", "Primary Fire", &g_pGameActions->attack1);
	CreateBooleanAction(m_ingameSet, m_suitMenu, "suit_menu", "Suit Menu", &g_pGameActions->hud_suit_menu);
	CreateBooleanAction(m_ingameSet, m_menu, "menu_toggle", "Open menu / show objectives", &g_pGameActions->xi_hud_back);
	CreateBooleanAction(m_ingameSet, m_sprint, "sprint", "Sprint", &g_pGameActions->sprint);
	CreateBooleanAction(m_ingameSet, m_reload, "reload", "Reload", &g_pGameActions->reload, &g_pGameActions->firemode);
	CreateBooleanAction(m_ingameSet, m_nextWeapon, "next_weapon", "Next Weapon", &g_pGameActions->nextitem, &g_pGameActions->xi_zoom, false);
	CreateBooleanAction(m_ingameSet, m_use, "use", "Use", &g_pGameActions->xi_use);
	CreateBooleanAction(m_ingameSet, m_binoculars, "binoculars", "Binoculars", &g_pGameActions->xi_binoculars);
	CreateBooleanAction(m_ingameSet, m_nightvision, "nightvision", "Nightvision", &g_pGameActions->hud_night_vision);
	CreateBooleanAction(m_ingameSet, m_melee, "melee", "Melee Attack", &g_pGameActions->special);

	CreateBooleanAction(m_menuSet, m_menuClick, "click", "Menu Click", &g_pGameActions->hud_mouseclick);
	CreateBooleanAction(m_menuSet, m_menuBack, "back", "Menu Back", nullptr);

	CreateBooleanAction(m_vehicleSet, m_vecBoost, "vec_boost", "Vehicle Boost", &g_pGameActions->v_boost);
	CreateBooleanAction(m_vehicleSet, m_vecAfterburner, "vec_afterburner", "Vehicle Afterburner", &g_pGameActions->v_afterburner);
	CreateBooleanAction(m_vehicleSet, m_vecBrake, "vec_brake", "Vehicle Brake", &g_pGameActions->v_brake);
	CreateBooleanAction(m_vehicleSet, m_vecExit, "vec_exit", "Exit vehicle", &g_pGameActions->use, nullptr, false, true);
	CreateBooleanAction(m_vehicleSet, m_vecHorn, "vec_horn", "Vehicle horn", &g_pGameActions->v_horn);
	CreateBooleanAction(m_vehicleSet, m_vecLights, "vec_lights", "Vehicle lights", &g_pGameActions->v_lights);
	CreateBooleanAction(m_vehicleSet, m_vecSwitchSeatView, "vec_switch", "Switch vehicle seat / view", &g_pGameActions->v_changeseat, &g_pGameActions->v_changeview, false);

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
	SuggestedProfileBinding knuckles(m_instance, false);
	knuckles.AddBinding(m_primaryFire.handle, "/user/hand/<weapon>/input/trigger/click");
	knuckles.AddBinding(m_controller[0], "/user/hand/left/input/grip/pose");
	knuckles.AddBinding(m_controller[1], "/user/hand/right/input/grip/pose");
	knuckles.AddBinding(m_moveX, "/user/hand/<movement>/input/thumbstick/x");
	knuckles.AddBinding(m_moveY, "/user/hand/<movement>/input/thumbstick/y");
	knuckles.AddBinding(m_rotateYaw, "/user/hand/<!movement>/input/thumbstick/x");
	knuckles.AddBinding(m_jumpCrouch, "/user/hand/<!movement>/input/thumbstick/y");
	knuckles.AddBinding(m_rotatePitch, "/user/hand/<!movement>/input/thumbstick/y");
	knuckles.AddBinding(m_sprint.handle, "/user/hand/<movement>/input/thumbstick/click");
	knuckles.AddBinding(m_menu.handle, "/user/hand/<!weapon>/input/trackpad/force");
	knuckles.AddBinding(m_reload.handle, "/user/hand/<weapon>/input/a");
	knuckles.AddBinding(m_suitMenu.handle, "/user/hand/<weapon>/input/trackpad/force");
	knuckles.AddBinding(m_nextWeapon.handle, "/user/hand/<weapon>/input/b");
	knuckles.AddBinding(m_use.handle, "/user/hand/<!weapon>/input/squeeze/force");
	knuckles.AddBinding(m_binoculars.handle, "/user/hand/<!weapon>/input/a");
	knuckles.AddBinding(m_nightvision.handle, "/user/hand/<!weapon>/input/b");
	knuckles.AddBinding(m_melee.handle, "/user/hand/<weapon>/input/thumbstick/click");
	knuckles.AddBinding(m_menuClick.handle, "/user/hand/<weapon>/input/trigger");
	knuckles.AddBinding(m_menuClick.handle, "/user/hand/<weapon>/input/a");
	knuckles.AddBinding(m_menuBack.handle, "/user/hand/<weapon>/input/b");
	knuckles.AddBinding(m_vecBoost.handle, "/user/hand/<!weapon>/input/trigger");
	knuckles.AddBinding(m_vecBrake.handle, "/user/hand/<!movement>/input/a");
	knuckles.AddBinding(m_vecSwitchSeatView.handle, "/user/hand/<!movement>/input/b");
	knuckles.AddBinding(m_vecAfterburner.handle, "/user/hand/<!weapon>/input/trigger");
	knuckles.AddBinding(m_vecHorn.handle, "/user/hand/<!movement>/input/thumbstick/click");
	knuckles.AddBinding(m_vecLights.handle, "/user/hand/<movement>/input/thumbstick/click");
	knuckles.AddBinding(m_vecExit.handle, "/user/hand/<movement>/input/a");
	knuckles.SuggestBindings("/interaction_profiles/valve/index_controller");

	SuggestedProfileBinding touch(m_instance, true);
	touch.AddBinding(m_primaryFire.handle, "/user/hand/<weapon>/input/trigger");
	touch.AddBinding(m_controller[0], "/user/hand/left/input/grip/pose");
	touch.AddBinding(m_controller[1], "/user/hand/right/input/grip/pose");
	touch.AddBinding(m_moveX, "/user/hand/<movement>/input/thumbstick/x");
	touch.AddBinding(m_moveY, "/user/hand/<movement>/input/thumbstick/y");
	touch.AddBinding(m_rotateYaw, "/user/hand/<!movement>/input/thumbstick/x");
	touch.AddBinding(m_jumpCrouch, "/user/hand/<!movement>/input/thumbstick/y");
	touch.AddBinding(m_rotatePitch, "/user/hand/<!movement>/input/thumbstick/y");
	touch.AddBinding(m_sprint.handle, "/user/hand/<movement>/input/thumbstick/click");
	touch.AddBinding(m_menu.handle, "/user/hand/left/input/menu/click");
	touch.AddBinding(m_reload.handle, "/user/hand/<weapon>/input/a");
	touch.AddBinding(m_suitMenu.handle, "/user/hand/<!movement>/input/thumbstick/click");
	touch.AddBinding(m_nextWeapon.handle, "/user/hand/<weapon>/input/b");
	touch.AddBinding(m_use.handle, "/user/hand/<!weapon>/input/squeeze");
	touch.AddBinding(m_binoculars.handle, "/user/hand/<!weapon>/input/a");
	touch.AddBinding(m_nightvision.handle, "/user/hand/<!weapon>/input/b");
	//touch.AddBinding(m_melee.handle, "/user/hand/<weapon>/input/thumbstick/click");
	touch.AddBinding(m_menuClick.handle, "/user/hand/<weapon>/input/trigger");
	touch.AddBinding(m_menuClick.handle, "/user/hand/<weapon>/input/a");
	touch.AddBinding(m_menuBack.handle, "/user/hand/<weapon>/input/b");
	touch.AddBinding(m_vecBoost.handle, "/user/hand/<!weapon>/input/trigger");
	touch.AddBinding(m_vecBrake.handle, "/user/hand/<!movement>/input/a");
	touch.AddBinding(m_vecSwitchSeatView.handle, "/user/hand/<!movement>/input/b");
	touch.AddBinding(m_vecAfterburner.handle, "/user/hand/<!weapon>/input/trigger");
	touch.AddBinding(m_vecHorn.handle, "/user/hand/<!movement>/input/thumbstick/click");
	touch.AddBinding(m_vecLights.handle, "/user/hand/<movement>/input/thumbstick/click");
	touch.AddBinding(m_vecExit.handle, "/user/hand/<movement>/input/a");
	touch.SuggestBindings("/interaction_profiles/oculus/touch_controller");
}


void OpenXRInput::CreateBooleanAction(XrActionSet actionSet, BooleanAction& action, const char* name,
	const char* description, ActionId* onPress, ActionId* onLongPress, bool sendRelease, bool pressOnRelease)
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
	action.pressOnRelease = pressOnRelease;
	action.longPressActive = false;
}

void OpenXRInput::UpdateIngameActions()
{
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
	UpdateBooleanAction(m_menuClick);
}

void OpenXRInput::UpdateVehicleActions()
{
	UpdatePlayerMovement();
	UpdateBooleanAction(m_menu);
	UpdateBooleanAction(m_primaryFire);
	UpdateBooleanAction(m_menuClick);
	UpdateBooleanAction(m_vecBoost);
	UpdateBooleanAction(m_vecAfterburner);
	UpdateBooleanAction(m_vecExit);
	UpdateBooleanAction(m_vecSwitchSeatView);
	UpdateBooleanAction(m_vecHorn);
	UpdateBooleanAction(m_vecLights);
	UpdateBooleanAction(m_vecBrake);
}

void OpenXRInput::UpdateMenuActions()
{
	UpdateBooleanActionForMenu(m_menuClick, eDI_XI, eKI_XI_A);
	UpdateBooleanActionForMenu(m_menuBack, eDI_XI, eKI_XI_B);
	UpdateBooleanActionForMenu(m_menu, eDI_Keyboard, eKI_Escape);
	// this is needed to potentially debounce the suit menu and not have it active in the background still
	UpdateBooleanAction(m_suitMenu);
}

void OpenXRInput::UpdateHUDActions()
{
	UpdatePlayerMovement();
	UpdateBooleanAction(m_menu);
	UpdateBooleanAction(m_suitMenu);
	UpdateBooleanAction(m_menuClick);
}

void OpenXRInput::UpdatePlayerMovement()
{
	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if (!pPlayer)
		return;
	IPlayerInput* input = pPlayer->GetPlayerInput();
	if (!input)
		return;

	bool inVehicle = pPlayer->GetLinkedVehicle() != nullptr;
	bool inHud = g_pGame->GetHUD() && g_pGame->GetHUD()->GetModalHUD() != nullptr;
	bool usingMountedGun = pPlayer->GetActorStats()->mountedWeaponID != 0;
	bool rendering2D = !gVRRenderer->ShouldRenderVR();
	bool usingBinoculars = gVRRenderer->AreBinocularsActive();

	XrActionStateFloat state{ XR_TYPE_ACTION_STATE_FLOAT };
	XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
	getInfo.subactionPath = XR_NULL_PATH;

	getInfo.action = m_moveX;
	xrGetActionStateFloat(m_session, &getInfo, &state);
	if (state.isActive)
	{
		input->OnAction(inVehicle ? g_pGameActions->xi_v_movex : g_pGameActions->xi_movex, eAAM_Always, state.currentState);
	}

	getInfo.action = m_moveY;
	xrGetActionStateFloat(m_session, &getInfo, &state);
	if (state.isActive)
	{
		input->OnAction(inVehicle ? g_pGameActions->xi_v_movey : g_pGameActions->xi_movey, eAAM_Always, state.currentState);
	}

	getInfo.action = m_jumpCrouch;
	xrGetActionStateFloat(m_session, &getInfo, &state);
	float jumpCrouch = state.isActive ? state.currentState : 0;

	getInfo.action = m_rotateYaw;
	xrGetActionStateFloat(m_session, &getInfo, &state);
	float yaw = state.isActive ? state.currentState : 0;

	getInfo.action = m_rotatePitch;
	xrGetActionStateFloat(m_session, &getInfo, &state);
	float pitch = state.isActive ? state.currentState : 0;

	if (inVehicle)
	{
		input->OnAction(g_pGameActions->xi_v_rotatepitch, eAAM_Always, pitch);
		input->OnAction(g_pGameActions->xi_v_rotateyaw, eAAM_Always, yaw);
		return;
	}

	if (inHud || usingMountedGun || (rendering2D && !usingBinoculars))
	{
		if (m_quickMenuActive)
		{
			// we are using movement of the main controller for the menu input, instead
			Vec3 movementDiff = GetControllerTransform(g_pGameCVars->vr_weapon_hand).GetTranslation() - m_quickMenuInitialHandPosition;
			Ang3 hmdAngles = Ang3(gVR->GetHMDQuat());
			Vec3 hmdRight = Matrix33::CreateRotationZ(hmdAngles.z).GetColumn0();
			Vec3 up = Vec3(0, 0, 1);

			const float MaxMove = 0.03f;
			yaw = clamp(movementDiff.Dot(hmdRight) / MaxMove, -1.f, 1.f);
			pitch = clamp(movementDiff.Dot(up) / MaxMove, -1.f, 1.f);
		}
		
		input->OnAction(g_pGameActions->xi_rotatepitch, eAAM_Always, pitch);
		input->OnAction(g_pGameActions->xi_rotateyaw, eAAM_Always, yaw);
		return;
	}

	if (fabsf(jumpCrouch) < g_pGameCVars->vr_controller_stick_zone_cutoff)
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
		if (m_wasJumpActive && !usingBinoculars)
			input->OnAction(g_pGameActions->jump, eAAM_OnRelease, 0);
		m_wasJumpActive = false;
	}
	else if (fabsf(yaw) < g_pGameCVars->vr_controller_stick_zone_cutoff)
	{
		if (!m_wasJumpActive)
			input->OnAction(usingBinoculars ? g_pGameActions->zoom_in : g_pGameActions->jump, eAAM_OnPress, 1);
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
			if (usingBinoculars)
			{
				input->OnAction(g_pGameActions->zoom_out, eAAM_OnPress, 1);
			}
			else if (pPlayer->GetStance() == STANCE_CROUCH)
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
		if (state.changedSinceLastSync)
		{
			if ((state.currentState && !action.pressOnRelease) || (!state.currentState && action.pressOnRelease))
				input->OnAction(*action.onPress, eAAM_OnPress, 1.f);
			else if (!state.currentState && action.sendRelease)
				input->OnAction(*action.onPress, eAAM_OnRelease, 0.f);
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
		else if (action.onPress && !action.longPressActive)
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

	if (state.currentState == XR_TRUE && action.timePressed >= 0 && !action.longPressActive)
	{
		float curTime = gEnv->pTimer->GetAsyncCurTime();
		if (curTime - action.timePressed >= 0.25f)
		{
			action.longPressActive = true;
			input->OnAction(*action.onLongPress, eAAM_OnPress, 1);
		}
	}
}

void OpenXRInput::UpdateBooleanActionForMenu(BooleanAction& action, EDeviceId device, EKeyId key)
{
	action.timePressed = -1;

	CFlashMenuObject* menu = g_pGame->GetMenu();
	if (!menu || !menu->IsMenuActive())
		return;

	XrActionStateBoolean state{ XR_TYPE_ACTION_STATE_BOOLEAN };
	XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
	getInfo.subactionPath = XR_NULL_PATH;
	getInfo.action = action.handle;
	XR_CheckResult(xrGetActionStateBoolean(m_session, &getInfo, &state), "getting boolean action state", m_instance);

	if (!state.isActive || !state.changedSinceLastSync)
		return;

	SInputEvent event;
	event.deviceId = device;
	event.keyId = key;
	event.state = state.currentState ? eIS_Pressed : eIS_Released;
	menu->OnInputEvent(event);
}

bool OpenXRInput::CalcControllerHudIntersection(int hand, float& x, float& y)
{
	hand = clamp_tpl(hand, 0, 1);

	XrSpaceLocation location{ XR_TYPE_SPACE_LOCATION };
	XR_CheckResult(xrLocateSpace(m_gripSpace[hand], m_trackingSpace, gXR->GetNextFrameDisplayTime(), &location), "locating grip space", m_instance);
	// need to massage the pose orientation a little bit to work in our favour
	Matrix33 correction = Matrix33::CreateRotationXYZ(Ang3(-gf_PI/3, 0, 0));
	Matrix34 controllerTransform = OpenXRToCrysis(location.pose.orientation, location.pose.position) * correction;

	XrPosef hudPose = gXR->GetHudPose();
	Matrix34 hudTransform = OpenXRToCrysis(hudPose.orientation, hudPose.position);
	hudTransform.InvertFast();

	Matrix34 controllerInHudSpace = hudTransform * controllerTransform;

	Vec3 pos = controllerInHudSpace.GetTranslation();
	Vec3 dir = controllerInHudSpace.GetColumn1();

	if (dir.y <= 0.01)
		return false;

	float t = -pos.y / dir.y;
	if (t < 0)
		return false;

	Vec2 intersection(pos.x + t * dir.x, pos.z + t * dir.z);
	Vec2 hudSize(gXR->GetHudWidth(), gXR->GetHudHeight());
	Vec2 upperLeft = -.5f * hudSize;

	intersection -= upperLeft;
	intersection.x /= hudSize.x;
	intersection.y /= hudSize.y;
	x = intersection.x;
	y = 1 - intersection.y;
	return (x >= 0 && x <= 1 && y >= 0 && y <= 1);
}
