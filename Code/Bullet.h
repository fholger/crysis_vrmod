/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Bullet

-------------------------------------------------------------------------
History:
- 12:10:2005   11:15 : Created by Márcio Martins

*************************************************************************/
#ifndef __BULLET_H__
#define __BULLET_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Projectile.h"

class CBullet : public CProjectile
{
public:
	CBullet();
	virtual ~CBullet();

	// CProjectile
	virtual void HandleEvent(const SGameObjectEvent &);
	// ~CProjectile

	//For underwater trails (Called only from WeaponSystem.cpp)
	static void SetWaterMaterialId();
	static int  GetWaterMaterialId() { return m_waterMaterialId; }

	static IEntityClass*	EntityClass;

private:
	
	static int  m_waterMaterialId;

};


#endif // __BULLET_H__