/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a vehicle action for an automatic door

-------------------------------------------------------------------------
History:
- 02:06:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEACTIONAUTOMATICDOOR_H__
#define __VEHICLEACTIONAUTOMATICDOOR_H__

class CVehicleActionAutomaticDoor
: public IVehicleAction
{
	IMPLEMENT_VEHICLEOBJECT;

public:

	CVehicleActionAutomaticDoor();
	virtual ~CVehicleActionAutomaticDoor();

	// IVehicleAction
	virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table);
	virtual void Reset();
	virtual void Release() { delete this; }

	virtual int OnEvent(int eventType, SVehicleEventParams& eventParams);
	void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); }
	// ~IVehicleAction

	// IVehicleObject
	virtual void Serialize(TSerialize ser, unsigned aspects);
	virtual void Update(const float deltaTime);
  virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);
	// ~IVehicleObject

	void OpenDoor(bool value);
	void BlockDoor(bool value);

	bool IsOpened();

protected:

	IVehicle* m_pVehicle;
	
	IVehicleAnimation* m_pDoorAnim;
	TVehicleAnimStateId m_doorOpenedStateId;
	TVehicleAnimStateId m_doorClosedStateId;

	float m_timeMax;
	float m_eventSamplingTime;
	bool m_isTouchingGroundBase;

	float m_timeOnTheGround;
	float m_timeInTheAir;
	bool m_isTouchingGround;
	bool m_isOpenRequested;
	bool m_isBlocked;
	bool m_isDisabled;

	float m_animGoal;
	float m_animTime;

	friend class CScriptBind_AutomaticDoor;
};

#endif
