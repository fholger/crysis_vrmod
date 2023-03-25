/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  Claymore mine implementation
-------------------------------------------------------------------------
History:
- 07:2:2007   12:34 : Created by Steve Humphreys

*************************************************************************/

#ifndef __CLAYMORE_H__
#define __CLAYMORE_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Projectile.h"
		
static const int EXPLOSIVE_REMOVAL_TIME	= 30000;	// remove claymores 30s after player dies


class CClaymore : public CProjectile
{
public:
	CClaymore();
	virtual ~CClaymore();

	virtual bool Init(IGameObject *pGameObject);

	virtual void ProcessEvent(SEntityEvent &event);
	virtual void HandleEvent(const SGameObjectEvent &event);
	virtual void Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale);
	virtual void Update(SEntityUpdateContext &ctx, int updateSlot);
	virtual void Explode(bool destroy, bool impact=false, const Vec3 &pos=ZERO, const Vec3 &normal=FORWARD_DIRECTION , const Vec3 &vel=ZERO, EntityId targetId=0);

	Vec3 GetTriggerDirection() {return m_triggerDirection; }

	virtual void SetParams(EntityId ownerId, EntityId hostId, EntityId weaponId, int fmId, int damage, int hitTypeId);

protected:
	int	m_teamId;							// which team placed this claymore
	Vec3 m_triggerDirection;	// direction of detector
	float m_triggerRadius;		// radius of cylinder segment used to trigger explosion
	float m_triggerAngle;			// angular dimension of cylinder segment
	float m_timeToArm;				// time until trigger becomes active
	bool m_armed;							// is this claymore armed
	std::list<EntityId>	m_targetList;	// entities within our trigger area (we need to keep
																		// a record and check their angle etc while inside our trigger).
	bool m_frozen;
};


#endif // __CLAYMORE_H__