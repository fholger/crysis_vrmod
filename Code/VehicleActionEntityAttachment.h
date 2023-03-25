/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a vehicle action for Entity Attachment

-------------------------------------------------------------------------
History:
- 07:12:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEACTIONENTITYATTACHMENT_H__
#define __VEHICLEACTIONENTITYATTACHMENT_H__

class CVehicleActionEntityAttachment
	: public IVehicleAction
{
	IMPLEMENT_VEHICLEOBJECT;

public:

	CVehicleActionEntityAttachment();
	virtual ~CVehicleActionEntityAttachment();

	// IVehicleAction
	virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table);
	virtual void Reset();
	virtual void Release() { delete this; }

	virtual int OnEvent(int eventType, SVehicleEventParams& eventParams);
	void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); }
	// ~IVehicleAction

	// IVehicleObject
	virtual void Serialize(TSerialize ser, unsigned aspects) {}
	virtual void Update(const float deltaTime);
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params){}
	// ~IVehicleObject

	bool DetachEntity();
	bool IsEntityAttached() { return m_isAttached; }

	EntityId GetAttachmentId() { return m_entityId; }

protected:

	void SpawnEntity();

	IVehicle* m_pVehicle;

	string m_entityClassName;
	IVehicleHelper* m_pHelper;

	EntityId m_entityId;
	bool m_isAttached;

	float m_timer;
};

#endif
