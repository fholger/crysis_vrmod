/************************************************************************* 
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 21:7:2005: Created by Mikko Mononen

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"

#include "Trooper.h"
#include "GameUtils.h"

#include <IViewSystem.h>
#include <IItemSystem.h>
#include <IPhysics.h>
#include <ICryAnimation.h>
#include <ISerialize.h>
#include <IRenderAuxGeom.h>
#include <IMaterialEffects.h>
#include <IEffectSystem.h>

const float CTrooper::CTentacle_maxTimeStep = 0.02f;
const float CTrooper::CMaxHeadFOR = 0.5*3.14159f;
const float CTrooper::ClandDuration = 1.f;
const float CTrooper::ClandStiffnessMultiplier = 10;


void CTrooper::Revive(bool fromInit)
{
	m_customLookIKBlends[0] = 0;
	m_customLookIKBlends[1] = 0;
	m_customLookIKBlends[2] = 0;
	m_customLookIKBlends[3] = 0;
	m_customLookIKBlends[4] = 1;
	
	m_steerInertia = 0;

	CAlien::Revive(fromInit);

	m_modelQuat.SetIdentity();
	//m_modelAddQuat.SetIdentity();

	if (m_pAnimatedCharacter)
	{
		SAnimatedCharacterParams params = m_pAnimatedCharacter->GetParams();
		params.flags &= ~eACF_NoTransRot2k;
		m_pAnimatedCharacter->SetParams(params);
	}

	m_lastNotMovingTime = gEnv->pSystem->GetITimer()->GetFrameStartTime();
	m_oldSpeed = 0;
	m_Roll = 0;
	m_Rollx = 0;
	m_oldDirFwd = 0;
	m_oldDirStrafe = 0;
	m_oldVelocity = ZERO;

	m_fDistanceToPathEnd = 0;
	m_bExactPositioning = false;
	m_lastExactPositioningTime = 0.f;
	
	m_landModelOffset = ZERO;
	m_steerModelOffset = ZERO;

	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	int tNum(0);
	IPhysicalEntity *pTentacle = pCharacter ? pCharacter->GetISkeletonPose()->GetCharacterPhysics(tNum) : NULL;

	pe_simulation_params ps;
	ps.maxTimeStep = CTentacle_maxTimeStep;

	while(pTentacle)
	{
		pTentacle->SetParams(&ps);
		pTentacle = pCharacter->GetISkeletonPose()->GetCharacterPhysics(++tNum);
	}

	if (m_stance == m_desiredStance)
		m_stance = STANCE_NULL;
	UpdateStance();

	m_lastTimeOnGround = gEnv->pSystem->GetITimer()->GetFrameStartTime();
	m_bOverrideFlyActionAnim = false;
	m_overrideFlyAction = "idle";
	
	m_bNarrowEnvironment = false;
	m_lastCheckEnvironmentPos = ZERO;
	m_fTtentacleBlendRotation = 0.f;

	m_jumpParams.Reset();
	IScriptTable* scriptTable = GetEntity()->GetScriptTable();
	if (scriptTable)
		scriptTable->GetValue("landPreparationTime", m_jumpParams.defaultLandPreparationTime);


/*
	m_CollSize.clear();
	//m_CollSize.insert(std::make_pair(STANCE_NULL,GetStanceInfo(STANCE_NULL)->size));
	m_CollSize.insert(std::make_pair(STANCE_STAND,GetStanceInfo(STANCE_STAND)->size));
	m_CollSize.insert(std::make_pair(STANCE_STEALTH,GetStanceInfo(STANCE_STEALTH)->size));
	m_CollSize.insert(std::make_pair(STANCE_CROUCH,GetStanceInfo(STANCE_CROUCH)->size));
	m_CollSize.insert(std::make_pair(STANCE_RELAXED,GetStanceInfo(STANCE_RELAXED)->size));
	//m_CollSize.insert(std::make_pair(STANCE_PRONE,GetStanceInfo(STANCE_PRONE)->size));
	*/
}

void CTrooper::SetParams(SmartScriptTable &rTable,bool resetFirst)
{

	if(rTable->GetValue("useLandEvent",m_jumpParams.bUseLandEvent ))
	  return;
	if(rTable->GetValue("specialAnimType",(int&)m_jumpParams.specialAnimType))
	{
		if(m_jumpParams.specialAnimType ==JUMP_ANIM_LAND)
			m_jumpParams.bUseLandAnim = true;
		rTable->GetValue("specialAnimAGInput",(int&)m_jumpParams.specialAnimAGInput);
		char* szValue=NULL;
		if(rTable->HaveValue("specialAnimAGInputValue"))
		{
			rTable->GetValue("specialAnimAGInputValue",szValue);
			m_jumpParams.specialAnimAGInputValue = szValue;
		}
		return;
	}

	char* szValue2=NULL;
	if(rTable->GetValue("overrideFlyAction",szValue2))
	{
		m_overrideFlyAction = szValue2;
		return;
	}	

	m_jumpParams.addVelocity = ZERO;
	if(rTable->GetValue("jumpTo",m_jumpParams.dest) && !m_jumpParams.dest.IsZero() || rTable->GetValue("addVelocity",m_jumpParams.addVelocity))
	{
		rTable->GetValue("jumpVelocity",m_jumpParams.velocity);
		rTable->GetValue("jumpTime",m_jumpParams.duration);
		m_jumpParams.bUseStartAnim = false;
		rTable->GetValue("jumpStart",m_jumpParams.bUseStartAnim);
		m_jumpParams.bUseSpecialAnim = false;
		rTable->GetValue("useSpecialAnim",m_jumpParams.bUseSpecialAnim);
		m_jumpParams.bUseAnimEvent = false;
		rTable->GetValue("useAnimEvent",m_jumpParams.bUseAnimEvent);
		m_jumpParams.bUseLandAnim = false;
		rTable->GetValue("jumpLand", m_jumpParams.bUseLandAnim);
		m_jumpParams.bRelative = false;
		rTable->GetValue("relative", m_jumpParams.bRelative);
	
		m_jumpParams.landPreparationTime = m_jumpParams.defaultLandPreparationTime;

		//m_input.actions |= ACTION_JUMP;
		m_jumpParams.bTrigger = true;
		//m_jumpParams.state = JS_None;
		m_jumpParams.bFreeFall = false;
		m_jumpParams.bPlayingSpecialAnim = false;
	}
	else
	{
		m_jumpParams.state = JS_None;
		CAlien::SetParams(rTable,resetFirst);
		InitHeightVariance(rTable);
	}

	rTable->GetValue("landPreparationTime",m_jumpParams.landPreparationTime);
	rTable->GetValue("steerInertia",m_steerInertia);


	
}

void CTrooper::InitHeightVariance(SmartScriptTable &rTable)
{

	float freqMin = 0, freqMax=0;

	if(rTable->GetValue("heightVarianceLow",m_heightVarianceLow) && rTable->GetValue("heightVarianceHigh",m_heightVarianceHigh))
	{
		m_heightVarianceRandomize = cry_rand () / (float )RAND_MAX;
/*		float range = m_heightVarianceHigh - m_heightVarianceLow;
		m_heightVarianceLow = m_heightVarianceLow-range/2;
		m_heightVarianceHigh = m_heightVarianceLow+range/2;*/
	}
	if(rTable->GetValue("heightVarianceOscMin",freqMin) && rTable->GetValue("heightVarianceOscMax",freqMax))
		m_heightVarianceFreq = freqMin + (cry_rand () / (float )RAND_MAX) * (freqMax - freqMin);
}


void CTrooper::Update(SEntityUpdateContext& ctx, int updateSlot)
{
	IEntity* pEnt = GetEntity();
	if (pEnt->IsHidden())
		return;

	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	CActor::Update(ctx,updateSlot);

	const float frameTime = ctx.fFrameTime;

	if (!m_stats.isRagDoll && GetHealth()>0)
	{
		//animation processing
		ProcessAnimation(pEnt->GetCharacter(0),frameTime);

		//reset the input for the next frame
		if (IsClient())
			m_input.ResetDeltas();

		//update tentacles blending
		Vec3 refVec(-m_viewMtx.GetColumn(1)*max(0.1f,m_params.forceView) + -m_desiredVelocity);
		refVec.NormalizeSafe();

		float directionDot = min(1.0f,fabsf(refVec * m_baseMtx.GetColumn(0)) * 3.0f);
		float animStiff = 0.0f;

		if (m_params.blendingRatio>0.001f)
		{
			float ratio((GetStanceInfo(m_stance)->maxSpeed - m_stats.speed * directionDot) / GetStanceInfo(m_stance)->maxSpeed);
			Interpolate(m_tentacleBlendRatio,m_params.blendingRatio,20.0f,frameTime);
			animStiff = 1.0f + (ratio) * m_tentacleBlendRatio;
		}

		//SetTentacles(pCharacter,animStiff);
		//CryLogAlways("%.1f",animStiff);
		if (gEnv->bClient)
		{
			float dist2 = (gEnv->pRenderer->GetCamera().GetPosition() - GetEntity()->GetWorldPos()).GetLengthSquared();

			//update ground effects, if any
			if (m_pGroundEffect)
			{ 
				float cloakMult = (m_stats.cloaked) ? 0.5f : 1.f;      
				float sizeScale = m_params.groundEffectBaseScale * cloakMult;
				float countScale = /*1.f * */ cloakMult;
				float speedScale = /*1.f * */ cloakMult; 

				if (m_params.groundEffectMaxSpeed != 0.f)
				{
					const static float minspeed = 1.f;
					float speed = max(0.f, m_stats.speed + m_stats.angVelocity.len() - minspeed);
					float speedScale = min(1.f, speed / m_params.groundEffectMaxSpeed);          
					sizeScale *= speedScale;
					countScale *= speedScale;
				}

				m_pGroundEffect->SetBaseScale(sizeScale, countScale, speedScale);
				m_pGroundEffect->Update();       
			}

			if (m_pTrailAttachment)
			{ 
				CEffectAttachment* pEffectAttachment = (CEffectAttachment*)m_pTrailAttachment->GetIAttachmentObject();
				if (pEffectAttachment)
				{
					float goalspeed = max(1.f, m_stats.speed - m_params.trailEffectMinSpeed);
					Interpolate(m_trailSpeedScale, goalspeed, 3.f, frameTime);

					SpawnParams sp;          
					//if (m_params.trailEffectMaxSpeedSize != 0.f)
						sp.fSizeScale = max(0.01f, div_min(m_trailSpeedScale,m_params.trailEffectMaxSpeedSize,1.f));
						//sp.fSizeScale = min(1.f, max(0.01f, m_trailSpeedScale/m_params.trailEffectMaxSpeedSize));

					//if (m_params.trailEffectMaxSpeedCount != 0.f)
						sp.fCountScale = div_min(m_trailSpeedScale, m_params.trailEffectMaxSpeedCount, 1.f);
						//sp.fCountScale = min(1.f, m_trailSpeedScale / m_params.trailEffectMaxSpeedCount);

					pEffectAttachment->SetSpawnParams(sp);
				}
			}

			if (m_pHealthTrailAttachment)
			{ 
				CEffectAttachment* pEffectAttachment = (CEffectAttachment*)m_pHealthTrailAttachment->GetIAttachmentObject();
				if (pEffectAttachment)
				{
					float goal = 1.0f - ((float)GetHealth() / (float)max(1, GetMaxHealth()));
					Interpolate(m_healthTrailScale, goal, 2.f, frameTime);

					SpawnParams sp;          
					if (m_params.healthTrailEffectMaxSize != 0.f)
						sp.fSizeScale = min(1.f, max(0.01f, m_healthTrailScale / m_params.healthTrailEffectMaxSize));

					if (m_params.healthTrailEffectMaxCount != 0.f)
						sp.fCountScale = 1.0f; // min(1.f, m_healthTrailScale / m_params.healthTrailEffectMaxCount);

					pEffectAttachment->SetSpawnParams(sp);
				}
			}

			if (m_searchbeam.active)
				UpdateSearchBeam(frameTime);

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
						int nIndex = m_pTurnSound->SetParam("acceleration", speedRel);
						pSoundProxy->PlaySound(m_pTurnSound);        
						pSoundProxy->SetStaticSound(m_pTurnSound->GetId(), true);
						//CryLog("angSpeed %.2f (rel %.2f)", dyn.w.len(), speedRel);
					} 
				}    
			}
		}
	}


	m_oldSpeed = m_stats.speed;

}


void CTrooper::UpdateStats(float frameTime)
{
	CAlien::UpdateStats(frameTime);

	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();
	if (!pPhysEnt)
		return;

	if (InZeroG())
	{
		ray_hit hit;
		int rayFlags = (COLLISION_RAY_PIERCABILITY & rwi_pierceability_mask);

		if (gEnv->pPhysicalWorld->RayWorldIntersection(GetEntity()->GetWorldPos(), m_baseMtx.GetColumn(2)*-5.0f, ent_terrain|ent_static|ent_rigid, rayFlags, &hit, 1, &pPhysEnt, 1))
		{
			pe_player_dynamics newGravity;
			newGravity.gravity = m_baseMtx.GetColumn(2) * -9.81f;
			pPhysEnt->SetParams(&newGravity);
		}
		else
			m_stats.isFloating = true;
	}
	else
	{
		pe_player_dynamics newGravity;
		m_stats.gravity.Set(0,0,-9.81f);
		newGravity.gravity = m_stats.gravity;
		pPhysEnt->SetParams(&newGravity);
	}
}

void CTrooper::ProcessRotation(float frameTime)
{

	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();

	if (!pPhysEnt)
		return;

	if (frameTime > 0.2f)
		frameTime = 0.2f;

	float rotSpeed(10.5f);
/*
	if (m_input.viewVector.len2()>0.0f)
	{
		m_eyeMtx.SetRotationVDir(m_input.viewVector.GetNormalizedSafe());
	}
*/
	if (m_input.viewVector.len2()>0.0f )
	{
		/*const SAnimationTarget * pAnimTarget = GetAnimationGraphState()->GetAnimationTarget();
		if (!(pAnimTarget != NULL ))
			m_viewMtx.SetRotationVDir(m_input.viewVector.GetNormalizedSafe());*/
	}
	else
	{
		/* TODO: check, rotation is done in SetDesiredDirection
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
		*/

	}

	//now build the base matrix
	Vec3 forward(m_viewMtx.GetColumn(1));

	if (forward.z>-0.9f && forward.z<0.9f) //apply rotation only if trooper is not looking at too vertical direction
	{
		Vec3 forwardMove(m_stats.velocity.GetNormalizedSafe());
		if(forwardMove.IsZero())
			forwardMove = forward;
		Vec3 forwardMoveXY(forwardMove.x,forwardMove.y, 0);
		forwardMoveXY.NormalizeSafe();

		Quat currRotation(GetEntity()->GetRotation());
		currRotation.Normalize();

		float roll = 0;
		float rollx = 0;
		/*
		IPhysicalEntity *phys = GetEntity()->GetPhysics();
		pe_status_dynamics	dyn;
		if(phys)
			phys->GetStatus(&dyn);
		*/
		Vec3 up(m_viewMtx.GetColumn2());
		
		int oldDir = m_stats.movementDir;

		if ((forward-up).len2()>0.001f)
		{
			float dotY(forwardMoveXY * m_viewMtx.GetColumn(1));
			//float dotX(forwardMoveXY * m_viewMtx.GetColumn(0));
			if (dotY<-0.6f)
			{
				m_stats.movementDir = 1;	//moving backwards 
			}
			else
			{
				m_stats.movementDir = 0;
			}


			IEntity* pEntity = GetEntity();
			IAIObject* pAIObject;
			IUnknownProxy* pProxy;
			if(pEntity && ( pAIObject = pEntity->GetAI())	&& ( pProxy = pAIObject->GetProxy()))
			{

				IAnimationGraphState* pAGState = GetAnimationGraphState();
				if ( pAGState )
				{
					pAGState->SetInput( m_idMovementInput, m_stats.movementDir );
				}
			}
			//if (m_stats.inAir<0.2f)
//			forward = m_viewMtx.GetColumn(1);

			IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();
			if(m_bExactPositioning)
			{
				roll = 0;
				rollx = 0;
				m_oldDirStrafe = 0;
				m_oldDirFwd = 0;
				if (pPhysEnt)
				{
					pe_player_dimensions params;
					if(pPhysEnt->GetParams(&params))
					{
						if(params.heightPivot !=0 )
						{
							Interpolate(params.heightPivot,0,10.f,frameTime);
							pPhysEnt->SetParams(&params);
						}
					}
				}
				/*
					Matrix33 mrot(m_baseMtx);
					Matrix33 mrot2(m_modelQuat);
					Vec3 basepos = GetEntity()->GetWorldPos()+Vec3(0,0,0.7f);
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(28,28,0,255), basepos + mrot.GetColumn(0) * 2.0f, ColorB(128,128,0,255));
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(192,192,0,255), basepos + mrot.GetColumn(1)* 2.0f, ColorB(192,192,0,255));
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(40,40,0,255), basepos + mrot.GetColumn(2) * 2.0f, ColorB(140,140,0,255));
					basepos.z +=0.1f;
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(38,0,0,255), basepos + mrot2.GetColumn(0) * 2.0f, ColorB(138,0,0,255));
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(255,0,0,255), basepos + mrot2.GetColumn(1)* 2.0f, ColorB(255,0,0,255));
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(70,0,0,255), basepos + mrot2.GetColumn(2) * 2.0f, ColorB(170,0,0,255));

					basepos.z +=0.4f;
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(255,255,255,255), basepos + forward * 3.0f, ColorB(255,255,255,255));

				*/
			}
			else if(m_stats.inAir >0)
			{
				roll=0;
				rollx=0;
				m_oldDirStrafe = 0;
				m_oldDirFwd = 0;
			}
			else
			{
				// Luciano : add banking factor when moving 

				Vec3 forwardXY(forward.x,forward.y,0);
				forwardXY.NormalizeSafe();

				Matrix33 invViewMtx(m_viewMtx.GetInverted());
				Vec3 localVel = invViewMtx * m_stats.velocity;

				float vX = localVel.x;
				float l = localVel.GetLength2D();
				float vXn = (l>0 ? vX/l : 0);
				float accelStrafe =  iszero(frameTime)  ? 0.0f : (vX - m_oldDirStrafe)/frameTime;

				m_oldDirStrafe = vX;
				vXn *= vXn;

				float bankMultiplier  = g_pGameCVars->g_trooperBankingMultiplier;

				roll =  vXn*vXn *(accelStrafe/3 +  vX /6)*bankMultiplier;
				roll = min(max(-DEG2RAD(15.0f),roll),DEG2RAD(15.0f));

				/*	disabled banking around X (when moving forward/backward)
				float vY = localVel.y;
				float vYn = (l>0 ? vY/l : 0);
				float accelFwd =  (vY - m_oldDirFwd)/frameTime;
				vYn *= vYn;
				m_oldDirFwd = vY;
				rollx =  -vYn*vYn *(accelFwd/6 +  vY /6)*bankMultiplier;
				if(rollx > 0)
					rollx/=2;
				rollx = min(max(-DEG2RAD(15.0f),rollx),DEG2RAD(7.5f));
				*/

				// tilt the trooper more like the ground
				pe_status_living livStat;
				if(pPhysEnt)
				{
					pPhysEnt->GetStatus(&livStat);
					Vec3 groundNormal((up+livStat.groundSlope).GetNormalizedSafe());
					Vec3 localUp(invViewMtx * (Vec3Constants<float>::fVec3_OneZ * Matrix33::CreateRotationXYZ(Ang3(rollx,roll,0))));
					Vec3 localGroundN(invViewMtx * groundNormal);
					Vec3 localUpx(localUp.x,0,localUp.z);
					Vec3 localUpy(0, localUp.y, localUp.z);
					Vec3 localGroundNx(localGroundN.x, 0, localGroundN.z);
					Vec3 localGroundNy(0, localGroundN.y, localGroundN.z);
					localUpx.NormalizeSafe();
					localUpy.NormalizeSafe();
					localGroundNx.NormalizeSafe();
					localGroundNy.NormalizeSafe();

					float dotNy = localUpy.Dot(localGroundNy);
					float dotNx = localUpx.Dot(localGroundNx);
					if(dotNy>1)
						dotNy = 1;
					if(dotNx>1)
						dotNx = 1;

					float angley = cry_acosf(dotNx)*sgn(localGroundN.x - localUp.x);
					float anglex = cry_acosf(dotNy)*sgn(localUp.y - localGroundN.y);

					static const float BANK_PRECISION = 100.f;
					anglex = floor(anglex * BANK_PRECISION)/BANK_PRECISION;
					angley = floor(angley * BANK_PRECISION)/BANK_PRECISION;
					//y =angley;
					rollx += anglex;
					roll += angley;

				}


			}

			//bool bRollYMatch = InterpolatePrecise(m_Roll, roll,2.0f,frameTime,0.001f,3.0f);
			//bool bRollXMatch = InterpolatePrecise(m_Rollx, rollx,2.0f,frameTime,0.001f,3.0f);
			Interpolate(m_Roll, roll,2.0f,frameTime,3.0f);
			Interpolate(m_Rollx, rollx,2.0f,frameTime,3.0f);

			up.Set(0,0,1);
			forward.z=0;
			forward.NormalizeSafe();
			Vec3 right = -(up % forward).GetNormalized();

			Quat currQuat(Matrix33::CreateFromVectors(right,up%right,up));
			currQuat.Normalize();

//			Quat goalQuat(Matrix33::CreateIdentity() * Matrix33::CreateRotationXYZ(Ang3(m_Rollx,m_Roll,0)));
//			goalQuat.Normalize();

			float maxSpeed = GetStanceInfo( m_stance )->maxSpeed;
			float rotSpeed;// = m_params.rotSpeed_min + (1.0f - (max(maxSpeed - max(m_stats.speed - m_params.speed_min,0.0f),0.0f) / maxSpeed)) * (m_params.rotSpeed_max - m_params.rotSpeed_min);

		//	SMovementState state;
		//	GetMovementController()->GetMovementState(state);
			SActorStats *pStats = GetActorStats();

			if(pStats && pStats->inFiring > 8.5f)
				rotSpeed = m_params.rotSpeed_max;
			else if(m_bExactPositioning)
				rotSpeed = m_params.rotSpeed_max*3;
			else
				rotSpeed = m_params.rotSpeed_min + (max(maxSpeed - max(m_stats.speed - m_params.speed_min,0.0f),0.0f) / maxSpeed) * (m_params.rotSpeed_max - m_params.rotSpeed_min);

			Interpolate(m_turnSpeed,rotSpeed,2.5f,frameTime);
			//m_turnSpeed = rotSpeed;

			m_modelQuat = Quat::CreateSlerp(currRotation, currQuat, min(1.0f,frameTime * m_turnSpeed)  );
			m_modelQuat.Normalize();


			m_moveRequest.rotation = currRotation.GetInverted() * m_modelQuat;
			m_moveRequest.rotation.Normalize();
			
			m_baseMtx = Matrix33(Quat::CreateSlerp( currQuat, m_modelQuat, frameTime * m_turnSpeed ));
			m_baseMtx.OrthonormalizeFast();
			
			//update the character offset
			Vec3 goal = (m_stats.isRagDoll?Vec3(0,0,0):GetStanceInfo(m_stance)->modelOffset);
			Interpolate(m_modelOffset,goal,5.0f,frameTime);

//			m_modelAddQuat.SetIdentity();
			m_charLocalMtx.SetIdentity();
			pe_player_dimensions params;
			if(pPhysEnt->GetParams(&params))
			{
				// rotate the character around the collider center
				m_charLocalMtx.SetRotationXYZ(Ang3(m_Rollx,m_Roll,0));
				Vec3 pivot(0,0,params.heightCollider);
				Vec3 trans( m_charLocalMtx.TransformVector(pivot));
				trans.z -= pivot.z;
				m_charLocalMtx.SetTranslation(-trans + m_modelOffset+m_modelOffsetAdd);
//				float transx = heightPivot * tan(m_Rollx);
//				float transy = heightPivot * tan(m_Roll);
//				m_modelAddQuat.SetTranslation(Vec3(-transy, -transx,0) + m_modelOffset+m_modelOffsetAdd);
			}
				//m_modelAddQuat = QuatT(Matrix33::CreateIdentity() * Matrix33::CreateRotationXYZ(Ang3(m_Rollx,m_Roll,0)));//goalQuat;

			//m_charLocalMtx = Matrix34(m_modelAddQuat);
			GetAnimatedCharacter()->SetExtraAnimationOffset(m_charLocalMtx);
			//GetAnimatedCharacter()->SetExtraAnimationOffset(m_modelAddQuat);
/*
			m_modelQuat = Quat::CreateSlerp( GetEntity()->GetRotation().GetNormalized(), goalQuat, min(0.5f,frameTime * m_turnSpeed)  );
			m_modelQuat.Normalize();
			m_moveRequest.rotation = currRotation.GetInverted() * m_modelQuat;
			m_moveRequest.rotation.Normalize();
			Quat currQuat(m_baseMtx);
			m_baseMtx = Matrix33(Quat::CreateSlerp( currQuat.GetNormalized(), m_modelQuat, frameTime * m_turnSpeed ));
			m_baseMtx.OrthonormalizeFast();
*/

			/*	
			Matrix33 mrot(m_charLocalMtx);
			Matrix33 mrot2(m_modelQuat);
			Vec3 basepos = GetEntity()->GetWorldPos()+Vec3(0,0,0.7f);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(28,28,0,255), basepos + mrot.GetColumn(0) * 2.0f, ColorB(128,128,0,255));
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(192,192,0,255), basepos + mrot.GetColumn(1)* 2.0f, ColorB(192,192,0,255));
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(40,40,0,255), basepos + mrot.GetColumn(2) * 2.0f, ColorB(140,140,0,255));
		
			basepos.z +=0.1f;
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(38,0,0,255), basepos + mrot2.GetColumn(0) * 2.0f, ColorB(138,0,0,255));
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(255,0,0,255), basepos + mrot2.GetColumn(1)* 2.0f, ColorB(255,0,0,255));
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(70,0,0,255), basepos + mrot2.GetColumn(2) * 2.0f, ColorB(170,0,0,255));
			
			basepos.z +=0.4f;
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(basepos, ColorB(255,255,255,255), basepos + forward * 3.0f, ColorB(255,255,255,255));
			*/
		}		
		else
			m_moveRequest.rotation.SetIdentity();
	}
}

void CTrooper::UpdateAnimGraph( IAnimationGraphState * pState )
{
	CActor::UpdateAnimGraph(pState);

	if (pState)
	{
		pState->SetInput(m_inputSpeed, m_stats.inAir <=0 && !m_bExactPositioning ? m_stats.speed : 0);
		pState->SetInput(m_inputDesiredSpeed, m_stats.desiredSpeed);
	}
}

void CTrooper::SetAnimTentacleParams(pe_params_rope& pRope, float physicBlend)
{

	float stiffnessOverride  = g_pGameCVars->g_trooperTentacleAnimBlend;
	if (/*m_bExactPositioning ||*/ stiffnessOverride < 0 || physicBlend<0.001f)
	{
		pRope.stiffnessAnim = 0;	// Special case, use full animation.
		pRope.dampingAnim = 1.0f;	// When stiffness is zero, this value does not really matter, set it to sane value anyway.
	}
	else
	{
		/*
		IPhysicalEntity *phys = GetEntity()->GetPhysics();
		pe_status_dynamics	dyn;
		if(phys)
			phys->GetStatus(&dyn);
		*/
		float coeff = 1-sqrt(physicBlend< 0 ? 0 : physicBlend);

		float frameTime = gEnv->pSystem->GetITimer()->GetFrameTime();
		if(frameTime==0) 
			frameTime = 0.05f;

		// check big rotations
		float rot = m_viewMtx.GetColumn1().GetNormalizedSafe().Dot(m_baseMtx.GetColumn1().GetNormalizedSafe());
		if(m_fTtentacleBlendRotation==0)
		{
			if(rot<0)
				m_fTtentacleBlendRotation = 0.5f - rot;
		}
		if(m_fTtentacleBlendRotation>0)
		{
				coeff -= 0.4f*(m_fTtentacleBlendRotation/1.5);
				if(coeff<0.01f)
					coeff=0.01f;
				m_fTtentacleBlendRotation -= frameTime;
				if(m_fTtentacleBlendRotation <0)
					m_fTtentacleBlendRotation =0;
		}
		//Vec3 velocity = (phys ? dyn.v : m_moveRequest.velocity);
		//float speed = velocity.GetLength();
		float vertSpeed = fabs(m_stats.velocity.z);
		coeff *= (vertSpeed/10+1);
		if(m_stats.speed>0.03f) //moving either backward or forward, increase stiffness
		{
			Vec3 forward = m_stats.velocity.GetNormalizedSafe();
			float dotX = forward * m_viewMtx.GetColumn(1);
			//coeff *= (2 - fabs(dotX));
			if(dotX>0.5f)
				coeff *= (1 + 2*(dotX-0.5f));
		} 
		// make tentacles stiffer when accelerating/decelerating

		float accelCoeff = 1+fabs(m_stats.speed - m_oldSpeed)/frameTime/30;
		if(accelCoeff > 2)
			accelCoeff =2;

		coeff *= accelCoeff;
		if(coeff>=1)
		{
			//coeff=1;
			pRope.stiffnessAnim = 0;
			pRope.dampingAnim = 1;
		}
		else
		{
			pRope.stiffnessAnim = 0.9*coeff/CTentacle_maxTimeStep;	
			pRope.dampingAnim = 0.4f*coeff/CTentacle_maxTimeStep;
		}
	}

}


void CTrooper::ProcessMovement(float frameTime)
{

	if (frameTime > 0.2f)
		frameTime = 0.2f;

	//movement
	Vec3 move;
	float	reqSpeed, maxSpeed;
	GetMovementVector(move, reqSpeed, maxSpeed);

	// NOTE Jan 18, 2007: <pvl> preserve unmodified AI request for later use
	Vec3 ai_requested_movement = move;

	CTimeValue currTime = gEnv->pSystem->GetITimer()->GetFrameStartTime();

	if (!m_stats.isFloating)
		move -= move * (m_baseMtx * Matrix33::CreateScale(Vec3Constants<float>::fVec3_OneZ));//make it flat

	if (m_stats.sprintLeft)
		move *= m_params.sprintMultiplier;

	m_moveRequest.type = eCMT_Normal;

	if (m_bExactPositioning)
	{
		m_lastExactPositioningTime = currTime;
	}

	IAnimationGraphState* pAGState = GetAnimationGraphState();
	IPhysicalEntity *phys = GetEntity()->GetPhysics();

	if (!m_bExactPositioning)// && 
		//(gEnv->pSystem->GetITimer()->GetFrameStartTime() - m_lastExactPositioningTime).GetSeconds() > 0.5f)
	{


		if(m_jumpParams.bTrigger)
		{
			m_jumpParams.bTrigger = false;
			m_jumpParams.state = JS_JumpStart;
			m_jumpParams.startTime = currTime;
			//m_moveRequest.type = eCMT_JumpAccumulate;
			IAnimationGraphState* pAGState = GetAnimationGraphState();
			if ( pAGState && m_jumpParams.bUseStartAnim )
			{
				Vec3 vN(m_jumpParams.velocity);
				Vec3 vNx(vN);
				vN.NormalizeSafe();
				vNx.z=0;
				vNx.NormalizeSafe();
				float dot = vN.Dot(vNx);
				float anglex = RAD2DEG(cry_acosf(CLAMP(dot,-1.f,1.f)));
				if(m_viewMtx.GetColumn1().Dot(m_jumpParams.velocity) <-0.001) // jump backwards
					anglex = 180 - anglex;
					
				pAGState->SetInput( m_idAngleXInput, anglex);

				Vec3 viewDir(m_viewMtx.GetColumn(1));
				dot = vNx.Dot(viewDir);
				float anglez = RAD2DEG(cry_acosf(CLAMP(dot,-1.f,1.f))*sgn(vNx.Cross(viewDir).z));
				pAGState->SetInput( m_idAngleZInput, anglez);
		
				if(m_jumpParams.bUseSpecialAnim && m_jumpParams.specialAnimType == JUMP_ANIM_FLY)
					pAGState->SetInput( m_idActionInput, m_jumpParams.specialAnimAGInputValue );
				else
					pAGState->SetInput( m_idActionInput, "fly" );
			}


		}

		if(m_jumpParams.state == JS_JumpStart )
		{
			if((!m_jumpParams.bUseStartAnim && !m_jumpParams.bUseAnimEvent) || 
				(currTime - m_jumpParams.startTime).GetSeconds() > 0.9f)
			{
				Jump();
			}
		}
		else if(m_jumpParams.state == JS_ApplyImpulse)
		{
			// actual impulse will be applied now
			if(phys)
			{
				pe_player_dynamics simParSet;
				simParSet.bSwimming = true;
				phys->SetParams(&simParSet);
			}
			m_moveRequest.type = eCMT_JumpInstant;

			pe_status_dynamics	dyn;
			if( !m_jumpParams.velocity.IsZero() && phys && phys->GetStatus(&dyn))
			{
				Vec3 vel(m_jumpParams.bRelative ? m_jumpParams.velocity : m_jumpParams.velocity - dyn.v);
				move = (vel+m_jumpParams.addVelocity);
				m_jumpParams.state = JS_Flying;//JS_JumpStart;
				JumpEffect();
			}
			else
				m_jumpParams.state = JS_None;

			m_jumpParams.velocity = ZERO;
			m_jumpParams.startTime= currTime;
		}

		if(m_stats.inAir <=0)
			m_lastTimeOnGround = currTime;
		
		if(m_stats.inAir >0.3f && phys)
		{
			pe_player_dynamics simParSet;
			simParSet.bSwimming = false;
				phys->SetParams(&simParSet);
		}

		if(m_stats.inAir > 0.0f && !InZeroG() && !GetEntity()->GetParent() && m_jumpParams.state != JS_ApproachLanding && m_jumpParams.state != JS_Landing)
		{
			m_jumpParams.curVelocity = m_stats.velocity;
			//check free fall
			if((currTime - m_lastTimeOnGround).GetSeconds()>0.1f)
			{

				if( m_jumpParams.state == JS_None || /*m_jumpParams.state ==JS_Landing ||*/ m_jumpParams.state ==JS_Landed)
				{
					//IAnimationGraphState* pAGState = GetAnimationGraphState();
					bool bUseSpecialFlyAnim = (m_bOverrideFlyActionAnim || m_jumpParams.bUseSpecialAnim && m_jumpParams.specialAnimType == JUMP_ANIM_FLY);
					if ( pAGState && !bUseSpecialFlyAnim)
					{
						pAGState->SetInput( m_idActionInput, m_bOverrideFlyActionAnim ? m_overrideFlyAction : "flyNoStart" );
					}
					m_jumpParams.state = JS_Flying;
					if(!bUseSpecialFlyAnim)
						m_jumpParams.bUseLandAnim = true; 
					m_jumpParams.bFreeFall= true;
				}

				//IPhysicalEntity *phys = GetEntity()->GetPhysics();
				if(phys)
				{
					if(m_jumpParams.bUseLandAnim)
					{
						// computing remaining time to land
						float t=-1;
						Vec3 vN(m_stats.velocity/(m_stats.speed>0 ? m_stats.speed : 1));
						if(!m_jumpParams.bFreeFall )//&& t > 0.4f) // avoid raycast when expected left fly time is high enough
							t = m_jumpParams.duration - (currTime - m_jumpParams.startTime).GetSeconds();
						else
						{

							if(vN.z < -0.05f) // going down
							{
								ray_hit hit;
								int rayFlags = rwi_stop_at_pierceable|(geom_colltype_player<<rwi_colltype_bit);
								Vec3 pos(GetEntity()->GetWorldPos());
								if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, (vN.z<0 ? vN : -Vec3Constants<float>::fVec3_OneZ)*20, ent_terrain|ent_static|ent_rigid, rayFlags, &hit, 1, &phys, 1))
								{

									// find approximate time of landing with given velocity
									Vec3 dist(pos - hit.pt); //Distance::Point_Point(pos,hit.pt);
									// use current actual gravity	of the object
									pe_player_dynamics	dyn;
									if(phys->GetParams(&dyn))
									{
										float a = dyn.gravity.z/2;
										float b = m_stats.velocity.z;
										float c = dist.z;
										float d = b*b - 4*a*c;
										if(d>=0)
										{
											float sqrtd = sqrt(d);
											t = (-b + sqrtd)/ (2*a);
											float t1 = (-b - sqrtd)/ (2*a);
											if(t <0 || t1>=0 && t1<t)
												t = t1;
										}
									}
								}
							}
						}
						if(t>=0 && t<2*frameTime)
						{
							m_jumpParams.state = JS_Landing;
							m_jumpParams.startTime = currTime;
							m_jumpParams.initLandVelocity = m_jumpParams.curVelocity;
							m_jumpParams.landDepth = m_jumpParams.curVelocity.GetNormalizedSafe().z;

						}
						if(t>=0 && t< m_jumpParams.landPreparationTime && !m_bOverrideFlyActionAnim)
						{
							//IAnimationGraphState* pAGState = GetAnimationGraphState();
							if ( pAGState )
							{
								Vec3 vNx(vN);
								vNx.z=0;
								vNx.NormalizeSafe();
								float dot = vN.Dot(vNx);
								float anglex = RAD2DEG(cry_acosf(CLAMP(dot,-1.f,1.f)));
								if(m_viewMtx.GetColumn1().Dot(vN) <-0.001) // land backwards
									anglex = 180 - anglex;

								pAGState->SetInput( m_idAngleXInput, anglex);

								Vec3 viewDir(m_viewMtx.GetColumn(1));
								dot = vNx.Dot(viewDir);
								float anglez = RAD2DEG(cry_acosf(CLAMP(dot,-1.f,1.f))*sgn(vNx.Cross(viewDir).z));
								pAGState->SetInput( m_idAngleZInput, anglez);

						 		if(m_jumpParams.bUseSpecialAnim && m_jumpParams.specialAnimType == JUMP_ANIM_LAND)
								{
									if(m_jumpParams.specialAnimAGInput == AIANIM_ACTION)
										pAGState->SetInput( m_idActionInput, m_jumpParams.specialAnimAGInputValue );
									else
									{
										pAGState->SetInput( m_idActionInput, "idle" );
										pAGState->SetInput( m_idSignalInput, m_jumpParams.specialAnimAGInputValue );
									}
									m_jumpParams.bPlayingSpecialAnim = true;
								}
								else
									pAGState->SetInput( m_idActionInput, "idle" );

							}

							//m_bOverrideFlyActionAnim = false;
							m_jumpParams.bFreeFall= false;
							m_jumpParams.bUseLandAnim = false;
							//if(m_jumpParams.state !=JS_Landing)
								m_jumpParams.state = JS_ApproachLanding;
						}
					}
				}
			}
		}

		//if (m_stats.inAir == 0 && !InZeroG() && (m_jumpParams.state == JS_Flying || m_jumpParams.state==JS_ApproachLanding ))
		if (m_stats.inAir == 0 && m_jumpParams.prevInAir > 0.3f)
		{
			m_jumpParams.bFreeFall= false;
			//m_bOverrideFlyActionAnim = false;
			IAISignalExtraData* pData=NULL;
			if(m_jumpParams.bUseSpecialAnim && m_jumpParams.specialAnimType == JUMP_ANIM_LAND 
					&& !m_jumpParams.bPlayingSpecialAnim)
			{
				// something went wrong, trooper landed before playing the special land animation he was supposed to do
				pData = gEnv->pAISystem->CreateSignalExtraData();
				pData->iValue = 1;
			}
			// send land event/signal			
			gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER,1,"OnLand",GetEntity()->GetAI(),pData);
			if(m_jumpParams.bUseLandEvent)
			{
				SEntityEvent event( ENTITY_EVENT_SCRIPT_EVENT );
				event.nParam[0] = (INT_PTR)"Land";
				event.nParam[1] = IEntityClass::EVT_BOOL;
				bool bValue = true;
				event.nParam[2] = (INT_PTR)&bValue;
				GetEntity()->SendEvent( event );
			}
			m_jumpParams.bUseLandEvent = false;

			if(!(m_jumpParams.bUseSpecialAnim && m_jumpParams.specialAnimAGInput == AIANIM_ACTION && m_jumpParams.specialAnimType == JUMP_ANIM_LAND))
			{
				IAnimationGraphState* pAGState = GetAnimationGraphState();
				if ( pAGState && !m_bOverrideFlyActionAnim )
				{
					pAGState->SetInput( m_idActionInput, "idle" );
				}
				m_jumpParams.state = JS_Landing;
				m_jumpParams.startTime = currTime;
				m_jumpParams.initLandVelocity = m_jumpParams.curVelocity;
				m_jumpParams.landDepth = m_jumpParams.curVelocity.GetNormalizedSafe().z;
			}
			else
				m_jumpParams.state = JS_None;

			m_jumpParams.bUseSpecialAnim = false;

			if(phys)
			{
				pe_player_dynamics simParSet;
				simParSet.bSwimming = false;
				phys->SetParams(&simParSet);
			}
      JumpEffect();
		}	

	}
	else // exact positioning
	{
		m_jumpParams.state = JS_None;
		m_jumpParams.bFreeFall = false;
		m_jumpParams.bUseLandAnim = false;
		m_bOverrideFlyActionAnim = false;
		m_overrideFlyAction = "idle";

		if(phys)
		{
			pe_player_dynamics simParSet;
			simParSet.bSwimming = false;
			phys->SetParams(&simParSet);
		}
	}

	m_stats.desiredSpeed = m_stats.speed;
	m_moveRequest.velocity = move;
	m_velocity.zero();
	m_jumpParams.prevInAir = m_stats.inAir;

}

void CTrooper::ProcessAnimation(ICharacterInstance *pCharacter,float frameTime)
{

	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();
	if (pPhysEnt)
	{
		if(m_bExactPositioning || m_stats.inAir>0)
			m_heightVariance = 0;
		else
		{
			float curTime = gEnv->pSystem->GetITimer()->GetFrameStartTime().GetSeconds();
			float speedCoeff = m_stats.speed/10;
			if(speedCoeff>1)
				speedCoeff = 1;
			else if(speedCoeff<0.1f)
				speedCoeff = 0;
			float range = (m_heightVarianceHigh - m_heightVarianceLow);
			m_heightVariance = speedCoeff*(m_heightVarianceLow + range/2 + sin((curTime + m_heightVarianceRandomize) * m_heightVarianceFreq * 2*gf_PI) * range);
		}

		pe_player_dimensions params;
		if(pPhysEnt->GetParams(&params))
		{
			Interpolate(params.heightPivot,m_heightVariance,3.f,frameTime);
			pPhysEnt->SetParams(&params);
		}

		if(!m_bExactPositioning)
		{
			IAnimationGraphState* pAGState = GetAnimationGraphState();
			if(pAGState)
			{
				if(!m_bOverrideFlyActionAnim && m_overrideFlyAction != "idle")
				{
					pAGState->SetInput( m_idActionInput, m_overrideFlyAction);
					m_bOverrideFlyActionAnim = true;
				}
				else if(m_bOverrideFlyActionAnim && m_overrideFlyAction == "idle")
				{
					pAGState->SetInput( m_idActionInput, "idle");
					m_bOverrideFlyActionAnim = false;
				}
			}
		}

		if(m_jumpParams.state == JS_Landing)
		{
			float landTime = (gEnv->pSystem->GetITimer()->GetFrameStartTime() -m_jumpParams.startTime).GetSeconds();
			if(landTime>= ClandDuration)
			{
				m_landModelOffset = ZERO;
				//m_stats.dynModelOffset = ZERO;
				m_jumpParams.state = JS_None;
			}
			else
			{
				float timeToZero = ClandDuration/2;
				float frameTime = gEnv->pSystem->GetITimer()->GetFrameTime();
				if(m_jumpParams.curVelocity.z< -0.01f) // going down
				{
					m_jumpParams.curVelocity -= m_jumpParams.initLandVelocity * frameTime * (ClandDuration - timeToZero) * ClandStiffnessMultiplier;
					//Interpolate(m_jumpParams.curVelocity,ZERO,1/(m_landDuration - timeToZero),gEnv->pSystem->GetITimer()->GetFrameTime());
/*					char b[1000];
					sprintf(b,"%f\n",m_jumpParams.curVelocity.z);
					OutputDebugString(b);
*/
					m_landModelOffset+= m_baseMtx.GetInverted()*m_jumpParams.curVelocity*frameTime;
				}
				else
				{
					Interpolate(m_landModelOffset,ZERO,2/timeToZero,frameTime);
				}
			}
		}
	}

	// simulating inertia when changing speed/direction
	if(m_steerInertia>0)
	{
		Vec3 goalSteelModelOffset(ZERO);
		float dot = 0;
		float interpolateSpeed;
		if(m_stats.inAir<=0 && !m_bExactPositioning && !m_stats.isGrabbed)
		{
			Vec3 desiredMovement(m_input.movementVector);
			float deslength = desiredMovement.GetLength();
			float maxSpeed = GetStanceInfo( m_stance )->maxSpeed;
			if(deslength > 0.2f && maxSpeed>0)
			{
				desiredMovement /= deslength;
				Vec3 curMoveDir(m_stats.velocity/maxSpeed);
				float curSpeed = curMoveDir.GetLength();
				if(curSpeed>0)
					curMoveDir /= curSpeed;
				dot = desiredMovement.Dot(curMoveDir);
				if(dot<0.9f)
				{
					float acc = m_stats.speed * (1 - dot)/2;
					goalSteelModelOffset = acc * m_steerInertia * m_stats.velocity.GetNormalizedSafe();
					goalSteelModelOffset = m_baseMtx.GetInverted() * goalSteelModelOffset;
				}
			}
			Interpolate(m_oldVelocity,m_stats.velocity,2.0f + max(dot,0.f),frameTime);
			goalSteelModelOffset = m_baseMtx.GetInverted() * (m_oldVelocity - m_stats.velocity  ) * m_steerInertia;
			/* debug
			Vec3 pos(GetEntity()->GetWorldPos());
			pos.z+=1;
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(128,255,128,255), pos + desiredMovement, ColorB(128,255,128,255), 1.0f);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,128,128,255), pos + m_stats.velocity, ColorB(255,128,128,255), 1.0f);
			*/
			interpolateSpeed = 2.0f+max(dot,0.f);
		}
		else if(m_bExactPositioning)
			interpolateSpeed = 6.0f;
		else if(m_stats.isGrabbed)
			interpolateSpeed = 6.0f;
		else
			interpolateSpeed = 4.0f;
		
		Interpolate(m_steerModelOffset,goalSteelModelOffset,interpolateSpeed,frameTime);
		
	}

	m_modelOffsetAdd = m_landModelOffset + m_steerModelOffset;

	//Beni - Disable look IK while the trooper is grabbed (special "state")
	if (pCharacter)
	{
		if(m_stats.isGrabbed)
			pCharacter->GetISkeletonPose()->SetLookIK(false,0,Vec3(0,0,0));
		else
			pCharacter->GetISkeletonPose()->SetLookIK(true,gf_PI*0.9f,m_stats.lookTargetSmooth);//,m_customLookIKBlends);
	}

	//m_oldSpeed = speed;

}

//---------------------------------
//AI Specific
void CTrooper::SetActorMovement(SMovementRequestParams &control)
{
	if (IsClient())
		return;

	SMovementState state;
	GetMovementController()->GetMovementState(state);

	CAlien::SetActorMovementCommon(control);
	Vec3 mypos(GetEntity()->GetWorldPos());

	const SAnimationTarget * pAnimTarget = GetAnimationGraphState()->GetAnimationTarget();
	if ((pAnimTarget != NULL) && pAnimTarget->preparing)
	{
		
		float offset = 3.0f;
		Vec3 bodyTarget = pAnimTarget->position + offset * (pAnimTarget->orientation * FORWARD_DIRECTION);// + Vec3(0, 0, 1.5);
		Vec3 bodyDir(bodyTarget - mypos);
		bodyDir.z=0;
		bodyDir = bodyDir.GetNormalizedSafe(pAnimTarget->orientation * FORWARD_DIRECTION);
		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(mypos, ColorB(255,128,128,255), mypos + bodyDir * 10.0f, ColorB(255,255,255,255), 5.0f);
		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(bodyTarget, 0.2f, ColorB(255, 255, 255, 255), true);
		SetDesiredDirection(bodyDir);
		/*
		float dist = Distance::Point_Point(state.weaponPosition,pAnimTarget->position);
		float coeff = pAnimTarget->maxRadius - dist;
		if(coeff<0)
			coeff = 0;
		coeff *= 5*coeff;
		Vec3 targetPos = pAnimTarget->position + coeff * (pAnimTarget->orientation * FORWARD_DIRECTION) + Vec3(0, 0, 1.5);
		SetDesiredDirection((targetPos - state.weaponPosition).GetNormalizedSafe());
		*/

		pAnimTarget->notAiControlledAnymore = true;
	}
	else if(!control.vAimTargetPos.IsZero())
		SetDesiredDirection((control.vAimTargetPos - state.weaponPosition).GetNormalizedSafe());
	else if(!control.vLookTargetPos.IsZero())
		SetDesiredDirection((control.vLookTargetPos - state.eyePosition).GetNormalizedSafe());
	else if(!control.vMoveDir.IsZero() && control.fDesiredSpeed > 0 && (!pAnimTarget || !pAnimTarget->notAiControlledAnymore))
		SetDesiredDirection(control.vMoveDir);
	else
		SetDesiredDirection(GetEntity()->GetWorldRotation() * FORWARD_DIRECTION);

	SetDesiredSpeed(control.vMoveDir*control.fDesiredSpeed);

	//	m_input.actions = control.m_desiredActions;
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
	//SetActorStance(control, actions);

	//	SetTilt(control,actions);

	m_input.actions = actions;

	Vec3	fireDir = GetEntity()->GetWorldRotation() * FORWARD_DIRECTION;
	if(!control.vAimTargetPos.IsZero())
	{
		fireDir = control.vAimTargetPos - state.weaponPosition;
		fireDir.NormalizeSafe();
	}
	if(IScriptTable* pScriptTable = GetEntity()->GetScriptTable())
		pScriptTable->SetValue( "fireDir", fireDir );

	m_fDistanceToPathEnd = control.fDistanceToPathEnd;
	m_bExactPositioning = pAnimTarget && pAnimTarget->notAiControlledAnymore;
	

}

void CTrooper::SetActorStance(SMovementRequestParams &control, int& actions)
{
	IPuppet * pPuppet;
	if(GetEntity() && GetEntity()->GetAI() && (pPuppet = GetEntity()->GetAI()->CastToIPuppet()))
	{
		float distance = control.fDistanceToPathEnd;
		if(m_stance == STANCE_PRONE)
		{
			IAIActor *pAIActor = CastToIAIActorSafe(GetEntity()->GetAI());
			if(pAIActor)
			{
				SOBJECTSTATE *pAIState(pAIActor->GetState());
				if(pAIState && (pAIState->allowStrafing ||distance < g_pGame->GetCVars()->g_trooperProneMinDistance))
				{
					pAIState->bodystate = BODYPOS_RELAX;
					actions = ACTION_RELAXED;
				}
			}
		}
	}
}
/*
bool CTrooper::UpdateStance()
{
	if (m_stance != GetStance() || m_forceUpdateStanceCollider)
	{
		EStance oldStance = m_stance;
		m_stance = GetStance();
		StanceChanged( oldStance );

		IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();
		if (pPhysEnt)
		{
			pe_player_dimensions playerDim;
			const SStanceInfo *sInfo = GetStanceInfo(m_stance);

			playerDim.heightEye = 0.0f;
			playerDim.heightCollider = sInfo->heightCollider;
			playerDim.sizeCollider = sInfo->size;
//			playerDim.heightPivot = m_bExactPositioning ? 0 : m_heightVariance;
			playerDim.maxUnproj = max(0.0f,sInfo->heightPivot);
			playerDim.bUseCapsule = sInfo->useCapsule;

			int result(pPhysEnt->SetParams(&playerDim));

			pe_action_awake aa;
			aa.bAwake = 1;
			pPhysEnt->Action(&aa);
		}
	}
	return true;
}
*/


void CTrooper::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	GetAlienMemoryStatistics(s);
}

void CTrooper::Jump()
{
	if(m_jumpParams.velocity.IsZero() )
		return;

	IPhysicalEntity *phys = GetEntity()->GetPhysics();
	m_jumpParams.state = phys ? JS_ApplyImpulse : JS_None;

}


void CTrooper::JumpEffect()
{   
  IMaterialEffects* pMaterialEffects = g_pGame->GetIGameFramework()->GetIMaterialEffects();
  TMFXEffectId effectId = pMaterialEffects->GetEffectId("trooper_jump", m_stats.groundMaterialIdx);
  if (effectId != InvalidEffectId)
  {
    SMFXRunTimeEffectParams fxparams;
    fxparams.pos = GetEntity()->GetWorldPos();   
		fxparams.soundSemantic = eSoundSemantic_Physics_Footstep;
    pMaterialEffects->ExecuteEffect(effectId, fxparams);
  }
}



void CTrooper::AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event)
{
	if(stricmp(event.m_EventName, "jump") == 0 && m_jumpParams.state != JS_Flying) 
		Jump();
	//else
	CAlien::AnimationEvent (pCharacter,event);
}


void CTrooper::ResetAnimations()
{
	ICharacterInstance *character = GetEntity()->GetCharacter(0);

	if (character)
	{
		if (m_pAnimatedCharacter)
		{
			m_pAnimatedCharacter->ClearForcedStates();
			//m_pAnimatedCharacter->GetAnimationGraphState()->Pause(true, eAGP_StartGame);
		}

		character->GetISkeletonAnim()->StopAnimationsAllLayers();
		character->GetISkeletonPose()->SetLookIK(false,gf_PI*0.9f,m_stats.lookTargetSmooth);//,m_customLookIKBlends);
	}
}

void CTrooper::BindInputs( IAnimationGraphState * pAGState )
{
	CAlien::BindInputs(pAGState);
	if (pAGState)
	{
		m_idAngleXInput = pAGState->GetInputId( "AngleX" );
		m_idAngleZInput = pAGState->GetInputId( "AngleZ" );
		m_idActionInput = pAGState->GetInputId( "Action" );
		m_idSignalInput = pAGState->GetInputId( "Signal" );
		m_idMovementInput = pAGState->GetInputId( "MovementDir" );
	}
}


void CTrooper::FullSerialize( TSerialize ser )
{
	CAlien::FullSerialize(ser);
	ser.Value("m_modelQuat",m_modelQuat);
	ser.Value("m_lastNotMovingTime",m_lastNotMovingTime);
	ser.Value("m_oldSpeed",m_oldSpeed);
	ser.Value("m_heightVariance",m_heightVariance);
	ser.Value("m_fDistanceToPathEnd",m_fDistanceToPathEnd);
	ser.Value("m_Roll",m_Roll);
	ser.Value("m_Rollx",m_Rollx);
	ser.Value("m_bExactPositioning",m_bExactPositioning);
	ser.Value("m_lastExactPositioningTime",m_lastExactPositioningTime);
	ser.Value("m_lastTimeOnGround",m_lastTimeOnGround);
	ser.Value("m_overrideFlyAction",m_overrideFlyAction);
	ser.Value("m_bOverrideFlyActionAnim",m_bOverrideFlyActionAnim);
	ser.Value("m_heightVarianceLow",m_heightVarianceLow);
	ser.Value("m_heightVarianceHigh",m_heightVarianceHigh);
	ser.Value("m_heightVarianceFreq",m_heightVarianceFreq);
	ser.Value("m_heightVarianceRandomize",m_heightVarianceRandomize);
	ser.Value("m_oldDirStrafe",m_oldDirStrafe);
	ser.Value("m_oldDirFwd",m_oldDirFwd);
	ser.Value("m_steerInertia",m_steerInertia);
	ser.Value("m_landModelOffset",m_landModelOffset);
	ser.Value("m_steerModelOffset",m_steerModelOffset);
	ser.Value("m_oldVelocity",m_oldVelocity);
	ser.Value("m_fTtentacleBlendRotation",m_fTtentacleBlendRotation);
	m_jumpParams.Serialize(ser);
}

