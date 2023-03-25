/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 29:05:2007 Benito G.R.

*************************************************************************/
#include "StdAfx.h"
#include "C4Detonator.h"
#include "Game.h"
#include "GameActions.h"
#include "Actor.h"

TActionHandler<CC4Detonator>	CC4Detonator::s_actionHandler;

CC4Detonator::CC4Detonator()
{
	if(s_actionHandler.GetNumHandlers() == 0)
	{
#define ADD_HANDLER(action, func) s_actionHandler.AddHandler(actions.action, &CC4Detonator::func)
		const CGameActions& actions = g_pGame->Actions();

		ADD_HANDLER(zoom,OnActionSelectC4);
		ADD_HANDLER(xi_zoom,OnActionSelectC4);
#undef ADD_HANDLER
	}
}

//----------------------------------------------
void CC4Detonator::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(!s_actionHandler.Dispatch(this,actorId,actionId,activationMode,value))
		CWeapon::OnAction(actorId, actionId, activationMode, value);
}

//----------------------------------------------
void CC4Detonator::SelectC4()
{
	if (CActor *pOwner=GetOwnerActor())
	{
		EntityId c4Id = pOwner->GetInventory()->GetItemByClass(CItem::sC4Class);
		if (c4Id)
		{
			//Do not reselect C4 is there's no ammo
			IItem* pItem = m_pItemSystem->GetItem(c4Id);
			CWeapon *pWep = pItem?static_cast<CWeapon*>(pItem->GetIWeapon()):NULL;
			if(pWep && pWep->OutOfAmmo(false))
				return;

			pOwner->SelectItemByName("C4", false);
		}
	}
}

//----------------------------------------------
bool CC4Detonator::OnActionSelectC4(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(activationMode == eAAM_OnPress)
		SelectC4();

	return true;
}
