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

#include "StdAfx.h"

#include "Game.h"
#include "GameCVars.h"

#include "Player.h"
#include "PlayerView.h"
#include "GameUtils.h"
#include "GameActions.h"

#include "Weapon.h"
#include "WeaponSystem.h"
#include "Single.h"

#include <IViewSystem.h>
#include <IItemSystem.h>
#include <IPhysics.h>
#include <ICryAnimation.h>
#include "IAISystem.h"
#include "IAgent.h"
#include <IVehicleSystem.h>
#include <ISerialize.h>

#include <IRenderAuxGeom.h>

#include <IGameTokens.h>

#include "HUD/HUD.h"


//CPlayerView::SViewStateIn CPlayerView::m_in;
//CPlayerView::SViewStateInOut CPlayerView::m_io;

CPlayerView::CPlayerView(const CPlayer &rPlayer,SViewParams &viewParams) : m_in(m_viewStateIn_private)
{
	ViewPreProcess(rPlayer,viewParams,m_viewStateIn_private);
}

void CPlayerView::Process(SViewParams &viewParams)
{
	ViewProcess(viewParams);
}

void CPlayerView::Commit(CPlayer &rPlayer,SViewParams &viewParams)
{
	ViewPostProcess(rPlayer,viewParams);
}

//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------
//--- ViewPreProcess
//--------------------------------------------------------------------------
// Fetch all the data we need to work on.
//--------------------------------------------------------------------------
void CPlayerView::ViewPreProcess(const CPlayer &rPlayer,SViewParams &viewParams,SViewStateIn &m_in)
{

	// SViewStateIn --- the constants
	{
		m_in.lastPos=viewParams.position;
		m_in.lastQuat=viewParams.rotation;
		m_in.defaultFov= g_pGameCVars->cl_fov;
		
		m_in.frameTime=min(gEnv->pTimer->GetFrameTime(),0.1f);
		
		m_in.pCharacter = rPlayer.GetEntity()->GetCharacter(0);
		m_in.pVehicle=rPlayer.GetLinkedVehicle();
		
		m_in.bIsGrabbing=false;
		/*if (rPlayer.m_grabStats.grabId>0)
		{
			m_in.bIsGrabbing=true;
			// *****
			viewParams.nearplane = 0.1f;
			// *****
		}*/
		
		m_in.stats_isRagDoll=rPlayer.m_stats.isRagDoll;
		m_in.stats_isStandingUp=rPlayer.m_stats.isStandingUp;
		m_in.stats_isFrozen=rPlayer.m_stats.isFrozen.Value();
		m_in.stats_isShattered=rPlayer.m_stats.isShattered.Value();
		m_in.stats_followCharacterHead=rPlayer.m_stats.followCharacterHead.Value();
		m_in.stats_flatSpeed=rPlayer.m_stats.speedFlat;	
		m_in.stats_leanAmount=rPlayer.m_stats.leanAmount;
		m_in.stats_inAir=rPlayer.m_stats.inAir;	
		m_in.stats_inWater=rPlayer.m_stats.inWaterTimer;
		m_in.stats_headUnderWater = (rPlayer.m_stats.headUnderWaterTimer > 0.0f);
		m_io.stats_jumped=rPlayer.m_stats.jumped;
		m_in.stats_firstPersonBody=rPlayer.m_stats.firstPersonBody.Value();	
		m_in.stats_onGround=rPlayer.m_stats.onGround;
		m_io.stats_landed=rPlayer.m_stats.landed;
		m_in.stats_velocity=rPlayer.m_stats.velocity;
		m_in.bSprinting=rPlayer.m_stats.bSprinting;
		m_io.stats_inFreefall=rPlayer.m_stats.inFreefall.Value();

		m_in.stats_onLadder=rPlayer.m_stats.isOnLadder;
		m_in.bLookingAtFriendlyAI = rPlayer.m_stats.bLookingAtFriendlyAI;
		m_in.bIsGrabbed = rPlayer.m_stats.isGrabbed;
	
		m_in.params_viewFoVScale=rPlayer.m_params.viewFoVScale;
		m_in.params_viewPivot=rPlayer.m_params.viewPivot;
		m_in.params_viewDistance=rPlayer.m_params.viewDistance;
		m_in.params_weaponInertiaMultiplier=rPlayer.m_params.weaponInertiaMultiplier;
		m_in.params_hudAngleOffset=rPlayer.m_params.hudAngleOffset;
		m_in.params_hudOffset=rPlayer.m_params.hudOffset;	
		m_in.params_viewHeightOffset=rPlayer.m_params.viewHeightOffset;
		m_in.params_weaponBobbingMultiplier=rPlayer.m_params.weaponBobbingMultiplier;
		
		
		m_io.bobMul=g_pGameCVars->cl_bob * m_in.params_weaponBobbingMultiplier;
		
		m_in.bIsThirdPerson=rPlayer.IsThirdPerson();
		m_in.vEntityWorldPos=rPlayer.GetEntity()->GetWorldPos();
		m_in.entityId=rPlayer.GetEntityId();

		// use absolute entity matrix when frozen
		if (rPlayer.m_stats.isFrozen.Value())
			m_in.entityWorldMatrix=Matrix34(rPlayer.GetEntity()->GetWorldTM());	
		else
			m_in.entityWorldMatrix=Matrix34(rPlayer.GetEntity()->GetSlotWorldTM(0));	

		m_in.localEyePos=rPlayer.GetLocalEyePos();
		m_in.worldEyePos=m_in.entityWorldMatrix*m_in.localEyePos;
		
		m_in.headMtxLocal.SetIdentity();

		if (m_in.stats_followCharacterHead)
		{
			int16 joint_id = rPlayer.GetBoneID(BONE_HEAD);
			if (joint_id>=0)
				m_in.headMtxLocal=Matrix33(m_in.pCharacter->GetISkeletonPose()->GetAbsJointByID(joint_id).q.GetNormalized());
		}
	
		m_in.thirdPersonDistance=g_pGameCVars->cl_tpvDist;
		m_in.thirdPersonYaw=g_pGameCVars->cl_tpvYaw;
		
		EStance refStance((rPlayer.m_stance==STANCE_PRONE)?STANCE_PRONE:STANCE_STAND);
		m_in.stand_MaxSpeed=rPlayer.GetStanceInfo(refStance)->maxSpeed;
		m_in.standSpeed=rPlayer.GetStanceMaxSpeed(refStance);	
		
		m_in.health=rPlayer.GetHealth();
		m_in.stance=rPlayer.m_stance;
		m_in.shake= g_pGameCVars->cl_sprintShake;

		m_in.deathTime = rPlayer.GetDeathTime();
	}
	
	// SViewStateInOut -- temporaries and output
	{
		//--- temporaries - FirstThird shared
		
		//m_io.bUsePivot
		//m_io.bobMul
		//m_io.viewQuatForWeapon
		//m_io.eyeOffsetGoal
		
		//--- temporaries - view shared
		m_io.wAngles=Ang3(0,0,0);
		//--- I/O
		//m_io.m_lastPos=rPlayer.m_lastPos;
		// Integ
		//if (m_io.m_lastPos.len2()>0.01f)
		//{
		//	m_io.blendViewOffset = m_io.m_lastPos - rPlayer.GetEntity()->GetWorldPos(); 
		//	m_io.m_lastPos.Set(0,0,0);
		//}
	}
	
	// *****
	viewParams.fov = m_in.defaultFov*rPlayer.m_params.viewFoVScale*(gf_PI/180.0f);
	viewParams.nearplane = 0.0f;
	// *****
		
	m_io.eyeOffsetViewGoal=rPlayer.GetStanceViewOffset(rPlayer.m_stance);

	m_io.viewQuatFinal=rPlayer.m_viewQuatFinal;
	m_io.viewQuat=rPlayer.m_viewQuat;

	m_io.stats_FPWeaponAngles=rPlayer.m_stats.FPWeaponAngles;
	m_io.stats_FPWeaponPos=rPlayer.m_stats.FPWeaponPos;

	m_io.vFPWeaponOffset=rPlayer.m_FPWeaponOffset;

	m_io.stats_FPSecWeaponAngles=rPlayer.m_stats.FPSecWeaponAngles;
	m_io.stats_FPSecWeaponPos=rPlayer.m_stats.FPSecWeaponPos;

	//m_io.viewShake=rPlayer.m_viewShake;

	m_io.eyeOffsetView=rPlayer.m_eyeOffsetView;
	m_io.baseQuat=rPlayer.m_baseQuat;

	m_io.vFPWeaponAngleOffset=rPlayer.m_FPWeaponAngleOffset;
	m_io.stats_bobCycle=rPlayer.m_stats.bobCycle;

	m_io.vFPWeaponLastDirVec=rPlayer.m_FPWeaponLastDirVec;
	m_io.bobOffset=rPlayer.m_bobOffset;
	m_io.angleOffset=rPlayer.m_angleOffset;
	m_io.viewAngleOffset = rPlayer.m_viewAnglesOffset;

	m_in.stats_flyMode=rPlayer.m_stats.flyMode;
	m_in.stats_spectatorMode=rPlayer.m_stats.spectatorMode;
	m_in.stats_spectatorTarget=rPlayer.m_stats.spectatorTarget;
	
	m_io.stats_smoothViewZ=rPlayer.m_stats.smoothViewZ;
	m_io.stats_smoothZType=rPlayer.m_stats.smoothZType;

	//-- Any individual PreProcess handlers should be called here
}

//--------------------------------------------------------------------------
//--- ViewProcess
//--------------------------------------------------------------------------
// Process our local copy of the data.
// (This is where all the actual work should occur)
//--------------------------------------------------------------------------
void CPlayerView::ViewProcess(SViewParams &viewParams)
{
	if(m_in.stats_spectatorMode == 0 && m_in.stats_spectatorTarget && gEnv->bMultiplayer && m_in.health <= 0 && g_pGameCVars->g_deathCam != 0)
	{
			ViewFirstThirdSharedPre(viewParams);
			ViewDeathCamTarget(viewParams);
			ViewFirstThirdSharedPost(viewParams);
	}
	// Externally controlled first person view e.g. by animation
 	else if ((m_in.stats_isRagDoll || m_in.stats_isFrozen || m_in.stats_isStandingUp) && !m_in.bIsThirdPerson && !m_in.stats_onLadder && m_in.pCharacter && (m_in.stats_firstPersonBody==1 || m_in.stats_followCharacterHead))
 	{
 		ViewFollowCharacterFirstPerson(viewParams);
 	}
 	else if (m_in.pVehicle)
 	{
 		ViewVehicle(viewParams);
 	}
 	else if(m_in.stats_onLadder)
 	{
 		ViewFirstThirdSharedPre(viewParams);
 		ViewFirstPersonOnLadder(viewParams);
 	}
  else if(m_in.stats_spectatorMode == CActor::eASM_Follow)
 	{
		ViewFirstThirdSharedPre(viewParams);
 		ViewSpectatorTarget(viewParams);
		ViewFirstThirdSharedPost(viewParams);
 	}
 	else
 	{
 		ViewFirstThirdSharedPre(viewParams);
 
		if (m_in.bIsThirdPerson || m_in.stats_isShattered || m_io.bUsePivot)
 			ViewThirdPerson(viewParams);
 		else
 			ViewFirstPerson(viewParams);
 
 		ViewFirstThirdSharedPost(viewParams);
 	}
}

//--------------------------------------------------------------------------
//--- ViewPostProcess
//--------------------------------------------------------------------------
// Commit all the changes
//--------------------------------------------------------------------------
void CPlayerView::ViewPostProcess(CPlayer &rPlayer,SViewParams &viewParams)
{
	/*IEntity *pLinked = rPlayer.GetLinkedEntity();
	if (pLinked && rPlayer.m_stats.linkedFreeLook)
	{
		viewParams.rotation = Quat(pLinked->GetWorldTM()) * viewParams.rotation;
	}*/


	// update the player rotation if view control is taken from somewhere else (e.g. animation or vehicle)
	ViewExternalControlPostProcess(rPlayer,viewParams);

	//set first person weapon position/rotation
	FirstPersonWeaponPostProcess(rPlayer,viewParams);

	ViewShakePostProcess(rPlayer,viewParams);


	//--------------------------
	// Output changed temporaries - debugging.
	//--------------------------

	//--------------------------
	// Output changed state.
	//--------------------------
	//rPlayer.m_lastPos=m_io.m_lastPos;

	rPlayer.m_viewQuat=m_io.viewQuat;
	//FIXME:updating the baseMatrix due being in a vehicle or having a first person animations playing has to be moved somewhere else
	rPlayer.m_baseQuat=m_io.baseQuat;
	
	if (!rPlayer.IsTimeDemo())
	{
		rPlayer.m_viewQuatFinal=m_io.viewQuatFinal;
	}

	rPlayer.m_stats.FPWeaponAngles=m_io.stats_FPWeaponAngles;
	rPlayer.m_stats.FPWeaponPos=m_io.stats_FPWeaponPos;
	rPlayer.m_stats.FPSecWeaponAngles=m_io.stats_FPSecWeaponAngles;
	rPlayer.m_stats.FPSecWeaponPos=m_io.stats_FPSecWeaponPos;
	//rPlayer.m_viewShake=m_io.viewShake;

	rPlayer.m_eyeOffsetView=m_io.eyeOffsetView;
	rPlayer.m_stats.bobCycle=m_io.stats_bobCycle;

	rPlayer.m_FPWeaponAngleOffset=m_io.vFPWeaponAngleOffset;
	rPlayer.m_FPWeaponLastDirVec=m_io.vFPWeaponLastDirVec;
	rPlayer.m_FPWeaponOffset=m_io.vFPWeaponOffset;
	rPlayer.m_bobOffset=m_io.bobOffset;
	rPlayer.m_angleOffset=m_io.angleOffset;
	rPlayer.m_viewAnglesOffset = m_io.viewAngleOffset;
	rPlayer.m_stats.landed=m_io.stats_landed;
	rPlayer.m_stats.jumped=m_io.stats_jumped;

	rPlayer.m_stats.smoothViewZ = m_io.stats_smoothViewZ;
	rPlayer.m_stats.smoothZType = m_io.stats_smoothZType;
	//--------------------------

	rPlayer.m_stats.shakeAmount = viewParams.shakingRatio;

	HandsPostProcess(rPlayer,viewParams);
}


void CPlayerView::ViewFirstThirdSharedPre(SViewParams &viewParams)
{
	// don't blend view when spectating
	if(m_in.stats_spectatorMode >= CActor::eASM_FirstMPMode && m_in.stats_spectatorMode <= CActor::eASM_LastMPMode)
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_in.entityId));
		if(pPlayer)
			pPlayer->SupressViewBlending();
	}

	viewParams.viewID = 0;

	m_io.bUsePivot = (m_in.params_viewPivot.len2() > 0) && !m_in.bIsThirdPerson && !m_in.stats_onLadder;
	viewParams.position = m_io.bUsePivot ? m_in.params_viewPivot : m_in.vEntityWorldPos;

	m_io.viewQuatForWeapon=m_io.viewQuatFinal;
	m_io.viewQuatFinal *= Quat::CreateRotationXYZ(m_io.angleOffset * (gf_PI/180.0f));

	viewParams.rotation = m_io.viewQuatFinal;
}

void CPlayerView::ViewFirstThirdSharedPost(SViewParams &viewParams)
{
	//--- Update the eye offset and apply 
	{
		// Blend towards the goal eye offset
		float stanceTransitionSpeed = g_pGameCVars->g_stanceTransitionSpeed;
		if ((m_in.stance == STANCE_PRONE) ||
			(m_io.eyeOffsetView.z < (m_io.eyeOffsetViewGoal.z-0.2f)))
			stanceTransitionSpeed = g_pGameCVars->g_stanceTransitionSpeedSecondary;

		Interpolate(m_io.eyeOffsetView, m_io.eyeOffsetViewGoal, stanceTransitionSpeed, m_in.frameTime);

		//apply eye offset
		if (!m_io.bUsePivot)
		{
			//apply some more offset to the view
			viewParams.position += m_io.baseQuat * m_io.eyeOffsetView;

			if (m_in.stats_firstPersonBody==2)
			{
				float lookDown(m_io.viewQuatFinal.GetColumn1() * m_io.baseQuat.GetColumn2());
				float forwardOffset(0.0f);
				if (m_in.stance == STANCE_STAND)
					forwardOffset = 0.15f;

				viewParams.position += m_io.viewQuatFinal.GetColumn1() * max(-lookDown,0.0f) * forwardOffset + m_io.viewQuatFinal.GetColumn2() * max(-lookDown,0.0f) * 0.0f;
				//Grabbed by the Hunter
				if(m_in.bIsGrabbed && !m_in.bIsThirdPerson)
					viewParams.position = m_in.entityWorldMatrix.TransformPoint(m_io.eyeOffsetView);
			}
		}
	}

  if(g_pGameCVars->g_detachCamera!=0)
  {
      viewParams.position = m_in.lastPos;
      viewParams.rotation = m_in.lastQuat;
  }

	//--- Weapon orientation
	//FIXME: this should be done in the player update anyway.
	//And all the view position update. (because the game may need to know other players eyepos and such)
	//update first person weapon model position/angles
	{
		Quat wQuat(m_io.viewQuatForWeapon * Quat::CreateRotationXYZ(m_io.vFPWeaponAngleOffset * gf_PI/180.0f));
		//wQuat *= Quat::CreateSlerp(viewParams.shakeQuat,IDENTITY,0.5f);
		wQuat *= Quat::CreateSlerp(viewParams.currentShakeQuat,IDENTITY,0.5f);
		wQuat.Normalize();

		m_io.wAngles = Ang3(wQuat);
	}

	//smooth out the view elevation		
	if (m_in.stats_inAir < 0.1f && !m_in.stats_flyMode && !m_in.stats_spectatorMode && !m_io.bUsePivot)
	{
		if (m_io.stats_smoothZType!=1)
		{
			m_io.stats_smoothViewZ = viewParams.position.z;
			m_io.stats_smoothZType = 1;
		}

		Interpolate(m_io.stats_smoothViewZ,viewParams.position.z,15.0f,m_in.frameTime);
		viewParams.position.z = m_io.stats_smoothViewZ;
	}
	else
	{
		if (m_io.stats_smoothZType==1)
		{
			m_io.stats_smoothViewZ = m_in.lastPos.z - m_io.stats_smoothViewZ;
			m_io.stats_smoothZType = 2;
		}

		Interpolate(m_io.stats_smoothViewZ,0.0f,15.0f,m_in.frameTime);
		viewParams.position.z += m_io.stats_smoothViewZ;
	}

}

void CPlayerView::ViewThirdPerson(SViewParams &viewParams)
{
	if (m_in.thirdPersonYaw>0.001f)
	{
		viewParams.rotation *= Quat::CreateRotationXYZ(Ang3(0,0,m_in.thirdPersonYaw * gf_PI/180.0f));
		m_io.viewQuatFinal = viewParams.rotation;
	}

	if (g_pGameCVars->goc_enable)
	{
		Vec3 target(g_pGameCVars->goc_targetx, g_pGameCVars->goc_targety, g_pGameCVars->goc_targetz);
		static Vec3 current(target);

		Interpolate(current, target, 5.0f, m_in.frameTime);

		// make sure we don't clip through stuff that much
		Vec3 offsetX(0,0,0);
		Vec3 offsetY(0,0,0);
		Vec3 offsetZ(0,0,0);
		offsetX = m_io.viewQuatFinal.GetColumn0() * current.x;
		offsetY = m_io.viewQuatFinal.GetColumn1() * current.y;
		offsetZ = m_io.viewQuatFinal.GetColumn2() * current.z;

		IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_in.entityId);
		if (pActor)
		{
			static ray_hit hit;	
			IPhysicalEntity* pSkipEntities[10];
			int nSkip = 0;
			IItem* pItem = pActor->GetCurrentItem();
			if (pItem)
			{
				CWeapon* pWeapon = (CWeapon*)pItem->GetIWeapon();
				if (pWeapon)
					 nSkip = CSingle::GetSkipEntities(pWeapon, pSkipEntities, 10);
			}

			float oldLen = offsetY.len();

			Vec3 start = m_io.baseQuat * m_io.eyeOffsetView + viewParams.position+offsetX+offsetZ;
			if (gEnv->pPhysicalWorld->RayWorldIntersection(start, offsetY, ent_static|ent_terrain|ent_rigid,
				rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1, pSkipEntities, nSkip))
			{
				offsetY = hit.pt - start;
				if (offsetY.len()> 0.25f)
				{
					offsetY -= offsetY.GetNormalized()*0.25f;
				}
				current.y = current.y * (hit.dist/oldLen);
			}
		}
		
		//viewParams.position += m_io.viewQuatFinal.GetColumn0() * current.x;		// right 
		//viewParams.position += m_io.viewQuatFinal.GetColumn1() * current.y;	// back
		//viewParams.position += m_io.viewQuatFinal.GetColumn2() * current.z;	// up
		viewParams.position += offsetX + offsetY + offsetZ;
	}
	else
	{
	if (m_io.bUsePivot)			
		viewParams.position += m_io.viewQuatFinal.GetColumn1() * m_in.params_viewDistance + m_io.viewQuatFinal.GetColumn2() * (0.25f + m_in.params_viewHeightOffset);
	else
		{
		viewParams.position += m_io.viewQuatFinal.GetColumn1() * -m_in.thirdPersonDistance + m_io.viewQuatFinal.GetColumn2() * (0.25f + m_in.params_viewHeightOffset);
}
	}
}

#if 0
void CPlayerView::ViewThirdPersonDirected(SViewParams &viewParams)
{
	if (m_in.thirdPersonYaw>0.001f)
	{
		viewParams.rotation *= Quat::CreateRotationXYZ(Ang3(0,0,m_in.thirdPersonYaw * gf_PI/180.0f));
		m_io.viewQuatFinal = Matrix33(viewParams.rotation);
	}

	if (m_io.bUsePivot)			
		viewParams.position += m_io.viewQuatFinal.GetColumn1() * m_in.params_viewDistance + m_io.viewQuatFinal.GetColumn2() * (0.25f + m_in.params_viewHeightOffset);
	else
		viewParams.position += m_io.viewQuatFinal.GetColumn1() * -m_in.thirdPersonDistance + m_io.viewQuatFinal.GetColumn2() * (0.25f + m_in.params_viewHeightOffset);
}
#endif

// jump/land spring effect. Adjust the eye and weapon pos as required.
void CPlayerView::FirstPersonJump(SViewParams &viewParams,Vec3 &weaponOffset, Ang3 &weaponAngleOffset)
{
	if (m_in.stats_onGround)
	{
		if (m_io.stats_landed)
		{
			m_io.eyeOffsetViewGoal.z -= (m_in.stance == STANCE_PRONE)?0.1f:0.3f;
			weaponOffset -= m_io.baseQuat.GetColumn2() * 0.125f;
			weaponAngleOffset.x += 3.0f;
		}

		if (m_in.stats_onGround>0.1f)
			m_io.stats_landed = false;
	}
}

//
// Extract the various modifiers out into their own function.
//
// Aim is to make the code easier to understand and modify, as 
// well as to ease the addition of new modifiers.
//
void CPlayerView::ViewFirstPerson(SViewParams &viewParams)
{
		//headbob
		Ang3 angOffset(0,0,0);
		Vec3 weaponOffset(0,0,0);
		Ang3 weaponAngleOffset(0,0,0);

		// jump/land spring effect. Adjust the eye and weapon pos as required.
		FirstPersonJump(viewParams,weaponOffset,weaponAngleOffset);

		//float standSpeed(GetStanceMaxSpeed(STANCE_STAND));

		Vec3 vSpeed(0,0,0);
		if (m_in.standSpeed>0.001f)
			vSpeed = (m_in.stats_velocity / m_in.standSpeed);

		float vSpeedLen(vSpeed.len());
		if (vSpeedLen>1.5f)
			vSpeed = vSpeed / vSpeedLen * 1.5f;

		float speedMul(0);
		if (m_in.standSpeed>0.001f)
			speedMul=(m_in.stats_flatSpeed / m_in.standSpeed * 1.1f);

		speedMul = min(1.5f,speedMul);

		bool crawling(m_in.stance==STANCE_PRONE /*&& m_in.stats_flatSpeed>0.1f*/ && m_in.stats_onGround>0.1f);
		bool weaponZoomed = false;
		bool weaponZomming = false;

		//Not crawling while in zoom mode
		IActor *owner = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_in.entityId);
		if(owner && owner->IsPlayer())
		{
			IItem *pItem = owner->GetCurrentItem();
			if(pItem)
			{
				CWeapon *pWeapon = static_cast<CWeapon*>(pItem->GetIWeapon());
				if(pWeapon)
				{
					weaponZoomed = pWeapon->IsZoomed();
					weaponZomming = pWeapon->IsZooming();
					if(weaponZoomed||weaponZomming||pWeapon->IsModifying())
						crawling = false;
				}
			}
		}
	
		// On the ground.
		if (m_in.stats_inAir < 0.1f /*&& m_in.stats_inWater < 0.1f*/)
		{
			//--- Bobbing.
			// bobCycle is a speed varying time step running (looping) from 0 to 1 
			// this feeds into a sin eqn creating a double horizontal figure of 8.
			// ( a lissajous figure with the vertical freq twice the horz freq ).

			// To tweak the total speed of the curve:

			// To tweak the effect speed has on the curve:
			float kSpeedToBobFactor=1.15f;//0.9f
			// To tweak the width of the bob:
			float kBobWidth=0.1f;
			// To tweak the height of the bob:
			float kBobHeight=0.05f;
			// To tweak the scale of strafing lag: (may need to manually adjust the strafing angle offsets as well.)
			const float kStrafeHorzScale=0.05f;

			kBobWidth = 0.15f;
			kBobHeight = 0.06f;

			m_io.stats_bobCycle += m_in.frameTime * kSpeedToBobFactor * speedMul;// * (m_in.bSprinting?1.25f:1.0f);

			//if player is standing set the bob to rest. (bobCycle reaches 1.0f within 1 second)
			if (speedMul < 0.1f)
				m_io.stats_bobCycle = min(m_io.stats_bobCycle + m_in.frameTime * 1.0f,1.0f);

			// bobCycle loops between 0 and 1
			if (m_io.stats_bobCycle>1.0f)
				m_io.stats_bobCycle = m_io.stats_bobCycle - 1.0f;

			if (crawling)
				kBobWidth *= 2.0f * speedMul;
			else if (m_in.bSprinting)
				kBobWidth *= 1.25f * speedMul;

			//set the bob offset
			Vec3 bobDir(cry_sinf(m_io.stats_bobCycle*gf_PI*2.0f)*kBobWidth*speedMul,0,cry_sinf(m_io.stats_bobCycle*gf_PI*4.0f)*kBobHeight*speedMul);
						
			//not the bob offset for the weapon
			bobDir *= 0.25f;
			//if player is strafing shift a bit the weapon on left/right
			if (speedMul > 0.01f)
			{
				// right vector dot speed vector
				float dot(m_io.viewQuatFinal.GetColumn0() * vSpeed);

				bobDir.x -= dot * kStrafeHorzScale;	// the faster we move right, the more the gun lags to the left and vice versa

				//tweak the right strafe for weapon laser
				if (dot>0.0f)
					weaponAngleOffset.z += dot * 1.5f;	// kStrafeHorzScale
				else
					weaponAngleOffset.z -= dot * 2.0f;	// kStrafeHorzScale

				weaponAngleOffset.y += dot * 5.0f;		// kStrafeHorzScale
			}
			//CryLogAlways("bobDir.z: %f", bobDir.z);
			if (bobDir.z < 0.0f)
			{
				bobDir.x *= 1.0f;
				bobDir.y *= 1.0f;
				bobDir.z *= 0.35f;
				speedMul *= 0.65f;
			}
			else
				bobDir.z *= 1.85f;

			//CryLogAlways("bobDir.z: %f after", bobDir.z);
			weaponOffset += m_io.viewQuatFinal * bobDir;
			weaponOffset -= m_io.baseQuat.GetColumn2() * 0.035f * speedMul;

			weaponAngleOffset.y += cry_sinf(m_io.stats_bobCycle*gf_PI*2.0f) * speedMul * -1.5f;
			if (crawling)
				weaponAngleOffset.y *= 3.0f;

			weaponAngleOffset.x += speedMul * 1.5f;
			if (crawling)
				weaponAngleOffset.z += cry_sinf(m_io.stats_bobCycle*gf_PI*2.0f) * speedMul * 3.0f;

			//FIXME: viewAngles must include all the view offsets, otherwise aiming wont be precise.
			angOffset.x += cry_sinf(m_io.stats_bobCycle*gf_PI*4.0f)*0.7f*speedMul;

			if (crawling)
			{
				angOffset.x *= 2.5f;
				angOffset.y += cry_sinf(m_io.stats_bobCycle*gf_PI*2.0f)*1.25f*speedMul;
				angOffset.z -= cry_sinf(m_io.stats_bobCycle*gf_PI*2.0f)*2.5f*speedMul;

			}
			else if (m_in.bSprinting)
			{
				angOffset.x *= 2.5f;
				angOffset.y += cry_sinf(m_io.stats_bobCycle*gf_PI*2.0f)*1.0f*speedMul;
				angOffset.z -= cry_sinf(m_io.stats_bobCycle*gf_PI*2.0f)*2.25f*speedMul;
			}
			else if(m_in.stance==STANCE_CROUCH && !weaponZoomed && !weaponZomming)
			{
				weaponOffset.z   += 0.035f;
				weaponOffset.y   -= m_io.viewQuatFinal.GetColumn1().y * 0.03f;
			}
			else if(m_in.stance==STANCE_CROUCH && weaponZomming)
			{
				weaponOffset.z	-= 0.07f;
				weaponOffset.y	+=  m_io.viewQuatFinal.GetColumn1().y * 0.06f;
			}
			else
			{
				//angOffset.x *= 2.25f;
				//angOffset.y += cry_sinf(m_io.stats_bobCycle*gf_PI*2.0f)*0.5f*speedMul;
				//angOffset.z -= cry_sinf(m_io.stats_bobCycle*gf_PI*2.0f)*1.125f*speedMul;
			}
		}
		else
		{
			m_io.stats_bobCycle = 0;

			//while flying offset a bit the weapon model by the player speed	
			if (m_in.stats_velocity.len2()>0.001f)
			{				
				float dotFwd(m_io.viewQuatFinal.GetColumn1() * vSpeed);
				float dotSide(m_io.viewQuatFinal.GetColumn0() * vSpeed);
				float dotUp(m_io.viewQuatFinal.GetColumn2() * vSpeed);

				weaponOffset += m_io.viewQuatFinal * Vec3(dotSide * -0.05f,dotFwd * -0.035f,dotUp * -0.05f);

				weaponAngleOffset.x += dotUp * 2.0f;
				weaponAngleOffset.y += dotSide * 5.0f;
				weaponAngleOffset.z -= dotSide * 2.0f;
			}
		}

		//add some inertia to weapon due view direction change.
		float deltaDotSide(m_io.vFPWeaponLastDirVec * m_io.viewQuatFinal.GetColumn0());
		float deltaDotUp(m_io.vFPWeaponLastDirVec * m_io.viewQuatFinal.GetColumn2());

		weaponOffset += m_io.viewQuatFinal * Vec3(deltaDotSide * 0.1f + m_in.stats_leanAmount * 0.05f,0,deltaDotUp * 0.1f - fabs(m_in.stats_leanAmount) * 0.05f) * m_in.params_weaponInertiaMultiplier;

		weaponAngleOffset.x -= deltaDotUp * 5.0f * m_in.params_weaponInertiaMultiplier;
		weaponAngleOffset.z += deltaDotSide * 5.0f * m_in.params_weaponInertiaMultiplier;
		weaponAngleOffset.y += deltaDotSide * 5.0f * m_in.params_weaponInertiaMultiplier;

		if(m_in.stats_leanAmount<0.0f)
			weaponAngleOffset.y += m_in.stats_leanAmount * 5.0f;

		//the weapon model tries to stay parallel to the terrain when the player is freefalling/parachuting

		if (m_in.stats_inWater > 0.0f)
			weaponOffset -= m_io.viewQuat.GetColumn2() * 0.15f;

		if (m_in.stats_inWater>0.1f && !m_in.stats_headUnderWater)
		{
			Ang3 offset(m_io.viewQuatFinal);
			offset.z = 0;
			if (offset.x<0.0f)
				offset.x = 0;

			weaponAngleOffset -= offset*(180.0f/gf_PI)*0.75f;
		}
		else if (m_io.stats_inFreefall)
		{
			Ang3 offset(m_io.viewQuatFinal);
			offset.z = 0;

			weaponAngleOffset -= offset*(180.0f/gf_PI)*0.5f;
		}
		//same thing with crawling
		else if (crawling)
		{
			//FIXME:to optimize, looks like a bit too expensive
			Vec3 forward(m_io.viewQuatFinal.GetColumn1());
			Vec3 up(m_io.baseQuat.GetColumn2());
			Vec3 right(-(up % forward));

			Matrix33 mat;
			mat.SetFromVectors(right,up%right,up);
			mat.OrthonormalizeFast();

			Ang3 offset(m_io.viewQuatFinal.GetInverted() * Quat(mat));

			weaponAngleOffset += offset*(180.0f/gf_PI)*0.5f;

			float lookDown(m_io.viewQuatFinal.GetColumn1() * m_io.baseQuat.GetColumn2());
			weaponOffset += m_io.baseQuat * Vec3(0,-0.5f*max(-lookDown,0.0f),-0.05f);

			float scale = 0.5f;;
			if(weaponAngleOffset.x>0.0f)
			{
				scale = min(0.5f,weaponAngleOffset.x/15.0f);
				weaponAngleOffset.x *= scale;
			}
			else
			{
				scale = min(0.5f,-weaponAngleOffset.x/20.0f);
				weaponAngleOffset *= (1.0f-scale);
				weaponOffset *= scale;
			}
			//if(vSpeedLen>0.1f)
				//weaponAngleOffset += Ang3(-8.0f,0,-12.5f);
			
		}
		else if (m_in.bSprinting && vSpeedLen>0.5f)
		{
			weaponAngleOffset += Ang3(-20.0f,0,10.0f);
			weaponOffset += m_io.viewQuatFinal * Vec3(0.0f, -.01f, .1f);
		}
		else if (m_in.bLookingAtFriendlyAI && !weaponZomming && !weaponZoomed)
		{
			weaponAngleOffset += Ang3(-15.0f,0,8.0f);
			weaponOffset += m_io.viewQuatFinal * Vec3(0.0f, -.01f, .05f);
		}

		//apply some multipliers
		weaponOffset *= m_in.params_weaponBobbingMultiplier;
		angOffset *= m_io.bobMul * 0.25f;
		if (m_io.bobMul*m_io.bobMul!=1.0f)
		{
			weaponOffset *= m_io.bobMul;
			weaponAngleOffset *= m_io.bobMul;
		}

		float bobSpeedMult(1.0f);
		if(m_in.stats_inWater>0.1)
			bobSpeedMult = 0.75f;
//		m_io.viewQuatForWeapon *= Quat::CreateRotationXYZ(Ang3(rx,ry,rz));
		
		Interpolate(m_io.vFPWeaponOffset,weaponOffset,3.95f*bobSpeedMult,m_in.frameTime);
		Interpolate(m_io.vFPWeaponAngleOffset,weaponAngleOffset,10.0f*bobSpeedMult,m_in.frameTime);
		Interpolate(m_io.vFPWeaponLastDirVec,m_io.viewQuatFinal.GetColumn1(),5.0f*bobSpeedMult,m_in.frameTime);

		Interpolate(m_io.angleOffset,angOffset,10.0f,m_in.frameTime,0.002f);
		if(weaponZomming)
		{
			m_io.vFPWeaponLastDirVec = m_io.viewQuatFinal.GetColumn1();
			m_io.vFPWeaponOffset.Set(0.0f,0.0f,0.0f);
			m_io.vFPWeaponAngleOffset.Set(0.0f,0.0f,0.0f);
			m_io.bobOffset.Set(0.0f,0.0f,0.0f);
		}

		if (m_in.bSprinting)
		{
			float headBobScale = (m_in.stats_flatSpeed / m_in.standSpeed);
			headBobScale = min(1.0f, headBobScale);

			m_io.bobOffset = m_io.vFPWeaponOffset * 2.5f * g_pGameCVars->cl_headBob * headBobScale;
			float bobLenSq = m_io.bobOffset.GetLengthSquared();
			float bobLenLimit = g_pGameCVars->cl_headBobLimit;
			if (bobLenSq > bobLenLimit*bobLenLimit)
			{
				float bobLen = sqrt_tpl(bobLenSq);
				m_io.bobOffset *= bobLenLimit/bobLen;
			}
			viewParams.position += m_io.bobOffset;
		}
}

void CPlayerView::ViewVehicle(SViewParams &viewParams)
{
	if (m_in.pVehicle)
	{
		m_in.pVehicle->UpdateView(viewParams, m_in.entityId);
		viewParams.viewID = 2;		
	}
}

void CPlayerView::ViewSpectatorTarget(SViewParams &viewParams)
{
	CActor* pTarget = (CActor*)g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_in.stats_spectatorTarget);
	if(!pTarget)
		return;

	IVehicle* pVehicle = pTarget->GetLinkedVehicle();

	static float defaultOffset = 0.3f;
	static float viewHeight = 1.8f;

	Matrix34 worldTM = pTarget->GetEntity()->GetWorldTM();
	Vec3 worldPos = worldTM.GetTranslation();
	if(!pVehicle)
	{
		const SStanceInfo* stanceInfo = pTarget->GetStanceInfo(pTarget->GetStance());
		if(stanceInfo)
		{
			Interpolate(viewHeight, stanceInfo->viewOffset.z, 5.0f, viewParams.frameTime);
			worldPos.z += viewHeight + defaultOffset;
		}
		else
		{
			worldPos.z += 1.8f;
		}
	}
	else
	{
		// use vehicle pos/ori
		worldTM = pVehicle->GetEntity()->GetWorldTM();
		worldPos = pVehicle->GetEntity()->GetWorldPos();
		worldPos.z += 1.5f;
	}
	
	Ang3 worldAngles = Ang3::GetAnglesXYZ(Matrix33(worldTM));
	float distance = 3;

	// if freelook allowed, get orientation and distance from player entity
	if(g_pGameCVars->g_spectate_FixedOrientation == 0)
	{
		CPlayer* pThisPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_in.entityId));
		if(!pThisPlayer)
			return;
		Matrix34 ownOrientation = pThisPlayer->GetEntity()->GetWorldTM();

		worldAngles += Ang3::GetAnglesXYZ(Matrix33(ownOrientation));
		distance = pThisPlayer->GetSpectatorZoom();
	}

	if(pVehicle)
	{
		distance *= 4.0f;

		// air vehicles need bigger distance
		if(pVehicle->GetMovement() && pVehicle->GetMovement()->GetMovementType() == IVehicleMovement::eVMT_Air)
			distance *= 2.0f;
	}

	Vec3 goal;
	goal.x = distance * cos(worldAngles.z + gf_PI*1.5f) + worldPos.x;
	goal.y = distance * sin(worldAngles.z - gf_PI/2.0f) + worldPos.y;

	AABB targetBounds;
	pTarget->GetEntity()->GetLocalBounds(targetBounds);
	goal.z = targetBounds.max.z;
	float offset = defaultOffset;
	if(pVehicle)
	{
		if(pVehicle->GetMovement() && pVehicle->GetMovement()->GetMovementType() == IVehicleMovement::eVMT_Air)
			offset = 3.0f;
		else
			offset = 1.0f;
	}
	goal.z += pTarget->GetEntity()->GetWorldPos().z + offset;

	// store / interpolate the offset, not the world pos (reduces percieved jitter in vehicles)
	static Vec3 viewOffset(goal-worldPos);
	static Vec3 camPos(goal);
	static Vec3 entPos(worldPos);
	static EntityId lastSpectatorTarget(m_in.stats_spectatorTarget);

	// do a ray cast to check for camera intersection
	static ray_hit hit;	
	IPhysicalEntity* pSkipEntities[10];
	int nSkip = 0;
	if(pVehicle)
	{
		// vehicle drivers don't seem to have current items, so need to add the vehicle itself here
		nSkip = pVehicle->GetSkipEntities(pSkipEntities, 10);
	}
	else
	{
		IItem* pItem = pTarget->GetCurrentItem();
		if (pItem)
		{
			CWeapon* pWeapon = (CWeapon*)pItem->GetIWeapon();
			if (pWeapon)
				nSkip = CSingle::GetSkipEntities(pWeapon, pSkipEntities, 10);
		}
	}

	static float minDist = 0.4f;	// how close we're allowed to get to the target
	static float wallSafeDistance = 0.3f; // how far to keep camera from walls

	Vec3 dir = goal - worldPos;
	primitives::sphere sphere;
	sphere.center = worldPos;
	sphere.r = wallSafeDistance;

	geom_contact *pContact = 0;          
	float hitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, dir, ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid,
		&pContact, 0, (geom_colltype_player<<rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEntities, nSkip);

	// even when we have contact, keep the camera the same height above the target
	float minHeightDiff = dir.z;

	if(hitDist > 0 && pContact)
	{
		goal = worldPos + (hitDist * dir.GetNormalizedSafe());
		
		if(goal.z - worldPos.z < minHeightDiff)
		{
			// can't move the camera far enough away from the player in this direction. Try moving it directly up a bit
			int numHits = 0;
			sphere.center = goal;

			// (move back just slightly to avoid colliding with the wall we've already found...)
			sphere.center -= dir.GetNormalizedSafe() * 0.05f;

			float newHitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, Vec3(0,0,minHeightDiff), ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid,
				&pContact, 0, (geom_colltype_player<<rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEntities, nSkip);

			float raiseDist = minHeightDiff - (goal.z - worldPos.z) - wallSafeDistance;
			if(newHitDist != 0)
			{
				raiseDist = MIN(minHeightDiff, newHitDist);
			}
			
			raiseDist = MAX(0.0f, raiseDist);
			
			goal.z += raiseDist;
			worldPos.z += raiseDist*0.8f;
		}
	}

	int thisFrameId = gEnv->pRenderer->GetFrameID();
	static int frameNo(thisFrameId);
	if(thisFrameId - frameNo > 5)
	{
		// reset positions
		viewOffset = goal - worldPos;
		entPos = worldPos;
		camPos = goal;
	}
	if(lastSpectatorTarget != m_in.stats_spectatorTarget)
	{
		viewOffset = goal - worldPos;
		entPos = worldPos;
		camPos = goal;
		lastSpectatorTarget = m_in.stats_spectatorTarget;
	}
	frameNo = thisFrameId;

	static float interpSpeed = 5.0f;
	static float interpSpeed2 = 5.0f;
	static float interpSpeed3 = 8.0f;

	if(pVehicle)
	{
		Interpolate(viewOffset, goal-worldPos, interpSpeed, viewParams.frameTime);
		entPos = worldPos;
		viewParams.position = worldPos + viewOffset;
		camPos = viewParams.position;
	}
	else
	{
		Vec3 camPosChange = goal - camPos;
		Vec3 entPosChange = worldPos - entPos;

		if(camPosChange.GetLengthSquared() > 100.0f)
			camPos = goal;
		if(entPosChange.GetLengthSquared() > 100.0f)
			entPos = worldPos;

		Interpolate(camPos, goal, interpSpeed2, viewParams.frameTime);
		Interpolate(entPos, worldPos, interpSpeed3, viewParams.frameTime);
		viewParams.position = camPos;
	}

	Matrix33 rotation = Matrix33::CreateRotationVDir((entPos - viewParams.position).GetNormalizedSafe());
	viewParams.rotation = GetQuatFromMat33(rotation);	
	m_io.bUsePivot = true;
	m_io.stats_bobCycle = 0.0;
}

void CPlayerView::ViewDeathCamTarget(SViewParams &viewParams)
{
	CActor* pTarget = (CActor*)g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_in.stats_spectatorTarget);
	if(!pTarget)
		return;

	Matrix34 targetWorldTM = pTarget->GetEntity()->GetWorldTM();

	Vec3 camPos = viewParams.position;
	static float offset = 1.5f;
	camPos.z += offset;

	float heightOffset = 1.5f;
	const SStanceInfo* pSI = pTarget->GetStanceInfo(pTarget->GetStance());
	if(pSI)
	{
		heightOffset = pSI->viewOffset.z;
	}

	Vec3 targetPos = targetWorldTM.GetTranslation();
	targetPos.z += heightOffset;

	int thisFrameId = gEnv->pRenderer->GetFrameID();
	static int frameNo(thisFrameId);
	static Vec3 oldCamPos(camPos);
	static Vec3 oldTargetPos(targetPos);
	static EntityId lastSpectatorTarget(m_in.stats_spectatorTarget);
	static float oldFOVScale(1.0f);

	// if more than a few frames have passed since our last update, invalidate the positions
	if(thisFrameId - frameNo > 5)
	{
		oldCamPos = viewParams.position;	// interpolate from current camera pos
		oldTargetPos = targetPos;
		oldFOVScale = 1.0f;
	}
	// if target changed, reset positions
	if(lastSpectatorTarget != m_in.stats_spectatorTarget)
	{
		oldCamPos = camPos;
		oldTargetPos = targetPos;
		lastSpectatorTarget = m_in.stats_spectatorTarget;
		oldFOVScale = 1.0f;
	}
	frameNo = thisFrameId;

	// slight zoom after 2s
	float timeNow = gEnv->pTimer->GetCurrTime();
	float distSq = (targetPos - camPos).GetLengthSquared();
	float scale = 1.0f;
	if(timeNow - m_in.deathTime > 1.0f && distSq > 2500.0f)
	{
		// 1.0f at 50m, 0.3f at 100m+
		scale = 1.0f - (distSq - 2500.0f)/25000.0f;
		scale = CLAMP(scale, 0.3f, 1.0f);
	}

	Interpolate(oldCamPos, camPos, 5.0f, viewParams.frameTime);
	Interpolate(oldTargetPos, targetPos, 5.0f, viewParams.frameTime);
	Interpolate(oldFOVScale, scale, 0.5f, viewParams.frameTime);
	
	viewParams.position = oldCamPos;
	Vec3 dir = (oldTargetPos - oldCamPos).GetNormalizedSafe();
	Matrix33 rotation = Matrix33::CreateRotationVDir(dir);	
	dir.z = 0.0f;

	// quick ray check to make sure there's not a wall in the way...
	IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_in.entityId);
	if (pActor)
	{
		static ray_hit hit;	
		IPhysicalEntity* pSkipEntities[10];
		int nSkip = 0;
		IItem* pItem = pActor->GetCurrentItem();
		if (pItem)
		{
			CWeapon* pWeapon = (CWeapon*)pItem->GetIWeapon();
			if (pWeapon)
				nSkip = CSingle::GetSkipEntities(pWeapon, pSkipEntities, 10);
		}

		if (gEnv->pPhysicalWorld->RayWorldIntersection(viewParams.position, -dir, ent_static|ent_terrain|ent_rigid,
			rwi_ignore_noncolliding | rwi_stop_at_pierceable, &hit, 1, pSkipEntities, nSkip))
		{
			dir.zero();
		}
	}

	viewParams.position -= dir;

	viewParams.fov = m_in.defaultFov*oldFOVScale*(gf_PI/180.0f);;
	viewParams.rotation = GetQuatFromMat33(rotation);	
	m_io.bUsePivot = true;
	m_io.stats_bobCycle = 0.0;
}

//FIXME:use the animated character view filter for this
void CPlayerView::ViewFollowCharacterFirstPerson(SViewParams &viewParams)
{
	viewParams.viewID = 3;

	//to avoid clipping
	viewParams.nearplane = 0.1f;
	viewParams.position = m_in.entityWorldMatrix *m_in.localEyePos;

	if (m_in.stats_isRagDoll)
		viewParams.position.z += 0.05f;

	if (m_in.stats_followCharacterHead)
		viewParams.rotation = Quat(Matrix33(m_in.entityWorldMatrix)*m_in.headMtxLocal*Matrix33::CreateRotationY(gf_PI*0.5f));
}

// View was set from an external source such as a vehicle, or followCharacterHead, so update the players rotation to match.
void CPlayerView::ViewExternalControlPostProcess(CPlayer &rPlayer,SViewParams &viewParams)
{
	// update the player rotation if view control is taken from somewhere else
	if (m_in.health>0 && (/*m_in.pVehicle || */m_in.stats_followCharacterHead))
	{
		Vec3 forward = viewParams.rotation.GetColumn1().GetNormalizedSafe();
		Vec3 up = Vec3(0,0,1);

		m_io.baseQuat = viewParams.rotation;//Quat(Matrix33::CreateFromVectors(forward % up,forward,up));
		//m_baseQuat.NoScale();

		m_io.viewQuat = m_io.viewQuatFinal = viewParams.rotation;
		//m_input.viewVector = viewParams.rotation.GetColumn1();
	}
}

void CPlayerView::ViewFirstPersonOnLadder(SViewParams & viewParams)
{

	viewParams.viewID = 3;

	viewParams.nearplane = 0.12f;
	viewParams.position = m_in.entityWorldMatrix * (m_in.localEyePos+Vec3(0.02f,0.0f,0.05f));// * (m_in.localEyePos+Vec3(0.1f,0.0f,0.0f));

	//Different camera angles for debugging
	if(g_pGameCVars->pl_debug_ladders!=0)
		viewParams.position = m_in.entityWorldMatrix * (m_in.localEyePos + Vec3(0.1f,-g_pGameCVars->cl_tpvDist,-0.3f));

	
	m_io.viewQuat = m_io.viewQuatFinal = viewParams.rotation;

	if(g_pGameCVars->g_detachCamera!=0)
	{
		viewParams.position = m_in.lastPos;
		viewParams.rotation = m_in.lastQuat;
	}

}

// Position the first person weapons
void CPlayerView::FirstPersonWeaponPostProcess(CPlayer &rPlayer,SViewParams &viewParams)
{
	rPlayer.m_stats.FPWeaponPosOffset = m_io.vFPWeaponOffset;
	rPlayer.m_stats.FPWeaponAnglesOffset = m_io.vFPWeaponAngleOffset;

	rPlayer.m_stats.FPSecWeaponPosOffset = m_io.vFPWeaponOffset;
	rPlayer.m_stats.FPSecWeaponAnglesOffset = m_io.vFPWeaponAngleOffset;
}

// Shake the view as requested
void CPlayerView::ViewShakePostProcess(CPlayer &rPlayer,SViewParams &viewParams)
{
	if (rPlayer.m_stats.inFreefall.Value())
	{
		IView *pView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetViewByEntityId(rPlayer.GetEntityId());
		if (pView)
		{
			float dot(0.1f * min(19.0f,5.0f + max(0.0f,rPlayer.m_stats.velocity*viewParams.rotation.GetColumn1())));
			if (rPlayer.m_stats.inFreefall.Value()==2)
				dot *= 0.5f;

			//float white[4] = {1,1,1,1};
			//gEnv->pRenderer->Draw2dLabel( 100, 50, 2, white, false, "dot:%f vel:%f", dot, rPlayer.m_stats.velocity.len() );
			
			pView->SetViewShake(Ang3(0.0005f,0.001f,0.0005f)*dot,Vec3(0.003f,0.0f,0.003f)*dot,0.3f,0.015f,1.0f,2);
		}
	}

#if 0
	if (m_io.viewShake.amount>0.001f)
	{
		viewParams.shakeDuration = m_io.viewShake.duration;
		viewParams.shakeFrequency = m_io.viewShake.frequency;
		viewParams.shakeAmount = m_io.viewShake.angle;
		viewParams.shakeID = 1;

		memset(&m_io.viewShake,0,sizeof(SViewShake));
	}
	/*else if (IsZeroG() && m_stats.flatSpeed > 10.0f)
	{
	viewParams.shakeDuration = 0.1f;
	viewParams.shakeFrequency = 0.1f;
	viewParams.shakeAmount = Ang3(m_stats.flatSpeed*0.001f,0,0);
	}*/
	else if (m_in.stats_inAir < 0.1f && m_in.stats_flatSpeed > m_in.stand_MaxSpeed * 1.1f)
	{
		viewParams.shakeDuration = 0.1f ;/// (shake*shake);
		viewParams.shakeFrequency = 0.1f ;/// (shake*shake);
		viewParams.shakeAmount = Ang3(m_in.stats_flatSpeed*0.0015f,m_in.stats_flatSpeed*0.00035f,m_in.stats_flatSpeed*0.00035f) * m_in.shake;
		viewParams.shakeID = 0;
	}
#endif
	//testing code
	/*if (m_stats.inAir < 0.1f && m_stats.flatSpeed > GetStanceInfo(STANCE_STAND)->maxSpeed * 1.1f)
	{
	float shake(g_pGameCVars->cl_sprintShake);

	IView *pView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetViewByEntityId(GetEntityId());
	if (pView)
	pView->SetViewShake(ZERO,Vec3(m_stats.flatSpeed*0.0035f,0,m_stats.flatSpeed*0.0035f) * shake,0.1f,0.05f,0.5f,1);
	}*/
}

// If there are first person hands present, then position and orient them relative to the view
void CPlayerView::HandsPostProcess(CPlayer &rPlayer,SViewParams &viewParams)
{
	//for testing sake
	float nearPlaneOverride(g_pGameCVars->cl_nearPlane);
	if (nearPlaneOverride > 0.001f)
		viewParams.nearplane = nearPlaneOverride;

	//FIXME:find a better place?
	// Convention: if Player has a renderable object in slot 2 then it's the first person hands.
	if (rPlayer.GetEntity()->GetSlotFlags(2) & ENTITY_SLOT_RENDER)
	{
		Matrix34 handsMtx(rPlayer.m_viewQuatFinal * Quat::CreateRotationZ(gf_PI));
		handsMtx.SetTranslation(viewParams.position);

		rPlayer.GetEntity()->SetSlotLocalTM(2,rPlayer.GetEntity()->GetWorldTM().GetInverted() * handsMtx);
		ICharacterInstance *pHands = rPlayer.GetEntity()->GetCharacter(2);
		if (pHands)
		{
			QuatT renderLocation = QuatT(handsMtx);
			pHands->GetISkeletonPose()->SetForceSkeletonUpdate(5);
			pHands->SkeletonPreProcess(renderLocation, renderLocation, GetISystem()->GetViewCamera(),0x55);
			pHands->SkeletonPostProcess(renderLocation, renderLocation, 0, 0.0f, 0x55);
		}
	}
}
