/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a damage behavior which burn stuff

-------------------------------------------------------------------------
History:
- 03:28:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEDAMAGEBEHAVIORBURN_H__
#define __VEHICLEDAMAGEBEHAVIORBURN_H__

class CVehicle;

class CVehicleDamageBehaviorBurn
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorBurn() {}
	virtual ~CVehicleDamageBehaviorBurn();

	virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table);
	virtual void Reset();
	virtual void Release() { delete this; }

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams);

	virtual void Serialize(TSerialize ser, unsigned aspects);
	virtual void Update(const float deltaTime);
  virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);

	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); }

protected:

  void Activate(bool activate);

	IVehicle* m_pVehicle;
	IVehicleHelper* m_pHelper;

  float m_damageRatioMin;
	float m_damage;
  float m_selfDamage;
	float m_interval;
	float m_radius;

	bool m_isActive;
	float m_timeCounter;

	EntityId m_shooterId;
  int m_timerId;
};

#endif
