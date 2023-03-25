/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 19:12:2005   12:10 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "FreezingBeam.h"
#include "Game.h"
#include "GameRules.h"
#include "WeaponSystem.h"



//------------------------------------------------------------------------
CFreezingBeam::CFreezingBeam()
{
}

//------------------------------------------------------------------------
CFreezingBeam::~CFreezingBeam()
{
}

//------------------------------------------------------------------------
void CFreezingBeam::ResetParams(const struct IItemParamsNode *params)
{
  CBeam::ResetParams(params);

  const IItemParamsNode *freeze = params?params->GetChild("freeze"):0;  
  m_freezeparams.Reset(freeze);  
}

//------------------------------------------------------------------------
void CFreezingBeam::PatchParams(const struct IItemParamsNode *patch)
{
  CBeam::PatchParams(patch);

  const IItemParamsNode *freeze = patch?patch->GetChild("freeze"):0;  
  m_freezeparams.Reset(freeze, false);
}


//------------------------------------------------------------------------
void CFreezingBeam::Hit(ray_hit &hit, const Vec3 &dir)
{
  if (gEnv->bMultiplayer)  
	  if (CActor *pActor=m_pWeapon->GetOwnerActor())
		  if (pActor && !pActor->IsClient())
			  return;
  
	IEntity *pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider);

	float frost = m_freezeparams.freeze_speed>0.0f?m_freezeparams.freeze_speed*m_beamparams.tick:1.0f;

	int type=hit.pCollider->GetType();
	if (pEntity && (type==PE_RIGID || type==PE_ARTICULATED || type==PE_LIVING || type==PE_WHEELEDVEHICLE || 
		(type==PE_STATIC && g_pGame->GetWeaponSystem()->GetProjectile(pEntity->GetId())))) // static projectiles are allowed to be frozen
	{
		SimpleHitInfo info(m_pWeapon->GetOwnerId(), pEntity->GetId(), m_pWeapon->GetEntityId(), 0xe);
		info.value=frost;

		g_pGame->GetGameRules()->ClientSimpleHit(info);
	}
}

//------------------------------------------------------------------------
void CFreezingBeam::Tick(ray_hit &hit, const Vec3 &dir)
{
}

//------------------------------------------------------------------------
void CFreezingBeam::GetMemoryStatistics(ICrySizer * s)
{
  s->Add(*this);
  CBeam::GetMemoryStatistics(s);
  m_freezeparams.GetMemoryStatistics(s);  
}
