/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Detonation Fire Mode Implementation

-------------------------------------------------------------------------
History:
- 11:9:2004   15:00 : Created by Márcio Martins

*************************************************************************/
#ifndef __DETONATE_H__
#define __DETONATE_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Single.h"

class CDetonate :
	public CSingle
{
	struct ExplodeAction;
protected:
public:
	CDetonate();
	virtual ~CDetonate();

	//IFireMode
	virtual void Update(float frameTime, uint frameId);
	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CSingle::GetMemoryStatistics(s); }

	virtual void ResetParams(const struct IItemParamsNode *params);
	virtual void PatchParams(const struct IItemParamsNode *patch);

	virtual void Activate(bool activate);

	virtual bool CanReload() const;

	virtual bool CanFire(bool considerAmmo = true) const;
	virtual void StartFire();

	virtual void NetShoot(const Vec3 &hit, int ph);
	//~IFireMode

	virtual const char *GetCrosshair() const;
protected:
	bool Detonate(bool net=false);
	void SelectLast();

	EntityId	m_projectileId;
	float			m_detonationTimer;
};


#endif //__DETONATE_H__