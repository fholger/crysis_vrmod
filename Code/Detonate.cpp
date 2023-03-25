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
#include "Game.h"
#include "Detonate.h"
#include "WeaponSystem.h"
#include "Item.h"
#include "Weapon.h"
#include "Actor.h"
#include "Projectile.h"
#include "Binocular.h"
#include "C4.h"

//------------------------------------------------------------------------
CDetonate::CDetonate()
{
}

//------------------------------------------------------------------------
CDetonate::~CDetonate()
{
}

//------------------------------------------------------------------------
struct CDetonate::ExplodeAction
{
	ExplodeAction(CDetonate *_detonate): pDetonate(_detonate) {};
	CDetonate *pDetonate;

	void execute(CItem *_this)
	{
		pDetonate->SelectLast();
	}
};

void CDetonate::Update(float frameTime, uint frameId)
{
	CSingle::Update(frameTime, frameId);

	if (m_detonationTimer>0.0f)
	{
		m_detonationTimer-=frameTime;

		if (m_detonationTimer<=0.0f)
		{
			m_detonationTimer=0.0f;

			bool detonated = Detonate();

			if (detonated && m_pWeapon->GetOwnerActor() && m_pWeapon->GetOwnerActor()->IsClient())
				m_pWeapon->GetScheduler()->TimerAction(uint(m_pWeapon->GetCurrentAnimationTime(CItem::eIGS_FirstPerson)*0.35f), CSchedulerAction<ExplodeAction>::Create(this), false);
		}
		else
			m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
}

//------------------------------------------------------------------------
void CDetonate::ResetParams(const struct IItemParamsNode *params)
{
	CSingle::ResetParams(params);
}

//------------------------------------------------------------------------
void CDetonate::PatchParams(const struct IItemParamsNode *patch)
{
	CSingle::PatchParams(patch);
}

//------------------------------------------------------------------------
void CDetonate::Activate(bool activate)
{
	CSingle::Activate(activate);
	
	m_detonationTimer=0.0f;
}

//------------------------------------------------------------------------
bool CDetonate::CanReload() const
{
	return false;
}

//------------------------------------------------------------------------
bool CDetonate::CanFire(bool considerAmmo) const
{
	return CSingle::CanFire(considerAmmo) && (m_detonationTimer<=0.0f);
}

//------------------------------------------------------------------------
void CDetonate::StartFire()
{
	if (CanFire(false))
	{
		m_pWeapon->RequireUpdate(eIUS_FireMode);
		m_detonationTimer = 0.1f;
		m_pWeapon->PlayAction(m_actions.fire.c_str());
	}
}

//------------------------------------------------------------------------
const char *CDetonate::GetCrosshair() const
{
	return "";
}

//------------------------------------------------------------------------
bool CDetonate::Detonate(bool net)
{
	if (m_pWeapon->IsServer())
	{
		CActor *pOwner=m_pWeapon->GetOwnerActor();
		if (!pOwner)
			return false;

		if (CWeapon *pWeapon=static_cast<CWeapon*>(pOwner->GetItemByClass(CItem::sC4Class)))
		{
			IFireMode* pFM = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
			//assert(pFM && "Detonator has not fire mode! Can not detonate C4");
			if(!pFM)
				return false;
			while(EntityId projectileId=pFM->RemoveProjectileId())
			{
				if (CProjectile *pProjectile=g_pGame->GetWeaponSystem()->GetProjectile(projectileId))
				{
					pProjectile->Explode(true, false);
					g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(pWeapon->GetOwner(), GameplayEvent(eGE_WeaponShot, pProjectile->GetEntity()->GetClass()->GetName(), 1, (void *)pWeapon->GetEntityId()));
				}
			}
		}
	}

	if (!net)
		m_pWeapon->RequestShoot(0, ZERO, ZERO, ZERO, ZERO, 1.0f, 0, 0, 0, false);

	return true;
}

//------------------------------------------------------------------------
void CDetonate::NetShoot(const Vec3 &hit, int ph)
{
	Detonate(true);
}

//------------------------------------------------------------------------
void CDetonate::SelectLast()
{
	CActor *pOwner=m_pWeapon->GetOwnerActor();
	if (!pOwner)
		return;

	EntityId lastItemId = pOwner->GetInventory()?pOwner->GetInventory()->GetLastItem():0;

	//Select C4, Fists or last item (check for binoculars)
	if (lastItemId)
	{
		CBinocular *pBinoculars = static_cast<CBinocular*>(pOwner->GetItemByClass(CItem::sBinocularsClass));
		CC4				 *pC4 = static_cast<CC4*>(pOwner->GetItemByClass(CItem::sC4Class));
		if ((pBinoculars) && (pBinoculars->GetEntityId() == lastItemId) && (!pC4 || pC4->OutOfAmmo(false)))
		{
				pOwner->SelectItemByName("Fists",false);
				return;
		}
		else if(pC4 && !pC4->OutOfAmmo(false))
		{
			if(!pC4->IsSelected())
				pOwner->SelectItemByName("C4",false);
			return;
		}
	}

	pOwner->SelectLastItem(false);
}
