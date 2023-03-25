// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "Projectile.h"
#pragma once

class CTagBullet :
	public CProjectile
{
public:
	CTagBullet(void);
	~CTagBullet(void);
	virtual void HandleEvent(const SGameObjectEvent &);
};
