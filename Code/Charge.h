/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Single-shot Fire Mode Implementation

-------------------------------------------------------------------------
History:
- 11:9:2004   15:00 : Created by Márcio Martins

*************************************************************************/
#ifndef __CHARGE_H__
#define __CHARGE_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Automatic.h"


class CCharge :
	public CAutomatic
{
	typedef struct SChargeParams
	{
		SChargeParams() { Reset(); };
		void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
		{
			CItemParamReader reader(params);
			ResetValue(time,						0.5f);
			ResetValue(max_charges,			1);
			ResetValue(shoot_on_stop,		false);
			ResetValue(reset_spinup,		false);
		};

		float		time;
		int			max_charges;
		bool		shoot_on_stop;
		bool		reset_spinup;
	} SChargeParams;

	typedef struct SChargeActions
	{
		SChargeActions() { Reset(); };
		void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
		{
			CItemParamReader reader(params);
			ResetValue(charge,	"charge");
			ResetValue(uncharge,"uncharge");
		};

		ItemString charge;
		ItemString uncharge;

	} SChargeActions;

public:
	CCharge();
	virtual ~CCharge();

	virtual void Update(float frameTime, uint frameId);
	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CSingle::GetMemoryStatistics(s); }

	virtual void ResetParams(const struct IItemParamsNode *params);
	virtual void PatchParams(const struct IItemParamsNode *patch);

	virtual void Activate(bool activate);

	virtual void StopFire();

	virtual bool Shoot(bool resetAnimation, bool autoreload, bool noSound /* =false */);

	virtual void ChargeEffect(bool attach);
	virtual void ChargedShoot();

protected:

	SEffectParams	m_chargeeffect;
	int						m_charged;
	bool					m_charging;
	float					m_chargeTimer;
	bool					m_autoreload;

	SChargeParams		m_chargeparams;
	SChargeActions	m_chargeactions;

	uint					m_chId;
	uint					m_chlightId;
	float					m_chTimer;
};


#endif //__CHARGE_H__