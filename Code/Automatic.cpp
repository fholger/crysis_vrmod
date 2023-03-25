/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 11:9:2005   15:00 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Automatic.h"
#include "Actor.h"


//------------------------------------------------------------------------
CAutomatic::CAutomatic()
{
}

//------------------------------------------------------------------------
CAutomatic::~CAutomatic()
{
}

//--------------------------------------------------
void CAutomatic::StartFire()
{
	CSingle::StartFire();

	if(m_soundId==INVALID_SOUNDID && !m_automaticactions.automatic_fire.empty())
		m_soundId = m_pWeapon->PlayAction(m_automaticactions.automatic_fire);
}
//------------------------------------------------------------------------
void CAutomatic::Update(float frameTime, uint frameId)
{
	CSingle::Update(frameTime, frameId);

	if (m_firing && CanFire(false))
		m_firing = Shoot(true);
}

//------------------------------------------------------------------------
void CAutomatic::StopFire()
{
	if (m_zoomtimeout > 0.0f)
	{
		CActor *pActor = m_pWeapon->GetOwnerActor();
		if (pActor && pActor->IsClient() && pActor->GetScreenEffects() != 0)
		{
			pActor->GetScreenEffects()->ClearBlendGroup(pActor->m_autoZoomInID);

			// this is so we will zoom out always at the right speed
			//float speed = (1.0f/.1f) * (1.0f - pActor->GetScreenEffects()->GetCurrentFOV())/(1.0f - .75f);
			//speed = fabs(speed);
			float speed = 1.0f/.1f;
			//if (pActor->GetScreenEffects()->HasJobs(pActor->m_autoZoomOutID))
			//	speed = pActor->GetScreenEffects()->GetAdjustedSpeed(pActor->m_autoZoomOutID);

			pActor->GetScreenEffects()->ClearBlendGroup(pActor->m_autoZoomOutID);
			CFOVEffect *fov = new CFOVEffect(pActor->GetEntityId(), 1.0f);
			CLinearBlend *blend = new CLinearBlend(1);
			pActor->GetScreenEffects()->StartBlend(fov, blend, speed, pActor->m_autoZoomOutID);
		}
		m_zoomtimeout = 0.0f;
	}

	if(m_firing)
		SmokeEffect();

	m_firing = false;

	if(m_soundId)
	{
		m_pWeapon->StopSound(m_soundId);
		m_soundId = INVALID_SOUNDID;
	}
}

//------------------------------------------------------------------------
const char *CAutomatic::GetType() const
{
	return "Automatic";
}

//---------------------------------------------------
void CAutomatic::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	CSingle::GetMemoryStatistics(s);
	m_automaticactions.GetMemoryStatistics(s);
}

//------------------------------------------------------------------------
void CAutomatic::ResetParams(const struct IItemParamsNode *params)
{
	CSingle::ResetParams(params);

	const IItemParamsNode *actions = params?params->GetChild("actions"):0;

	m_automaticactions.Reset(actions);
}

//------------------------------------------------------------------------
void CAutomatic::PatchParams(const struct IItemParamsNode *patch)
{
	CSingle::PatchParams(patch);

	const IItemParamsNode *actions = patch->GetChild("actions");

	m_automaticactions.Reset(actions, false);
}