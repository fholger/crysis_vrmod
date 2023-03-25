/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Implements the Hunter alien.
  
 -------------------------------------------------------------------------
  History:
  - 25:4:2005: Created by Filippo De Luca

*************************************************************************/
#ifndef __HUNTER_H__
#define __HUNTER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Alien.h"

#include <bitset>

class CHunter :
	public CAlien
{
public:

	CHunter ();

	virtual IGrabHandler *CreateGrabHanlder();

	virtual void Revive(bool fromInit);
	virtual bool CreateCodeEvent(SmartScriptTable &rTable);

	virtual void ProcessRotation(float frameTime);
	virtual void ProcessMovement(float frameTime);

	virtual void ProcessAnimation(ICharacterInstance *pCharacter,float frameTime);
	virtual void PostPhysicalize();
	virtual void RagDollize( bool fallAndPlay );
	virtual void ProcessEvent(SEntityEvent& event);

	virtual void UpdateFiringDir(float frameTime);

	virtual void	SetActorStance(SMovementRequestParams &control, int& actions)
	{
		// Empty
	}

	virtual void GetActorInfo( SBodyInfo& bodyInfo );
	virtual void AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event);

	virtual void SetFiring(bool fire);

	virtual int GetBoneID(int ID,int slot = 0) const;

	virtual bool IsFlying(){return false;}

	virtual bool SetAnimationInput( const char * inputID, const char * value );

	virtual void FullSerialize( TSerialize ser );
	
	void GetMemoryStatistics(ICrySizer * s);

	//Player can't grab hunters
	virtual int	 GetActorSpecies() { return eGCT_UNKNOWN; }

	virtual void PlayAction( const char* action, const char* extension, bool looping );

protected:
	
	virtual void UpdateAnimGraph( IAnimationGraphState * pState );

	void PlayFootstepEffects (int tentacle) const;
	void PlayFootliftEffects (int tentacle) const;

	bool TentacleInTheAir (int tentacle) const;
	/// These functions work from the animation point of view, unaltered by IK.
	/// See also m_footTouchesGround.
	bool TentacleGoingUp (int tentacle) const;
	bool TentacleGoingDown (int tentacle) const;

	bool Airborne () const;

	Vec3 m_smoothMovementVec;
	Vec3 m_balancePoint;
	float m_zDelta;

	/// 'true' if the hunter is turning around.  We need to know because the decision
	/// whether to turn in this frame depends on whether the hunter's already turning.
	bool m_turning;
	/// Both given as the maximum dot product of (normalized) desired and actual
	/// directions for turning to happen.
	static const float s_turnThreshIdling;
	static const float s_turnThreshTurning;

	//
	int m_IKLimbIndex[4];
	Vec3 m_footGroundPos[4];
	Vec3 m_footGroundPosLast[4];
  int  m_footGroundSurface[4];
	/// True if tentacle's final world position, after all adjustments, is on the ground.
	bool m_footTouchesGround[4];
	f32 m_footTouchesGroundSmooth[4];
	f32 m_footTouchesGroundSmoothRate[4];
  IAttachment* m_footAttachments[4];

	bool m_IKLook;

	float m_nextStopCheck;

	//TEMPORARY
	float m_footSoundTime[4];

	float m_smoothZ;
	float m_zOffset;

	enum WalkEventIndex {
		FRONT_LEFT_UP = 0,
		FRONT_LEFT_DOWN,
		FRONT_RIGHT_UP,
		FRONT_RIGHT_DOWN,
		BACK_LEFT_UP,
		BACK_LEFT_DOWN,
		BACK_RIGHT_UP,
		BACK_RIGHT_DOWN,
		NUM_WALK_EVENTS
	};
	std::bitset<NUM_WALK_EVENTS> m_walkEventFlags;
};

inline bool CHunter::TentacleInTheAir (int tentacle) const
{
	return m_footGroundPos[tentacle] == Vec3 (0.0f, 0.0f, 0.0f);
}

inline bool CHunter::TentacleGoingUp (int tentacle) const
{
	return m_walkEventFlags.test (2*tentacle);
}

inline bool CHunter::TentacleGoingDown (int tentacle) const
{
	return m_walkEventFlags.test (2*tentacle + 1);
}

inline bool CHunter::Airborne () const
{
	return false; //pretend the hunter is always on ground

	// m_stats.inAir indicates if the hunter as a whole is on the ground
	if (m_stats.inAir > 2.0f)
		return true;
	return false;
}

#endif //__HUNTER_H__
