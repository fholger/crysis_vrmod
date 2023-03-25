/************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: C4Detonator Implementation

-------------------------------------------------------------------------
History:
- 29:05:2007 Benito G.R.

*************************************************************************/
#ifndef __C4DETONATOR_H__
#define __C4DETONATOR_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IItemSystem.h>
#include "Weapon.h"


class CC4Detonator :
	public CWeapon
{

public:
	CC4Detonator();

	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);

	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CWeapon::GetMemoryStatistics(s); }

private:

	static TActionHandler<CC4Detonator> s_actionHandler;

	bool OnActionSelectC4(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	void SelectC4();

};

#endif // __C4DETONATOR_H__