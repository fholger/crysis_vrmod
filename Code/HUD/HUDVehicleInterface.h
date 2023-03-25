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

#ifndef HUD_VEHICLE_INTERFACE_H
#define HUD_VEHICLE_INTERFACE_H

# pragma once


#include "HUDObject.h"
#include "IFlashPlayer.h"
#include "IVehicleSystem.h"
#include "GameFlashAnimation.h"

class CHUD;
class CPlayer;

class CHUDVehicleInterface : IVehicleEventListener
{
	friend class CHUD;

public:

	enum EVehicleHud 
	{
		EHUD_NONE = 0,
		EHUD_VTOL,
		EHUD_TANKUS,
		EHUD_AAA,
		EHUD_HELI,
		EHUD_LTV,
		EHUD_APC,
		EHUD_SMALLBOAT,
		EHUD_PATROLBOAT,
		EHUD_CIVBOAT,
		EHUD_HOVER,
		EHUD_TRUCK,
		EHUD_CIVCAR,
		EHUD_PARACHUTE,
		EHUD_TANKA,
		EHUD_APC2,
		EHUD_LAST
	};

	CHUDVehicleInterface(CHUD *pHUD, CGameFlashAnimation *pAmmo);
	~CHUDVehicleInterface();

	void Update(float fDeltaTime);

	void Serialize(TSerialize ser);
	bool IsAbleToBuy();

	// IVehicleEventListener
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);
	// ~IVehicleEventListener

	//get the current vehicle
	ILINE IVehicle *GetVehicle() { return m_pVehicle; }
	//get the current hud type
	ILINE EVehicleHud GetHUDType() { return m_eCurVehicleHUD; }
	//special case : is this a parachute ??
	ILINE bool IsParachute() { return m_bParachute; }
	//aiming at friendly unit
	ILINE void DisplayFriendlyFire( bool friendly ) { m_friendlyFire = friendly; }
	ILINE bool GetFriendlyFire() { return m_friendlyFire; }
	ILINE void AmmoForceNextUpdate() { m_bAmmoForceNextUpdate = true; }

	//select current interface
	EVehicleHud ChooseVehicleHUD(IVehicle* pVehicle);
	//update damaged parts in stats
	float UpdateDamages(EVehicleHud eHud, IVehicle *pVehicle, bool updateFlash = true);

private:

	//player interaction - called by main hud
	void OnEnterVehicle(IActor *pActor,const char *strVehicleClassName,const char *strSeatName);
	void OnExitVehicle(IActor *pActor);

	//main enter function
	void OnEnterVehicle(CPlayer *pPlayer);
	//update used seats
	void UpdateSeats();
	//display specific interface
	void ShowVehicleInterface(EVehicleHud type, bool forceFlashUpdate = false);
	//sets basic value to the flash asset
	void InitVehicleHuds();
	//hide current interface
	void HideVehicleInterface();
	//unload flash movies
	void UnloadVehicleHUD(bool remove);
	//update all displays
	void LoadVehicleHUDs(bool force=false);
	void UpdateVehicleHUDDisplay();
	//get vehicle data
	float GetVehicleSpeed();
	float GetVehicleHeading();
	float GetRelativeHeading();

	bool ForceCrosshair();

	//****************************************** MEMBER VARIABLES ***********************************

	//the main HUD
	CHUD			*g_pHUD;
	//the vehicle flash movies
	CGameFlashAnimation m_animMainWindow;
	CGameFlashAnimation m_animStats;
	//another flash animation controlled by hud.cpp
	CGameFlashAnimation *g_pAmmo;
	//the vehicle
	IVehicle	*m_pVehicle; 
	//seat occupied
	TVehicleSeatId m_seatId;
	//special case parachute (no vehicle)
	bool			m_bParachute;
	//which HUD animation is shown ?
	EVehicleHud	m_eCurVehicleHUD;
	//saves whether the specified hud has a main window
	bool			m_hasMainHUD[EHUD_LAST];
	//saves the displayed tank names
	std::map<string, string> m_hudTankNames;
	//saving invokes
	int				m_statsSpeed, m_statsHeading;
	//aim-at-friend state settings
	bool			m_lastSetFriendly, m_friendlyFire;
	//ammo values last send
	int m_iSecondaryAmmoCount, m_iPrimaryAmmoCount, m_iSecondaryClipSize, m_iPrimaryClipSize/*, m_iHeat*/;
	int m_iLastReloadBarValue, m_iLastReloadBarValue2;
	//ammo Flash anim is shared and can be wrong when tokens set the values
	bool			m_bAmmoForceNextUpdate;
};

#endif
