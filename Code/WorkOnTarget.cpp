/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 05:8:2007:		Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "WorkOnTarget.h"
#include "Game.h"
#include "Item.h"
#include "Weapon.h"
#include "Player.h"
#include "GameCVars.h"
#include "GameRules.h"
#include <IEntitySystem.h>


//------------------------------------------------------------------------
CWorkOnTarget::CWorkOnTarget() :
	m_working(false),
	m_firing(false)
{
}

//------------------------------------------------------------------------
CWorkOnTarget::~CWorkOnTarget()
{
}

//------------------------------------------------------------------------
void CWorkOnTarget::Init(IWeapon *pWeapon, const struct IItemParamsNode *params)
{
	m_pWeapon = static_cast<CWeapon *>(pWeapon);

	if (params)
		ResetParams(params);

	m_working = false;
	m_firing = false;
	m_lastTargetId = 0;
	m_soundId = INVALID_SOUNDID;
	m_effectId = 0;
}

//------------------------------------------------------------------------
void CWorkOnTarget::Update(float frameTime, uint frameId)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );

	if (!m_firing)
		return;

	bool requireUpdate=false;

	CActor *pActor=m_pWeapon->GetOwnerActor();

	if (m_delayTimer>0.0f)
	{
		m_working = false;
		m_lastTargetId = 0;
		m_delayTimer -= frameTime;

		if (m_delayTimer<=0.0f)
		{
			m_delayTimer=0.0f;

			if (m_pWeapon->IsClient())
			{
				m_pWeapon->PlayAction(m_workactions.prefire.c_str());

				if (m_soundId!=INVALID_SOUNDID)
				{
					if (ISound *pSound=m_pWeapon->GetISound(m_soundId))
					{
						pSound->SetLoopMode(true);
						pSound->SetPaused(false);
					}
				}
			}
		}
	}

	if (!m_effectId && m_delayTimer<=0.0f && m_pWeapon->IsClient())
	{
		int slot = m_pWeapon->GetStats().fp ? CItem::eIGS_FirstPerson : CItem::eIGS_ThirdPerson;
		int id = m_pWeapon->GetStats().fp ? 0 : 1;

		CSingle::SEffectParams &effect=m_workparams.working_effect;
		if (!effect.effect[id].empty())
			m_effectId = m_pWeapon->AttachEffect(slot, 0, true, effect.effect[id].c_str(), effect.helper[id].c_str());
	}

	if (m_delayTimer<=0.0f && m_pWeapon->IsServer())
	{
		bool keepWorking=false;

		if (IEntity	*pEntity=CanWork())
		{
			if (pEntity->GetId() == m_lastTargetId)
				keepWorking=WorkOnTarget(pEntity, frameTime);
			else
			{
				if (IEntity *pLast=gEnv->pEntitySystem->GetEntity(m_lastTargetId))
					StopWork();

				StartWork(pEntity);
				keepWorking=true;
			}
		}

		if (!keepWorking)
		{
			StopWork();

			if (m_pWeapon->IsServer())
				m_pWeapon->GetGameObject()->InvokeRMI(CWeapon::ClStopFire(), CWeapon::EmptyParams(), eRMI_ToRemoteClients);
		}
	}

	if (requireUpdate)
		m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//------------------------------------------------------------------------
void CWorkOnTarget::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CWorkOnTarget::ResetParams(const struct IItemParamsNode *params)
{
	const IItemParamsNode *work = params?params->GetChild("work"):0;
	const IItemParamsNode *actions = params?params->GetChild("actions"):0;

	m_workparams.Reset(work);
	m_workactions.Reset(actions);
}

//------------------------------------------------------------------------
void CWorkOnTarget::PatchParams(const struct IItemParamsNode *patch)
{
	const IItemParamsNode *work = patch->GetChild("work");
	const IItemParamsNode *actions = patch->GetChild("actions");

	m_workparams.Reset(work, false);
	m_workactions.Reset(actions, false);
}

//------------------------------------------------------------------------
void CWorkOnTarget::Activate(bool activate)
{
	m_working = false;
	m_delayTimer = 0.0f;
	m_lastTargetId = 0;

	if (m_soundId!=INVALID_SOUNDID)
		m_pWeapon->StopSound(m_soundId);

	m_soundId=INVALID_SOUNDID;
}

//------------------------------------------------------------------------
bool CWorkOnTarget::CanFire(bool considerAmmo) const
{
	return !m_working && m_delayTimer<=0.0f;
}

//------------------------------------------------------------------------
void CWorkOnTarget::StartFire()
{
	if (m_pWeapon->IsBusy() || !CanFire(false) || !CanWork())
		return;

	m_firing=true;
	m_pWeapon->SetBusy(true);

	m_soundId=m_pWeapon->PlayAction(m_workactions.work.c_str(), 0, true, CItem::eIPAF_Default|CItem::eIPAF_CleanBlending|CItem::eIPAF_SoundStartPaused);
	m_pWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, m_workactions.work.c_str());

	m_pWeapon->EnableUpdate(true, eIUS_FireMode);	
	m_delayTimer=m_workparams.delay;

	if (!m_pWeapon->IsServer())
		m_pWeapon->RequestStartFire();
}

//------------------------------------------------------------------------
void CWorkOnTarget::StopFire()
{
	if (m_firing)
	{
		m_pWeapon->EnableUpdate(false, eIUS_FireMode);
		m_firing=false;
		m_delayTimer=0.0f;

		StopWork();

		if (!m_pWeapon->IsServer())
			m_pWeapon->RequestStopFire();
	}
}

//------------------------------------------------------------------------
void CWorkOnTarget::NetShoot(const Vec3 &hit, int ph)
{
}

//------------------------------------------------------------------------
void CWorkOnTarget::NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int ph)
{
}

//------------------------------------------------------------------------
void CWorkOnTarget::Shoot(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel)
{
}

//------------------------------------------------------------------------
void CWorkOnTarget::NetStartFire()
{
	m_pWeapon->EnableUpdate(true, eIUS_FireMode);

	m_soundId=m_pWeapon->PlayAction(m_workactions.work.c_str(), 0, true, CItem::eIPAF_Default|CItem::eIPAF_CleanBlending|CItem::eIPAF_SoundStartPaused);
	m_pWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, m_workactions.work.c_str());

	m_delayTimer=m_workparams.delay;
	m_firing=true;
}

//------------------------------------------------------------------------
void CWorkOnTarget::NetStopFire()
{
	m_pWeapon->EnableUpdate(false, eIUS_FireMode);
	m_firing=false;
	m_delayTimer=0.0f;

	StopWork();
}

//------------------------------------------------------------------------
const char *CWorkOnTarget::GetType() const
{
	return "WorkOnTarget";
}

//------------------------------------------------------------------------
void CWorkOnTarget::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->Add(m_name);
	m_workparams.GetMemoryStatistics(s);
	m_workactions.GetMemoryStatistics(s);
}

//------------------------------------------------------------------------
IEntity *CWorkOnTarget::CanWork()
{
	static Vec3 pos,dir; 
	static ICVar* pAimDebug = gEnv->pConsole->GetCVar("g_aimdebug");
	
	CActor *pActor=m_pWeapon->GetOwnerActor();
	
	static IPhysicalEntity* pSkipEntities[10];
	int nSkip = CSingle::GetSkipEntities(m_pWeapon, pSkipEntities, 10);

	IEntity *pEntity=0;
	float range=m_workparams.range;
	
	IMovementController * pMC = pActor ? pActor->GetMovementController() : 0;
	if (pMC)
	{ 
		SMovementState info;
		pMC->GetMovementState(info);

		pos = info.weaponPosition;

		if (!pActor->IsPlayer())
		{
			dir = range * (info.fireTarget-pos).normalized();
		}
		else
		{
			dir = range * info.fireDirection;    

			// marcok: leave this alone
			if (g_pGameCVars->goc_enable && pActor->IsClient())
			{
				CPlayer *pPlayer = (CPlayer*)pActor;
				pos = pPlayer->GetViewMatrix().GetTranslation();
			}
		}
	}
	else
	{ 
		assert(0);
	}

	primitives::sphere sphere;
	sphere.center = pos;
	sphere.r = m_workparams.radius;
	Vec3 end = pos+dir;

	geom_contact *pContact=0;
	float dst=gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, end-sphere.center, ent_all,
		&pContact, 0, (geom_colltype_player<<rwi_colltype_bit)|rwi_stop_at_pierceable, 0, 0, 0, pSkipEntities, nSkip);

	if (pContact && dst>=0.0f)
	{
		IPhysicalEntity *pCollider = gEnv->pPhysicalWorld->GetPhysicalEntityById(pContact->iPrim[0]);

		if(pCollider && pCollider->GetiForeignData() == PHYS_FOREIGN_ID_ENTITY)
		{
			if (pEntity = static_cast<IEntity *>(pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY)))
			{
				if (CGameRules *pGameRules=g_pGame->GetGameRules())
				{
					if (IScriptTable *pScriptTable=pGameRules->GetEntity()->GetScriptTable())
					{
						HSCRIPTFUNCTION pfnCanWork=0;
						if (pScriptTable->GetValueType("CanWork")==svtFunction && pScriptTable->GetValue("CanWork", pfnCanWork))
						{
							bool result=false;
							Script::CallReturn(gEnv->pScriptSystem, pfnCanWork, pScriptTable, ScriptHandle(pEntity->GetId()), ScriptHandle(m_pWeapon->GetOwnerId()), m_workparams.work_type.c_str(), result);
							gEnv->pScriptSystem->ReleaseFunc(pfnCanWork);

							if (result)
								return pEntity;
						}
					}
				}
			}
		}
	}

	return 0;
}

//------------------------------------------------------------------------
void CWorkOnTarget::StartWork(IEntity *pEntity)
{
	m_pWeapon->SetBusy(true);
	m_working=true;
	m_lastTargetId=pEntity->GetId();

	if (CGameRules *pGameRules=g_pGame->GetGameRules())
	{
		if (m_pWeapon->IsServer())
		{
			if (IScriptTable *pScriptTable=pGameRules->GetEntity()->GetScriptTable())
				Script::CallMethod(pScriptTable, "StartWork", ScriptHandle(m_lastTargetId), ScriptHandle(m_pWeapon->GetOwnerId()), m_workparams.work_type.c_str());
		}
	}
}

//------------------------------------------------------------------------
void CWorkOnTarget::StopWork()
{
	if (m_working)
		m_pWeapon->PlayAction(m_workactions.postfire.c_str());
	m_pWeapon->PlayAction(m_workactions.idle.c_str(), 0, true, CItem::eIPAF_Default|CItem::eIPAF_CleanBlending);
	m_pWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, m_workactions.idle.c_str());

	if (m_effectId)
	{
		int slot = m_pWeapon->GetStats().fp?CItem::eIGS_FirstPerson:CItem::eIGS_ThirdPerson;
		m_pWeapon->AttachEffect(slot, m_effectId, false);
		m_effectId=0;
	}

	if (m_soundId != INVALID_SOUNDID)
	{
		m_pWeapon->StopSound(m_soundId);
		m_soundId=INVALID_SOUNDID;
	}

	if (CGameRules *pGameRules=g_pGame->GetGameRules())
	{
		if (m_pWeapon->IsServer())
		{
			if (IScriptTable *pScriptTable=pGameRules->GetEntity()->GetScriptTable())
				Script::CallMethod(pScriptTable, "StopWork", ScriptHandle(m_pWeapon->GetOwnerId()));
		}
	}

	m_pWeapon->SetBusy(false);
	m_working=false;
	m_firing=false;
	m_lastTargetId=0;
}

//------------------------------------------------------------------------
bool CWorkOnTarget::WorkOnTarget(IEntity *pEntity, float frameTime)
{
	if (!m_pWeapon->IsServer())
		return false;

	if (CGameRules *pGameRules=g_pGame->GetGameRules())
	{
		if (IScriptTable *pScriptTable=pGameRules->GetEntity()->GetScriptTable())
		{
			HSCRIPTFUNCTION pfnWork=0;
			if (pScriptTable->GetValueType("Work")==svtFunction && pScriptTable->GetValue("Work", pfnWork))
			{
				bool result=false;
				Script::CallReturn(gEnv->pScriptSystem, pfnWork, pScriptTable, ScriptHandle(m_pWeapon->GetOwnerId()), m_workparams.amount, frameTime, result);
				gEnv->pScriptSystem->ReleaseFunc(pfnWork);

				return result;
			}
		}
	}

	return false;
}