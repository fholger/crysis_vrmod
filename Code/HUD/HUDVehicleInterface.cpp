/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Vehicle HUD object (refactored from old HUD code)

-------------------------------------------------------------------------
History:
- 21:02:2007  16:00 : Created by Jan Müller

*************************************************************************/

#include "StdAfx.h"
#include "HUDVehicleInterface.h"
#include "GameFlashAnimation.h"
#include "GameFlashLogic.h"
#include "HUD.h"
#include "HUDRadar.h"
#include "Weapon.h"
#include "IWorldQuery.h"
#include "GameCVars.h"

CHUDVehicleInterface::CHUDVehicleInterface(CHUD *pHUD, CGameFlashAnimation *pAmmo) : m_pVehicle(NULL)
{
	m_bParachute = false;
	m_bAmmoForceNextUpdate = false;
	m_eCurVehicleHUD = EHUD_NONE;
	g_pHUD = pHUD;
	g_pAmmo = pAmmo;  
	m_lastSetFriendly = m_friendlyFire = false;
	m_seatId = InvalidVehicleSeatId;

	m_iSecondaryAmmoCount = m_iPrimaryAmmoCount = m_iSecondaryClipSize = m_iPrimaryClipSize = 0;// m_iHeat = 0;
	m_iLastReloadBarValue = -1;

	m_animMainWindow.Init("Libs/UI/HUD_VehicleHUD.gfx", eFD_Center, eFAF_ManualRender|eFAF_Visible|eFAF_ThisHandler);
	m_animStats.Init("Libs/UI/HUD_VehicleStats.gfx", eFD_Center, eFAF_ManualRender|eFAF_Visible);

	memset(m_hasMainHUD, 0, (int)EHUD_LAST);

	//fill "hasMainHUD" list
	m_hasMainHUD[EHUD_TANKUS] = true;
	m_hasMainHUD[EHUD_AAA] = true;
	m_hasMainHUD[EHUD_HELI] = true;
	m_hasMainHUD[EHUD_VTOL] = true;
	m_hasMainHUD[EHUD_LTV] = false;
	m_hasMainHUD[EHUD_APC] = true;
	m_hasMainHUD[EHUD_APC2] = true;
	m_hasMainHUD[EHUD_SMALLBOAT] = true;
	m_hasMainHUD[EHUD_PATROLBOAT] = true;
	m_hasMainHUD[EHUD_CIVCAR] = false;
	m_hasMainHUD[EHUD_CIVBOAT] = false;
	m_hasMainHUD[EHUD_TRUCK] = false;
	m_hasMainHUD[EHUD_HOVER] = true;
	m_hasMainHUD[EHUD_PARACHUTE ] = true;
	m_hasMainHUD[EHUD_TANKA] = true;

	m_hudTankNames["US_tank"] = "M5A2 Atlas";
	m_hudTankNames["Asian_tank"] = "NK T-108";
	m_hudTankNames["Asian_aaa"] = "NK AAA";
	m_hudTankNames["US_apc"] = "US APC";
}

//-----------------------------------------------------------------------------------------------------

CHUDVehicleInterface::~CHUDVehicleInterface()
{
	if(m_pVehicle)
	{
		m_pVehicle->UnregisterVehicleEventListener(this);
		m_pVehicle = NULL;
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDVehicleInterface::Update(float fDeltaTime)
{
	if(m_animMainWindow.GetVisible())
	{
		if(m_friendlyFire != m_lastSetFriendly)
		{
			m_animMainWindow.Invoke("setFriendly", m_friendlyFire);
			m_lastSetFriendly = m_friendlyFire;
		}

		m_animMainWindow.GetFlashPlayer()->Advance(fDeltaTime);
		m_animMainWindow.GetFlashPlayer()->Render();
	}

	if(m_animStats.GetVisible())
	{
		m_animStats.GetFlashPlayer()->Advance(fDeltaTime);
		m_animStats.GetFlashPlayer()->Render();
	}
	g_pHUD->UpdateCrosshairVisibility();
}

//-----------------------------------------------------------------------------------------------------

bool CHUDVehicleInterface::ForceCrosshair()
{
	if (m_pVehicle && m_seatId != InvalidVehicleSeatId)
	{
		if (IVehicleSeat* pSeat = m_pVehicle->GetSeatById(m_seatId))
		{
			return pSeat->IsGunner() && !m_animMainWindow.GetVisible();
		}
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------

CHUDVehicleInterface::EVehicleHud CHUDVehicleInterface::ChooseVehicleHUD(IVehicle* pVehicle)
{
	if(m_bParachute)
		return EHUD_PARACHUTE;

	if(!pVehicle)
		return EHUD_NONE;

	IEntityClass *cls = pVehicle->GetEntity()->GetClass();
	CHUDRadar *pRadar = g_pHUD->GetRadar();

	if(!cls || !pRadar)
		return EHUD_NONE;

	if(cls == pRadar->m_pTankUS)
	{
		return EHUD_TANKUS;
	}
	else if(cls == pRadar->m_pTankA)
	{
		return EHUD_TANKA;
	}
	else if(cls == pRadar->m_pAAA)
	{
		return EHUD_AAA;
	}
	else if(cls == pRadar->m_pVTOL)
	{
		return EHUD_VTOL;
	}
	else if(cls == pRadar->m_pHeli)
	{
		return EHUD_HELI;
	}
	else if(cls == pRadar->m_pLTVA || cls == pRadar->m_pLTVUS)
	{
		return EHUD_LTV;
	}
	else if(cls == pRadar->m_pAPCUS)
	{
		return EHUD_APC;
	}
	else if(cls == pRadar->m_pAPCA)
	{
		return EHUD_APC2;
	}
	else if(cls == pRadar->m_pTruck)
	{
		return EHUD_TRUCK;
	}
	else if(cls == pRadar->m_pBoatCiv)
	{
		return EHUD_CIVBOAT;
	}
	else if(cls == pRadar->m_pCarCiv)
	{
		return EHUD_CIVCAR;
	}
	else if(cls == pRadar->m_pBoatUS)
	{
		return EHUD_SMALLBOAT;
	}
	else if(cls == pRadar->m_pBoatA)
	{
		return EHUD_PATROLBOAT;
	}
	else if(cls == pRadar->m_pHover)
	{
		return EHUD_HOVER;
	}
	else
		return EHUD_NONE;
}

//-----------------------------------------------------------------------------------------------------

void CHUDVehicleInterface::OnEnterVehicle(IActor *pActor,const char *szVehicleClassName,const char *szSeatName)
{
	m_bParachute = (bool) (!strcmpi(szVehicleClassName,"Parachute"));
	if(m_bParachute)
	{
		bool open = (bool)(!strcmpi(szSeatName,"Open"));
		m_animStats.Reload();
		CRY_ASSERT_MESSAGE(NULL == m_pVehicle,"Attempt to enter in parachute while already in a vehicle!");
		m_animStats.Invoke("setActiveParachute", open);
		if(!open || !m_animMainWindow.GetVisible())
			OnEnterVehicle(static_cast<CPlayer*> (pActor));
	}
	else
		OnEnterVehicle(static_cast<CPlayer*> (pActor));
	g_pHUD->HideInventoryOverview();
}

//-----------------------------------------------------------------------------------------------------

void CHUDVehicleInterface::OnEnterVehicle(CPlayer *pPlayer)
{
	if (!pPlayer || !pPlayer->IsClient())
		return;

	if(m_pVehicle)
	{
		GameWarning("[HUD]: Attempt to enter a vehicle while already in one!");
		return;
	}

	m_pVehicle = pPlayer->GetLinkedVehicle();
	m_seatId = InvalidVehicleSeatId;

	if(m_pVehicle)
	{
		m_pVehicle->RegisterVehicleEventListener(this, "HUDVehicleInterface");

		if (IVehicleSeat *seat = m_pVehicle->GetSeatForPassenger(pPlayer->GetEntityId()))
		{ 
			m_seatId = seat->GetSeatId();
			g_pHUD->UpdateCrosshairVisibility();
		}
	}

	//reset ammos
	m_bAmmoForceNextUpdate = true;

	//choose vehicle hud
	m_eCurVehicleHUD = ChooseVehicleHUD(m_pVehicle);
	LoadVehicleHUDs();

	//setup flash hud
	InitVehicleHuds();

	g_pHUD->UpdateCrosshairVisibility();
}

//-----------------------------------------------------------------------------------------------------

void CHUDVehicleInterface::LoadVehicleHUDs(bool forceEverything)
{
	if(m_hasMainHUD[m_eCurVehicleHUD] || forceEverything)
		m_animMainWindow.Reload();
	m_animStats.Reload();
}

//-----------------------------------------------------------------------------------------------------

void CHUDVehicleInterface::OnExitVehicle(IActor *pActor)
{
	if(m_pVehicle)
	{
		m_pVehicle->UnregisterVehicleEventListener(this);
		m_pVehicle = NULL;
	}
	else
		m_bParachute = false;

	m_seatId = InvalidVehicleSeatId;

	if(m_eCurVehicleHUD!=EHUD_NONE)
	{
		HideVehicleInterface();
		m_eCurVehicleHUD = EHUD_NONE;
	}

	m_animMainWindow.Unload();
	m_animStats.Unload();

	g_pHUD->UpdateCrosshairVisibility();
}

//-----------------------------------------------------------------------------------------------------

void CHUDVehicleInterface::InitVehicleHuds()
{
	if(m_eCurVehicleHUD != EHUD_NONE)
	{
		const char *szVehicleClassName = NULL;

		if(m_pVehicle)
		{
			szVehicleClassName = m_pVehicle->GetEntity()->GetClass()->GetName();
		}

		m_animMainWindow.Invoke("showHUD");
		m_animMainWindow.Invoke("setVehicleHUDMode", (int)m_eCurVehicleHUD);
		m_animMainWindow.SetVisible(true);

		m_animStats.Invoke("showStats");
		m_animStats.Invoke("setVehicleStatsMode", (int)m_eCurVehicleHUD);
		m_animStats.SetVisible(true);

		UpdateVehicleHUDDisplay();
		UpdateDamages(m_eCurVehicleHUD, m_pVehicle);

		if(szVehicleClassName)
		{
			string name = m_hudTankNames[szVehicleClassName];

			if(g_pAmmo)
				g_pAmmo->Invoke("setTankName", name.c_str());
		}
		m_statsSpeed = -999;
		m_statsHeading = -999;
		m_lastSetFriendly = false;
		m_friendlyFire = false;

		if(m_eCurVehicleHUD == EHUD_VTOL)
			m_animStats.Invoke("disableEject", !(gEnv->bMultiplayer));	//no eject warning in SP
	}
}

//-----------------------------------------------------------------------------------------------------

float CHUDVehicleInterface::UpdateDamages(EVehicleHud eHud, IVehicle *pVehicle, bool updateFlash)	//this could be put in an xml file for better mod-ability
{
	CHUDRadar *pRadar = g_pHUD->GetRadar();
	if(!pVehicle || !pVehicle->GetEntity() || !pRadar)
		return 1.0f;
	if(eHud == EHUD_TANKA || eHud == EHUD_TANKUS || eHud == EHUD_APC)
	{
		IEntityClass *cls = pVehicle->GetEntity()->GetClass();
		if(cls == pRadar->m_pTankA || cls == pRadar->m_pTankUS || cls == pRadar->m_pAPCUS)
		{
			float fH = pVehicle->GetComponent("hull")->GetDamageRatio();
			float fE = pVehicle->GetComponent("engine")->GetDamageRatio();
			float fL = pVehicle->GetComponent("leftTread")->GetDamageRatio();
			float fR = pVehicle->GetComponent("rightTread")->GetDamageRatio();
			float fT = pVehicle->GetComponent("turret")->GetDamageRatio();
			if(updateFlash)
			{
				SFlashVarValue args[5] = {fH, fE, fL, fR, fT};
				m_animStats.Invoke("setDamage", args, 5);
			}
			return (fH + fE +fL + fR + fT) * 0.2f;
		}
	}
	else if(eHud == EHUD_APC2)
	{
		if(pVehicle->GetEntity()->GetClass() == pRadar->m_pAPCA)
		{
			float fH = pVehicle->GetComponent("hull")->GetDamageRatio();
			float fE = pVehicle->GetComponent("engine")->GetDamageRatio();
			float fT = pVehicle->GetComponent("turret")->GetDamageRatio();
			float fW1 = pVehicle->GetComponent("wheel1")->GetDamageRatio();
			float fW2 = pVehicle->GetComponent("wheel2")->GetDamageRatio();
			float fW3 = pVehicle->GetComponent("wheel3")->GetDamageRatio();
			float fW4 = pVehicle->GetComponent("wheel4")->GetDamageRatio();
			float fW5 = pVehicle->GetComponent("wheel5")->GetDamageRatio();
			float fW6 = pVehicle->GetComponent("wheel6")->GetDamageRatio();
			float fW7 = pVehicle->GetComponent("wheel7")->GetDamageRatio();
			float fW8 = pVehicle->GetComponent("wheel8")->GetDamageRatio();
			if(updateFlash)
			{
				SFlashVarValue args[11] = {fH, fE, fT, fW1, fW5, fW2, fW6, fW3, fW7, fW4, fW8};
				m_animStats.Invoke("setDamage", args, 11);
			}
			return (fH + fE + fT + fW1 + fW2 + fW3 + fW4 + fW5 + fW6 + fW7 + fW8) / 11.0f;
		}
	}
	else if(eHud == EHUD_AAA)
	{
		if(pVehicle->GetEntity()->GetClass() == pRadar->m_pAAA)
		{
			float hull = pVehicle->GetComponent("hull")->GetDamageRatio();
			float turret = pVehicle->GetComponent("turret")->GetDamageRatio();
			float radar = pVehicle->GetComponent("radar")->GetDamageRatio();
			if(updateFlash)
			{
				SFlashVarValue args[3] = {hull, turret, radar};
				m_animStats.Invoke("setDamage", args, 3);
			}
			return (hull + turret + radar) * 0.3333f;
		}
	}
	else if(eHud == EHUD_VTOL)
	{
		if(pVehicle->GetEntity()->GetClass() == pRadar->m_pVTOL)
		{
			float fH = pVehicle->GetComponent("Hull")->GetDamageRatio();
			float fL = 0.0f; //pVehicle->GetComponent("WingLeft")->GetDamageRatio();
			float fR = 0.0f; //pVehicle->GetComponent("WingRight")->GetDamageRatio();
			if(updateFlash)
			{
				SFlashVarValue args[3] = {fH, fL, fR};
				m_animStats.Invoke("setDamage", args, 3);
			}
			//return (fH + fL + fR) * 0.3333f;
			return fH;
		}
	}
	else if(eHud == EHUD_HELI)
	{
		if(pVehicle->GetEntity()->GetClass() == pRadar->m_pHeli)
		{
			float fH = pVehicle->GetComponent("Hull")->GetDamageRatio();
			float fR = pVehicle->GetComponent("Rotor")->GetDamageRatio();
			float fE = pVehicle->GetComponent("BackRotor")->GetDamageRatio();
			float fC = pVehicle->GetComponent("Cockpit")->GetDamageRatio();
			if(updateFlash)
			{
				SFlashVarValue args[4] = {fH, fR, fE, fC};
				m_animStats.Invoke("setDamage", args, 4);
			}
			return (fH + fR + fE + fC) * 0.25f;
		}
	}
	else if(eHud == EHUD_LTV)
	{
		if(pVehicle->GetEntity()->GetClass() == pRadar->m_pLTVA || pVehicle->GetEntity()->GetClass() == pRadar->m_pLTVUS)
		{
			float fH = pVehicle->GetComponent("Hull")->GetDamageRatio();
			float fE = pVehicle->GetComponent("Engine")->GetDamageRatio();
			float fT = pVehicle->GetComponent("FuelCan")->GetDamageRatio();
			float fW1 = pVehicle->GetComponent("wheel1")->GetDamageRatio();
			float fW2 = pVehicle->GetComponent("wheel2")->GetDamageRatio();
			float fW3 = pVehicle->GetComponent("wheel3")->GetDamageRatio();
			float fW4 = pVehicle->GetComponent("wheel4")->GetDamageRatio();
			if(updateFlash)
			{
				SFlashVarValue args[7] = {fH, fE, fT, fW1, fW2, fW3, fW4};
				m_animStats.Invoke("setDamage", args, 7);
			}
			return (fH + fE + fT + fW1 + fW2 + fW3 + fW4) / 7.0f;
		}
	}
	else if(eHud == EHUD_TRUCK)
	{
		if(pVehicle->GetEntity()->GetClass() == pRadar->m_pTruck)
		{
			float fH = pVehicle->GetComponent("Hull")->GetDamageRatio();
			float fE = pVehicle->GetComponent("Engine")->GetDamageRatio();
			float fT1 = pVehicle->GetComponent("LeftFuelTank")->GetDamageRatio();
			float fT2 = pVehicle->GetComponent("RightFuelTank")->GetDamageRatio();
			float fW1 = pVehicle->GetComponent("wheel1")->GetDamageRatio();
			float fW2 = pVehicle->GetComponent("wheel2")->GetDamageRatio();
			float fW3 = pVehicle->GetComponent("wheel3")->GetDamageRatio();
			float fW4 = pVehicle->GetComponent("wheel4")->GetDamageRatio();
			float fW5 = pVehicle->GetComponent("wheel5")->GetDamageRatio();
			float fW6 = pVehicle->GetComponent("wheel6")->GetDamageRatio();
			if(updateFlash)
			{
				SFlashVarValue args[10] = {fH, fE, fT1, fT2, fW1, fW2, fW3, fW4, fW5, fW6};
				m_animStats.Invoke("setDamage", args, 10);
			}
			return (fH + fE + fT1 + fT2 + fW1 + fW2 + fW3 + fW4 + fW5 + fW6) * 0.1f;
		}
	}
	else if(eHud == EHUD_SMALLBOAT || m_eCurVehicleHUD == EHUD_CIVBOAT)
	{
		if(pVehicle->GetEntity()->GetClass() == pRadar->m_pBoatUS || pVehicle->GetEntity()->GetClass() == pRadar->m_pBoatCiv)
		{
			float fH = pVehicle->GetComponent("Hull")->GetDamageRatio();
			float fL = pVehicle->GetComponent("leftEngine")->GetDamageRatio();
			float fR = pVehicle->GetComponent("rightEngine")->GetDamageRatio();
			if(updateFlash)
			{
				SFlashVarValue args[3] = {fH, fL, fR};
				m_animStats.Invoke("setDamage", args, 3);
			}
			return (fH + fL + fR) * 0.3333f;
		}
	}
	else if(eHud == EHUD_PATROLBOAT)
	{
		if(pVehicle->GetEntity()->GetClass() == pRadar->m_pBoatA)
		{
			float fH = pVehicle->GetComponent("Hull")->GetDamageRatio();
			if(updateFlash)
			{
				SFlashVarValue args[3] = {fH, 0, 0};
				m_animStats.Invoke("setDamage", args, 3);
			}
			return fH;
		}
	}
	else if(eHud == EHUD_HOVER)
	{
		if(pVehicle->GetEntity()->GetClass() == pRadar->m_pHover)
		{
			float fH = pVehicle->GetComponent("Hull")->GetDamageRatio();
			if(updateFlash)
			{
				SFlashVarValue args[3] = {fH, 0, 0};
				m_animStats.Invoke("setDamage", args, 3);
			}
			return fH;
		}
	}
	else if(eHud == EHUD_CIVCAR)
	{
		if(pVehicle->GetEntity()->GetClass() == pRadar->m_pCarCiv)
		{
			float fH = pVehicle->GetComponent("Hull")->GetDamageRatio();
			float fE = pVehicle->GetComponent("Engine")->GetDamageRatio();
			float fW1 = pVehicle->GetComponent("wheel1")->GetDamageRatio();
			float fW2 = pVehicle->GetComponent("wheel2")->GetDamageRatio();
			float fW3 = pVehicle->GetComponent("wheel3")->GetDamageRatio();
			float fW4 = pVehicle->GetComponent("wheel4")->GetDamageRatio();
			if(updateFlash)
			{
				SFlashVarValue args[6] = {fH,fE, fW1, fW2, fW3, fW4};
				m_animStats.Invoke("setDamage", args, 6);
			}
			return (fH + fE + fW1 + fW2 + fW3 + fW4) / 6;
		}
	}
	return 1.0f;
}

//-----------------------------------------------------------------------------------------------------

void CHUDVehicleInterface::UpdateSeats()
{
	if(!m_pVehicle)
		return;
	int seatCount = m_pVehicle->GetSeatCount();
	for(int i = 1; i <= seatCount; ++i)
	{
		IVehicleSeat *pSeat = m_pVehicle->GetSeatById(TVehicleSeatId(i));
		if(pSeat)
		{
			EntityId passenger = pSeat->GetPassenger();

			//set seats in flash
			if(m_eCurVehicleHUD)
			{
				SFlashVarValue args[2] = {i, 0};
				if(passenger)
				{
					IActor *pActor=gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(passenger);					
					if (pActor && pActor->GetHealth()>0) // don't show dead players on the hud
					{					
						// set different colors if the passenger is the player
						args[1] = (passenger == gEnv->pGame->GetIGameFramework()->GetClientActor()->GetEntityId())?2:1;
					}
				}
				else if(pSeat->IsLocked())
				{
					args[1] = 3;
				}
				m_animStats.Invoke("setSeat", args, 2);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDVehicleInterface::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	if(eVE_VehicleDeleted == event)  
	{
		m_pVehicle = NULL;
		m_seatId = InvalidVehicleSeatId;
	}

	if (!m_pVehicle)
		return;

	CActor *pPlayerActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if(!pPlayerActor)
		return;

	if(event == eVE_PassengerEnter || event == eVE_PassengerChangeSeat || event == eVE_SeatFreed)
	{
		g_pHUD->m_buyMenuKeyLog.Clear();
		if(params.entityId == pPlayerActor->GetEntityId())
		{ 
			m_seatId = params.iParam;

			UpdateVehicleHUDDisplay();
			g_pHUD->UpdateBuyMenuPages();
		}

		if (eVE_PassengerChangeSeat == event)
		{ 
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(params.entityId))
			{
				IEntitySoundProxy* pSoundProxy = (IEntitySoundProxy*)pEntity->GetProxy(ENTITY_PROXY_SOUND);
				if (pSoundProxy)      
					pSoundProxy->PlaySound("sounds/physics:player_foley:switch_seat", Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_OneY, 0, eSoundSemantic_Player_Foley);
				if(pPlayerActor->GetEntity() == pEntity)
					g_pHUD->SetFireMode(NULL, NULL, true);
			}
		}

		UpdateSeats();
	}
	else if(eVE_Damaged == event || eVE_Collision == event || eVE_Repair)
	{
		UpdateDamages(m_eCurVehicleHUD, m_pVehicle);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDVehicleInterface::UpdateVehicleHUDDisplay()
{
	CActor *pPlayerActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if(!pPlayerActor)
		return;

	IVehicleSeat *seat = NULL;
	if(m_pVehicle)
	{
		seat = m_pVehicle->GetSeatForPassenger(pPlayerActor->GetEntityId());
		if(!seat)
			return;
	}

	m_eCurVehicleHUD = ChooseVehicleHUD(m_pVehicle);

	if(!g_pAmmo)
		return;

	if((seat && seat->IsDriver()) || m_bParachute)
	{
		if(m_hasMainHUD[m_eCurVehicleHUD])
		{
			m_animMainWindow.Invoke("showHUD");
			m_animMainWindow.SetVisible(true);
		}
		else if(m_animMainWindow.IsLoaded())
			m_animMainWindow.Unload();

		if(m_pVehicle && m_pVehicle->GetWeaponCount() > 1)
			g_pAmmo->Invoke("showReloadDuration2");
		else
			g_pAmmo->Invoke("hideReloadDuration2");
		g_pAmmo->Invoke("setAmmoMode", m_eCurVehicleHUD);
	}
	else if(seat && seat->IsGunner())
	{
		m_animMainWindow.Invoke("hideHUD");
		m_animMainWindow.SetVisible(false);
		g_pAmmo->Invoke("setAmmoMode", 0);
	}
	else
	{
		m_animMainWindow.Invoke("hideHUD");
		m_animMainWindow.SetVisible(false);
		g_pAmmo->Invoke("setAmmoMode", 21);
	}

	g_pHUD->UpdateCrosshairVisibility();
}

//-----------------------------------------------------------------------------------------------------

bool CHUDVehicleInterface::IsAbleToBuy()
{
	if(!m_pVehicle)
		return false;

	if(m_seatId == InvalidVehicleSeatId)
		return false;

	if (IVehicleSeat* pSeat = m_pVehicle->GetSeatById(m_seatId))
	{
		return pSeat->IsDriver() || pSeat->IsGunner();
	}
	return false;
}

//-----------------------------------------------------------------------------------------------------

float CHUDVehicleInterface::GetVehicleSpeed()
{
	float fSpeed = 0.0;
	if(m_pVehicle)
	{
		fSpeed = m_pVehicle->GetStatus().speed;
	}
	else
	{
		CActor *pPlayerActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
		if(pPlayerActor)
		{
			fSpeed = pPlayerActor->GetActorStats()->velocity.len();
		}
	}
	fSpeed *= 2.24f; // Meter per second TO Miles hour
	return fSpeed;
}

//-----------------------------------------------------------------------------------------------------

float CHUDVehicleInterface::GetVehicleHeading()
{
	float fAngle = 0.0;
	if(m_pVehicle)
	{
		SMovementState sMovementState;
		m_pVehicle->GetMovementController()->GetMovementState(sMovementState);
		Vec3 vEyeDirection = sMovementState.eyeDirection;
		vEyeDirection.z = 0.0f;
		vEyeDirection.normalize();
		fAngle = RAD2DEG(acos_tpl(vEyeDirection.x));
		if(vEyeDirection.y < 0) fAngle = -fAngle;
	}
	return fAngle;
}


//-----------------------------------------------------------------------------------------------------

float CHUDVehicleInterface::GetRelativeHeading()
{
	float fAngle = 0.0;
	if(m_pVehicle)
	{
		CActor *pPlayerActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
		if(pPlayerActor)
		{
			if (IVehicleSeat *pSeat = m_pVehicle->GetSeatForPassenger(pPlayerActor->GetEntityId()))
			{
				//this is kinda workaround since it requires the "turning part" of the vehicle to be called "turret" (but everything else would be way more complicated)
				if (IVehiclePart* pPart = m_pVehicle->GetPart("turret"))
				{
					const Matrix34& matLocal = pPart->GetLocalTM(false);

					Vec3 vLocalLook = matLocal.GetColumn(1);
					vLocalLook.z = 0.0f;
					vLocalLook.normalize();

					fAngle = RAD2DEG(acos_tpl(vLocalLook.x));
					if(vLocalLook.y < 0) fAngle = -fAngle;
					fAngle -= 90.0f;
				}
				else
					return 0.0f;
			}
		}
	}
	return fAngle;
}

//-----------------------------------------------------------------------------------------------------

void CHUDVehicleInterface::ShowVehicleInterface(EVehicleHud type, bool forceFlashUpdate)
{
	if(!m_pVehicle && !m_bParachute)
		return;

	CActor *pPlayerActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor());

	IVehicleSeat *pSeat = NULL;
	if(m_pVehicle)
	{
		pSeat = m_pVehicle->GetSeatForPassenger(pPlayerActor->GetEntityId());
		if(!pSeat)
			return;
	}

	if(g_pAmmo)
	{
		int iPrimaryAmmoCount = 0;
		int iPrimaryClipSize = 0;
		int iSecondaryAmmoCount = 0;
		int iSecondaryClipSize = 0;

		if(m_pVehicle)
		{
			CWeapon *pWeapon = pPlayerActor->GetWeapon(m_pVehicle->GetCurrentWeaponId(pPlayerActor->GetEntity()->GetId()));
			if(pWeapon)
			{
				IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
				if(pFireMode)
				{
					IEntityClass *pAmmoType = pFireMode->GetAmmoType();

					iPrimaryClipSize = pFireMode->GetClipSize();
					if (iPrimaryClipSize==0)
						iPrimaryAmmoCount=m_pVehicle->GetAmmoCount(pAmmoType);
					else
					{
						if (iPrimaryClipSize!=-1)
							iPrimaryClipSize=m_pVehicle->GetAmmoCount(pAmmoType);
						iPrimaryAmmoCount=pWeapon->GetAmmoCount(pAmmoType);
					}

					/*if(pFireMode->CanOverheat())
					{
						int heat = int(pFireMode->GetHeat()*100);
						if(m_iHeat != heat)
						{
							SFlashVarValue args[2] = {true, pFireMode->GetHeat()*100};
							g_pAmmo->Invoke("setOverheatBar", args, 2);
							m_iHeat = heat;
						}
					}
					else
						m_iHeat = 0;*/
					
					int fmIdx = g_pHUD->GetSelectedFiremode();
					if(fmIdx == 8 || fmIdx == 15 || fmIdx == 22 || fmIdx == 24) //infite ammo firemodes
						iSecondaryClipSize = -1;
				}
			}
			else
			{
				if(IItem *pFists = pPlayerActor->GetItemByClass(CItem::sFistsClass))
					g_pHUD->SetFireMode(pFists, NULL);
			}

			pWeapon = pPlayerActor->GetWeapon(m_pVehicle->GetCurrentWeaponId(pPlayerActor->GetEntity()->GetId(),true));
			if(pWeapon)
			{
				IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
				if(pFireMode)
				{
					IEntityClass *pAmmoType = pFireMode->GetAmmoType();

					iSecondaryClipSize = pFireMode->GetClipSize();
					if (iSecondaryClipSize==0)
						iSecondaryAmmoCount=m_pVehicle->GetAmmoCount(pAmmoType);
					else
					{
						if (iSecondaryClipSize!=-1)
							iSecondaryClipSize=m_pVehicle->GetAmmoCount(pAmmoType);
						iSecondaryAmmoCount=pWeapon->GetAmmoCount(pAmmoType);
					}
				}
			}

			if(	forceFlashUpdate ||
				m_iSecondaryAmmoCount	!= iSecondaryAmmoCount	||
				m_iPrimaryAmmoCount		!= iPrimaryAmmoCount	||
				m_iSecondaryClipSize	!= iSecondaryClipSize	||
				m_iPrimaryClipSize		!= iPrimaryClipSize		||
				m_bAmmoForceNextUpdate)
			{
				SFlashVarValue args[7] = {iSecondaryAmmoCount, iPrimaryAmmoCount, iSecondaryClipSize, iPrimaryClipSize, 0, "", false};
				g_pAmmo->Invoke("setAmmo", args, 7);
				//if(iSecondaryClipSize == -1)
				//	g_pAmmo->Invoke("setFireMode", 8);
				m_iSecondaryAmmoCount	= iSecondaryAmmoCount;
				m_iPrimaryAmmoCount		= iPrimaryAmmoCount;
				m_iSecondaryClipSize	= iSecondaryClipSize;
				m_iPrimaryClipSize		= iPrimaryClipSize;
				m_bAmmoForceNextUpdate = false;
			}
		}
	}

	SMovementState sMovementState;
	if(m_pVehicle)
	{
		m_pVehicle->GetMovementController()->GetMovementState(sMovementState);
	}
	else
	{
		pPlayerActor->GetMovementController()->GetMovementState(sMovementState);
	}

	float fAngle = GetVehicleHeading();
	float fSpeed = GetVehicleSpeed();
	float fRelAngle = GetRelativeHeading();

	float fPosHeading = (fAngle*8.0f/3.0f);

	wchar_t szN[256];
	wchar_t szW[256];
	char szSpeed[32];
	char szAltitude[32];
	char szDistance[32];

	CrySwprintf(szN,32,L"%.0f",0.f);
	CrySwprintf(szW,32,L"%.0f",0.f);
	sprintf(szSpeed,"%.2f",fSpeed);
	sprintf(szAltitude,"%.0f",0.f);
	sprintf(szDistance,"%.0f",0.f);

	float fAltitude;

	if(((int)fSpeed) != m_statsSpeed)
	{
		SFlashVarValue args[2] = {szSpeed, (int)fSpeed};
		m_animStats.CheckedInvoke("setSpeed", args, 2);
		m_statsSpeed = (int)fSpeed;
	}

	// Note: this needs to be done even if we are not the driver, as the driver
	// may change the direction of the main turret while we'are at the gunner seat
	if(pSeat && (type == EHUD_TANKA || type == EHUD_TANKUS || type == EHUD_AAA || type == EHUD_APC || type == EHUD_APC2))
	{
		if((int)fRelAngle != m_statsHeading)
		{
			m_animStats.CheckedInvoke("setDirection", (int)fRelAngle);	//vehicle/turret angle
			m_statsHeading = (int)fRelAngle;
		}
	}

	if(type == EHUD_VTOL || type == EHUD_HELI || type == EHUD_PARACHUTE)
	{
		float fHorizon;
		if(m_pVehicle)
		{
			Vec3 vWorldPos = m_pVehicle->GetEntity()->GetWorldPos();
			float waterLevel = gEnv->p3DEngine->GetWaterLevel(&vWorldPos);
			float terrainZ = GetISystem()->GetI3DEngine()->GetTerrainZ((int)vWorldPos.x,(int)vWorldPos.y);
			if(terrainZ < waterLevel)
				terrainZ = waterLevel;
			fAltitude = vWorldPos.z-terrainZ;
			fHorizon = -RAD2DEG(m_pVehicle->GetEntity()->GetWorldAngles().y);
		}
		else
		{
			Vec3 vWorldPos = pPlayerActor->GetEntity()->GetWorldPos();
			float waterLevel = gEnv->p3DEngine->GetWaterLevel(&vWorldPos);
			float terrainZ = GetISystem()->GetI3DEngine()->GetTerrainZ((int)vWorldPos.x,(int)vWorldPos.y);
			if(terrainZ < waterLevel)
				terrainZ = waterLevel;
			fAltitude = vWorldPos.z-terrainZ;
			fHorizon = -RAD2DEG(pPlayerActor->GetAngles().y);
		}

		sprintf(szAltitude,"%.2f",fAltitude);

		g_pHUD->GetGPSPosition(szN,szW);
		m_animMainWindow.Invoke("setHorizon", fHorizon);
	}
	if(type == EHUD_VTOL || type == EHUD_HELI)
	{
		float fVerticalHorizon = 0.0f;
		if(m_pVehicle)
		{
			fVerticalHorizon = RAD2DEG(m_pVehicle->GetEntity()->GetWorldAngles().x);
			m_animMainWindow.Invoke("setVerticalHorizon", fVerticalHorizon);
		}
	}
	if(type == EHUD_VTOL || type == EHUD_HELI || type == EHUD_TANKA || type == EHUD_TANKUS ||
		type == EHUD_AAA || type == EHUD_LTV || type == EHUD_APC || type == EHUD_TRUCK ||
		type == EHUD_SMALLBOAT || type == EHUD_PATROLBOAT || type == EHUD_APC2)
	{
		if(g_pAmmo)
		{
			IWeapon *pPlayerWeapon = pPlayerActor->GetWeapon(m_pVehicle->GetCurrentWeaponId(pPlayerActor->GetEntity()->GetId()));
			if(pPlayerWeapon)
			{
				if(IFireMode *pFireMode = pPlayerWeapon->GetFireMode(pPlayerWeapon->GetCurrentFireMode()))
				{
					int duration = 0;
					if(pFireMode->CanOverheat())
					{
						duration = (int)(pFireMode->GetHeat()*100);
						if(duration != m_iLastReloadBarValue)
						{
							g_pAmmo->Invoke("setOverheatBar", duration);
							m_iLastReloadBarValue = duration;
						}
					}
					else
					{
						float fFireRate = 60.0f / pFireMode->GetFireRate();
						float fNextShotTime = pFireMode->GetNextShotTime();
						duration = int(((fFireRate-fNextShotTime)/fFireRate)*100.0f+1.0f);
						if(duration != m_iLastReloadBarValue)
						{
							g_pAmmo->Invoke("setReloadDuration", duration);
							if(g_pGameCVars->hud_showBigVehicleReload && m_hasMainHUD[m_eCurVehicleHUD])
								m_animMainWindow.Invoke("setReloadDuration", duration);
							m_iLastReloadBarValue = duration;
						}
					}
				}
			}

			if(type == EHUD_AAA || type == EHUD_APC || type == EHUD_APC2 || type == EHUD_TANKA || type == EHUD_TANKUS || type == EHUD_VTOL || type == EHUD_HELI)	//get reload for secondary guns
			{
				IItem *pItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_pVehicle->GetWeaponId(1));
				if(pItem)
				{
					IWeapon *pWeapon =  pItem->GetIWeapon();
					if(IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()))
					{
						int duration = 0;
						if(pFireMode->CanOverheat())
						{
							duration = (int)(pFireMode->GetHeat()*100);
							if(duration != m_iLastReloadBarValue2)
							{
								g_pAmmo->Invoke("setOverheatBar2", duration);
								m_iLastReloadBarValue2 = duration;
							}
						}
						else
						{
							float fFireRate = 60.0f / pFireMode->GetFireRate();
							float fNextShotTime = pFireMode->GetNextShotTime();
							duration = int(((fFireRate-fNextShotTime)/fFireRate)*100.0f+1.0f);
							if(duration != m_iLastReloadBarValue2)
							{
								g_pAmmo->Invoke("setReloadDuration2", duration);
								if(g_pGameCVars->hud_showBigVehicleReload && m_hasMainHUD[m_eCurVehicleHUD])
									m_animMainWindow.Invoke("setReloadDuration2", duration);
								m_iLastReloadBarValue2 = duration;
							}
						}
					}
				}
			}
		}

		// FIXME: This one doesn't work because the nearest object often is ... the cannon of the tank !!!
		const ray_hit *pRay = pPlayerActor->GetGameObject()->GetWorldQuery()->GetLookAtPoint(100.0f);

		if(pRay)
		{
			sprintf(szDistance,"%.1f",pRay->dist);
		}
		else
		{
			sprintf(szDistance,"%.0f",11.0);
			szDistance[0]='-';
		}
	}

	{
		SFlashVarValue args[10] = {(int)fPosHeading, szN, szW, szAltitude, fAltitude, (int)(sMovementState.eyeDirection.z*90.0), szSpeed, (int)fSpeed, (int)fAngle, szDistance};
		m_animMainWindow.Invoke("setVehicleValues", args, 10);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDVehicleInterface::HideVehicleInterface()
{
	m_animMainWindow.Invoke("hideHUD");
	m_animMainWindow.SetVisible(false);

	m_animStats.Invoke("hideStats");
	m_animStats.SetVisible(false);
}

//-----------------------------------------------------------------------------------------------------

void CHUDVehicleInterface::UnloadVehicleHUD(bool remove)
{
	if(remove)
	{
		m_animStats.Unload();
		m_animMainWindow.Unload();
		m_statsSpeed = -999;
		m_statsHeading = -999;
	}
	else if(m_pVehicle && m_eCurVehicleHUD != EHUD_NONE)
	{
		if(m_hasMainHUD[m_eCurVehicleHUD])
		{
			m_animMainWindow.Reload();
			m_animMainWindow.SetVariable("SkipSequence",SFlashVarValue(true));
		}
		m_animStats.Reload();
		ShowVehicleInterface(m_eCurVehicleHUD);
		InitVehicleHuds();
		UpdateVehicleHUDDisplay();
		UpdateSeats();
		UpdateDamages(m_eCurVehicleHUD, m_pVehicle);
	}
}

void CHUDVehicleInterface::Serialize(TSerialize ser)
{
	ser.Value("hudParachute", m_bParachute);	
	EVehicleHud oldVehicleHud = m_eCurVehicleHUD;
	ser.EnumValue("CurVehicleHUD", m_eCurVehicleHUD, EHUD_NONE, EHUD_LAST);

	if(ser.IsReading())
	{
		IActor *pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
		if(pActor)
		{
			OnExitVehicle(pActor);
		}
	}
}
