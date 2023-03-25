/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a damage behavior which gives camera shake to the 
player

-------------------------------------------------------------------------
History:
- 31:07:2007: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEDAMAGEBEHAVIORCAMERASHAKE_H__
#define __VEHICLEDAMAGEBEHAVIORCAMERASHAKE_H__

class CVehicle;

class CVehicleDamageBehaviorCameraShake
	: public IVehicleDamageBehavior
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehicleDamageBehaviorCameraShake();
	virtual ~CVehicleDamageBehaviorCameraShake();

	virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table);
	virtual void Reset();
	virtual void Release() { delete this; }

	virtual void OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams);

	virtual void Serialize(TSerialize ser, unsigned aspects) {}
	virtual void Update(const float deltaTime) {}
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);

	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); }

protected:

	void ShakeClient(float angle, float shift, float duration, float frequency);

	IVehicle* m_pVehicle;
	float m_damageMult;
};

#endif
