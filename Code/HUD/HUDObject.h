/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 	
	Header for HUD object base class
	Shared by G02 and G04

-------------------------------------------------------------------------
History:
- 07:11:2005: Created by Julien Darre
- 22:02:2006: Refactored for G04 by Matthew Jack

*************************************************************************/
#ifndef __HUDOBJECT_H__
#define __HUDOBJECT_H__

//-----------------------------------------------------------------------------------------------------

#include "ISerialize.h"

//-----------------------------------------------------------------------------------------------------

class CHUDObject
{
public:

						CHUDObject();
	virtual ~	CHUDObject();

	virtual void Update(float fDeltaTime) = 0;

	virtual void PreUpdate() {};
	virtual void OnHUDToBeDestroyed() {};
	virtual void Serialize(TSerialize ser) {};

	void GetHUDObjectMemoryStatistics(ICrySizer * s);

protected:

	float m_fX;
	float m_fY;
};

//-----------------------------------------------------------------------------------------------------

#endif

//-----------------------------------------------------------------------------------------------------