/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Binocular Implementation

-------------------------------------------------------------------------
History:
- 18:12:2005   14:00 : Created by Márcio Martins

*************************************************************************/
#ifndef __BINOCULAR_H__
#define __BINOCULAR_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IItemSystem.h>
#include "Weapon.h"


class CBinocular :	public CWeapon
{
public:

	CBinocular();

	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	
	virtual void Select(bool select);
	virtual void UpdateFPView(float frameTime);

	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CWeapon::GetMemoryStatistics(s); }

private:

	~CBinocular();

	static TActionHandler<CBinocular> s_actionHandler;

	bool OnActionZoomIn(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionZoomOut(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionZoom(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value);
};

#endif // __BINOCULAR_H__