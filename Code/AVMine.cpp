/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  Anti-Vehicle mine implementation
-------------------------------------------------------------------------
History:
- 22:1:2007   14:39 : Created by Steve Humphreys

*************************************************************************/

#include "StdAfx.h"
#include "AVMine.h"

#include "Game.h"
#include "GameRules.h"
#include "HUD/HUD.h"
#include "Player.h"

#include "IEntityProxy.h"


//------------------------------------------------------------------------
CAVMine::CAVMine()
: m_currentWeight(0)
, m_triggerWeight(100)
, m_teamId(0)
, m_frozen(false)
{
}

//------------------------------------------------------------------------
CAVMine::~CAVMine()
{
	if(gEnv->bMultiplayer && gEnv->bServer)
	{
		IActor* pOwner = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_ownerId);
		if(pOwner && pOwner->IsPlayer())
		{
			((CPlayer*)pOwner)->RecordExplosiveDestroyed(GetEntityId(), 1);
		}
	}

	if(g_pGame->GetHUD())
		g_pGame->GetHUD()->RecordExplosiveDestroyed(GetEntityId());
}

//------------------------------------------------------------------------
bool CAVMine::Init(IGameObject *pGameObject)
{
	bool ok = CProjectile::Init(pGameObject);

	if(g_pGame->GetHUD())
		g_pGame->GetHUD()->RecordExplosivePlaced(GetEntityId());

	// if not already a hit listener, register us.
	//	(removed in ~CProjectile )
	if(!m_hitListener)
	{
		g_pGame->GetGameRules()->AddHitListener(this);
		m_hitListener = true;
	}

	return ok;
}


//------------------------------------------------------------------------

void CAVMine::Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale)
{
	// sit flat on the ground.
	Vec3 newDir = dir;
	newDir.z = 0.0f;

	CProjectile::Launch(pos, newDir, velocity, speedScale);

	if(gEnv->bMultiplayer && gEnv->bServer)
	{
		CActor* pOwner = GetWeapon()->GetOwnerActor();
		if(pOwner && pOwner->IsPlayer())
		{
			((CPlayer*)pOwner)->RecordExplosivePlaced(GetEntityId(), 1);
		}
	}

	float boxDimension = 3;
	m_triggerWeight = GetParam("triggerweight", m_triggerWeight);
	boxDimension = GetParam("box_dimension", boxDimension);

	if(gEnv->bServer)
	{
		IEntityTriggerProxy *pTriggerProxy = (IEntityTriggerProxy*)(GetEntity()->GetProxy(ENTITY_PROXY_TRIGGER));
		
		if (!pTriggerProxy)
		{
			GetEntity()->CreateProxy(ENTITY_PROXY_TRIGGER);
			pTriggerProxy = (IEntityTriggerProxy*)GetEntity()->GetProxy(ENTITY_PROXY_TRIGGER);
		}

		if(pTriggerProxy)
		{
			// increase box in the z direction to cope with big vehicles passing over the top (eg NK truck)
			AABB boundingBox = AABB(Vec3(-boxDimension,-boxDimension,0), Vec3(boxDimension,boxDimension,(boxDimension+1)));
			pTriggerProxy->SetTriggerBounds(boundingBox);
		}
	}
}


void CAVMine::HandleEvent(const SGameObjectEvent &event)
{
	CProjectile::HandleEvent(event);

	if (event.event==eCGE_PostFreeze)
		m_frozen=event.param!=0;
}


void CAVMine::ProcessEvent(SEntityEvent &event)
{
	if (m_frozen)
		return;

	switch(event.event)
	{
		case ENTITY_EVENT_ENTERAREA:
		{
			IEntity * pEntity = gEnv->pEntitySystem->GetEntity(event.nParam[0]);
			CGameRules* pGR = g_pGame->GetGameRules();
			if(pEntity && pGR)
			{
				// if this is a team game, mines aren't set off by their own team
				if(pGR->GetTeamCount() > 0 && (m_teamId != 0 && pGR->GetTeam(pEntity->GetId()) == m_teamId))
					break;

				// otherwise, not set off by the player who dropped them.
				if(pGR->GetTeamCount() == 0 && m_ownerId == pEntity->GetId())
					break;

				// or a vehicle that player might happen to be in
				IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(event.nParam[0]);
				if(pVehicle && pVehicle->GetSeatForPassenger(m_ownerId))
					break;

				IPhysicalEntity *pPhysics = pEntity->GetPhysics();
				if(pPhysics)
				{
					pe_status_dynamics physStatus;
					if(0 != pPhysics->GetStatus(&physStatus))
					{
						// only count moving objects
						if(physStatus.v.GetLengthSquared() > 0.1f)
							m_currentWeight += physStatus.mass;

						if (m_currentWeight > m_triggerWeight)
							Explode(true);
					}
				}
			}
			break;
		}
		

		case ENTITY_EVENT_LEAVEAREA:
		{
			IEntity * pEntity = gEnv->pEntitySystem->GetEntity(event.nParam[0]);
			if(pEntity)
			{
				IPhysicalEntity *pPhysics = pEntity->GetPhysics();
				if(pPhysics)
				{
					pe_status_dynamics physStatus;
					if(0 != pPhysics->GetStatus(&physStatus))
					{
						m_currentWeight -= physStatus.mass;

						if(m_currentWeight < 0)
							m_currentWeight = 0;
					}
				}
			}
			break;
		}

		default:
			break;
	}

	return CProjectile::ProcessEvent(event);
}

void CAVMine::SetParams(EntityId ownerId, EntityId hostId, EntityId weaponId, int fmId, int damage, int hitTypeId)
{
	// if this is a team game, record which team placed this mine...
	if(gEnv->bServer)
	{
		if(CGameRules* pGameRules = g_pGame->GetGameRules())
		{
			m_teamId = pGameRules->GetTeam(ownerId);
			pGameRules->SetTeam(m_teamId, GetEntityId());
		}
	}

	CProjectile::SetParams(ownerId, hostId, weaponId, fmId, damage, hitTypeId);
}