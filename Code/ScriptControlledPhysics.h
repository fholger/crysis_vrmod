/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Script Interface for controlling physics.
						 Neccessary when changing physics positions and velocities
						 while keeping visual/physics in sync...

-------------------------------------------------------------------------
History:
- 26:7:2007   17:00 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTCONTROLLEDPHYSICS_H__
#define __SCRIPTCONTROLLEDPHYSICS_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "IGameObject.h"


class CScriptControlledPhysics: public CGameObjectExtensionHelper<CScriptControlledPhysics, IGameObjectExtension>,
	CScriptableBase
{
public:
	CScriptControlledPhysics();
	virtual ~CScriptControlledPhysics();

	// IGameObjectExtension
	virtual bool Init( IGameObject * pGameObject );
	using CScriptableBase::Init;

	virtual void InitClient(int channelId) {};
	virtual void PostInit( IGameObject * pGameObject );
	virtual void PostInitClient(int channelId) {};
	virtual void Release();
	virtual void FullSerialize( TSerialize ser ) {};
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags ) { return true; }
	virtual void PostSerialize() {}
	virtual void SerializeSpawnInfo( TSerialize ser ) {}
	virtual ISerializableInfoPtr GetSpawnInfo() {return 0;}
	virtual void Update( SEntityUpdateContext& ctx, int slot ) {};
	virtual void HandleEvent( const SGameObjectEvent& );
	virtual void ProcessEvent(SEntityEvent& ) {}
	virtual void SetChannelId(uint16 id) {};
	virtual void SetAuthority(bool auth) {}
	virtual void PostUpdate(float frameTime) { assert(false); }
	virtual void PostRemoteSpawn() {};
	virtual void GetMemoryStatistics(ICrySizer * s);
	//~IGameObjectExtension

	int GetSpeed(IFunctionHandler *pH);
	int GetAcceleration(IFunctionHandler *pH);

	int GetAngularSpeed(IFunctionHandler *pH);
	int GetAngularAcceleration(IFunctionHandler *pH);

	int MoveTo(IFunctionHandler *pH, Vec3 point, float initialSpeed, float speed, float acceleration, float stopTime);
	int RotateTo(IFunctionHandler *pH, Vec3 dir, float roll, float initialSpeed, float speed, float acceleration, float stopSpeed);
	int RotateToAngles(IFunctionHandler *pH, Vec3 angles, float initialSpeed, float speed, float acceleration, float stopSpeed);

	void OnPostStep(EventPhysPostStep *pPostStep);
private:
	void RegisterGlobals();
	void RegisterMethods();

	bool m_moving;
	Vec3  m_moveTarget;
	float m_speed;
	float m_maxSpeed;
	float m_acceleration;
	float m_stopTime;

	bool m_rotating;
	Quat m_rotationTarget;
	float m_rotationSpeed;
	float m_rotationMaxSpeed;
	float m_rotationAcceleration;
	float m_rotationStopTime;
};


#endif //__SCRIPTCONTROLLEDPHYSICS_H__
