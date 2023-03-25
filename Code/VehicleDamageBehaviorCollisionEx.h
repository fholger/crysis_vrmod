/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a damage behavior which improves collision damages 

-------------------------------------------------------------------------
History:
- 06:10:2007: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEDAMAGEBEHAVIORCOLLISIONEX_H__
#define __VEHICLEDAMAGEBEHAVIORCOLLISIONEX_H__

class CVehicle;

class CVehicleDamageBehaviorCollisionEx
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorCollisionEx() {}
	virtual ~CVehicleDamageBehaviorCollisionEx();

	virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table);
	virtual void Reset() {}
	virtual void Release() { delete this; }

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams) {}

	virtual void Serialize(TSerialize ser, unsigned aspects) {}
	virtual void Update(const float deltaTime) {}
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);

	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); }

protected:

	IVehicle* m_pVehicle;

	string m_componentName;
	float m_damages;
};

#endif
