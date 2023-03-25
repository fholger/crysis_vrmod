/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Rock

-------------------------------------------------------------------------
History:
- 5:5:2006   15:24 : Created by Márcio Martins

*************************************************************************/
#ifndef __ROCK_H__
#define __ROCK_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Projectile.h"
#include "VectorSet.h"

#define MAX_SPAWNED_ROCKS	10

class CRock : public CProjectile
{
public:
	CRock();
	virtual ~CRock();

	// CProjectile
	virtual void HandleEvent(const SGameObjectEvent &);
	// ~CProjectile

private:
	static VectorSet<CRock*> s_rocks;
};


#endif // __ROCK_H__