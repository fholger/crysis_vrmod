/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Implements the naked alien.
  
 -------------------------------------------------------------------------
  History:
  - 6:12:2004: Created by Filippo De Luca

*************************************************************************/
#ifndef __SCOUT_H__
#define __SCOUT_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Alien.h"

class CScout :
	public CAlien
{
public:

	CScout();
	~CScout();

	virtual IGrabHandler *CreateGrabHanlder();

	virtual bool CreateCodeEvent(SmartScriptTable &rTable);

	virtual void Revive(bool fromInit = false);

	virtual void ProcessRotation(float frameTime);
	virtual void ProcessSwimming(float frameTime);
	virtual void ProcessMovement(float frameTime);

	virtual void	SetActorStance(SMovementRequestParams &control, int& actions)
	{
		// Empty
	}
	virtual void AnimationEvent(ICharacterInstance *, const AnimEventInstance &);

	virtual void UpdateGrab(float frameTime);

	virtual void SetActorMovement(SMovementRequestParams &control);

	virtual void FullSerialize( TSerialize ser );
	//virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );

	void GetMemoryStatistics(ICrySizer * s);

	//Player can't grab scouts
	virtual int	 GetActorSpecies() { return eGCT_UNKNOWN; }

	virtual void GetActorInfo( SBodyInfo& bodyInfo );

private:

	void ProcessMovementNew(float frameTime);
	void ProcessRotationNew(float frameTime);
  bool EnableSearchBeam(bool enable);

protected:

};

#endif //__SCOUT_H__
