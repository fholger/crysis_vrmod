/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 18:10:2005   14:14 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Rocket.h"
#include "Game.h"
#include "Bullet.h"


//------------------------------------------------------------------------
CRocket::CRocket()
: m_launchLoc(0,0,0),
	m_safeExplosion(0),
	m_skipWater(false)
{
}

//------------------------------------------------------------------------
CRocket::~CRocket()
{
	//LAW might be dropped automatically (to be sure that works in MP too)
	if(CWeapon* pWeapon = GetWeapon())
	{
		if(pWeapon->IsAutoDroppable())
			pWeapon->AutoDrop();
	}
}

//------------------------------------------------------------------------
bool CRocket::Init(IGameObject *pGameObject)
{
	if (CProjectile::Init(pGameObject))
	{
		m_skipWater = GetParam("ignoreWater", m_skipWater);
		return true;
	}

	return false;
}

//------------------------------------------------------------------------
void CRocket::HandleEvent(const SGameObjectEvent &event)
{
	if (m_destroying)
		return;

	CProjectile::HandleEvent(event);

	if (!gEnv->bServer || IsDemoPlayback())
		return;

	if (event.event == eGFE_OnCollision)
	{		
		EventPhysCollision *pCollision = (EventPhysCollision *)event.ptr;
		if (m_safeExplosion>0.0f)
		{
			float dp2=(m_launchLoc-GetEntity()->GetWorldPos()).len2();
			if (dp2<=m_safeExplosion*m_safeExplosion)
				return;
		}

		if(pCollision && pCollision->pEntity[0]->GetType()==PE_PARTICLE)
		{
			float bouncy, friction;
			uint	pierceabilityMat;
			gEnv->pPhysicalWorld->GetSurfaceParameters(pCollision->idmat[1], bouncy, friction, pierceabilityMat);

			pe_params_particle params;
			
			if(pCollision->pEntity[0]->GetParams(&params)==0)
				SetDefaultParticleParams(&params);
			
			if((params.velocity>1.0f) && (pCollision->idmat[1] != CBullet::GetWaterMaterialId())
				&& (!pCollision->pEntity[1] || (pCollision->pEntity[1]->GetType() != PE_LIVING && pCollision->pEntity[1]->GetType() != PE_ARTICULATED)))
			{
				if(pierceabilityMat>params.iPierceability)
					return;
			}
			else if((pCollision->idmat[1] == CBullet::GetWaterMaterialId()) && m_skipWater)
				return;

		}

    IEntity* pTarget = pCollision->iForeignData[1]==PHYS_FOREIGN_ID_ENTITY ? (IEntity*)pCollision->pForeignData[1] : 0;

    Explode(true, true, pCollision->pt, pCollision->n, pCollision->vloc[0], pTarget?pTarget->GetId():0);
	}
}

//------------------------------------------------------------------------
void CRocket::Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale)
{
	CProjectile::Launch(pos, dir, velocity, speedScale);

	m_launchLoc=pos;

	m_safeExplosion = GetParam("safeexplosion", m_safeExplosion);

	if(CWeapon* pWeapon = GetWeapon())
	{
		if(pWeapon->IsAutoDroppable())
			pWeapon->AddFiredRocket();
	}
}

