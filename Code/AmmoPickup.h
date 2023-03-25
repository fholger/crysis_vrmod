/*************************************************************************
	Crytek Source File.
	Copyright (C), Crytek Studios, 2001-2004.
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: AmmoPickup Implementation

	-------------------------------------------------------------------------
	History:
	- 9:2:2006   17:09 : Created by Márcio Martins

*************************************************************************/
#ifndef __AMMOPICKUP_H__
#define __AMMOPICKUP_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IItemSystem.h>
#include "Weapon.h"


class CAmmoPickup :
	public CWeapon
{
public:
	virtual void PostInit( IGameObject * pGameObject );
	virtual bool CanUse(EntityId userId) const;
	virtual bool CanPickUp(EntityId pickerId) const;

	virtual void SerializeSpawnInfo( TSerialize ser );
	virtual ISerializableInfoPtr GetSpawnInfo();

	virtual void PickUp(EntityId pickerId, bool sound, bool select, bool keepHistory);
	virtual bool CheckAmmoRestrictions(EntityId pickerId);
	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); 
																										s->Add(m_pickup_sound);
																										s->Add(m_modelName);
																										CWeapon::GetMemoryStatistics(s); }

protected:
	virtual bool ReadItemParams(const IItemParamsNode *root);

private:
	
	//Special case for grenades (might need to switch firemode)
	void    ShouldSwitchGrenade(IEntityClass* pClass);
	void    OnIncendiaryAmmoPickedUp(IEntityClass *pClass, int count);

	ItemString	m_modelName;
	ItemString	m_ammoName;
	int					m_ammoCount;
	ItemString  m_pickup_sound;
};


#endif // __AMMOPICKUP_H__