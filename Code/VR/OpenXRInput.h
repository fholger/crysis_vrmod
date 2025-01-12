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

	void EnableHandMovementForQuickMenu();
	void DisableHandMovementForQuickMenu();

	float GetGripAmount(int side) const;

	void SendHapticEvent(EVRHand hand, float duration, float amplitude);
	void SendHapticEvent(float duration, float amplitude);
	void StopHaptics(EVRHand hand);
	void StopHaptics();

private:
	struct BooleanAction
	{
		XrAction handle = nullptr;
		ActionId* onPress = nullptr;
		ActionId* onLongPress = nullptr;
		bool sendRelease = true;
		bool sendLongRelease = true;
		bool pressOnRelease = false;
		bool longPressActive = false;
		float timePressed = -1;
	};

	XrInstance m_instance = nullptr;
	XrSession m_session = nullptr;
	XrSpace m_trackingSpace = nullptr;

	XrActionSet m_ingameSet = nullptr;
	XrAction m_controller[2] = {};
	XrAction m_moveX = nullptr;
	XrAction m_moveY = nullptr;
	XrAction m_rotateYaw = nullptr;
	XrAction m_rotatePitch = nullptr;
	XrAction m_jumpCrouch = nullptr;
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
	BooleanAction m_nightvision;
	BooleanAction m_melee;
	BooleanAction m_grenades;
	XrSpace m_gripSpace[2] = {};

	XrActionSet m_menuSet = nullptr;
	BooleanAction m_menuClick;
	BooleanAction m_menuBack;

	XrActionSet m_vehicleSet = nullptr;
	BooleanAction m_vecBoost;
	BooleanAction m_vecAfterburner;
	BooleanAction m_vecBrake;
	BooleanAction m_vecHorn;
	BooleanAction m_vecLights;
	BooleanAction m_vecExit;
	BooleanAction m_vecSwitchSeatView;

	float m_gripAmount[2] = {};
	float m_triggerAmount[2] = {};

	bool m_wasJumpActive = false;
	bool m_wasCrouchActive = false;

	static const int MOUSE_SAMPLE_COUNT = 10;
	Vec2 m_hudMousePosSamples[MOUSE_SAMPLE_COUNT];
	int m_curMouseSampleIdx = 0;

	bool m_quickMenuActive = false;
	Vec3 m_quickMenuInitialHandPosition;

	void CreateInputActions();
	void SuggestBindings();
	void AttachActionSets();
	void CreateBooleanAction(XrActionSet actionSet, BooleanAction& action, const char* name, const char* description, ActionId* onPress, ActionId* onLongPress = nullptr, bool sendRelease = true, bool sendLongRelease = true, bool pressOnRelease = false);
	void UpdateIngameActions();
	void UpdateVehicleActions();
	void UpdateMenuActions();
	void UpdateHUDActions();
	void UpdatePlayerMovement();
	void UpdateGripAmount();
	void UpdateWeaponScopes();
	void UpdateBooleanAction(BooleanAction& action);
	void UpdateBooleanActionForMenu(BooleanAction& action, EDeviceId device, EKeyId key);

	bool CalcControllerHudIntersection(int hand, float& x, float& y);

	void DestroyAction(XrAction& action);
};
