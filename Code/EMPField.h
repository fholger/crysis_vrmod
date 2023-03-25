/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  Anti-Suit field
-------------------------------------------------------------------------
History:
- 10:04:2007   14:39 : Created by Marco Koegler
- 21:08:2007   Benito G.R. - Not used (not registered in WeaponSytem)

*************************************************************************/

#ifndef __EMPFIELD_H__
#define __EMPFIELD_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Projectile.h"

class CPlayer;

class CEMPField : public CProjectile
{
public:
	CEMPField();
	virtual ~CEMPField();

	virtual bool Init(IGameObject *pGameObject);
	virtual void ProcessEvent(SEntityEvent &event);
	virtual void Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale);

protected:
	void OnEMPActivate();
	void OnPlayerEnter(CPlayer* pPlayer);
	void OnPlayerLeave(CPlayer* pPlayer);
	void ReleaseAll();
	void RemoveEntity(EntityId id);

protected:
	typedef std::vector<EntityId>	TEntities;
	TEntities	m_players;
	float	m_activationTime;
	float	m_radius;
	bool	m_charging;
	int		m_empEffectId;
};


#endif // __EMPFIELD_H__