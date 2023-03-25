// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "TacBullet.h"
#include "Game.h"
#include <IEntitySystem.h>
#include <IGameRulesSystem.h>
#include <IAnimationGraph.h>
#include "GameRules.h"

CTacBullet::CTacBullet()
{
}

CTacBullet::~CTacBullet()
{
}

void CTacBullet::HandleEvent(const SGameObjectEvent &event)
{
	if (m_destroying)
		return;

	CProjectile::HandleEvent(event);

	if (event.event == eGFE_OnCollision)
	{
		EventPhysCollision *pCollision = reinterpret_cast<EventPhysCollision *>(event.ptr);
		if (!pCollision)
			return;

		IEntity *pTarget = pCollision->iForeignData[1]==PHYS_FOREIGN_ID_ENTITY ? (IEntity*)pCollision->pForeignData[1]:0;
		if (pTarget)
		{
			CGameRules *pGameRules = g_pGame->GetGameRules();

			EntityId targetId = pTarget->GetId();
			CActor *pActor = (CActor *)gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(targetId);
			
			if(pActor && pActor->CanSleep())
			{
				if(pActor->GetLinkedVehicle() && !gEnv->bMultiplayer)
				{
					SleepTargetInVehicle(m_ownerId,pActor);
				}
				else if(IEntity* pOwnerEntity = gEnv->pEntitySystem->GetEntity(m_ownerId))
				{
					if(pTarget->GetAI() && pTarget->GetAI()->IsHostile(pOwnerEntity->GetAI(),false))
					{
						float sleepTime = 30.0f;
						if(m_pAmmoParams)
							sleepTime = m_pAmmoParams->sleepTime;
						SimpleHitInfo info(m_ownerId, targetId, m_weaponId, 1, sleepTime); // 0=tag,1=tac
						info.remote=IsRemote();

						pGameRules->ClientSimpleHit(info);
					}
				}
			}
			else
			{
				Vec3 dir(0, 0, 0);
				if (pCollision->vloc[0].GetLengthSquared() > 1e-6f)
					dir = pCollision->vloc[0].GetNormalized();

				HitInfo hitInfo(m_ownerId, pTarget->GetId(), m_weaponId,
					m_fmId, 0.0f, pGameRules->GetHitMaterialIdFromSurfaceId(pCollision->idmat[1]), pCollision->partid[1],
					m_hitTypeId, pCollision->pt, dir, pCollision->n);

				hitInfo.remote = IsRemote();
				hitInfo.projectileId = GetEntityId();
				hitInfo.bulletType = m_pAmmoParams->bulletType;
				hitInfo.damage = m_damage;
				
				if (!hitInfo.remote)
					hitInfo.seq=m_seq;

				pGameRules->ClientHit(hitInfo);
			}
		}
		else if (pCollision->pEntity[0]->GetType() == PE_PARTICLE)
		{
			float bouncy, friction;
			uint	pierceabilityMat;
			gEnv->pPhysicalWorld->GetSurfaceParameters(pCollision->idmat[1], bouncy, friction, pierceabilityMat);
			pierceabilityMat&=sf_pierceable_mask;

			pe_params_particle params;
			if(pCollision->pEntity[0]->GetParams(&params)==0)
				SetDefaultParticleParams(&params);

			if (pierceabilityMat>params.iPierceability && pCollision->idCollider!=-1) 
				return;
		}

		Destroy();
	}
}

//---------------------------------------------------
void CTacBullet::SleepTargetInVehicle(EntityId shooterId, CActor* pActor)
{
	IAISystem *pAISystem=gEnv->pAISystem;
	if (pAISystem)
	{
		if(IEntity* pEntity=pActor->GetEntity())
		{
			if(IAIObject* pAIObj=pEntity->GetAI())
			{
				IAIActor* pAIActor = pAIObj->CastToIAIActor();
				if(pAIActor)
				{
					IAISignalExtraData *pEData = pAISystem->CreateSignalExtraData();	
					// try to retrieve the shooter position
					if (IEntity* pOwnerEntity = gEnv->pEntitySystem->GetEntity(shooterId))
						pEData->point = pOwnerEntity->GetWorldPos();
					else
						pEData->point = pEntity->GetWorldPos();
					pAIActor->SetSignal(1,"OnFallAndPlay",0,pEData);
				}
			}
		}
	}

	pActor->CreateScriptEvent("sleep", 0);
	if (IAnimationGraphState* animGraph = pActor->GetAnimationGraphState())
		animGraph->SetInput( "Signal", "fall" );

	float sleepTime = 30.0f;
	if(m_pAmmoParams)
		sleepTime = m_pAmmoParams->sleepTime;
	pActor->SetSleepTimer(sleepTime);
}
