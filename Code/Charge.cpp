/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 11:9:2005   15:00 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Charge.h"
#include "Item.h"
#include "Weapon.h"
#include "Projectile.h"



//------------------------------------------------------------------------
CCharge::CCharge()
: m_charged(0)
, m_chId(0)
, m_chlightId(0)
, m_chTimer(0.0f)
{
}

//------------------------------------------------------------------------
CCharge::~CCharge()
{
}

//----------------------------------------"--------------------------------
void CCharge::Update(float frameTime, uint frameId)
{
	if (m_charging)
	{
		if (m_chargeTimer>0.0f)
		{
			m_chargeTimer -= frameTime;
			if (m_chargeTimer<=0.0f)
			{
				m_charged++;
				if (m_charged >= m_chargeparams.max_charges)
				{
					m_charging = false;
					m_charged = m_chargeparams.max_charges;
					if (!m_chargeparams.shoot_on_stop)
					{
						if (m_firing)
							ChargedShoot();
					}
				}
			}
		}

		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
	else
	{
		if (!m_chargeparams.shoot_on_stop)
		{
			CAutomatic::Update(frameTime, frameId);
		}
		else
		{
			CSingle::Update(frameTime, frameId);
		}
	}

	// update spinup effect
	if (m_chTimer>0.0f)
	{
		m_chTimer -= frameTime;
		if (m_chTimer <= 0.0f)
		{
			m_chTimer = 0.0f;
			if (m_chId)
				ChargeEffect(false);
		}

		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
}

//------------------------------------------------------------------------
void CCharge::ResetParams(const struct IItemParamsNode *params)
{
	CAutomatic::ResetParams(params);

	const IItemParamsNode *charge = params?params->GetChild("charge"):0;
	const IItemParamsNode *actions = params?params->GetChild("actions"):0;
	const IItemParamsNode *effect = params?params->GetChild("effect"):0;

	m_chargeparams.Reset(charge);
	m_chargeactions.Reset(actions);
	m_chargeeffect.Reset(effect);
}

//------------------------------------------------------------------------
void CCharge::PatchParams(const struct IItemParamsNode *patch)
{
	CAutomatic::PatchParams(patch);

	const IItemParamsNode *charge = patch->GetChild("charge");
	const IItemParamsNode *actions = patch->GetChild("actions");
	const IItemParamsNode *effect = patch->GetChild("effect");

	m_chargeparams.Reset(charge, false);
	m_chargeactions.Reset(actions, false);
	m_chargeeffect.Reset(effect, false);
}

//------------------------------------------------------------------------
void CCharge::Activate(bool activate)
{
	CAutomatic::Activate(activate);

	ChargeEffect(0);

	m_charged=0;
	m_charging=false;
	m_chargeTimer=0.0;
}

//------------------------------------------------------------------------
void CCharge::StopFire()
{
	if (m_chargeparams.shoot_on_stop)
	{
		if (m_charged > 0)
		{
			ChargedShoot();
		}
		m_pWeapon->PlayAction(m_chargeactions.uncharge.c_str());
		m_charged = 0;
		m_charging = false;
		m_chargeTimer = 0.0f;
	}

	CAutomatic::StopFire();
}

//------------------------------------------------------------------------
bool CCharge::Shoot(bool resetAnimation, bool autoreload /* =true */, bool noSound /* =false */)
{
	m_autoreload = autoreload;

	if (!m_charged)
	{
		m_charging = true;
		m_chargeTimer = m_chargeparams.time;
		m_pWeapon->PlayAction(m_chargeactions.charge.c_str(),  0, false, CItem::eIPAF_Default|CItem::eIPAF_RepeatLastFrame);

		ChargeEffect(true);
	}
	else if (!m_charging && m_firing)
		ChargedShoot();

	m_pWeapon->RequireUpdate(eIUS_FireMode);

	return true;
}

//------------------------------------------------------------------------
void CCharge::ChargedShoot()
{
	CAutomatic::Shoot(true, m_autoreload);

	m_charged=0;

	if(m_chargeparams.reset_spinup)
		StopFire();
}

//------------------------------------------------------------------------
void CCharge::ChargeEffect(bool attach)
{
	m_pWeapon->AttachEffect(0, m_chId, false);
	m_pWeapon->AttachLight(0, m_chlightId, false);
	m_chId=0;
	m_chlightId=0;

	if (attach)
	{
		int slot = m_pWeapon->GetStats().fp ? CItem::eIGS_FirstPerson : CItem::eIGS_ThirdPerson;
		int id = m_pWeapon->GetStats().fp ? 0 : 1;

		m_chId = m_pWeapon->AttachEffect(slot, 0, true, m_chargeeffect.effect[id].c_str(), 
			m_chargeeffect.helper[id].c_str(), Vec3(0,0,0), Vec3(0,1,0), 1.0f, false);

		m_chlightId = m_pWeapon->AttachLight(slot, 0, true, m_chargeeffect.light_radius[id], m_chargeeffect.light_color[id], 1.0f, 0, 0,
			m_chargeeffect.light_helper[id].c_str());

		m_chTimer = (uint)(m_chargeeffect.time[id]);

	}
}