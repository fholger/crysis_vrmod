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
#include "VehicleDamageBehaviorBurn.h"
#include "Game.h"
#include "GameRules.h"

//------------------------------------------------------------------------
CVehicleDamageBehaviorBurn::~CVehicleDamageBehaviorBurn()
{
	if (gEnv->pAISystem)
	  gEnv->pAISystem->RegisterDamageRegion(this, Sphere(ZERO, -1.0f)); // disable
}


//------------------------------------------------------------------------
bool CVehicleDamageBehaviorBurn::Init(IVehicle* pVehicle, const SmartScriptTable &table)
{
	m_pVehicle = pVehicle;
	m_isActive = false;
  m_damageRatioMin = 1.f;
  m_timerId = -1;

	m_shooterId = 0;

  table->GetValue("damageRatioMin", m_damageRatioMin);

	SmartScriptTable burnParams;
	if (table->GetValue("Burn", burnParams))
	{
		burnParams->GetValue("damage", m_damage);
    burnParams->GetValue("selfDamage", m_selfDamage);
		burnParams->GetValue("interval", m_interval);
		burnParams->GetValue("radius", m_radius);

		m_pHelper = NULL;

		char* pHelperName = NULL;
		if (burnParams->GetValue("helper", pHelperName))
			m_pHelper = m_pVehicle->GetHelper(pHelperName);

		return true;
	}

	return false;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBurn::Reset()
{
	if (m_isActive)
	{
		Activate(false);
	}

	m_shooterId = 0;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBurn::Activate(bool activate)
{
  if (activate && !m_isActive)
  {
    m_timeCounter = m_interval;
    m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
    m_timerId = m_pVehicle->SetTimer(-1, 20000, this); // total burn of 60 secs

    if (!m_pVehicle->IsDestroyed() && !m_pVehicle->IsFlipped() && gEnv->pAISystem )
      gEnv->pAISystem->SetSmartObjectState(m_pVehicle->GetEntity(), "Exploding");        

	m_pVehicle->NeedsUpdate(IVehicle::eVUF_AwakePhysics);
  }
  else if (!activate && m_isActive)
  {
    m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
    m_pVehicle->KillTimer(m_timerId);
    m_timerId = -1;

		if (gEnv->pAISystem)
	    gEnv->pAISystem->RegisterDamageRegion(this, Sphere(ZERO, -1.0f)); // disable
  }

  m_isActive = activate;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBurn::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
  if (event == eVDBE_Repair)
  {
    if (behaviorParams.componentDamageRatio < m_damageRatioMin)
      Activate(false);

		m_shooterId = 0;
  }
	else 
	{
    if (behaviorParams.componentDamageRatio >= m_damageRatioMin)
	    Activate(true);

		m_shooterId = behaviorParams.shooterId;
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBurn::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
  switch (event)
  {
  case eVE_Timer:
    if (params.iParam == m_timerId)
      Activate(false);
    break;
  }
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBurn::Update(const float deltaTime)
{
	m_timeCounter -= deltaTime;

	if (m_timeCounter <= 0.0f)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		if (pGameRules && gEnv->bServer)
		{	
			Vec3 worldPos;
			if (m_pHelper)
				worldPos = m_pHelper->GetWorldTM().GetTranslation();
			else
				worldPos = m_pVehicle->GetEntity()->GetWorldTM().GetTranslation();

      SEntityProximityQuery query;
      query.box = AABB(worldPos-Vec3(m_radius), worldPos+Vec3(m_radius));
      gEnv->pEntitySystem->QueryProximity(query);

      IEntity* pEntity = 0;
      
			for (int i = 0; i < query.nCount; ++i)
			{				
				if ((pEntity = query.pEntities[i]) && pEntity->GetPhysics())
				{
          float damage = (pEntity->GetId() == m_pVehicle->GetEntityId()) ? m_selfDamage : m_damage;

					// SNH: need to check vertical distance here as the QueryProximity() call seems to work in 2d only
					Vec3 pos = pEntity->GetWorldPos();
					if(abs(pos.z - worldPos.z) < m_radius)
					{
						if (damage > 0.f)
						{
							HitInfo hitInfo(m_shooterId, pEntity->GetId(), m_pVehicle->GetEntityId(), -1, m_radius);
							hitInfo.damage = damage;
							hitInfo.pos = worldPos;
							hitInfo.type = pGameRules->GetHitTypeId("fire");

							pGameRules->ServerHit(hitInfo);
						}   
					}
				}
			}
			
			if (gEnv->pAISystem)
	      gEnv->pAISystem->RegisterDamageRegion(this, Sphere(worldPos, m_radius));
		}

		m_timeCounter = m_interval;
	}

  m_pVehicle->NeedsUpdate();
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBurn::Serialize(TSerialize ser, unsigned aspects)
{
	if(ser.GetSerializationTarget()!=eST_Network)
	{
		bool active = m_isActive;
		ser.Value("Active", active);
		ser.Value("time", m_timeCounter);
		ser.Value("shooterId", m_shooterId);

    if (ser.IsReading())
    {
			if(active != m_isActive)
				Activate(active);
    }
	}
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorBurn);
