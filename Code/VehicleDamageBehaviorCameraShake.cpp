/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a damage behavior which gives camera shake to the 
player

-------------------------------------------------------------------------
History:
- 31:07:2007: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "IVehicleSystem.h"
#include "VehicleDamageBehaviorCameraShake.h"
#include "Game.h"
#include "GameRules.h"

//m_damageRatio
//------------------------------------------------------------------------
CVehicleDamageBehaviorCameraShake::CVehicleDamageBehaviorCameraShake()
: m_damageMult(1.0f)
{
}

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorCameraShake::Init(IVehicle* pVehicle, const SmartScriptTable &table)
{
	m_pVehicle = pVehicle;
	
	if (gEnv->bClient)
		m_pVehicle->RegisterVehicleEventListener(this, "VehicleDamageBehaviorCameraShake");
	return true;
}

//------------------------------------------------------------------------
CVehicleDamageBehaviorCameraShake::~CVehicleDamageBehaviorCameraShake()
{
	if (gEnv->bClient)
		m_pVehicle->UnregisterVehicleEventListener(this);
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorCameraShake::Reset()
{
	m_damageMult = 1.0f;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorCameraShake::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
	if (event == eVDBE_Hit)
	{
		m_damageMult = max(1.0f, behaviorParams.componentDamageRatio / 0.25f);
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorCameraShake::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	if (!gEnv->bClient)
		return;

	if (event == eVE_Damaged)
	{
		float angle = 10.0f * (m_damageMult);
		ShakeClient(angle, 0.2f, 0.05f, 0.25f);
	}
	else if (event == eVE_Hit)
	{
		float angle = 2.0f * (m_damageMult);
		ShakeClient(5.0f, 0.10f, 0.1f, 0.05f);
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorCameraShake::ShakeClient(float angle, float shift, float duration, float frequency)
{
	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	assert(pActorSystem);

	EntityId clientId = g_pGame->GetIGameFramework()->GetClientActorId();

	for (TVehicleSeatId seatId = InvalidVehicleSeatId + 1; 
		seatId != m_pVehicle->GetLastSeatId(); seatId++)
	{
		if (IVehicleSeat* pSeat = m_pVehicle->GetSeatById(seatId))
		{
			EntityId passengerId = pSeat->GetPassenger();

			if (passengerId == clientId)
			{
				CActor* pActor = static_cast<CActor*>(pActorSystem->GetActor(passengerId));
				if(pActor)
					pActor->CameraShake(angle, shift, duration, frequency, Vec3(0.0f, 0.0f, 0.0f), 5, "VehicleDamageBehaviorCameraShake");
			}
		}
	}
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorCameraShake);
