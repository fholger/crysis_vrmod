/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a damage behavior which create an explosion

-------------------------------------------------------------------------
History:
- 03:28:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEDAMAGEBEHAVIOREXPLOSION_H__
#define __VEHICLEDAMAGEBEHAVIOREXPLOSION_H__

class CVehicle;

class CVehicleDamageBehaviorExplosion
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorExplosion() {}
	virtual ~CVehicleDamageBehaviorExplosion() {}

	virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table);
	virtual void Reset();
	virtual void Release() { delete this; }

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams);

	virtual void Serialize(TSerialize ser, unsigned aspects);
	virtual void Update(const float deltaTime) {}
  virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params){}

	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); }

protected:

	IVehicle* m_pVehicle;

	float m_damage;
	float m_minRadius;
	float m_radius;
	float m_minPhysRadius;
	float m_physRadius;
	float m_pressure;
	IVehicleHelper* m_pHelper;

  bool m_exploded;
};

#endif
