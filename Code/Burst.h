/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Burst Fire Mode Implementation

-------------------------------------------------------------------------
History:
- 26:10:2005   12:15 : Created by Márcio Martins

*************************************************************************/
#ifndef __BURST_H__
#define __BURST_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Single.h"


class CBurst : public CSingle
{
protected:
	typedef struct SBurstParams
	{
		SBurstParams() { Reset(); };
		void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
		{
			CItemParamReader reader(params);
			ResetValue(nshots,			3);
			ResetValue(rate,				32);
			ResetValue(noSound,		false);
		}

		short nshots;
		short	rate;
		bool  noSound;

	} SBurstParams;
public:
	CBurst();
	virtual ~CBurst();

	// CSingle
	virtual void Update(float frameTime, uint frameId);
	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CSingle::GetMemoryStatistics(s); }

	virtual void ResetParams(const struct IItemParamsNode *params);
	virtual void PatchParams(const struct IItemParamsNode *patch);

	virtual void Activate(bool activate);
	virtual bool CanFire(bool considerAmmo /* = true */) const;

	virtual void StartFire();
	virtual void StopFire();
	virtual const char *GetType() const;
	// ~CSingle

protected:
	int		m_burst_shot;
	bool	m_bursting;

	float	m_next_burst_dt;
	float	m_next_burst;

	bool  m_canShoot;

	SBurstParams m_burstparams;
};


#endif