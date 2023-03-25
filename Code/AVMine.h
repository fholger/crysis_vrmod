/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  Anti-Vehicle mine implementation
-------------------------------------------------------------------------
History:
- 22:1:2007   14:39 : Created by Steve Humphreys

*************************************************************************/

#ifndef __AVMINE_H__
#define __AVMINE_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Projectile.h"


class CAVMine : public CProjectile
{
public:
	CAVMine();
	virtual ~CAVMine();

	virtual bool Init(IGameObject *pGameObject);

	virtual void ProcessEvent(SEntityEvent &event);
	virtual void HandleEvent(const SGameObjectEvent &event);
	virtual void Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale);
	virtual void SetParams(EntityId ownerId, EntityId hostId, EntityId weaponId, int fmId, int damage, int hitTypeId);

protected:
	int m_teamId;
	float m_triggerWeight;
	float m_currentWeight;
	bool m_frozen;
};


#endif // __AVMINE_H__