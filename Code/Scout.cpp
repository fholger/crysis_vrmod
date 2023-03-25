/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 4:12:2005: Created by Filippo De Luca
*************************************************************************/

#include "StdAfx.h"
#include "Game.h"
#include "Scout.h"
#include "GameUtils.h"

#include <IViewSystem.h>
#include <IItemSystem.h>
#include <IPhysics.h>
#include <ICryAnimation.h>
#include <ISerialize.h>
#include <IRenderAuxGeom.h>
#include "Lam.h"


class CScoutBeam : public CBaseGrabHandler
{
public:

	typedef std::vector<IGrabHandler *> TScoutBeams;
	
public:

	CScoutBeam(CActor *pActor);
	~CScoutBeam();

	//
	virtual bool SetGrab(SmartScriptTable &rParams);
	
	//
	ILINE static int BeamNumByObject(EntityId grabId)
	{
		int num(0);
		for (int i=0;i<s_scoutBeams.size();++i)
			if (s_scoutBeams[i]->GetStats()->grabId == grabId)
				++num;

		return num;
	}
		
protected:

	virtual void UpdatePosVelRot(float frameTime);

public:

	char m_beamBone[128];

protected:

	float m_followDelay;
	float m_followDelayMax;

	float m_criticalDistance;

	//
	static TScoutBeams s_scoutBeams;
};


CScout::CScout() 
{
}

CScout::~CScout()
{
}

bool CScout::CreateCodeEvent(SmartScriptTable &rTable)
{
	const char *event = 0;
	if (!rTable->GetValue("event",event))
    return false;
  
	if (!strcmp(event,"spawnDebris"))
	{
		return true;
	} 
  else if (!strcmp(event,"dropObject"))
	{
		if (m_pGrabHandler)
			m_pGrabHandler->SetDrop(rTable);

		return true;
	}
  else if (!strcmp(event,"searchBeam"))
  {
    bool bEnable = false;
    if (!(rTable->GetValue("enable", bEnable)))
      return false;
    
    return EnableSearchBeam(bEnable);
  }
	else
		return CAlien::CreateCodeEvent(rTable);
}

bool CScout::EnableSearchBeam(bool enable)
{
  if (!(m_searchbeam.itemId && m_searchbeam.pAttachment))
    return false;

  m_searchbeam.active = enable;
  
  if (CLam* pLam = (CLam*)g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_searchbeam.itemId))
  {
    pLam->ActivateLight(enable, true);
    return true;
  }    

  return false;
}

IGrabHandler *CScout::CreateGrabHanlder()
{
//	m_pGrabHandler = new CScoutBeam(this);
//	m_pGrabHandler = new CAnimatedGrabHandler(this);
	m_pGrabHandler = new CMultipleGrabHandler(this);
	return m_pGrabHandler;
}

void CScout::Revive(bool fromInit)
{
	CAlien::Revive(fromInit);

	//FIXME:deactivate transrot2k until the assets gets fixed
	if (m_pAnimatedCharacter)
	{
		SAnimatedCharacterParams acFlags = m_pAnimatedCharacter->GetParams();
		acFlags.flags |= eACF_NoTransRot2k;

		m_pAnimatedCharacter->SetParams(acFlags);
	}

  // setup searchbeam
  static IEntityClass* pBeamClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("ScoutSearchBeam");
  m_searchbeam.itemId = GetInventory()->GetItemByClass(pBeamClass);  
  
  if (m_searchbeam.itemId)
  {
    if (!m_searchbeam.pAttachment)
    {
      if (ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0)) 
      {
        IAttachmentManager* pAttachMan = pCharacter->GetIAttachmentManager();
        m_searchbeam.pAttachment = pAttachMan->GetInterfaceByName("searchlight_attachment");

        if (!m_searchbeam.pAttachment)    
          m_searchbeam.pAttachment = pAttachMan->CreateAttachment("searchlight_attachment", CA_BONE, "weapon_bone");
      }
    } 

    if (m_searchbeam.pAttachment)
    {
      CEntityAttachment *pEntityAttachment = new CEntityAttachment();
      pEntityAttachment->SetEntityId(m_searchbeam.itemId);
      m_searchbeam.pAttachment->AddBinding(pEntityAttachment);
    }   
  }  
}

void CScout::UpdateGrab(float frameTime)
{
	CActor::UpdateGrab(frameTime);

	bool showBeam(m_pGrabHandler && m_pGrabHandler->GetStats()->grabId>0);

	if (showBeam && !m_pBeamEffect)
		m_pBeamEffect = new CAlienBeam(this);

	if (m_pBeamEffect)
	{
		if (m_pBeamEffect->IsActive() != showBeam)
		{
			if (showBeam)
			{
				//FIXME:that cast is quite bad
				m_pBeamEffect->Start(
					"Alien_Weapons.Freeze_Beam.beam_firemode1",
					m_pGrabHandler->GetStats()->grabId,
					Ang3(gf_PI*-0.25f,0,gf_PI),
					((CScoutBeam *)m_pGrabHandler)->m_beamBone
					);
			}
			else
			{
				m_pBeamEffect->Stop();
			}
		}

		m_pBeamEffect->Update(frameTime);
	}
}

void CScout::ProcessRotationNew(float frameTime)
{

}

void CScout::ProcessRotation(float frameTime)
{
	ProcessRotationNew(frameTime);
	return;
	//rotation
	//6 degree of freedom
	//FIXME:put mouse sensitivity here!
	//TODO:use radians
	float rotSpeed(0.3f);

//	if (m_stats.inAir && IsZeroG())
	{		

		// Mikko: Separated the look and movement directions. This is a workaround! The reason is below (moved from the SetActorMovement):
		// >> Danny - old code had desired direction using vLookDir but this caused spinning behaviour
		// >> when it was significantly different to vMoveDir

		if (m_input.viewVector.len2()>0.0f)
		{
			m_eyeMtx.SetRotationVDir(m_input.viewVector.GetNormalizedSafe());
		}

		if (m_input.viewVector.len2()>0.0f)
		{
			m_viewMtx.SetRotationVDir(m_input.viewVector.GetNormalizedSafe());
		}
		else
		{
			Ang3 desiredAngVel(m_input.deltaRotation.x * rotSpeed,0,m_input.deltaRotation.z * rotSpeed);
					
			//rollage
			if (m_input.actions & ACTION_LEANLEFT)
				desiredAngVel.y -= 10.0f * rotSpeed;
			if (m_input.actions & ACTION_LEANRIGHT)
				desiredAngVel.y += 10.0f * rotSpeed;

			Interpolate(m_angularVel,desiredAngVel,3.5f,frameTime);

			Matrix33 yawMtx;
			Matrix33 pitchMtx;
			Matrix33 rollMtx;

			//yaw
			yawMtx.SetRotationZ(m_angularVel.z * gf_PI/180.0f);
			//pitch
			pitchMtx.SetRotationX(m_angularVel.x * gf_PI/180.0f);
			//roll
			if (fabs(m_angularVel.y) > 0.001f)
				rollMtx.SetRotationY(m_angularVel.y * gf_PI/180.0f);
			else
				rollMtx.SetIdentity();
			//
			
			m_viewMtx = m_viewMtx * yawMtx * pitchMtx * rollMtx;
			m_viewMtx.OrthonormalizeFast();

		}
	}
}
void CScout::ProcessMovementNew(float frameTime)
{

	frameTime = min( 1.0f, frameTime );
	if ( m_params.forceView < 50.0 )
	{	// normal action

		// velocity/speed setting up
		m_desiredVelocity = m_input.movementVector;
		m_desiredVeloctyQuat = GetQuatFromMat33(m_viewMtx);
		m_velocity = m_stats.velocity;

		float desiredSpeed = m_desiredVelocity.GetLength();
		float fCurSpeed = m_stats.speed;
		float maxSpeed     = 30.0f;

		float cof1   = min( maxSpeed, fabsf( desiredSpeed - fCurSpeed ) );
		float cofVel  = ( cof1 > 0.0f ) ? min( frameTime*2.5f, 5.0f/(cof1*cof1) ) : frameTime*2.5f;
		float cofRot  = ( cof1 > 0.0f ) ? min( frameTime*2.5f, 2.5f/(cof1*cof1) ) : frameTime*2.5f;

		// velocity/speed interpolation
		Interpolate( m_velocity, m_desiredVelocity, 1.0f, cofVel );

		// pitch/roll
		if ( desiredSpeed > 0.0f && fCurSpeed > 0.0f )
		{

			Vec3 vUp( 0.0f, 0.0f, 1.0f );
			Vec3 vFwd = m_velocity.GetNormalized();
			Vec3 vNew = m_desiredVelocity.GetNormalized();

			if ( fabs( vFwd * vUp ) < cosf( DEG2RAD(3.0f) ) )
		{
				vFwd.z = 0;
				vFwd.NormalizeSafe();
				Vec3 vWng = vFwd.Cross( vUp );
				vWng.NormalizeSafe();

				float cofRoll = 6.0f * ( vWng * vNew ) * fCurSpeed / maxSpeed;
				float cofPitch =   ( vNew * vFwd ) * cof1 / maxSpeed;
				cofRoll =  max( -1.0f, min( 1.0f, cofRoll ) );
				cofPitch =  max( -1.0f, min( 1.0f, cofPitch ) );
				m_desiredVeloctyQuat *= Quat::CreateRotationY( DEG2RAD(60.0f) * cofRoll ); 
				m_desiredVeloctyQuat *= Quat::CreateRotationX( DEG2RAD(-60.0f) * cofPitch ); 
			}
		}

		// rotation interpolation
		Quat modelRot( GetEntity()->GetRotation() );
		modelRot = Quat::CreateSlerp( modelRot, m_desiredVeloctyQuat, cofRot );

		// commit the result interpolation
		assert(GetEntity()->GetRotation().IsValid());
		assert(GetEntity()->GetRotation().GetInverted().IsValid());
		assert(modelRot.IsValid());

		m_moveRequest.rotation = GetEntity()->GetRotation().GetInverted() * modelRot;
		m_moveRequest.rotation.Normalize();

		assert(m_moveRequest.rotation.IsValid());

		m_moveRequest.velocity = m_velocity;
		m_moveRequest.type = eCMT_Fly;
	}
	else if ( m_params.forceView < 101.0f )
	{	// for the emergency situation...set speed immidiately

		// velocity/speed setting up
		m_desiredVelocity = m_input.movementVector;
		m_velocity = m_stats.velocity;
		Interpolate( m_velocity, m_desiredVelocity, 1.0f, 0.25f );

		// just keep rotation angles
		Quat modelRot( GetEntity()->GetRotation() );

		// commit the result interpolation & set velocity immidiately
		m_moveRequest.rotation = GetEntity()->GetRotation().GetInverted() * modelRot;
    m_moveRequest.rotation.Normalize();
		m_moveRequest.velocity = m_velocity;
		m_moveRequest.type = eCMT_Fly;

	}
	else if ( m_params.forceView < 151.0f )
	{	// for the emergency situation...set speed immidiately

		// velocity/speed setting up
		m_desiredVelocity = m_input.movementVector;
		m_velocity = m_stats.velocity;
		Interpolate( m_velocity, m_desiredVelocity, 1.0f, 0.25f );

		m_desiredVeloctyQuat = GetQuatFromMat33(m_viewMtx);
		Quat modelRot( GetEntity()->GetRotation() );
		modelRot = Quat::CreateSlerp( modelRot, m_desiredVeloctyQuat, 0.20f );

		// commit the result interpolation & set velocity immidiately
		m_moveRequest.rotation = GetEntity()->GetRotation().GetInverted() * modelRot;
		m_moveRequest.rotation.Normalize();
		m_moveRequest.velocity = m_velocity;
		m_moveRequest.type = eCMT_Fly;
	}
	else if ( m_params.forceView < 201.0f )
	{	// for vsAir

		// velocity/speed setting up
		m_desiredVelocity = m_input.movementVector;
		m_velocity = m_stats.velocity;
		Interpolate( m_velocity, m_desiredVelocity, 1.0f, 0.10f );

		m_desiredVeloctyQuat = GetQuatFromMat33(m_viewMtx);
		Quat modelRot( GetEntity()->GetRotation() );
		modelRot = Quat::CreateSlerp( modelRot, m_desiredVeloctyQuat, 0.10f );

		// commit the result interpolation & set velocity immidiately
		m_moveRequest.rotation = GetEntity()->GetRotation().GetInverted() * modelRot;
		m_moveRequest.rotation.Normalize();
		m_moveRequest.velocity = m_velocity;
		m_moveRequest.type = eCMT_Fly;

	}

}

void CScout::ProcessSwimming(float frameTime)
{
	ProcessMovementNew(frameTime);
}

void CScout::ProcessMovement(float frameTime)
{
	ProcessMovementNew(frameTime);
	return;

	//movement
	Vec3 move;
	float	reqSpeed, maxSpeed;
	GetMovementVector(move, reqSpeed, maxSpeed);

	if (m_stats.sprintLeft)
		move *= m_params.sprintMultiplier;

  //if (m_input.posTarget.len2()>0.0f)
 	  m_desiredVelocity = move;
  //else
  //  Interpolate(m_desiredVelocity,move,3.0f,frameTime);

	Matrix33 velMtx;
	Vec3 vecRefRoll;

	Vec3 tempVel;
	//a bit workaround: needed when the alien is forced to look in some direction
	if (m_input.dirTarget.len2()>0.0f)
		tempVel = m_viewMtx.GetColumn(1);
	else
		tempVel = m_viewMtx.GetColumn(1)*max(0.1f,m_params.forceView) + m_stats.velocity;

	// A little bit more workaround, need to aling the alien to some up vector too.
	Vec3 up;
	if (m_input.upTarget.len2()>0.0f)
	{
		Vec3	diff = m_input.upTarget - m_input.posTarget;
		diff.NormalizeSafe();
		up = diff;
	}
	else
		up = m_viewMtx.GetColumn(2);

	Vec3 forward = tempVel.GetNormalized();
	Vec3 right = (forward % up).GetNormalized();

	velMtx.SetFromVectors(right,forward,right % forward);
	vecRefRoll = tempVel;

	m_desiredVeloctyQuat = GetQuatFromMat33(velMtx);

	//rollage
	if (m_input.upTarget.len2()>0.0f)
	{
		// No rollage, when the up vector is forced!
	}
	else
	{
		float	rollAmt = 0.0f;
		float dotRoll(vecRefRoll * m_baseMtx.GetColumn(0));
		if (fabs(dotRoll)>0.001f)
			rollAmt = max(min(gf_PI*0.30f,dotRoll*m_params.rollAmount),-gf_PI*0.30f);

		//roll less when carrying something
		if (m_pGrabHandler && m_pGrabHandler->GetStats()->grabId>0)
			rollAmt *= 0.3f;

		Interpolate(m_roll,rollAmt,m_params.rollSpeed,frameTime);

		m_desiredVeloctyQuat *= Quat::CreateRotationY(m_roll);
	}

	if (m_params.movingBend)
	{
		float bend(m_stats.speed/GetStanceInfo(m_stance)->maxSpeed);
		bend *= m_stats.velocity.GetNormalizedSafe(ZERO) * m_baseMtx.GetColumn(1);

		m_desiredVeloctyQuat *= Quat::CreateRotationX(-bend * DEG2RAD(m_params.movingBend));
	}

	float rotSpeed = m_params.rotSpeed_min + (1.0f - (max(GetStanceInfo( m_stance )->maxSpeed - max(m_stats.speed - m_params.speed_min,0.0f),0.0f) / GetStanceInfo( m_stance )->maxSpeed)) * (m_params.rotSpeed_max - m_params.rotSpeed_min);

	Interpolate(m_turnSpeed,rotSpeed,3.0f,frameTime);

	Quat currQuat(m_baseMtx);
	m_baseMtx = Matrix33(Quat::CreateSlerp( currQuat.GetNormalized(), m_desiredVeloctyQuat, frameTime * m_turnSpeed ));
	m_baseMtx.OrthonormalizeFast();
	
	float desiredVelMod(m_desiredVelocity.len());

	if (m_params.idealSpeed > -0.001f)
		desiredVelMod = min(desiredVelMod,m_params.idealSpeed);

	/*Vec3 desiredVel;
	//a bit workaround: needed when the alien is forced to move in some position
	if (m_input.posTarget.len2()>0.0f)
		desiredVel = m_desiredVelocity;
	else
		desiredVel = m_baseMtx.GetColumn(1)*desiredVelMod;
	
  // make sure alien reaches destination, ignore speedInertia
  if (m_input.posTarget.len2()>0.0f)
    m_velocity = desiredVel;
  else
	  Interpolate(m_velocity,desiredVel,m_params.speedInertia,frameTime);*/

	Interpolate(m_velocity,m_desiredVelocity,m_params.speedInertia,frameTime);

	Quat modelRot(m_baseMtx);
	modelRot = Quat::CreateSlerp(GetEntity()->GetRotation().GetNormalized(), modelRot, min(frameTime * 6.6f/*m_turnSpeed*/ /** (m_stats.speed/GetStanceInfo(m_stance)->maxSpeed)*/, 1.0f));

	m_moveRequest.rotation = GetEntity()->GetRotation().GetInverted() * modelRot;
  m_moveRequest.rotation.Normalize();
	m_moveRequest.velocity = m_velocity;
	m_moveRequest.type = eCMT_Fly;

	//FIXME:sometime
	m_stats.desiredSpeed = m_stats.speed;

	//FIXME:client view shake when the scout get closer, put this somewhere else.
	IActor *pClient = g_pGame->GetIGameFramework()->GetClientActor();
	if (pClient && m_stats.speed>0.5f)
	{
		float dist2ToClient((pClient->GetEntity()->GetWorldPos() - GetEntity()->GetWorldPos()).len2());
		float maxRange(m_params.cameraShakeRange * m_params.cameraShakeRange);
		if (dist2ToClient<maxRange)
		{
			IView *pView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetViewByEntityId(pClient->GetEntityId());
			//IView *pViewActive = g_pGame->GetIGameFramework()->GetIViewSystem()->GetActiveView();
			if (pView)
			{
				float shake = min(m_stats.speed*0.0015f,0.05f);
				float strenght = (1.0f - (dist2ToClient/maxRange)) * m_params.cameraShakeMultiplier;
				pView->SetViewShake(ZERO,Vec3(shake*strenght,0,shake*strenght),0.1f,0.0225f,1.5f,1);
			}
		}
	}
}

//
CScoutBeam::TScoutBeams CScoutBeam::s_scoutBeams;

CScoutBeam::CScoutBeam(CActor *pActor) : CBaseGrabHandler(pActor), 
	m_followDelay(0),
	m_criticalDistance(0)
{
	m_beamBone[0] = 0;

	for (int i=0;i<s_scoutBeams.size();++i)
		if (s_scoutBeams[i] == NULL)
		{
			s_scoutBeams[i] = this;
			return;
		}

	s_scoutBeams.push_back(this);
}

CScoutBeam::~CScoutBeam()
{
	for (int i=0;i<s_scoutBeams.size();++i)
		if (s_scoutBeams[i] == this)
			s_scoutBeams[i] = NULL;
}

bool CScoutBeam::SetGrab(SmartScriptTable &rParams)
{
	rParams->GetValue("followDelay",m_followDelay);
	m_followDelayMax = m_followDelay;

	rParams->GetValue("criticalDistance",m_criticalDistance);

	const char *pStr;
	if (rParams->GetValue("beamBone",pStr))
	{
		strncpy(m_beamBone,pStr,128);
		m_beamBone[127] = 0;
	}
	else
		m_beamBone[0] = 0;
	
	return CBaseGrabHandler::SetGrab(rParams);
}

void CScoutBeam::UpdatePosVelRot(float frameTime)
{
	if (m_grabStats.grabDelay>0.001f)
		return;

	IEntity *pGrab = gEnv->pEntitySystem->GetEntity(m_grabStats.grabId);
	
	if (pGrab)
	{
		IEntity *pEnt = m_pActor->GetEntity();

		Vec3 grabWPos(pEnt->GetSlotWorldTM(0) * m_grabStats.lHoldPos);
		Quat grabWQuat(pEnt->GetRotation());// * m_grabStats.additionalRotation);

		int grabbersNum(1);
		//FIXME:its not very efficient to check all the scout beams, but for the current case there is no real need to optimize this.
		for (int i=0;i<s_scoutBeams.size();++i)
		{
			IGrabHandler *pBeam = s_scoutBeams[i];
			if (pBeam && pBeam!=(IGrabHandler *)this)
				if (pBeam->GetStats()->grabId == m_grabStats.grabId)
				{
					IEntity *pBEnt = pBeam->GetOwner()->GetEntity();
					
					grabWPos = (grabWPos + pBEnt->GetSlotWorldTM(0) * pBeam->GetStats()->lHoldPos) * 0.5f;
					grabWQuat = Quat::CreateSlerp( grabWQuat, pBEnt->GetRotation()/* * pBeam->GetStats()->additionalRotation*/, 0.5f );

					++grabbersNum;
				}
		}

		float followRatio((m_followDelayMax - m_followDelay)/m_followDelayMax);
		followRatio *= grabbersNum*0.333f;

		m_grabStats.additionalRotation = Quat::CreateSlerp( m_grabStats.additionalRotation, IDENTITY, frameTime * 3.3f * followRatio );

		grabWQuat = Quat::CreateSlerp( pGrab->GetRotation(), grabWQuat, frameTime * 1.1f );
		pGrab->SetRotation(grabWQuat);
			
		AABB bbox;
		pGrab->GetLocalBounds(bbox);
		Vec3 grabCenter(pGrab->GetWorldTM() * ((bbox.max + bbox.min) * 0.5f));

		Vec3 setGrabVel(0,0,0);
		if (m_grabStats.throwDelay>0.001f)
		{
			float half(m_grabStats.maxDelay * 0.5f);
			float firstHalf(m_grabStats.throwDelay - half);

			if (firstHalf>0.0f)
			{
				setGrabVel = (m_grabStats.throwVector*-0.5f) * (1.0f-min((firstHalf*2) / m_grabStats.maxDelay,1.0f));
			}
			else
			{
				float secondHalf(firstHalf + half);
				setGrabVel = (m_grabStats.throwVector) * (1.0f-min((secondHalf*2) / m_grabStats.maxDelay,1.0f));
			}

			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(grabWPos+setGrabVel,2.5f,ColorB(0,255,0,255) );
		}

		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(grabWPos,0.25f,ColorB(0,255,0,255) );

		if (pEnt->GetPhysics() && pGrab->GetPhysics())
		{
			pe_status_dynamics dyn;
			if (pEnt->GetPhysics()->GetStatus(&dyn))
			{
				pe_action_set_velocity asv;
				if (setGrabVel.len2()>0.01f)
					asv.v = dyn.v + setGrabVel;
				else
				{
					asv.v = (grabWPos - grabCenter) * (m_grabStats.followSpeed * followRatio);

					//Vec3 delta(grabWPos - grabCenter);
					//float deltaLen(delta.len());
					//asv.v = delta.GetNormalizedSafe(ZERO)*min(deltaLen,5.0f)*m_grabStats.followSpeed;

					//asv.v = dyn.v + (grabWPos - grabCenter)*m_grabStats.followSpeed;
				}

				asv.w.Set(0,0,0);
				
				pGrab->GetPhysics()->Action(&asv);
			}
			else
			{
				pe_action_set_velocity asv;
				asv.v.Set(0,0,0);
				asv.w.Set(0,0,0);
				pGrab->GetPhysics()->Action(&asv);

			}
		}

		if (m_followDelay>0.001f)
			m_followDelay -= frameTime;

		if (m_criticalDistance && (grabWPos - grabCenter).len2()>m_criticalDistance*m_criticalDistance)
				StartDrop();
	}
}

void CScout::SetActorMovement(SMovementRequestParams &control)
{
	SMovementState state;
	GetMovementController()->GetMovementState(state);

	SetActorMovementCommon(control);

	if( control.vMoveDir.IsZero())
	{
		if( control.vLookTargetPos.IsZero())
		{
			SetDesiredDirection( GetEntity()->GetWorldRotation() * FORWARD_DIRECTION );
			SetDesiredSpeed( ZERO );
		}
		else
		{
			SetDesiredDirection((control.vLookTargetPos - state.eyePosition).GetNormalizedSafe());
			SetDesiredSpeed( ZERO );
		}
	}
	else
	{
		if( control.vLookTargetPos.IsZero())
		{
			SetDesiredDirection( control.vMoveDir.GetNormalizedSafe() );
			SetDesiredSpeed( control.vMoveDir * control.fDesiredSpeed );
		}
		else
		{
			SetDesiredDirection((control.vLookTargetPos - state.eyePosition).GetNormalizedSafe());
			SetDesiredSpeed( control.vMoveDir * control.fDesiredSpeed );
		}
	}

	int actions;

	switch(control.bodystate)
	{
	case 1:
		actions = ACTION_CROUCH;
		break;
	case 2:
		actions = ACTION_PRONE;
		break;
	case 3:
		actions = ACTION_RELAXED;
		break;
	case 4:
		actions = ACTION_STEALTH;
		break;
	default:
		actions = 0;
		break;
	}

	// Override the stance based on special behavior.
	SetActorStance(control, actions);

	m_input.actions = actions;

}

void CScout::FullSerialize(TSerialize ser)
{
	CAlien::FullSerialize(ser);

//	ser.BeginGroup("CScout");
//	ser.EndGroup();
}

void CScout::AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event)
{
	if (strcmp (event.m_EventName, "ObjectGrabbed") == 0) {
		m_pGrabHandler->StartGrab ();
	} else if (strcmp (event.m_EventName, "ObjectThrown") == 0) {
		m_pGrabHandler->StartDrop ();
	}	else if (strcmp (event.m_EventName, "StartIK") == 0) {
		(static_cast <CAnimatedGrabHandler*> (m_pGrabHandler))->ActivateIK ();
	}
}

void CScout::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);

	GetAlienMemoryStatistics(s);
}

void CScout::GetActorInfo( SBodyInfo& bodyInfo )
{
	CAlien::GetActorInfo( bodyInfo );

	// the scout model is wrong, the head bone is x-forward
	// that's why we need this special handling here:
	int headBoneID = GetBoneID( BONE_HEAD );
	if ( headBoneID > -1 && GetEntity()->GetCharacter(0) )
	{
		Matrix33 HeadMat( Matrix33(GetEntity()->GetCharacter(0)->GetISkeletonPose()->GetAbsJointByID(headBoneID).q) );
		bodyInfo.vEyeDirAnim = Matrix33(GetEntity()->GetSlotWorldTM(0) * HeadMat).GetColumn(0);
	}
}
