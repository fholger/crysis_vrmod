/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a damage behavior which improves collision damages 

-------------------------------------------------------------------------
History:
- 06:10:2007: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "IVehicleSystem.h"
#include "VehicleDamageBehaviorCollisionEx.h"
#include "Game.h"
#include "GameRules.h"

//------------------------------------------------------------------------
CVehicleDamageBehaviorCollisionEx::~CVehicleDamageBehaviorCollisionEx()
{
	m_pVehicle->UnregisterVehicleEventListener(this);
}

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorCollisionEx::Init(IVehicle* pVehicle, const SmartScriptTable &table)
{
	m_pVehicle = pVehicle;

	//<CollisionEx component="CollisionDamages" damages="500">

	SmartScriptTable collisionParams;
	if (!table->GetValue("CollisionEx", collisionParams))
		return false;

	char* pComponentName;
	if (!collisionParams->GetValue("component", pComponentName))
		return false;

	m_componentName = pComponentName;

	if (!collisionParams->GetValue("damages", m_damages))
		return false;

	m_pVehicle->RegisterVehicleEventListener(this, "CollisionEx");
	return true;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorCollisionEx::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	// SNH: only apply damage on the server
	if (event == eVE_Collision && gEnv->bServer)
	{
		Vec3 localPos = m_pVehicle->GetEntity()->GetWorldTM().GetInverted() * params.vParam;

		IVehicleComponent* pComponent = m_pVehicle->GetComponent(m_componentName);

		if (pComponent->GetBounds().IsContainPoint(localPos))
		{
			float damages = max(1.0f, m_damages * params.fParam2);

			if (!params.entityId && damages > 0.0f)
			{
				m_pVehicle->OnHit(m_pVehicle->GetEntityId(), 0, damages, params.vParam, 5.0f, "collision", false);
			}
		}
	}
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorCollisionEx);
