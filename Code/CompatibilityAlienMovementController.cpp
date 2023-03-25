// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "CompatibilityAlienMovementController.h"
#include "Alien.h"
#include <IItemSystem.h>


CCompatibilityAlienMovementController::CCompatibilityAlienMovementController( CAlien * pAlien ) : m_pAlien(pAlien), m_atTarget(false)
{
}

void CCompatibilityAlienMovementController::Reset()
{
}

bool CCompatibilityAlienMovementController::Update( float frameTime, SActorFrameMovementParams& params )
{
	UpdateCurMovementState( params );
	return false;
}

void CCompatibilityAlienMovementController::Release()
{
	delete this;
}

bool CCompatibilityAlienMovementController::RequestMovement( CMovementRequest& request )
{
	SMovementState state;
	GetMovementState(state);

	Vec3 currentEyePos = state.eyePosition;
	Vec3 currentPos = m_pAlien->GetEntity()->GetWorldPos();
	Vec3 currentForw = m_pAlien->GetEntity()->GetWorldRotation() * FORWARD_DIRECTION;

	CAlien::SMovementRequestParams os (request);

	if (request.HasMoveTarget())
		os.vMoveDir = (request.GetMoveTarget() - currentPos).GetNormalizedSafe(FORWARD_DIRECTION);


	if (request.HasForcedNavigation())
	{
		os.vMoveDir = request.GetForcedNavigation();
		os.fDesiredSpeed = os.vMoveDir.GetLength();
		os.vMoveDir.NormalizeSafe();
	}

	IAnimationGraphState *pAnimationGraphState = m_pAlien->GetAnimationGraphState();
	//CRY_ASSERT(pAnimationGraphState); // Hey, we can't assume we get a state!

	if(pAnimationGraphState)
	{
		if (const SAnimationTarget * pTarget = pAnimationGraphState->GetAnimationTarget())
		{
			if (pTarget->preparing)
			{
				os.bExactPositioning = true;
				PATHPOINT p;
				p.vPos = pTarget->position;
				p.vDir = pTarget->orientation * FORWARD_DIRECTION;
				os.remainingPath.push_back(p);
			}

			// workaround: DistanceToPathEnd is bogus values. Using true distance to anim target pos instead.
			//pAnimTarget->allowActivation = m_state.GetDistanceToPathEnd() < 5.0f;
			bool b3D = false;
			IAIActor* pAIActor = CastToIAIActorSafe(m_pAlien->GetEntity()->GetAI());
			// sort of working, would fail if the AI moved in 2D while being able to do it in 3D
			if(pAIActor)
				b3D = pAIActor->GetMovementAbility().b3DMove;

			Vec3 targetDisp(pTarget->position - currentPos);
			float distance = b3D ? targetDisp.GetLength() : targetDisp.GetLength2D();

			Vec3 targetDir(pTarget->orientation.GetColumn1()) ;
			float diffRot = targetDir.Dot(m_pAlien->GetEntity()->GetWorldTM().GetColumn1());
			if ( distance < 2.0f)
			{
				if(os.fDesiredSpeed == 0.0f )
				{
					os.fDesiredSpeed = max(0.1f,(distance/2)*0.8f);
					os.vMoveDir = pTarget->position - currentPos;
					if ( !b3D )
						os.vMoveDir.z = 0;
					os.vMoveDir.NormalizeSafe(FORWARD_DIRECTION);
				}
				// slow down if alien is pretty much close and not enough oriented like target
				if(diffRot<0.7f)
				{
					os.fDesiredSpeed *= max(distance/2,0.f);
				}
				/*if(distance>0)
				{
					if ( !b3D )
						targetDisp.z=0;
					targetDisp.NormalizeSafe(FORWARD_DIRECTION);
					float diffDisp = targetDir.Dot(targetDisp);
					if(diffDisp<0.9f)
					{
						os.vMoveDir = (pTarget->position - targetDir*0.3f) - currentPos;
						if ( !b3D )
							os.vMoveDir.z = 0;
						os.vMoveDir.NormalizeSafe(FORWARD_DIRECTION);
					}
				}*/
			}

			float frameTime = gEnv->pTimer->GetFrameTime();
			if(frameTime>0)
			{
				float expectedSpeed = distance/frameTime;
				if(os.fDesiredSpeed > expectedSpeed)
					os.fDesiredSpeed = expectedSpeed;
			}
			//ColorB sd;
			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(currentPos,ColorB(255,255,255,255),currentPos+os.vMoveDir*2,ColorB(255,255,255,255));

			static float startDistance = 5.0f;
			pTarget->allowActivation = distance < startDistance;
		}
	}

	m_pAlien->SetActorMovement(os);

	if(pAnimationGraphState)
	{
		if (request.HasActorTarget())
		{
			// clear any action which might mess up the incoming exact positioning related AG states
			//pAnimationGraphState->SetInput("Action","idle");

			const SActorTargetParams& p = request.GetActorTarget();
			SAnimationTargetRequest req;
			req.position = p.location;
			req.positionRadius = p.locationRadius;
			req.direction = p.direction;
			req.directionRadius = p.directionRadius;
			req.prepareRadius = 3.0f;
			req.projectEnd = p.projectEnd;
			IAnimationSpacialTrigger * pTrigger = pAnimationGraphState->SetTrigger(req, p.triggerUser, p.pQueryStart, p.pQueryEnd);
			if (!p.vehicleName.empty())
			{
				pTrigger->SetInput( "Vehicle", p.vehicleName.c_str() );
				pTrigger->SetInput( "VehicleSeat", p.vehicleSeat );
			}
			pTrigger->SetInput( "DesiredSpeed", p.speed );
			pTrigger->SetInput( "DesiredTurnAngleZ", 0 );
			if (!p.animation.empty())
			{
				pTrigger->SetInput( p.signalAnimation? "Signal" : "Action", p.animation.c_str() );
			}
		}
		else if (request.RemoveActorTarget())
		{
			pAnimationGraphState->ClearTrigger(eAGTU_AI);
		}
	}

	m_atTarget = os.eActorTargetPhase == eATP_Finished;

	if (request.HasFireTarget())
		m_currentMovementRequest.SetFireTarget( request.GetFireTarget() );
	else if (request.RemoveFireTarget())
		m_currentMovementRequest.ClearFireTarget();

	if (request.HasAimTarget())
		m_currentMovementRequest.SetAimTarget( request.GetAimTarget() );
	else if (request.RemoveAimTarget())
		m_currentMovementRequest.ClearAimTarget();

	return true;
}

void CCompatibilityAlienMovementController::UpdateCurMovementState(const SActorFrameMovementParams& params)
{
	SMovementState& state(m_currentMovementState);
	CAlien::SBodyInfo bodyInfo;
	m_pAlien->GetActorInfo( bodyInfo );
	//state.maxSpeed = bodyInfo.maxSpeed;
	//state.minSpeed = bodyInfo.minSpeed;
	//state.normalSpeed = bodyInfo.normalSpeed;
	state.stance = bodyInfo.stance;
	state.m_StanceSize		= bodyInfo.m_stanceSizeAABB;
	state.m_ColliderSize	= bodyInfo.m_colliderSizeAABB;
	state.eyeDirection = bodyInfo.vEyeDir;
	state.animationEyeDirection = bodyInfo.vEyeDirAnim;
	state.eyePosition = bodyInfo.vEyePos;
	state.weaponPosition = bodyInfo.vFirePos;
	state.movementDirection = bodyInfo.vFwdDir;
	state.upDirection = bodyInfo.vUpDir;
	state.atMoveTarget = m_atTarget;
	state.bodyDirection = m_pAlien->GetEntity()->GetWorldRotation() * Vec3(0,1,0);
	/*	if (IItem * pItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem( itemEntity ))
	if (const IWeapon * pWeapon = pItem->GetIWeapon())
	state.weaponPosition = pWeapon->GetFiringPos(Vec3(0,0,0));*/

	if(m_currentMovementRequest.HasAimTarget())
		state.aimDirection = (m_currentMovementRequest.GetAimTarget()-state.weaponPosition).GetNormalizedSafe();
	else
		state.aimDirection = bodyInfo.vFireDir.GetNormalizedSafe();

	if(m_currentMovementRequest.HasFireTarget())
		state.fireDirection = (m_currentMovementRequest.GetFireTarget()-state.weaponPosition).GetNormalizedSafe(state.aimDirection);
	else
		state.fireDirection = state.aimDirection;

	state.isAlive = (m_pAlien->GetHealth()>0);
	// get weapon position -----------------------
	IInventory * pInventory = m_pAlien->GetInventory();
	if (!pInventory)
		return;
	EntityId itemEntity = pInventory->GetCurrentItem();


	//---------------------------------------------
	//FIXME
	state.isAiming = true;
	/*Vec3	fireDir(outShootTargetPos - m_pShooter->GetFirePos());
	fireDir.normalize();
	if(fireDir.Dot(bodyInfo.vFireDir) < cry_cosf(DEG2RAD(10.0)) )*/

	state.isFiring = (m_pAlien->GetActorStats()->inFiring>0.001f);

	if(m_currentMovementRequest.HasFireTarget())
		state.fireTarget = m_currentMovementRequest.GetFireTarget();
}

bool CCompatibilityAlienMovementController::GetStanceState(EStance stance, float lean, bool defaultPose, SStanceState& state)
{
	const SStanceInfo*	pStance = m_pAlien->GetStanceInfo(stance);
	if(!pStance)
		return false;

	if(defaultPose)
	{
		state.pos.Set(0,0,0);
		state.bodyDirection = FORWARD_DIRECTION;
		state.upDirection(0,0,1);
		state.weaponPosition = m_pAlien->GetWeaponOffsetWithLean(stance, lean, m_pAlien->GetEyeOffset());
		state.aimDirection = FORWARD_DIRECTION;
		state.fireDirection = FORWARD_DIRECTION;
		state.eyePosition = pStance->GetViewOffsetWithLean(lean);
		state.eyeDirection = FORWARD_DIRECTION;
		state.m_StanceSize = pStance->GetStanceBounds();
		state.m_ColliderSize = pStance->GetColliderBounds();
	}
	else
	{
		// TODO: the directions are like not to match. Is the AI even using them?
		CAlien::SBodyInfo bodyInfo;
		m_pAlien->GetActorInfo( bodyInfo );

		Matrix34	tm = m_pAlien->GetEntity()->GetWorldTM();

		state.pos = m_pAlien->GetEntity()->GetWorldPos();
		state.bodyDirection = bodyInfo.vFwdDir;
		state.upDirection = bodyInfo.vUpDir;
		state.weaponPosition = tm.TransformPoint(m_pAlien->GetWeaponOffsetWithLean(stance, lean, m_pAlien->GetEyeOffset()));
		state.aimDirection = bodyInfo.vFireDir;
		state.fireDirection = bodyInfo.vFireDir;
		state.eyePosition = tm.TransformPoint(pStance->GetViewOffsetWithLean(lean));
		state.eyeDirection = bodyInfo.vEyeDir;
		state.m_StanceSize = pStance->GetStanceBounds();
		state.m_ColliderSize = pStance->GetColliderBounds();
	}

	return true;
}