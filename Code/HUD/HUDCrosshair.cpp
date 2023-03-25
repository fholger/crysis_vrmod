/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Crosshair HUD object (refactored from old HUD code)

-------------------------------------------------------------------------
History:
- 15:05:2007  11:00 : Created by Jan Müller

*************************************************************************/

#include "StdAfx.h"
#include "HUDCrosshair.h"
#include "IWorldQuery.h"
#include "GameCVars.h"
#include "GameRules.h"
#include "GameUtils.h"
#include "HUD.h"
#include "HUDVehicleInterface.h"
#include "Item.h"
#include "Weapon.h"
#include "OffHand.h"

//-----------------------------------------------------------------------------------------------------

CHUDCrosshair::CHUDCrosshair(CHUD* pHUD) : g_pHUD(pHUD), m_bUsable(false)
{
	m_animCrossHair.Load("Libs/UI/HUD_Crosshair.gfx", eFD_Center, eFAF_ManualRender);
	m_animFriendCross.Load("Libs/UI/HUD_FriendlyCross.gfx", eFD_Center, eFAF_ManualRender);
	m_animInterActiveIcons.Load("Libs/UI/HUD_InterActiveIcons.gfx", eFD_Center, eFAF_ManualRender);
	m_iFriendlyTarget = 0;
	m_iCrosshair = -1;
	m_bHideUseIconTemp = false;
	m_bBroken = 0;
	m_opacity = 1.0f;

	m_useIcons["@use"] = 1;
	m_useIcons["@use_vehicle"] = 2; //"enter
	m_useIcons["@pick_item"] = 3; //pick
	m_useIcons["@pick_weapon"] = 3; //pick
	m_useIcons["@grab_enemy"] = 4;	//grab
	m_useIcons["@grab_object"] = 4;	//grab
	m_useIcons["@use_ladder"] = 5; //ladder

	Reset();
}

//-----------------------------------------------------------------------------------------------------

CHUDCrosshair::~CHUDCrosshair()
{
	m_animCrossHair.Unload();
	m_animFriendCross.Unload();
	m_animInterActiveIcons.Unload();
}

//-----------------------------------------------------------------------------------------------------

void CHUDCrosshair::Reset()
{
	m_spread = 0.0f;
	m_smoothSpread = 0.0f;
	m_animCrossHair.Invoke("setRecoil", 0.0f);
	m_iCrosshair = -1;
}

//-----------------------------------------------------------------------------------------------------

void CHUDCrosshair::Update(float fDeltaTime)
{
	if(m_bBroken)
		return;

	IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
	IItemSystem *pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
	
	if(!pClientActor || !pItemSystem)
		return;

	IInventory *pInventory = pClientActor->GetInventory();
	if(!pInventory)
		return;

  IItem *pItem = pItemSystem->GetItem(pInventory->GetCurrentItem());      
  IWeapon *pWeapon = NULL;
	IWeapon *pSlaveWeapon = NULL;
	const float fAlternateIronSight = 0.03f;
  
  if(pItem)
	{
		pWeapon = pItem->GetIWeapon();        

		if(pItem->IsDualWieldMaster())
		{
			if(IItem *pSlave=pItem->GetDualWieldSlave())
				pSlaveWeapon = pSlave->GetIWeapon();
		}
  }
  else if(IVehicle *pVehicle=pClientActor->GetLinkedVehicle())
  {
    pItem = pItemSystem->GetItem(pVehicle->GetCurrentWeaponId(pClientActor->GetEntityId()));
    if(pItem)
      pWeapon = pItem->GetIWeapon();
  }

	if(pWeapon)
	{
		float fMinSpread = 0.0f;
		float fMaxSpread = 0.0f;

		m_spread = 0.0f;

		if(IFireMode *pFireMode=pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()))
		{
			fMinSpread = pFireMode->GetMinSpread();
			fMaxSpread = pFireMode->GetMaxSpread();

			m_spread = pFireMode->GetSpread();
		}

		if(pSlaveWeapon)
		{
			if(IFireMode *pSlaveFireMode=pSlaveWeapon->GetFireMode(pSlaveWeapon->GetCurrentFireMode()))
			{
				fMinSpread += pSlaveFireMode->GetMinSpread();
				fMaxSpread += pSlaveFireMode->GetMaxSpread();

				m_spread += pSlaveFireMode->GetSpread();
			}
		}

		CPlayer *pPlayer = static_cast<CPlayer*>(pClientActor);
		if(pPlayer && pPlayer->GetNanoSuit() && pPlayer->GetNanoSuit()->GetMode() == NANOMODE_STRENGTH)
			m_spread *=0.5;

		if(g_pGameCVars->hud_iAlternateCrosshairSpread)
		{
			if(m_spread < fMinSpread)
				m_spread = min(m_spread,g_pGameCVars->hud_fAlternateCrosshairSpreadCrouch) / g_pGameCVars->hud_fAlternateCrosshairSpreadCrouch;
			else
				m_spread = min(m_spread,g_pGameCVars->hud_fAlternateCrosshairSpreadNeutral) / g_pGameCVars->hud_fAlternateCrosshairSpreadNeutral;
		}
		else
		{
			m_spread = min((m_spread-fMinSpread),15.0f) / 15.0f;

			IZoomMode *pZoomMode = pWeapon->GetZoomMode(pWeapon->GetCurrentZoomMode());
			if(pZoomMode && !pZoomMode->IsToggle() && (pZoomMode->IsZoomed() || pZoomMode->IsZooming()))
			{
				m_spread -= fAlternateIronSight;
			}
			else
			{
				m_spread = min(m_spread,1.0f);
				m_spread = max(m_spread,0.0f);
			}
		}
	}

	if(m_animCrossHair.GetVisible() && !g_pHUD->InSpectatorMode())
	{
		//also disables the damage indicator
		if(/*g_pGameCVars->hud_crosshair>0 && m_iCrosshair > 0 &&*/ (g_pGameCVars->g_difficultyLevel<4 || gEnv->bMultiplayer))
		{
			m_animCrossHair.GetFlashPlayer()->Advance(fDeltaTime);
			m_animCrossHair.GetFlashPlayer()->Render();
		}

		if(m_animInterActiveIcons.GetVisible()) //if the crosshair is invisible, the use icon should be too
		{
			if(!m_bHideUseIconTemp)	//hides the icon, when something is already grabbed/being used
			{
				m_animInterActiveIcons.GetFlashPlayer()->Advance(fDeltaTime);
				m_animInterActiveIcons.GetFlashPlayer()->Render();
			}
		}
	}

	if(m_animFriendCross.GetVisible())
	{
		m_animFriendCross.GetFlashPlayer()->Advance(fDeltaTime);
		m_animFriendCross.GetFlashPlayer()->Render();
	}

	if(!g_pGameCVars->hud_iAlternateCrosshairSpread)
	{
		m_spread = max(m_spread,-fAlternateIronSight);
		m_spread = min(m_spread,1.0f);
	}

	if (m_smoothSpread != m_spread)
	{
		Interpolate(m_smoothSpread, m_spread, 20.0f, fDeltaTime);
		m_animCrossHair.Invoke("setRecoil", m_smoothSpread);
	}

	UpdateCrosshair();
}

//-----------------------------------------------------------------------------------------------------

void CHUDCrosshair::SetUsability(int usable, const char* actionLabel, const char* paramA, const char* paramB)
{
	m_bUsable = (usable>0)?true:false;
	m_animCrossHair.Invoke("setUsable", usable);
	if(actionLabel)
	{
		if(paramA)
		{
			string paramLocA;
			string paramLocB;
			if(paramA[0] != '@')
			{
				paramLocA = "@";
				paramLocA.append(paramA);
			}
			else
				paramLocA = paramA;

			if(paramB)
			{
				if(paramB[0] != '@')
				{
					paramLocB = "@";
					paramLocB.append(paramB);
				}
				else
					paramLocB = paramB;
			}

			m_animInterActiveIcons.Invoke("setText",g_pHUD->LocalizeWithParams(actionLabel, true, paramLocA.c_str(), paramLocB.c_str()));
		}
		else
			m_animInterActiveIcons.Invoke("setText", actionLabel);

		//set icon
		int icon = stl::find_in_map(m_useIcons, actionLabel, 0);
		if(!icon && usable)
			icon = 1;
		if(icon)
		{
			m_animInterActiveIcons.SetVisible(true);
			m_animInterActiveIcons.Invoke("setUseIcon", icon);
		}
		else if(m_animInterActiveIcons.GetVisible())
			m_animInterActiveIcons.SetVisible(false);
	}
	else if(m_animInterActiveIcons.GetVisible())
		m_animInterActiveIcons.SetVisible(false);
}

//-----------------------------------------------------------------------------------------------------

bool CHUDCrosshair::GetUsability() const
{
	return m_bUsable;
}

//-----------------------------------------------------------------------------------------------------

void CHUDCrosshair::CrosshairHit()
{
	m_animCrossHair.Invoke("setHit");
}

//-----------------------------------------------------------------------------------------------------

void CHUDCrosshair::SetCrosshair(int iCrosshair)
{
	if(g_pGameCVars->g_difficultyLevel>3 && !gEnv->bMultiplayer)
		iCrosshair = 0;

	iCrosshair = MAX(0,iCrosshair);
	iCrosshair = MIN(13,iCrosshair);

	if(m_iCrosshair != iCrosshair)
	{
		m_iCrosshair = iCrosshair;
		m_animCrossHair.Invoke("setCrossHair", iCrosshair);
		m_animCrossHair.Invoke("setUsable", m_bUsable);
	}
}

//-----------------------------------------------------------------------------------------------------

bool CHUDCrosshair::IsFriendlyEntity(IEntity *pEntity)
{
	IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
	CGameRules *pGameRules = g_pGame->GetGameRules();

	if(!pEntity || !pClientActor || !pGameRules)
		return false;

	// Less than 2 teams means we are in a FFA based game.
	if(pGameRules->GetTeamCount() < 2)
		return false;

	bool bFriendly = false;

	int iClientTeam = pGameRules->GetTeam(pClientActor->GetEntityId());

	// First, check if entity is a player
	IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());
	if(pActor && pActor->IsPlayer())
	{
		if(iClientTeam && (pGameRules->GetTeam(pActor->GetEntityId()) == iClientTeam))
		{
			bFriendly = true;
		}
	}
	else
	{
		// Then, check if entity is a vehicle
		IVehicle *pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(pEntity->GetId());
		if(pVehicle && pGameRules->GetTeam(pVehicle->GetEntityId()) == iClientTeam && pVehicle->GetStatus().passengerCount)
		{
			IActor *pDriver = pVehicle->GetDriver();
			/*if(pDriver && pGameRules->GetTeam(pDriver->GetEntityId()) == iClientTeam)
				bFriendly = true;
			else
				bFriendly = false;*/

			bFriendly = true;

			//fix for bad raycast
			if(pDriver && pDriver == pClientActor)
				bFriendly = false;
		}
	}

  return bFriendly;
}

//-----------------------------------------------------------------------------------------------------

void CHUDCrosshair::UpdateCrosshair()
{
  IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
	if(!pClientActor)
		return;

  int iNewFriendly = 0;

  if(pClientActor->GetLinkedVehicle())
  { 
    // JanM/MichaelR: 
    // Get status from the VehicleWeapon, which raycasts considering the necessary SkipEntities (in contrast to WorldQuery)
    // Julien: this is now done in MP as well
    iNewFriendly = g_pHUD->GetVehicleInterface()->GetFriendlyFire();
  }
  else
  {
    if(!gEnv->bMultiplayer)
    {
			CWeapon *pWeapon = g_pHUD->GetCurrentWeapon();
			if(pWeapon)
			{
				iNewFriendly = pWeapon->IsWeaponLowered() && pWeapon->IsPendingFireRequest();
				if(iNewFriendly && pWeapon->GetEntity()->GetClass() == CItem::sTACGunFleetClass)
					iNewFriendly = 0;
			}
			else{
				//Two handed pickups need the red X as well
				CPlayer *pPlayer= static_cast<CPlayer*>(pClientActor);
				if(CWeapon *pOffHand = static_cast<CWeapon*>(pPlayer->GetItemByClass(CItem::sOffHandClass)))
					iNewFriendly = pOffHand->IsWeaponLowered();
			}
    }
    else
    {
	    EntityId uiCenterId = pClientActor->GetGameObject()->GetWorldQuery()->GetLookAtEntityId();
			if(uiCenterId)
			{
				iNewFriendly = IsFriendlyEntity(gEnv->pEntitySystem->GetEntity(uiCenterId));
			}
    }
  }	

	// SNH: if player is carrying a claymore or mine, ask the weapon whether it is possible to place it currently
	//	(takes into account player speed / stance / aim direction).
	// So 'friendly' is a bit of a misnomer here, but we want the "don't/can't fire" crosshair...
	if(iNewFriendly != 1 && g_pHUD)
	{
		CWeapon *pWeapon = g_pHUD->GetCurrentWeapon();
		if(pWeapon)
		{
			static IEntityClass* pClaymoreClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Claymore");
			static IEntityClass* pAVMineClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AVMine");
			IEntityClass* pClass = pWeapon->GetEntity()->GetClass();
			if(pClass == pClaymoreClass || pClass == pAVMineClass)
			{
				if(IFireMode* pfm = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()))
				{
					if(!pfm->IsFiring())
						iNewFriendly = pWeapon->CanFire() ? 0 : 1;
				}
			}
		}
	}

	if(iNewFriendly != m_iFriendlyTarget)
	{
		m_iFriendlyTarget = iNewFriendly;
		//m_animCrossHair.Invoke("setFriendly", m_iFriendlyTarget);
		if(iNewFriendly)
			m_animFriendCross.SetVisible(true);
		else
			m_animFriendCross.SetVisible(false);
	}

	if(m_animInterActiveIcons.GetVisible())
	{
		m_bHideUseIconTemp = false;
		CItem *pItem = static_cast<CItem*>(pClientActor->GetCurrentItem());
		if(pItem)
		{
			IWeapon *pWeapon = pItem->GetIWeapon();
			if(pWeapon)
			{
				CItem::SStats stats = pItem->GetStats();
				if(stats.mounted && stats.used)
					m_bHideUseIconTemp = true;
			}
		}
		if(!m_bHideUseIconTemp)
		{
			EntityId offHandId = pClientActor->GetInventory()->GetItemByClass(CItem::sOffHandClass);
			IItem *pOffHandItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(offHandId);
			if(pOffHandItem)
			{
				COffHand *pOffHand = static_cast<COffHand*>(pOffHandItem);
				uint32 offHandState = pOffHand->GetOffHandState();
				if(offHandState == eOHS_HOLDING_OBJECT || offHandState == eOHS_THROWING_OBJECT ||
					offHandState == eOHS_HOLDING_NPC || offHandState == eOHS_THROWING_NPC)
					m_bHideUseIconTemp = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDCrosshair::SetOpacity(float opacity)
{
	if(opacity != m_opacity)
	{
		m_animCrossHair.Invoke("setOpacity", opacity);
		m_opacity = opacity;
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDCrosshair::SelectCrosshair(IItem *pItem)
{
	//set special crosshairs design comes up with ...
	bool bSpecialCrosshairSet = false;
	if(IActor *pActor = g_pGame->GetIGameFramework()->GetClientActor())
	{
		if(g_pGameCVars->hud_crosshair != 0)
		{
			if(!pItem) 
				pItem = pActor->GetCurrentItem();

			if(!pItem ||
					pItem->GetEntity()->GetClass() == CItem::sFistsClass ||
					pItem->GetEntity()->GetClass() == CItem::sAlienCloak)
			{
				SetCrosshair(0); //was 10 
				bSpecialCrosshairSet = true;
			}
			else if(IWeapon *pWeapon = pItem->GetIWeapon())		//Laser attached
			{
				if((static_cast<CWeapon*>(pWeapon))->IsLamAttached())
				{
					SetCrosshair(0);
					bSpecialCrosshairSet = true;
				}
			}

			// No current item or current item are fists or AlienCloak or LAW
			if(!bSpecialCrosshairSet)
			{
				if(pItem->GetEntity()->GetClass() == CItem::sRocketLauncherClass ||
					pItem->GetEntity()->GetClass() == CItem::sTACGunFleetClass)
				{
					SetCrosshair(0);
					bSpecialCrosshairSet = true;
				}
				else if(pItem->GetEntity()->GetClass() == CItem::sTACGunClass)
				{
					SetCrosshair(11);
					bSpecialCrosshairSet = true;
				}
				else if(g_pHUD->GetSelectedFiremode() == 6) //sleep bullet
				{
					SetCrosshair(12);
					bSpecialCrosshairSet = true;
				}
				else if(g_pHUD->GetSelectedFiremode() == 4) //grenade launcher
				{
					SetCrosshair(13);
					bSpecialCrosshairSet = true;
				}
			}
		}
	}

	//now set normal crosshair
	if(!bSpecialCrosshairSet)
		SetCrosshair(g_pGameCVars->hud_crosshair);
}

void CHUDCrosshair::Break(bool state)
{
	m_bBroken = state;

	if(CGameFlashAnimation *pAnim = &m_animCrossHair)
	{
		pAnim->Invoke("clearDamageDirection");
		pAnim->GetFlashPlayer()->Advance(0.1f);
	}
}
