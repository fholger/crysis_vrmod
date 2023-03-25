// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------

#ifndef __AIDEMOINPUT_H__
#define __AIDEMOINPUT_H__

#pragma once

#include "IEntitySystem.h"
#include "IPlayerInput.h"

static const float THETA = 3.14159265358979323f / 180.0f * 30;

// special input for dedicated client (simple, generating enough client side traffic for server)
class CDedicatedInput : public IPlayerInput
{
public:
	CDedicatedInput(CPlayer* pPlayer) :
		m_pPlayer(pPlayer),
		m_deltaMovement(0.0f,1.0f,0.0f),
		m_deltaRotation(0.0f,0.0f,THETA),
		m_rand( (uint32)gEnv->pTimer->GetAsyncCurTime() ),
		m_movingTime(5.0f)
	{
	}

	virtual void PreUpdate()	
	{
		if (m_movingTime >= 5.0f)
		{
			m_deltaMovement = -m_deltaMovement;
			m_deltaRotation = -m_deltaRotation;
			m_movingTime = 0.0f;
		}

		m_movingTime += gEnv->pTimer->GetFrameTime();

		m_pPlayer->GetGameObject()->ChangedNetworkState( INPUT_ASPECT );
	}

	virtual void Update()
	{
		CMovementRequest request;
		request.AddDeltaMovement(m_deltaMovement);
		request.AddDeltaRotation(m_deltaRotation);
		float pseudoSpeed = m_pPlayer->CalculatePseudoSpeed(false);
		request.SetPseudoSpeed(pseudoSpeed);
		m_pPlayer->GetMovementController()->RequestMovement(request);

		IItem* pItem = m_pPlayer->GetCurrentItem();
		if (pItem)
		{
			IWeapon* pWeapon = pItem->GetIWeapon();
			if (pWeapon)
			{
				if ( pWeapon->OutOfAmmo(false) )
					pWeapon->SetAmmoCount( pWeapon->GetFireMode(pWeapon->GetCurrentFireMode())->GetAmmoType(), 100 );
				pWeapon->StartFire();
			}
		}
	}

	virtual void PostUpdate()
	{
		SMovementState movementState;
		m_pPlayer->GetMovementController()->GetMovementState(movementState);
		m_serializedInput.deltaMovement = m_deltaMovement;
		m_serializedInput.lookDirection = movementState.eyeDirection;
		m_serializedInput.bodyDirection = movementState.bodyDirection;
		m_serializedInput.stance = movementState.stance;
	}

	virtual void OnAction( const ActionId& action, int activationMode, float value )
	{
	}

	virtual void SetState( const SSerializedPlayerInput& input )
	{
		GameWarning("CDedicatedInput::SetState called - should not happen");
	}

	virtual void GetState( SSerializedPlayerInput& input )
	{
		input = m_serializedInput;
	}

	virtual void Reset()
	{
		m_deltaMovement.Set(0.0f, 1.0f, 0.0f);
		m_deltaRotation.Set(0.0f, 0.0f, THETA);
	}

	virtual void DisableXI(bool disabled)
	{
	}

	virtual EInputType GetType() const
	{
		return DEDICATED_INPUT;
	}

	virtual void GetMemoryStatistics(ICrySizer * pSizer)
	{
		pSizer->Add(*this);
	}

	ILINE virtual uint32 GetMoveButtonsState() const { return 0; }
	ILINE virtual uint32 GetActions() const { return 0; }

private:
	CPlayer* m_pPlayer;
	SSerializedPlayerInput m_serializedInput;

	Vec3 m_deltaMovement;
	Ang3 m_deltaRotation;

	CMTRand_int32 m_rand;

	float m_movingTime;
};

#endif

