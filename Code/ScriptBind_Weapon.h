/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Script Binding for Weapon
  
 -------------------------------------------------------------------------
  History:
  - 25:11:2004   11:30 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTBIND_WEAPON_H__
#define __SCRIPTBIND_WEAPON_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IScriptSystem.h>
#include <ScriptHelpers.h>


struct IItemSystem;
struct IGameFramework;
class CItem;
class CWeapon;


class CScriptBind_Weapon :
	public CScriptableBase
{
public:
	CScriptBind_Weapon(ISystem *pSystem, IGameFramework *pGameFramework);
	virtual ~CScriptBind_Weapon();

	void AttachTo(CWeapon *pWeapon);

	int SetAmmoCount(IFunctionHandler *pH);
	int GetAmmoCount(IFunctionHandler *pH);
	int GetClipSize(IFunctionHandler *pH);

	int IsZoomed(IFunctionHandler *pH);
	int IsZooming(IFunctionHandler *pH);
	int GetDamage(IFunctionHandler *pH);
	int GetAmmoType(IFunctionHandler *pH);

	int GetRecoil(IFunctionHandler *pH);
	int GetSpread(IFunctionHandler *pH);
	int GetCrosshair(IFunctionHandler *pH);
	int GetCrosshairOpacity(IFunctionHandler *pH);
	int GetCrosshairVisibility(IFunctionHandler *pH);
	int ModifyCommit(IFunctionHandler *pH);
	int SupportsAccessory(IFunctionHandler *pH, const char *accessoryName);
	int GetAccessory(IFunctionHandler *pH, const char *accessoryName);
	int AttachAccessoryPlaceHolder(IFunctionHandler *pH, SmartScriptTable accessory, bool attach);
	int GetAttachmentHelperPos(IFunctionHandler *pH, const char *helperName);
	int GetShooter(IFunctionHandler *pH);
	int ScheduleAttach(IFunctionHandler *pH, const char *className, bool attach);
	int AttachAccessory(IFunctionHandler *pH, const char *className, bool attach, bool force);
	int SwitchAccessory(IFunctionHandler *pH, const char *className);

	int IsFiring(IFunctionHandler *pH);


	int SetCurrentFireMode(IFunctionHandler *pH, const char *name);
	int SetCurrentZoomMode(IFunctionHandler *pH, const char *name);

	int AutoShoot(IFunctionHandler *pH, int nshots, bool autoReload);

	int Reload(IFunctionHandler *pH);

	int ActivateLamLaser(IFunctionHandler *pH, bool activate);
	int ActivateLamLight(IFunctionHandler *pH, bool activate);

private:
	void RegisterGlobals();
	void RegisterMethods();

	CItem *GetItem(IFunctionHandler *pH);
	CWeapon *GetWeapon(IFunctionHandler *pH);

	ISystem						*m_pSystem;
	IScriptSystem			*m_pSS;
	IGameFramework		*m_pGameFW;
};


#endif //__SCRIPTBIND_ITEM_H__