/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a vehicle action for landing gears

-------------------------------------------------------------------------
History:
- 02:06:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "IVehicleSystem.h"
#include "VehicleActionLandingGears.h"
#include "Game.h"
#include "GameUtils.h"

const float GEARS_EXTRACTED_TIME = 0.0f;
const float GEARS_RETRACTED_TIME = 1.0f;

//------------------------------------------------------------------------
CVehicleActionLandingGears::CVehicleActionLandingGears()
: m_pVehicle(NULL),
	m_pLandingGearsAnim(NULL),
	m_landingGearOpenedId(InvalidVehicleAnimStateId),
	m_landingGearClosedId(InvalidVehicleAnimStateId),
	m_isDestroyed(false),
	m_damageReceived(0.0f),
	m_pPartToBlockRotation(NULL),
	m_velocityMax(0.0f),
	m_isOnlyAutoForPlayer(false),
	m_isDriverPlayer(false),
	m_animTime(0.0f),
	m_animGoal(0.0f),
	m_timer(0.0f),
	m_minTimeForChange(0.5f)
{

}

//------------------------------------------------------------------------
CVehicleActionLandingGears::~CVehicleActionLandingGears()
{
	m_pVehicle->UnregisterVehicleEventListener(this);
}

//------------------------------------------------------------------------
bool CVehicleActionLandingGears::Init(IVehicle* pVehicle, const SmartScriptTable &table)
{
	m_pVehicle = pVehicle;
	m_pLandingGearsAnim = m_pVehicle->GetAnimation("landingGears");

	m_altitudeToRetractGears = 10.0f;
	m_landingDamages = 0.0f;

	SmartScriptTable landingGearsTable;
	if (table->GetValue("LandingGears", landingGearsTable))
	{
		landingGearsTable->GetValue("altitudeToRetractGears", m_altitudeToRetractGears);
		landingGearsTable->GetValue("landingDamages", m_landingDamages);
		landingGearsTable->GetValue("velocityMax", m_velocityMax);
		
		char* pPartName = NULL;
		if (landingGearsTable->GetValue("blockPartRotation", pPartName))
		{
			m_pPartToBlockRotation = m_pVehicle->GetPart(pPartName);

			if (m_pPartToBlockRotation)
			{
				IVehiclePart::SVehiclePartEvent partEvent;
				partEvent.type = IVehiclePart::eVPE_BlockRotation;
				partEvent.bparam = true;

				m_pPartToBlockRotation->OnEvent(partEvent);
			}
		}

		landingGearsTable->GetValue("isOnlyAutoForPlayer", m_isOnlyAutoForPlayer);
	}

	if (!m_pLandingGearsAnim)
		return false;

	m_pLandingGearsAnim->StartAnimation();
	m_pLandingGearsAnim->ToggleManualUpdate(true);
	//m_pLandingGearsAnim->SetTime(GEARS_EXTRACTED_TIME);

	m_landingGearOpenedId = m_pLandingGearsAnim->GetStateId("opened");
	m_landingGearClosedId = m_pLandingGearsAnim->GetStateId("closed");

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_PassengerUpdate);
	m_pVehicle->RegisterVehicleEventListener(this, "CVehicleActionLandingGears");

	return true;
}

//------------------------------------------------------------------------
void CVehicleActionLandingGears::Reset()
{
	m_isDestroyed = false;
	m_damageReceived = 0.0f;

	if (m_pPartToBlockRotation)
	{
		IVehiclePart::SVehiclePartEvent partEvent;
		partEvent.type = IVehiclePart::eVPE_BlockRotation;
		partEvent.bparam = true;

		m_pPartToBlockRotation->OnEvent(partEvent);
	}

	m_animGoal = 0.0f;
	m_animTime = 0.0f;
	m_timer = 0.0f;

	m_pLandingGearsAnim->StopAnimation();
	m_pLandingGearsAnim->StartAnimation();
	m_pLandingGearsAnim->ToggleManualUpdate(true);
	m_pLandingGearsAnim->SetTime(GEARS_EXTRACTED_TIME);
}

//------------------------------------------------------------------------
void CVehicleActionLandingGears::OnVehicleEvent(EVehicleEvent eventType, const SVehicleEventParams& params)
{
	if (eventType == eVE_PassengerEnter || eventType == eVE_PassengerChangeSeat)
	{
		IVehicleSeat* pSeat = m_pVehicle->GetSeatById(params.iParam);
		assert(pSeat);

		if (pSeat->IsDriver())
		{
			IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
			assert(pActorSystem);

			if (IActor* pActor = pActorSystem->GetActor(pSeat->GetPassenger(false)))
			{
				m_isDriverPlayer = pActor->IsPlayer();
			}
		}
	}
	else if (eventType == eVE_Destroyed)
	{
		m_pLandingGearsAnim->StopAnimation();
		m_isDestroyed = true;
	}
	else if (eventType == eVE_ExtractGears)
	{
		m_isOnlyAutoForPlayer = true;
		ExtractGears();
	}
	else if (eventType == eVE_RetractGears)
	{
		m_isOnlyAutoForPlayer = false;
		RetractGears();
	}
}

//------------------------------------------------------------------------
int CVehicleActionLandingGears::OnEvent(int eventType, SVehicleEventParams& eventParams)
{
	const float velThresold = 6.0f;

	if (eventType == eVAE_OnGroundCollision || eventType == eVAE_OnEntityCollision)
	{

		if (!m_isDriverPlayer)
			return 0;	// no collision damage for ai

		float velLen = 0.0f;

		if (IPhysicalEntity* pPhysEntity = m_pVehicle->GetEntity()->GetPhysics())
		{
			pe_status_dynamics dyn;
			if (pPhysEntity->GetStatus(&dyn))
				velLen = dyn.v.GetLength();
		}

		if (velLen < velThresold)
			return 1;

		TVehicleAnimStateId animStateId = m_pLandingGearsAnim->GetState();
		if (animStateId != InvalidVehicleAnimStateId)
		{
			float gearStatus = 0.0f; // 0 = in, 1 = out
			if (m_isDestroyed)	
				gearStatus = 0.0f;
			else if (animStateId == m_landingGearClosedId)
				gearStatus = 0.0f;
			else if (animStateId == m_landingGearOpenedId)
				gearStatus = min(1.0f, max(0.0f, m_pLandingGearsAnim->GetAnimTime()));

			if (gearStatus < 1.0f)
			{
				EntityId shooterId = m_pVehicle->GetEntityId();

				if (IVehicleSeat* pSeat = m_pVehicle->GetSeatById(1))
					if (pSeat->GetPassenger() > 0)
						shooterId = pSeat->GetPassenger();

				float damage;

				if (gearStatus < 0.85f)
				{
					// we got some damages, but did we break the landing gears?
					//m_pLandingGearsAnim->StopAnimation();
					//m_isDestroyed = true;

					damage = m_landingDamages;
				}
				else
				{
					damage = m_landingDamages * 0.25f;
				}

				if (damage > m_damageReceived)
				{
					m_pVehicle->OnHit(m_pVehicle->GetEntityId(), shooterId, damage, Vec3(0.0f, 0.0f, 0.0f), 1.0f, "landingCollision", false);
					m_damageReceived = damage;

					if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
					{
						SVehicleMovementEventParams movementEventParams;
						movementEventParams.fValue = 0.5f;
						pMovement->OnEvent(IVehicleMovement::eVME_Damage, movementEventParams);
					}
				}
			}
		}

		return 1;
	}

	return 0;
}

//------------------------------------------------------------------------
void CVehicleActionLandingGears::Serialize(TSerialize ser, unsigned aspects)
{
	ser.Value("damageReceived", m_damageReceived);
	ser.Value("isDestroyed", m_isDestroyed);
	ser.Value("animTime", m_animTime);
	
	if (ser.IsReading())
	{
		m_pLandingGearsAnim->StartAnimation();
		m_pLandingGearsAnim->ToggleManualUpdate(true);
		m_pLandingGearsAnim->SetTime(m_animTime);
	}
}

//------------------------------------------------------------------------
void CVehicleActionLandingGears::Update(const float deltaTime)
{
	if (m_isDestroyed)
		return;

	float altitude = m_pVehicle->GetAltitude();

	if (!m_isOnlyAutoForPlayer || m_isDriverPlayer)
	{
		if (altitude > m_altitudeToRetractGears)
			RetractGears();
		else
			ExtractGears();
	}

	if (m_timer > m_minTimeForChange)
	{
		float speed = 0.65f;

		if (IPhysicalEntity* pPhysEntity = m_pVehicle->GetEntity()->GetPhysics())
		{
			pe_status_dynamics dyn;
			if (pPhysEntity->GetStatus(&dyn))
				speed += min(abs(dyn.v.GetLength()) * 0.1f, 1.0f);
		}

		Interpolate(m_animTime, m_animGoal, speed, deltaTime);
		m_pLandingGearsAnim->SetTime(m_animTime);
	}
	else
	{
		m_timer += deltaTime;
	}
}

//------------------------------------------------------------------------
void CVehicleActionLandingGears::ExtractGears()
{
	float velLength = m_pVehicle->GetStatus().vel.GetLength();

	if (iszero(m_velocityMax) || velLength <= m_velocityMax)
	{
		if (m_animGoal != GEARS_EXTRACTED_TIME)
			m_timer = 0.0f;

		m_animGoal = GEARS_EXTRACTED_TIME;

		if (m_pPartToBlockRotation)
		{
			IVehiclePart::SVehiclePartEvent partEvent;

			partEvent.type = IVehiclePart::eVPE_BlockRotation;
			partEvent.bparam = true;

			m_pPartToBlockRotation->SetMoveable();
			m_pPartToBlockRotation->OnEvent(partEvent);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleActionLandingGears::RetractGears()
{
	if (m_animGoal != GEARS_RETRACTED_TIME)
		m_timer = 0.0f;

	m_animGoal = GEARS_RETRACTED_TIME;

	if (m_pPartToBlockRotation)
	{
		IVehiclePart::SVehiclePartEvent partEvent;
		partEvent.type = IVehiclePart::eVPE_BlockRotation;
		partEvent.bparam = false;

		m_pPartToBlockRotation->OnEvent(partEvent);
	}
}

//------------------------------------------------------------------------
bool CVehicleActionLandingGears::AreLandingGearsOpen()
{
	//return (m_pLandingGearsAnim->GetState() == m_landingGearOpenedId);
	return (m_animGoal < 1.0f);
}

DEFINE_VEHICLEOBJECT(CVehicleActionLandingGears);
