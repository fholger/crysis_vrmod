// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "Game.h"
#include "PlayerMovementController.h"
#include "GameUtils.h"
#include "ITimer.h"
#include "IVehicleSystem.h"
#include "IItemSystem.h"
#include "GameCVars.h"
#include "NetInputChainDebug.h"
#include "Item.h"

#define ENABLE_NAN_CHECK

#ifdef ENABLE_NAN_CHECK
#define CHECKQNAN_FLT(x) \
	assert(((*(unsigned*)&(x))&0xff000000) != 0xff000000u && (*(unsigned*)&(x) != 0x7fc00000))
#else
#define CHECKQNAN_FLT(x) (void*)0
#endif

#define CHECKQNAN_VEC(v) \
	CHECKQNAN_FLT(v.x); CHECKQNAN_FLT(v.y); CHECKQNAN_FLT(v.z)

// minimum desired speed accepted
static const float MIN_DESIRED_SPEED = 0.00002f;
// distance from target that is accepted as "there"
static const float TARGET_REACHED_DISTANCE = 0.4f;
// distance to start slowing down before reaching target
static const float TARGET_SLOWDOWN_DISTANCE = 1.0f;

// maximum head turn angle. This is really low, should be nearer 90, but the helmet intersects with the suit for larger values.
static const float MAX_HEAD_TURN_ANGLE = DEG2RAD(45);
// maximum aim angle
static const float MAX_AIM_TURN_ANGLE = DEG2RAD(60);
// anticipation look ik
static const float ANTICIPATION_COSINE_ANGLE = DEG2RAD(45);
// amount of time to aim for
static const CTimeValue AIM_TIME = 0.1f; //2.5f;
// amount of time to look for
static const CTimeValue LOOK_TIME = 2.0f;
// maximum angular velocity for look-at motion (radians/sec)
static const float MAX_LOOK_AT_ANGULAR_VELOCITY = DEG2RAD(75.0f);
// maximum angular velocity for aim-at motion (radians/sec)
static const float MAX_AIM_AT_ANGULAR_VELOCITY = DEG2RAD(75.0f);
static const float MAX_FIRE_TARGET_DELTA_ANGLE = DEG2RAD(10.0f);

// IDLE Checking stuff from here on
#define DEBUG_IDLE_CHECK
#undef  DEBUG_IDLE_CHECK

static const float IDLE_CHECK_TIME_TO_IDLE = 5.0f;
static const float IDLE_CHECK_MOVEMENT_TIMEOUT = 3.0f;
// ~IDLE Checking stuff

CPlayerMovementController::CPlayerMovementController( CPlayer * pPlayer ) : m_pPlayer(pPlayer), m_animTargetSpeed(-1.0f), m_animTargetSpeedCounter(0)
{
	m_lookTarget=Vec3(ZERO);
	m_aimTarget=Vec3(ZERO);
	m_fireTarget=Vec3(ZERO);
	Reset();


}

void CPlayerMovementController::Reset()
{
	m_aimNextTarget = 0;
	m_aimTargetsCount = 0;
	m_aimTargetsIterator = 0;

	m_state = CMovementRequest();
	m_atTarget = false;
	m_desiredSpeed = 0.0f;
	m_usingAimIK = m_usingLookIK = false;
	m_aimClamped = false;

	m_aimInterpolator.Reset();
	m_lookInterpolator.Reset();
	m_updateFunc = &CPlayerMovementController::UpdateNormal;
	m_targetStance = STANCE_NULL;
	m_strengthJump = false;
	m_idleChecker.Reset(this);

	if(!GetISystem()->IsSerializingFile() == 1)
		UpdateMovementState( m_currentMovementState );

	//
	m_aimTargets.resize(1);
	uint32 numAimTargets = m_aimTargets.size();
	for (uint32 i=0; i<numAimTargets; i++)
	{
		m_aimTargets[i]=Vec3(ZERO);
	}

	m_lookTarget=Vec3(ZERO);
	m_aimTarget=Vec3(ZERO);
	m_fireTarget=Vec3(ZERO);

}

bool CPlayerMovementController::RequestMovement( CMovementRequest& request )
{
	if (!m_pPlayer->IsPlayer())
	{
		if (IVehicle* pVehicle = m_pPlayer->GetLinkedVehicle())
		{
			if (IMovementController* pController = pVehicle->GetPassengerMovementController(m_pPlayer->GetEntityId()))
			{
        IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger(m_pPlayer->GetEntityId());
        if (!pSeat->IsDriver())
				  pController->RequestMovement(request);
			}
		}
	}
	// because we're not allowed to "commit" values here, we make a backup,
	// perform modifications, and then copy the modifications make if everything
	// was successful
	CMovementRequest state = m_state;
	// we have to process right through and not early out, because otherwise we
	// won't modify request to a "close but correct" state
	bool ok = true;

	bool idleCheck = m_idleChecker.Process(m_currentMovementState, m_state, request);

	if (request.HasMoveTarget())
	{
		// TODO: check validity of getting to that target
		state.SetMoveTarget( request.GetMoveTarget() );
		m_atTarget = false;

		float distanceToEnd(request.GetDistanceToPathEnd());
		if (distanceToEnd>0.001f)
			state.SetDistanceToPathEnd(distanceToEnd);
	}
	else if (request.RemoveMoveTarget())
	{
		state.ClearMoveTarget();
		state.ClearDesiredSpeed();
		state.ClearDistanceToPathEnd();
	}

	if (request.HasStance())
	{
		state.SetStance( request.GetStance() );
	}
	else if (request.RemoveStance())
	{
		state.ClearStance();
	}

	if (request.HasDesiredSpeed())
	{
		if (!state.HasMoveTarget() && request.GetDesiredSpeed() > 0.0f)
		{
			request.SetDesiredSpeed(0.0f);
			ok = false;
		}
		else
		{
			state.SetDesiredSpeed( request.GetDesiredSpeed() );
		}
	}
	else if (request.RemoveDesiredSpeed())
	{
		state.RemoveDesiredSpeed();
	}

	if (request.HasAimTarget())
	{
		state.SetAimTarget( request.GetAimTarget() );
	}
	else if (request.RemoveAimTarget())
	{
		state.ClearAimTarget();
		//state.SetNoAiming();
	}

	if (request.HasBodyTarget())
	{
		state.SetBodyTarget( request.GetBodyTarget() );
	}
	else if (request.RemoveBodyTarget())
	{
		state.ClearBodyTarget();
		//state.SetNoBodying();
	}

	if (request.HasFireTarget())
	{
		state.SetFireTarget( request.GetFireTarget() );
		//forcing fire target here, can not wait for it to be set in the dateNormalate.
		//if don't do it - first shot of the weapon might be in some undefined direction (ProsessShooting is done right after this 
		//call in AIProxy). This is particularly problem for RPG shooting
		m_currentMovementState.fireTarget = request.GetFireTarget();
		// the weaponPosition is from last update - might be different at this moment, but should not be too much
		m_currentMovementState.fireDirection = (m_currentMovementState.fireTarget - m_currentMovementState.weaponPosition).GetNormalizedSafe();
	}
	else if (request.RemoveFireTarget())
	{
		state.ClearFireTarget();
		//state.SetNoAiming();
	}

	if (request.HasLookTarget())
	{
		// TODO: check against move direction to validate request
		state.SetLookTarget( request.GetLookTarget(), request.GetLookImportance() );
	}
	else if (request.RemoveLookTarget())
	{
		state.ClearLookTarget();
	}

	if (request.HasStance())
	{
		state.SetStance( request.GetStance() );
	}
	else if (request.RemoveStance())
	{
		state.ClearStance();
	}

	if (request.HasLean())
	{
		state.SetLean( request.GetLean() );
	}
	else if (request.RemoveLean())
	{
		state.ClearLean();
	}

	if (request.ShouldJump())
		state.SetJump();

	state.SetAllowStrafing(request.AllowStrafing());

	if (request.HasNoAiming())
		state.SetNoAiming();

	if (request.HasDeltaMovement())
		state.AddDeltaMovement( request.GetDeltaMovement() );
	if (request.HasDeltaRotation())
		state.AddDeltaRotation( request.GetDeltaRotation() );

	if (request.HasPseudoSpeed())
		state.SetPseudoSpeed(request.GetPseudoSpeed());
	else if (request.RemovePseudoSpeed())
		state.ClearPseudoSpeed();

	if (request.HasPrediction())
		state.SetPrediction( request.GetPrediction() );
	else if (request.RemovePrediction())
		state.ClearPrediction();

	state.SetAlertness(request.GetAlertness());

	// commit modifications
	if (ok)
	{
		m_state = state;
	}

//	if (ICharacterInstance * pChar = m_pPlayer->GetEntity()->GetCharacter(0))
//		pChar->GetISkeleton()->SetFuturePathAnalyser( (m_state.HasPathTarget() || m_state.HasMoveTarget())? 1 : 0 );

/*
	if (m_state.HasPathTarget())
	{
		SAnimationTargetRequest req;
		if (m_state.HasDesiredBodyDirectionAtTarget())
		{
			req.direction = m_state.GetDesiredBodyDirectionAtTarget();
			req.directionRadius = DEG2RAD(1.0f);
		}
		else
		{
			req.direction = FORWARD_DIRECTION;
			req.directionRadius = DEG2RAD(180.0f);
		}
		req.position = m_state.GetPathTarget();

		IAnimationSpacialTrigger * pTrigger = m_pPlayer->GetAnimationGraphState()->SetTrigger( req );
		if (m_state.HasDesiredSpeedAtTarget())
			pTrigger->SetInput("DesiredSpeed", m_state.GetDesiredSpeedAtTarget());
		else
			pTrigger->SetInput("DesiredSpeed", 0.0f); // TODO: workaround to test
	}
*/

	if (request.HasActorTarget())
	{
		const SActorTargetParams& p = request.GetActorTarget();

		SAnimationTargetRequest req;
		req.position = p.location;
		req.positionRadius = std::max( p.locationRadius, DEG2RAD(0.05f) );
		static float minRadius = 0.05f;
		req.startRadius = std::max(minRadius, 2.0f*p.locationRadius);
		if (p.startRadius > minRadius)
			req.startRadius = p.startRadius;
		req.direction = p.direction;
		req.directionRadius = std::max( p.directionRadius, DEG2RAD(0.05f) );
		req.prepareRadius = 3.0f;
		req.projectEnd = p.projectEnd;
		req.navSO = p.navSO;

		IAnimationSpacialTrigger * pTrigger = m_pPlayer->GetAnimationGraphState()->SetTrigger(req, p.triggerUser, p.pQueryStart, p.pQueryEnd);
		if (pTrigger)
		{
			if (!p.vehicleName.empty())
			{
				pTrigger->SetInput( "Vehicle", p.vehicleName.c_str() );
				pTrigger->SetInput( "VehicleSeat", p.vehicleSeat );
			}
			if (p.speed >= 0.0f)
			{
				pTrigger->SetInput( m_inputDesiredSpeed, p.speed );
			}
			m_animTargetSpeed = p.speed;
			pTrigger->SetInput( m_inputDesiredTurnAngleZ, 0 );
			if (!p.animation.empty())
			{
				pTrigger->SetInput( p.signalAnimation? "Signal" : "Action", p.animation.c_str() );
			}
			if (p.stance != STANCE_NULL)
			{
				m_targetStance = p.stance;
				pTrigger->SetInput( m_inputStance, m_pPlayer->GetStanceInfo(p.stance)->name );
			}
		}
	}
	else if (request.RemoveActorTarget())
	{
		if(m_pPlayer->GetAnimationGraphState())
			m_pPlayer->GetAnimationGraphState()->ClearTrigger(eAGTU_AI);
	}

	return ok;
}

ILINE static f32 GetYaw( const Vec3& v0, const Vec3& v1 )
{
  float a0 = atan2f(v0.y, v0.x);
  float a1 = atan2f(v1.y, v1.x);
  float a = a1 - a0;
  if (a > gf_PI) a -= gf_PI2;
  else if (a < -gf_PI) a += gf_PI2;
  return a;
}

ILINE static Quat GetTargetRotation( Vec3 oldTarget, Vec3 newTarget, Vec3 origin )
{
	Vec3 oldDir = (oldTarget - origin).GetNormalizedSafe(FORWARD_DIRECTION);
	Vec3 newDir = (newTarget - origin).GetNormalizedSafe(ZERO);
	if (newDir.GetLength() < 0.001f)
		return Quat::CreateIdentity();
	else
		return Quat::CreateRotationV0V1( oldDir, newDir );
}

static void DrawArrow( Vec3 from, Vec3 to, float length, float * clr )
{
	float r = clr[0];
	float g = clr[1];
	float b = clr[2];
	float color[] = {r,g,b,1};
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( from, ColorF(r,g,b,1), to, ColorF(r,g,b,1) );
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere( to, 0.05f, ColorF(r,g,b,1) );
}

static float bias( float b, float t )
{
	if (b == 0.0f)
		return 0.0f;
	return cry_powf( b, cry_logf(b)/cry_logf(0.5f) );
}

static float gain( float g, float t )
{
	if (t < 0.5f)
		return bias( 1.0f-g, 2.0f*t );
	else
		return 1.0f - bias( 1.0f-g, 2.0f-2.0f*t );
}

static float white[4] = {1,1,1,1};
static float blue[4] = {0,0,1,1};
static float red[4] = {1,0,0,1};
static float yellow[4] = {1,1,0,1};
static float green[4] = {0,1,0,1};
static CTimeValue lastTime;
static int y = 100;

void CPlayerMovementController::BindInputs( IAnimationGraphState * pAGState )
{
	if ( pAGState )
	{
		m_inputDesiredSpeed = pAGState->GetInputId("DesiredSpeed");
		m_inputDesiredTurnAngleZ = pAGState->GetInputId("DesiredTurnAngleZ");
		m_inputStance = pAGState->GetInputId("Stance");
		m_inputPseudoSpeed = pAGState->GetInputId("PseudoSpeed");
	}
}

bool CPlayerMovementController::Update( float frameTime, SActorFrameMovementParams& params )
{
	bool ok = false;

	params.lookTarget = Vec3(-1,-1,-1);
	params.aimTarget = Vec3(-1,-1,-1);

/*
	if (m_pPlayer->IsFrozen())
	{
		params.lookTarget = Vec3(-2,-2,-2);
		params.aimTarget = Vec3(-2,-2,-2);

		if (m_state.HasAimTarget())
		{
			Vec3 aimTarget = m_state.GetAimTarget();
			if (!aimTarget.IsValid())
				CryError("");
			m_state.ClearAimTarget();
		}

		if (m_state.HasLookTarget())
		{
			Vec3 lookTarget = m_state.GetLookTarget();
			if (!lookTarget.IsValid())
				CryError("");
			m_state.ClearLookTarget();
		}
	}
*/

	if (m_updateFunc)
	{
		ok = (this->*m_updateFunc)(frameTime, params);
	}


//	m_state.RemoveDeltaMovement();
//	m_state.RemoveDeltaRotation();

	return ok;
}

void CPlayerMovementController::CTargetInterpolator::TargetValue(
	const Vec3& value, 
	CTimeValue now, 
	CTimeValue timeout, 
	float frameTime, 
	bool updateLastValue, 
	const Vec3& playerPos, 
	float maxAngularVelocity )
{
	m_target = value;
	if (updateLastValue)
		m_lastValue = now;
}

static Vec3 ClampDirectionToCone( const Vec3& dir, const Vec3& forward, float maxAngle )
{
//	float angle = cry_acosf( dir.Dot(forward) );
	float angle = cry_acosf( clamp_tpl(dir.Dot(forward),-1.0f,1.0f) );
	if (angle > maxAngle)
	{
		Quat fromForwardToDir = Quat::CreateRotationV0V1(forward, dir);
		Quat rotate = Quat::CreateSlerp( Quat::CreateIdentity(), fromForwardToDir, min(maxAngle/angle, 1.0f) );
		return rotate * forward;
	}
	return dir;
}

static Vec3 FlattenVector( Vec3 x )
{
	x.z = 0;
	return x;
}

void CPlayerMovementController::CTargetInterpolator::Update(float frameTime)
{
	m_pain += frameTime * (m_painDelta - DEG2RAD(10.0f));
	m_pain = CLAMP(m_pain, 0, 2);
}

bool CPlayerMovementController::CTargetInterpolator::GetTarget(
	Vec3& target, 
	Vec3& rotateTarget, 
	const Vec3& playerPos, 
	const Vec3& moveDirection, 
	const Vec3& bodyDirection,
	const Vec3& entityDirection,
	float maxAngle, 
	float distToEnd, 
	float viewFollowMovement, 
	ColorB * clr,
	const char ** bodyTargetType )
{
  bool retVal = true;
	Vec3 desiredTarget = GetTarget();
	Vec3 desiredDirection = desiredTarget - playerPos;
	float	targetDist = desiredDirection.GetLength();

	// if the target gets very close, then move it to the current body direction (failsafe)
	float desDirLenSq = desiredDirection.GetLengthSquared2D();
	if (desDirLenSq < sqr(0.2f))
	{
		float blend = min(1.0f, 5.0f * (0.2f - sqrtf(desDirLenSq)));
		desiredDirection = LERP(desiredDirection, entityDirection, blend);
	}
	desiredDirection.NormalizeSafe(entityDirection);

	Vec3 clampedDirection = desiredDirection;

	// clamp yaw rotation into acceptable bounds
	float yawFromBodyToDesired = GetYaw( bodyDirection, desiredDirection );
	if (fabsf(yawFromBodyToDesired) > maxAngle)
	{
		float clampAngle = (yawFromBodyToDesired < 0) ? -maxAngle : maxAngle;
		clampedDirection = Quat::CreateRotationZ(clampAngle) * bodyDirection;
		clampedDirection.z = desiredDirection.z;
		clampedDirection.Normalize();

		if (viewFollowMovement > 0.1f)
		{
			clampedDirection = Quat::CreateSlerp( 
			Quat::CreateIdentity(), 
			Quat::CreateRotationV0V1(clampedDirection, moveDirection), 
			min(viewFollowMovement, 1.0f)) * clampedDirection;
      // return false to indicate that the direction is out of range. This stops the gun
      // being held up in the aiming pose, but not pointing at the target, whilst running
      retVal = false;
		}
		// Clamped, rotate the body until it is facing towards the target.
	}

	if(viewFollowMovement > 0.5f)
	{
		// Rotate the body towards the movement direction while moving and strafing is not specified.
		rotateTarget = playerPos + 5.0f * moveDirection.GetNormalizedSafe();
		*bodyTargetType = "ik-movedir";
	}
	else
	{
		rotateTarget = desiredTarget;
		*bodyTargetType = "ik-target";
	}

	// Return the safe aim/look direction.
	target = clampedDirection * targetDist + playerPos;

	return retVal;
}

static void DebugDrawWireFOVCone(IRenderer* pRend, const Vec3& pos, const Vec3& dir, float rad, float fov, ColorB col)
{
	const unsigned npts = 32;
	const unsigned npts2 = 16;
	Vec3	points[npts];
	Vec3	pointsx[npts2];
	Vec3	pointsy[npts2];

	Matrix33	base;
	base.SetRotationVDir(dir);

	fov *= 0.5f;

	float coneRadius = sinf(fov) * rad;
	float coneHeight = cosf(fov) * rad;

	for(unsigned i = 0; i < npts; i++)
	{
		float	a = ((float)i / (float)npts) * gf_PI2;
		float rx = cosf(a) * coneRadius;
		float ry = sinf(a) * coneRadius;
		points[i] = pos + base.TransformVector(Vec3(rx, coneHeight, ry));
	}

	for(unsigned i = 0; i < npts2; i++)
	{
		float	a = -fov + ((float)i / (float)(npts2-1)) * (fov*2);
		float rx = sinf(a) * rad;
		float ry = cosf(a) * rad;
		pointsx[i] = pos + base.TransformVector(Vec3(rx, ry, 0));
		pointsy[i] = pos + base.TransformVector(Vec3(0, ry, rx));
	}

	pRend->GetIRenderAuxGeom()->DrawPolyline(points, npts, true, col);
	pRend->GetIRenderAuxGeom()->DrawPolyline(pointsx, npts2, false, col);
	pRend->GetIRenderAuxGeom()->DrawPolyline(pointsy, npts2, false, col);

	pRend->GetIRenderAuxGeom()->DrawLine(points[0], col, pos, col);
	pRend->GetIRenderAuxGeom()->DrawLine(points[npts/4], col, pos, col);
	pRend->GetIRenderAuxGeom()->DrawLine(points[npts/2], col, pos, col);
	pRend->GetIRenderAuxGeom()->DrawLine(points[npts/2+npts/4], col, pos, col);
}


static bool ClampTargetPointToCone(Vec3& target, const Vec3& pos, const Vec3& dir, float coneAngle)
{
	Vec3	reqDir = target - pos;
	float dist = reqDir.NormalizeSafe();
	if (dist < 0.01f)
		return false;

	// Slerp
	float cosAngle = dir.Dot(reqDir);

	coneAngle *= 0.5f;

	const float eps = 1e-6f;
	const float thr = cosf(coneAngle);

	if (cosAngle > thr)
		return false;

	// Target is outside the cone
	float targetAngle = acos_tpl(cosAngle);
	float t = coneAngle / targetAngle;

	Quat	cur;
	cur.SetRotationVDir(dir);
	Quat	req;
	req.SetRotationVDir(reqDir);

	Quat	view;
	view.SetSlerp(cur, req, t);

	target = pos + view.GetColumn1() * dist;

	return true;
}


bool CPlayerMovementController::UpdateNormal( float frameTime, SActorFrameMovementParams& params )
{
	// TODO: Don't update all this mess when a character is dead.

	ITimer * pTimer = gEnv->pTimer;

	IRenderer * pRend = gEnv->pRenderer;
	if (g_pGame->GetCVars()->g_debugaimlook)
	{
		if (pTimer->GetFrameStartTime()!=lastTime)
		{
			y = 100;
			lastTime = pTimer->GetFrameStartTime();
		}
	}

	IEntity * pEntity = m_pPlayer->GetEntity();
	Vec3 playerPos = pEntity->GetWorldPos();

	params.desiredVelocity = Vec3(0,0,0);
	params.lookTarget = m_currentMovementState.eyePosition + m_currentMovementState.eyeDirection * 10.0f;
//	params.aimTarget = m_currentMovementState.handPosition + m_currentMovementState.aimDirection * 10.0f;
	if(m_state.HasAimTarget())
		params.aimTarget = m_state.GetAimTarget();
	else
		params.aimTarget = m_currentMovementState.weaponPosition + m_currentMovementState.aimDirection * 10.0f;
	params.lookIK = false;
	params.aimIK = false;
	params.deltaAngles = Ang3(0,0,0);

	static bool forceStrafe = false;	// [mikko] For testing, should be 'false'.
	bool allowStrafing = m_state.AllowStrafing();
	params.allowStrafe = allowStrafing || forceStrafe;

	if(gEnv->bMultiplayer && m_state.HasStance() && m_state.GetStance() == STANCE_PRONE)
	{
		// send a ray backwards to check we're not intersecting a wall. If we are, go to crouch instead.
		if (gEnv->bClient && m_pPlayer->GetGameObject()->IsProbablyVisible())
		{
			Vec3 dir = -m_currentMovementState.bodyDirection;
			dir.NormalizeSafe();
			dir.z = 0.0f;

			Vec3 pos = pEntity->GetWorldPos();
			pos.z += 0.3f;	// move position up a bit so it doesn't intersect the ground.

			//pRend->GetIRenderAuxGeom()->DrawLine( pos, ColorF(1,1,1,0.5f), pos + dir, ColorF(1,1,1,0.5f) );

			ray_hit hit;
			static const int obj_types = ent_static|ent_rigid|ent_sleeping_rigid|ent_living;
			static const unsigned int flags = rwi_stop_at_pierceable|rwi_colltype_any;
			bool rayHitAny = 0 != gEnv->pPhysicalWorld->RayWorldIntersection( pos, dir, obj_types, flags, &hit, 1, pEntity->GetPhysics() );
			if (rayHitAny)
			{
				m_state.SetStance(STANCE_CROUCH);
			}				

			// also don't allow prone for hurricane/LAW type weapons.
			if(g_pGameCVars->g_proneNotUsableWeapon_FixType == 2)
			{
				if(CItem* pItem = (CItem*)(m_pPlayer->GetCurrentItem()))
				{
					if(pItem->GetParams().prone_not_usable)
						m_state.SetStance(STANCE_CROUCH);
				}
			}
		}
	}

	if (m_state.AlertnessChanged())
	{
		if (m_state.GetAlertness() == 2)
		{
			if (!m_state.HasStance() || m_state.GetStance() == STANCE_RELAXED)
				m_state.SetStance(STANCE_STAND);
		}
	}

	ICharacterInstance * pCharacter = pEntity->GetCharacter(0);
	if (!pCharacter)
		return true;

	ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();

	CTimeValue now = pTimer->GetFrameStartTime();

	Vec3 currentBodyDirection = pEntity->GetRotation().GetColumn1();
	Vec3 moveDirection = currentBodyDirection;
	if (m_pPlayer->GetLastRequestedVelocity().len2() > 0.01f)
		moveDirection = m_pPlayer->GetLastRequestedVelocity().GetNormalized();

	// potentially override our normal targets due to a targeted animation request
	bool hasMoveTarget = false;
	bool hasBodyTarget = false;
	Vec3 moveTarget;
	Vec3 bodyTarget;

	float viewFollowMovement = 0.0f;

	const char * bodyTargetType = "none";
	const char * moveTargetType = "none";
	if (m_state.HasMoveTarget())
	{
		hasMoveTarget = true;
		moveTarget = m_state.GetMoveTarget();
		moveTargetType = "steering";
	}
	const SAnimationTarget * pAnimTarget = m_pPlayer->GetAnimationGraphState()->GetAnimationTarget();
	if ((pAnimTarget != NULL) && pAnimTarget->preparing)
	{
		float distanceToTarget = (pAnimTarget->position - playerPos).GetLength2D();

		Vec3 animTargetFwdDir = pAnimTarget->orientation.GetColumn1();

		Vec3 moveDirection2d = hasMoveTarget ? moveDirection : (pAnimTarget->position - playerPos);
		moveDirection2d.z = 0.0f;
		moveDirection2d.NormalizeSafe(animTargetFwdDir);

		float deltaAngleDot = animTargetFwdDir.Dot(moveDirection2d);
		float fraction = (distanceToTarget - 0.2f) / 1.8f;
		float blend = CLAMP(fraction, 0.0f, 1.0f);
		animTargetFwdDir.SetSlerp(animTargetFwdDir, moveDirection2d, blend );

		float animTargetFwdDepthFraction = max(0.0f, animTargetFwdDir.Dot(moveDirection2d));
		float animTargetFwdDepth = LERP(1.0f, 3.0f, animTargetFwdDepthFraction);

/*
		Vec3 bump(0,0,0.1f);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(pAnimTarget->position+bump, 0.05f, ColorB(255,255,255,255), true);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pAnimTarget->position+bump, ColorB(255,255,255,255), 
			pAnimTarget->position+bump + animTargetFwdDepth * animTargetFwdDir, ColorB(255,255,255,255), 4.0f);
*/

		bodyTarget = pAnimTarget->position + animTargetFwdDepth * animTargetFwdDir;
		bodyTargetType = "animation";
		hasBodyTarget = true;
		allowStrafing = true;

		if (!hasMoveTarget || pAnimTarget->notAiControlledAnymore || (distanceToTarget < 1.0f))
		{
			moveTarget = pAnimTarget->position;
			moveTargetType = "animation";
			hasMoveTarget = true;
			pAnimTarget->notAiControlledAnymore = true;
		}
	}
	if (pAnimTarget != NULL)
	{
    static float criticalDistance = 5.0f;
    float distance = (pAnimTarget->position - playerPos).GetLength();
		// workaround: DistanceToPathEnd is bogus values. Using true distance to anim target pos instead.
		pAnimTarget->allowActivation = m_state.GetDistanceToPathEnd() < criticalDistance && distance < criticalDistance;
	}

	m_animTargetSpeedCounter -= m_animTargetSpeedCounter!=0;

 	// speed control
	if (hasMoveTarget)
	{
		Vec3 desiredMovement = moveTarget - playerPos;
		if (!m_pPlayer->InZeroG())
		{
			// should do the rotation thing?
			desiredMovement.z = 0;
		}
		float distance = desiredMovement.len();

		if (distance > 0.01f) // need to have somewhere to move, to actually move :)
		{
			// speed at which things are "guaranteed" to stop... 0.03f is a fudge factor to ensure this
			static const float STOP_SPEED = MIN_DESIRED_SPEED-0.03f;

			desiredMovement /= distance;
			float desiredSpeed = 1.0f;
			if (m_state.HasDesiredSpeed())
				desiredSpeed = m_state.GetDesiredSpeed();

			if (pAnimTarget && pAnimTarget->preparing)  
			{
				desiredSpeed = std::max(desiredSpeed, m_desiredSpeed);
				desiredSpeed = std::max(1.0f, desiredSpeed);

				bool isNavigationalSO = (pAnimTarget->isNavigationalSO != 0);

				float frameTimeHi = 0.025f;
				float frameTimeLo = 0.1f;
				float finalSpeedFraction = CLAMP((frameTime - frameTimeHi) / (frameTimeLo - frameTimeHi), 0.0f, 1.0f);
				float finalSpeed = isNavigationalSO ? LERP(3.0f, 0.0f, finalSpeedFraction) : LERP(1.0f, 0.0f, finalSpeedFraction);
				float antiOscilationSpeedFraction = 1.0f - CLAMP(distance / 1.0f, 0.0f, 1.0f);
				float antiOscilationSpeed = LERP(desiredSpeed, finalSpeed, antiOscilationSpeedFraction);
				float approachSpeed = iszero(frameTime) ? 0.0f : (distance / frameTime) * 0.4f;
/*
				if (isNavigationalSO)
					approachSpeed = max(approachSpeed, 2.0f);
*/
				desiredSpeed = std::min(desiredSpeed, approachSpeed);
				//desiredSpeed = antiOscilationSpeed;
			}
			if (pAnimTarget && (pAnimTarget->activated || m_animTargetSpeedCounter) && m_animTargetSpeed >= 0.0f)
			{
				desiredSpeed = m_animTargetSpeed;
        if (pAnimTarget->activated)
  				m_animTargetSpeedCounter = 4;
			}

			float stanceSpeed=m_pPlayer->GetStanceMaxSpeed(m_pPlayer->GetStance());
			NETINPUT_TRACE(m_pPlayer->GetEntityId(), desiredSpeed);
			NETINPUT_TRACE(m_pPlayer->GetEntityId(), stanceSpeed);
			NETINPUT_TRACE(m_pPlayer->GetEntityId(), desiredMovement);
			if ((desiredSpeed > MIN_DESIRED_SPEED) && stanceSpeed>0.001f)
			{
				//pRend->GetIRenderAuxGeom()->DrawLine( playerPos, ColorF(1,1,1,1), playerPos + desiredMovement, ColorF(1,1,1,1) );

				//calculate the desired speed amount (0-1 length) in world space
				params.desiredVelocity = desiredMovement * desiredSpeed / stanceSpeed;
				viewFollowMovement = 1.0f; //5.0f * desiredSpeed / stanceSpeed;
				//
				//and now, after used it for the viewFollowMovement convert it to the Actor local space
				moveDirection = params.desiredVelocity.GetNormalizedSafe(ZERO);
				params.desiredVelocity = m_pPlayer->GetEntity()->GetWorldRotation().GetInverted() * params.desiredVelocity;
			}
		}
	}

/*	if (m_state.HasAimTarget())
	{
		float aimLength = (m_currentMovementState.weaponPosition - m_state.GetAimTarget()).GetLengthSquared();
		float aimLimit = 9.0f * 9.0f;
		if (aimLength < aimLimit)
		{
			float mult = aimLength / aimLimit;
			viewFollowMovement *= mult * mult;
		}
	}*/
	float distanceToEnd(m_state.GetDistanceToPathEnd()); // TODO: Warning, a comment above say this function returns incorrect values.
/*	if (distanceToEnd>0.001f)
	{
		float lookOnPathLimit(7.0f);
		if (distanceToEnd<lookOnPathLimit)
		{
			float mult(distanceToEnd/lookOnPathLimit);
			viewFollowMovement *= mult * mult;
			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere( playerPos, 0.5f, ColorF(1,1,1,1) );
		}
		//pRend->Draw2dLabel( 100, 100, 1.0f, yellow, false, "distance:%.1f", distanceToEnd);
	}*/

	if (allowStrafing)
		viewFollowMovement = 0.0f;

	if (!hasBodyTarget)
	{
		if (m_state.HasBodyTarget())
		{
			bodyTarget = m_state.GetBodyTarget();
			bodyTargetType = "requested";
		}
		else if (hasMoveTarget)
		{
			bodyTarget = playerPos + moveDirection;
			bodyTargetType = "movement";
		}
		else
		{
			bodyTarget = playerPos + currentBodyDirection;
			bodyTargetType = "current";
		}
	}

	// look and aim direction
	Vec3 tgt;

	Vec3 eyePosition(playerPos.x, playerPos.y, m_currentMovementState.eyePosition.z);

	bool hasLookTarget = false;
	tgt = eyePosition + 5.0f * currentBodyDirection;
	const char * lookType = "current";
	if (pAnimTarget && pAnimTarget->preparing)
	{
		hasLookTarget = false;
		tgt = pAnimTarget->position + 3.0f * (pAnimTarget->orientation * FORWARD_DIRECTION) + Vec3(0, 0, 1.5);
		lookType = "exactpositioning";
	}
	else if (pAnimTarget && pAnimTarget->activated)
	{
		hasLookTarget = false;
		tgt = eyePosition + 5.0f * currentBodyDirection;
		lookType = "animation";
	}
	else if (m_state.HasLookTarget())
	{
		hasLookTarget = true;
		tgt = m_state.GetLookTarget();
		lookType = "look";
	}
	else if (m_state.HasAimTarget())
	{
		hasLookTarget = true;
		tgt = m_state.GetAimTarget();
		lookType = "aim";

		/*		if (m_state.HasFireTarget() && m_state.GetFireTarget().Dot(m_state.GetAimTarget()) < cry_cosf(MAX_FIRE_TARGET_DELTA_ANGLE))
		{
		tgt = m_state.GetFireTarget();
		lookType = "fire";
		}*/
	}
	else if (hasMoveTarget)
	{
		Vec3 desiredMoveDir = (moveTarget - playerPos).GetNormalizedSafe(ZERO);
		if (moveDirection.Dot(desiredMoveDir) < ANTICIPATION_COSINE_ANGLE)
		{
			hasLookTarget = true;
			tgt = moveTarget;
			lookType = "anticipate";
		}
	}
	else if (m_lookInterpolator.HasTarget(now, LOOK_TIME))
	{
		tgt = m_lookInterpolator.GetTarget();
		lookType = "lastLook";
	}
//	AdjustForMovement( tgt, moveTarget, playerPos, viewFollowMovement );

	Vec3 lookTarget = tgt;

	// Not all of these arguments are used (stupid function).
	m_lookInterpolator.TargetValue( tgt, now, LOOK_TIME, frameTime, hasLookTarget, playerPos, MAX_LOOK_AT_ANGULAR_VELOCITY );

	bool hasAimTarget = false;
	const char * aimType = lookType;
	if (pAnimTarget && pAnimTarget->preparing)
	{
		hasAimTarget = true;
		tgt = pAnimTarget->position + 3.0f * (pAnimTarget->orientation * FORWARD_DIRECTION) + Vec3(0, 0, 1.5);
		aimType = "animation";
	}
	else if (m_state.HasAimTarget())
	{
		hasAimTarget = true;
		tgt = m_state.GetAimTarget();
		aimType = "aim";
//		AdjustForMovement( tgt, moveTarget, playerPos, viewFollowMovement );
	}

	Vec3 aimTarget = tgt;

	//if ((pAnimTarget == NULL) || (!pAnimTarget->activated)) // (Dejan was not sure about this yet, for leaning and such stuff.)
		m_aimInterpolator.TargetValue( tgt, now, AIM_TIME, frameTime, hasAimTarget, playerPos, MAX_AIM_AT_ANGULAR_VELOCITY );

	const char * ikType = "none";
	ColorB dbgClr(0,255,0,255);
	ColorB * pDbgClr = g_pGame->GetCVars()->g_debugaimlook? &dbgClr : NULL;
	//if (!m_state.HasNoAiming() && m_aimInterpolator.HasTarget( now, AIM_TIME ))	// AIM IK

	bool swimming = (m_pPlayer->GetStance() == STANCE_SWIM);

	bool hasControl = !pAnimTarget || !pAnimTarget->activated && !pAnimTarget->preparing;
	hasControl = hasControl && (!pEntity->GetAI() || pEntity->GetAI()->IsEnabled());

	m_aimClamped = false;

	if (!m_pPlayer->IsClient() && hasControl)
	{
		if (((SPlayerStats*)m_pPlayer->GetActorStats())->mountedWeaponID)
		{

			if (hasAimTarget)
				params.aimTarget = aimTarget;
			else
				params.aimTarget = lookTarget;

			// Luciano - prevent snapping of aim direction out of existing horizontal angle limits
			float limitH = m_pPlayer->GetActorParams()->vLimitRangeH;
			IEntity *pWeaponEntity = GetISystem()->GetIEntitySystem()->GetEntity(m_pPlayer->GetActorStats()->mountedWeaponID);

			// m_currentMovementState.weaponPosition is offset to the real weapon position
			Vec3 weaponPosition (pWeaponEntity ? pWeaponEntity->GetWorldPos() : m_currentMovementState.weaponPosition); 
			
			if(limitH > 0 && limitH < gf_PI)
			{
				Vec3 aimDirection(params.aimTarget - weaponPosition);//m_currentMovementState.weaponPosition);
				Vec3 limitAxisY(m_pPlayer->GetActorParams()->vLimitDir);
				limitAxisY.z = 0;
				Vec3 limitAxisX(limitAxisY.y, -limitAxisY.x, 0);
				limitAxisX.NormalizeSafe(Vec3Constants<float>::fVec3_OneX);
				limitAxisY.NormalizeSafe(Vec3Constants<float>::fVec3_OneY);
				float x = limitAxisX.Dot(aimDirection);
				float y = limitAxisY.Dot(aimDirection);
				float len = cry_sqrtf(sqr(x) + sqr(y));
				len = max(len,2.f);// bring the aimTarget away for degenerated cases when it's between firepos and weaponpos
				float angle = cry_atan2f(x, y);
				if (angle < -limitH || angle > limitH)
				{
					Limit(angle, -limitH, limitH);
					x = sinf(angle) * len;
					y = cosf(angle) * len;
					params.aimTarget = weaponPosition + x* limitAxisX + y * limitAxisY + Vec3(0,0, aimDirection.z);
					m_aimClamped = true;
				}

			}

			float limitUp = m_pPlayer->GetActorParams()->vLimitRangeVUp;
			float limitDown = m_pPlayer->GetActorParams()->vLimitRangeVDown;
			if(limitUp!=0 || limitDown != 0)
			{
				Vec3 aimDirection(params.aimTarget - weaponPosition);//m_currentMovementState.weaponPosition);
				Vec3 limitAxisXY(aimDirection);
				limitAxisXY.z = 0;
				Vec3 limitAxisZ(Vec3Constants<float>::fVec3_OneZ);

				limitAxisXY.NormalizeSafe(Vec3Constants<float>::fVec3_OneY);
				float z = limitAxisZ.Dot(aimDirection);
				float x = limitAxisXY.Dot(aimDirection);
				float len = cry_sqrtf(sqr(z) + sqr(x));
				len = max(len,2.f); // bring the aimTarget away for degenerated cases when it's between firepos and weaponpos
				float angle = cry_atan2f(z, x);
				if (angle < limitDown && limitDown!=0 || angle > limitUp && limitUp !=0)
				{
					Limit(angle, limitDown, limitUp);
					z = sinf(angle) * len;
					x = cosf(angle) * len;
					params.aimTarget = weaponPosition + z* limitAxisZ + x * limitAxisXY;
					m_aimClamped = true;
				}
			}
			params.aimIK = true;
			params.lookIK = true;

			lookType = aimType = bodyTargetType = "mountedweapon";
		}
		else
		{
			Vec3 limitDirection = pEntity->GetRotation().GetColumn1();
			Vec3 weaponPos(playerPos.x, playerPos.y, m_currentMovementState.weaponPosition.z);

			const float LOOK_CONE_FOV = DEG2RAD(80.0f);
			const float AIM_CONE_FOV = DEG2RAD(m_pPlayer->IsPlayer() ? 180.0f : 70.0f);

			if (g_pGame->GetCVars()->g_debugaimlook)
			{
				DebugDrawWireFOVCone(gEnv->pRenderer, weaponPos, limitDirection, 2.0f, AIM_CONE_FOV, ColorB(255,255,255));
				DebugDrawWireFOVCone(gEnv->pRenderer, eyePosition, limitDirection, 3.0f, LOOK_CONE_FOV, ColorB(0,196,255));
			}

			if (hasLookTarget)
			{
				params.lookTarget = lookTarget;
				bool lookClamped = ClampTargetPointToCone(params.lookTarget, eyePosition, limitDirection, LOOK_CONE_FOV);
				if (!lookClamped)
				{
					ikType = "look";
					params.lookIK = true;
				}
				if (m_state.AllowStrafing() || !hasMoveTarget)
				{
					bodyTarget = params.lookTarget;
					bodyTargetType = "look";
				}
			}

			if (hasAimTarget && !swimming)
			{
				params.aimTarget = aimTarget;
				m_aimClamped = ClampTargetPointToCone(params.aimTarget, weaponPos, limitDirection, AIM_CONE_FOV);
				if (!m_aimClamped)
				{
					ikType = "aim";
					params.aimIK = !m_aimClamped;
				}
				if (m_state.AllowStrafing() || !hasMoveTarget)
				{
					bodyTarget = params.aimTarget;
					bodyTargetType = "aim";
				}
			}
		
/*
		if (!m_state.HasNoAiming() && m_aimInterpolator.HasTarget( now, AIM_TIME ) && !swimming)
		{
			params.aimIK = m_aimInterpolator.GetTarget( 
				params.aimTarget, 
				bodyTarget, 
				Vec3( playerPos.x, playerPos.y, m_currentMovementState.weaponPosition.z ),
				moveDirection, 
				animBodyDirection, 
				entDirection,
				MAX_AIM_TURN_ANGLE, 
				m_state.GetDistanceToPathEnd(), 
				viewFollowMovement, 
				pDbgClr,
				&bodyTargetType );
			ikType = "aim";
		}
		else if (m_lookInterpolator.HasTarget( now, LOOK_TIME ))	// Look IK
		{
			params.lookIK = m_lookInterpolator.GetTarget( 
				params.lookTarget, 
				bodyTarget, 
				Vec3( playerPos.x, playerPos.y, m_currentMovementState.eyePosition.z ),
				moveDirection, 
				animBodyDirection, 
				entDirection,
				MAX_HEAD_TURN_ANGLE, 
				m_state.GetDistanceToPathEnd(), 
				viewFollowMovement, 
				pDbgClr,
				&bodyTargetType );
			ikType = "look";
		}
*/
		}	
	}


	Vec3 viewDir = ((bodyTarget - playerPos).GetNormalizedSafe(ZERO));
	if (!m_pPlayer->IsClient() && viewDir.len2() > 0.01f)
	{
		//Vec3 localVDir(m_pPlayer->GetEntity()->GetWorldRotation().GetInverted() * viewDir);
		Vec3 localVDir(m_pPlayer->GetViewQuat().GetInverted() * viewDir);
		
		CHECKQNAN_VEC(localVDir);

		if ((pAnimTarget == NULL) || (!pAnimTarget->activated))
		{
			params.deltaAngles.x += asin(CLAMP(localVDir.z,-1,1));
			params.deltaAngles.z += cry_atan2f(-localVDir.x,localVDir.y);
		}

		CHECKQNAN_VEC(params.deltaAngles);

		static float maxDeltaAngleRateNormal = DEG2RAD(180.0f);
		static float maxDeltaAngleRateAnimTarget = DEG2RAD(360.0f);
		static float maxDeltaAngleMultiplayer = DEG2RAD(3600.0f);

		float maxDeltaAngleRate = maxDeltaAngleRateNormal;

		if (gEnv->bMultiplayer)
		{
			maxDeltaAngleRate = maxDeltaAngleMultiplayer;
		}
		else
		{
			// Less clamping when approaching animation target.
			if (pAnimTarget && pAnimTarget->preparing)
			{
				float	distanceFraction = CLAMP(pAnimTarget->position.GetDistance(playerPos) / 0.5f, 0.0f, 1.0f);
				float	animTargetTurnRateFraction = 1.0f - distanceFraction;
				animTargetTurnRateFraction = 0.0f;
				maxDeltaAngleRate = LERP(maxDeltaAngleRate, maxDeltaAngleRateAnimTarget, animTargetTurnRateFraction);
			}
		}

		for (int i=0; i<3; i++)
			Limit(params.deltaAngles[i], -maxDeltaAngleRate * frameTime, maxDeltaAngleRate * frameTime);

		CHECKQNAN_VEC(params.deltaAngles);
	}

	if (m_state.HasDeltaRotation())
	{
		params.deltaAngles += m_state.GetDeltaRotation();
		CHECKQNAN_VEC(params.deltaAngles);
		ikType = "mouse";
	}


	if (g_pGame->GetCVars()->g_debugaimlook)
	{
		pRend->Draw2dLabel( 10, y, 1.5f, white, false, 
			"%s:  body=%s   look=%s   aim=%s   rotik=%s   move=%s   delta ang=(%3.2f, %3.2f, %3.2f)", 
			pEntity->GetName(), bodyTargetType, aimType, lookType, ikType, moveTargetType, 
			params.deltaAngles.x, params.deltaAngles.y, params.deltaAngles.z );
		y += 15;
		if (m_state.GetDistanceToPathEnd() >= 0.0f)
		{
			pRend->Draw2dLabel( 10, y, 1.5f, yellow, false, "distanceToEnd: %f (%f)", m_state.GetDistanceToPathEnd(), moveTarget.GetDistance(playerPos) );
			y += 15;
		}

		if (m_state.HasAimTarget())
		{
			pRend->GetIRenderAuxGeom()->DrawLine( m_currentMovementState.weaponPosition, ColorF(1,1,1,1), params.aimTarget+Vec3(0,0,0.05f), ColorF(1,1,1,1), 3.0f);
			pRend->GetIRenderAuxGeom()->DrawLine( m_currentMovementState.weaponPosition, ColorF(1,1,1,0.3f), m_state.GetAimTarget(), ColorF(1,1,1,0.3f));
		}
	}

	// process incremental movement
	if (m_state.HasDeltaMovement())
	{
		params.desiredVelocity += m_state.GetDeltaMovement();
	}

	// stance control
	if (pAnimTarget && pAnimTarget->activated && m_targetStance != STANCE_NULL)
	{
		m_state.SetStance( m_targetStance );
	}

	if (m_state.HasStance())
	{
		params.stance = m_state.GetStance();
	}

	// leaning
	if (m_state.HasLean())
		params.desiredLean = m_state.GetLean();
	else
		params.desiredLean = 0.0f;

	params.jump = m_state.ShouldJump();
	m_state.ClearJump();

	params.strengthJump=ShouldStrengthJump();
	ClearStrengthJump();

	// TODO: This should probably be calculate BEFORE it is used (above), or the previous frame's value is used.
	m_desiredSpeed = params.desiredVelocity.GetLength() * m_pPlayer->GetStanceMaxSpeed( m_pPlayer->GetStance() );

	m_state.RemoveDeltaRotation();
	m_state.RemoveDeltaMovement();

  if (params.aimIK)
  {
    m_aimTarget = params.aimTarget;
    // if aiming force looking as well
    // In spite of what it may look like with eye/look IK, the look/aim target tends
    // to be right on/in the target's head so doesn't need any extra offset
    params.lookTarget = params.aimTarget;
    params.lookIK = true;
    m_lookTarget = params.lookTarget;
  }
  else
  {
    m_aimTarget = m_lookTarget;
    if (params.lookIK)
		  m_lookTarget = params.lookTarget;
	  else
		  m_lookTarget = m_currentMovementState.eyePosition + m_pPlayer->GetEntity()->GetRotation() * FORWARD_DIRECTION * 10.0f;
  }

  m_usingAimIK = params.aimIK;
  m_usingLookIK = params.lookIK;

	if (m_state.HasFireTarget())
	{
		m_fireTarget = m_state.GetFireTarget();
	}

	m_aimInterpolator.Update(frameTime);
	m_lookInterpolator.Update(frameTime);

	float pseudoSpeed = 0.0f;
	if (m_state.HasPseudoSpeed())
		pseudoSpeed = m_state.GetPseudoSpeed();
	if (pAnimTarget && pAnimTarget->preparing && pseudoSpeed < 0.4f)
		pseudoSpeed = 0.4f;
	if (params.stance == STANCE_CROUCH && pseudoSpeed > 0.4f && m_pPlayer->IsPlayer())
		pseudoSpeed = 0.4f;
	m_pPlayer->GetAnimationGraphState()->SetInput(m_inputPseudoSpeed, pseudoSpeed);

	static float PredictionDeltaTime = 0.4f;
	bool hasPrediction = m_state.HasPrediction() && (m_state.GetPrediction().nStates > 0);
	bool hasAnimTarget = (pAnimTarget != NULL) && (pAnimTarget->activated || pAnimTarget->preparing);
	if (hasPrediction && !hasAnimTarget)
	{
		params.prediction = m_state.GetPrediction();
	}
	else
	{
    params.prediction.nStates = 0;
	 }

/*
#ifdef USER_dejan
	// Debug Render & Text
	{
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(moveTarget, 0.2f, ColorB(128,255,0, 255), true);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(playerPos, ColorB(255,255,255, 128), moveTarget, ColorB(128,255,0, 128), 0.4f);

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(bodyTarget, 0.2f, ColorB(255,128,0, 128), true);
		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(moveTarget, ColorB(255,255,255, 128), bodyTarget, ColorB(255,128,0, 128), 0.2f);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(playerPos, ColorB(255,255,255, 128), bodyTarget, ColorB(255,128,0, 128), 0.4f);
	}
#endif
*/

	NETINPUT_TRACE(m_pPlayer->GetEntityId(), params.desiredVelocity);
	NETINPUT_TRACE(m_pPlayer->GetEntityId(), params.desiredLean);
	NETINPUT_TRACE(m_pPlayer->GetEntityId(), Vec3(params.deltaAngles));
	NETINPUT_TRACE(m_pPlayer->GetEntityId(), params.sprint);

	return true;
}

void CPlayerMovementController::UpdateMovementState( SMovementState& state )
{
	IEntity * pEntity = m_pPlayer->GetEntity();
	ICharacterInstance * pCharacter = pEntity->GetCharacter(0);
	if (!pCharacter)
		return;
	ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();
	//FIXME:
	if (!pSkeletonAnim)
		return;

	int boneEyeL = m_pPlayer->GetBoneID(BONE_EYE_L);
	int boneEyeR = m_pPlayer->GetBoneID(BONE_EYE_R);
	int boneHead = m_pPlayer->GetBoneID(BONE_HEAD);
	int boneWeapon = m_pPlayer->GetBoneID(BONE_WEAPON);

	Vec3	forward(1,0,0);
	bool isCharacterVisible = pCharacter->IsCharacterVisible() != 0;

	if (m_pPlayer->IsPlayer())
	{
		// PLAYER CHARACTER

		Vec3 viewOfs(m_pPlayer->GetStanceViewOffset(m_pPlayer->GetStance()));
		state.eyePosition = pEntity->GetWorldPos() + m_pPlayer->GetBaseQuat() * viewOfs;

		// E3HAX E3HAX E3HAX
		if (((SPlayerStats *)m_pPlayer->GetActorStats())->mountedWeaponID)
			state.eyePosition = GetISystem()->GetViewCamera().GetPosition();

		if (!m_pPlayer->IsClient()) // marcio: fixes the eye direction for remote players
			state.eyeDirection = (m_lookTarget-state.eyePosition).GetNormalizedSafe(state.eyeDirection);
		else if(((SPlayerStats *)m_pPlayer->GetActorStats())->FPWeaponSwayOn) // Beni - Fixes aim direction when zoom sway applies
			state.eyeDirection = GetISystem()->GetViewCamera().GetViewdir();
		else
			state.eyeDirection = m_pPlayer->GetViewQuatFinal().GetColumn1();


		state.animationEyeDirection = state.eyeDirection;
		state.weaponPosition = state.eyePosition;
		state.fireDirection = state.aimDirection = state.eyeDirection;
		state.fireTarget = m_fireTarget;
	}
	else
	{
		// AI CHARACTER

		Quat	orientation = pEntity->GetWorldRotation();
		forward = orientation.GetColumn1();
		Vec3	entityPos = pEntity->GetWorldPos();
		assert( entityPos.IsValid() );
		Vec3	constrainedLookDir = m_lookTarget - entityPos;
		Vec3	constrainedAimDir = m_aimTarget - entityPos;
		constrainedLookDir.z = 0.0f;
		constrainedAimDir.z = 0.0f;

		constrainedAimDir = constrainedAimDir.GetNormalizedSafe(constrainedLookDir.GetNormalizedSafe(forward));
		constrainedLookDir = constrainedLookDir.GetNormalizedSafe(forward);

		const SStanceInfo*	stanceInfo = m_pPlayer->GetStanceInfo(m_pPlayer->GetStance());

		Matrix33	lookRot;
		lookRot.SetRotationVDir(constrainedLookDir);
		state.eyePosition = entityPos + lookRot.TransformVector(m_pPlayer->GetEyeOffset());

		Matrix33	aimRot;
		aimRot.SetRotationVDir(constrainedAimDir);
		state.weaponPosition = entityPos + aimRot.TransformVector(m_pPlayer->GetWeaponOffset());
			
		state.upDirection = orientation.GetColumn2();

		state.eyeDirection = (m_lookTarget - state.eyePosition).GetNormalizedSafe(forward); //(lEyePos - posHead).GetNormalizedSafe(FORWARD_DIRECTION);
		state.animationEyeDirection = state.eyeDirection;
		state.aimDirection = (m_aimTarget - state.weaponPosition).GetNormalizedSafe((m_lookTarget - state.weaponPosition).GetNormalizedSafe(forward)); //pEntity->GetRotation() * dirWeapon;
		state.fireTarget = m_fireTarget;
		state.fireDirection = (state.fireTarget - state.weaponPosition).GetNormalizedSafe(forward);
	}

	//changed by ivo: most likely this doesn't work any more
	//state.movementDirection = pEntity->GetRotation() * pSkeleton->GetCurrentBodyDirection();
	state.movementDirection = pEntity->GetRotation().GetColumn1();


	if (m_pPlayer->GetLastRequestedVelocity().len2() > 0.01f)
		state.movementDirection = m_pPlayer->GetLastRequestedVelocity().GetNormalized();

	//changed by ivo: most likely this doesn't work any more
	//state.bodyDirection = pEntity->GetRotation() * pSkeleton->GetCurrentBodyDirection();
	// [kirill] when AI at MG need to update body/movementDirection coz entity is not moved/rotated AND set AIPos/weaponPs to weapon
	if (!m_pPlayer->IsPlayer() && m_pPlayer->GetActorStats() && m_pPlayer->GetActorStats()->mountedWeaponID)
	{
		
		IEntity *pWeaponEntity = GetISystem()->GetIEntitySystem()->GetEntity(m_pPlayer->GetActorStats()->mountedWeaponID);
		if(pWeaponEntity)
		{
			state.eyePosition.x = pWeaponEntity->GetWorldPos().x;
			state.eyePosition.y = pWeaponEntity->GetWorldPos().y;
			state.weaponPosition = state.eyePosition;
			if(	m_pPlayer->GetEntity()->GetAI() &&
					m_pPlayer->GetEntity()->GetAI()->GetProxy() &&
					m_pPlayer->GetEntity()->GetAI()->GetProxy()->GetCurrentWeapon() )
			{
				IWeapon * pMG = m_pPlayer->GetEntity()->GetAI()->GetProxy()->GetCurrentWeapon();
				state.weaponPosition = pMG->GetFiringPos( m_fireTarget ); //pWeaponEntity->GetWorldPos();
			}
			// need to recalculate aimDirection, since weaponPos is changed
			state.aimDirection = (m_aimTarget - state.weaponPosition).GetNormalizedSafe((m_lookTarget - state.weaponPosition).GetNormalizedSafe(forward)); //pEntity->GetRotation() * dirWeapon;
		}
		
		state.bodyDirection = state.aimDirection;
		state.movementDirection = state.aimDirection;
	}
	else
		state.bodyDirection = pEntity->GetWorldRotation().GetColumn1();

	state.lean = m_pPlayer->GetActorStats() ? ((SPlayerStats*)m_pPlayer->GetActorStats())->leanAmount : 0.0f;

	state.atMoveTarget = m_atTarget;
	state.desiredSpeed = m_desiredSpeed;
	state.stance = m_pPlayer->GetStance();
	state.upDirection = pEntity->GetWorldRotation().GetColumn2();
//	state.minSpeed = MIN_DESIRED_SPEED;
//	state.normalSpeed = m_pPlayer->GetStanceNormalSpeed(state.stance);
//	state.maxSpeed = m_pPlayer->GetStanceMaxSpeed(state.stance); 

	/*
  Vec2 minmaxSpeed = Vec2(0, 0);
	if(m_pPlayer->GetAnimationGraphState())	//might get here during loading before AG is serialized
		minmaxSpeed = m_pPlayer->GetAnimationGraphState()->GetQueriedStateMinMaxSpeed();
  state.minSpeed = minmaxSpeed[0];
  state.maxSpeed = minmaxSpeed[1];
  state.normalSpeed = 0.5f*(state.minSpeed + state.maxSpeed);
	if (state.maxSpeed < state.minSpeed)
	{
//		assert(state.stance == STANCE_NULL);
		state.maxSpeed = state.minSpeed;
		//if (!g_pGame->GetIGameFramework()->IsEditing())
		//	GameWarning("%s In STANCE_NULL - movement speed is clamped", pEntity->GetName());
	}
	if (state.normalSpeed < state.minSpeed)
		state.normalSpeed = state.minSpeed;
*/

	state.minSpeed = -1.0f;
	state.maxSpeed = -1.0f;
	state.normalSpeed = -1.0f;

	state.m_StanceSize = m_pPlayer->GetStanceInfo(state.stance)->GetStanceBounds();

	state.pos = pEntity->GetWorldPos();
	//FIXME:some E3 work around
	if (m_pPlayer->GetActorStats() && m_pPlayer->GetActorStats()->mountedWeaponID)
	{
		if (m_pPlayer->IsPlayer())
			state.isAiming = true;
		else
			state.isAiming = !m_aimClamped; // AI
	}
	else if (isCharacterVisible)
		state.isAiming = pCharacter->GetISkeletonPose()->GetAimIKBlend() > 0.99f;
	else
		state.isAiming = true;

	state.isFiring = (m_pPlayer->GetActorStats()->inFiring>=10.f);

	// TODO: remove this
	//if (m_state.HasAimTarget())
	//	state.aimDirection = (m_state.GetAimTarget() - state.handPosition).GetNormalizedSafe();

	state.isAlive = (m_pPlayer->GetHealth()>0);

	IVehicle *pVehicle = m_pPlayer->GetLinkedVehicle();
	if (pVehicle)
	{
		IMovementController *pVehicleMovementController = pVehicle->GetPassengerMovementController(m_pPlayer->GetEntityId());
		if (pVehicleMovementController)
			pVehicleMovementController->GetMovementState(state);
	}	

	const IAnimatedCharacter* pAC = m_pPlayer->GetAnimatedCharacter();
	state.slopeAngle = (pAC != NULL) ? pAC->GetGroundSlopeAngle() : 0.0f;
}

bool CPlayerMovementController::GetStanceState( EStance stance, float lean, bool defaultPose, SStanceState& state )
{
	IEntity * pEntity = m_pPlayer->GetEntity();
	const SStanceInfo*	stanceInfo = m_pPlayer->GetStanceInfo(stance);
	if(!stanceInfo)
		return false;

	Quat	orientation = pEntity->GetWorldRotation();
	Vec3	forward = orientation.GetColumn1();
	Vec3	entityPos = pEntity->GetWorldPos();

	state.pos = entityPos;
	state.m_StanceSize = stanceInfo->GetStanceBounds();
	state.m_ColliderSize = stanceInfo->GetColliderBounds();
	state.lean = lean;	// pass through

	if(defaultPose)
	{
		state.eyePosition = entityPos + stanceInfo->GetViewOffsetWithLean(lean);
		state.weaponPosition = entityPos + m_pPlayer->GetWeaponOffsetWithLean(stance, lean, m_pPlayer->GetEyeOffset());
		state.upDirection.Set(0,0,1);
		state.eyeDirection = FORWARD_DIRECTION;
		state.aimDirection = FORWARD_DIRECTION;
		state.fireDirection = FORWARD_DIRECTION;
		state.bodyDirection = FORWARD_DIRECTION;
	}
	else
	{
		Vec3	constrainedLookDir = m_lookTarget - entityPos;
		Vec3	constrainedAimDir = m_aimTarget - entityPos;
		constrainedLookDir.z = 0.0f;
		constrainedAimDir.z = 0.0f;

		constrainedAimDir = constrainedAimDir.GetNormalizedSafe(constrainedLookDir.GetNormalizedSafe(forward));
		constrainedLookDir = constrainedLookDir.GetNormalizedSafe(forward);

		Matrix33	lookRot;
		lookRot.SetRotationVDir(constrainedLookDir);
		state.eyePosition = entityPos + lookRot.TransformVector(stanceInfo->GetViewOffsetWithLean(lean));

		Matrix33	aimRot;
		aimRot.SetRotationVDir(constrainedAimDir);
		state.weaponPosition = entityPos + aimRot.TransformVector(m_pPlayer->GetWeaponOffsetWithLean(stance, lean, m_pPlayer->GetEyeOffset()));

		state.upDirection = orientation.GetColumn2();

		state.eyeDirection = (m_lookTarget - state.eyePosition).GetNormalizedSafe(forward);
		state.aimDirection = (m_aimTarget - state.weaponPosition).GetNormalizedSafe((m_lookTarget - state.weaponPosition).GetNormalizedSafe(forward));
		state.fireDirection = state.aimDirection;
		state.bodyDirection = forward;
	}

	return true;
}

Vec3 CPlayerMovementController::ProcessAimTarget(const Vec3 &newTarget,float frameTime)
{
	Vec3 delta(m_aimTargets[0] - newTarget);
	m_aimTargets[0] = Vec3::CreateLerp(m_aimTargets[0],newTarget,min(1.0f,frameTime*max(1.0f,delta.len2()*1.0f)));

	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( newTarget, ColorF(1,0,0,1), m_aimTargets[0], ColorF(1,0,0,1) );

	return m_aimTargets[0];
	/*int targets(m_aimTargets.size());

	m_aimTargets[m_aimTargetsCount++] = newTarget;
	bool exit;
	do
	{
		exit = true;

		if (m_aimTargetsCount == m_aimTargetsIterator)
		{
			exit = false;
			++m_aimTargetsCount;
		}
		if (m_aimTargetsCount>=targets)
			m_aimTargetsCount = 0;
	}
	while(!exit);

	float holdTime(0.55f);
	m_aimNextTarget -= frameTime;
	if (m_aimNextTarget<0.001f)
	{
		m_aimNextTarget = holdTime;

		++m_aimTargetsIterator;
		if (m_aimTargetsIterator>=targets)
			m_aimTargetsIterator = 0;
	}

	int nextIter(m_aimTargetsIterator+1);
	if (nextIter>=targets)
		nextIter = 0;

	return Vec3::CreateLerp(m_aimTargets[nextIter],m_aimTargets[m_aimTargetsIterator],m_aimNextTarget*(1.0f/holdTime));*/
}

void CPlayerMovementController::Release()
{
	delete this;
}

void CPlayerMovementController::IdleUpdate(float frameTime)
{
	m_idleChecker.Update(frameTime);
}

bool CPlayerMovementController::GetStats(IActorMovementController::SStats& stats)
{
	stats.idle = m_idleChecker.IsIdle();
	return true;
}

CPlayerMovementController::CIdleChecker::CIdleChecker() : m_pMC(0)
{
}


void CPlayerMovementController::CIdleChecker::Reset(CPlayerMovementController* pMC)
{
	m_pMC = pMC;
	m_timeToIdle = IDLE_CHECK_TIME_TO_IDLE;
	m_movementTimeOut = IDLE_CHECK_MOVEMENT_TIMEOUT;
	m_bInIdle = false;
	m_bHaveValidMove = false;
	m_bHaveInteresting = false;
	m_frameID = -1;
}

void CPlayerMovementController::CIdleChecker::Update(float frameTime)
{
	if (m_pMC->m_pPlayer->IsPlayer())
		return;

	bool goToIdle = false;
	bool wakeUp = false;

	if (m_bHaveValidMove)
	{
		m_movementTimeOut = IDLE_CHECK_MOVEMENT_TIMEOUT;
		m_bHaveValidMove = false;
		wakeUp = true;
	}
	else
	{
		m_movementTimeOut-= frameTime;
		if (m_movementTimeOut <= 0.0f)
		{
			goToIdle = true;
		}
	}

#if 0
	if (m_bHaveInteresting)
	{
		m_timeToIdle = IDLE_CHECK_TIME_TO_IDLE;
		m_bHaveInteresting = false;
		wakeUp = true;
	}
	else
	{
		m_timeToIdle -= frameTime;
		if (m_timeToIdle <= 0.0f)
		{
			goToIdle = true;
		}
	}
#endif

	if (goToIdle && !wakeUp && m_bInIdle == false)
	{
		// turn idle on
#ifdef DEBUG_IDLE_CHECK
		CryLogAlways("Turn Idle ON 0x%p %s", this, m_pMC->m_pPlayer->GetEntity()->GetName());
#endif
		m_bInIdle = true;
	}
	else if (wakeUp && m_bInIdle)
	{
		// turn idle off
#ifdef DEBUG_IDLE_CHECK
		CryLogAlways("Turn Idle OFF 0x%p %s", this, m_pMC->m_pPlayer->GetEntity()->GetName());
#endif
		m_bInIdle = false;
	}

	static int sCurFrame = -1;
	static int sIdles = 0;
	static int sAwake = 0;
	static ICVar* pShowIdleStats = gEnv->pConsole->GetCVar("g_showIdleStats");
	if (pShowIdleStats && pShowIdleStats->GetIVal() != 0)
	{
		int frameId = gEnv->pRenderer->GetFrameID(false);
		if (frameId != sCurFrame)
		{
			float fColor[4] = {1,0,0,1};
			gEnv->pRenderer->Draw2dLabel( 1,22, 1.3f, fColor, false,"Idles=%d Awake=%d", sIdles, sAwake );
			sCurFrame = frameId;
			sIdles = sAwake = 0;
		}
		if (m_bInIdle)
			++sIdles;
		else
			++sAwake;
	}
}

bool CPlayerMovementController::CIdleChecker::Process(SMovementState& movementState, 
																										  CMovementRequest& currentReq,
																										  CMovementRequest& newReq)
{
	if (m_pMC->m_pPlayer->IsPlayer())
		return false;

	IRenderer* pRend = gEnv->pRenderer;
	int nCurrentFrameID = pRend->GetFrameID(false);

	// check if we have been called already in this frame
	// if so, and we already have a validMove request -> early return
	if (nCurrentFrameID != m_frameID)
	{
		m_frameID = nCurrentFrameID;
	}
	else
	{
		if (m_bHaveValidMove)
			return true;
	}

	// no valid move request up to now (not in this frame)
	m_bHaveValidMove = false;

	// an empty moverequest 
	if (currentReq.IsEmpty())
	{
		// CryLogAlways("IdleCheck: Req empty 0x%p %s", this, m_pMC->m_pPlayer->GetEntity()->GetName());
		;
	}
	else if (newReq.HasDeltaMovement() && newReq.HasDeltaRotation())
	{
		m_bHaveValidMove = true;
	}
	else if (newReq.HasMoveTarget() && 
		( (newReq.HasDesiredSpeed() && newReq.GetDesiredSpeed() > 0.001f)
		||(newReq.HasPseudoSpeed() && newReq.GetPseudoSpeed() > 0.001f) ))
	{
		m_bHaveValidMove = true;
	}

	return m_bHaveValidMove;
}

void CPlayerMovementController::CIdleChecker::Serialize(TSerialize &ser)
{
	ser.Value("m_timeToIdle", m_timeToIdle);
	ser.Value("m_movementTimeOut", m_movementTimeOut);
	ser.Value("m_frameID", m_frameID);
	ser.Value("m_bHaveInterest", m_bHaveInteresting);
	ser.Value("m_bInIdle", m_bInIdle);
	ser.Value("m_bHaveValidMove", m_bHaveValidMove);
}

void CPlayerMovementController::Serialize(TSerialize &ser)
{
	if(ser.GetSerializationTarget() != eST_Network)	//basic serialization
	{
		ser.Value("DesiredSpeed", m_desiredSpeed);
		ser.Value("atTarget", m_atTarget);
		ser.Value("m_usingLookIK", m_usingLookIK);
		ser.Value("m_usingAimIK", m_usingAimIK);
		ser.Value("m_aimClamped", m_aimClamped);
		ser.Value("m_lookTarget", m_lookTarget);
		ser.Value("m_aimTarget", m_aimTarget);
		ser.Value("m_animTargetSpeed", m_animTargetSpeed);
		ser.Value("m_animTargetSpeedCounter", m_animTargetSpeedCounter);
		ser.Value("m_fireTarget", m_fireTarget);
		ser.EnumValue("targetStance", m_targetStance, STANCE_NULL, STANCE_LAST);

		ser.Value("NumAimTarget", m_aimTargetsCount);
		int numTargets = m_aimTargets.size();
		ser.Value("ActualNumTarget", numTargets);
		if(ser.IsReading())
		{
			m_aimTargets.clear();
			m_aimTargets.resize(numTargets);
		}
		for(int i = 0; i < numTargets; ++i)
			ser.Value("aimTarget", m_aimTargets[i]);

		ser.Value("m_aimTargetIterator", m_aimTargetsIterator);
		ser.Value("m_aimNextTarget", m_aimNextTarget);

		SMovementState m_currentMovementState;

		ser.BeginGroup("m_currentMovementState");

		ser.Value("bodyDir", m_currentMovementState.bodyDirection);
		ser.Value("aimDir", m_currentMovementState.aimDirection);
		ser.Value("fireDir", m_currentMovementState.fireDirection);
		ser.Value("fireTarget", m_currentMovementState.fireTarget);
		ser.Value("weaponPos", m_currentMovementState.weaponPosition);
		ser.Value("desiredSpeed", m_currentMovementState.desiredSpeed);
		ser.Value("moveDir", m_currentMovementState.movementDirection);
		ser.Value("upDir", m_currentMovementState.upDirection);
		ser.EnumValue("stance", m_currentMovementState.stance, STANCE_NULL, STANCE_LAST);
		ser.Value("Pos", m_currentMovementState.pos);
		ser.Value("eyePos", m_currentMovementState.eyePosition);
		ser.Value("eyeDir", m_currentMovementState.eyeDirection);
		ser.Value("animationEyeDirection", m_currentMovementState.animationEyeDirection);
		ser.Value("minSpeed", m_currentMovementState.minSpeed);
		ser.Value("normalSpeed", m_currentMovementState.normalSpeed);
		ser.Value("maxSpeed", m_currentMovementState.maxSpeed);
		ser.Value("stanceSize.Min", m_currentMovementState.m_StanceSize.min);
		ser.Value("stanceSize.Max", m_currentMovementState.m_StanceSize.max);
		ser.Value("colliderSize.Min", m_currentMovementState.m_ColliderSize.min);
		ser.Value("colliderSize.Max", m_currentMovementState.m_ColliderSize.max);
		ser.Value("atMoveTarget", m_currentMovementState.atMoveTarget);
		ser.Value("isAlive", m_currentMovementState.isAlive);
		ser.Value("isAiming", m_currentMovementState.isAiming);
		ser.Value("isFiring", m_currentMovementState.isFiring);
		ser.Value("isVis", m_currentMovementState.isVisible);

		ser.EndGroup();

		ser.BeginGroup("AimInterpolator");
		m_aimInterpolator.Serialize(ser);
		ser.EndGroup();

		ser.BeginGroup("LookInterpolator");
		m_lookInterpolator.Serialize(ser);
		ser.EndGroup();

		ser.Value("inputDesiredSpeed", m_inputDesiredSpeed);
		ser.Value("inputDersiredAngleZ", m_inputDesiredTurnAngleZ);
		ser.Value("inputStance", m_inputStance);
		ser.Value("inputPseudoSpeed", m_inputPseudoSpeed);

		if(ser.IsReading())
			m_idleChecker.Reset(this);
		m_idleChecker.Serialize(ser);

		if(ser.IsReading())
			UpdateMovementState(m_currentMovementState);
	}
}
