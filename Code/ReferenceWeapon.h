// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
// Reference weapon for lighting tests
// John Newfield

#include "Weapon.h"
#pragma once

class CReferenceWeapon :
	public CWeapon
{
public:
	CReferenceWeapon(void);
	~CReferenceWeapon(void);
	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CWeapon::GetMemoryStatistics(s); }
};
