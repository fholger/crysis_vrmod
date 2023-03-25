/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Player Feature Enabling Item

-------------------------------------------------------------------------
History:
- 23:10:2006   12:25 : Created by Márcio Martins

*************************************************************************/
#ifndef __PLAYERFEATURE_H__
#define __PLAYERFEATURE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Item.h"


class CActor;
class CPlayerFeature: public CItem
{
public:
	CPlayerFeature();

	virtual void PostInit(IGameObject * pGameObject );
	virtual bool ReadItemParams(const IItemParamsNode *root);

	virtual void OnReset();
	virtual void PickUp(EntityId pickerId, bool sound, bool select/* =true */, bool keepHistory/* =true */);
	virtual void OnPickedUp(EntityId pickerId, bool destroyed);

	virtual void ActivateFeature(CActor *pActor, const char *feature);
	virtual void NanoSuit(CActor *pActor);
	virtual void Parachute(CActor *pActor);
	virtual void AlienCloak(CActor *pActor);
	virtual void NightVision(CActor *pActor);
	virtual void DualSOCOM(CActor *pActor); //Dual socoms for AI
	
private:
	std::vector<string> m_features;
	bool                m_notPickUp;
};


#endif //__PLAYERFEATURE_H__