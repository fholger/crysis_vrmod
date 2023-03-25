/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 27:4:2004: Created by Filippo De Luca

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include "Hunter.h"
#include "GameUtils.h"

#include <IViewSystem.h>
#include <IItemSystem.h>
#include <IPhysics.h>
#include <ICryAnimation.h>
#include <ISerialize.h>
#include <IRenderAuxGeom.h>
#include <IMaterialEffects.h>


const float CHunter::s_turnThreshIdling = cry_cosf (DEG2RAD (30.0f));
const float CHunter::s_turnThreshTurning = cry_cosf (DEG2RAD (5.0f));


//Just for getting water surface id
#include "Bullet.h"

class CHunterBlending : public IAnimationBlending
{
public:
	CHunterBlending()
	{
	}

	~CHunterBlending()
	{
	}

	virtual SAnimationBlendingParams *Update(IEntity *pIEntity, Vec3 DesiredBodyDirection, Vec3 DesiredMoveDirection, f32 fDesiredMovingSpeed)
	{
		m_params.m_yawAngle = 0; 
		m_params.m_speed = 0; 
		m_params.m_strafeParam = 0; 
		m_params.m_turnAngle = 0;
		m_params.m_diffBodyMove = 0;
		m_params.m_fBlendedDesiredSpeed = 0;

		ICharacterInstance* pCharacter = pIEntity->GetCharacter(0);
		if (!pCharacter)
			return &m_params;

		Quat rot = pIEntity->GetRotation();
		Vec3 UpVector = rot.GetColumn2();
		//Vec3 pos = pEnt->GetWorldPos();

		//changed by ivo: most likely this doesn't work any more
//		Vec3 absCurrentBodyDirection(rot * pCharacter->GetISkeleton()->GetCurrentBodyDirection());
		Vec3 absCurrentBodyDirection(rot * Vec3(0,1,0));

		Vec3 absCurrentMoveDirection(rot * pCharacter->GetISkeletonAnim()->GetCurrentVelocity().GetNormalizedSafe(Vec3(0,1,0)));

		//body-radiant 
		Vec3 bforward(absCurrentBodyDirection.GetNormalized());
		Vec3 bup(UpVector);
		Vec3 bright(bforward % bup);
		//Matrix33 currBodyDirectionMatrix(Matrix33::CreateFromVectors(bforward, bup%bforward, bup));	
		Matrix33 currBodyDirectionMatrix(Matrix33::CreateFromVectors(bforward%bup,bforward,bup));	
		Vec3 newBodyDirLocal(currBodyDirectionMatrix.GetInverted() * DesiredBodyDirection);
		f32 body_radiant = cry_atan2f(-newBodyDirLocal.x,newBodyDirLocal.y);
		if (fabsf(body_radiant)<0.01f)
			body_radiant = 0;

		m_params.m_yawAngle = 0.0f;//body_radiant;		
		m_params.m_turnAngle	=	max(-1.0f,min(1.0f,body_radiant*5.0f));

		return &m_params;
	}

protected:

	SAnimationBlendingParams m_params;
};

CHunter::CHunter ()
{
}

IGrabHandler *CHunter::CreateGrabHanlder()
{
	m_pGrabHandler = new CAnimatedGrabHandler(this);
	return m_pGrabHandler;
}

//////////////////////////////////////////////////////////////////////////
void CHunter::RagDollize( bool fallAndPlay )
{
	// Hunter do not ragdollize.
}

//////////////////////////////////////////////////////////////////////////
void CHunter::PostPhysicalize()
{
	CAlien::PostPhysicalize();

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	IPhysicalEntity *pPhysEnt = pCharacter?pCharacter->GetISkeletonPose()->GetCharacterPhysics(-1):NULL;

	if (pPhysEnt)
	{
		pe_params_pos pp;
		pp.iSimClass = 2;
		pPhysEnt->SetParams(&pp);

		pe_simulation_params ps;
		ps.mass = 0;
		pPhysEnt->SetParams(&ps);
	}

/*
	// GC 2007 : this is an unfortunate solution for a hunter physics collision bug [JAN]
	if (IPhysicalEntity *pPE = GetEntity()->GetPhysics())
	{
		pe_params_part params;
		params.ipart = 0;
		params.scale = 0;
		params.flagsAND = ~geom_colltype_player;
		pPE->SetParams(&params);
	}
*/
}

bool CHunter::CreateCodeEvent(SmartScriptTable &rTable)
{
	const char *event = NULL;
	rTable->GetValue("event",event);

	if (event && !strcmp(event,"IKLook"))
	{
		bool lastIKLook(m_IKLook);
		rTable->GetValue("activate",m_IKLook);

		return true;
	} else if (event && !strcmp(event,"dropObject"))
	{
		if (m_pGrabHandler)
			m_pGrabHandler->SetDrop(rTable);

		return true;
	}
	else
		return CActor::CreateCodeEvent(rTable);
}

void CHunter::ProcessEvent(SEntityEvent& event)
{
	switch (event.event)
	{
		case ENTITY_EVENT_UNHIDE:
		{
			ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
			IPhysicalEntity *pPhysEnt = pCharacter?pCharacter->GetISkeletonPose()->GetCharacterPhysics(-1):NULL;

			if (pPhysEnt)
			{
				pe_params_pos pp;
				pp.iSimClass = 2;
				pPhysEnt->SetParams(&pp);

				pe_simulation_params ps;
				ps.mass = 0;
				pPhysEnt->SetParams(&ps);
			}
			break;
		}
	}
	CAlien::ProcessEvent (event);
}

void CHunter::Revive(bool fromInit)
{
	CAlien::Revive(fromInit);

	memset(m_footGroundPos,0,sizeof(m_footGroundPos));
	memset(m_footGroundPosLast,0,sizeof(m_footGroundPosLast));
	memset(m_IKLimbIndex,-1,sizeof(m_IKLimbIndex));
	memset(m_footSoundTime,0,sizeof(m_footSoundTime));
  memset(m_footGroundSurface,0,sizeof(m_footGroundSurface));
  memset(m_footAttachments,0,sizeof(m_footAttachments));
	for (int i=0; i<4; ++i)
	{
		m_footTouchesGround[i] = false;
		m_footTouchesGroundSmooth[i] = 0.0f;
		m_footTouchesGroundSmoothRate[i] = 0.0f;
	}
	m_IKLook = false;

	m_smoothMovementVec.Set(0,0,0);
	m_balancePoint = GetEntity()->GetWorldPos();

	m_nextStopCheck = 0.0f;

	m_zDelta = 0.0f;

	m_turning = false;

	m_smoothZ = GetEntity()->GetWorldPos().z;

	m_zOffset = 0.0f;

	m_walkEventFlags.reset ();

	//FIXME:
	if (m_pAnimatedCharacter)
	{
		SAnimatedCharacterParams params = m_pAnimatedCharacter->GetParams();
		
		params.flags &= ~(eACF_NoTransRot2k | eACF_ConstrainDesiredSpeedToXY/* | eACF_NoLMErrorCorrection*/);
		params.flags |= eACF_ImmediateStance | eACF_ConstrainDesiredSpeedToXY | eACF_ZCoordinateFromPhysics/* | eACF_NoLMErrorCorrection*/;

		//if (!params.pAnimationBlending)
		//	params.pAnimationBlending = new CHunterBlending();

		m_pAnimatedCharacter->SetParams(params);
	}


	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	if (pCharacter)
	{
		pCharacter->SetFlags(pCharacter->GetFlags() | CS_FLAG_UPDATE_ALWAYS);

    //TODO:this is temporary: remove it once the anim sys supports keyframe events
    if (m_IKLimbIndex[0]<0)	m_IKLimbIndex[0] = GetIKLimbIndex("frontLeftTentacle");
    if (m_IKLimbIndex[1]<0)	m_IKLimbIndex[1] = GetIKLimbIndex("frontRightTentacle");
    if (m_IKLimbIndex[2]<0)	m_IKLimbIndex[2] = GetIKLimbIndex("backLeftTentacle");
    if (m_IKLimbIndex[3]<0)	m_IKLimbIndex[3] = GetIKLimbIndex("backRightTentacle");

    // create attachments for footstep fx
    IAttachmentManager* pAttMan = pCharacter->GetIAttachmentManager();                        
    for (int i=0; i<4; ++i)
    {
      if (m_IKLimbIndex[i] == -1) 
        continue;
      
      SIKLimb *pLimb = &m_IKLimbs[m_IKLimbIndex[i]];
      const char* boneName = pCharacter->GetISkeletonPose()->GetJointNameByID(pLimb->endBoneID);      
      if (boneName && boneName[0])
      {
        char attName[128];
        _snprintf(attName, sizeof(attName), "%s_effect_attach", boneName);
        attName[sizeof(attName)-1] = 0;

        IAttachment* pAttachment = pAttMan->GetInterfaceByName(attName);
        if (!pAttachment)              
          pAttachment = pAttMan->CreateAttachment(attName, CA_BONE, boneName);
        else
          pAttachment->ClearBinding();

        m_footAttachments[i] = pAttachment;

				//before we use CCD-IK the first time, we have to initialize the limp-position to the current FK-position
				pLimb->Update(GetEntity(),0.000001f);  
				Vec3 lPos(pLimb->lAnimPos.x,pLimb->lAnimPos.y,0);
				m_footGroundPos[i] = GetEntity()->GetSlotWorldTM(pLimb->characterSlot)*lPos;

			}

    }
	}
}

void CHunter::UpdateFiringDir(float frameTime)
{
	m_stats.fireDir = m_viewMtx.GetColumn(1);//Vec3::CreateSlerp(m_stats.fireDir,m_viewMtx.GetColumn(1),1.9f*frameTime);
}

void CHunter::ProcessRotation(float frameTime)
{
	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();

	if (!pPhysEnt)
		return; 

	if (frameTime > 0.1f)
		frameTime = 0.1f;

	float rotSpeed(0.3f);

	if (m_input.viewVector.len2()>0.0f)
	{
		// pvl Probably obsolete.
		m_eyeMtx.SetRotationVDir(m_input.viewVector.GetNormalizedSafe());
	}

	if (m_input.viewVector.len2()>0.0f)
	{
		// pvl The same as m_eyeMtx above
		m_viewMtx.SetRotationVDir(m_input.viewVector.GetNormalizedSafe());
	}

	// compute the turn speed
	// NOTE Sep 13, 2007: <pvl> I suspect the turn speed has no influence on the hunter
	Vec3 forward(m_viewMtx.GetColumn(1));

	// pvl Check if the hunter is looking directly up or down.	
	if (Vec3(forward.x,forward.y,0).len2()>0.001f)
	{
		//force the up vector to be 0,0,1 for now
		Vec3 up(0,0,1);
		Vec3 right = (up % forward).GetNormalized();

		Quat goalQuat(Matrix33::CreateFromVectors(right % up,right,up)*Matrix33::CreateRotationZ(gf_PI*-0.5f));
		Quat currQuat(m_baseMtx);

		float rotSpeed = m_params.rotSpeed_min + (1.0f - (max(GetStanceInfo( m_stance )->maxSpeed - max(m_stats.speed - m_params.speed_min,0.0f),0.0f) / GetStanceInfo( m_stance )->maxSpeed)) * (m_params.rotSpeed_max - m_params.rotSpeed_min);
		Interpolate(m_turnSpeed,rotSpeed,3.0f/100.0f,frameTime);
	}
}

void CHunter::ProcessMovement(float frameTime)
{
	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();

	if (!pPhysEnt)
		return;

	if (frameTime > 0.1f)
		frameTime = 0.1f;

	//movement
	Vec3 reqMove;
	float	reqSpeed, maxSpeed;
	GetMovementVector(reqMove, reqSpeed, maxSpeed);

	Interpolate(m_smoothMovementVec,reqMove,1.0f,frameTime);

	Vec3 move(m_smoothMovementVec);//usually 0, except AIs
	move -= move * (m_baseMtx * Matrix33::CreateScale(Vec3(0,0,1)));//make it flat

	if (m_stats.sprintLeft)
		move *= m_params.sprintMultiplier;

	m_moveRequest.velocity = move;
	m_moveRequest.type = eCMT_Normal;
	//m_moveRequest.turn = GetEntity()->GetRotation().GetInverted() * Quat(m_baseMtx);
	
	Vec3 viewFwd(m_viewMtx.GetColumn1() + m_moveRequest.velocity * 10);
	viewFwd.z = 0.0f;
	viewFwd.Normalize();

	Vec3 entFwd(GetEntity()->GetRotation().GetColumn1());
	entFwd.z = 0.0f;
	entFwd.Normalize();

	const float dot = entFwd | viewFwd;
	const float cross = entFwd.Cross(viewFwd).z;

/*
	char action[256];
	IAnimationGraphState::InputID actionID = m_pAnimatedCharacter->GetAnimationGraphState()->GetInputId("Action");
	m_pAnimatedCharacter->GetAnimationGraphState()->GetInput(actionID, action);
	bool bAlreadyTurning = (strcmp(action, "idle") != 0);
*/

	if (( ! m_turning && (dot < s_turnThreshIdling)) || (m_turning && (dot < s_turnThreshTurning)))
	{
		if (cross > 0.0f)
		{
			m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( "Action", "turnleft" );
			m_moveRequest.rotation.SetRotationZ(DEG2RAD(25) * frameTime);
		}
		else
		{
			m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( "Action", "turnright" );
			m_moveRequest.rotation.SetRotationZ(-DEG2RAD(25) * frameTime);
		}
		m_turning = true;
	}
	else
	{
		m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( "Action", "idle" );
		m_moveRequest.rotation.SetIdentity();
		m_turning = false;
	}

	m_moveRequest.prediction.nStates = 0;

	//FIXME:sometime
	m_stats.desiredSpeed = m_stats.speed;
}

void CHunter::ProcessAnimation(ICharacterInstance *pCharacter,float frameTime)
{
	GetISystem()->GetIRenderer()->GetIRenderAuxGeom()->SetRenderFlags( e_Def3DPublicRenderflags );
//	GetISystem()->GetIRenderer()->GetIRenderAuxGeom()->DrawAABB(AABB (Vec3 (0,0,0), Vec3 (0.5,0.5,0.5)), true, ColorB(255,0,0,255), eBBD_Faceted);

	if (!pCharacter)
		return;

	//update look ik
	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(lookTarget,0.25f,ColorB(255,255,0,255));
	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(GetEntity()->GetWorldPos(), ColorB(255,255,0,255), lookTarget, ColorB(255,255,0,255));
	bool lookIK = (!m_pGrabHandler || m_pGrabHandler->GetStats()->grabId<1);
	lookIK &= ! Airborne ();

	// FIXME Dez 20, 2006: <pvl> disabled lookIK temporarily
	pCharacter->GetISkeletonPose()->SetLookIK(lookIK,gf_PI*0.9f,m_stats.lookTargetSmooth);

	int feetOnGround(0);
	Vec3 balancePoint(0,0,0);
	float highestLegZ(0);
	float lowestLegZ(10000);

	for (int i=0;i<4;++i)
	{
		if (m_IKLimbIndex[i] == -1) continue;

		SIKLimb *pLimb = &m_IKLimbs[m_IKLimbIndex[i]];

		// NOTE Jul 31, 2007: <pvl> the second part of this condition is a guard against
		// spurious "tentacle down" animation events.  Those can occur in presence of
		// animation blending which can cause unpaired "down" events.  We ignore a "down"
		// event if the tentacle is down already.  If we didn't do so, position of the
		// tentacle would be recomputed now, most likely causing a snap.
		if (TentacleGoingDown (i) && TentacleInTheAir (i))
		{
			if ( ! Airborne ())
			{
				Vec3 lPos(pLimb->lAnimPos.x,pLimb->lAnimPos.y,0);

				// transform lPos to the world
				m_footGroundPos[i] = GetEntity()->GetSlotWorldTM(pLimb->characterSlot)*lPos;

				ray_hit hit;
				int rayFlags = (COLLISION_RAY_PIERCABILITY & rwi_pierceability_mask);
				if (GetISystem()->GetIPhysicalWorld()->RayWorldIntersection(m_footGroundPos[i] + Vec3(0,0,30.0f), Vec3(0,0,-60.0f), ent_terrain|ent_static|ent_water, rayFlags, &hit, 1))
        {
					//m_footGroundPos[i] = hit.pt;
					//fix problem with hunter tantacles
					if (i==0 ||i==1)
						m_footGroundPos[i] = Vec3(hit.pt.x,hit.pt.y,hit.pt.z+0.20f); //front tentacles
					else
						m_footGroundPos[i] = Vec3(hit.pt.x,hit.pt.y,hit.pt.z+0.60f); //back tentacles

          m_footGroundSurface[i] = hit.surface_idx;
        }
			}
		}
		if (TentacleGoingUp (i))
		{
			// don't play the sound too often
			if (m_footSoundTime[i]<0.001f)
			{
				CreateScriptEvent("footlift",i+1);
				m_footSoundTime[i] = 0.35f;
			}
      
			if ( ! Airborne ())
			{
        if (m_footAttachments[i])
        {          
          if (m_footGroundSurface[i] == 0)
          {
            // in case surface is queried the 1st time
            ray_hit hit;
            int rayFlags = (COLLISION_RAY_PIERCABILITY & rwi_pierceability_mask);
            Vec3 pos = GetEntity()->GetSlotWorldTM(pLimb->characterSlot)*pLimb->lAnimPos;
            if (gEnv->pPhysicalWorld->RayWorldIntersection(pos + Vec3(0,0,5.0f), Vec3(0,0,-15.0f), ent_terrain|ent_static|ent_water, rayFlags, &hit, 1))
              m_footGroundSurface[i] = hit.surface_idx;
          }

					PlayFootliftEffects (i);					
        }

				m_footGroundPos[i].Set(0,0,0);	// the limb is lifting, reset its ground pos so that IK doesn't act on it	
				m_footTouchesGround[i] = false;
			}
		}

		//if (Airborne ())
		//	m_footGroundPos[i].Set(0,0,0);

		// transform lAnimPos (which is in model local space) to the world space
		Vec3 vLimbPos = GetEntity()->GetSlotWorldTM(pLimb->characterSlot)*pLimb->lAnimPos;
		Vec3 delta(vLimbPos - m_footGroundPos[i]);	// check for IK stretching the limb too much
		delta.z = 0;

		//if (delta.len2()>/*10.0f*10.0f*/ 500.0f)
		//	m_footGroundPos[i].Set(0,0,0);// = limbPos;

		bool footOnGround=m_footGroundPos[i].len2()>0.01f;

		
//		SmoothCD(m_footTouchesGroundSmooth[i], m_footTouchesGroundSmoothRate[i], frameTime, f32(m_footTouchesGround[i]), 0.10f);
		if (m_footTouchesGround[i])
			SmoothCD(m_footTouchesGroundSmooth[i], m_footTouchesGroundSmoothRate[i], frameTime, f32(footOnGround), 0.002f);
		else
			SmoothCD(m_footTouchesGroundSmooth[i], m_footTouchesGroundSmoothRate[i], frameTime, f32(footOnGround), 0.10f);

		ray_hit hit;
		Vec3 checkFrom(vLimbPos);
		checkFrom.z += 30.0f;	// make sure the position from which we'll check is above the terrain
		Vec3 checkDir(0,0,-60.0f);
		int rayFlags = (COLLISION_RAY_PIERCABILITY & rwi_pierceability_mask);
		Vec3 vFinalLimbPos=vLimbPos;

		int32 intersection = GetISystem()->GetIPhysicalWorld()->RayWorldIntersection(checkFrom, checkDir, ent_terrain|ent_static, rayFlags, &hit, 1);
		if (intersection)
		{
			Vec3 hitInLocal(GetEntity()->GetSlotWorldTM(0).GetInverted() * hit.pt);
			vFinalLimbPos.z += hitInLocal.z;
		}

		Vec3 vFinalLimbPosition=vFinalLimbPos;
		if (m_footGroundPos[i].GetLength())
			m_footGroundPosLast[i]=m_footGroundPos[i];
		f32 t0=m_footTouchesGroundSmooth[i];
		f32 t1=1.0f-m_footTouchesGroundSmooth[i];

		f32 fDistance=(m_footGroundPosLast[i]-vFinalLimbPos).GetLength();
		if (fDistance<30.0f)
			vFinalLimbPosition = m_footGroundPosLast[i]*t0 + vFinalLimbPos*t1;

		//float fColor[4] = {1,1,0,1};
		//GetISystem()->GetIRenderer()->Draw2dLabel( 1,40+16*i, 1.3f, fColor, false,"leg: %f %f %f   OnGround: %d  fDistance:%f",m_footGroundPos[i].x,m_footGroundPos[i].y,m_footGroundPos[i].z, footOnGround,fDistance );	

		if (g_pGameCVars->h_useIK) 
			pLimb->SetWPos(GetEntity(),vFinalLimbPosition,Vec3(0,0,-1),1.0f,1.0f,100); // a call to IK

		if (g_pGameCVars->h_drawSlippers)
		{
			GetISystem()->GetIRenderer()->GetIRenderAuxGeom()->DrawSphere(vFinalLimbPosition,0.52f,ColorB(uint8(t0*255.0f),uint8(t1*255.0f),0,255));
			GetISystem()->GetIRenderer()->GetIRenderAuxGeom()->DrawSphere(m_footGroundPosLast[i],0.5f,ColorB(0,0,255,255));
		}



		if (footOnGround)
		{
			float distToGround = (pLimb->currentWPos - pLimb->goalWPos).len2();
			if (distToGround < 0.01f && ! m_footTouchesGround[i])
			{
				//GetISystem()->GetIRenderer()->GetIRenderAuxGeom()->DrawSphere(pLimb->currentWPos,1.0f,ColorB(0,0,255,255));
				CreateScriptEvent("footstep",i+1);
				m_footSoundTime[i] = 0.35f;
				PlayFootstepEffects (i);
				m_footTouchesGround[i] = true;
			}
		}
		
		



		if (footOnGround)
		{
			if (m_footGroundPos[i].z > highestLegZ)
				highestLegZ = m_footGroundPos[i].z;
			
			if (m_footGroundPos[i].z < lowestLegZ)
				lowestLegZ = m_footGroundPos[i].z;

			balancePoint = (balancePoint + m_footGroundPos[i]) * 0.5f;
			++feetOnGround;
		}

		m_footSoundTime[i] -= frameTime;

		//
		//GetISystem()->GetIRenderer()->GetIRenderAuxGeom()->DrawLine(GetEntity()->GetSlotWorldTM(0).GetTranslation(), ColorB(255,255,0,255), m_footGroundPos[i], ColorB(255,255,0,255));
	}

	//
	float wZ(GetEntity()->GetWorldPos().z);
	float zOffsetGoal(0.0f);

	// put the hunter's center of mass to height equal to average of heights of its highest and lowest
	// positioned tentacle
	if (highestLegZ>0.001f && lowestLegZ<9999.999f)
	{
		zOffsetGoal = (lowestLegZ + highestLegZ)*0.5f;
		zOffsetGoal = (zOffsetGoal + wZ)*0.5f;
		zOffsetGoal -= wZ;
	}

	Interpolate(m_smoothZ,wZ,1.0f,frameTime);
	Interpolate(m_zOffset,zOffsetGoal,1.0f,frameTime);

	m_modelOffsetAdd.z = (m_smoothZ - wZ) + m_zOffset;

	m_walkEventFlags.reset ();

  /*for (int i=0;i<4;++i)
  {
    if (m_footAttachments[i])
    {
      Matrix34 boneLocalTM(GetEntity()->GetCharacter(0)->GetISkeleton()->GetAbsJointByID(m_footAttachments[i]->GetBoneID()));
      Matrix34 tm = GetEntity()->GetSlotWorldTM(0) * boneLocalTM;
      gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(tm.GetTranslation(), 1.f, ColorB(0,255,0,255));
    }
  }*/
}

void CHunter::PlayFootstepEffects (int tentacle) const
{
	IMaterialEffects* pMaterialEffects = g_pGame->GetIGameFramework()->GetIMaterialEffects();

	float footWaterLevel = gEnv->p3DEngine->GetWaterLevel(&m_footGroundPos[tentacle]);

	TMFXEffectId effectId = InvalidEffectId;
	if(footWaterLevel>m_footGroundPos[tentacle].z)
		effectId = pMaterialEffects->GetEffectId("hunter_footstep", CBullet::GetWaterMaterialId());
	else
		effectId = pMaterialEffects->GetEffectId("hunter_footstep", m_footGroundSurface[tentacle]);
	if (effectId != InvalidEffectId)
	{
		SMFXRunTimeEffectParams fxparams;
		fxparams.pos = m_footGroundPos[tentacle];
		fxparams.soundSemantic = eSoundSemantic_Physics_Footstep;
		pMaterialEffects->ExecuteEffect(effectId, fxparams);                        
	}
}

void CHunter::PlayFootliftEffects (int tentacle) const
{
	IMaterialEffects* pMaterialEffects = g_pGame->GetIGameFramework()->GetIMaterialEffects();

	float footWaterLevel = gEnv->p3DEngine->GetWaterLevel(&m_footGroundPos[tentacle]);

	TMFXEffectId effectId = InvalidEffectId;
	if(footWaterLevel>m_footGroundPos[tentacle].z)
		effectId = pMaterialEffects->GetEffectId("hunter_footlift", CBullet::GetWaterMaterialId());
	else
		effectId = pMaterialEffects->GetEffectId("hunter_footlift", m_footGroundSurface[tentacle]);
	if (effectId != InvalidEffectId)
	{ 
		SMFXResourceListPtr pList = pMaterialEffects->GetResources(effectId);
		if (pList && pList->m_particleList)        
		{
			const char* effect = pList->m_particleList->m_particleParams.name;
			CEffectAttachment* pEffectAttachment = new CEffectAttachment(effect, Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_OneY, 1.f);
			pEffectAttachment->CreateEffect();
			m_footAttachments[tentacle]->AddBinding(pEffectAttachment);              
		}
	}
}

void CHunter::UpdateAnimGraph( IAnimationGraphState * pState )
{
	CAlien::UpdateAnimGraph(pState);
}

void CHunter::SetFiring(bool fire)
{
	if (fire != m_stats.isFiring)
	{
		if (!m_stats.isFiring)
			CreateScriptEvent("fireWeapon",0);

		m_stats.isFiring = fire;
	}
}

int CHunter::GetBoneID(int ID,int slot) const
{
	if (m_boneIDs[ID]<0)
	{
		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
		if (!pCharacter)
			return -1;

		char boneStr[64];
		switch(ID)
		{
		case BONE_HEAD:		strcpy(boneStr,"face_bigass_gun");break;
		case BONE_WEAPON: strcpy(boneStr,"face_bigass_gun");break;
		case BONE_EYE_R:	strcpy(boneStr,"face_bigass_gun");break;
		case BONE_EYE_L:	strcpy(boneStr,"face_bigass_gun");break;
		}

		m_boneIDs[ID] = pCharacter->GetISkeletonPose()->GetJointIDByName(boneStr);
	}

	return CActor::GetBoneID(ID,slot);
}

void CHunter::GetActorInfo(SBodyInfo& bodyInfo)
{
	CAlien::GetActorInfo(bodyInfo);

	int headBoneID = GetBoneID(BONE_HEAD);
	if (headBoneID>-1 && GetEntity()->GetCharacter(0))
	{
	//	Matrix33 HeadMat(GetEntity()->GetCharacter(0)->GetISkeleton()->GetAbsJMatrixByID(headBoneID));
		Matrix34 HeadMat( Matrix34(GetEntity()->GetCharacter(0)->GetISkeletonPose()->GetAbsJointByID(headBoneID)) );
		HeadMat = GetEntity()->GetSlotWorldTM(0) * HeadMat;
		bodyInfo.vFirePos = HeadMat.GetColumn3();
		bodyInfo.vFireDir = bodyInfo.vEyeDir = HeadMat.GetColumn(1);
	}

	bodyInfo.vFireDir = bodyInfo.vEyeDir = m_viewMtx.GetColumn(1);
	
	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(bodyInfo.vEyePos, ColorB(0,255,0,100), bodyInfo.vEyePos + bodyInfo.vEyeDir * 10.0f, ColorB(255,255,0,100));
}

bool CHunter::SetAnimationInput( const char * inputID, const char * value )
{
	ICharacterInstance * pCharacter = GetEntity()->GetCharacter(0);
	if (pCharacter && pCharacter->GetISkeletonAnim()->GetTrackViewStatus())
		return false;
	
	return CActor::SetAnimationInput(inputID,value);
}

void CHunter::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	GetAlienMemoryStatistics(s);
}

void CHunter::AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event)
{
	// NOTE Dez 19, 2006: <pvl> grabbing events
	if (strcmp (event.m_EventName, "ObjectGrabbed") == 0) {
		m_pGrabHandler->StartGrab ();
	} else if (strcmp (event.m_EventName, "ObjectThrown") == 0) {
		m_pGrabHandler->StartDrop ();
	} else if (strcmp (event.m_EventName, "StartIK") == 0) {
		(static_cast <CAnimatedGrabHandler*> (m_pGrabHandler))->ActivateIK ();

	// NOTE Dez 19, 2006: <pvl> walking events
	//
	// NOTE Jul 31, 2007: <pvl> doesn't feel right but works: we accept all "down"
	// events but ignore "up" events if they aren't sent by the "most recent"
	// animation in the anim queue.  That's either the only animation in the queue or
	// the animation that's bleding in (while all others are blending out).
	//
	// This, with addition of "down" events to the beginning of non-walking animations
	// for every tentacle that's down at the beginning of that animation, helps
	// to prevent situation where an anim being blended out issues an "up" event
	// but doesn't manage to send the matching "down".  This can leave the tentacle
	// in the air for an indefinite amount of time, causing sliding.
	} else if (strcmp (event.m_EventName, "FrontRightUp") == 0) {
		if (event.m_nAnimNumberInQueue == 0)
			m_walkEventFlags.set (FRONT_RIGHT_UP);
	} else if (strcmp (event.m_EventName, "FrontRightDown") == 0) {
		m_walkEventFlags.set (FRONT_RIGHT_DOWN);
	} else if (strcmp (event.m_EventName, "FrontLeftUp") == 0) {
		if (event.m_nAnimNumberInQueue == 0)
			m_walkEventFlags.set (FRONT_LEFT_UP);
	} else if (strcmp (event.m_EventName, "FrontLeftDown") == 0) {
		m_walkEventFlags.set (FRONT_LEFT_DOWN);
	} else if (strcmp (event.m_EventName, "BackLeftUp") == 0) {
		if (event.m_nAnimNumberInQueue == 0)
			m_walkEventFlags.set (BACK_LEFT_UP);
	} else if (strcmp (event.m_EventName, "BackLeftDown") == 0) {
		m_walkEventFlags.set (BACK_LEFT_DOWN);
	} else if (strcmp (event.m_EventName, "BackRightUp") == 0) {
		if (event.m_nAnimNumberInQueue == 0)
			m_walkEventFlags.set (BACK_RIGHT_UP);
	} else if (strcmp (event.m_EventName, "BackRightDown") == 0) {
		m_walkEventFlags.set (BACK_RIGHT_DOWN);
	} else
		CActor::AnimationEvent (pCharacter,event);
}

void CHunter::PlayAction( const char* action, const char* extension, bool looping )
{
	if (!m_pAnimatedCharacter)
		return;

	if (looping)
		m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( "Action", action );
	else
		m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( "Signal", action );
}

template <typename T>
static void SerializeArray (TSerialize ser, T * arr, int elemCount, const string & label)
{
	const int labelBufSize = label.length () + 32;
	char * labelBuf = new char [labelBufSize];

	string labelFmt (label);
	labelFmt += "%d";

	for (int i=0; i < elemCount; ++i)
	{
		_snprintf (labelBuf, labelBufSize, labelFmt.c_str (), i);
		ser.Value(labelBuf, arr[i]);
	}

	delete [] labelBuf;
}

void CHunter::FullSerialize( TSerialize ser )
{
	CAlien::FullSerialize(ser);

	ser.BeginGroup("CHunter");
	ser.Value("m_smoothMovementVec", m_smoothMovementVec);
	ser.Value("m_balancePoint", m_balancePoint);
	ser.Value("m_zDelta", m_zDelta);
	ser.Value("m_turning", m_turning);

	SerializeArray (ser, m_IKLimbIndex, 4, "IKLimbIndex");
	SerializeArray (ser, m_footGroundPos, 4, "footGroundPos");
	SerializeArray (ser, m_footGroundPosLast, 4, "footGroundPosLast");
	SerializeArray (ser, m_footGroundSurface, 4, "footGroundSurface");
	SerializeArray (ser, m_footTouchesGround, 4, "footTouchesGround");
	SerializeArray (ser, m_footTouchesGroundSmooth, 4, "footTouchesGroundSmooth");
	SerializeArray (ser, m_footTouchesGroundSmoothRate, 4, "footTouchesGroundSmoothRate");
	SerializeArray (ser, m_footSoundTime, 4, "footSoundTime");

	ser.Value("m_IKLook", m_IKLook);
	ser.Value("m_nextStopCheck", m_nextStopCheck);
	ser.Value("m_smoothZ", m_smoothZ);
	ser.Value("m_zOffset", m_zOffset);

	// NOTE Okt 6, 2007: <pvl> the explicit std::string/string casts in bitset
	// serialization are necessary because the engine plays some dirty tricks
	// with std::string (replacing it silently with something else) and
	// std::bitset requires std::string.
	if (ser.IsReading ())
	{
		string walkEventFlagsStr;
		ser.Value ("walkEventFlags", walkEventFlagsStr);
		m_walkEventFlags = std::bitset <NUM_WALK_EVENTS> (std::string (walkEventFlagsStr.c_str ()));
	}
	else
	{
		// NOTE Okt 6, 2007: <pvl> STLP implementation of bitset::to_string() seems
		// to be a bit lacking.  It seems necessary to explicitly list std::string's
		// template arguments at the call site (I think MS STL doesn't have the problem).
		ser.Value ("walkEventFlags", string (m_walkEventFlags.to_string<char, std::char_traits<char>, std::allocator<char> > ().c_str ()));
	}

	ser.EndGroup();
}
