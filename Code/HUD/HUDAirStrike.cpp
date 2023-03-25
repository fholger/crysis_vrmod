// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "HUD.h"
#include "GameFlashAnimation.h"
#include "../Actor.h"
#include "IVehicleSystem.h"
#include "IWorldQuery.h"
#include "Weapon.h"
#include "HUDScopes.h"
#include "HUDSilhouettes.h"

//-----------------------------------------------------------------------------------------------------

#undef HUD_CALL_LISTENERS
#define HUD_CALL_LISTENERS(func) \
{ \
	if (m_hudListeners.empty() == false) \
	{ \
		m_hudTempListeners = m_hudListeners; \
		for (std::vector<IHUDListener*>::iterator tmpIter = m_hudTempListeners.begin(); tmpIter != m_hudTempListeners.end(); ++tmpIter) \
			(*tmpIter)->func; \
	} \
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::IsAirStrikeAvailable()
{
	return m_bAirStrikeAvailable;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetAirStrikeEnabled(bool bEnabled)
{
	if(bEnabled)
	{
		if(!m_animAirStrike.IsLoaded())
		{
			m_animAirStrike.Load("Libs/UI/HUD_AirStrikeLocking_Text.gfx",eFD_Center,eFAF_Visible|eFAF_ManualRender);
			SetFlashColor(&m_animAirStrike);
		}
		m_animAirStrike.Invoke("enableAirStrike",true);
	}
	else
	{
		m_animAirStrike.Unload();
		ClearAirstrikeEntities();
	}

	m_bAirStrikeAvailable = bEnabled;
	SetAirStrikeBinoculars(m_pHUDScopes->IsBinocularsShown());
}

//-----------------------------------------------------------------------------------------------------

void CHUD::AddAirstrikeEntity(EntityId uiEntityId)
{
	if(!stl::find(m_possibleAirStrikeTargets, uiEntityId))
		m_possibleAirStrikeTargets.push_back(uiEntityId);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ClearAirstrikeEntities()
{
	std::vector<EntityId>::const_iterator it = m_possibleAirStrikeTargets.begin();
	for(; it != m_possibleAirStrikeTargets.end(); ++it)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(*it);
		if(pEntity)
			UnlockTarget(*it);
	}
	m_possibleAirStrikeTargets.clear();
}

//-----------------------------------------------------------------------------------------------------

void CHUD::NotifyAirstrikeSucceeded(bool bSucceeded)
{
	if(!bSucceeded)
	{
		m_animAirStrike.Invoke("stopCountdown");
		m_fAirStrikeStarted = 0.0f;
		SetAirStrikeEnabled(true);
		EntityId id = 0;
		UpdateAirStrikeTarget(id);
	}
	else
	{
		EntityId id = 0;
		UpdateAirStrikeTarget(id);
		SetAirStrikeEnabled(false);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetAirStrikeBinoculars(bool bEnabled)
{
	if(!m_animAirStrike.IsLoaded())
		return;
	m_animAirStrike.Invoke("setBinoculars",bEnabled);

	if(bEnabled)
	{
		std::vector<EntityId>::const_iterator it = m_possibleAirStrikeTargets.begin();
		for(; it != m_possibleAirStrikeTargets.end(); ++it)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(*it);
			if(pEntity)
				LockTarget(*it, eLT_Locked, false, true);
		}
	}
	else
	{
		std::vector<EntityId>::const_iterator it = m_possibleAirStrikeTargets.begin();
		for(; it != m_possibleAirStrikeTargets.end(); ++it)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(*it);
			if(pEntity)
				UnlockTarget(*it);
		}
		EntityId id = 0;
		UpdateAirStrikeTarget(id);
	}
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::StartAirStrike()
{
	if(!m_animAirStrike.IsLoaded())
		return false;

	CCamera camera=GetISystem()->GetViewCamera();

	IActor *pActor=gEnv->pGame->GetIGameFramework()->GetClientActor();
	IPhysicalEntity *pSkipEnt=pActor?pActor->GetEntity()->GetPhysics():0;

	Vec3 dir=(camera.GetViewdir())*500.0f;

	std::vector<EntityId>::const_iterator it = m_possibleAirStrikeTargets.begin();
	for(; it != m_possibleAirStrikeTargets.end(); ++it)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(*it);
		if(pEntity)
		{
			IPhysicalEntity *pPE=pEntity->GetPhysics();
			if(pPE)
			{
				ray_hit hit;

				if (gEnv->pPhysicalWorld->RayWorldIntersection(camera.GetPosition(), dir, ent_all, (13&rwi_pierceability_mask), &hit, 1, &pSkipEnt, pSkipEnt?1:0))
				{
					if (!hit.bTerrain && hit.pCollider==pPE)
					{
						IEntity *pHitEntity = gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider);
						if (pHitEntity == pEntity)
						{
							UpdateAirStrikeTarget(pHitEntity->GetId());
							LockTarget(pHitEntity->GetId(),eLT_Locking, false);
							m_animAirStrike.Invoke("startCountdown");
							HUD_CALL_LISTENERS(OnAirstrike(1,pHitEntity->GetId()));
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UpdateAirStrikeTarget(EntityId uiTargetEntityId)
{
	if(!m_animAirStrike.IsLoaded())
		return;
	if(stl::find(m_possibleAirStrikeTargets, uiTargetEntityId))
	{
		if(m_iAirStrikeTarget)
		{
			LockTarget(m_iAirStrikeTarget,eLT_Locked, false);
			LockTarget(uiTargetEntityId,eLT_Locking, false);
		}
		m_iAirStrikeTarget = uiTargetEntityId;
		m_animAirStrike.Invoke("setTarget", (uiTargetEntityId!=0));
	}
	else
	{
		m_iAirStrikeTarget = 0;
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::LockTarget(EntityId uiTargetEntityId, ELockingType eType, bool bShowText, bool bMultiple)
{
	if(!bMultiple)
	{
		if(m_entityTargetAutoaimId && uiTargetEntityId!=m_entityTargetAutoaimId)
		{
			UnlockTarget(m_entityTargetAutoaimId);
		}

		m_entityTargetAutoaimId = uiTargetEntityId;
	}

	if(eType == eLT_Locked)
	{
		m_pHUDScopes->m_animSniperScope.Invoke("setLocking",2);
	}
	else if(eType == eLT_Locking)
	{
		m_pHUDScopes->m_animSniperScope.Invoke("setLocking",1);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UnlockTarget(EntityId uiTargetEntityId)
{
	m_pHUDSilhouettes->ResetSilhouette(uiTargetEntityId);
	m_pHUDSilhouettes->ResetSilhouette(m_entityTargetAutoaimId);

	m_pHUDScopes->m_animSniperScope.Invoke("setLocking",3);
	
	m_entityTargetAutoaimId = 0;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::DrawAirstrikeTargetMarkers()
{
	if(!m_pHUDScopes->IsBinocularsShown() || !IsAirStrikeAvailable())
		return;

	float fCos = cosf(gEnv->pTimer->GetAsyncCurTime());
	fCos = fabsf(fCos);

	int amount = m_possibleAirStrikeTargets.size();
	for(int i = 0; i < amount; ++i)
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_possibleAirStrikeTargets[i]);
		m_pHUDSilhouettes->SetSilhouette(pEntity, 1.0f-0.6f*fCos, 1.0f-0.4f*fCos, 1.0f-0.20f*fCos, 0.5f, 1.5f);
	}
}

//-----------------------------------------------------------------------------------------------------