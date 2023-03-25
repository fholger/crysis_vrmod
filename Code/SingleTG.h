/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Some special stuff for the TACGun in fleet

-------------------------------------------------------------------------
History:
- 18:07:2007  12:15 : Created by Benito Gangoso Rodriguez

*************************************************************************/
#ifndef __SINGLETG_H__
#define __SINGLETG_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Single.h"

#define SINGLETG_MAX_TARGETS 4

class CSingleTG : public CSingle
{
public:
	CSingleTG();
	virtual ~CSingleTG();

	// CSingle
	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CSingle::GetMemoryStatistics(s); }

	virtual bool CanFire(bool considerAmmo /* = true */) const;
	virtual void UpdateFPView(float frameTime);

	virtual void StartFire();

	virtual void UpdateAutoAim(float frameTime);
	virtual bool IsValidAutoAimTarget(IEntity* pEntity, int partId = 0);

	virtual void StartLocking(EntityId targetId, int partId = 0);
	virtual void Lock(EntityId targetId, int partId = 0);
	virtual void Unlock();

	virtual void Serialize(TSerialize ser);
	// ~CSingle

private:
	void UpdateTargets();

	EntityId    m_targetIds[SINGLETG_MAX_TARGETS];
	EntityId		m_idSerializeTarget;
	float				m_fSerializeProgress;
	int					m_iSerializeIgnoreUpdate;
};


#endif