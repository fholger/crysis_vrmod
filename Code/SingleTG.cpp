/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 18:07:2007  12:15 : Created by Benito Gangoso Rodriguez

*************************************************************************/
#include "StdAfx.h"
#include "SingleTG.h"
#include <IGameTokens.h>
#include "Actor.h"
#include "Player.h"
#include "HUD/HUD.h"
#include "IUIDraw.h"

namespace
{
	const char* g_TokenTable[SINGLETG_MAX_TARGETS] = 
	{
		"weapon.tacgun.target0",
		"weapon.tacgun.target1",
		"weapon.tacgun.target2",
		"weapon.tacgun.target3"
	};
}

//------------------------------------------------------------------------
CSingleTG::CSingleTG()
{
	m_iSerializeIgnoreUpdate = 0;
	m_fSerializeProgress = 0.0f;
	m_idSerializeTarget = 0;

	// create the game tokens if not present
	IGameTokenSystem *pGameTokenSystem = gEnv->pGame->GetIGameFramework()->GetIGameTokenSystem();
	if(pGameTokenSystem)
	{
		for(int i=0; i<SINGLETG_MAX_TARGETS; i++)
		{
			EntityId entityId = 0;
			if (pGameTokenSystem->FindToken(g_TokenTable[i]) == 0)
				pGameTokenSystem->SetOrCreateToken(g_TokenTable[i], TFlowInputData(entityId));
		}
	}
}

//------------------------------------------------------------------------
CSingleTG::~CSingleTG()
{
}

//------------------------------------------------------------------------
bool CSingleTG::CanFire(bool considerAmmo) const
{
	return CSingle::CanFire(considerAmmo) && m_bLocked;
}

//----------------------------------------------------------------------
void CSingleTG::UpdateFPView(float frameTime)
{
	CSingle::UpdateFPView(frameTime);

	//Weapon can not be fired if there's not a target locked
	if(!m_bLocked)
	{
		m_pWeapon->LowerWeapon(true);

		if(!m_bLocking)
		{
			CActor * pPlayer = m_pWeapon->GetOwnerActor();
			SPlayerStats* stats = pPlayer?static_cast<SPlayerStats*>(pPlayer->GetActorStats()):NULL;

			if(stats && !gEnv->bMultiplayer)
				stats->bLookingAtFriendlyAI = true;

			SAFE_HUD_FUNC(ShowProgress((int)0, true, 400, 300, "@no_lock_tac",true, true));
		}
	}
}
//----------------------------------------------------
void CSingleTG::StartFire()
{
	if(!m_bLocked)
		m_pWeapon->PlayAction(m_actions.null_fire);
	else
		CSingle::StartFire();
}

//------------------------------------------------------
void CSingleTG::UpdateAutoAim(float frameTime)
{
	static IGameObjectSystem* pGameObjectSystem = gEnv->pGame->GetIGameFramework()->GetIGameObjectSystem();

	CActor *pOwner = m_pWeapon->GetOwnerActor();
	if (!pOwner || !pOwner->IsPlayer())
		return;

	// todo: use crosshair/aiming dir
	IMovementController *pMC = pOwner->GetMovementController();
	if (!pMC)
		return;

	// re-get targets from gametokens
	UpdateTargets();

	if(m_iSerializeIgnoreUpdate) 	//workaround serialization requested by design & RnD
	{
		m_iSerializeIgnoreUpdate--;
		SAFE_HUD_FUNC(ShowProgress((int)((m_fSerializeProgress / m_fireparams.autoaim_locktime)*100.0f), true, 400, 300, "@locking_tac",true, true));
		if(m_iSerializeIgnoreUpdate <= 0)
		{
			m_iSerializeIgnoreUpdate = 0;
			StartLocking(m_idSerializeTarget, 0);
			m_fStareTime = m_fSerializeProgress;
		}
		return;
	}

	SMovementState state;
	pMC->GetMovementState(state);

	Vec3 aimDir = state.eyeDirection;
	Vec3 aimPos = state.eyePosition;

	float maxDistance = m_fireparams.autoaim_distance;

	ray_hit ray;

	IPhysicalEntity* pSkipEnts[10];
	int nSkipEnts = GetSkipEntities(m_pWeapon, pSkipEnts, 10);

	const int objects = ent_all;
	const int flags = rwi_stop_at_pierceable|rwi_ignore_back_faces;

	int result = gEnv->pPhysicalWorld->RayWorldIntersection(aimPos, aimDir * 2.f * maxDistance, 
		objects, flags, &ray, 1, pSkipEnts, nSkipEnts);		

	bool hitValidTarget = false;
	IEntity* pEntity = 0;

	if (result && ray.pCollider)
	{	
		pEntity = (IEntity *)ray.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);    	        
		if (pEntity && IsValidAutoAimTarget(pEntity,ray.partid))
			hitValidTarget = true;
	}

	if(m_bLocked)
		m_autoaimTimeOut -= frameTime;

	if(hitValidTarget && (m_bLocked || m_bLocking))
	{
		int value = (int)((m_fStareTime / m_fireparams.autoaim_locktime)*100.0f);
		SAFE_HUD_FUNC(ShowProgress(value, true, 400, 300, (value<100)?"@locking_tac":"",true, true));
	}

	if (hitValidTarget && ray.dist <= maxDistance)
	{	
		if (m_bLocked)
		{
			if ((m_lockedTarget != pEntity->GetId()) && 
				(m_lockedTarget != pEntity->UnmapAttachedChild(ray.partid)->GetId()) && m_autoaimTimeOut<=0.0f)
				StartLocking(pEntity->GetId(),ray.partid);
		}
		else
		{	
			if (!m_bLocking || (m_lockedTarget!=pEntity->GetId()) && (m_lockedTarget!=pEntity->UnmapAttachedChild(ray.partid)->GetId()))
				StartLocking(pEntity->GetId(),ray.partid);
			else
				m_fStareTime += frameTime;
		}
	}
	else if(!hitValidTarget && m_bLocking)
	{
		m_pWeapon->RequestUnlock();
		Unlock();
	}
	else
	{
		// check if we're looking far away from our locked target
		if ((m_bLocked && !(ray.dist<=maxDistance && CheckAutoAimTolerance(aimPos, aimDir))) || (!m_bLocked && m_lockedTarget && m_fStareTime != 0.0f))
		{ 
			if(!m_fireparams.autoaim_timeout)
			{
				m_pWeapon->RequestUnlock();
				Unlock();
			}
		}
	}

	if (m_bLocking && !m_bLocked && m_fStareTime >= m_fireparams.autoaim_locktime && m_lockedTarget)
		m_pWeapon->RequestLock(m_lockedTarget);
	else if(m_bLocked && hitValidTarget && m_lockedTarget!=pEntity->GetId() &&
		(m_lockedTarget != pEntity->UnmapAttachedChild(ray.partid)->GetId()))
		m_pWeapon->RequestLock(pEntity->GetId(),ray.partid);
	else if (m_bLocked)
	{
		// check if target still valid (can e.g. be killed)
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_lockedTarget);	
		if ((pEntity && !IsValidAutoAimTarget(pEntity)) || (m_fireparams.autoaim_timeout && m_autoaimTimeOut<=0.0f))
		{
			m_pWeapon->RequestUnlock();
			Unlock();
		}
	}
}

//----------------------------------------------------
bool CSingleTG::IsValidAutoAimTarget(IEntity* pEntity, int partId)
{
	for(int i=0; i<SINGLETG_MAX_TARGETS;i++)
	{
		if(pEntity->UnmapAttachedChild(partId)->GetId()==m_targetIds[i])
			return true;
		
	}

	return false;
}

//--------------------------------------------------
void CSingleTG::StartLocking(EntityId targetId, int partId /*= 0*/)
{
	// start locking
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(targetId);
	if(!pEntity)
		return;

	m_lockedTarget = pEntity->UnmapAttachedChild(partId)->GetId();
	m_bLocking = true;
	m_bLocked = false;
	m_fStareTime = 0.0f;

	if (CActor *pActor=m_pWeapon->GetOwnerActor())
	{
		if (pActor->IsClient())
		{
			SAFE_HUD_FUNC(AutoAimLocking(m_lockedTarget));
		}
	}
}

//--------------------------------------------------
void CSingleTG::Unlock()
{
	CSingle::Unlock();

	SAFE_HUD_FUNC(ShowProgress((int)0, true, 400, 300, "@no_lock_tac",true, true));
}

//--------------------------------------------------
void CSingleTG::Lock(EntityId targetId, int partId /*= 0*/)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(targetId);
	if(!pEntity)
		return;

	m_lockedTarget = pEntity->UnmapAttachedChild(partId)->GetId();
	m_bLocking = false;
	m_bLocked = true;
	m_autoaimTimeOut = AUTOAIM_TIME_OUT;

	if (CActor *pActor=m_pWeapon->GetOwnerActor())
	{
		if (pActor->IsClient())
		{
			SAFE_HUD_FUNC(AutoAimLocked(m_lockedTarget));

			_smart_ptr< ISound > pBeep = gEnv->pSoundSystem->CreateSound("Sounds/interface:hud:target_lock", 0);
			if (pBeep)
			{
				pBeep->SetSemantic(eSoundSemantic_HUD);
				pBeep->Play();
			}
		}
	}
}

//---------------------------------------------------
void CSingleTG::UpdateTargets()
{
	IGameTokenSystem *pGameTokenSystem = gEnv->pGame->GetIGameFramework()->GetIGameTokenSystem();

	if(pGameTokenSystem)
	{
		for(int i=0; i<SINGLETG_MAX_TARGETS; i++)
		{
			EntityId entityId = 0;
		  // next call will leave entityId unchanged if not found, but it's 0 by default
			pGameTokenSystem->GetTokenValueAs(g_TokenTable[i], entityId);
			m_targetIds[i] = entityId;
		}
	}
	else
	{
		for(int i=0; i<SINGLETG_MAX_TARGETS; i++)
			m_targetIds[i] = 0;
	}
}

//-----------------------------------------------
void CSingleTG::Serialize(TSerialize ser)
{
	//workaround serialization requested by design & RnD
	m_idSerializeTarget = m_lockedTarget;
	m_fSerializeProgress = m_fStareTime;
	ser.Value("m_idSerializeTarget", m_idSerializeTarget);
	ser.Value("m_fSerializeProgress", m_fSerializeProgress);

	if(ser.IsReading())
	{
		m_iSerializeIgnoreUpdate = 3;
		UpdateTargets();
	}
}