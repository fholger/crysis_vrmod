/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- Aug 2007: Created by Luciano Morpurgo

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include "Shark.h"
#include "GameUtils.h"

#include <IViewSystem.h>
#include <IPhysics.h>
#include <ICryAnimation.h>
#include <ISerialize.h>
#include <IRenderAuxGeom.h>
#include <IEffectSystem.h>
#include <ISound.h>

#include "SharkMovementController.h"

//Vec3 CShark::m_lastSpawnPoint;
/**
* Tries to initialize as much of SMovementRequestParams' data
* as possible from a CMovementRequest instance.
*/
CShark::SMovementRequestParams::SMovementRequestParams(CMovementRequest& request) :
aimLook (false),
// NOTE Nov 9, 2006: <pvl> default values taken from SOBJECTSTATE constructor
bodystate (0),
fDesiredSpeed (1.0f),
eActorTargetPhase (eATP_None),
bExactPositioning (false)    
{	
	aimLook = false;

	vMoveDir.zero();
	vLookTargetPos.zero();
	vAimTargetPos.zero();
	vShootTargetPos.zero();

	if (request.HasLookTarget())
	{
		vLookTargetPos = request.GetLookTarget();
	}
	if (request.HasAimTarget())
	{
		vAimTargetPos = request.GetAimTarget();
		aimLook = true;
	}
	if (request.HasFireTarget())
	{
		vShootTargetPos = request.GetFireTarget();
	}

	fDistanceToPathEnd = request.GetDistanceToPathEnd();

	if (request.HasDesiredSpeed())
		fDesiredSpeed = request.GetDesiredSpeed();

	if (request.HasStance())
	{
		switch (request.GetStance())
		{
		case STANCE_CROUCH:
			bodystate = 1;
			break;
		case STANCE_PRONE:
			bodystate = 2;
			break;
		case STANCE_RELAXED:
			bodystate = 3;
			break;
		case STANCE_STEALTH:
			bodystate = 4;
			break;
		}
	}
}

//debug
void DrawSharkCircle( const Vec3& center, float radius )
{
	Vec3 p0,p1,pos;
	pos = center;
	p0.x = pos.x + radius*sin(0.0f);
	p0.y = pos.y + radius*cos(0.0f);
	p0.z = pos.z;
	float step = 10.0f/180*gf_PI;
	ColorB col(0,255,0,128);

	for (float angle = step; angle < 360.0f/180*gf_PI+step; angle += step)
	{
		p1.x = pos.x + radius*sin(angle);
		p1.y = pos.y + radius*cos(angle);
		p1.z = pos.z;
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(p0,col,p1,col);
		p0 = p1;
	}    
}

inline float CrossZ(const Vec3& a, const Vec3& b)
{
	return a.x * b.y - a.y * b.x;
}

inline float GetFixNum(float a)
{
	return _isnan(a) ? 0:a;
}

inline void FixVec(Vec3& v)
{
	if(_isnan(v.x)||_isnan(v.y)||_isnan(v.z))
		v.zero();
}

bool GetCirclePassingBy(const Vec3& P, const Vec3& Q, const Vec3& Ptan, Vec3& center,float& radius)
{
	// gets the 2D circle passing by two points P,Q and given tangent in P
	Vec3 PQcenter((P+Q)/2);
	Vec3 PQ(Q-P);
	float crossprod = CrossZ(Ptan, PQ);
	Vec3 axis;
	Vec3 PRadius;
	if(crossprod<0)
	{
		axis.y = -PQ.x;
		axis.x = PQ.y;
		axis.z = 0;
		PRadius.y = -Ptan.x;
		PRadius.x = Ptan.y;
		PRadius.z = 0;
	}
	else
	{
		axis.y = PQ.x;
		axis.x = -PQ.y;
		axis.z = 0;
		PRadius.y = Ptan.x;
		PRadius.x = -Ptan.y;
		PRadius.z = 0;
	}
	axis.NormalizeSafe();
	PRadius.NormalizeSafe();

	const float maxlength = 400;
	Vec3 PQAxisEnd = PQcenter + axis*maxlength;
	Lineseg PQAxis(PQcenter,PQAxisEnd );
	Lineseg PRadiusSeg(P,P+PRadius*maxlength);
	float tPQ,tPrad;
	if(Intersect::Lineseg_Lineseg2D(PQAxis,PRadiusSeg,tPQ,tPrad))
	{
		radius = tPrad * maxlength;
		center = P + PRadius * radius;
		return true;
	}
	return false;
}


//====================================================================
// IntersectSweptSphere
// hitPos is optional - may be faster if 0
//====================================================================
/*
bool IntersectSweptSphere(Vec3 *hitPos, float& hitDist, const Lineseg& lineseg, float radius,IPhysicalEntity** pSkipEnts=0, int nSkipEnts=0, int additionalFilter = 0)
{
	IPhysicalWorld* pPhysics = gEnv->pPhysicalWorld;
	primitives::sphere spherePrim;
	spherePrim.center = lineseg.start;
	spherePrim.r = radius;
	Vec3 dir = lineseg.end - lineseg.start;

	geom_contact *pContact = 0;
	geom_contact **ppContact = hitPos ? &pContact : 0;
	int geomFlagsAll=0;
	int geomFlagsAny=rwi_stop_at_pierceable|(geom_colltype_player<<rwi_colltype_bit);

	float d = pPhysics->PrimitiveWorldIntersection(spherePrim.type, &spherePrim, dir, 
		ent_static | ent_terrain | ent_ignore_noncolliding | additionalFilter, ppContact, 
		geomFlagsAll, geomFlagsAny, 0,0,0, pSkipEnts, nSkipEnts);

	if (d > 0.0f)
	{
		hitDist = d;
		if (pContact && hitPos)
			*hitPos = pContact->pt;
		return true;
	}
	else
	{
		return false;
	}
}
*/

// -------------------

CShark::CShark() : 
//m_pTrailAttachment(NULL),
//m_trailSpeedScale(0.f),
m_chosenEscapeDir(ZERO),
m_escapeDir(ZERO)
{	
	ResetValues();
}

void CShark::ResetValues()
{
	m_stats = SSharkStats();

	m_state = S_Sleeping;

	m_velocity = ZERO;
	m_lastCheckedPos = ZERO;
	m_turnSpeed = 0.0f;
	m_turnSpeedGoal = 0.0f;
	m_roll = 0.0f;
	m_weaponOffset = ZERO;
	m_remainingCirclingTime = -1;
	m_targetId = 0;
	m_moveTarget = ZERO;
	m_circleDisplacement = ZERO;
	m_lastPos.zero();
	m_lastRot.zero();
	m_curSpeed = 0;
	m_bCircularTrajectory = false;
	m_turnRadius = 0;
	m_turnAnimBlend = 0.5;
	m_bCircularTrajectory = false;
	m_circleRadius = 12;// dummy but reasonable value to avoid fp exceptions
	m_startPos.zero();

	m_lastSpawnPoint.zero();
	
	m_baseMtx.SetIdentity();// = Matrix33(GetEntity()->GetRotation());
	m_modelQuat.SetIdentity();// = GetEntity()->GetRotation();
	m_viewMtx.SetIdentity();
	m_eyeMtx.SetIdentity();
	m_charLocalMtx.SetIdentity();

	m_angularVel = Ang3(0,0,0);
	m_AABBMaxZCache = 0;

	m_input.movementVector.zero();

	//m_trailSpeedScale = 0;

	m_animationSpeedAttack = m_animationSpeed = 1;

}

CShark::~CShark()
{

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	if(pCharacter)
		pCharacter->GetISkeletonPose()->SetPostProcessCallback0(0,0);
}

void CShark::BindInputs( IAnimationGraphState * pAGState )
{
	CActor::BindInputs(pAGState);

	if (pAGState)
	{
		m_inputSpeed = pAGState->GetInputId("Speed");
		m_idSignalInput  = pAGState->GetInputId("Signal");

	}
}

void CShark::ProcessEvent(SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_HIDE || event.event == ENTITY_EVENT_UNHIDE)
	{
		CreateScriptEvent("hide", event.event == ENTITY_EVENT_HIDE ? 1 : 0);
	}
	else if (event.event == ENTITY_EVENT_XFORM)
	{
		int flags = event.nParam[0];
		if (flags & ENTITY_XFORM_ROT && !(flags & (ENTITY_XFORM_USER|ENTITY_XFORM_PHYSICS_STEP)))
		{
			m_baseMtx = m_viewMtx = m_eyeMtx = Matrix33(GetEntity()->GetRotation());
		}
	}
	else if (event.event == ENTITY_EVENT_PREPHYSICSUPDATE)
	{
		PrePhysicsUpdate();
	}

	CActor::ProcessEvent(event);
}


bool CShark::Init( IGameObject * pGameObject )
{
	if (!CActor::Init(pGameObject))
		return false;


	Revive();

	return true;
}

void CShark::PostPhysicalize()
{
	CActor::PostPhysicalize();

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);

	if (!pCharacter)
		return;

	pCharacter->EnableStartAnimation(true);
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

	//set a default offset for the character, so in the editor the bbox is correct
	m_charLocalMtx.SetIdentity();
	m_charLocalMtx.SetTranslation(GetStanceInfo(STANCE_STAND)->modelOffset);

	GetAnimatedCharacter()->SetExtraAnimationOffset(m_charLocalMtx);

}

void CShark::UpdateAnimGraph( IAnimationGraphState * pState )
{
	CActor::UpdateAnimGraph(pState);

	if (pState)
	{
		pState->SetInput(m_inputSpeed, m_stats.speed);
	}
}

void CShark::PrePhysicsUpdate()
{
	IEntity* pEnt = GetEntity();
	if (pEnt->IsHidden())
		return;

	float frameTime = gEnv->pTimer->GetFrameTime();

	if (!m_stats.isRagDoll && GetHealth()>0)
	{	
		UpdateStats(frameTime);

		if (m_pMovementController)
		{
			SActorFrameMovementParams params;
			m_pMovementController->Update(frameTime, params);
		}

		assert(m_moveRequest.rotation.IsValid());
		assert(m_moveRequest.velocity.IsValid());

		//rotation processing
		if (m_linkStats.CanRotate())
			ProcessRotation(frameTime);

		assert(m_moveRequest.rotation.IsValid());
		assert(m_moveRequest.velocity.IsValid());

		//movement processing
		if (m_linkStats.CanMove())
		{
			ProcessMovement(frameTime);

			assert(m_moveRequest.rotation.IsValid());
			assert(m_moveRequest.velocity.IsValid());


			//send the movement request to the animated character
			if (m_pAnimatedCharacter)
			{
				// synthesize a prediction
				m_moveRequest.prediction.nStates = 1;
				m_moveRequest.prediction.states[0].deltatime = 0.0f;
				m_moveRequest.prediction.states[0].velocity = m_moveRequest.velocity;
				m_moveRequest.prediction.states[0].position = pEnt->GetWorldPos();
				m_moveRequest.prediction.states[0].orientation = pEnt->GetWorldRotation();
				//m_moveRequest.prediction.states[0].rotation.SetIdentity();

				assert(m_moveRequest.rotation.IsValid());
				assert(m_moveRequest.velocity.IsValid());

				m_pAnimatedCharacter->AddMovement(m_moveRequest);
			}
		}
	}
}

void CShark::Update(SEntityUpdateContext& ctx, int updateSlot)
{
	float frameTime = ctx.fFrameTime;

	if(frameTime == 0.f)
		frameTime = 0.01f;

	if(m_targetId)
	{
		IEntity* pTarget = gEnv->pEntitySystem->GetEntity(m_targetId);
		if(pTarget)
			UpdateStatus(frameTime,pTarget);
	}

	IEntity* pEnt = GetEntity();
	if (pEnt->IsHidden())
		return;

	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	CActor::Update(ctx,updateSlot);


	if (!m_stats.isRagDoll && GetHealth()>0)
	{
		//animation processing
		ProcessAnimation(pEnt->GetCharacter(0),frameTime);

		//reset the input for the next frame
		if (IsClient())
			m_input.ResetDeltas();


		if (gEnv->bClient)
		{
/*			float dist2 = (gEnv->pRenderer->GetCamera().GetPosition() - GetEntity()->GetWorldPos()).GetLengthSquared();

			if (m_pTrailAttachment)
			{ 
				CEffectAttachment* pEffectAttachment = (CEffectAttachment*)m_pTrailAttachment->GetIAttachmentObject();
				if (pEffectAttachment)
				{
					float goalspeed = max(0.f, m_stats.speed - m_params.trailEffectMinSpeed);
					Interpolate(m_trailSpeedScale, goalspeed, 3.f, frameTime);

					SpawnParams sp;          
					if (m_params.trailEffectMaxSpeedSize != 0.f)
						sp.fSizeScale = min(1.f, max(0.01f, m_trailSpeedScale/m_params.trailEffectMaxSpeedSize));

					if (m_params.trailEffectMaxSpeedCount != 0.f)
						sp.fCountScale = min(1.f, m_trailSpeedScale / m_params.trailEffectMaxSpeedCount);

					pEffectAttachment->SetSpawnParams(sp);
				}
			}

			if (m_pTurnSound && m_params.turnSoundMaxVel != 0.f && m_params.turnSoundBoneId != -1 && !m_pTurnSound->IsPlaying() && dist2<sqr(60.f))
			{ 
				if (IPhysicalEntity *pPhysics = GetEntity()->GetPhysics())
				{
					pe_status_dynamics dyn;
					dyn.partid = m_params.turnSoundBoneId;
					if (pPhysics->GetStatus(&dyn) && dyn.v.len2() > sqr(0.01f) && dyn.w.len2() > sqr(0.5f*m_params.turnSoundMaxVel))
					{
						float speedRel = min(1.f, dyn.w.len()/m_params.turnSoundMaxVel); 

						IEntitySoundProxy* pSoundProxy = (IEntitySoundProxy*)GetEntity()->CreateProxy(ENTITY_PROXY_SOUND);
						pSoundProxy->PlaySound(m_pTurnSound);        
						m_pTurnSound->SetParam("acceleration", speedRel);
						//CryLog("angSpeed %.2f (rel %.2f)", dyn.w.len(), speedRel);
					} 
				}    
			}
*/
		}
	}
/*
	//update the character offset
	Vec3 goal = (m_stats.isRagDoll?Vec3(0,0,0):GetStanceInfo(m_stance)->modelOffset);
	//if(!m_stats.isRagDoll)
	//	goal += m_stats.dynModelOffset;
	Interpolate(m_modelOffset,goal,5.0f,frameTime);
*/

	m_charLocalMtx.SetTranslation(ZERO);
	GetAnimatedCharacter()->SetExtraAnimationOffset(m_charLocalMtx);

}

void CShark::SetStartPos(const Vec3& targetPos)
{
	Vec3 dir(targetPos - m_startPos);
	dir.z = 0;
	float dist = dir.GetLength2D();
	if(dist>0)
		dir /= dist;
	else
		dir = Vec3Constants<float>::fVec3_OneY;

	Matrix34 tm(GetEntity()->GetWorldTM());
	
	// keep max 30m distance
	if(dist > 70)
		m_startPos = targetPos - dir * 70;

	float maxLevel = gEnv->p3DEngine->GetOceanWaterLevel( m_startPos) - 2;
	if (m_startPos.z > maxLevel)
		m_startPos.z = maxLevel;

	tm.SetTranslation(m_startPos);

	Vec3 right = dir ^ Vec3Constants<float>::fVec3_OneZ;

	tm.SetColumn(1, dir);
	tm.SetColumn(0, right.normalize());
	tm.SetColumn(2, Vec3Constants<float>::fVec3_OneZ);

	GetEntity()->SetWorldTM(tm);

	m_lastSpawnPoint = m_startPos;
}


float CShark::GetDistHeadTarget(const Vec3& targetPos, const Vec3& targetDirN,float& dotMouth)
{
	ICharacterInstance* pCharacter; 
	ISkeletonPose* pSkeletonPose;
	if((pCharacter = GetEntity()->GetCharacter(0)) && (pSkeletonPose= pCharacter->GetISkeletonPose()))
	{
		Vec3 headBonePos(ZERO);

		int16 jointid = pSkeletonPose->GetJointIDByName(m_params.headBoneName);
		int16 jointid1 = pSkeletonPose->GetJointIDByName(m_params.spineBoneName1);
		int16 jointid2 = pSkeletonPose->GetJointIDByName(m_params.spineBoneName2);
		if(jointid>=0  && jointid1>=0 && jointid2>=0)
		{
			// can't rely on bones orientation, simulate the head orientation by predicting 
			// the imaginary "bone 0" position given the first 3 bones 1,2,3
			const Matrix34& worldTM = GetEntity()->GetSlotWorldTM(0);
			QuatT jointQuat = pSkeletonPose->GetAbsJointByID(jointid);
			headBonePos =  worldTM* jointQuat.t;
			QuatT jointQuat1 = pSkeletonPose->GetAbsJointByID(jointid1);
			Vec3 bonePos1(worldTM * jointQuat1.t);
			QuatT jointQuat2 = pSkeletonPose->GetAbsJointByID(jointid2);
			Vec3 bonePos2(worldTM * jointQuat2.t);

			Vec3 v0(headBonePos - bonePos1);//.GetNormalizedSafe());
			Vec3 v1(bonePos1 - bonePos2);//.GetNormalizedSafe());

			Vec3 boneDir = (v0+v0-v1 ).GetNormalizedSafe();
			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(bonePos, ColorB(255,120,0,255), bonePos + boneDir*6, ColorB(255,120,0,255));
			dotMouth = targetDirN.Dot(boneDir);
			return Distance::Point_Point(headBonePos,targetPos);
		}
	}
	return -1.f;
}

void CShark::UpdateStatus(float frameTime, const IEntity* pTarget)
{
	const int numCircleSteps = 12;

	Vec3 targetPos(pTarget->GetWorldPos());
	bool bTargetOnVehicle = false;
	IActor * pTargetActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pTarget->GetId());
	if(pTargetActor)
	{
		// get the whole stuff and copy an entire structure just to get the eyeheight which can't be got alone
		SMovementState sMovementState;
		pTargetActor->GetMovementController()->GetMovementState(sMovementState);
		targetPos.z = sMovementState.eyePosition.z;
		// check if target is on a vehicle
		if(pTargetActor->GetLinkedVehicle())
			bTargetOnVehicle = true;
	}

	Vec3 myPos(GetEntity()->GetWorldPos());
	Vec3 myBodyDir(GetEntity()->GetRotation().GetColumn1());
	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(myPos, ColorB(255,0,255,255), myPos+ myBodyDir*6, ColorB(255,0,255,255));
	Vec3 targetDir(targetPos - myPos);
	float distTarget = targetDir.GetLength();
	Vec3 targetDirN(distTarget>0 ? targetDir/distTarget : ZERO);
	float distHeadTarget = -1;
	Vec3 targetDir2DN(targetDirN.x,targetDirN.y,0);
	if(targetDir2DN.IsZero())
		targetDir2DN = m_baseMtx.GetColumn1();
	targetDir2DN.NormalizeSafe();

	float dot = targetDirN.Dot(myBodyDir);
	float waterLevel = gEnv->p3DEngine->GetOceanWaterLevel( myPos);
	float maxHeight = waterLevel - 1;

	float dotMouth=-1;
	// get head orientation

	if (m_state != S_Spawning && !targetDirN.IsZero())
		distHeadTarget = GetDistHeadTarget(targetPos,targetDirN, dotMouth);

	switch(m_state)
	{
		case S_Spawning:
		{
			// avoid teleport if shark is already active and close to the target
			if(distTarget < 110 && !GetEntity()->IsHidden() && m_params.bSpawned)
			{
				SetReaching(targetPos);
				break;
			}
			int escapePointSize = m_EscapePoints.size();
			const int numTriesRadialDirections = 8;

			for(int i=0;i<2;i++)
			{
				if(m_tryCount >= numTriesRadialDirections + escapePointSize)
				{
					if(m_startPos.IsZero())
					{
						// retry another cycle
						m_tryCount = 0;
						break;
					}
					SetStartPos(targetPos);
					SetReaching(targetPos);
					break;
				}
				Vec3 currentStartPos;
				if(m_tryCount < escapePointSize)
				{
					currentStartPos = m_EscapePoints[m_tryCount];
					if(m_tryCount < escapePointSize -1 && currentStartPos.x==m_lastSpawnPoint.x && currentStartPos.y==m_lastSpawnPoint.y)
					{
						//low priority to last used spawn point - swap it with the last
						Vec3 swap(currentStartPos);
						currentStartPos = m_EscapePoints[escapePointSize -1 ];
						m_EscapePoints[escapePointSize -1 ] = swap;
					}
					m_escapeDir = currentStartPos - targetPos;
				}
				else
				{
					if(m_tryCount == escapePointSize)
						m_escapeDir = Vec3(70,0,0);
					m_escapeDir = m_escapeDir.GetRotated(ZERO,Vec3Constants<float>::fVec3_OneZ,2 * gf_PI / float(numTriesRadialDirections));
					currentStartPos = targetPos + m_escapeDir;
				}
				//find escape direction
				static const int objTypes = ent_terrain|ent_static|ent_sleeping_rigid;  // |ent_rigid;
				static const unsigned int flags = rwi_stop_at_pierceable|rwi_colltype_any;
				ray_hit ray;
				Vec3 startPos(myPos+Vec3(0,0,1));
				if (gEnv->pPhysicalWorld->RayWorldIntersection(targetPos, m_escapeDir, objTypes, flags, &ray, 1))
				{
					Vec3 hitDir(ray.pt - targetPos);
					if(hitDir.GetLengthSquared() > m_chosenEscapeDir.GetLengthSquared())
					{
						m_chosenEscapeDir = hitDir;
						m_startPos = targetPos + hitDir*(ray.dist - 4)/ray.dist;
					}
				}
				else
				{
					m_startPos = currentStartPos;
					SetStartPos(targetPos);
					SetReaching(targetPos);
					break;
				}

				m_tryCount++;
			}
			maxHeight = waterLevel - 1;
			
		}
		break;

		case S_Reaching:
		{
			maxHeight = waterLevel - 1;

			Vec3 moveTarget( targetPos);
			bool force = false;
			const int objTypes = ent_terrain|ent_static|ent_sleeping_rigid;//|ent_rigid;    
			const unsigned int flags = rwi_stop_at_pierceable|rwi_colltype_any;
			ray_hit ray;
	
			if(distTarget > m_params.minDistanceCircle * 3)
			{	
				// enough far, move directly towards target
				if (!gEnv->pPhysicalWorld->RayWorldIntersection(myPos, targetDir*(distTarget - 3)/distTarget, objTypes, flags, &ray, 1))
				{
					// skip drivable boats in collision check
//					if(ray.pCollider)
//					{
//						IEntity* pColliderEntity = (IEntity*)ray.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
//						if(!pColliderEntity || !pColliderEntity->GetAI() || pColliderEntity->GetAI()->GetAIType()!=AIOBJECT_VEHICLE)
//					}
					SetMoveTarget(moveTarget, force, m_params.minDistForUpdatingMoveTarget);
				} // else keep previous moveTarget
				break; 
			}
			else
			{
				// move aside of the target
				if(m_circleDisplacement.IsZero())
				{
					m_circleRadius = m_params.minDistanceCircle;
					m_circleDisplacement = targetDir2DN * m_circleRadius;
					float a = m_circleDisplacement.x;
					m_circleDisplacement.x = m_circleDisplacement.y;
					m_circleDisplacement.y = -a;
				}

				moveTarget = targetPos + m_circleDisplacement;
				Vec3 moveTargetNeg(targetPos -m_circleDisplacement);
				Vec3 dir(m_circleDisplacement);
				if(Distance::Point_Point2DSq(myPos,moveTargetNeg) < Distance::Point_Point2DSq(myPos,moveTarget))
				{
					moveTarget = moveTargetNeg;
					dir = -dir;
				}
				const float thr = 1.2f;
				if (gEnv->pPhysicalWorld->RayWorldIntersection(targetPos, dir* thr, objTypes, flags, &ray, 1))
				{
					Vec3 diff(ray.pt - targetPos);
					float newRadius = ray.dist/thr - 2;
					if(m_circleRadius > newRadius)
					{
						m_circleRadius = newRadius;
						force = true;
						moveTarget = targetPos + diff*newRadius/ray.dist ;
					}
				}
			}
			SetMoveTarget(moveTarget, force, m_params.minDistForUpdatingMoveTarget);

			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(targetPos, ColorB(255,255,255,255), m_moveTarget, ColorB(255,255,255,255));
			Vec3 moveTargetDir(m_moveTarget - myPos);
			float moveDistTarget = moveTargetDir.GetLength();
			moveTargetDir.z = 0;
			myBodyDir.z = 0;
			if(moveDistTarget<2 || moveDistTarget <6.f && moveTargetDir.GetNormalizedSafe().Dot(m_stats.velocity.GetNormalizedSafe())<0.1f)
			{
				m_state = S_Circling;
				if(m_remainingCirclingTime==-1)
					m_remainingCirclingTime = m_params.circlingTime;
				m_numHalfCircles = m_params.numCircles * numCircleSteps+1;
				m_circleDisplacement = ZERO;
				m_bCircularTrajectory = false;
			}
		}
			break;
		case S_Circling:
		{
			if(distTarget >m_params.maxDistanceCircle)
			{
				m_state = S_Reaching;
			}	
			else
			{
				if(distTarget < m_params.meleeDistance && !bTargetOnVehicle)
				{
					if(dotMouth > 0.9f)
					{
						//shark is aligned, attack

						Attack(true);	
						break;
					}
				}
				
				m_remainingCirclingTime -= frameTime;

				Vec3 moveTargetDir(m_moveTarget - myPos);
				float moveDistTarget = moveTargetDir.GetLength();
				bool force = false;
				//Vec3 pos(targetPos + m_circleDisplacement);
				if(!bTargetOnVehicle && m_remainingCirclingTime<=0 && targetDirN.Dot(pTarget->GetRotation().GetColumn1()) < -0.7f)
				{
					//shark is in front of target and he's circled around enough
					m_state = S_FinalAttackAlign;
					break;
				}
				if(moveDistTarget<2 || moveDistTarget <6.f && moveTargetDir.Dot(m_stats.velocity.GetNormalizedSafe())<0)
				{

//					m_numHalfCircles --;
					
					const float angle = DEG2RAD(360/float(numCircleSteps)) * sgn(CrossZ(myBodyDir, targetDir2DN));
					Vec3 radius = - targetDir2DN * m_circleRadius;
					m_circleDisplacement = radius.GetRotated(ZERO,Vec3Constants<float>::fVec3_OneZ,angle);

					if(!bTargetOnVehicle)
					{
						static const int objTypes = ent_terrain|ent_static|ent_sleeping_rigid|ent_rigid;    
						static const unsigned int flags = rwi_stop_at_pierceable|rwi_colltype_any;
						const float thr = 1.3f;
						ray_hit ray;

						// anticipate collision with current+next+next point
						Vec3 nextCircleDisplacement(m_circleDisplacement);

						for(int i=0; i<3; i++)
						{
							if(i>0)
								nextCircleDisplacement = nextCircleDisplacement.GetRotated(ZERO,Vec3Constants<float>::fVec3_OneZ,angle);
							
							if (gEnv->pPhysicalWorld->RayWorldIntersection(targetPos, nextCircleDisplacement* thr, objTypes, flags, &ray, 1))
							{
								Vec3 dir(ray.pt - targetPos);
								float newRadius = ray.dist/thr - 2;
								if(m_circleRadius > newRadius)
								{
									m_circleRadius = newRadius;
									force = true;
								}
							}
						}
						m_circleDisplacement *= m_circleRadius/m_circleDisplacement.GetLength() ;
					}
				}

				SetMoveTarget(targetPos + m_circleDisplacement, force, m_params.minDistForUpdatingMoveTarget); 
				Interpolate(m_circleRadius,m_params.minDistanceCircle * 1.1f,0.1f,frameTime);
			}			
		}
			break;
		case S_FinalAttackAlign:
		{
			maxHeight = waterLevel;

			if(bTargetOnVehicle)
			{
				m_state = S_Circling;
				break;
			}
			SetMoveTarget(targetPos,true);
			if(dotMouth >0.8f)
			{

				if(distHeadTarget >= m_params.meleeDistance)
				{
//					IAnimationGraphState* pAGState = GetAnimationGraphState();
//					if ( pAGState)
	//					pAGState->SetInput( m_idSignalInput, "meleeSprint" );
					m_state = S_FinalAttack;
					m_remainingAttackTime = 5.0f;
				}
				else if(distHeadTarget>=0)
					Attack();	
			}
		}
			break;
		case S_FinalAttack:
		{
			if(bTargetOnVehicle)
			{
				m_state = S_Circling;
				break;
			}

			SetMoveTarget(targetPos,true);

			m_remainingAttackTime -= frameTime;
			if(m_remainingAttackTime<=0)
				m_state = S_FinalAttackAlign;

			if(dotMouth >0.8f && distHeadTarget < m_params.meleeDistance)
				Attack();	

			maxHeight = waterLevel;

		}
			break;
		case S_PrepareEscape:
			{
				int escapePointSize = m_EscapePoints.size();
				const int numTriesRadialDirections = 8;
				for(int i=0;i<2;i++)
				{
					
					if(m_tryCount >= numTriesRadialDirections + escapePointSize)
					{
						m_moveTarget = myPos + m_chosenEscapeDir;
						m_state = S_Escaping;
						break;
					}
					if(m_tryCount < escapePointSize)
						m_escapeDir = m_EscapePoints[m_tryCount] - myPos;
					else
					{
						if(m_tryCount == escapePointSize)
							m_escapeDir = Vec3(500,0,0);
						m_escapeDir = m_escapeDir.GetRotated(ZERO,Vec3Constants<float>::fVec3_OneZ,2 * gf_PI / float(numTriesRadialDirections));
					}
					//find escape direction
					static const int objTypes = ent_terrain|ent_static|ent_sleeping_rigid|ent_rigid;    
					static const unsigned int flags = rwi_stop_at_pierceable|rwi_colltype_any;
					ray_hit ray;
					Vec3 startPos(myPos+Vec3(0,0,1));
					if (gEnv->pPhysicalWorld->RayWorldIntersection(startPos, m_escapeDir, objTypes, flags, &ray, 1))
					{
						Vec3 hitDir(ray.pt - startPos);
						if(hitDir.GetLengthSquared() > m_chosenEscapeDir.GetLengthSquared())
							m_chosenEscapeDir = hitDir;
					}
					else
					{
						m_chosenEscapeDir = m_escapeDir;
						m_moveTarget = myPos + m_chosenEscapeDir;
						m_state = S_Escaping;
						break;
					}

					m_tryCount++;
				}
			}
			break;

		case S_Attack:
			{
				if(bTargetOnVehicle)
				{
					m_state = S_Circling;
					break;
				}
				SetMoveTarget(targetPos,true);
				m_remainingAttackTime -= frameTime;
				if(m_remainingAttackTime<=0)
				{
					GoAway();
				}
				maxHeight = waterLevel;
			}
			break;
		case S_Escaping:
			{
				if(distTarget <=2)
				{
					// deactivate, end of escape
					m_state = S_Sleeping;
					GetEntity()->Hide(true);
				}
			}
			break;
		default: 
			break;
	}
	
	if(bTargetOnVehicle)
		maxHeight = waterLevel - 3; // keep safe level to avoid being hit by the vehicle

	AdjustMoveTarget(maxHeight,pTarget);
}

/*
void CShark::UpdateSpawning()
{
	if(distTarget < 110 && !GetEntity()->IsHidden() && m_params.bSpawned)
	{
		SetReaching(targetPos);
		break;
	}
	int escapePointSize = m_EscapePoints.size();
	const int numTriesRadialDirections = 8;

	for(int i=0;i<2;i++)
	{
		if(m_tryCount >= numTriesRadialDirections + escapePointSize)
		{
			if(m_startPos.IsZero())
			{
				// retry another cycle
				m_tryCount = 0;
				break;
			}
			SetStartPos(targetPos);
			SetReaching(targetPos);
			break;
		}
		Vec3 currentStartPos;
		if(m_tryCount < escapePointSize)
		{
			currentStartPos = m_EscapePoints[m_tryCount];
			if(m_tryCount < escapePointSize -1 && currentStartPos.x==m_lastSpawnPoint.x && currentStartPos.y==m_lastSpawnPoint.y)
			{
				//low priority to last used spawn point - swap it with the last
				Vec3 swap(currentStartPos);
				currentStartPos = m_EscapePoints[escapePointSize -1 ];
				m_EscapePoints[escapePointSize -1 ] = swap;
			}
			m_escapeDir = currentStartPos - targetPos;
		}
		else
		{
			if(m_tryCount == escapePointSize)
				m_escapeDir = Vec3(70,0,0);
			m_escapeDir = m_escapeDir.GetRotated(ZERO,Vec3Constants<float>::fVec3_OneZ,2 * gf_PI / float(numTriesRadialDirections));
			currentStartPos = targetPos + m_escapeDir;
		}
		//find escape direction
		static const int objTypes = ent_terrain|ent_static|ent_sleeping_rigid;  // |ent_rigid;
		static const unsigned int flags = rwi_stop_at_pierceable|rwi_colltype_any;
		ray_hit ray;
		Vec3 startPos(myPos+Vec3(0,0,1));
		if (gEnv->pPhysicalWorld->RayWorldIntersection(targetPos, m_escapeDir, objTypes, flags, &ray, 1))
		{
			Vec3 hitDir(ray.pt - targetPos);
			if(hitDir.GetLengthSquared() > m_chosenEscapeDir.GetLengthSquared())
			{
				m_chosenEscapeDir = hitDir;
				m_startPos = targetPos + hitDir*(ray.dist - 4)/ray.dist;
			}
		}
		else
		{
			m_startPos = currentStartPos;
			SetStartPos(targetPos);
			SetReaching(targetPos);
			break;
		}

		m_tryCount++;
	}
	maxHeight = waterLevel - 1;


}

*/
void CShark::SetReaching(const Vec3& targetPos)
{
	SetMoveTarget(targetPos, true, m_params.minDistForUpdatingMoveTarget);
	m_state = S_Reaching;
	GetEntity()->Hide(false);
}


void CShark::AdjustMoveTarget(float maxHeight, const IEntity* pTargetEntity)
{
	if(m_moveTarget.z > maxHeight)
		m_moveTarget.z = maxHeight;
	Vec3 myPos(GetEntity()->GetWorldPos());
	// check for possible collisions with floating entities
	if(Distance::Point_Point2DSq(myPos, m_lastCheckedPos) > 4*4)
	{
		SEntityProximityQuery query;	
		//query.nEntityFlags = ENTITY_FLAG_HAS_AI;
		query.pEntityClass = NULL;
		float dimxy = 10;
		float dimz = 3;
		query.box = AABB(Vec3(myPos.x - dimxy, myPos.y - dimxy, myPos.z /*avoid checking entities below*/),\
			Vec3(myPos.x + dimxy, myPos.y + dimxy, myPos.z+dimz));

		gEnv->pEntitySystem->QueryProximity(query);

		float myz = myPos.z + m_AABBMaxZCache;

		for(int i=0; i<query.nCount; ++i)
		{
			IEntity* pEntity = query.pEntities[i];
			if(pEntity != GetEntity() && pEntity != pTargetEntity)
			{
				AABB targetBounds;
				Vec3 pos(pEntity->GetWorldPos());
				pEntity->GetLocalBounds(targetBounds);
				// preliminary check for the height
				float targetZ = targetBounds.min.z + pos.z;
				if(myz >= targetZ)
				{
					// check actual collision
					targetBounds.SetTransformedAABB( pEntity->GetWorldTM(),targetBounds );
					Vec3 collidingDir(m_moveTarget - myPos);
					collidingDir += collidingDir.GetNormalizedSafe()*4.f;
					Lineseg segDir(myPos,myPos+collidingDir);
					Vec3 intersection;
					if(Intersect::Lineseg_AABB(segDir, targetBounds,intersection))
					{
						m_moveTarget.z = min(targetZ - 0.5f - myz,m_moveTarget.z);
					}
				}
			}
		}
		m_lastCheckedPos = myPos;

	}
}

void CShark::GoAway()
{
	m_state = S_PrepareEscape;
	//m_escapeDir = Vec3(500,0,0);
	m_tryCount =0;
	m_chosenEscapeDir.zero();
}

void CShark::Attack(bool fast)
{
//	IAnimationGraphState* pAGState = GetAnimationGraphState();
	//if ( pAGState)
		//pAGState->SetInput( m_idSignalInput, "melee" );
	m_state = S_Attack;
	m_remainingAttackTime = 3.0f;
	MeleeAnimation();
	m_animationSpeedAttack = fast ? 1.5f : 1.0;

}

//FIXME:at some point, unify this with CPlayer via CActor
void CShark::UpdateStats(float frameTime)
{
	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();

	if (!pPhysEnt)
		return;

	pe_player_dynamics simPar;
	if (pPhysEnt->GetParams(&simPar)==0 || !simPar.bActive)
	{
		m_stats.velocity = m_stats.velocityUnconstrained.zero();
		m_stats.speed = m_stats.speedFlat = 0.0f;
		m_stats.inFiring = 0;		

		/*pe_player_dynamics simParSet;
		simParSet.bSwimming = true;
		pPhysEnt->SetParams(&simParSet);*/

		return;
	}

	//retrieve some information about the status of the player
	pe_status_dynamics dynStat;
	pe_status_living livStat;



	if( !pPhysEnt->GetStatus(&dynStat) ||
		!pPhysEnt->GetStatus(&livStat) ||
		!pPhysEnt->GetParams(&simPar) )
		return;

	//update status table

	m_stats.inWaterTimer += frameTime;
	m_stats.inAir = 0.0f;

	m_stats.gravity = simPar.gravity;
	m_stats.velocity = m_stats.velocityUnconstrained = dynStat.v;
	m_stats.angVelocity = dynStat.w;
	FixVec(m_stats.angVelocity);
	m_stats.speed = m_stats.speedFlat = GetFixNum(m_stats.velocity.GetLength());

	m_stats.turnRadius = m_stats.angVelocity.z !=0 ? m_stats.speed / fabs(m_stats.angVelocity.z) : m_params.minTurnRadius * 3;
	if(m_stats.turnRadius< m_params.minTurnRadius)
		m_stats.turnRadius = m_params.minTurnRadius;

	// [Mikko] The velocity from the physics in some weird cases have been #INF because of the player
	// Zero-G movement calculations. If this asserts triggers, the alien might have just collided with
	// a play that is stuck in the geometry.
	assert(NumberValid(m_stats.speed));

	m_stats.mass = dynStat.mass;

}

void CShark::ProcessRotation(float frameTime)
{
	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();

	if (!pPhysEnt)
		return;

	if (frameTime > 0.1f)
		frameTime = 0.1f;

	//rotation
	//6 degree of freedom
	//FIXME:put mouse sensitivity here!
	//TODO:use radians
	float rotSpeed(0.5f);


	// Mikko: Separated the look and movement directions. This is a workaround! The reason is below (moved from the SetActorMovement):
	// >> Danny - old code had desired direction using vLookDir but this caused spinning behaviour
	// >> when it was significantly different to vMoveDir

	/*if (m_input.viewVector.len2()>0.0f)
	{
		//			m_eyeMtx.SetRotationVDir(m_input.viewVector.GetNormalizedSafe());
		Matrix33	eyeTarget;
		eyeTarget.SetRotationVDir(m_input.viewVector.GetNormalizedSafe());
		Quat	eyeTargetQuat(eyeTarget);
		Quat	currQuat(m_eyeMtx);
		m_eyeMtx = Matrix33(Quat::CreateSlerp( currQuat.GetNormalized(), eyeTargetQuat, min(frameTime * 12.0f, 1.0f)));
	}
	

//		if (m_input.viewVector.len2()>0.0f)
	{
		Vec3	lookat = m_eyeMtx.GetColumn(1);
		Vec3	orient = m_viewMtx.GetColumn(1);
		m_viewMtx.SetRotationVDir(m_input.viewVector.GetNormalizedSafe());
	}
	*/
	m_eyeMtx =m_baseMtx ;
	m_viewMtx = m_baseMtx ;
	
	// rolling when attacking
	if(m_rollTime >0)
	{
		m_rollTime-=frameTime;
		if(m_rollTime<0) m_rollTime = 0;
		m_roll = (1 - cos((m_params.attackRollTime - m_rollTime)/m_params.attackRollTime * 2*gf_PI ) )/2 * m_rollMaxAngle;
		float straightFactor = fabs(m_turnAnimBlend - 0.5);
		static const float lthr = 0.2f, hthr = 0.3f;
		if(straightFactor< lthr) 
			straightFactor =1;
		else if (straightFactor> hthr) 
			straightFactor = 0;
		else
			straightFactor = (hthr - straightFactor)/(hthr - lthr);

		m_roll *= straightFactor ; // don't roll too much if turning

	}
}

void CShark::GetMovementVector(Vec3& move, float& speed, float& maxSpeed)
{
	maxSpeed = GetStanceInfo(m_stance)->maxSpeed;

	// AI Movement
	move = m_input.movementVector;
	// Player movement
/*	// For controlling an alien as if it was a player (dbg stuff)
	move += m_viewMtx.GetColumn(0) * m_input.deltaMovement.x * maxSpeed;
	move += m_viewMtx.GetColumn(1) * m_input.deltaMovement.y * maxSpeed;
	move += m_viewMtx.GetColumn(2) * m_input.deltaMovement.z * maxSpeed;
*/
	// Cap the speed to stance max stance speed.
	speed = move.len();
	if(speed > maxSpeed)
	{
		move *= maxSpeed / speed;
		speed = maxSpeed;
	}
}

void CShark::ProcessMovement(float frameTime)
{
	
	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();

	if (!pPhysEnt)
		return;

	if (frameTime > 0.05f)
		frameTime = 0.05f;

	//movement
	Vec3 move;
	float	speed,maxSpeed;
	GetMovementVector(move, speed, maxSpeed);
	Vec3 myPos(GetEntity()->GetWorldPos());
	Vec3 forward;
	if( !m_moveTarget.IsZero() )
		forward = (m_moveTarget - myPos).GetNormalizedSafe();
	else
		forward = m_baseMtx.GetColumn(1);
/*
	const SStanceInfo* pStanceInfo = GetStanceInfo(m_stance);
	float rotScale = 1.0f;
	if (pStanceInfo)
	{
		const float speedRange = max(1.0f, pStanceInfo->normalSpeed - m_params.speed_min);
		rotScale = (m_stats.speed - m_params.speed_min) / speedRange;
	}
	float rotSpeed = m_params.rotSpeed_min + (1.0f - rotScale) * (m_params.rotSpeed_max - m_params.rotSpeed_min);
*/
	float desiredTurnRadius;
	float desiredSpeed;
	float rotSpeed;
	Vec3 center;
	ColorB debugColor(0, 0, 255, 48);

	if(m_moveTarget.IsZero())
	{
		desiredSpeed =0;
		rotSpeed = 0;
	}
	else
	{
		// Slow down if the desired direction differs from the current direction.
		Vec3 desiredDir(m_moveTarget - GetEntity()->GetWorldPos()) ;
		Vec3 bodyDir(GetEntity()->GetWorldTM().TransformVector(Vec3Constants<float>::fVec3_OneY));
		float	dot =  bodyDir.Dot(desiredDir.GetNormalizedSafe());
		float distMoveTarget = desiredDir.GetLength();
/*
		float velScale =  (dot< 0.7f ? 0 : (dot-0.7f)/0.3f) ;

		// Slow down if too fast compared to slower target, and not aligned to it
		if(m_state == S_FinalAttack || m_state == S_Attack || m_state == S_FinalAttackAlign)
		{
			IEntity* pTarget = gEnv->pEntitySystem->GetEntity(m_targetId);
			if(pTarget)
			{
				float distHead = m_headOffset*2.5f;
				//Vec3 dir( pTarget->GetWorldPos() - GetEntity()->GetWorldPos());
				if(distMoveTarget < distHead)
				{
					IPhysicalEntity *phys = pTarget->GetPhysics();
					pe_status_dynamics	targetdyn;
					if( phys && phys->GetStatus(&targetdyn))
					{
						Vec3 relVelocity(m_stats.velocity - targetdyn.v);
						float relSpeed = relVelocity.GetLength();
						static const float speedThr = 1.f;
						//float dot = bodyDir.Dot(dir.GetNormalizedSafe());
						if(dot<0.7f && relSpeed > speedThr)
						{
							if(dot<0)
								dot=0;
							float maxRelSpeed = speedThr*2;
							if(relSpeed > maxRelSpeed)
								relSpeed = maxRelSpeed;
							velScale *= dot/0.7f*(relSpeed - speedThr)/(maxRelSpeed - speedThr);
						}
					}
				}
			}
		}
		*/
		// get the turn radius to reach the move target
		Vec3 velNorm(m_stats.velocity.GetNormalizedSafe());
		
		if(m_state == S_Escaping)
		{
			desiredSpeed = (maxSpeed + m_params.speed_min) / 2;
			m_bCircularTrajectory = false;
			rotSpeed = desiredSpeed/(m_params.minTurnRadius * 1.5f);
			rotSpeed = CLAMP(rotSpeed,0.7f,m_params.rotSpeed_max);
		}
		else if(m_state != S_Circling && dot<0.2 || dot<0.8 && ( m_state == S_FinalAttackAlign ))
		{
			// target behind or align before final attack, do a slow narrow turn
//			m_bCircularTrajectory = true;
			desiredSpeed = m_params.speed_min;// + (maxSpeed - m_params.speed_min)/3;
			rotSpeed = desiredSpeed/m_params.minTurnRadius;
			rotSpeed = CLAMP(rotSpeed,0,m_params.rotSpeed_max);
			// debug
			m_debugradius = desiredSpeed/rotSpeed;
			Vec3 disp(bodyDir.y,-bodyDir.x,bodyDir.z);
			if(CrossZ(bodyDir, desiredDir) >0)
				disp = -disp;
			m_debugcenter = myPos+disp*m_debugradius;

			debugColor.Set(12,255,12,48);
			m_bCircularTrajectory = false;

		}
		else if(m_state != S_Circling && dot>0.7f || ( m_state == S_FinalAttackAlign || m_state == S_FinalAttack))
		{
			// almost aligned, try to go straight
			//m_bCircularTrajectory = true;
			desiredSpeed = max(dot*dot,0.3f)*maxSpeed;
			float eta = distMoveTarget/desiredSpeed;
			//rotSpeed = 2*cry_acosf(dot)/eta;
			//if(rotSpeed > m_params.rotSpeed_max)
				rotSpeed = m_params.rotSpeed_max;
			//debug
			m_debugradius = 0;//rotSpeed>0? desiredSpeed/rotSpeed : 0;
			m_debugcenter = m_moveTarget;
			m_bCircularTrajectory = false;

		}
		else if(!m_bCircularTrajectory)// && !velNorm.IsZero() && !m_moveTarget.IsZero())
		{
			//float reqTurnRadius = m_params.rotSpeed_max > 0 ? maxSpeed * m_params.rotSpeed_max : m_params.minTurnRadius;
			//if(reqTurnRadius < m_params.minTurnRadius)
			m_bCircularTrajectory = GetCirclePassingBy(myPos,m_moveTarget,velNorm,center,desiredTurnRadius);
			if(m_bCircularTrajectory)
			{
				m_debugcenter = center;
				m_turnRadius = desiredTurnRadius;
				m_debugradius = desiredTurnRadius;
			}
			else
			{
				rotSpeed = m_params.rotSpeed_max;
				desiredSpeed = maxSpeed;
			}
		}
		if(m_bCircularTrajectory)
		{
			desiredSpeed = maxSpeed;
			rotSpeed = desiredSpeed/m_turnRadius;// * 1.2f;// correction
			if(rotSpeed > m_params.rotSpeed_max)
			{
				rotSpeed = m_params.rotSpeed_max;
				desiredSpeed = rotSpeed * m_turnRadius;
				desiredSpeed = CLAMP(desiredSpeed, m_params.speed_min, maxSpeed);
			}
		}

		// adjust speed depending on target's speed and orientation
		IEntity* pTarget = gEnv->pEntitySystem->GetEntity(m_targetId);
		if(pTarget)
		{
			IPhysicalEntity *phys = pTarget->GetPhysics();
			pe_status_dynamics	targetdyn;
			if( phys && phys->GetStatus(&targetdyn) && m_stats.speed>0.5f && targetdyn.v.GetLengthSquared()>1)
			{
				Vec3 normVel(m_stats.velocity.GetNormalizedSafe());
				float dotvel = targetdyn.v.GetNormalizedSafe().Dot(normVel );//1 = same velocity, -1=opposite
				Vec3 targetDir((GetEntity()->GetWorldPos() - pTarget->GetWorldPos()).GetNormalizedSafe());
				float dotOrient = targetDir.Dot(pTarget->GetRotation().GetColumn1());
				float dot = (dotvel + dotOrient)/2;
				float relspeed = (targetdyn.v - m_stats.velocity).GetLength();
				desiredSpeed += dot * relspeed;
				desiredSpeed = CLAMP(desiredSpeed,m_params.speed_min, maxSpeed*1.3f);
			}
		}
	}
	// use maximum speed
	// rotSpeed = speed / turnRadius
	Interpolate(m_curSpeed,desiredSpeed,(m_curSpeed < desiredSpeed ? m_params.accel : m_params.decel),frameTime);
	Interpolate(m_turnSpeed,rotSpeed,5.0f,frameTime);
	/*
	if(m_bCircularTrajectory)
	{
		if(!m_debugcenter.IsZero())
		{
			if(m_debugradius==0)
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(myPos, ColorB(0,0,255,255), m_moveTarget, ColorB(128,128,255,255));
			else
			{
				IEntity* pTarget = gEnv->pEntitySystem->GetEntity(m_targetId);
				if(!(pTarget && strcmp(pTarget->GetClass()->GetName(),"Player")==0))
				{
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_debugcenter, 0.5f, ColorB(255, 255, 12, 60), true);
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawCylinder(m_debugcenter, Vec3(0,0,1), m_debugradius,0.1f,debugColor, true);
				}
			}
		}
	}
	*/
	/*Quat currQuat(m_baseMtx);
	m_baseMtx = Matrix33(Quat::CreateSlerp( currQuat.GetNormalized(), m_desiredVeloctyQuat, min(frameTime * m_turnSpeed , 1.0f)));
	m_baseMtx.OrthonormalizeFast();
	*/
	Vec3 right(forward.y, -forward.x, 0);
	if(right.IsZero())
		right = m_baseMtx.GetColumn0();
	else
		right.Normalize();

	Vec3 up = (right % forward).GetNormalized();

	Quat goalQuat(Matrix33::CreateFromVectors(right,forward,up));
	goalQuat.Normalize();
	
	Quat currRotation(GetEntity()->GetRotation());
	currRotation.Normalize();

	m_modelQuat = Quat::CreateSlerp(currRotation, goalQuat, min(1.0f,frameTime * m_turnSpeed)  );
	m_modelQuat.Normalize();

	Quat currQuat(m_baseMtx);
	m_baseMtx = Matrix33(m_modelQuat);//Quat::CreateSlerp( currQuat.GetNormalized(), m_modelQuat, min(frameTime *  m_turnSpeed , 2.0f)));
	m_baseMtx.OrthonormalizeFast();

	// using a class member m_modelAddQuat probably deprecated (needed for Slerp/blending)
	m_modelAddQuat = Quat(Matrix33::CreateIdentity() * Matrix33::CreateRotationXYZ(Ang3(0,m_roll,0)));//goalQuat;
	m_charLocalMtx = Matrix34(m_modelAddQuat);
	m_charLocalMtx.OrthonormalizeFast();

	m_velocity = move.GetNormalizedSafe()*m_curSpeed;
	assert(GetEntity()->GetRotation().IsValid());
	assert(GetEntity()->GetRotation().GetInverted().IsValid());

	m_moveRequest.rotation = currRotation.GetInverted() * m_modelQuat;
	m_moveRequest.rotation.Normalize();
	assert(m_moveRequest.rotation.IsValid());

	m_moveRequest.velocity = m_velocity;
	m_moveRequest.type = eCMT_Fly;

	m_lastPos = myPos;

	// debug draw
	/*
	Vec3 basepos = GetEntity()->GetWorldPos();
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(255,255,255,255), basepos+ goalQuat.GetColumn0() * 4.0f, ColorB(255,255,255,255));
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(0,0,255,255), basepos+ goalQuat.GetColumn1() * 4.0f, ColorB(255,255,255,255));
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(255,255,255,255),basepos+  goalQuat.GetColumn2() * 4.0f, ColorB(255,255,255,255));

	gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(0,255,255,255), basepos+ m_modelQuat.GetColumn0() * 7.0f, ColorB(0,255,255,255));
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(0,0,0,255), basepos+ m_modelQuat.GetColumn1() * 7.0f, ColorB(0,255,255,255));
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(0,255,255,255), basepos+ m_modelQuat.GetColumn2() * 7.0f, ColorB(0,255,255,255));
	*/
}


void CShark::ProcessAnimation(ICharacterInstance *pCharacter,float frameTime)
{
	float turnAnimBlend;
//	float angVel = m_stats.angVelocity.z;
	Vec3 curRot = GetEntity()->GetRotation().GetColumn1();
	curRot.z = 0;
	curRot.NormalizeSafe();
	float	angVel = m_stats.angVelocity.z;//cry_acosf(curRot.Dot(m_lastRot))/frameTime;

	if(angVel==0)
		turnAnimBlend = 0.5;
	else
	{
		turnAnimBlend = 0.5f + sgn(CrossZ(curRot,m_lastRot))*0.5/(m_stats.turnRadius/m_params.minTurnRadius);
	}
	float angVel2 = cry_acosf(curRot.Dot(m_lastRot))/frameTime;
	Interpolate(m_turnAnimBlend,turnAnimBlend,2.f + fabs(angVel)*1.5f,frameTime);

	pCharacter->GetISkeletonAnim()->SetBlendSpaceOverride(eMotionParamID_TurnSpeed,m_turnAnimBlend,true);

	m_lastRot = curRot;

	// dynamic adjustment of animation playback speed
	float animSpeed;
	float maxSpeed = GetStanceInfo(m_stance)->maxSpeed;

	if(m_state == S_Attack)
		animSpeed = m_animationSpeedAttack;
	else
	{
		if(maxSpeed==0)
			animSpeed = 1;
		else
			animSpeed = CLAMP(m_stats.speed/maxSpeed,0.5,1.f)*1.2f;
	}

	Interpolate(m_animationSpeed, animSpeed, m_state==S_Attack ? 4.0f : 2.0f, frameTime);

	pCharacter->SetAnimationSpeed(m_animationSpeed);

}



void CShark::Draw(bool draw)
{
	uint32 slotFlags = GetEntity()->GetSlotFlags(0);

	if (draw)
		slotFlags |= ENTITY_SLOT_RENDER;
	else
		slotFlags &= ~ENTITY_SLOT_RENDER;

	GetEntity()->SetSlotFlags(0,slotFlags);
}

void CShark::ResetAnimations()
{
	ICharacterInstance *character = GetEntity()->GetCharacter(0);
	
	SmartScriptTable entityTable = GetEntity()->GetScriptTable();
	//m_idleAnimationName = ""; 
	
	if (m_pAnimatedCharacter)
		m_pAnimatedCharacter->ClearForcedStates();
	ISkeletonAnim* pSkeleton;
	if(character && (pSkeleton=character->GetISkeletonAnim()))
	{

		pSkeleton->StopAnimationsAllLayers();

		//if( entityTable.GetPtr() && entityTable->GetValue("idleAnimation",m_idleAnimationName))
		{
			pSkeleton->SetBlendSpaceOverride(eMotionParamID_TurnSpeed,0.5f,true);
		}
	}
	m_animationSpeedAttack = m_animationSpeed = 1;
}

//------------------------------------------------------------------------
void CShark::Reset(bool toGame)
{

}

void CShark::Kill()
{
	CActor::Kill();

	ResetAnimations();

	/*if (m_pTrailAttachment)  
		m_pTrailAttachment->ClearBinding();    */
}

void CShark::Revive(bool fromInit)
{
	CActor::Revive(fromInit);

	ResetValues();

	m_lastPos = GetEntity()->GetWorldPos();
	m_lastRot = GetEntity()->GetRotation().GetColumn1();

	m_baseMtx = Matrix33(GetEntity()->GetRotation());
	m_modelQuat = GetEntity()->GetRotation();
	SetDesiredDirection(GetEntity()->GetRotation().GetColumn1());
	m_charLocalMtx.SetIdentity();
	AABB box;
	GetEntity()->GetLocalBounds(box);
	m_AABBMaxZCache = box.max.z;

	ResetAnimations();

	//initialize the ground effect
	/*
	if (m_params.trailEffect[0] && gEnv->p3DEngine->FindParticleEffect(m_params.trailEffect))
	{
		if (ICharacterInstance* pCharInstance = GetEntity()->GetCharacter(0))
		{
			IAttachmentManager* pAttachmentManager = pCharInstance->GetIAttachmentManager(); 
			if (IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName("trail_attachment"))
			{ 
				pAttachment->ClearBinding();
				CEffectAttachment* pEffectAttachment = new CEffectAttachment(m_params.trailEffect, Vec3(0,0,0), m_params.trailEffectDir.GetNormalized(), 1);
				pEffectAttachment->CreateEffect();
				pAttachment->AddBinding(pEffectAttachment);
				m_pTrailAttachment = pAttachment;
				m_trailSpeedScale = 0.f;
			} 
			else
				CryLog("[CShark::Revive] %s: 'trail_attachment' not found.", GetEntity()->GetName());
		}
	}
	*/

	if (m_pAnimatedCharacter)
	{
		SAnimatedCharacterParams params = m_pAnimatedCharacter->GetParams();
		params.flags |= eACF_ZCoordinateFromPhysics | eACF_NoTransRot2k | eACF_ImmediateStance | eACF_NoLMErrorCorrection;
		m_pAnimatedCharacter->SetParams(params);
	}

	GetEntity()->Hide(true);
}

void CShark::RagDollize( bool fallAndPlay )
{
	return;
	if(fallAndPlay)
		return;
	if (m_stats.isRagDoll)
		return;

	ResetAnimations();

//	assert(!fallAndPlay && "Fall and play not supported for aliens yet");

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	if (pCharacter)
		pCharacter->GetISkeletonPose()->SetRagdollDefaultPose();

	//CActor::RagDollize( fallAndPlay );
	CActor::RagDollize( false );

	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();

	if (pPhysEnt)
	{
		pe_params_flags flags;
		flags.flagsOR = pef_log_collisions;
		pPhysEnt->SetParams(&flags);

		pe_simulation_params sp;
		sp.damping = 1.0f;
		sp.dampingFreefall = 0.0f;
		sp.mass = m_stats.mass;
		pPhysEnt->SetParams(&sp);

		pe_params_articulated_body pa;
		pa.dampingLyingMode = 5.5f;    
		//pa.scaleBounceResponse = 0.1f;
		pPhysEnt->SetParams(&pa);
	}

	pCharacter = GetEntity()->GetCharacter(0);	
	if (pCharacter)
	{
		pCharacter->EnableStartAnimation(false);
	}
}

void	CShark::MeleeAnimation()
{
	/*
	CryCharAnimationParams aparams;
	aparams.m_nLayerID = 1;
	aparams.m_fPlaybackSpeed=1.f;
	aparams.m_nFlags=0;
	//			character->EnableStartAnimation(true);
	ICharacterInstance *character = GetEntity()->GetCharacter(0);
	ISkeleton* pSkeleton;
	if(character && (pSkeleton=character->GetISkeleton()))
		pSkeleton->StartAnimation(m_params.meleeAnimation,0,0,0,aparams);
	*/
	IAnimationGraphState* pAGState = GetAnimationGraphState();
	if ( pAGState)
		pAGState->SetInput( m_idSignalInput, "melee" );
	m_rollTime = m_params.attackRollTime;
	m_rollMaxAngle = sgn(m_stats.angVelocity.z)*m_params.attackRollAngle;
	if(m_rollMaxAngle==0)
		m_rollMaxAngle = m_params.attackRollAngle;
}


void	CShark::SetStance(EStance stance)
{
	CActor::SetStance(stance);
	const SStanceInfo* pStanceInfo = GetStanceInfo(stance);
}


//this will convert the input into a structure the player will use to process movement & input
void CShark::OnAction(const ActionId& actionId, int activationMode, float value)
{
	GetGameObject()->ChangedNetworkState( eEA_GameServerStatic | eEA_GameServerDynamic | eEA_GameClientStatic | eEA_GameClientDynamic );


	CActor::OnAction(actionId, activationMode, value);
}

void CShark::SetStats(SmartScriptTable &rTable)
{
	CActor::SetStats(rTable);
}

//fill the status table for the scripts
void CShark::UpdateScriptStats(SmartScriptTable &rTable)
{
	CActor::UpdateScriptStats(rTable);
}

void CShark::SetParams(SmartScriptTable &rTable,bool resetFirst)
{
	ScriptHandle hdl;
	if(rTable->GetValue("targetId",hdl))
	{
		m_targetId = hdl.n;
		return;
	}

	if(rTable->GetValue("selectTarget",hdl))
	{
		m_targetId = hdl.n;
		m_tryCount = 0;
		m_startPos.zero();
		m_state = S_Spawning;
		m_chosenEscapeDir.zero();
		FindEscapePoints();
		rTable->GetValue("spawned",m_params.bSpawned);
		return;
	}
	
	int goAway;
	if(rTable->GetValue("goAway",goAway))
	{
		GoAway();
		return;
	}

	//not sure about this
	if (resetFirst)
	{
		m_params = SSharkParams();
	}
	CActor::SetParams(rTable,resetFirst);

	rTable->GetValue("speedInertia",m_params.speedInertia);

	rTable->GetValue("sprintMultiplier",m_params.sprintMultiplier);
	rTable->GetValue("sprintDuration",m_params.sprintDuration);

	rTable->GetValue("rotSpeed_min",m_params.rotSpeed_min);
	rTable->GetValue("rotSpeed_max",m_params.rotSpeed_max);
	rTable->GetValue("minTurnRadius",m_params.minTurnRadius);

	rTable->GetValue("speed_min",m_params.speed_min);

	rTable->GetValue("accel",m_params.accel);
	rTable->GetValue("decel",m_params.decel);

	rTable->GetValue("minDistanceCircle",m_params.minDistanceCircle);
	rTable->GetValue("maxDistanceCircle",m_params.maxDistanceCircle);
	rTable->GetValue("circlingTime",m_params.circlingTime);
	rTable->GetValue("numCircles",m_params.numCircles);
	rTable->GetValue("minDistForUpdatingMoveTarget",m_params.minDistForUpdatingMoveTarget);
	const char *name;
	rTable->GetValue("headBone",name);
	m_params.headBoneName = name;
	rTable->GetValue("spineBone1",name);
	m_params.spineBoneName1 = name;
	rTable->GetValue("spineBone2",name);
	m_params.spineBoneName2 = name;
	
	SmartScriptTable meleeTable;
	if(rTable->GetValue("melee",meleeTable))
	{
		meleeTable->GetValue("radius",m_params.meleeDistance);
		meleeTable->GetValue("animation",name);
		m_params.meleeAnimation = name;
		meleeTable->GetValue("rollTime",m_params.attackRollTime);
		meleeTable->GetValue("rollAngle",m_params.attackRollAngle);
		m_params.attackRollAngle = DEG2RAD(m_params.attackRollAngle);
	}
	
	rTable->GetValue("escapeAnchorType",m_params.escapeAnchorType);
		

	/*m_params.trailEffect = "";
	const char *str;
	if (rTable->GetValue("trailEffect",str))
	{
		
		m_params.trailEffect = str;

		rTable->GetValue("trailEffectMinSpeed",m_params.trailEffectMinSpeed);
		rTable->GetValue("trailEffectMaxSpeedSize",m_params.trailEffectMaxSpeedSize);
		rTable->GetValue("trailEffectMaxSpeedCount",m_params.trailEffectMaxSpeedCount);
		rTable->GetValue("trailEffectDir",m_params.trailEffectDir);
	}
	*/
/*	if (rTable->GetValue("turnSound",str) && gEnv->pSoundSystem)
	{ 
		m_pTurnSound = gEnv->pSoundSystem->CreateSound(str, FLAG_SOUND_DEFAULT_3D);  

		if (m_pTurnSound)
			m_pTurnSound->SetSemantic(eSoundSemantic_Living_Entity);

		if (rTable->GetValue("turnSoundBone",str))
		{ 
			if (ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0))      
				m_params.turnSoundBoneId = pCharacter->GetISkeletonPose()->GetJointIDByName(str);
		}

		rTable->GetValue("turnSoundMaxVel", m_params.turnSoundMaxVel);
	}  
*/
}

void CShark::SetDesiredSpeed(const Vec3 &desiredSpeed)
{
	m_input.movementVector = desiredSpeed;
}

void CShark::SetDesiredDirection(const Vec3 &desiredDir)
{
	if (desiredDir.len2()>0.001f)
	{
		m_viewMtx.SetRotationVDir(desiredDir.GetNormalizedSafe());
		m_eyeMtx = m_viewMtx;
	}

	//m_input.viewVector = desiredDir;
}

// common functionality can go in here and called from subclasses that override SetActorMovement
void CShark::SetActorMovementCommon(SMovementRequestParams& control)
{
	SMovementState state;
	GetMovementController()->GetMovementState(state);

	if(!control.vAimTargetPos.IsZero())
		m_input.viewDir = (control.vAimTargetPos - state.weaponPosition).GetNormalizedSafe();
	else if(!control.vLookTargetPos.IsZero())
		m_input.viewDir = (control.vLookTargetPos - state.eyePosition).GetNormalizedSafe();
	else
		m_input.viewDir = GetEntity()->GetWorldRotation() * FORWARD_DIRECTION;
	m_input.pathLength = control.fDistanceToPathEnd;

	// added bExactPos property for easier testing
	SmartScriptTable props;
	int bExactPos = 0;
	if(IScriptTable* pScriptTable = GetEntity()->GetScriptTable())
		if(pScriptTable->GetValue("Properties",props))
			props->GetValue("bExactPos", bExactPos);

//	m_input.posTarget.zero();
//	m_input.dirTarget.zero();


}

//---------------------------------
//AI Specific
void CShark::SetActorMovement(SMovementRequestParams &control)
{
	SMovementState state;
	GetMovementController()->GetMovementState(state);

	SetActorMovementCommon(control);


	Vec3 bodyDir(GetEntity()->GetWorldRotation() * FORWARD_DIRECTION);
	SetDesiredDirection(bodyDir);

	// overrides AI (which doesn't request movement
	// shark always moves

	SetDesiredSpeed(bodyDir*m_curSpeed);//(control.vMoveDir * control.fDesiredSpeed);
	 
	// debug state and move target
	/*
	static char stateName[9][32] = {"S_Sleeping","S_Reaching","S_Circling","S_Attack","S_FinalAttackAlign","S_FinalAttack","S_PrepareEscape","S_Escaping","S_Spawning"};
	

	if(!m_moveTarget.IsZero())
	{
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_moveTarget, 0.5f, ColorB(255, 0, 255, 255), true);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(GetEntity()->GetWorldPos(), ColorB(0,255,0,100), m_moveTarget, ColorB(255,0,0,100));
		gEnv->pRenderer->DrawLabel(GetEntity()->GetWorldPos()+Vec3(0,0,2), 1.4f, stateName[m_state]);
		IEntity* pTarget = gEnv->pEntitySystem->GetEntity(m_targetId);
		if(pTarget)
			DrawSharkCircle(pTarget->GetWorldPos(),m_circleRadius);

	}
	else
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(GetEntity()->GetWorldPos()+Vec3(0,0,1), 0.5f, ColorB(255, 255, 255, 255), true);
	*/
	
	//	m_input.actions = control.m_desiredActions;
	/*
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


	m_input.actions = actions;

	*/
}


void CShark::GetActorInfo(SBodyInfo& bodyInfo)
{
	bodyInfo.vEyePos = GetEntity()->GetWorldPos();//GetEntity()->GetSlotWorldTM(0) * m_eyeOffset;
	bodyInfo.vFirePos = GetEntity()->GetWorldPos();//GetEntity()->GetSlotWorldTM(0) * m_weaponOffset;

	bodyInfo.vEyeDir = m_viewMtx.GetColumn(1);//m_eyeMtx.GetColumn(1);

	int headBoneID = GetBoneID(BONE_HEAD);
	if (headBoneID>-1 && GetEntity()->GetCharacter(0))
	{
		//Matrix33 HeadMat(GetEntity()->GetCharacter(0)->GetISkeleton()->GetAbsJMatrixByID(headBoneID));
		Matrix33 HeadMat( Matrix33(GetEntity()->GetCharacter(0)->GetISkeletonPose()->GetAbsJointByID(headBoneID).q) );
		bodyInfo.vEyeDirAnim = Matrix33(GetEntity()->GetSlotWorldTM(0) * HeadMat).GetColumn(1);
	} else {
		bodyInfo.vEyeDirAnim = bodyInfo.vEyeDir;
	}

	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(bodyInfo.vEyePos, ColorB(0,255,0,100), bodyInfo.vEyePos + bodyInfo.vEyeDir * 10.0f, ColorB(255,0,0,100));

	bodyInfo.vFwdDir = GetEntity()->GetRotation().GetColumn1();//m_viewMtx.GetColumn(1);
	bodyInfo.vUpDir = m_viewMtx.GetColumn(2);
	bodyInfo.vFireDir = bodyInfo.vFwdDir;
	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(GetEntity()->GetWorldPos(), ColorB(0,255,0,100), GetEntity()->GetWorldPos() + bodyInfo.vFwdDir * 10, ColorB(255,0,0,100));

	const SStanceInfo * pStanceInfo = GetStanceInfo(m_stance);
	bodyInfo.minSpeed = min(m_params.speed_min, pStanceInfo->maxSpeed*0.01f);
	bodyInfo.normalSpeed = pStanceInfo->normalSpeed;
	bodyInfo.maxSpeed = pStanceInfo->maxSpeed;
	// if you want to change the .eyeHeight for aliens to something different from 0 - talk to Kirill. It might cause problems in
	// CPuppet::AddToVisibleList when calculating time-out increase using target's eyeHeight.
	bodyInfo.stance = m_stance;
	bodyInfo.m_stanceSizeAABB = pStanceInfo->GetStanceBounds();
	bodyInfo.m_colliderSizeAABB = pStanceInfo->GetColliderBounds();
}

void CShark::SetAngles(const Ang3 &angles) 
{
	Matrix33 rot(Matrix33::CreateRotationXYZ(angles));
	SetDesiredDirection(rot.GetColumn(1));
}

Ang3 CShark::GetAngles() 
{
	Ang3 angles;
	angles.SetAnglesXYZ(m_viewMtx);

	return angles;
}

void CShark::SetMoveTarget(Vec3 moveTarget, bool bForced, float minDistForUpdate)
{
	// minDistForUpdate: don't reposition the move target if it's close enough to the shark, 
	// to avoid local sudden change directions
	if( bForced ||Distance::Point_PointSq(GetEntity()->GetWorldPos(),moveTarget)> minDistForUpdate * minDistForUpdate &&
		Distance::Point_PointSq(m_moveTarget,moveTarget)>1) 
	{
		m_moveTarget=moveTarget;
	}
}


void CShark::FindEscapePoints()
{
	m_EscapePoints.clear();
	if(m_params.escapeAnchorType && m_targetId)
	{
		IEntity* pTarget = gEnv->pEntitySystem->GetEntity(m_targetId);
		if(pTarget)
		{
			AutoAIObjectIter it(gEnv->pAISystem->GetFirstAIObjectInRange(IAISystem::OBJFILTER_TYPE, m_params.escapeAnchorType, pTarget->GetWorldPos(), 400, true));
			for(; it->GetObject(); it->Next())
				m_EscapePoints.push_back(it->GetObject()->GetPos());
		}
	}
}

void CShark::FullSerialize( TSerialize ser )
{
	CActor::FullSerialize(ser);

	ser.BeginGroup("CShark");		
	ser.Value("modelQuat", m_modelQuat, 'ori1');
	ser.Value("health", m_health);
	// skip matrices
	ser.Value("velocity", m_velocity);
	ser.Value("turnSpeed", m_turnSpeed);
	ser.Value("turnSpeedGoal", m_turnSpeedGoal);
	ser.Value("angularVel", m_angularVel);
	ser.EnumValue("stance", m_stance, STANCE_NULL, STANCE_LAST);
	
	ser.EnumValue("m_state",m_state,S_Reaching,S_Last);
	ser.Value("m_moveTarget",m_moveTarget);
	ser.Value("m_numHalfCircles",m_numHalfCircles);
	ser.Value("m_remainingCirclingTime",m_remainingCirclingTime);
	ser.Value("m_remainingAttackTime",m_remainingAttackTime);
	ser.Value("m_animationSpeed",m_animationSpeed);
	ser.Value("m_animationSpeedAttack",m_animationSpeedAttack);
	ser.Value("m_curSpeed",m_curSpeed);
	ser.Value("m_lastPos",m_lastPos);
	ser.Value("m_lastRot",m_lastRot);
	ser.Value("m_turnAnimBlend",m_turnAnimBlend);
	ser.Value("m_modelQuat",m_modelQuat);
	ser.Value("m_circleDisplacement",m_circleDisplacement);
	ser.Value("m_bCircularTrajectory",m_bCircularTrajectory);
	ser.Value("m_turnRadius",m_turnRadius);
	ser.Value("m_rollTime",m_rollTime);
	ser.Value("m_roll",m_roll);
	ser.Value("m_rollMaxAngle",m_rollMaxAngle);
	ser.Value("m_chosenEscapeDir",m_chosenEscapeDir);
	ser.Value("m_escapeDir",m_escapeDir);
	ser.Value("m_tryCount",m_tryCount);
	ser.Value("m_targetId",m_targetId);
	ser.Value("m_AABBMaxZCache",m_AABBMaxZCache);
	ser.Value("m_lastCheckedPos",m_lastCheckedPos);
	ser.Value("m_circleRadius",m_circleRadius);
	ser.Value("m_startPos",m_startPos);

	ser.BeginGroup("SharkEscapePoints");
	{
		int size = m_EscapePoints.size();
		ser.Value("Size",size);
		char name[32];
		if(ser.IsReading())
			m_EscapePoints.clear();

		for(int i=0; i<size; i++)
		{
			Vec3 point;
			sprintf(name,"Point_%d",i);

			if(ser.IsWriting())
				point = m_EscapePoints[i];

			ser.Value(name,point);

			if(ser.IsReading())
				m_EscapePoints.push_back(point);
		}
		ser.EndGroup();
	}
	ser.EndGroup();

	m_input.Serialize(ser);
//	m_stats.Serialize(ser);
	m_params.Serialize(ser);
}

void CShark::SerializeXML( XmlNodeRef& node, bool bLoading )
{
}

void CShark::SetAuthority( bool auth )
{

}

IActorMovementController * CShark::CreateMovementController()
{
	return new CSharkMovementController(this);
}

void SSharkInput::Serialize( TSerialize ser )
{
	ser.BeginGroup("SSharkInput");
	ser.Value("deltaMovement", deltaMovement);
	ser.Value("deltaRotation", deltaRotation);
	ser.Value("actions", actions);
	ser.Value("movementVector", movementVector);
	ser.Value("viewVector", viewVector);
/*	ser.Value("posTarget", posTarget);
	ser.Value("dirTarget", dirTarget);
	ser.Value("upTarget", upTarget);
	ser.Value("speedTarget", speedTarget);*/
	ser.EndGroup();
}

void SSharkParams::Serialize( TSerialize ser )
{
	ser.BeginGroup("SSharkParams");
	ser.Value("speedInertia", speedInertia);
	ser.Value("rollAmount", rollAmount);
	ser.Value("sprintMultiplier", sprintMultiplier);
	ser.Value("sprintDuration", sprintDuration);
	ser.Value("rotSpeed_min", rotSpeed_min);
	ser.Value("rotSpeed_max", rotSpeed_max);
	ser.Value("speed_min", speed_min);

	ser.Value("accel",accel);
	ser.Value("decel",decel);

	ser.Value("minDistanceCircle",minDistanceCircle);
	ser.Value("maxDistanceCircle",maxDistanceCircle);
	ser.Value("numCircles",numCircles);
	ser.Value("minDistForUpdatingMoveTarget",minDistForUpdatingMoveTarget);
	ser.Value("meleeAnimation",meleeAnimation);
	ser.Value("meleeDistance",meleeDistance);
	ser.Value("circlingTime",circlingTime);
	ser.Value("attackRollTime",attackRollTime);
	ser.Value("attackRollAngle",attackRollAngle);

	ser.Value("headBoneName",headBoneName);
	ser.Value("spineBoneName1",spineBoneName1);
	ser.Value("spineBoneName2",spineBoneName2);
	ser.Value("escapeAnchorType",escapeAnchorType);
	ser.Value("bSpawned",bSpawned);

	ser.EndGroup();
}
/*
void SSharkStats::Serialize( TSerialize ser )
{
	ser.BeginGroup("SSharkStats");
	ser.Value("inAir", inAir);
	ser.Value("onGround", onGround);
	ser.Value("speedModule", speed);
	ser.Value("mass", mass);
	ser.Value("bobCycle", bobCycle);
	ser.Value("isRagdoll", isRagDoll);
	ser.Value("velocity", velocity);
//	ser.Value("eyePos", eyePos);
	//ser.Value("eyeAngles", eyeAngles);
	//ser.Value("dynModelOffset",dynModelOffset);
	ser.EndGroup();
}
*/
