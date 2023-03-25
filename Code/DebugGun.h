/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: DebugGun Implementation

-------------------------------------------------------------------------
History:
- 09:01:2006   14:00 : Created by Michael Rauh

*************************************************************************/
#ifndef __DebugGun_H__
#define __DebugGun_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IItemSystem.h>
#include "Weapon.h"


class CDebugGun :
  public CWeapon
{
public:
  CDebugGun();
  void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);
  void Update(SEntityUpdateContext& ctx, int update);
  void Shoot( bool bPrimary);
	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CWeapon::GetMemoryStatistics(s); s->AddContainer(m_fireModes); }

  virtual void Select(bool select);

private:
  ICVar* m_pAIDebugDraw;
  int m_aiDebugDrawPrev;
  
  typedef std::pair<string, float> TFmPair;
  std::vector<TFmPair> m_fireModes;    
  int m_fireMode;
};

#endif // __DebugGun_H__