/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 18:12:2005   14:01 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Binocular.h"
#include "GameActions.h"

#include <IActorSystem.h>
#include <IMovementController.h>

#include "Game.h"
#include "HUD/HUD.h"
#include "HUD/HUDScopes.h"
#include "SoundMoods.h"

TActionHandler<CBinocular> CBinocular::s_actionHandler;

CBinocular::CBinocular()
{
	if(s_actionHandler.GetNumHandlers() == 0)
	{
#define ADD_HANDLER(action, func) s_actionHandler.AddHandler(actions.action, &CBinocular::func)
		const CGameActions& actions = g_pGame->Actions();

		ADD_HANDLER(zoom,OnActionZoom);
		ADD_HANDLER(attack1,OnActionAttack);
		ADD_HANDLER(zoom_in,OnActionZoomIn);
		ADD_HANDLER(v_zoom_in,OnActionZoomIn);
		ADD_HANDLER(zoom_out,OnActionZoomOut);
		ADD_HANDLER(v_zoom_out,OnActionZoomOut);
#undef ADD_HANDLER
	}
}
//------------------------------------------------------------------------
CBinocular::~CBinocular()
{
	SAFE_HUD_FUNC(GetScopes()->ShowBinoculars(0));
}

//------------------------------------------------------------------------
void CBinocular::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(!s_actionHandler.Dispatch(this,actorId,actionId,activationMode,value))
		CWeapon::OnAction(actorId, actionId, activationMode, value);
}

//------------------------------------------------------------------------
void CBinocular::Select(bool select)
{
	CWeapon::Select(select);

	if (!GetOwnerActor() || !GetOwnerActor()->IsClient())
		return;

	if(gEnv->pSoundSystem)		//turn sound-zooming on / off
		gEnv->pSoundSystem->CalcDirectionalAttenuation(GetOwnerActor()->GetEntity()->GetWorldPos(), GetOwnerActor()->GetViewRotation().GetColumn1(), select?0.15f:0.0f);

	SAFE_HUD_FUNC(GetScopes()->ShowBinoculars(select));

	if(select)
		SAFE_SOUNDMOODS_FUNC(AddSoundMood(SOUNDMOOD_ENTER_BINOCULARS))
	else
		SAFE_SOUNDMOODS_FUNC(AddSoundMood(SOUNDMOOD_LEAVE_BINOCULARS))

	if (select && m_zm)
	{
		SetBusy(false);

		m_zm->StartZoom();
	}
}

//---------------------------------------------------------------------
void CBinocular::UpdateFPView(float frameTime)
{
	CWeapon::UpdateFPView(frameTime);

	CActor *pOwner = GetOwnerActor();
	if(pOwner && pOwner->IsClient())
	{
		if(m_zm && IsZoomed() && gEnv->pSoundSystem)
			gEnv->pSoundSystem->CalcDirectionalAttenuation(pOwner->GetEntity()->GetWorldPos(), pOwner->GetViewRotation().GetColumn1(), 0.35f - m_zm->GetCurrentStep() * 0.05f);
	}
}

//-----------------------------------------------------------------------
bool CBinocular::OnActionZoom(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	CActor *pOwner = GetOwnerActor();
	if (pOwner && (pOwner->GetActorClass() == CPlayer::GetActorClassType()))
	{
		CPlayer *pPlayer = (CPlayer *)pOwner;
		pPlayer->SelectLastItem(false,true);
	}

	return true;
}

//-------------------------------------------------------------------------
bool CBinocular::OnActionZoomIn(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	bool ok=!SAFE_HUD_FUNC_RET(IsPDAActive());
	if (ok && m_zm && (m_zm->GetCurrentStep()<m_zm->GetMaxZoomSteps()) && m_zm->StartZoom(false, false))
	{
		CActor *pOwner = GetOwnerActor();
		if(pOwner == g_pGame->GetIGameFramework()->GetClientActor())
			SAFE_HUD_FUNC(PlaySound(ESound_BinocularsZoomIn));
	}

	return true;
}

//--------------------------------------------------------------------------
bool CBinocular::OnActionZoomOut(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	bool ok=!SAFE_HUD_FUNC_RET(IsPDAActive());
	if (m_zm && m_zm->ZoomOut())
	{
		CActor *pOwner = GetOwnerActor();
		if(pOwner == g_pGame->GetIGameFramework()->GetClientActor())
			SAFE_HUD_FUNC(PlaySound(ESound_BinocularsZoomOut));
	}

	return true;
}

//----------------------------------------------------------------------------
bool CBinocular::OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
	{      
		// trigger OnShoot in here.. Binocs don't have any firemode
		Vec3 pos(ZERO);
		Vec3 dir(FORWARD_DIRECTION);

		IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(actorId);
		if (pActor)
		{
			IMovementController* pMC = pActor->GetMovementController();
			if (pMC)
			{
				SMovementState state;
				pMC->GetMovementState(state);          
				pos = state.pos;
				dir = state.eyeDirection;
			}
		}
		OnShoot(actorId, 0, 0, pos, dir, Vec3(ZERO));
	}

	return true;
}
