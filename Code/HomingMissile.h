/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: HomingMissile

-------------------------------------------------------------------------
History:
- 12:10:2005   11:15 : Created by Márcio Martins

*************************************************************************/
#ifndef __HomingMissile_H__
#define __HomingMissile_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Rocket.h"


class CHomingMissile : public CRocket
{
public:
	CHomingMissile();
	virtual ~CHomingMissile();
  
	// CRocket	
	virtual bool Init(IGameObject *pGameObject);
  virtual void Update(SEntityUpdateContext &ctx, int updateSlot);
  virtual void Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale);
  virtual void SetDestination(const Vec3& pos)
	{
		m_destination = pos;
		if (!m_destination.IsEquivalent(pos, 0.01f))
			GetGameObject()->ChangedNetworkState(eEA_GameServerDynamic);
	};

  virtual void SetDestination(EntityId targetId)
	{
		m_targetId = targetId;
	}

	virtual void FullSerialize(TSerialize ser);
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	// ~CRocket


	struct DestinationParams
	{
		DestinationParams(): pt(0) {};
		void SerializeWith(TSerialize ser)
		{
			ser.Value("pt", pt, 'wrl3');
		}

		Vec3 pt;
	};

	DECLARE_SERVER_RMI_NOATTACH_FAST(SvRequestDestination, DestinationParams, eNRT_UnreliableOrdered);

protected:

	virtual void UpdateControlledMissile(float frameTime);   //LAW missiles
	virtual void UpdateCruiseMissile(float frameTime);       //Vehicle missiles

private:  
	void SerializeDestination( TSerialize ser );

  bool			m_controlled;
	bool      m_autoControlled;
	bool			m_cruise;
	Vec3			m_destination;
	float			m_maxTargetDistance;
	float			m_controlledTimer;
	float			m_lockedTimer;
	float     m_detonationRadius;
  
	EntityId	m_targetId;

  // params
  float m_accel;
  float m_turnSpeed;
  float m_maxSpeed;
  float m_cruiseAltitude;
  float m_alignAltitude;
  float m_descendDistance;
	float	m_lazyness;
	float m_initialDelay;

  // status
  bool m_isCruising;
  bool m_isDescending;
};


#endif // __HomingMissile_H__