/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Simple Actor implementation
  
 -------------------------------------------------------------------------
  History:
  - 19:6:2006 : Created by Filippo De Luca

*************************************************************************/
#ifndef __GRABHANDLER_H__
#define __GRABHANDLER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <vector>

/**
 * Grabbing & throwing/dropping typically occurs in 3 phases:
 *
 * - reaching out:
 *
 * During this phase, the grabbing limb moves to the object
 * to be grabbed.  Ends when the grabbing limb's end effector is at the same
 * place as the object to be grabbed.  IK may be used to modulate an animation
 * to achieve this.
 *
 * - object grabbed:
 *
 * In this state, the grabbed object is attached to the limb.
 *
 * - object thrown:
 *
 * In this state, the object is no longer attached to the limb.
 * However, the grab/throw action still is not over since we need to wait
 * some time after throwing before we re-enable the object's collisions
 * (to prevent it from colliding superfluously with the grabbing limb).
 */

struct SGrabStats
{
	#define GRAB_MAXLIMBS 4

	EntityId grabId;
	EntityId dropId;
	
	Vec3 lHoldPos;//holding position relative to the entity matrix

	Vec3 throwVector;

	Quat additionalRotation;

	int limbId[GRAB_MAXLIMBS];
	int limbNum;

	float resetFlagsDelay;//this is used to reset the grabbed object collision flags after its thrown
	float grabDelay;
	float throwDelay;

	float maxDelay;//used for both grabDelay and throwDelay

	float followSpeed;

	bool useIKRotation;

	unsigned int collisionFlags;

	//animation
	bool usingAnimation;
	bool usingAnimationForGrab;
	bool usingAnimationForDrop;

	// NOTE Mrz 2, 2007: <pvl> there are three kinds of animation-driven grabbing
	// and throwing processes.  The first one uses a single animation for both
	// grabbing and throwing (i.e. the grabbing entity throws the object away
	// immediately after it picks it up, there's no intervening carrying of the
	// object).  The animation is equipped with events marking both the instant
	// when the object is grabbed and when it's thrown.  This scheme is currently
	// used for the Hunter.
	//
	// The second and third kinds both involve a separate animations for grabbing
	// and throwing but they differ in what happens in the meantime.  One case
	// occurs when the grabbed object doesn't require a dedicated "carry"
	// animation.  If that's the case, the grabbing anim just emits a "grabbed"
	// Signal and the throwing anim emits a "thrown" Signal to let the
	// AnimatedGrabHandler instance know when to grab and let go of the object.
	// Both anims are one-shot and nothing special happens in between (e.g.
	// a normal movement is played).
	//
	// On the other hand, if carrying the object requires a special animation
	// (e.g. when the object being carried is heavy), a dedicated Action-like
	// AG input is used to control the process instead of Signal.  When this input
	// is asserted, grabbing anim is played that goes on to the carrying anim
	// (which is usually looped).  When the input stops being asserted any more,
	// drop anim is played and the character returns to a suitable normal
	// animation state.  As with the second case, the grabbing anim emits
	// the "grabbed" event and the dropping anim emits the "thrown" event.
	static const int s_maxAGInputNameLen = 64;
	char carryAnimGraphInput[s_maxAGInputNameLen];

	char grabAnimGraphSignal[s_maxAGInputNameLen];
	char dropAnimGraphSignal[s_maxAGInputNameLen];

	bool IKActive;	
	float releaseIKTime;

	int followBoneID;
	Vec3 followBoneWPos;

	/// Where grabbing anim expects the object to be grabbed to be. Given in grabber's local coord frame.
	Vec3 grabbedObjOfs;

	// NOTE Aug 2, 2007: <pvl> this is a bad workaround but what can I do if neither
	// Ivo nor Anton can help.
	// While reaching out for something to grab, the grabbing limb is retargetter
	// using IK so that its tip should end up where the grabbed entity is.  For
	// some reason it ends up near but not quite there.  When the entity is then
	// linked to the tentacle tip, it snaps.  To get rid of the snap, in principle
	// either IK could be made to move *exactly* where the entity is, or physics
	// could be used to set the entity's velocity towards the tentacle, avoiding
	// the sudden snap.  Neither seems to be doable, though.  So what we do is
	// just record the IK inaccuracy at the moment when the entity is linked to
	// the tentacle and use it to correct its further movement along with the
	// tentacle.
	bool readIkInaccuracyCorrection;
	Vec3 ikInaccuracyCorrection;

	/// World rotation of the grabbed object at the instant when it was grabbed.
	Quat origRotation;
	/// World rotation of the grabbing limb end bone at the instant when the object was grabbed.
	QuatT origEndBoneWorldRot;
	/// 'true' if the above members have valid values already loaded.
	bool origRotationsValid;

	// FIXME Sep 14, 2007: <pvl> this should be a property of the grabbed
	// entity and not the grabbing one but there's no time now to try and track down
	// scripts of everything that the hunter or the scout might possibly grab.
	/// Place where the entity should be grabbed, given as offset from the grabbed entity position.
	Vec3 entityGrabSpot;
	/// How the entityGrabSpot should be offset from the grabbing bone position when the object is held.
	/// Given in the grabbing bone coordinate system.
	Vec3 boneGrabOffset;

	SGrabStats()
	{
		memset(this,0,sizeof(SGrabStats));

		followBoneID = -1;
		lHoldPos.Set(0,0.5f,0.25f);
		additionalRotation.SetIdentity();
		IKActive = false;
		ikInaccuracyCorrection.Set (0.0f, 0.0f, 0.0f);
		readIkInaccuracyCorrection = true;
	}

	void Reset()
	{
		//*this = SGrabStats ();
		grabId = 0;
		followBoneWPos.Set(0,0,0);
		IKActive = false;
		ikInaccuracyCorrection.Set (0.0f, 0.0f, 0.0f);
		readIkInaccuracyCorrection = true;
		origRotationsValid = false;
	}

	void Serialize(TSerialize ser);
};

struct SGrabParams
{
	//
	bool throwImmediately;

	unsigned int collisionFlags;

	EntityId grabId;

	float followSpeed;
	float grabDelay;
	float throwDelay;

	// NOTE Dez 15, 2006: <pvl> lHoldPos is only used because no animation
	// is here to tell where the object should be when it's grabbed.  So
	// lHoldPos stores fixed coords wrt the grabber's coord system where
	// the grabbed object sticks.
	Vec3 lHoldPos;

	Vec3 throwVector;

	//
	SGrabParams()
	{
		memset(this,0,sizeof(SGrabParams));
	}

	SGrabParams(SmartScriptTable &rParams)
	{
		//memset(this,0,sizeof(SAnimGrabParams));

		rParams->GetValue("entityId",grabId);
		rParams->GetValue("holdPos",lHoldPos);
		rParams->GetValue("followSpeed",followSpeed);
		rParams->GetValue("collisionFlags",collisionFlags);
		rParams->GetValue("grabDelay",grabDelay);
		//drop
		rParams->GetValue("throwVec",throwVector);
		rParams->GetValue("throwImmediately",throwImmediately);
		rParams->GetValue("throwDelay",throwDelay);
	}
};

struct SAnimGrabParams : public SGrabParams
{
	char animGraphSignal[64];
	char followBone[64];

	float releaseIKTime;

	//
	SAnimGrabParams()
	{
		memset(this,0,sizeof(SAnimGrabParams));
	}

	SAnimGrabParams(SmartScriptTable &rParams)
	{
		memset(this,0,sizeof(SAnimGrabParams));

		SGrabParams baseGrabParams(rParams);
		memcpy(this,&baseGrabParams,sizeof(SGrabParams));

		SmartScriptTable animationTable;
		if (rParams->GetValue("animation",animationTable))
		{
			const char *pAnimGraphSignal = NULL;
			if (animationTable->GetValue("animGraphSignal",pAnimGraphSignal))
			{
				strncpy(animGraphSignal,pAnimGraphSignal,64);
				animGraphSignal[63] = 0;
			}

			animationTable->GetValue("forceThrow",throwDelay);					
			animationTable->GetValue("releaseIKTime",releaseIKTime);
		}

		const char *pStr;
		if (rParams->GetValue("followBone",pStr))
		{
			strncpy(followBone,pStr,64);
			followBone[63] = 0;
		}
	}
};

class CActor;
struct ICharacterInstance;

struct IGrabHandler
{
	virtual ~IGrabHandler(){};

	virtual bool Grab(SmartScriptTable &rParams) = 0;
	virtual bool Drop(SmartScriptTable &rParams) = 0;

	/// Called when the grab action is invoked.
	virtual bool SetGrab(SmartScriptTable &rParams) = 0;
	/// Called when it's actually possible to grab an object.
	virtual bool StartGrab(/*EntityId objectId*/) = 0;
	virtual bool SetDrop(SmartScriptTable &rParams) = 0;
	virtual bool StartDrop() = 0;

	virtual void Update(float frameTime) = 0;
	virtual void Reset() = 0;

	virtual CActor *GetOwner() = 0;

	//FIXME
	virtual SGrabStats *GetStats() = 0;
	//
	/**
	 * NOTE Mrz 21, 2007: <pvl> this function doesn't really belong here (it
	 * makes no sense for any grab handler type that doesn't use IK) but having
	 * it is still much better than having CActor::ProcessIKLimbs() process IK
	 * for us, manipulating SGrabStats directly.  (This troubled Filippo, too.)
	 * This was ugly but bearable until CMultipleGrabHandler - there was an
	 * implicit assumption in CActor::ProcessIKLimbs() that a grab handler had
	 * a single IK limb which was broken by CMultipleGrabHandler.
	 */
	// TODO Mrz 21, 2007: <pvl> deprecate GetStats() if possible
	virtual void ProcessIKLimbs (ICharacterInstance * ) = 0;

	virtual void Serialize(TSerialize ser) = 0;
};

class CBaseGrabHandler : public IGrabHandler
{
public:
	
	CBaseGrabHandler(CActor *pActor) : m_pActor(pActor)
	{}

	~CBaseGrabHandler()
	{}

	//
	virtual bool Grab(SmartScriptTable &rParams);
	virtual bool Drop(SmartScriptTable &rParams);

	virtual bool SetGrab(SmartScriptTable &rParams);
	virtual bool StartGrab();
	virtual bool SetDrop(SmartScriptTable &rParams);
	virtual bool StartDrop();

	virtual void Update(float frameTime);
	virtual void Reset();

	ILINE virtual CActor *GetOwner()
	{
		return m_pActor;
	}

	//FIXME:
	ILINE virtual SGrabStats *GetStats()
	{
		return &m_grabStats;
	}
	//
	virtual void ProcessIKLimbs (ICharacterInstance * ) { }

	virtual void Serialize(TSerialize ser);

protected:

	virtual void UpdatePosVelRot(float frameTime);

	virtual Vec3 GetGrabWPos();
	void IgnoreCollision(EntityId eID,unsigned int flags,bool ignore);
	void DisableGrabbedAnimatedCharacter (bool enable) const;
	
	CActor *m_pActor;

	SGrabStats m_grabStats;
};

class CAnimatedGrabHandler : public CBaseGrabHandler
{
public:
	
	CAnimatedGrabHandler(CActor *pActor) : CBaseGrabHandler(pActor)
	{}

	~CAnimatedGrabHandler()
	{}

	//
	virtual bool SetGrab(SmartScriptTable &rParams);
	virtual bool StartGrab();
	virtual bool SetDrop(SmartScriptTable &rParams);
	virtual bool StartDrop();
	virtual void ProcessIKLimbs (ICharacterInstance * pCharacter);
	//
	/// @brief Tells the handler that IK should be used from now on to
	/// retarget the grabbing limb to reach the object to be grabbed.
	void ActivateIK ();
		
protected:

	virtual void UpdatePosVelRot(float frameTime);
	QuatT GetGrabBoneWorldTM() const;

	virtual Vec3 GetGrabWPos();
	Vec3 GetGrabIKPos(IEntity *pGrab,int limbIdx);
};

/**
 * @brief Handles cases where multiple objects are grabbed simulataneously.
 *
 * Basically, this class aggregates multiple CAnimatedGrabHandlers.  It has
 * little functionality of its own, forwarding work to the CAnimatedGrabHandlers
 * most of the time.
 *
 * Its script setup table (processed in SetGrab()) expects either a simple
 * CAnimatedGrabHandler-style table, in which case it creates only one
 * "slave" CAnimatedGrabHandler and its overall behavior should be identical
 * to that of a CAnimatedGrabHandler.  Or there should be a 'grabParams' item
 * item in the table which is then expected to be an array of simple
 * CAnimatedGrabHandler-style setup tables.
 *
 * This class assumes implicitly that all operations performed by
 * CAnimatedGrabHandler that are not related to a specific object being grabbed
 * (e.g. setting AG inputs) are idempotent.  This is a side-effect of the fact
 * that this class builds upon a class that apparently wasn't really designed
 * to be reused.
 *
 * Currently (Mar 21, 2007) used for Scouts only.
 */
class CMultipleGrabHandler : public CBaseGrabHandler
{
public:
	CMultipleGrabHandler (CActor *pActor) : CBaseGrabHandler(pActor){}
	virtual ~CMultipleGrabHandler(){}

	virtual bool SetGrab(SmartScriptTable &rParams);
	virtual bool StartGrab();
	virtual bool SetDrop(SmartScriptTable &rParams);
	virtual bool StartDrop();

	virtual void Update(float frameTime);
	virtual void Reset();

	virtual void ProcessIKLimbs (ICharacterInstance * );

	virtual void Serialize(TSerialize ser);
private:
	std::vector <CAnimatedGrabHandler*> m_handlers;
};


#endif //__GRABHANDLER_H__
