/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$   All input weapon stuff here

-------------------------------------------------------------------------
History:
- 30.07.07   12:50 : Created by Benito G.R.

*************************************************************************/

#include "StdAfx.h"
#include "Weapon.h"
#include "GameActions.h"
#include "Game.h"
#include "GameCVars.h"
#include "HUD/HUD.h"

#include "OffHand.h"
#include "IPlayerInput.h"
 

//===========AUX FUNCTIONS====================
namespace
{
	void GetOffHandInfo(CWeapon* pThis, bool &offHandSelected, COffHand** pOffHand)
	{
		CActor *pOwnerActor=pThis->GetOwnerActor();
		if (pOwnerActor)
		{
			(*pOffHand) = static_cast<COffHand*>(pOwnerActor->GetWeaponByClass(CItem::sOffHandClass));
			if((*pOffHand) && (*pOffHand)->IsSelected())
				offHandSelected = true;
		}
	}

	void GetDualWieldInfo(CWeapon* pThis, bool &isDualWield, CWeapon** pSlave)
	{
		if (pThis->IsDualWieldMaster())
		{
			IItem *slave = pThis->GetDualWieldSlave();
			if (slave && slave->GetIWeapon())
			{
				(*pSlave) = static_cast<CWeapon *>(slave);
				isDualWield = true;
			}
		}
	}
}

//=================================================================
TActionHandler<CWeapon>	CWeapon::s_actionHandler;

void CWeapon::RegisterActions()
{
	if (s_actionHandler.GetNumHandlers() == 0)
	{
		#define ADD_HANDLER(action, func) s_actionHandler.AddHandler(actions.action, &CWeapon::func)
		const CGameActions& actions = g_pGame->Actions();

		ADD_HANDLER(attack1,OnActionAttack);
		ADD_HANDLER(reload,OnActionReload);
		ADD_HANDLER(special,OnActionSpecial);
		ADD_HANDLER(modify,OnActionModify);
		ADD_HANDLER(firemode,OnActionFiremode);
		ADD_HANDLER(zoom_in,OnActionZoomIn);
		ADD_HANDLER(zoom_out,OnActionZoomOut);
		ADD_HANDLER(zoom,OnActionZoom);
		ADD_HANDLER(xi_zoom,OnActionZoomXI);

		#undef ADD_HANDLER
	}
}
//-----------------------------------------------------
void CWeapon::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	CItem::OnAction(actorId, actionId, activationMode, value);

	s_actionHandler.Dispatch(this,actorId,actionId,activationMode,value);
}

//------------------------------------------------------
void CWeapon::ForcePendingActions()
{
	CItem::ForcePendingActions();

	CActor* pOwner = GetOwnerActor();
	if(!pOwner || !pOwner->IsClient())
		return;

	//Force start firing, if needed and possible
	if(m_requestedFire)
	{
		if(!IsDualWield() && !IsWeaponRaised())
		{
			m_requestedFire = false;
			if(IsTargetOn() || (m_fm && !m_fm->AllowZoom()))
				return;
			
			OnAction(GetOwnerId(),"attack1",eAAM_OnPress,0.0f);
		}
		else if(IsDualWield() && IsDualWieldMaster())
		{
			IItem *slave = GetDualWieldSlave();
			if(!IsWeaponRaised())
			{
				m_requestedFire = false;
				OnAction(GetOwnerId(),"attack1",eAAM_OnPress,0.0f);
			}
			else if(slave && slave->GetIWeapon())
			{
				CWeapon* dualwield = static_cast<CWeapon*>(slave);
				if(!dualwield->IsWeaponRaised())
				{
					m_requestedFire = false;
					OnAction(GetOwnerId(),"attack1",eAAM_OnPress,0.0f);
				}
			}
		}
	}
}

//--------------------------------------------------------------------
bool CWeapon::PreActionAttack(bool startFire)
{
	// Melee while pressing SHIFT for SP
	CPlayer *pPlayer = static_cast<CPlayer*>(GetOwnerActor());
	if(!pPlayer)
		return false;

	//if(gEnv->bMultiplayer)
	{
		if(startFire && pPlayer->IsSprinting())
		{
			//Stop sprinting, start firing
			SPlayerStats *pStats = static_cast<SPlayerStats*>(pPlayer->GetActorStats());
			if(pStats)
			{
				pStats->bSprinting = false;
				pStats->bIgnoreSprinting = true;
			}
		}
		else if(!startFire)
		{
			//Stop firing, continue sprinting
			SPlayerStats *pStats = static_cast<SPlayerStats*>(pPlayer->GetActorStats());
			if(pStats)
				pStats->bIgnoreSprinting = false;

		}
	}

	return false;
}

//--------------------------------------------------------------------
bool CWeapon::OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(!m_modifying)
	{
		if(IsTwoHand())
		{
			COffHand * offHandWeapon = NULL;
			bool isOffHandSelected = false;
			GetOffHandInfo(this,isOffHandSelected,&offHandWeapon);

			if(offHandWeapon && 
				(offHandWeapon->GetOffHandState()&(eOHS_HOLDING_GRENADE|eOHS_SWITCHING_GRENADE|eOHS_PICKING_ITEM)))
				return false;
		}

		if (activationMode == eAAM_OnPress)
		{

			if(PreActionAttack(true))
				return true;

			bool isDualWield = false;
			CWeapon *dualWield = NULL;
			GetDualWieldInfo(this,isDualWield,&dualWield);

			if (isDualWield)
			{
				m_fire_alternation = !m_fire_alternation;
				m_requestedFire = true;

				if (!m_fire_alternation && dualWield->OutOfAmmo(false) && dualWield->CanReload())
				{
					dualWield->Reload();
					return true;
				}
				else if(m_fire_alternation && OutOfAmmo(false) && CanReload())
				{
					Reload();
					return true;
				}

				if (m_fire_alternation || (!dualWield->CanFire() || !dualWield->IsSelected()))
				{
					if(!IsWeaponRaised() && CanFire())
						StartFire();
					else if(!dualWield->IsWeaponRaised() && dualWield->IsSelected())
						dualWield->StartFire();
				}
				else if (dualWield->CanFire())
				{
					if(!dualWield->IsWeaponRaised() && dualWield->CanFire())
						dualWield->StartFire();
					else if(!IsWeaponRaised())
						StartFire();
				}

			}
			else
			{
				if(!m_weaponRaised)
					StartFire();

				m_requestedFire = true;
			}
		}
		else if (activationMode == eAAM_OnRelease)
		{
			PreActionAttack(false);

			StopFire();
			m_requestedFire = false;
		}
	}

	return true;
}

//---------------------------------------------------------
bool CWeapon::OnActionReload(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(activationMode=eAAM_OnPress)
	{
		COffHand * offHandWeapon = NULL;
		bool isOffHandSelected = false;
		GetOffHandInfo(this,isOffHandSelected,&offHandWeapon);

		if (!IsBusy() && !m_modifying && !isOffHandSelected)
		{
			bool isDualWield = false;
			CWeapon *dualWield = NULL;
			GetDualWieldInfo(this,isDualWield,&dualWield);

			if(IsWeaponRaised() && m_fm && m_fm->CanReload())
				RaiseWeapon(false);

			Reload();

			if (isDualWield)
			{
				if(dualWield->IsWeaponRaised() && dualWield->CanReload())
					dualWield->RaiseWeapon(false);
				dualWield->Reload();
			}
		}
	}

	return true;
}

//---------------------------------------------------------------------------------
bool CWeapon::OnActionFiremode(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode==eAAM_OnPress)
	{
		bool isDualWield = false;
		CWeapon *dualWield = NULL;
		GetDualWieldInfo(this,isDualWield,&dualWield);

		if (isDualWield)
		{
			if(IsWeaponRaised())
				RaiseWeapon(false,true);

			if(dualWield->IsWeaponRaised())
				dualWield->RaiseWeapon(false,true);

			StartChangeFireMode();
		}
		else
		{
			if(m_weaponRaised)
				RaiseWeapon(false,true);

			StartChangeFireMode();
		}
	}

	return true;
}

//---------------------------------------------------------------------
bool CWeapon::OnActionSpecial(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (activationMode == eAAM_OnPress)
	{
		if(m_weaponRaised)
			RaiseWeapon(false,true);

		CActor* pOwnerActor = GetOwnerActor();
		if (pOwnerActor && g_pGameCVars->dt_enable)
		{
			CPlayer *pPlayer = static_cast<CPlayer*>(pOwnerActor);
			pPlayer->GetNanoSuit()->Tap(eNA_Melee);
		}

		COffHand * offHandWeapon = NULL;
		bool isOffHandSelected = false;
		GetOffHandInfo(this,isOffHandSelected,&offHandWeapon);

		if (CanMeleeAttack() && (!isOffHandSelected || (offHandWeapon->GetOffHandState()&(eOHS_HOLDING_NPC|eOHS_TRANSITIONING))))
			MeleeAttack();
	}

	return true;
}

//------------------------------------------------------------------------
class CWeapon::ScheduleLayer_Leave
{
public:
	ScheduleLayer_Leave(CWeapon *wep)
	{
		_pWeapon = wep;
	}
	void execute(CItem *item) {
		_pWeapon->m_transitioning = false;
		gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 0.0f);
		_pWeapon->m_dofSpeed=0.0f;
	}
private:
	CWeapon *_pWeapon;
};

class CWeapon::ScheduleLayer_Enter
{
public:
	ScheduleLayer_Enter(CWeapon *wep)
	{
		_pWeapon = wep;
	}
	void execute(CItem *item) {
		SAFE_HUD_FUNC(WeaponAccessoriesInterface(true));
		_pWeapon->PlayLayer(g_pItemStrings->modify_layer, eIPAF_Default|eIPAF_NoBlend, false);
		_pWeapon->m_transitioning = false;

		gEnv->p3DEngine->SetPostEffectParam("Dof_BlurAmount", 1.0f);
		_pWeapon->m_dofSpeed=0.0f;
	}
private:
	CWeapon *_pWeapon;
};

//-------------------------------------------------------------------------
bool CWeapon::OnActionModify(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	COffHand * offHandWeapon = NULL;
	bool isOffHandSelected = false;
	GetOffHandInfo(this,isOffHandSelected,&offHandWeapon);

	if (!IsBusy() && !isOffHandSelected)
	{
		if (m_fm)
			m_fm->StopFire();

		if(m_zm && m_zm->IsZoomed())
			m_zm->StopZoom();

		if(m_weaponRaised)
			RaiseWeapon(false,true);

		if (m_modifying && !m_transitioning)
		{
			StopLayer(g_pItemStrings->modify_layer, eIPAF_Default, false);
			PlayAction(g_pItemStrings->leave_modify, 0);
			m_dofSpeed = -1.0f/((float)GetCurrentAnimationTime(eIGS_FirstPerson)/1000.0f);
			m_dofValue = 1.0f;
			m_focusValue = -1.0f;

			GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson), CSchedulerAction<ScheduleLayer_Leave>::Create(this), false);
			m_transitioning = true;

			SAFE_HUD_FUNC(WeaponAccessoriesInterface(false));
			m_modifying = false;

			GetGameObject()->InvokeRMI(CItem::SvRequestLeaveModify(), CItem::EmptyParams(), eRMI_ToServer);
		}
		else if (!m_modifying && !m_transitioning)
		{
			gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 1.0f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusRange", -1.0f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMin", 0.0f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMax", 5.0f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusLimit", 20.0f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_UseMask", 0.0f);

			PlayAction(g_pItemStrings->enter_modify, 0, false, eIPAF_Default | eIPAF_RepeatLastFrame);
			m_dofSpeed = 1.0f/((float)GetCurrentAnimationTime(eIGS_FirstPerson)/1000.0f);
			m_dofValue = 0.0f;
			m_transitioning = true;

			GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson), CSchedulerAction<ScheduleLayer_Enter>::Create(this), false);
			m_modifying = true;

			GetGameObject()->InvokeRMI(CItem::SvRequestEnterModify(), CItem::EmptyParams(), eRMI_ToServer);
		}
	}

	return true;
}

//---------------------------------------------------------
bool CWeapon::OnActionZoomIn(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(m_zm && m_zm->IsZoomed())
	{
		int numSteps = m_zm->GetMaxZoomSteps();
		if((numSteps>1) && (m_zm->GetCurrentStep()<numSteps))
			StartZoom(actorId,1);	
	}

	return true;
}

//----------------------------------------------------------
bool CWeapon::OnActionZoomOut(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(m_zm && m_zm->IsZoomed())
	{
		int numSteps = m_zm->GetMaxZoomSteps();
		if((numSteps>1) && (m_zm->GetCurrentStep()>1))
			m_zm->ZoomOut();
	}

	return true;
}

//----------------------------------------------------------
bool CWeapon::OnActionZoom(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	COffHand * offHandWeapon = NULL;
	bool isOffHandSelected = false;
	GetOffHandInfo(this,isOffHandSelected,&offHandWeapon);

	if (!m_modifying && (!isOffHandSelected || (offHandWeapon->GetOffHandState()&eOHS_TRANSITIONING)))
	{
		if (activationMode == eAAM_OnPress && m_useViewMode)
		{
			IncrementViewmode();
		}
		else
		{
			bool isDualWield = false;
			CWeapon *dualWield = NULL;
			GetDualWieldInfo(this,isDualWield,&dualWield);

			if (!isDualWield)
			{
				if (m_fm && !m_fm->IsReloading())
				{
					if (activationMode == eAAM_OnPress)
					{
						if(!m_fm->AllowZoom())
						{
							if(!IsTargetOn()) //Allow zoom-in, when using aiming helper
								return false;
						}

						if(m_weaponRaised)
							RaiseWeapon(false,true);

						//Use mouse wheel for scopes with several steps/stages
						if (m_zm && m_zm->IsZoomed() && m_zm->GetMaxZoomSteps()>1)
							m_zm->StopZoom();
						else
							StartZoom(actorId,1);
					}
					else if (activationMode == eAAM_OnRelease)
					{
						if(m_zm && !m_zm->IsToggle())
							m_zm->StopZoom();
					}
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------------
bool CWeapon::OnActionZoomXI(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	COffHand * offHandWeapon = NULL;
	bool isOffHandSelected = false;
	GetOffHandInfo(this,isOffHandSelected,&offHandWeapon);

	if (!m_modifying && !isOffHandSelected && !IsWeaponRaised())
	{
		bool isDualWield = false;
		CWeapon *dualWield = NULL;
		GetDualWieldInfo(this,isDualWield,&dualWield);

		if (m_useViewMode)
		{
			if (activationMode == eAAM_OnPress)
				IncrementViewmode();
		}
		else if (g_pGameCVars->hud_ctrlZoomMode)
		{
			if (activationMode == eAAM_OnPress)
			{
				if (!isDualWield)
				{
					if(m_fm && !m_fm->IsReloading())
					{
						// The zoom code includes the aim assistance
						if (m_fm->AllowZoom())
							StartZoom(actorId,1);
						else
							m_fm->Cancel();
					}
				}
				else
				{
					// If the view does not zoom, we need to force aim assistance
					AssistAiming(1, true);
				}
			}
			else if (activationMode == eAAM_OnRelease)
			{
				if (!isDualWield)
				{
					if(m_fm && !m_fm->IsReloading())
						StopZoom(actorId);
				}
			}
		}
		else
		{
			if (activationMode == eAAM_OnPress && m_fm && !m_fm->IsReloading())
			{
				if (!isDualWield)
				{
					if (m_fm->AllowZoom())
						StartZoom(actorId,1);		
					else
						m_fm->Cancel();
				}
				else
				{
					// If the view does not zoom, we need to force aim assistance
					AssistAiming(1, true);
				}
			}
		}
	}

	return true;
}