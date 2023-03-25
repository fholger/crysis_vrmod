/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  C4 projectile specific stuff
-------------------------------------------------------------------------
History:
- 08:06:2007   : Created by Benito G.R.

*************************************************************************/

#ifndef __C4PROJECTILE_H__
#define __C4PROJECTILE_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Projectile.h"

#define MAX_STICKY_POINTS					3

class CC4Projectile : public CProjectile
{
public:
	CC4Projectile();
	virtual ~CC4Projectile();

	virtual void HandleEvent(const SGameObjectEvent &event);
	virtual void Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale);
	virtual void Explode(bool destroy, bool impact/* =false */, const Vec3 &pos/* =ZERO */, const Vec3 &normal/* =FORWARD_DIRECTION */, const Vec3 &vel/* =ZERO */, EntityId targetId/* =0  */);
	virtual void OnHit(const HitInfo& hit);

	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );

private:

	void Stick(EventPhysCollision *pCollision);
	void StickToStaticObject(EventPhysCollision *pCollision, IPhysicalEntity* pTarget);
	void StickToEntity(IEntity* pEntity, Matrix34 &localMatrix);
	bool StickToCharacter(bool stick, IEntity* pActor);

	void RefineCollision(EventPhysCollision *pCollision);
	
	virtual void SetParams(EntityId ownerId, EntityId hostId, EntityId weaponId, int fmId, int damage, int hitTypeId);

	int				m_teamId;
	bool			m_stuck;
	bool      m_notStick;
	bool      m_frozen;

	int       m_nConstraints;
	int       m_constraintIds[MAX_STICKY_POINTS];

	EntityId	m_parentEntity;
	Vec3			m_localChildPos;
	Quat			m_localChildRot;
};

#endif