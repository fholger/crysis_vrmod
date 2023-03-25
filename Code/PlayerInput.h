// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
// handles turning actions into CMovementRequests and setting player state
// for the local player

#ifndef __PLAYERINPUT_H__
#define __PLAYERINPUT_H__

#pragma once

#include "IActionMapManager.h"
#include "IPlayerInput.h"
#include "Player.h"

class CPlayer;
struct SPlayerStats;

class CPlayerInput : public IPlayerInput, public IActionListener, public IPlayerEventListener
{
public:

	enum EMoveButtonMask
	{
		eMBM_Forward	= (1 << 0),
		eMBM_Back			= (1 << 1),
		eMBM_Left			= (1 << 2),
		eMBM_Right		= (1 << 3)
	};

	CPlayerInput( CPlayer * pPlayer );
	~CPlayerInput();

	// IPlayerInput
	virtual void PreUpdate();
	virtual void Update();
	virtual void PostUpdate();
	// ~IPlayerInput

	// IActionListener
	virtual void OnAction( const ActionId& action, int activationMode, float value );
	// ~IActionListener
	
	// IPlayerEventListener
	virtual void OnObjectGrabbed(IActor* pActor, bool bIsGrab, EntityId objectId, bool bIsNPC, bool bIsTwoHanded);
	// ~IPlayerEventListener

	virtual void SetState( const SSerializedPlayerInput& input );
	virtual void GetState( SSerializedPlayerInput& input );

	virtual void Reset();
	virtual void DisableXI(bool disabled);

	virtual void GetMemoryStatistics(ICrySizer * s) {s->Add(*this);}

	virtual EInputType GetType() const
	{
		return PLAYER_INPUT;
	};

	ILINE virtual uint32 GetMoveButtonsState() const { return m_moveButtonState; }
	ILINE virtual uint32 GetActions() const { return m_actions; }

	// ~IPlayerInput

	void SerializeSaveGame( TSerialize ser );

private:

	EStance FigureOutStance();
	void AdjustMoveButtonState( EMoveButtonMask buttonMask, int activationMode );
	bool CheckMoveButtonStateChanged( EMoveButtonMask buttonMask, int activationMode );
	float MapControllerValue(float value, float scale, float curve, bool inverse);

	void ApplyMovement(Vec3 delta);
	const Vec3 &FilterMovement(const Vec3 &desired);

	bool CanMove() const;

	bool OnActionMoveForward(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionMoveBack(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionMoveLeft(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionMoveRight(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionVRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value); // needed so player can shake unfreeze while in a vehicle
	bool OnActionVRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionJump(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionCrouch(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionSprint(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionSuitMode(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionSuitSkin(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionToggleStance(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionProne(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	//bool OnActionZeroGBrake(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionGyroscope(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionGBoots(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionLeanLeft(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionLeanRight(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	//bool OnActionHolsterItem(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionUse(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	// Nanosuit
	bool OnActionSpeedMode(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionStrengthMode(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionDefenseMode(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionSuitCloak(EntityId entityId, const ActionId& actionId, int activationMode, float value);

	// Cheats
	bool OnActionThirdPerson(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionFlyMode(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionGodMode(EntityId entityId, const ActionId& actionId, int activationMode, float value);

	bool OnActionXIRotateYaw(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionXIRotatePitch(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionXIMoveX(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionXIMoveY(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionXIDisconnect(EntityId entityId, const ActionId& actionId, int activationMode, float value);

	bool OnActionInvertMouse(EntityId entityId, const ActionId& actionId, int activationMode, float value);
private:
	Vec3 m_lastPos;

	CPlayer* m_pPlayer;
	SPlayerStats* m_pStats;
	uint32 m_actions;
	uint32 m_lastActions;
	Vec3 m_deltaMovement;
	Vec3 m_xi_deltaMovement;
  Vec3 m_deltaMovementPrev;
	Ang3 m_deltaRotation;
	Ang3 m_xi_deltaRotation;
	float m_speedLean;
	float	m_buttonPressure;
	bool m_bDisabledXIRot;
	float	m_fCrouchPressedTime;
	float m_binocularsTime;
	bool m_bUseXIInput;
	uint32 m_moveButtonState;
	Vec3 m_filteredDeltaMovement;
	bool m_checkZoom;
	float m_fSuitModeActionTime;
	int	m_iSuitModeActionPressed;
	int m_iCarryingObject;
	int m_lastSerializeFrameID;

	bool m_doubleJumped;

	static TActionHandler<CPlayerInput>	s_actionHandler;
};

#endif
