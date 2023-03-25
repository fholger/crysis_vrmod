/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 2:3:2005   16:06 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "C4.h"
#include "Plant.h"

#include "Game.h"
#include "Actor.h"
#include "WeaponSystem.h"
#include "GameActions.h"


TActionHandler<CC4>	CC4::s_actionHandler;
//------------------------------------------------------------------------
CC4::CC4()
{
	if(s_actionHandler.GetNumHandlers() == 0)
	{
#define ADD_HANDLER(action, func) s_actionHandler.AddHandler(actions.action, &CC4::func)
		const CGameActions& actions = g_pGame->Actions();

		ADD_HANDLER(zoom,OnActionSelectDetonator);
		ADD_HANDLER(xi_zoom,OnActionSelectDetonator);
#undef ADD_HANDLER
	}
}


//------------------------------------------------------------------------
CC4::~CC4()
{
};


//------------------------------------------------------------------------
void CC4::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(!s_actionHandler.Dispatch(this,actorId,actionId,activationMode,value))
		CWeapon::OnAction(actorId, actionId, activationMode, value);
}

//-----------------------------------------------------------------------
bool CC4::OnActionSelectDetonator(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(activationMode == eAAM_OnPress)
	{
		bool projectile = false;

		IFireMode* pFM = GetFireMode(GetCurrentFireMode());
		if(pFM)
		{
			EntityId projectileId = pFM->GetProjectileId();
			if(projectileId && g_pGame->GetWeaponSystem()->GetProjectile(projectileId))
				projectile = true;
		}
		//If there is some projectile we can switch
		if(projectile)
			SelectDetonator();
	}

	return true;
}
//------------------------------------------------------------------------
void CC4::PickUp(EntityId pickerId, bool sound, bool select/* =true */, bool keepHistory/* =true */)
{
	CActor *pActor=GetActor(pickerId);
	if (pActor)
	{
		IInventory *pInventory=GetActorInventory(pActor);
		if (pInventory)
		{
			if (!pInventory->GetItemByClass(CItem::sDetonatorClass))
			{
				if (IsServer())
					m_pItemSystem->GiveItem(pActor, "Detonator", false, false, false);
			}
		}
	}

	CWeapon::PickUp(pickerId, sound, select, keepHistory);
}

//------------------------------------------------------------------------
bool CC4::CanSelect() const
{
	bool canSelect = (CWeapon::CanSelect() && !OutOfAmmo(false));

	//Check for remaining projectiles to detonate
	if(!canSelect)
	{
		CActor *pOwner = GetOwnerActor();
		if(!pOwner)
			return false;

		EntityId detonatorId = pOwner->GetInventory()?pOwner->GetInventory()->GetItemByClass(CItem::sDetonatorClass):0;
		
		//Do not re-select detonator again
		if(detonatorId && (detonatorId==pOwner->GetCurrentItemId()))
			return false;

		IFireMode* pFM = GetFireMode(GetCurrentFireMode());
		if(pFM)
		{
			//CC4::Select will select the detonator in this case
			EntityId projectileId = pFM->GetProjectileId();
			if(projectileId && g_pGame->GetWeaponSystem()->GetProjectile(projectileId))
				return true;

		}
	}

	return canSelect;
};

//------------------------------------------------------------------------
void CC4::Select(bool select)
{
	if (select)
	{
		bool outOfAmmo = OutOfAmmo(false);
		bool projectile = false;

		IFireMode* pFM = GetFireMode(GetCurrentFireMode());
		if(pFM)
		{
			EntityId projectileId = pFM->GetProjectileId();
			if(projectileId && g_pGame->GetWeaponSystem()->GetProjectile(projectileId))
				projectile = true;
		}

		if(outOfAmmo && projectile)
		{
				SelectDetonator();
				return;
		}
		else if(outOfAmmo)
		{
			Select(false);
			CActor *pOwner=GetOwnerActor();
			if (!pOwner)
				return;

			EntityId fistsId = pOwner->GetInventory()?pOwner->GetInventory()->GetItemByClass(CItem::sFistsClass):0;
			if (fistsId)
				pOwner->SelectItem(fistsId,true);

			return;
		}
	}

	CWeapon::Select(select);
}

//------------------------------------------------------------------------
void CC4::Drop(float impulseScale, bool selectNext, bool byDeath)
{
	EntityId fistsId = 0;
	
	CActor *pOwner = GetOwnerActor();
	if(pOwner)
		fistsId = pOwner->GetInventory()?pOwner->GetInventory()->GetItemByClass(CItem::sFistsClass):0;

	CItem::Drop(impulseScale,selectNext,byDeath);

	if (fistsId)
		pOwner->SelectItem(fistsId,true);

}

//=======================================================================
void CC4::SelectDetonator()
{
	if (CActor *pOwner=GetOwnerActor())
	{
		EntityId detonatorId = pOwner->GetInventory()->GetItemByClass(CItem::sDetonatorClass);
		if (detonatorId)
			pOwner->SelectItemByName("Detonator", false);
	}
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CC4, ClSetProjectileId)
{
	IFireMode *pFireMode=GetFireMode(params.fmId);
	if (pFireMode)
		pFireMode->SetProjectileId(params.id);
	else
		return false;

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CC4, SvRequestTime)
{
	IFireMode *pFireMode=GetFireMode(params.fmId);
	if (pFireMode && !stricmp(pFireMode->GetType(), "Plant"))
	{
		CPlant *pPlant=static_cast<CPlant *>(pFireMode);
		pPlant->SetTime(params.time);
	}
	else
		return false;

	return true;
}