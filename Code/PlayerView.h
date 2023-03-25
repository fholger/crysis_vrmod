/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

Description:
The view specific code (First person, Third person, followCharacterHead etc).


-------------------------------------------------------------------------
History:
- 21:02:2006: Re-factored from Filippo's CPlayer code by Nick Hesketh

*************************************************************************/

#ifndef __PLAYERVIEW_H__
#define __PLAYERVIEW_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Player.h"


class CPlayerView
{
public:
	CPlayerView(const CPlayer &rPlayer,SViewParams &viewParams);

	void Process(SViewParams &viewParams);

	void Commit(CPlayer &rPlayer,SViewParams &viewParams);

protected:
	struct SViewStateIn
	{
		Vec3 lastPos;				// previous view position
		Quat lastQuat;			// previous view rotation
		float defaultFov;

		float frameTime;

		ICharacterInstance *pCharacter;
		IVehicle *pVehicle;

		bool bIsGrabbing;
		bool stats_isRagDoll;							// ViewFollowCharacterFirstPerson (Primarily: Dead or TrackView) uses to add a small z offset to the view
		bool stats_isFrozen;
		bool stats_isStandingUp;
		bool stats_isShattered;
		uint8 stats_followCharacterHead;		// Part of selection criteria for ViewFollowCharacterFirstPerson. Also forces an update to baseMtx, viewMtx,viewMtxFinal (update the player rotation if view control is taken from somewhere else)
		float stats_flatSpeed;
		float stats_leanAmount;
		float stats_inAir;
		float stats_inWater;
		bool  stats_headUnderWater;
		int stats_firstPersonBody;
		float stats_onGround;
		Vec3 stats_velocity;
		uint8 stats_flyMode;
		uint8 stats_spectatorMode;
		EntityId stats_spectatorTarget;
		bool  stats_onLadder;
		
		float params_viewFoVScale;
		Vec3 params_viewPivot;
		float params_viewDistance;
		float params_weaponInertiaMultiplier;
		Ang3 params_hudAngleOffset;
		Vec3 params_hudOffset;
		float params_viewHeightOffset;
		float params_weaponBobbingMultiplier;

		bool bIsThirdPerson;
		Vec3 vEntityWorldPos;
		EntityId entityId;
		Matrix34 entityWorldMatrix;
		Vec3 localEyePos;
		Vec3 worldEyePos;
		Matrix33 headMtxLocal;
		float thirdPersonDistance;
		float thirdPersonYaw;
		float stand_MaxSpeed;
		float deathTime;

		int32 health;

		EStance stance;
		float shake;
		float standSpeed;
		bool bSprinting;
		bool bLookingAtFriendlyAI;
		bool bIsGrabbed;
	};

	struct SViewStateInOut
	{
		//--- temporaries - FirstThird shared
		bool bUsePivot;
		float bobMul;
		Quat viewQuatForWeapon;	// First Person view matrix before bobbing has been applied
		Vec3 eyeOffsetViewGoal;
		//--- temporaries - view shared
		Ang3 wAngles;
		//--- I/O
		//--- Used by Pre-process only (in from game?)
		//Vec3 m_lastPos;	// Tested in pre-process stage, and used to calc m_blendViewOffset. cleared at this point

		Vec3 stats_HUDPos;		// Ouput only in Post-Process
		Quat viewQuatFinal;	// ProcessRotation: m_viewMtxFinal = m_viewMtx * Matrix33::CreateRotationXYZ(m_viewAnglesOffset);
		Quat viewQuat;
		Quat baseQuat;				// Output IF: in vehicle or following the character head (ViewFollowCharacterFirstPerson)

		Ang3 stats_FPWeaponAngles;
		Vec3 stats_FPWeaponPos;
		Ang3 stats_FPSecWeaponAngles;
		Vec3 stats_FPSecWeaponPos;
		//SViewShake viewShake;
		Vec3 eyeOffsetView;
		float stats_bobCycle;
		float stats_smoothViewZ;
		uint8 stats_smoothZType;

		Ang3 vFPWeaponAngleOffset;
		Vec3 vFPWeaponLastDirVec;
		Vec3 vFPWeaponOffset;
		Vec3 bobOffset;
		Ang3 angleOffset;
		Ang3 viewAngleOffset;
		bool stats_landed;
		bool stats_jumped;

		int8 stats_inFreefall;
	};

	SViewStateIn m_viewStateIn_private;	// only write-able from pre-process (Should move into a base class to enforce this)

	const SViewStateIn &m_in;
	SViewStateInOut m_io;

	void ViewFirstThirdSharedPre(SViewParams &viewParams);
	void ViewFirstThirdSharedPost(SViewParams &viewParams);
	void ViewThirdPerson(SViewParams &viewParams);
	void ViewFirstPerson(SViewParams &viewParams);
	void FirstPersonJump(SViewParams &viewParams,Vec3 &weaponOffset, Ang3 &weaponAngleOffset);
	void ViewVehicle(SViewParams &viewParams);
	void ViewFollowCharacterFirstPerson(SViewParams &viewParams);
	void ViewFirstPersonOnLadder(SViewParams & viewParams);
	void ViewSpectatorTarget(SViewParams &viewParams);
	void ViewDeathCamTarget(SViewParams &viewParams);

	void ViewExternalControlPostProcess(CPlayer &rPlayer,SViewParams &viewParams);
	public:
	void FirstPersonWeaponPostProcess(CPlayer &rPlayer,SViewParams &viewParams);
	protected:
	void ViewShakePostProcess(CPlayer &rPlayer,SViewParams &viewParams);
	void HudPostProcess(CPlayer &rPlayer,SViewParams &viewParams);
	void HandsPostProcess(CPlayer &rPlayer,SViewParams &viewParams);

	void ViewProcess(SViewParams &viewParams);
	void ViewPreProcess(const CPlayer &rPlayer,SViewParams &viewParams,SViewStateIn & m_viewStateIn);
	void ViewPostProcess(CPlayer &rPlayer,SViewParams &viewParams);
};

#endif
