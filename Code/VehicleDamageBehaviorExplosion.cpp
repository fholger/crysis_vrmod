/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a damage behavior which create an explosion

-------------------------------------------------------------------------
History:
- 03:28:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "IVehicleSystem.h"
#include "VehicleDamageBehaviorExplosion.h"
#include "Game.h"
#include "GameRules.h"
#include "Environment/BattleDust.h"

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorExplosion::Init(IVehicle* pVehicle, const SmartScriptTable &table)
{
	m_pVehicle = pVehicle;
  m_exploded = false;

	SmartScriptTable explosionParams;
	if (table->GetValue("Explosion", explosionParams))
	{
		explosionParams->GetValue("damage", m_damage);
		explosionParams->GetValue("radius", m_radius);
		if (!explosionParams->GetValue("minRadius", m_minRadius))
			m_minRadius = m_radius/2.0f;
		if (!explosionParams->GetValue("physRadius", m_physRadius))
			m_physRadius = min(m_radius, 5.0f);
		if (!explosionParams->GetValue("minPhysRadius", m_minPhysRadius))
			m_minPhysRadius = m_physRadius/2.0f;
		explosionParams->GetValue("pressure", m_pressure);

		m_pHelper = NULL;

		char* pHelperName = NULL;
		if (explosionParams->GetValue("helper", pHelperName))
			m_pHelper = m_pVehicle->GetHelper(pHelperName);

		return true;
	}

	return false;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorExplosion::Reset()
{
  m_exploded = false;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorExplosion::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
  if (event == eVDBE_Repair)
    return;

	if (!m_exploded && behaviorParams.componentDamageRatio >= 1.0f)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		if (pGameRules && gEnv->bServer)
		{
			ExplosionInfo explosionInfo;
			explosionInfo.damage = m_damage;
			explosionInfo.damage += (behaviorParams.randomness) * Random(-0.25f, 0.25f) * m_damage;

			if (m_pHelper)
				explosionInfo.pos = m_pHelper->GetWorldTM().GetTranslation();
      else if (behaviorParams.pVehicleComponent)
        explosionInfo.pos = m_pVehicle->GetEntity()->GetWorldTM() * behaviorParams.pVehicleComponent->GetBounds().GetCenter();
			else
				explosionInfo.pos = m_pVehicle->GetEntity()->GetWorldTM().GetTranslation();

			explosionInfo.radius = m_radius;
			explosionInfo.minRadius = m_minRadius;
			explosionInfo.physRadius = m_physRadius;
			explosionInfo.minPhysRadius = m_minPhysRadius;
			explosionInfo.shooterId = behaviorParams.shooterId;
			explosionInfo.weaponId = m_pVehicle->GetEntityId();
			explosionInfo.pressure = m_pressure;
			pGameRules->ServerExplosion(explosionInfo);

			if(CBattleDust* pBD = pGameRules->GetBattleDust())
				pBD->RecordEvent(eBDET_VehicleExplosion, explosionInfo.pos, m_pVehicle->GetEntity()->GetClass());
		}

    m_exploded = true;
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorExplosion::Serialize(TSerialize ser, unsigned aspects)
{
  if (ser.GetSerializationTarget() != eST_Network)
  {
    ser.Value("exploded", m_exploded);
  }
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorExplosion);
