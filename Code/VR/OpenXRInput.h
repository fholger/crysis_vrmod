#pragma once
#include <openxr/openxr.h>

#include "EVRHand.h"
#include "IActionMapManager.h"

class OpenXRInput
{
public:
	void Init(XrInstance instance, XrSession session, XrSpace space);
	void Shutdown();

	void Update();

	Matrix34 GetControllerTransform(int hand);
	Matrix34 GetControllerWeaponTransform(int hand);
	Vec3 GetControllerVelocity(int hand);

	void EnableHandMovementForQuickMenu();
	void DisableHandMovementForQuickMenu();

	float GetGripAmount(int side) const;

	void SendHapticEvent(EVRHand hand, float duration, float amplitude, float frequency = XR_FREQUENCY_UNSPECIFIED);
	void SendHapticEvent(float duration, float amplitude, float frequency = XR_FREQUENCY_UNSPECIFIED);
	void StopHaptics(EVRHand hand);
	void StopHaptics();

private:
	struct BooleanAction
	{
		XrAction handle = XR_NULL_HANDLE;
		ActionId* onPress = nullptr;
		ActionId* onLongPress = nullptr;
		bool sendRelease = true;
		bool sendLongRelease = true;
		bool pressOnRelease = false;
		float longPressActivationTime = 0.25f;
		bool longPressActive = false;
		float timePressed = -1;
	};

	XrInstance m_instance = XR_NULL_HANDLE;
	XrSession m_session = XR_NULL_HANDLE;
	XrSpace m_trackingSpace = XR_NULL_HANDLE;

	XrActionSet m_ingameSet = XR_NULL_HANDLE;
	XrAction m_controller[2] = {};
	XrAction m_moveX = XR_NULL_HANDLE;
	XrAction m_moveY = XR_NULL_HANDLE;
	XrAction m_rotateYaw = XR_NULL_HANDLE;
	XrAction m_rotatePitch = XR_NULL_HANDLE;
	XrAction m_jumpCrouch = XR_NULL_HANDLE;
	XrAction m_grip[2] = {};
	XrAction m_trigger[2] = {};
	XrAction m_haptics[2] = {};
	BooleanAction m_primaryFire;
	BooleanAction m_sprint;
	BooleanAction m_reload;
	BooleanAction m_menu;
	BooleanAction m_suitMenu;
	BooleanAction m_binoculars;
	BooleanAction m_nextWeapon;
	BooleanAction m_use;
	BooleanAction m_gripUse;
	BooleanAction m_nightvision;
	BooleanAction m_melee;
	BooleanAction m_grenades;
	XrSpace m_gripSpace[2] = {};

	XrActionSet m_menuSet = XR_NULL_HANDLE;
	BooleanAction m_menuClick;
	BooleanAction m_menuBack;
	BooleanAction m_dropWeapon;

	XrActionSet m_vehicleSet = XR_NULL_HANDLE;
	BooleanAction m_vecBoost;
	BooleanAction m_vecAfterburner;
	BooleanAction m_vecSecondaryFire;
	BooleanAction m_vecAscend;
	BooleanAction m_vecHorn;
	BooleanAction m_vecLights;
	BooleanAction m_vecExit;
	BooleanAction m_vecSwitchSeatView;

	float m_gripAmount[2] = {};
	float m_triggerAmount[2] = {};

	bool m_wasJumpActive = false;
	bool m_wasCrouchActive = false;
	int m_snapTurnState = 0;

	static const int MOUSE_SAMPLE_COUNT = 10;
	Vec2 m_hudMousePosSamples[MOUSE_SAMPLE_COUNT];
	int m_curMouseSampleIdx = 0;

	bool m_quickMenuActive = false;
	Vec3 m_quickMenuInitialHandPosition;

	Vec3 m_controllerPos[2];
	Quat m_controllerRot[2];

	void CreateInputActions();
	void SuggestBindings();
	void AttachActionSets();
	void CreateBooleanAction(XrActionSet actionSet, BooleanAction& action, const char* name, const char* description, ActionId* onPress, ActionId* onLongPress = nullptr, bool sendRelease = true, bool sendLongRelease = true, bool pressOnRelease = false, float longPressActivationTime = 0.25f);
	void UpdateMeleeAttacks();
	void UpdateIngameActions();
	void UpdateVehicleActions();
	void UpdateMenuActions();
	void UpdateHUDActions();
	void UpdatePlayerMovement();
	void UpdateGripAmount();
	void UpdateWeaponScopes();
	void UpdateBooleanAction(BooleanAction& action);
	void UpdateBooleanActionForMenu(BooleanAction& action, EDeviceId device, EKeyId key);
	void UpdateControllerPoses();

	bool CalcControllerHudIntersection(int hand, float& x, float& y);

	void DestroyAction(XrAction& action);
};
