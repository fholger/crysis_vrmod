/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 6:12:2004: Created by Filippo De Luca

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include "Alien.h"
#include "GameUtils.h"
#include "GameActions.h"
#include "IDebrisMgr.h"

#include <IViewSystem.h>
#include <IItemSystem.h>
#include <IPhysics.h>
#include <ICryAnimation.h>
#include <ISerialize.h>
#include <IRenderAuxGeom.h>
#include <IEffectSystem.h>
#include <ISound.h>

#include <IDebugHistory.h>

#include "CompatibilityAlienMovementController.h"

// ----------------------------------------------------------------------

CDebrisSpawner::CDebrisSpawner() : m_pAlien (0)
{
}

CDebrisSpawner::~CDebrisSpawner()
{
	Reset();
}

/**
 * This function does anything that needs to be done to arrive from Alien*
 * (and a Lua table, maybe?) to a nice vector of IEntities, each of which
 * represents an individual piece of debris ready to be spawned.
 */
bool CDebrisSpawner::Init(CAlien* alien /*, const SmartScriptTable & table*/)
{
	m_pAlien = alien;

	// NOTE Mai 25, 2007: <pvl> if we're loading from a file the debris parts
	// will be read and filled in by Serialize().
	if (GetISystem()->IsSerializingFile())
		return true;

	for (int i=0; i<6; ++i) {

		char name [64];
		_snprintf (name, 64, "%s-debris-%.2d", alien->GetEntity()->GetName(), i);
		name[63] = 0;

		SEntitySpawnParams spawnParams;
		spawnParams.sName = name;
		spawnParams.nFlags = ENTITY_FLAG_CLIENT_ONLY;
		spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
		if (!spawnParams.pClass)
			return false;

		IEntity* pSpawnedDebris = gEnv->pEntitySystem->SpawnEntity(spawnParams, true);

		if (!pSpawnedDebris)
			return false;

		pSpawnedDebris->LoadGeometry(-1, "Objects/Characters/Alien/Scout/spike05x.cgf");

		m_debrisParts.push_back(pSpawnedDebris->GetId());
	}

	return true;
}

void CDebrisSpawner::Reset()
{ 
	if (GetISystem()->IsSerializingFile()) //gameserialize will do that 
		return;

  // clean up own pieces
  if (IDebrisMgr* pDebrisMgr = g_pGame->GetIGameFramework()->GetDebrisMgr())
  {
    for (std::vector<EntityId>::const_iterator it=m_debrisParts.begin(),end=m_debrisParts.end(); it!=end; ++it)  
      pDebrisMgr->RemovePiece(*it);
  }
    	
	m_debrisParts.erase (m_debrisParts.begin(), m_debrisParts.end());
}

void CDebrisSpawner::Serialize(TSerialize ser)
{
	ser.BeginGroup ("CDebrisSpawner");
	int numDebrisParts;
	if (ser.IsWriting())
		numDebrisParts = m_debrisParts.size();

	ser.Value("numDebrisParts", numDebrisParts);

	if (ser.IsReading())
	{
		m_debrisParts.resize (numDebrisParts);
	}

	ser.BeginGroup ("DebrisPartEntityIds");
	for (int i=0; i < numDebrisParts; ++i)
	{
		char label [64];
		_snprintf (label, 64, "debris-part-%d", i);
		ser.Value (label, m_debrisParts[i]);
	}
	ser.EndGroup ();
	ser.EndGroup ();
}

// NOTE Jan 29, 2007: <pvl> not used ATM.
void CDebrisSpawner::Update(const float deltaTime)
{
}

/**
 * Basically does the following for every piece of debris (prepared by Init()):
 *
 * - translates the piece to place where the Alien died
 * - turns physics on for the piece and gives it an impulse
 * - add the piece to DebrisMgr to manage its lifespan
 */
void CDebrisSpawner::OnKillEvent()
{
	if(gEnv->pSystem->IsSerializingFile())
		return;

	const Matrix34 & alienTM = m_pAlien->GetEntity()->GetWorldTM();

	std::vector <EntityId>::const_iterator it = m_debrisParts.begin ();
	std::vector <EntityId>::const_iterator end = m_debrisParts.end ();

	Vec3 dir (0,0,-1);

	for ( ; it != end; ++it) {

		int i = it - m_debrisParts.begin();

		IEntity * pSpawnedDebris = gEnv->pEntitySystem->GetEntity(*it);
    if (!pSpawnedDebris)
      continue;

		Matrix34 loc;
		loc.SetTranslationMat(Vec3 (0, 0, 2.0f + i));
		// FIXME Jan 22, 2007: <pvl> account for debris part's local xform!
		pSpawnedDebris->SetWorldTM(loc * alienTM);

		// run physics on the debris part
		SEntityPhysicalizeParams physicsParams;
		physicsParams.mass = 200.0f;
		physicsParams.type = PE_RIGID;
		physicsParams.nFlagsOR &= pef_log_collisions;
		physicsParams.nSlot = 0;
		pSpawnedDebris->Physicalize(physicsParams);

		// give it an impulse
		pe_action_impulse imp;

		if (i % 2) {
			dir = Vec3::CreateReflection (dir, Vec3 (0,0,1.0f));
		} else {
			dir = Vec3 (0,0,-1);
			while (dir.z < 0.2f)
				dir.SetRandomDirection ();
		}

		imp.impulse = dir;
		imp.impulse *= 2000.0f;

		imp.angImpulse = Vec3(0.0f, 0.0f, 1.0f);
		imp.angImpulse *= 10000.0f;

		IPhysicalEntity* pDebrisPhys = pSpawnedDebris->GetPhysics();
		if (pDebrisPhys)
			pDebrisPhys->Action(&imp);

		IDebrisMgr * debrisMgr = g_pGame->GetIGameFramework()->GetDebrisMgr();
		if (debrisMgr)
			debrisMgr->AddPiece (pSpawnedDebris);
	}
}

void CDebrisSpawner::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
}

// ----------------------------------------------------------------------

//--------------------
void CAlienBeam::Start(const char *effect,EntityId targetId,Ang3 rotOffset,const char *attachToBone)
{
	//if (m_active)
	Stop();

	IEntity *pTarget = gEnv->pEntitySystem->GetEntity(targetId);
	if (pTarget)
	{
		AABB bbox;
		pTarget->GetLocalBounds(bbox);
		m_lCenter = (bbox.max + bbox.min) * 0.5f;

		m_followBoneID = -1;
		if (attachToBone && pTarget->GetCharacter(0))
			m_followBoneID = pTarget->GetCharacter(0)->GetISkeletonPose()->GetJointIDByName(attachToBone);
	}
	else
		return;

	IParticleEffect *pEffect = gEnv->p3DEngine->FindParticleEffect(effect);
	if (pEffect)
	{
		m_active = true;
		m_beamTargetId = targetId;
		
		SpawnParams params;
		params.fPulsePeriod = 0.0f;
		params.fSizeScale = 0.5f;
		params.fCountScale = 1.0f;
		params.bCountPerUnit = 0;

		m_effectSlot = m_pAlien->GetEntity()->LoadParticleEmitter( -1, pEffect, &params, false );
		m_pAlien->GetEntity()->SetSlotLocalTM(m_effectSlot,Matrix33::CreateRotationZ(rotOffset.z)*Matrix33::CreateRotationX(rotOffset.x));
	}	
}

void CAlienBeam::Stop()
{
	IParticleEmitter *pEmitter = m_pAlien->GetEntity()->GetParticleEmitter(m_effectSlot);
	if (pEmitter)
		gEnv->p3DEngine->DeleteParticleEmitter(pEmitter);

	m_effectSlot = 0;
	m_beamTargetId = 0;
	m_active = false;
}

void CAlienBeam::Update(float frameTime)
{
	if (!m_active)
		return;

	IParticleEmitter *pEmitter = m_pAlien->GetEntity()->GetParticleEmitter(m_effectSlot);
	if (pEmitter)
	{
		IEntity *pTarget = gEnv->pEntitySystem->GetEntity(m_beamTargetId);
		if (pTarget)
		{
			ParticleTarget targetOptions;	
			targetOptions.bTarget = true;
			targetOptions.bExtendCount = true;
			targetOptions.bExtendLife = false;
			targetOptions.bExtendSpeed = true;
			targetOptions.bPriority = true;

			//follow the bone if necessary
			if (m_followBoneID>-1 && pTarget->GetCharacter(0))
					m_lCenter = pTarget->GetCharacter(0)->GetISkeletonPose()->GetAbsJointByID(m_followBoneID).t;

			targetOptions.vTarget = pTarget->GetWorldTM() * m_lCenter;

			pEmitter->SetTarget(targetOptions);
		}
	}
}

//--------------------
//this function will be called from the engine at the right time, since bones editing must be placed at the right time.
int AlienProcessBones(ICharacterInstance *pCharacter,void *pAlien)
{
	//FIXME: do something to remove gEnv->pTimer->GetFrameTime()
	//process bones specific stuff (IK, torso rotation, etc)
	((CAlien *)pAlien)->ProcessBonesRotation(pCharacter,gEnv->pTimer->GetFrameTime());

	return 1;
}

int AlienPostPhysicsSkeletonCallbk (ICharacterInstance *pCharacter, void *pAlien)
{
	((CAlien * )pAlien)->UpdateGrab (gEnv->pTimer->GetFrameTime ());

	return 1;
}

//--------------------

/**
 * Tries to initialize as much of SMovementRequestParams' data
 * as possible from a CMovementRequest instance.
 */
CAlien::SMovementRequestParams::SMovementRequestParams(CMovementRequest& request) :
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

// -------------------

CAlien::CAlien() : 
	m_pItemSystem(0),
	m_weaponOffset(ZERO),
	m_eyeOffset(ZERO),
	m_curSpeed(0),
	m_forceOrient(false),
	m_endOfThePathTime(-1.0f),
	m_roll(0),
	m_pGroundEffect(NULL),
  m_pTrailAttachment(NULL),
	m_pHealthTrailAttachment(NULL),
	m_pBeamEffect(NULL),
	m_pTurnSound(NULL),
	m_oldGravity(0,0,-9.81f),
  m_trailSpeedScale(0.f),
	m_healthTrailScale(0.f),
	m_followEyesTime(0.f),
	m_pDebugHistoryManager(0)
{	
	m_tentaclesProxy.clear();
	m_tentaclesProxyFullAnimation.clear();
	memset(&m_moveRequest.prediction,0,sizeof(m_moveRequest.prediction));
	m_desiredVeloctyQuat.SetIdentity();
}

CAlien::~CAlien()
{
	m_tentaclesProxy.clear();
	m_tentaclesProxyFullAnimation.clear();

	GetGameObject()->ReleaseActions( this );

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	if(pCharacter)
	{
		pCharacter->GetISkeletonPose()->SetPostProcessCallback0(0,0);
		pCharacter->GetISkeletonPose()->SetPostPhysicsCallback(0,0);
	}

	SAFE_DELETE(m_pGroundEffect);
  	
	SAFE_DELETE(m_pBeamEffect);
	SAFE_DELETE(m_pDebugHistoryManager);
}

void CAlien::BindInputs( IAnimationGraphState * pAGState )
{
	CActor::BindInputs(pAGState);

	if (pAGState)
	{
		m_inputSpeed = pAGState->GetInputId("Speed");
		m_inputDesiredSpeed = pAGState->GetInputId("DesiredSpeed");
		m_inputAiming = pAGState->GetInputId("Aiming");
	}
}

void CAlien::ProcessEvent(SEntityEvent& event)
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

bool CAlien::CreateCodeEvent(SmartScriptTable &rTable)
{
	/*const char *event = NULL;
	rTable->GetValue("event",event);

	if (event && !strcmp(event,"beamStart"))
	{
		const char *effect = NULL;
		ScriptHandle id;
		id.n = 0;

		if (rTable->GetValue("effect",effect) && rTable->GetValue("targetId",id))
			m_pBeamEffect->Start(GetEntity(),effect,id.n);
	}
	else if (event && !strcmp(event,"beamStop"))
	{
		m_pBeamEffect->Stop(GetEntity());
	}
	else*/
		return CActor::CreateCodeEvent(rTable);
}

bool CAlien::Init( IGameObject * pGameObject )
{
	if (!CActor::Init(pGameObject))
		return false;

	if (!pGameObject->CaptureActions( this ))
		return false;

	m_pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();

	Revive();

	return true;
}

//------------------------------------------------------------------------
void CAlien::Physicalize(EStance stance)
{
	// make sure alien does not have NULL stance - default (null) stance is too different from all alien stances, possibly can stuck
	if( m_stance==STANCE_NULL )
		m_stance = STANCE_STAND;
	CActor::Physicalize( m_stance );
}

void CAlien::PostPhysicalize()
{
	CActor::PostPhysicalize();

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);

	if (!pCharacter)
		return;

	m_tentaclesProxy.clear();
	m_tentaclesProxyFullAnimation.clear();

	pCharacter->GetISkeletonPose()->SetPostProcessCallback0(AlienProcessBones,this);
	pCharacter->GetISkeletonPose()->SetPostPhysicsCallback(AlienPostPhysicsSkeletonCallbk,this);
	pCharacter->EnableStartAnimation(true);

	//collect all the tentacle proxies from the attachments
  IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();
	uint32 numAttachmnets = pIAttachmentManager ? pIAttachmentManager->GetAttachmentCount() : 0;

  for (uint32 i=0; i<numAttachmnets; ++i) 
  {
		IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByIndex(i);			
    IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();

    if (pIAttachmentObject) 
    {
			ICharacterInstance* pITentacleInstance = pIAttachmentObject->GetICharacterInstance();
			PushCharacterTentacles(pITentacleInstance);
		}
	}

	//and from the main character
	PushCharacterTentacles(pCharacter);

	//CryLogAlways("%s has %i tentacles",m_pEntity->GetName(),m_tentaclesProxy.size());

	//set tentacles params
	pe_params_flags pf;
	pf.flagsOR = rope_findiff_attached_vel | rope_no_solver | pef_traceable;
	if (m_params.tentaclesCollide)
		pf.flagsOR |= rope_ignore_attachments | rope_collides_with_terrain | rope_collides;

	pe_params_rope pRope;							
	pRope.bTargetPoseActive = 1;
	pRope.stiffnessAnim = 1.0f;
	pRope.stiffnessDecayAnim = 0.0f;
	pRope.collDist = m_params.tentaclesRadius;
	pRope.surface_idx = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeIdByName( m_params.tentaclesMaterial,"alien tentacles" );
	
	pe_simulation_params	sim;
	sim.gravity.Set(0,0,0);

	float jlim = g_pGameCVars->g_tentacle_joint_limit;
	if (jlim>=0)
		pRope.jointLimit = DEG2RAD(jlim);
	else
		pRope.jointLimit = DEG2RAD(m_params.tentaclesJointLimit);

	std::vector<IPhysicalEntity *>::iterator it;
	for (it = m_tentaclesProxy.begin(); it != m_tentaclesProxy.end(); it++)
	{
		IPhysicalEntity *pT = *it;
		if (pT)
		{
			pT->SetParams(&pRope);
 			pT->SetParams(&pf);
			pT->SetParams(&sim);
		}
	}

	//FIXME:this disable the impulse, remove it
	IPhysicalEntity *pPhysEnt = pCharacter->GetISkeletonPose()->GetCharacterPhysics(-1);

	/*if (pPhysEnt)
	{
		pe_params_flags pFlags;
		pFlags.flagsOR = pef_monitor_impulses;
		//pPhysEnt->SetParams(&pFlags);
	}*/

	//set a default offset for the character, so in the editor the bbox is correct
	m_charLocalMtx.SetIdentity();
	m_charLocalMtx.SetTranslation(GetStanceInfo(STANCE_STAND)->modelOffset);

	GetAnimatedCharacter()->SetExtraAnimationOffset(m_charLocalMtx);
}

void CAlien::DetachTentacle(ICharacterInstance *pCharacter,const char *tentacle)
{
  IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();
  uint32 numAttachmnets = pIAttachmentManager->GetAttachmentCount();

  IAttachment* pIAttachment = pIAttachmentManager->GetInterfaceByName(tentacle);
	IAttachmentObject* pIAttachmentObject = pIAttachment->GetIAttachmentObject();

  if (pIAttachmentObject) 
	{
	  //get character-instance of a tentacle 
    ICharacterInstance *pITentacleInstance = pIAttachmentObject->GetICharacterInstance();

		//detach tentacle from methagen 
		pIAttachment->ClearBinding();
	}
}

void CAlien::PushCharacterTentacles(ICharacterInstance *pCharacter)
{
	if (!pCharacter)
		return;

	ISkeletonPose* pISkeletonPose = pCharacter->GetISkeletonPose();
	if (pISkeletonPose==0)
		return; //if the character is a skin-attachment, it will have no skeleton

	int tNum(0);
	IPhysicalEntity *pTentacle = pISkeletonPose->GetCharacterPhysics(tNum);

	while(pTentacle)
	{
		m_tentaclesProxy.push_back(pTentacle);
		pTentacle = pISkeletonPose->GetCharacterPhysics(++tNum);
	}

	if (m_params.fullAnimTentacles[0])
	{
		char *pBone;
		char boneList[256];

		strcpy(boneList,m_params.fullAnimTentacles);
		pBone = boneList;
		pBone = strtok(pBone,";");

		while (pBone != NULL && *pBone)
		{
			pTentacle = pISkeletonPose->GetCharacterPhysics(pBone);
			if (pTentacle) 
				m_tentaclesProxyFullAnimation.push_back(pTentacle);

			pBone = strtok(NULL,";");
		}
	}
}

void CAlien::UpdateAnimGraph( IAnimationGraphState * pState )
{
	CActor::UpdateAnimGraph(pState);

	if (pState)
	{
		pState->SetInput(m_inputSpeed, m_stats.speed);
		pState->SetInput(m_inputDesiredSpeed, m_stats.desiredSpeed);
	}
}

void CAlien::PrePhysicsUpdate()
{
	IEntity* pEnt = GetEntity();
	if (pEnt->IsHidden())
		return;

	float frameTime = gEnv->pTimer->GetFrameTime();

	if (!m_stats.isRagDoll && GetHealth()>0)
	{	
		UpdateStats(frameTime);

		// pvl Probably obsolete more or less.

		// marcok: moved out
		Vec3 desiredMovement(m_input.posTarget - pEnt->GetWorldPos());
		float distance = desiredMovement.len();

		//FIXME:maybe find a better position for this?
		//when the player is supposed to reach some precise position/direction
		if (m_input.posTarget.len2()>0.0f)
		{ 
			Vec3 desiredMovement(m_input.posTarget - pEnt->GetWorldPos());
			
			float distance = desiredMovement.len();

	/*		if (distance > 0.01f)
			{
				// normalize it
				desiredMovement /= distance;
				desiredMovement *= m_input.speedTarget;
				SetDesiredSpeed(desiredMovement);        		
			}	    */
		}

		if (m_input.dirTarget.len2()>0.0f)
		{
			float t = distance > 3.f ? 0 : (3.f - distance) / 3.f;
			Vec3 desiredDir = m_input.viewDir.IsZero() ?
				Vec3::CreateSlerp( m_input.movementVector.GetNormalizedSafe(m_input.dirTarget), m_input.dirTarget, t ) :
				Vec3::CreateSlerp( m_input.viewDir, m_input.dirTarget, t );
			desiredDir.NormalizeSafe();
			SetDesiredDirection(desiredDir);
		}
	
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
			if (m_stats.inWaterTimer>0.1f)
				ProcessSwimming(frameTime);
			else
				ProcessMovement(frameTime);

			assert(m_moveRequest.rotation.IsValid());
			assert(m_moveRequest.velocity.IsValid());

			if (m_stats.inWaterTimer>0.1f)
					SetStance(STANCE_SWIM);
			else if (m_input.actions & ACTION_CROUCH)
				SetStance(STANCE_CROUCH);
			else if (m_input.actions & ACTION_PRONE)
				SetStance(STANCE_PRONE);
			else if (m_input.actions & ACTION_STEALTH)
				SetStance(STANCE_STEALTH);
			else
				SetStance(STANCE_STAND);

			//send the movement request to the animated character
			if (m_pAnimatedCharacter)
			{
				// synthesize a prediction
				m_moveRequest.prediction.nStates = 1;
				m_moveRequest.prediction.states[0].deltatime = 0.0f;
				m_moveRequest.prediction.states[0].velocity = m_moveRequest.velocity;
				m_moveRequest.prediction.states[0].position = pEnt->GetWorldPos();
				m_moveRequest.prediction.states[0].orientation = pEnt->GetWorldRotation();

				assert(m_moveRequest.rotation.IsValid());
				assert(m_moveRequest.velocity.IsValid());

				int frameID = gEnv->pRenderer->GetFrameID();
				DebugGraph_AddValue("EntID", pEnt->GetId());
				DebugGraph_AddValue("ReqVelo", m_moveRequest.velocity.GetLength());
				DebugGraph_AddValue("ReqVeloX", m_moveRequest.velocity.x);
				DebugGraph_AddValue("ReqVeloY", m_moveRequest.velocity.y);
				DebugGraph_AddValue("ReqVeloZ", m_moveRequest.velocity.z);
				DebugGraph_AddValue("ReqRotZ", RAD2DEG(m_moveRequest.rotation.GetRotZ()));

				m_pAnimatedCharacter->AddMovement(m_moveRequest);
			}
		}
	}

	UpdateDebugGraphs();
}

void CAlien::Update(SEntityUpdateContext& ctx, int updateSlot)
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
        float countScale = 1.f * cloakMult;
        float speedScale = 1.f * cloakMult; 
        
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

	//update the character offset
	Vec3 goal = (m_stats.isRagDoll?Vec3(0,0,0):GetStanceInfo(m_stance)->modelOffset);
	//if(!m_stats.isRagDoll)
	//	goal += m_stats.dynModelOffset;
	Interpolate(m_modelOffset,goal,5.0f,frameTime);
	
	m_charLocalMtx.SetTranslation(m_modelOffset+m_modelOffsetAdd);

	GetAnimatedCharacter()->SetExtraAnimationOffset(m_charLocalMtx);
}

void CAlien::UpdateView(SViewParams &viewParams)
{
	viewParams.nearplane = 0.0f;
	viewParams.fov = 90.0f*gf_PI/180.0f;

	viewParams.position = GetEntity()->GetPos();

	Matrix33 viewMtx(m_viewMtx);
	
	if (m_stats.isThirdPerson)
	{
		float thirdPersonDistance(g_pGameCVars->cl_tpvDist);
		float thirdPersonYaw(g_pGameCVars->cl_tpvYaw);

		if (thirdPersonYaw>0.001f)
			viewMtx *= Matrix33::CreateRotationZ(thirdPersonYaw * gf_PI/180.0f);

		viewParams.position += viewMtx.GetColumn(1) * -thirdPersonDistance;
	}

	viewParams.rotation = GetQuatFromMat33(viewMtx);
}

//FIXME:at some point, unify this with CPlayer via CActor
void CAlien::UpdateStats(float frameTime)
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
		m_stats.gravity = m_oldGravity;

		/*pe_player_dynamics simParSet;
		simParSet.bSwimming = true;
		pPhysEnt->SetParams(&simParSet);*/

		return;
	}

	//retrieve some information about the status of the player
	pe_status_dynamics dynStat;
	pe_status_living livStat;

	m_stats.isFloating = false;//used only by trooper for now
	
	//FIXME: temporary
	if (InZeroG() || IsFlying())
	{
		pe_player_dynamics simParSet;
		simParSet.bSwimming = true;
		simParSet.gravity.zero();
		pPhysEnt->SetParams(&simParSet);
	}
	//

	if( !pPhysEnt->GetStatus(&dynStat) ||
			!pPhysEnt->GetStatus(&livStat) ||
			!pPhysEnt->GetParams(&simPar) )
		return;

	//update status table
	if (livStat.bFlying)
	{
		if (m_stats.inAir<0.001f)
			CreateScriptEvent("liftOff",0);

		m_stats.inAir += frameTime;
		m_stats.onGround = 0.0f;
	}
	else
	{
		m_stats.onGround += frameTime;
		m_stats.inAir = 0.0f;
	}

  m_stats.groundMaterialIdx = livStat.groundSurfaceIdx;

	// check if walking on water/underwater
	Vec3 ppos(GetEntity()->GetWorldPos());

	float waterLevel(gEnv->p3DEngine->GetWaterLevel(&ppos));

	m_stats.relativeWaterLevel = ppos.z + GetStanceInfo(m_stance)->viewOffset.z - 0.3f - waterLevel;

	if (m_stats.relativeWaterLevel<=0.25f)
	{
		if (m_stats.inWaterTimer<=0.0f)
			CreateScriptEvent("splash",0);
		if (m_stats.relativeWaterLevel<0.0f)
		{
			m_stats.inWaterTimer += frameTime;
			m_stats.inAir = 0.0f;
		}
	}
	else
	{
		m_stats.inWaterTimer = 0.0f;
	}
	
	m_oldGravity = simPar.gravity;
	m_stats.gravity = simPar.gravity;
	m_stats.velocity = m_stats.velocityUnconstrained = dynStat.v;
  m_stats.angVelocity = dynStat.w;
	m_stats.speed = m_stats.speedFlat = m_stats.velocity.len();

	// [Mikko] The velocity from the physics in some weird cases have been #INF because of the player
	// Zero-G movement calculations. If this asserts triggers, the alien might have just collided with
	// a play that is stuck in the geometry.
	assert(NumberValid(m_stats.speed));

	m_stats.mass = dynStat.mass;

	//the alien is able to sprint for a bit right after being standing
	if (m_stats.speed > m_stats.sprintTreshold)
	{
		m_stats.sprintLeft = max(0.0f,m_stats.sprintLeft - frameTime);
		m_stats.sprintMaxSpeed = min(m_stats.speed, GetStanceInfo( m_stance )->maxSpeed );
	}
	else
	{
		// If the speed slowsdown to 80% of the last max speed, allow to sprint again.
		const float slowdownPercent = 0.8f;
		if( m_stats.speed < m_stats.sprintMaxSpeed * slowdownPercent )
			m_stats.sprintTreshold = m_stats.sprintMaxSpeed * slowdownPercent;
		m_stats.sprintLeft = m_params.sprintDuration;
	}

	//misc things
	Vec3 lookTarget(ZERO);//GetAIAttentionPos());
	if (lookTarget.len2()<0.01f)
		lookTarget = GetEntity()->GetSlotWorldTM(0) * GetLocalEyePos() + m_eyeMtx.GetColumn(1) * 10.0f;

	if (m_stats.lookTargetSmooth.len2()<0.01f)
		m_stats.lookTargetSmooth = lookTarget;
	else
		Interpolate(m_stats.lookTargetSmooth,lookTarget,3.0f,frameTime);

	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(GetEntity()->GetWorldPos(), ColorB(255,0,0,255), m_stats.lookTargetSmooth, ColorB(255,0,0,255));

	//
	UpdateFiringDir(frameTime);
	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(GetEntity()->GetWorldPos() + Vec3(0,0,5), ColorB(255,0,0,255), GetEntity()->GetWorldPos() + Vec3(0,0,5) + m_stats.fireDir * 20.0f, ColorB(255,0,0,255));

	//update some timers
	m_stats.inFiring = max(0.0f,m_stats.inFiring - frameTime);

	Interpolate(m_weaponOffset,GetStanceInfo(m_stance)->weaponOffset,2.0f,frameTime);
	Interpolate(m_eyeOffset,GetStanceInfo(m_stance)->viewOffset,2.0f,frameTime);

	if (m_endOfThePathTime > 0.0f)
		m_endOfThePathTime -= frameTime;	
}

void CAlien::ProcessRotation(float frameTime)
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

//	if (m_stats.inAir && IsZeroG())
	{		

		// Mikko: Separated the look and movement directions. This is a workaround! The reason is below (moved from the SetActorMovement):
		// >> Danny - old code had desired direction using vLookDir but this caused spinning behaviour
		// >> when it was significantly different to vMoveDir

		if (m_input.viewVector.len2()>0.0f)
		{
//			m_eyeMtx.SetRotationVDir(m_input.viewVector.GetNormalizedSafe());
			Matrix33	eyeTarget;
			eyeTarget.SetRotationVDir(m_input.viewVector.GetNormalizedSafe());
			Quat	eyeTargetQuat(eyeTarget);
			Quat	currQuat(m_eyeMtx);
			m_eyeMtx = Matrix33(Quat::CreateSlerp( currQuat.GetNormalized(), eyeTargetQuat, min(frameTime * 12.0f, 1.0f)));
		}

		if (m_input.viewVector.len2()>0.0f)
		{
			Vec3	lookat = m_eyeMtx.GetColumn(1);
			Vec3	orient = m_viewMtx.GetColumn(1);
			if( lookat.Dot( orient ) < cosf( DEG2RAD( 25.0f ) ) || m_forceOrient )
				m_viewMtx.SetRotationVDir(m_input.viewVector.GetNormalizedSafe());
		}
		else //if (m_input.deltaRotation.len2()>0.001f)
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
			m_eyeMtx = m_viewMtx;
		}
	}
}

void CAlien::GetMovementVector(Vec3& move, float& speed, float& maxSpeed)
{
	maxSpeed = GetStanceInfo(m_stance)->maxSpeed;

	// AI Movement
	move = m_input.movementVector;
	// Player movement
	// For controlling an alien as if it was a player (dbg stuff)
	move += m_viewMtx.GetColumn(0) * m_input.deltaMovement.x * maxSpeed;
	move += m_viewMtx.GetColumn(1) * m_input.deltaMovement.y * maxSpeed;
	move += m_viewMtx.GetColumn(2) * m_input.deltaMovement.z * maxSpeed;

	// probably obsolete
	move += m_viewMtx.GetColumn(1) * m_params.approachLookat * maxSpeed;

	// Cap the speed to stance max stance speed.
	speed = move.len();
	if(speed > maxSpeed)
	{
		move *= maxSpeed / speed;
		speed = maxSpeed;
	}
}

void CAlien::ProcessMovement(float frameTime)
{
	//FIXME:dont remove this yet
	//ProcessMovement2(frameTime);
	//return;

	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();

	if (!pPhysEnt)
		return;

	if (frameTime > 0.1f)
			frameTime = 0.1f;

	//movement
	Vec3 move;
	float	reqSpeed, maxSpeed;
	GetMovementVector(move, reqSpeed, maxSpeed);

	float reqSpeedNormalized(reqSpeed);
	reqSpeedNormalized /= maxSpeed;

	if (m_stats.sprintLeft)
		move *= m_params.sprintMultiplier;

	//FIXME:testing
	Interpolate(m_stats.physicsAnimationRatio,min(reqSpeedNormalized*(g_pGameCVars->g_alienPhysicsAnimRatio),1.0f),3.3f,frameTime,1.0f);

	Matrix33 velMtx( m_baseMtx );
	Vec3 vecRefRoll( move );

	// A little bit more workaround, need to aling the alien to some up vector too.
	Vec3 up, right, forward;

	if (m_input.upTarget.len2()>0.0f)
	{
		Vec3	diff = m_input.upTarget - m_input.posTarget;
		diff.NormalizeSafe();
		up = diff;
	}
	else
		up = m_viewMtx.GetColumn(2);

	if( move.len2() > 0 )
	{
		forward = move.GetNormalizedSafe();
	}
	else
	{
		forward = m_viewMtx.GetColumn(1);

		if (!m_forceOrient)
		{
			float dot(forward * m_baseMtx.GetColumn(1));
			if (dot>0.2f)
				forward = m_baseMtx.GetColumn(1);
			else
				m_followEyesTime = 1.0f;
		}
	}

	if ((m_followEyesTime-=frameTime)>0.001)
		forward = m_viewMtx.GetColumn(1);

	right = (forward % up).GetNormalizedSafe();
	velMtx.SetFromVectors(right,forward,right % forward);

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
			rollAmt = max(min(gf_PI*0.49f,dotRoll*m_params.rollAmount),-gf_PI*0.49f);

		Interpolate(m_roll,rollAmt,m_params.rollSpeed,frameTime);

		velMtx *= Matrix33::CreateRotationY(m_roll);
	}

	m_desiredVeloctyQuat = GetQuatFromMat33(velMtx);
	const SStanceInfo* pStanceInfo = GetStanceInfo(m_stance);
	float rotScale = 1.0f;
	if (pStanceInfo)
	{
		const float speedRange = max(1.0f, pStanceInfo->maxSpeed - m_params.speed_min);
		rotScale = (m_stats.speed - m_params.speed_min) / speedRange;
	}
	float rotSpeed = m_params.rotSpeed_min + (1.0f - rotScale) * (m_params.rotSpeed_max - m_params.rotSpeed_min);

	Interpolate(m_turnSpeed,rotSpeed,3.0f,frameTime);

	float turnSlowDown = min(1.0f,max(0.0f,1.0f-m_followEyesTime));
	Quat currQuat(m_baseMtx);
	m_baseMtx = Matrix33(Quat::CreateSlerp( currQuat.GetNormalized(), m_desiredVeloctyQuat, min(frameTime * m_turnSpeed * turnSlowDown, 1.0f)));
	m_baseMtx.OrthonormalizeFast();

	// Slow down if the forward direction differs from the movement direction.
	float speed = move.len();
	float	velScale(1.0);
	if(speed > 0.0001f)
	{
		Vec3	moveNorm(move);
		Vec3	forw(GetEntity()->GetRotation().GetColumn1()); //m_viewMtx.GetColumn(1);
		moveNorm.z = 0;
		moveNorm.NormalizeSafe();
		forw.z = 0;
		forw.NormalizeSafe();

		float	dot = (1.0f + forw.Dot(moveNorm)) / 2.0f;
		velScale = 0.3f + dot * 0.7f;
	}

	// Accelerate faster than slowdown.
	float	target = speed * velScale;
	float	s = 5.0f;
	//	if( target > m_curSpeed || bExactPositioning )
	if( m_input.posTarget.len2()>0.0f )
		s *= 2.0f;
	Interpolate( m_curSpeed, target, s, frameTime );
	if(speed > 0.0001f)
		velScale *= m_curSpeed / speed;

	move *= velScale;

	// make sure alien reaches destination, ignore speedInertia
	if (m_input.posTarget.len2()>0.0f)
		m_velocity = move;
	else
		Interpolate(m_velocity,move,m_params.speedInertia,frameTime);

	Quat modelRot(m_baseMtx);
	modelRot = Quat::CreateSlerp(GetEntity()->GetRotation().GetNormalized(), modelRot, min(frameTime * 6.6f/*m_turnSpeed*/ /** (m_stats.speed/GetStanceInfo(m_stance)->maxSpeed)*/, 1.0f));

	assert(GetEntity()->GetRotation().IsValid());
	assert(GetEntity()->GetRotation().GetInverted().IsValid());
	assert(modelRot.IsValid());
	m_moveRequest.rotation = GetEntity()->GetRotation().GetInverted() * modelRot;
	assert(m_moveRequest.rotation.IsValid());

	m_moveRequest.velocity = m_velocity;
	m_moveRequest.type = eCMT_Fly;

	//FIXME:sometime
	m_stats.desiredSpeed = m_stats.speed;
}

void CAlien::ProcessSwimming(float frameTime)
{
	if (frameTime > 0.1f)
			frameTime = 0.1f;

	//movement
	Vec3 move;
	float	reqSpeed, maxSpeed;
	GetMovementVector(move, reqSpeed, maxSpeed);

	if (m_stats.sprintLeft)
		move *= m_params.sprintMultiplier;

	//apply movement
	Vec3 desiredVel(move);

	//float up if no movement requested
	if (move.z>-0.1f)
		desiredVel.z += min(2.0f,-m_stats.relativeWaterLevel*1.5f);

	if (m_velocity.len2()<=0.0f)
		m_velocity = desiredVel;

	if (m_stats.inWaterTimer < 0.15f)
		m_velocity.z = m_stats.velocity.z * 0.9f;

	Interpolate(m_velocity,desiredVel,3.0f,frameTime);

	Quat modelRot(m_baseMtx);
	modelRot = Quat::CreateSlerp(GetEntity()->GetRotation().GetNormalized(), modelRot, min(frameTime * 6.6f/*m_turnSpeed*/ /** (m_stats.speed/GetStanceInfo(m_stance)->maxSpeed)*/, 1.0f));

	m_moveRequest.rotation = GetEntity()->GetRotation().GetInverted() * modelRot;
	m_moveRequest.type = eCMT_Fly;
	m_moveRequest.velocity = m_velocity;
}

void CAlien::ProcessMovement2(float frameTime)
{
	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();

	if (!pPhysEnt)
		return;

	if (frameTime > 0.1f)
			frameTime = 0.1f;

	//movement
	Vec3 move(m_input.movementVector);

	move += m_viewMtx.GetColumn(0) * m_input.deltaMovement.x;
	move += m_viewMtx.GetColumn(1) * m_input.deltaMovement.y;
	move += m_viewMtx.GetColumn(2) * m_input.deltaMovement.z;
	
	move += m_viewMtx.GetColumn(1) * m_params.approachLookat;

	//cap the movement vector to max 1
	float moveModule(move.len());

	if (moveModule > 1.0f)
		move /= moveModule;

	move *= GetStanceInfo( m_stance )->maxSpeed;

	if (m_stats.sprintLeft)
		move *= m_params.sprintMultiplier;

	//FIXME:testing
	Interpolate(m_stats.physicsAnimationRatio,min(moveModule*(g_pGameCVars->g_alienPhysicsAnimRatio),1.0f),3.3f,frameTime,1.0f);
		
	float color[] = {1,1,1,0.5f};
	gEnv->pRenderer->Draw2dLabel(100,100,2,color,false,"moveModule:%f,physicsAnimationRatio:%f",moveModule,m_stats.physicsAnimationRatio);
	//

	Matrix33 velMtx;
	Vec3 vecRefRoll;

	Vec3 tempVel;
	//a bit workaround: needed when the alien is forced to look in some direction
	if (m_input.dirTarget.len2()>0.0f)
		tempVel = m_viewMtx.GetColumn(1);
	else
		tempVel = m_viewMtx.GetColumn(1)*max(0.1f,m_params.forceView) + m_stats.velocity;//move;

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

	/*gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(GetEntity()->GetWorldPos(), ColorB(255,0,0,255), GetEntity()->GetWorldPos() + velMtx.GetColumn(0), ColorB(255,0,0,255));
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(GetEntity()->GetWorldPos(), ColorB(0,255,0,255), GetEntity()->GetWorldPos() + velMtx.GetColumn(1), ColorB(0,255,0,255));
	gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(GetEntity()->GetWorldPos(), ColorB(0,0,255,255), GetEntity()->GetWorldPos() + velMtx.GetColumn(2), ColorB(0,0,255,255));
	*/

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
			rollAmt = max(min(gf_PI*0.49f,dotRoll*m_params.rollAmount),-gf_PI*0.49f);

		Interpolate(m_roll,rollAmt,m_params.rollSpeed,frameTime);

		velMtx *= Matrix33::CreateRotationY(m_roll);
	}

	m_desiredVeloctyQuat = GetQuatFromMat33(velMtx);
	float rotSpeed = m_params.rotSpeed_min + (1.0f - (max(GetStanceInfo( m_stance )->maxSpeed - max(m_stats.speed - m_params.speed_min,0.0f),0.0f) / GetStanceInfo( m_stance )->maxSpeed)) * (m_params.rotSpeed_max - m_params.rotSpeed_min);
	Interpolate(m_turnSpeed,rotSpeed,3.0f,frameTime);

	Quat currQuat(m_baseMtx);
	m_baseMtx = Matrix33(Quat::CreateSlerp( currQuat.GetNormalized(), m_desiredVeloctyQuat, min(frameTime * m_turnSpeed, 1.0f)));
	m_baseMtx.OrthonormalizeFast();

	//a bit workaround: needed when the alien is forced to move in some position
/*	if (m_input.posTarget.len2()>0.0f)
		move = m_desiredVelocity;
	else*/
	{
		// Cap the speed to the ideal speed.
		float	moveLen( move.len() );
		if( moveLen > 0 && m_params.idealSpeed >= 0 && moveLen > m_params.idealSpeed )
			move *= m_params.idealSpeed / moveLen;
	}

	// make sure alien reaches destination, ignore speedInertia
	if (m_input.posTarget.len2()>0.0f)
		m_velocity = move;
	else
		Interpolate(m_velocity,move,m_params.speedInertia,frameTime);

	// Slow down if the forward direction differs from the movement direction.
	// A side effect of this is that the velocity after turning will be higher (=good).
	float speed = m_velocity.len();
	float	velScale( speed );
	if(velScale > 0 )
	{
		Vec3	move = m_velocity / velScale;
		Vec3	forw = GetEntity()->GetRotation().GetColumn1(); //m_viewMtx.GetColumn(1);
		float	dot = forw.Dot( move );
		const float treshold = cosf(DEG2RAD(15.0));
		if( dot > treshold )
			velScale = 1.0;
		else
		{
			if( dot < 0 ) dot = 0;
			velScale = (dot / treshold) * 0.99f + 0.01f;
		}
	}

	// Accelerate faster than slowdown.
	float	target = speed * velScale;
	float	s = 5.0f;
//	if( target > m_curSpeed || bExactPositioning )
	if( m_input.posTarget.len2()>0.0f )
		s *= 2.0f;
	Interpolate( m_curSpeed, target, s, frameTime );

	if( speed > 0 )
		velScale *= m_curSpeed / speed;

	pe_action_move actionMove;
	//FIXME:wip
	actionMove.dir = m_velocity + m_stats.animationSpeedVec;// * velScale;
	actionMove.iJump = 3;

	//FIXME:sometime
	m_stats.desiredSpeed = m_stats.speed;
//	m_stats.xDelta = m_stats.zDelta = 0.0f;

	pPhysEnt->Action(&actionMove);
}

void CAlien::ProcessAnimation(ICharacterInstance *pCharacter,float frameTime)
{
	if (m_linkStats.CanDoIK())
	{
		ISkeletonPose *pSkeletonPose = pCharacter ? pCharacter->GetISkeletonPose() : NULL;
		if (pSkeletonPose)
		{
			static const float customBlend[5] = { 0.04f, 0.06f, 0.08f, 0.6f, 0.6f };
			//update look ik	
			if(!m_stats.isGrabbed)
				pSkeletonPose->SetLookIK(true,gf_PI*0.9f,m_stats.lookTargetSmooth,customBlend);
			else
				pSkeletonPose->SetLookIK(false,0,Vec3(0,0,0));
		}
	}
}

void CAlien::ProcessBonesRotation(ICharacterInstance *pCharacter,float frameTime)
{
	CActor::ProcessBonesRotation(pCharacter,frameTime);

	//FIXME:testing
	if (pCharacter)
	{
		if (m_stats.physicsAnimationRatio>0.001f)
		{
			int32 idx = pCharacter->GetISkeletonPose()->GetJointIDByName("root");
			if (idx>-1)
			{
				Vec3 rootPos(pCharacter->GetISkeletonPose()->GetAbsJointByID(idx).t*m_stats.physicsAnimationRatio);
        m_stats.animationSpeedVec = frameTime>0.f ? Matrix33(GetEntity()->GetSlotWorldTM(0)) * ((rootPos - m_stats.lastRootPos) / frameTime) : Vec3(0);
				m_stats.lastRootPos = rootPos;

				m_charLocalMtx.SetTranslation(m_charLocalMtx.GetTranslation()-rootPos);
				GetAnimatedCharacter()->SetExtraAnimationOffset(m_charLocalMtx);
			}
		}
		else
		{
			m_stats.animationSpeedVec.zero();
			m_stats.lastRootPos.zero();
		}
	}
	//

	return;

	//flat desired view direction
	Matrix33 modelMtx(GetEntity()->GetRotation()/* * Matrix33::CreateRotationZ(-gf_PI * 0.5)*/);

	Vec3 viewFlat(m_viewMtx.GetColumn(1) - m_viewMtx.GetColumn(1) * (modelMtx * Matrix33::CreateScale(Vec3(0,0,1))));
	viewFlat.NormalizeSafe();

	float dotForward(viewFlat * modelMtx.GetColumn(1));
	float dotSide(viewFlat * modelMtx.GetColumn(0));
	float yawDiff(cry_atan2f(-dotSide,-dotForward));

	float pitchDiff(0);
	/*modelMtx = modelMtx * Matrix33::CreateRotationZ(yawDiff);

	dotForward = m_viewMtx.GetColumn(1) * modelMtx.GetColumn(1);
	float dotUp(m_viewMtx.GetColumn(1) * modelMtx.GetColumn(2));
	float pitchDiff(cry_atan2f(dotUp,-dotForward));*/

	//CryLogAlways("y:%.1f | p:%.1f",RAD2DEG(yawDiff),RAD2DEG(pitchDiff));

	if (yawDiff>0.52f)
		yawDiff = 0.52f;

	if (yawDiff<-0.52f)
		yawDiff = -0.52f;

	if (pitchDiff>1.04f)
		pitchDiff = 1.04f;

	if (pitchDiff<-1.04f)
		pitchDiff = -1.04f;

	//IJoint *pBones[2];
	//pBones[0] = pCharacter->GetISkeleton()->GetIJointByName("head");
	//pBones[1] = pCharacter->GetISkeleton()->GetIJointByName("neck");

	int16 id[2];
	id[0] = pCharacter->GetISkeletonPose()->GetJointIDByName("head");
	id[1] = pCharacter->GetISkeletonPose()->GetJointIDByName("neck");

	pitchDiff /= 2.0f;
	yawDiff /= 2.0f;

	Quat qtH;
	Quat qtV;
	Quat qtR;
	Quat qtTotal;
	Quat qtParent;
	Quat qtParentCnj;

	for (int i=0;i<2;++i)
	{
		if (id[i])
		{
			qtH.SetRotationAA( yawDiff, Vec3(0.0f, 0.0f, 1.0f) );//yaw
			qtV.SetRotationAA( pitchDiff, Vec3(1.0f, 0.0f, 0.0f) );//pitch
			qtR.SetRotationAA( 0.0f,  Vec3(0.0f, 1.0f, 0.0f) );//roll
			
		//	IJoint* pIJoint = pBones[i]->GetParent();
			int16 parentID = pCharacter->GetISkeletonPose()->GetParentIDByID(id[i]);
			Quat wquat(IDENTITY);
			if (parentID>=0)
				//	wquat=!Quat(pCharacter->GetISkeleton()->GetAbsJMatrixByID(parentID));
				wquat=!pCharacter->GetISkeletonPose()->GetAbsJointByID(parentID).q;

			qtParent = wquat;//pBones[i]->GetParentWQuat();
			qtParentCnj = qtParent;
			qtParentCnj.w = -qtParentCnj.w;
			qtTotal = qtParent*qtR*qtV*qtH*qtParentCnj;

		//	pBones[i]->SetPlusRotation( qtTotal );
			//pCharacter->GetISkeleton()->SetPlusRotation( id[i], qtTotal );
		}
	}
}

//FIXME:tentacle testing
void CAlien::SetTentacles(ICharacterInstance *pCharacter,float animStiffness,float mass,float damping,bool bRagdolize)
{
	//TODO:use the correct number, not an hardcoded "8", and make it faster by holding pointers and such.
	pe_params_rope pRope;
	pe_simulation_params sp;
	pe_action_target_vtx atv;
	pe_params_flags pf;

	pRope.stiffnessAnim = animStiffness;
	
	float jlim = g_pGameCVars->g_tentacle_joint_limit;
	if (jlim>=0)
		pRope.jointLimit = DEG2RAD(jlim);
	else
		pRope.jointLimit = DEG2RAD(m_params.tentaclesJointLimit);

	//pRope.stiffnessDecayAnim = 10.1f;
	if (bRagdolize)
	{
		pRope.bTargetPoseActive=2, pRope.collDist=m_params.tentaclesRadius;
		pf.flagsOR = rope_target_vtx_rel0|rope_no_stiffness_when_colliding/*|rope_collides|rope_collides_with_terrain*/;
		pf.flagsAND = ~(rope_findiff_attached_vel | rope_no_solver);
		sp.minEnergy = sqr(0.03f);
	}

	if (damping > 0.001f)
		sp.damping = damping;

	if (mass > 0.001f)
		pRope.mass = mass;

	std::vector<IPhysicalEntity *>::iterator it;
	for (it = m_tentaclesProxy.begin(); it != m_tentaclesProxy.end(); it++)
	{
		IPhysicalEntity *pT = *it;
		if (pT)
		{
			pT->SetParams(&pRope);
			pT->SetParams(&sp);
			pT->SetParams(&pf);
			pT->Action(&atv);
		}
	}

	if (m_params.fullAnimationTentaclesBlendMult>0.001)
		pRope.stiffnessAnim = animStiffness * m_params.fullAnimationTentaclesBlendMult;
	else
		pRope.stiffnessAnim = 0;

	for (it = m_tentaclesProxyFullAnimation.begin(); it != m_tentaclesProxyFullAnimation.end(); it++)
	{
		IPhysicalEntity *pT = *it;
		if (pT)
			pT->SetParams(&pRope);
	}
}

void CAlien::Draw(bool draw)
{
	uint32 slotFlags = GetEntity()->GetSlotFlags(0);

	if (draw)
		slotFlags |= ENTITY_SLOT_RENDER;
	else
		slotFlags &= ~ENTITY_SLOT_RENDER;

	GetEntity()->SetSlotFlags(0,slotFlags);
}

void CAlien::ResetAnimations()
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
		character->GetISkeletonPose()->SetLookIK(false,gf_PI*0.9f,m_stats.lookTargetSmooth);
	}
}

//------------------------------------------------------------------------
void CAlien::Reset(bool toGame)
{
  if (m_pGroundEffect)
  {
    m_pGroundEffect->Stop(!toGame);
  }
}

void CAlien::Kill()
{
	CActor::Kill();

	ResetAnimations();

	//FIXME:this stops the ground effect, maybe its better to directly remove the effect instead?
	if (m_pGroundEffect)
		m_pGroundEffect->Stop(true);

	if (m_pBeamEffect)
		m_pBeamEffect->Stop();

  if (m_pTrailAttachment)  
    m_pTrailAttachment->ClearBinding();    

	if (m_pTurnSound)
	{
		IEntitySoundProxy* pSoundProxy = (IEntitySoundProxy*)GetEntity()->CreateProxy(ENTITY_PROXY_SOUND);
		pSoundProxy->SetStaticSound(m_pTurnSound->GetId(), false);
		pSoundProxy->StopSound(m_pTurnSound->GetId(), ESoundStopMode_EventFade);
		m_pTurnSound->Stop(ESoundStopMode_EventFade);
		m_pTurnSound = NULL;
	}

}

void CAlien::Revive(bool fromInit)
{
	CActor::Revive(fromInit);

	m_stats = SAlienStats();
	
	m_modelOffset = Vec3(0,0,0);
	m_modelOffsetAdd = Vec3(0,0,0);
	m_velocity = Vec3(0,0,0);
	m_desiredVelocity = Vec3(0,0,0);

	m_turnSpeed = 0.0f;
	m_turnSpeedGoal = 0.0f;
	m_roll = 0.0f;

	m_curSpeed = 0;

	m_endOfThePathTime = -1.0f;

	m_tentacleBlendRatio = 1.0f;

	m_baseMtx = Matrix33(GetEntity()->GetRotation());
	m_modelQuat = GetEntity()->GetRotation();
	SetDesiredDirection(GetEntity()->GetRotation().GetColumn1());

	m_angularVel = Ang3(0,0,0);

	m_charLocalMtx.SetIdentity();

	m_isFiring = false;

  m_input.posTarget.zero();
  m_input.dirTarget.zero();

	m_forceOrient = false;

	ResetAnimations();

	//initialize the ground effect
	if (m_params.groundEffect[0])
	{
		if (!m_pGroundEffect)
			m_pGroundEffect = g_pGame->GetIGameFramework()->GetIEffectSystem()->CreateGroundEffect(GetEntity());

		if (m_pGroundEffect)
		{
			m_pGroundEffect->SetInteraction(m_params.groundEffect);
			m_pGroundEffect->SetHeight(m_params.groundEffectHeight);
      m_pGroundEffect->SetHeightScale(m_params.groundEffectHeightScale, m_params.groundEffectHeightScale);
			m_pGroundEffect->SetFlags(m_pGroundEffect->GetFlags() | IGroundEffect::eGEF_StickOnGround);
      
      if (gEnv->pSystem->IsEditor())
        m_pGroundEffect->Stop(true);
		}
	}

  if(!m_pTrailAttachment && m_params.trailEffect[0] && gEnv->p3DEngine->FindParticleEffect(m_params.trailEffect))
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
        CryLog("[CAlien::Revive] %s: 'trail_attachment' not found.", GetEntity()->GetName());
    }
  }
  
	if (m_params.healthTrailEffect[0] && gEnv->p3DEngine->FindParticleEffect(m_params.healthTrailEffect))
	{
		if (ICharacterInstance* pCharInstance = GetEntity()->GetCharacter(0))
		{
			IAttachmentManager* pAttachmentManager = pCharInstance->GetIAttachmentManager(); 
			if (IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName("health_trail_attachment"))
			{ 
				pAttachment->ClearBinding();
				CEffectAttachment* pEffectAttachment = new CEffectAttachment(m_params.healthTrailEffect, Vec3(0,0,0), m_params.healthTrailEffectDir.GetNormalized(), 1);
				pEffectAttachment->CreateEffect();
				pAttachment->AddBinding(pEffectAttachment);
				m_pHealthTrailAttachment = pAttachment;
				m_healthTrailScale = 0.f;
			} 
			else
				CryLog("[CAlien::Revive] %s: 'health_trail_attachment' not found.", GetEntity()->GetName());
		}
	}

	if (m_pAnimatedCharacter)
	{
		SAnimatedCharacterParams params = m_pAnimatedCharacter->GetParams();
		params.flags |= eACF_ZCoordinateFromPhysics | eACF_NoTransRot2k | eACF_ImmediateStance | eACF_NoLMErrorCorrection;
		m_pAnimatedCharacter->SetParams(params);
	}

	if (m_pBeamEffect)
		m_pBeamEffect->Stop();  

  m_searchbeam.goalQuat.SetIdentity();
}

void CAlien::RagDollize( bool fallAndPlay )
{
  if (m_stats.isRagDoll && !gEnv->pSystem->IsSerializingFile())
		return;

	ResetAnimations();

	assert(!fallAndPlay && "Fall and play not supported for aliens yet");

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	if (pCharacter)
		pCharacter->GetISkeletonPose()->SetRagdollDefaultPose();

	CActor::RagDollize( fallAndPlay );

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
		if(sp.mass <= 0)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Tried ragdollizing alien with 0 mass.");
			sp.mass = 200.0f;
		}
		pPhysEnt->SetParams(&sp);

		pe_params_articulated_body pa;
		pa.dampingLyingMode = 5.5f;    
		//pa.scaleBounceResponse = 0.1f;
		pPhysEnt->SetParams(&pa);
	}

	if(gEnv->pSystem->IsSerializingFile())
	{
		//the finish physicalization
		PostPhysicalize();
	}

	pCharacter = GetEntity()->GetCharacter(0);	
	if (pCharacter)
	{
		pCharacter->EnableStartAnimation(false);
		SetTentacles(pCharacter,8.5f,0,2.25f,true);
	}
}

// sets searchbeam goal dir in entity space
void CAlien::SetSearchBeamGoal(const Vec3& dir)
{  
  m_searchbeam.goalQuat = Quat::CreateRotationVDir(dir);
}

// returns searchbeam dir in entity space
Quat CAlien::GetSearchBeamQuat() const
{
  return Quat(GetEntity()->GetSlotLocalTM(0, false)) * m_searchbeam.pAttachment->GetAttModelRelative().q;
}

// sets searchbeam attachment to orientation given in entity space
void CAlien::SetSearchBeamQuat(const Quat& rot)
{
  ICharacterInstance* pChar = GetEntity()->GetCharacter(0);
  if (!pChar)
    return;

  // transform to attachment space
  Quat rotCharInv = Quat(GetEntity()->GetSlotLocalTM(0, false)).GetInverted();
  Quat rotBoneInv = pChar->GetISkeletonPose()->GetAbsJointByID(m_searchbeam.pAttachment->GetBoneID()).q.GetInverted();
  
  QuatT lm = m_searchbeam.pAttachment->GetAttRelativeDefault();  
  lm.q = rotBoneInv * rotCharInv * rot;  
  m_searchbeam.pAttachment->SetAttRelativeDefault(lm);  
}

void CAlien::UpdateSearchBeam(float frameTime)
{
  ICharacterInstance* pChar = GetEntity()->GetCharacter(0);
  if (!pChar)
    return;

  Quat beamQuat = GetSearchBeamQuat();
  if (beamQuat.IsEquivalent(m_searchbeam.goalQuat))
    return;

  beamQuat.SetNlerp(beamQuat, m_searchbeam.goalQuat, frameTime*5.f);
  
  SetSearchBeamQuat(beamQuat);
}
 

//this will convert the input into a structure the player will use to process movement & input
void CAlien::OnAction(const ActionId& actionId, int activationMode, float value)
{
	GetGameObject()->ChangedNetworkState( eEA_GameServerStatic | eEA_GameServerDynamic | eEA_GameClientStatic | eEA_GameClientDynamic );

	//this tell if OnAction have to be forwarded to scripts
	bool filterOut(true);
	const CGameActions& actions = g_pGame->Actions();

	if (actions.rotateyaw == actionId)
	{
		m_input.deltaRotation.z -= value;
		filterOut = false;
	}
	else if (actions.rotatepitch == actionId)
	{
		m_input.deltaRotation.x -= value;
		filterOut = false;
	}
	else if (actions.moveright == actionId)
	{
		m_input.deltaMovement.x += value;
		filterOut = false;
	}
	else if (actions.moveleft == actionId)
	{
		m_input.deltaMovement.x -= value;
		filterOut = false;
	}
	else if (actions.moveforward == actionId)
	{
		m_input.deltaMovement.y += value;
		filterOut = false;
	}
	else if (actions.moveback == actionId)
	{
		m_input.deltaMovement.y -= value;
		filterOut = false;
	}
	else if (actions.jump == actionId)
	{
		m_input.actions |= ACTION_JUMP;		
	}
	else if (actions.crouch == actionId)
	{
		m_input.actions |= ACTION_CROUCH;
	}
	else if (actions.prone == actionId)
	{
		if (!(m_input.actions & ACTION_PRONE))
			m_input.actions |= ACTION_PRONE;
		else
			m_input.actions &= ~ACTION_PRONE;
	}
	else if (actions.sprint == actionId)
	{
		m_input.actions |= ACTION_SPRINT;
	}
	else if (actions.leanleft == actionId)
	{
		m_input.actions |= ACTION_LEANLEFT;
	}
	else if (actions.leanright == actionId)
	{
		m_input.actions |= ACTION_LEANRIGHT;
	}
	else if (actions.thirdperson == actionId)
	{ 
		m_stats.isThirdPerson = !m_stats.isThirdPerson;
	}

	//FIXME: this is duplicated from CPlayer.cpp, put it just once in CActor.
	//send the onAction to scripts, after filter the range of actions. for now just use and hold
	if (filterOut)
	{
		HSCRIPTFUNCTION scriptOnAction(NULL);

		IScriptTable *scriptTbl = GetEntity()->GetScriptTable();

		if (scriptTbl)
		{
			scriptTbl->GetValue("OnAction", scriptOnAction);

			if (scriptOnAction)
			{
				char *activation = 0;

				switch(activationMode)
				{
				case eAAM_OnHold:
					activation = "hold";
					break;
				case eAAM_OnPress:
					activation = "press";
					break;
				case eAAM_OnRelease:
					activation = "release";
					break;
				default:
					activation = "";
					break;
				}

				Script::Call(gEnv->pScriptSystem,scriptOnAction,scriptTbl,actionId.c_str(),activation, value);
			}
		}
	}

	CActor::OnAction(actionId, activationMode, value);
}

void CAlien::SetStats(SmartScriptTable &rTable)
{
	CActor::SetStats(rTable);
}

//fill the status table for the scripts
void CAlien::UpdateScriptStats(SmartScriptTable &rTable)
{
	CActor::UpdateScriptStats(rTable);
}

void CAlien::SetParams(SmartScriptTable &rTable,bool resetFirst)
{
	//not sure about this
	if (resetFirst)
	{
		m_params = SAlienParams();
	}

	CActor::SetParams(rTable,resetFirst);

	rTable->GetValue("speedInertia",m_params.speedInertia);
	rTable->GetValue("rollAmount",m_params.rollAmount);
	rTable->GetValue("rollSpeed",m_params.rollSpeed);

	rTable->GetValue("sprintMultiplier",m_params.sprintMultiplier);
	rTable->GetValue("sprintDuration",m_params.sprintDuration);
			
	rTable->GetValue("rotSpeed_min",m_params.rotSpeed_min);
	rTable->GetValue("rotSpeed_max",m_params.rotSpeed_max);

	rTable->GetValue("speed_min",m_params.speed_min);
	rTable->GetValue("movingBend",m_params.movingBend);

	rTable->GetValue("idealSpeed",m_params.idealSpeed);
	rTable->GetValue("blendingRatio",m_params.blendingRatio);
	rTable->GetValue("approachLookat",m_params.approachLookat);

	rTable->GetValue("forceView",m_params.forceView);

	const char *str;
	if (rTable->GetValue("fullAnimationTentacles",str))
		strncpy(m_params.fullAnimTentacles,str,256);
	else
		m_params.fullAnimTentacles[0] = 0;

	rTable->GetValue("fullAnimationTentaclesBlendMult",m_params.fullAnimationTentaclesBlendMult);

	int tentaclesCollide(0);
	if (rTable->GetValue("tentaclesCollide",tentaclesCollide))
		m_params.tentaclesCollide = tentaclesCollide;

	//
	//rTable->GetValue("jumpTo",m_params.jumpTo);

	//
	rTable->GetValue("tentaclesRadius",m_params.tentaclesRadius);
	rTable->GetValue("tentaclesJointLimit",m_params.tentaclesJointLimit);
	if (rTable->GetValue("tentaclesMaterial",str))
	{
		strncpy(m_params.tentaclesMaterial,str,64);
		m_params.tentaclesMaterial[63] = 0;
	}
	rTable->GetValue("tentacleStiffnessDecay",m_params.tentacleStiffnessDecay);
	rTable->GetValue("tentacleDampAnim",m_params.tentacleDampAnim);

	//
	rTable->GetValue("cameraShakeRange",m_params.cameraShakeRange);
	rTable->GetValue("cameraShakeMultiplier",m_params.cameraShakeMultiplier);

	//
  m_params.groundEffect[0] = 0;
	if (rTable->GetValue("groundEffect",str))
	{
		strncpy(m_params.groundEffect,str,128);
		m_params.groundEffect[127] = 0;
	}
	
	rTable->GetValue("groundEffectHeight",m_params.groundEffectHeight);
  rTable->GetValue("groundEffectHeightScale",m_params.groundEffectHeightScale);
  rTable->GetValue("groundEffectBaseScale",m_params.groundEffectBaseScale);
  rTable->GetValue("groundEffectMaxSpeed",m_params.groundEffectMaxSpeed);

  m_params.trailEffect[0] = 0;
  if (rTable->GetValue("trailEffect",str))
  {
    strncpy(m_params.trailEffect,str,128);
    m_params.trailEffect[127] = 0;
    
    rTable->GetValue("trailEffectMinSpeed",m_params.trailEffectMinSpeed);
    rTable->GetValue("trailEffectMaxSpeedSize",m_params.trailEffectMaxSpeedSize);
    rTable->GetValue("trailEffectMaxSpeedCount",m_params.trailEffectMaxSpeedCount);
		rTable->GetValue("trailEffectDir",m_params.trailEffectDir);
  }
  
	m_params.healthTrailEffect[0] = 0;
	if (rTable->GetValue("healthTrailEffect",str))
	{
		strncpy(m_params.healthTrailEffect,str,128);
		m_params.healthTrailEffect[127] = 0;

		rTable->GetValue("healthTrailEffectMaxSize",m_params.healthTrailEffectMaxSize);
		rTable->GetValue("healthTrailEffectMaxCount",m_params.healthTrailEffectMaxCount);
		rTable->GetValue("healthTrailEffectDir",m_params.healthTrailEffectDir);
	}

  if (rTable->GetValue("turnSound",str) && gEnv->pSoundSystem)
  { 
		if (!m_pTurnSound)
		{
			// create sound once
			m_pTurnSound = gEnv->pSoundSystem->CreateSound(str, FLAG_SOUND_DEFAULT_3D);  

			if (m_pTurnSound)
				m_pTurnSound->SetSemantic(eSoundSemantic_Living_Entity);
		}
    
    if (rTable->GetValue("turnSoundBone",str))
    { 
      if (ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0))      
        m_params.turnSoundBoneId = pCharacter->GetISkeletonPose()->GetJointIDByName(str);
    }
        
    rTable->GetValue("turnSoundMaxVel", m_params.turnSoundMaxVel);
  }  
}

void CAlien::SetDesiredSpeed(const Vec3 &desiredSpeed)
{
	m_input.movementVector = desiredSpeed;
}

void CAlien::SetDesiredDirection(const Vec3 &desiredDir)
{
	if (desiredDir.len2()>0.001f)
	{
		m_viewMtx.SetRotationVDir(desiredDir.GetNormalizedSafe());
		m_eyeMtx = m_viewMtx;
	}

	//m_input.viewVector = desiredDir;
}

// common functionality can go in here and called from subclasses that override SetActorMovement
void CAlien::SetActorMovementCommon(SMovementRequestParams& control)
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

  int nPoints = control.remainingPath.size();
  bool exactActive = false;

	control.eActorTargetPhase = eATP_None;
	if (control.bExactPositioning || bExactPos) 
	{
		// activate exact positioning    
		// todo: determine a better threshold
		if (nPoints && (nPoints == 1 || GetEntity()->GetWorldPos().GetSquaredDistance( control.remainingPath[0].vPos ) < 2.5f))
		{      
			// todo: determine a better threshold
			float dist((GetEntity()->GetWorldPos() - control.remainingPath[0].vPos).GetLengthSquared());
			if(dist<.3f*.3f)
				// tell AI the position is reached, approach/trace should be finished
				control.eActorTargetPhase = eATP_Finished;
			m_input.posTarget = control.remainingPath[0].vPos;
			m_input.dirTarget = control.remainingPath[0].vDir;
			// this used to be control.
			m_input.speedTarget = 10.0f;
			exactActive = true;
		}    
	}

	if(control.fDistanceToPathEnd < 2.5f)
		m_forceOrient = true;

  if (!exactActive)
  {
    m_input.posTarget.zero();
    m_input.dirTarget.zero();
  }

	m_stats.fireDirGoal = (control.vShootTargetPos - state.weaponPosition).GetNormalizedSafe();

	if (m_pAnimatedCharacter)
		m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( m_inputAiming, control.aimLook? 1 : 0 );

  // draw pathpoints
  /*if (nPoints)
  { 
    IRenderAuxGeom *pGeom = gEnv->pRenderer->GetIRenderAuxGeom();
    float size = 0.25f;

    for (PATHPOINTVECTOR::iterator it = control.path.begin(); it != control.path.end(); ++it)
    {        
      pGeom->DrawSphere((*it).vPos, size, ColorB(0,255,0,128));
      size += 0.1f;
    }
  }*/
}

//---------------------------------
//AI Specific
void CAlien::SetActorMovement(SMovementRequestParams &control)
{
	SMovementState state;
	GetMovementController()->GetMovementState(state);

  SetActorMovementCommon(control);

	if(!control.vAimTargetPos.IsZero())
		SetDesiredDirection((control.vAimTargetPos - state.weaponPosition).GetNormalizedSafe());
	else if(!control.vLookTargetPos.IsZero())
		SetDesiredDirection((control.vLookTargetPos - state.eyePosition).GetNormalizedSafe());
	else
		SetDesiredDirection(GetEntity()->GetWorldRotation() * FORWARD_DIRECTION);

	SetDesiredSpeed(control.vMoveDir * control.fDesiredSpeed);

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

	// When firing, force orientation towards the target.
	if(!control.vShootTargetPos.IsZero())
		m_forceOrient = true;
	else
		m_forceOrient = false;

	// Override the stance based on special behavior.
	SetActorStance(control, actions);

	m_input.actions = actions;


//	GetEntity()->GetScriptTable()->SetValue( "fireDir", control.vFireDir );

//	CryLog( "mv: (%f, %f, %f)", m_input.movementVector.x, m_input.movementVector.y, m_input.movementVector.z );
//	CryLog( "desiredSpeed: %f (mv.len=%f)", control.m_desiredSpeed, control.m_movementVector.GetLength() );

	//SetFiring(control.fire);
}

void CAlien::SetFiring(bool fire)
{
	//weapons
	//FIXME:remove this weapon specific ASAP
	if (fire)
	{
		if (!m_stats.isFiring)
			OnAction("attack1", eAAM_OnPress, 1.0f);
	}
	else
	{
		if (m_stats.isFiring)
			OnAction("attack1", eAAM_OnRelease, 1.0f);
	}

	m_stats.isFiring = fire;
}

void CAlien::SetActorStance(SMovementRequestParams &control, int& actions)
{
	bool	atEndOfPath(false);
	float	curSpeed(m_velocity.len());
	if (curSpeed < 0.1f)
		curSpeed = 0.1f;

	float	timeEstimate = 0.25f;	// if we are the point in timeEstime seconds, start to go vert already.

//	if (GetStance() == STANCE_STEALTH || (actions & ACTION_STEALTH))
//		timeEstimate *= 1.0f; //0.5f;

	if (GetStance() == STANCE_PRONE && control.fDistanceToPathEnd > 0 && control.fDistanceToPathEnd < curSpeed * timeEstimate )
	{
		if (m_endOfThePathTime < 0.0f)
			m_endOfThePathTime = timeEstimate;
//		atEndOfPath = true;
		m_forceOrient = true;
	}

	if (control.bExactPositioning && m_endOfThePathTime > 0.1f)
	{
		atEndOfPath = true;
		m_forceOrient = true;
	}

	// Set stance based on the desired speed.
	if(!atEndOfPath && control.vMoveDir.GetLengthSquared() > 0.1f && control.fDesiredSpeed > 0.001f)
	{
//		CryLog( "ACTION_PRONE %s modeDirLen=%f desiredSpeed=%f m_endOfThePathTime=%f dist=%f", atEndOfPath ? "atEndOfPath" : "", control.vMoveDir.GetLengthSquared(), control.fDesiredSpeed, m_endOfThePathTime, control.fDistanceToPathEnd);
		actions = ACTION_PRONE;
		m_forceOrient = true;
	}
	else
	{
		if (actions == ACTION_STEALTH)
		{
//			CryLog( "ACTION_STEALTH %s modeDirLen=%f desiredSpeed=%f m_endOfThePathTime=%f dist=%f", atEndOfPath ? "atEndOfPath" : "", control.vMoveDir.GetLengthSquared(), control.fDesiredSpeed, m_endOfThePathTime, control.fDistanceToPathEnd);
			actions = ACTION_STEALTH;
		}
		else if (actions == ACTION_PRONE)
		{
			actions = ACTION_PRONE;
			m_forceOrient = true;
		}
		else
		{
//			CryLog( "ACTION_STAND %s modeDirLen=%f desiredSpeed=%f m_endOfThePathTime=%f dist=%f", atEndOfPath ? "atEndOfPath" : "", control.vMoveDir.GetLengthSquared(), control.fDesiredSpeed, m_endOfThePathTime, control.fDistanceToPathEnd);
			actions = 0;
		}
	}
}

void CAlien::SetAnimTentacleParams(pe_params_rope& pRope, float animBlend)
{
	pRope.stiffnessDecayAnim = 0.75f;


	if (animBlend<0.001f)
	{
		pRope.stiffnessAnim = 0;	// Special case, use full animation.
		pRope.dampingAnim = 1.0f;	// When stiffness is zero, this value does not really matter, set it to sane value anyway.
	}
	else
	{
		//FIXME:compatibility for old values
		if (animBlend>1.001f)
		{
			pRope.stiffnessAnim = animBlend;
			pRope.dampingAnim = 1.0f + (animBlend / 100.0f) * 3.5f;
		}
		else
		{
			float vMax(100.0f);
			float vMin(1.0f);
			float value(1.0f - animBlend);

			pRope.stiffnessAnim = vMin + (value*value*value) * (vMax - vMin);
			pRope.dampingAnim = 1.0f + value * 3.5f;
		}
	}

}


void CAlien::GetActorInfo(SBodyInfo& bodyInfo)
{
	bodyInfo.vEyePos = GetEntity()->GetSlotWorldTM(0) * m_eyeOffset;
	bodyInfo.vFirePos = GetEntity()->GetSlotWorldTM(0) * m_weaponOffset;

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
	bodyInfo.vFireDir = m_stats.fireDir;
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

void CAlien::SetAngles(const Ang3 &angles) 
{
	Matrix33 rot(Matrix33::CreateRotationXYZ(angles));
	SetDesiredDirection(rot.GetColumn(1));
}

Ang3 CAlien::GetAngles() 
{
	Ang3 angles;
	angles.SetAnglesXYZ(m_viewMtx);

	return angles;
}

void CAlien::StanceChanged(EStance last)
{
	float delta(GetStanceInfo(last)->modelOffset.z - GetStanceInfo(m_stance)->modelOffset.z);
	if (delta>0.0f)
		m_modelOffset.z -= delta;
}

void CAlien::FullSerialize( TSerialize ser )
{
	CActor::FullSerialize(ser);

	ser.BeginGroup("CAlien");		//this serialization has to be redesigned to work properly in MP
	ser.Value("modelQuat", m_modelQuat, 'ori1');
	ser.Value("health", m_health);
	// skip matrices
	ser.Value("velocity", m_velocity);
	ser.Value("desiredVelocity", m_desiredVelocity);
	ser.Value("desiredVelocityQuat", m_desiredVeloctyQuat);
	ser.Value("turnSpeed", m_turnSpeed);
	ser.Value("turnSpeedGoal", m_turnSpeedGoal);
	ser.Value("angularVel", m_angularVel);
	ser.Value("tentacleBlendRatio", m_tentacleBlendRatio);
	ser.Value("isFiring", m_isFiring, 'bool');
	ser.EnumValue("stance", m_stance, STANCE_NULL, STANCE_LAST);
	ser.EnumValue("desiredStance", m_desiredStance, STANCE_NULL, STANCE_LAST);
	ser.EndGroup();

	m_input.Serialize(ser);
	m_stats.Serialize(ser);
	m_params.Serialize(ser);

	if(ser.IsReading())
	{
		m_tentaclesProxy.clear();
		m_tentaclesProxyFullAnimation.clear();
	}
}

void CAlien::PostSerialize()
{
	CActor::PostSerialize();

	//temporary fix for non-physicalized scouts in fleet
	/*SActorStats *pStats = GetActorStats();
	if(pStats && pStats->isRagDoll)	//fixes "frozen" scouts in fleet if Anton doesn't
	{
		if(IItem* pItem = GetCurrentItem(false))
		{
			pItem->Drop(1.0f, false, true);
			gEnv->pEntitySystem->RemoveEntity(pItem->GetEntityId()); //else firing effects could continue to render
		}
		Physicalize();
	}*/
}

void CAlien::SerializeXML( XmlNodeRef& node, bool bLoading )
{
}

void CAlien::SetAuthority( bool auth )
{

}

IActorMovementController * CAlien::CreateMovementController()
{
	return new CCompatibilityAlienMovementController(this);
}

void SAlienInput::Serialize( TSerialize ser )
{
	ser.BeginGroup("SAlienInput");
	ser.Value("deltaMovement", deltaMovement);
	ser.Value("deltaRotation", deltaRotation);
	ser.Value("actions", actions);
	ser.Value("movementVector", movementVector);
	ser.Value("viewVector", viewVector);
	ser.Value("posTarget", posTarget);
	ser.Value("dirTarget", dirTarget);
	ser.Value("upTarget", upTarget);
	ser.Value("speedTarget", speedTarget);
	ser.EndGroup();
}

void SAlienParams::Serialize( TSerialize ser )
{
	ser.BeginGroup("SAlienParams");
	ser.Value("speedInertia", speedInertia);
	ser.Value("rollAmount", rollAmount);
	ser.Value("sprintMultiplier", sprintMultiplier);
	ser.Value("sprintDuration", sprintDuration);
	ser.Value("rotSpeed_min", rotSpeed_min);
	ser.Value("rotSpeed_max", rotSpeed_max);
	ser.Value("speed_min", speed_min);
	ser.Value("forceView", forceView);
	ser.Value("movingBend", movingBend);
	ser.Value("idealSpeed", idealSpeed);
	ser.Value("blendingRatio", blendingRatio);
	ser.Value("approachLookAt", approachLookat);
	// skip string
	ser.Value("fullAnimationTentaclesBlendMult", fullAnimationTentaclesBlendMult);
	ser.Value("tentaclesCollide", tentaclesCollide);
	ser.EndGroup();
}

void SAlienStats::Serialize( TSerialize ser )
{
	ser.BeginGroup("SAlienStats");
	ser.Value("inAir", inAir);
	ser.Value("onGround", onGround);
	ser.Value("speedModule", speed);
	ser.Value("mass", mass);
	ser.Value("bobCycle", bobCycle);
	ser.Value("sprintLeft", sprintLeft);
	ser.Value("sprintThreshold", sprintTreshold);
	ser.Value("sprintMaxSpeed", sprintMaxSpeed);
	ser.Value("isThirdPerson", isThirdPerson);
	ser.Value("isRagdoll", isRagDoll);
	ser.Value("velocity", velocity);
	ser.Value("eyePos", eyePos);
	ser.Value("eyeAngles", eyeAngles);
  ser.Value("cloaked", cloaked);
	//ser.Value("dynModelOffset",dynModelOffset);
	ser.EndGroup();
}


void CAlien::UpdateDebugGraphs()
{
	bool debug = (g_pGameCVars->aln_debug_movement != 0);

	const char* filter = g_pGameCVars->aln_debug_filter->GetString();
	const char* name = GetEntity()->GetName();
	if ((strcmp(filter, "0") != 0) && (strcmp(filter, name) != 0))
		debug = false;

	if (!debug)
	{
		if (m_pDebugHistoryManager != NULL)
			m_pDebugHistoryManager->Clear();
		return;
	}

	if (m_pDebugHistoryManager == NULL)
		m_pDebugHistoryManager = g_pGame->GetIGameFramework()->CreateDebugHistoryManager();

	m_pDebugHistoryManager->LayoutHelper("EntID", NULL, true, -1000000, 1000000, 1000000, -1000000, 0.0f, 1.0f);

	bool showReqVelo = true;
	m_pDebugHistoryManager->LayoutHelper("ReqVelo", NULL, showReqVelo, -20, 20, 0, 5, 0.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("ReqVeloX", NULL, showReqVelo, -20, 20, -5, 5, 1.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("ReqVeloY", NULL, showReqVelo, -20, 20, -5, 5, 2.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("ReqVeloZ", NULL, showReqVelo, -20, 20, -5, 5, 3.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("ReqRotZ", NULL, showReqVelo, -360, 360, -5, 5, 4.0f, 0.0f);
}

void CAlien::DebugGraph_AddValue(const char* id, float value) const
{
	if (m_pDebugHistoryManager == NULL)
		return;

	if (id == NULL)
		return;

	// NOTE: It's alright to violate the const here. The player is a good common owner for debug graphs, 
	// but it's also not non-const in all places, even though graphs might want to be added from those places.
	IDebugHistory* pDH = const_cast<IDebugHistoryManager*>(m_pDebugHistoryManager)->GetHistory(id);
	if (pDH != NULL)
		pDH->AddValue(value);
}

void CAlien::GetAlienMemoryStatistics(ICrySizer * s)
{
	s->AddContainer(m_tentaclesProxy);
	s->AddContainer(m_tentaclesProxyFullAnimation);
	if (m_pBeamEffect)
		m_pBeamEffect->GetMemoryStatistics(s);
}

