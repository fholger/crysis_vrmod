/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 7:10:2004   14:48 : Created by Márcio Martins
												taken over by Filippo De Luca

*************************************************************************/
#include "StdAfx.h"
#include <StringUtils.h>
#include "Game.h"
#include "GameCVars.h"
#include "Actor.h"
#include "ScriptBind_Actor.h"
#include "ISerialize.h"
#include "GameUtils.h"
#include <ICryAnimation.h>
#include <IGameTokens.h>
#include <IItemSystem.h>
#include <IInteractor.h>
#include "Item.h"
#include "Weapon.h"
#include "Player.h"
#include "GameRules.h"
#include <IMaterialEffects.h>
#include "HUD/HUD.h"
#include "HUD/HUDRadar.h"
#include "HUD/HUDTagNames.h"
#include "IVehicleSystem.h"
#include "OffHand.h"
#include "IAgent.h"
#include "IPlayerInput.h"

#include "IFacialAnimation.h"

IItemSystem *CActor::m_pItemSystem=0;
IGameFramework	*CActor::m_pGameFramework=0;
IGameplayRecorder	*CActor::m_pGameplayRecorder=0;

SStanceInfo CActor::m_defaultStance;

//FIXME:
#define REUSE_VECTOR(table, name, value)	\
	{ if (table->GetValueType(name) != svtObject) \
	{ \
	table->SetValue(name, (value)); \
	} \
		else \
	{ \
	SmartScriptTable v; \
	table->GetValue(name, v); \
	v->SetValue("x", (value).x); \
	v->SetValue("y", (value).y); \
	v->SetValue("z", (value).z); \
	} \
	}

//int AnimEventCallback(ICharacterInstance *pCharacter, void *userdata)
//{
//	CActor *pInstance=static_cast<CActor *>(userdata);
//	if (pInstance)
//		pInstance->AnimationEvent(pCharacter, pCharacter->GetISkeleton()->GetLastAnimEvent());
//
//	return 1;
//}

//------------------------------------------------------------------------
// "W" stands for "world"
void SIKLimb::SetWPos(IEntity *pOwner,const Vec3 &pos,const Vec3 &normal,float blend,float recover,int requestID)
{
  assert(!_isnan(pos.len2()));
  assert(!_isnan(normal.len2()));
  assert(pos.len()<25000.f);

	// NOTE Dez 13, 2006: <pvl> request ID's work like priorities - if
	// the new request has an ID lower than the one currently being performed,
	// nothing happens. 
	if (requestID<blendID)
		return;

	goalWPos = pos;
	goalNormal = normal;

	if (requestID!=blendID)
	{
		blendTime = blendTimeMax = blend;
		blendID = requestID;
	}
	else if (blendTime<0.001f)
	{
		// NOTE Dez 18, 2006: <pvl> this is just a workaround way of telling
		// the Update() function that the client code called SetWPos()
		// in this frame.  Only after the client code stops calling this
		// function, the "recovery" branch of Update() starts to be
		// executed.
		blendTime = 0.0011f;
	}

	recoverTime = recoverTimeMax = recover;
}

void SIKLimb::Update(IEntity *pOwner,float frameTime)
{	
  ICharacterInstance *pCharacter = pOwner->GetCharacter(characterSlot);

	lAnimPosLast = lAnimPos;
	// pvl: the correction for translation is to be removed once character offsets are redone
//	lAnimPos = pCharacter->GetISkeleton()->GetAbsJointByID(endBoneID).t - pOwner->GetSlotLocalTM (characterSlot, false).GetTranslation ();

	Vec3 vRootBone = pCharacter->GetISkeletonPose()->GetAbsJointByID(0).t; // - pOwner->GetSlotLocalTM (characterSlot, false).GetTranslation ();
	vRootBone.z=0;
	lAnimPos = pCharacter->GetISkeletonPose()->GetAbsJointByID(endBoneID).t-vRootBone;// - pOwner->GetSlotLocalTM (characterSlot, false).GetTranslation ();
  
  assert(!_isnan(lAnimPos.len2()));

	bool setLimbPos(true);
	Vec3 finalPos=Vec3(ZERO);

	if (blendTime>0.001f)
	{
		Vec3 limbWPos = currentWPos;
		finalPos = goalWPos;

		//float middle(0.5f-fabs(0.5f - (blendTime / blendTimeMax)));
		//finalPos.z += middle * 2.0f * 5.0f;

		if (blendTime == 0.0011f) blendTime = 0.0f;

		finalPos -= (finalPos - limbWPos) * min(blendTime / blendTimeMax,1.0f);
		currentWPos = finalPos;

		blendTime -= frameTime;
	}
	else if (recoverTime>0.001f)
	{
		Vec3 limbWPos = currentWPos;
		finalPos = pOwner->GetSlotWorldTM(characterSlot) * lAnimPos;

		finalPos -= (finalPos - limbWPos) * min(recoverTime / recoverTimeMax,1.0f);
		currentWPos = finalPos;
		goalNormal.zero();

		recoverTime -= frameTime;
		
		if (recoverTime<0.001f)
			blendID = -1;
	}
	else
	{
		currentWPos = pOwner->GetSlotWorldTM(characterSlot) * lAnimPos;
		setLimbPos = false;
	}

  assert(!_isnan(finalPos.len2()));
  assert(!_isnan(goalNormal.len2()));

	if (setLimbPos)
	{
		if (flags & IKLIMB_RIGHTHAND)
			pCharacter->GetISkeletonPose()->SetHumanLimbIK(finalPos,CA_ARM_RIGHT); //SetRArmIK(finalPos);
		else if (flags & IKLIMB_LEFTHAND)
			pCharacter->GetISkeletonPose()->SetHumanLimbIK(finalPos,CA_ARM_LEFT);  //SetLArmIK(finalPos);
		else if (middleBoneID>-1)
		{
			pCharacter->GetISkeletonPose()->SetCustomArmIK(finalPos,rootBoneID,middleBoneID,endBoneID);
		}
		else
		{
			ISkeletonPose* pISkeletonPose = pCharacter->GetISkeletonPose();
			uint32 numJoints	= pISkeletonPose->GetJointCount();
			QuatT* pRelativeQuatIK = (QuatT*)alloca( numJoints*sizeof(QuatT) );
			QuatT* pAbsoluteQuatIK = (QuatT*)alloca( numJoints*sizeof(QuatT) );

			pISkeletonPose->CCDInitIKBuffer(pRelativeQuatIK,pAbsoluteQuatIK);
			pISkeletonPose->CCDInitIKChain(rootBoneID,endBoneID);

			Vec3 limbEndNormal(0,0,0);
			//limbEndNormal = Matrix33(pISkeleton->GetAbsJMatrixByID(endBoneID)).GetColumn(0);
			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(finalPos, ColorB(0,255,0,100), finalPos + limbEndNormal * 3.0f, ColorB(255,0,0,100));
			pISkeletonPose->CCDRotationSolver(finalPos,0.02f,0.25f,100,goalNormal,pRelativeQuatIK,pAbsoluteQuatIK);
			pISkeletonPose->CCDTranslationSolver(finalPos,pRelativeQuatIK,pAbsoluteQuatIK);
			pISkeletonPose->CCDUpdateSkeleton(pRelativeQuatIK,pAbsoluteQuatIK);
		}

		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(finalPos,0.2f,ColorB(0,255,255,255) );
	}
}

//--------------------
IVehicle *SLinkStats::GetLinkedVehicle()
{
	if (!linkID)
		return NULL;
	else
	{
		IVehicle *pVehicle = NULL;
		if(g_pGame && g_pGame->GetIGameFramework() && g_pGame->GetIGameFramework()->GetIVehicleSystem())
			pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(linkID);
		//if the vehicle doesnt exist and this is supposed to be a vehicle linking forget about it.
		if (!pVehicle && flags & LINKED_VEHICLE)
			UnLink();

		return pVehicle;
	}
}

void SLinkStats::Serialize(TSerialize ser)
{
	assert(ser.GetSerializationTarget() != eST_Network);

	ser.BeginGroup("PlayerLinkStats");

	//when reading, reset the structure first.
	if (ser.IsReading())
		*this = SLinkStats();

	ser.Value("linkID", linkID);
	ser.Value("flags", flags);

	ser.EndGroup();
}
//--------------------

//------------------------------------------------------------------------
CActor::CActor()
: m_pAnimatedCharacter(0)
, m_isClient(false)
, m_health(100.0f)
, m_maxHealth(0)
, m_pMovementController(0)
, m_stance(STANCE_NULL)
, m_desiredStance(STANCE_NULL)
, m_zoomSpeedMultiplier(1.0f)
, m_frozenAmount(0.f)
, m_screenEffects(0)
, m_bHasHUD(false)
, m_autoZoomInID(0)
, m_autoZoomOutID(0)
, m_saturationID(0)
, m_hitReactionID(0)
, m_inCombat(false)
, m_enterCombat(false)
, m_pGrabHandler(NULL)
,	m_teamId(0)
,	m_lastItemId(0)
, m_pInventory(0)
, m_pInteractor(0)
, m_sleepTimer(0.0f)
, m_sleepTimerOrg(0.0f)
, m_currentFootID(BONE_FOOT_L)
, m_lostHelmet(0)
, m_pWeaponAM(0)
{
	m_currentPhysProfile=GetDefaultProfile(eEA_Physics);
	//memset(&m_stances,0,sizeof(m_stances));
	//SetupStance(STANCE_NULL,&SStanceInfo());

	m_timeImpulseRecover = 0.0f;
	m_airResistance = 0.0f;
	m_airControl = 1.0f;
	m_netLastSelectablePickedUp = 0;
}

//------------------------------------------------------------------------
CActor::~CActor()
{
	ClearExtensionCache();
	GetGameObject()->SetMovementController(NULL);
	SAFE_RELEASE(m_pMovementController);

	IInventory *pInventory=GetInventory();
	if (pInventory)
	{
		if (gEnv->bServer)
			pInventory->Destroy();
		GetGameObject()->ReleaseExtension("Inventory");
	}
	
	if (m_pAnimatedCharacter)
		GetGameObject()->ReleaseExtension("AnimatedCharacter");
	GetGameObject()->ReleaseView( this );
	GetGameObject()->ReleaseProfileManager( this );

	if(m_lostHelmet)
		gEnv->pEntitySystem->RemoveEntity(m_lostHelmet);

	if(g_pGame && g_pGame->GetIGameFramework() && g_pGame->GetIGameFramework()->GetIActorSystem())
		g_pGame->GetIGameFramework()->GetIActorSystem()->RemoveActor( GetEntityId() );

	SAFE_DELETE(m_screenEffects);
	SAFE_DELETE(m_pGrabHandler);
	SAFE_DELETE(m_pWeaponAM);
}

void CActor::ClearExtensionCache()
{
	if (m_pInventory)
	{
		GetGameObject()->ReleaseExtension("Inventory");
		m_pInventory = 0;
	}
	if (m_pInteractor)
	{
		GetGameObject()->ReleaseExtension("Interactor");
		m_pInteractor = 0;
	}
}

//------------------------------------------------------------------------
bool CActor::Init( IGameObject * pGameObject )
{
	SetGameObject(pGameObject);

	if (!GetGameObject()->CaptureView(this))
		return false;
	if (!GetGameObject()->CaptureProfileManager(this))
		return false;

	m_pMovementController = CreateMovementController();
	GetGameObject()->SetMovementController(m_pMovementController);

	g_pGame->GetIGameFramework()->GetIActorSystem()->AddActor( GetEntityId(), this );

	g_pGame->GetActorScriptBind()->AttachTo(this);
	m_pAnimatedCharacter = static_cast<IAnimatedCharacter*>(GetGameObject()->AcquireExtension("AnimatedCharacter"));
	if (m_pAnimatedCharacter)
	{
		BindInputs( m_pAnimatedCharacter->GetAnimationGraphState() );
	}

	pGameObject->AcquireExtension("Inventory");

	if (!m_pGameFramework)
	{
		m_pGameFramework = g_pGame->GetIGameFramework();
		m_pGameplayRecorder = g_pGame->GetIGameFramework()->GetIGameplayRecorder();
		m_pItemSystem = m_pGameFramework->GetIItemSystem();
	}

	GetGameObject()->EnablePrePhysicsUpdate( ePPU_WhenAIActivated );

	if (!GetGameObject()->BindToNetwork())
		return false;

	GetEntity()->SetFlags(GetEntity()->GetFlags()|(ENTITY_FLAG_ON_RADAR|ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO));

	return true;
}

void CActor::PostInit( IGameObject * pGameObject )
{
	pGameObject->EnableUpdateSlot( this, 0 );	
	pGameObject->EnablePostUpdates( this );

	//activate HUD (workaround to avoid multiply HUDs in MP
	if(!m_bHasHUD && IsPlayer())
	{
		if(this->GetEntityId() == LOCAL_PLAYER_ENTITY_ID)
		{
			if(g_pGame->GetHUD())
			{
				if (g_pGameCVars->cl_hud != 0)
					g_pGame->GetHUD()->Show(true);
				m_bHasHUD = true;
			}
		}
	}

	if(gEnv->bMultiplayer)
	{
		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
		if (pCharacter)
			pCharacter->GetISkeletonPose()->SetAimIKFadeOut(0);
	}

	InitActorAttachments();
}

//----------------------------------------------------------------------
void CActor::InitActorAttachments()
{
	if(m_pWeaponAM)
		SAFE_DELETE(m_pWeaponAM);
	
	//Weapon attachments stuff
	m_pWeaponAM = new CWeaponAttachmentManager(this);
	if(!m_pWeaponAM->Init())
	{
		SAFE_DELETE(m_pWeaponAM);
	}
}

//------------------------------------------------------------------------
void CActor::HideAllAttachments(bool isHiding)
{
	if (m_pWeaponAM)
	{
		m_pWeaponAM->HideAllAttachments(isHiding);
	}
}

//------------------------------------------------------------------------
void CActor::InitClient(int channelId)
{
	if (GetHealth()<=0 && !GetSpectatorMode())
		GetGameObject()->InvokeRMI(ClSimpleKill(), NoParams(), eRMI_ToClientChannel, channelId);
}

//------------------------------------------------------------------------
void CActor::Revive( bool fromInit )
{
	ClearExtensionCache();

	if (fromInit)
		g_pGame->GetGameRules()->OnRevive(this, GetEntity()->GetWorldPos(), GetEntity()->GetWorldRotation(), m_teamId);

	//set the actor game parameters
	SmartScriptTable gameParams;
	if (GetEntity()->GetScriptTable() && GetEntity()->GetScriptTable()->GetValue("gameParams", gameParams))
		SetParams(gameParams,true);

	SetActorModel(); // set the model before physicalizing

	m_stance = STANCE_NULL;
	m_desiredStance = STANCE_NULL;

	if (gEnv->bServer)
	{
		Physicalize();
		GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
	}

	Freeze(false);

  if (IPhysicalEntity* pPhysics = GetEntity()->GetPhysics())
  {
    pe_action_move actionMove;    
    actionMove.dir.zero();
    actionMove.iJump = 1;

		pe_action_set_velocity actionVel;
		actionVel.v.zero();
		actionVel.w.zero();
    
    pPhysics->Action(&actionMove);
		pPhysics->Action(&actionVel);
  }

	m_zoomSpeedMultiplier = 1.0f;

	memset(m_boneIDs,-1,sizeof(m_boneIDs));

	if (m_pAnimatedCharacter)
		m_pAnimatedCharacter->ResetState();
	if (m_pMovementController)
		m_pMovementController->Reset();
	if (m_pGrabHandler)
		m_pGrabHandler->Reset();

	m_sleepTimer = 0.0f;
	
	m_linkStats = SLinkStats();

	m_inCombat = false;
	m_enterCombat = false;
	m_combatTimer = 0.0f;
//	m_lastFootStepPos = ZERO;
	m_rightFoot = true;
	m_pReplacementMaterial = 0;

	m_frozenAmount = 0.0f;

	if (m_screenEffects)
		m_screenEffects->ClearAllBlendGroups(true);

  if (IsClient())
    gEnv->p3DEngine->ResetPostEffects();

	//reset some AG inputs
	if (m_pAnimatedCharacter)
	{
		UpdateAnimGraph(m_pAnimatedCharacter->GetAnimationGraphState());
		m_pAnimatedCharacter->GetAnimationGraphState()->SetInput("Action", "idle" );
		m_pAnimatedCharacter->GetAnimationGraphState()->SetInput("waterLevel", 0 );
		m_pAnimatedCharacter->SetParams( m_pAnimatedCharacter->GetParams().ModifyFlags(eACF_EnableMovementProcessing,0));
		m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( m_inputHealth, GetMaxHealth() );
	}

//	m_footstepAccumDistance = 0.0f;

	ResetHelmetAttachment();

	if (ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0))
		pCharacter->EnableProceduralFacialAnimation(GetMaxHealth() > 0);

	if (IEntityPhysicalProxy* pPProxy = (IEntityPhysicalProxy*)GetEntity()->GetProxy(ENTITY_PROXY_PHYSICS))
		pPProxy->EnableRestrictedRagdoll(false);
}

IGrabHandler *CActor::CreateGrabHanlder()
{
	m_pGrabHandler = new CBaseGrabHandler(this);
	return m_pGrabHandler;
}

//------------------------------------------------------------------------
void CActor::Physicalize(EStance stance)
{
	//FIXME:this code is duplicated from scriptBind_Entity.cpp, there should be a function that fill a SEntityPhysicalizeParams struct from a script table.
	IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
  assert(pScriptTable);
  if (!pScriptTable)
    return;

  SmartScriptTable physicsParams;
  if (pScriptTable->GetValue("physicsParams", physicsParams))
	{
		//first, the actor model has to be loaded, this is still using lua for the moment.
		//ICharacterInstance *pChar = GetEntity()->GetCharacter(0);
		//pChar->GetModelFilePath()

		SetActorModel();

		pe_player_dimensions playerDim;
		pe_player_dynamics playerDyn;
		SEntityPhysicalizeParams pp;
		
		pp.pPlayerDimensions = &playerDim;
		pp.pPlayerDynamics = &playerDyn;

		pp.nSlot = 0;
		pp.type = PE_LIVING;

		physicsParams->GetValue("mass",pp.mass);
		physicsParams->GetValue("density",pp.density);
		physicsParams->GetValue("flags",pp.nFlagsOR);
		physicsParams->GetValue("partid",pp.nAttachToPart);
		physicsParams->GetValue("stiffness_scale",pp.fStiffnessScale);


		SmartScriptTable props;
		if(GetEntity()->GetScriptTable()->GetValue("Properties", props))
		{
			float massMult = 1.0f;
			props->GetValue("physicMassMult", massMult);
			pp.mass *= massMult;
		}

		SmartScriptTable livingTab;
		if (physicsParams->GetValue( "Living",livingTab ))
		{
			// Player Dimensions
			if (stance==STANCE_NULL)
			{
				livingTab->GetValue( "height",playerDim.heightCollider );
				livingTab->GetValue( "size",playerDim.sizeCollider );
				livingTab->GetValue( "height_eye",playerDim.heightEye );
				livingTab->GetValue( "height_pivot",playerDim.heightPivot );
				livingTab->GetValue( "use_capsule",playerDim.bUseCapsule );
			}
			else
			{
				const SStanceInfo *sInfo = GetStanceInfo(stance);
				playerDim.heightCollider = sInfo->heightCollider;
				playerDim.sizeCollider = sInfo->size;
				playerDim.heightPivot = sInfo->heightPivot;
				playerDim.maxUnproj = max(0.0f,sInfo->heightPivot);
				playerDim.bUseCapsule = sInfo->useCapsule;
			}

			playerDim.headRadius = 0.0f;
			playerDim.heightEye = 0.0f;

			// Player Dynamics.
			livingTab->GetValue( "inertia",playerDyn.kInertia );
			livingTab->GetValue( "k_air_control",playerDyn.kAirControl);
			livingTab->GetValue( "inertiaAccel",playerDyn.kInertiaAccel );
			livingTab->GetValue( "air_resistance",playerDyn.kAirResistance );
			livingTab->GetValue( "gravity",playerDyn.gravity.z );
			livingTab->GetValue( "mass",playerDyn.mass );
			livingTab->GetValue( "min_slide_angle",playerDyn.minSlideAngle );
			livingTab->GetValue( "max_climb_angle",playerDyn.maxClimbAngle );
			livingTab->GetValue( "max_jump_angle",playerDyn.maxJumpAngle );
			livingTab->GetValue( "min_fall_angle",playerDyn.minFallAngle );
			livingTab->GetValue( "max_vel_ground",playerDyn.maxVelGround );
			livingTab->GetValue( "timeImpulseRecover",playerDyn.timeImpulseRecover );

			// for MP allow players to stand on fast moving surfaces (specifically moving vehicles, but will apply to everything)
			if(gEnv->bMultiplayer)
				playerDyn.maxVelGround = 200.0f;

			SActorParams* params = GetActorParams();


			if(!is_unused(playerDyn.timeImpulseRecover))
				m_timeImpulseRecover = playerDyn.timeImpulseRecover;
			else
				m_timeImpulseRecover = 0.0f;

			if(!is_unused(playerDyn.kAirResistance))
				m_airResistance = playerDyn.kAirResistance;
			else
				m_airResistance = 0.0f;

			if(!is_unused(playerDyn.kAirControl))
				m_airControl = playerDyn.kAirControl;
			else
				m_airControl = 1.0f;

			const char *colliderMat=0;
			if (livingTab->GetValue( "colliderMat", colliderMat) && colliderMat && colliderMat[0])
			{
				if (ISurfaceType *pSurfaceType=gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeByName(colliderMat))
					playerDyn.surface_idx=pSurfaceType->GetId();
			}
		}

		if (IPhysicalEntity *pPrevPE=GetEntity()->GetPhysics())
		{
			if (GetEntity()->GetParent() == NULL)
			{
				Ang3 rot(GetEntity()->GetWorldAngles());
				GetEntity()->SetRotation(Quat::CreateRotationZ(rot.z));
			}

			SEntityPhysicalizeParams nop;
			nop.type = PE_NONE;
			GetEntity()->Physicalize(nop);
		}

		GetEntity()->Physicalize(pp);
	}

	// for the client we add an additional proxy for bending vegetation to look correctly
	if (IsPlayer())
	{
		primitives::capsule prim;

		prim.axis.Set(0,0,1);
		prim.center.zero(); prim.r = 0.4f; prim.hh = 0.2f;
		IGeometry *pPrimGeom = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::capsule::type, &prim);
		phys_geometry *pGeom = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(pPrimGeom, 0);
		pe_geomparams gp;
 		gp.pos = Vec3(0.0f,0.2f,0.7f);
		gp.flags = geom_colltype_foliage;
		gp.flagsCollider = 0;
		pGeom->nRefCount = 0;
		//just some arbitrary id, except 100, which is the main cylinder
		GetEntity()->GetPhysics()->AddGeometry(pGeom, &gp, 101);
	}

	//the finish physicalization
	PostPhysicalize();
}

//
void CActor::SetActorModel()
{
	// this should be pure-virtual, but for the moment to support alien scripts
  if (IScriptTable* pScriptTable = GetEntity()->GetScriptTable())
	  Script::CallMethod(pScriptTable, "SetActorModel", IsClient());
}

//------------------------------------------------------------------------
void CActor::PostPhysicalize()
{
	//force the physical proxy to be rebuilt
	m_stance = STANCE_NULL;
	SetStance(STANCE_STAND);

	GetGameObject()->RequestRemoteUpdate(eEA_Physics | eEA_GameClientDynamic | eEA_GameServerDynamic | eEA_GameClientStatic | eEA_GameServerStatic);

//	if (m_pAnimatedCharacter)
//		m_pAnimatedCharacter->ResetState();

	if (IsPlayer())
	{
		IEntityRenderProxy *pRenderProxy = static_cast<IEntityRenderProxy *>(GetEntity()->GetProxy(ENTITY_PROXY_RENDER));

		if (pRenderProxy)
		{
			IRenderNode *pRenderNode = pRenderProxy?pRenderProxy->GetRenderNode():0;

			if (pRenderNode)
			{
				pRenderNode->SetViewDistRatio(255);
				pRenderNode->SetLodRatio(80); //IVO: changed to fix LOD problem in MP
			}
		}
	}
}

//------------------------------------------------------------------------
void CActor::SetZoomSpeedMultiplier(float m)
{
	m_zoomSpeedMultiplier=m;
}

//------------------------------------------------------------------------
float CActor::GetZoomSpeedMultiplier() const
{
	return m_zoomSpeedMultiplier;
}

//------------------------------------------------------------------------
void CActor::RagDollize( bool fallAndPlay )
{
	if (GetLinkedVehicle())
		return;

	SActorStats *pStats = GetActorStats();
	
	if (pStats && (!pStats->isRagDoll || gEnv->pSystem->IsSerializingFile()))
	{
		GetGameObject()->SetAutoDisablePhysicsMode(eADPM_Never);

		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
		if (pCharacter)
		{
			// dead guys shouldn't blink
			pCharacter->EnableProceduralFacialAnimation(false);
			//Anton :: SetDefaultPose on serialization
			if(gEnv->pSystem->IsSerializingFile() && pCharacter->GetISkeletonPose())
				pCharacter->GetISkeletonPose()->SetDefaultPose();
		}

		SEntityPhysicalizeParams pp;

		pp.type = PE_ARTICULATED;
		pp.nSlot = 0;
		if(SActorStats* stats = GetActorStats())
			pp.mass = stats->mass;
		if(pp.mass <= 0)
			pp.mass = 80.0f; //never ragdollize without mass [Anton]

		if (fallAndPlay)
			pp.fStiffnessScale = 1200;

		pe_player_dimensions playerDim;
		pe_player_dynamics playerDyn;

		playerDyn.gravity.z = 15.0f;
		playerDyn.kInertia = 5.5f;

		pp.pPlayerDimensions = &playerDim;
		pp.pPlayerDynamics = &playerDyn;

		IPhysicalEntity *pPhysicalEntity=GetEntity()->GetPhysics();
		if (!pPhysicalEntity || pPhysicalEntity->GetType()!=PE_LIVING)
			pp.nLod = 1;

		GetEntity()->Physicalize(pp);

		pStats->isRagDoll = true;

		if (fallAndPlay)
		{
			IAnimationGraphState *animGraph = GetAnimationGraphState();
			if (animGraph)
				animGraph->PushForcedState( "FallAndPlay" );
				//animGraph->SetInput("Signal", "fall");
		}

		// [Mikko] 12.10.2007 Skipping the timer reset here or else QL tranquilized characters does not work.
		// Setting sleep timer here is a bug. SetAspectProfile should only affect the actor
		// physicalization and not have any other side effects.
		if (!gEnv->pSystem->IsSerializingFile())
			m_sleepTimer=0.0f;
	}
}

//------------------------------------------------------------------------
bool CActor::IsFallen() const
{
	const SActorStats *pStats = GetActorStats();
	return (pStats && pStats->isRagDoll || m_sleepTimer > 0.0f) && GetHealth() > 0;
}

//------------------------------------------------------------------------
int CActor::GetFallenTime() const
{
	const SActorStats *pStats = GetActorStats();
	if(pStats && pStats->isRagDoll && pStats->timeFallen && GetHealth() > 0)
		return int((gEnv->pTimer->GetFrameStartTime().GetSeconds() - pStats->timeFallen)*1000.0f);
	return 0;
}

//------------------------------------------------------------------------
void CActor::Fall(Vec3 hitPos, bool forceFall, float sleepTime /*=0.0f*/)
{
	// player doesn't fall in single-player for now
	if(IsPlayer() && !gEnv->bMultiplayer)
		if(!forceFall)
			return;

	//Aliens don't support fall&play, please ragdollize instead
	if(IsAlien())
		return;

	if(IsFallen())
		return;

	bool inVehicle = GetLinkedVehicle() != NULL;
	if ( inVehicle == true )
		return;

	SActorStats *pStats = GetActorStats();
	if(pStats && pStats->inZeroG)
		return;

	//we don't want noDeath (tutorial) AIs to loose their weapon, since we don't have pickup animations yet
	bool	dropWeapon(true);
	bool  hasDamageTable = false;
	SmartScriptTable props;
	SmartScriptTable propsDamage;
	if(GetEntity()->GetScriptTable() && GetEntity()->GetScriptTable()->GetValue("Properties", props))
		if(props->GetValue("Damage", propsDamage))
			hasDamageTable = true;

	if(!hasDamageTable)
		return;

	int noDeath(0);
	int	fallPercentage(0);
	if(	propsDamage->GetValue("bNoDeath", noDeath) && noDeath!=0 ||
		propsDamage->GetValue("FallPercentage", fallPercentage) && fallPercentage>0 )
		dropWeapon = false;

	IAISystem *pAISystem=gEnv->pAISystem;
	if (pAISystem)
	{
		if(IEntity* pEntity=GetEntity())
		{
			if(IAIObject* pAIObj=pEntity->GetAI())
			{
				IAIActor* pAIActor = CastToIAIActorSafe(GetEntity()->GetAI());
				if(pAIActor)
				{
					IAISignalExtraData *pEData = pAISystem->CreateSignalExtraData();	// no leak - this will be deleted inside SendAnonymousSignal
					pEData->point = Vec3(0,0,0);
					pAIActor->SetSignal(1,"OnFallAndPlay",0,pEData);
				}
			}
		}
	}

	CreateScriptEvent("sleep", 0);
	if ( inVehicle )
	{
		if ( IAnimationGraphState *animGraph = GetAnimationGraphState() )
			animGraph->SetInput( "Signal", "fall" );
	}
	else
	{
		GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Sleep);
	}

	//Do we want this for the player? (Sure not for AI)
	if(IsPlayer() && dropWeapon && !inVehicle)
	{
		DropItem(GetCurrentItemId(), 1.0f, false);
		if (GetCurrentItemId(false))
			HolsterItem(true);
	}

	// stop shooting
	if ( EntityId currentItem = GetCurrentItemId(true) )
		if ( CWeapon* pWeapon = GetWeapon(currentItem) )
			pWeapon->StopFire();

	//add some twist
	if(!IsClient() && hitPos.len() && !inVehicle)
	{
		if(IPhysicalEntity *pPE = GetEntity()->GetPhysics())
		{
			pe_action_impulse imp;

			float angularDir = 1.0f;
			if(IMovementController *pMC = GetMovementController())
			{
				//where was the hit ?
				SMovementState sMovementState;
				pMC->GetMovementState(sMovementState);
				Vec3 rightDir = sMovementState.eyeDirection.Cross(sMovementState.upDirection);
				Vec3 dir = hitPos - GetEntity()->GetWorldPos();
				float right = dir.Dot(rightDir);
				float front = sMovementState.eyeDirection.Dot(dir);

				if(right > 0)
					angularDir = -1.0f;
				if(front < 0)
					angularDir *= -1.0f;
			}

			imp.impulse = Vec3(0, 0, -200.0);
			imp.angImpulse = Vec3(0.0f, 0.0f, angularDir*200.0f);
			pPE->Action(&imp);
		}
	}
	
	float r = cry_frand();
	if(r > 0.5f && r < 0.6f)
		LooseHelmet();

	float sleep(3.0f);
	if(sleepTime>0.0f)
		SetSleepTimer(sleepTime);
	else if(!propsDamage->GetValue("FallSleepTime", sleepTime))
		SetSleepTimer(3.0f);
	else if(IsClient())
		SetSleepTimer(3.0f);
	else
		SetSleepTimer(sleep);

	if(pStats)
		pStats->timeFallen = gEnv->pTimer->GetFrameStartTime().GetSeconds();

	IAnimationGraphState* pAGState = GetAnimationGraphState();
	if ( pAGState && m_pAnimatedCharacter )
	{
		ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
		if ( pCharacter )
		{
			ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
			if ( pSkeletonPose )
			{
				int fnpGroudId = 0;
				IAnimationSet* pAnimSet = pCharacter->GetIAnimationSet();
				if ( pAnimSet )
				{
					char stance[256];
					pAGState->GetInput( m_inputStance, stance );
					if ( stance[0] == 0 )
						strcpy( stance, "combat" );
					char item[256];
					pAGState->GetInput( pAGState->GetInputId( "Item" ), item );
					if ( item[0] == 0 )
						strcpy( item, "nw" );

					int maxScore = 0;
					const char* groupName = NULL;
					for ( int i = 0; (groupName = pAnimSet->GetFnPAnimGroupName(i)) != NULL && groupName[0] != 0; ++i )
					{
						int score = 0;
						if ( CryStringUtils::stristr( groupName, item ) != NULL )
							score += 2;
						if ( CryStringUtils::stristr( groupName, stance ) != NULL )
							score += 1;
						if ( score > maxScore )
						{
							maxScore = score;
							fnpGroudId = i;
						}
					}
				}
				pSkeletonPose->SetFnPAnimGroup( fnpGroudId );
			}
		}
	}
}

//------------------------------------------------------------------------
void CActor::GoLimp()
{
	ICharacterInstance *pCharacter=GetEntity()->GetCharacter(0);
	if (pCharacter && pCharacter->GetISkeletonAnim())
		pCharacter->GetISkeletonPose()->GoLimp();
}

//------------------------------------------------------------------------
void CActor::StandUp()
{
	if ( GetHealth() <= 0 )
	{
		GoLimp();
	}
	else
	{
		// if m_sleepTimer > 0.0f it means waking up is scheduled already for later so we just ignore this for now!
		if ( m_sleepTimer > 0.0f )
			return;

		if ( GetLinkedVehicle() )
			GetAnimationGraphState()->Hurry();
		else if ( m_currentPhysProfile == eAP_Sleep )
			GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
	}

	if(SActorStats *pStats = GetActorStats())
		pStats->timeFallen = 0;
}

void CActor::NotifyLeaveFallAndPlay()
{
	IAIObject* pAI;
	if(GetEntity() && (pAI = GetEntity()->GetAI()))
	{
		pAI->Event(AIEVENT_WAKEUP, 0);
		gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER, 1, "OnFallAndPlayWakeUp", pAI);
	}
}

//------------------------------------------------------------------------
void CActor::SetupStance(EStance stance,SStanceInfo *info)
{
	if (stance >= STANCE_NULL && stance < STANCE_LAST)
		memcpy((void *)GetStanceInfo(stance),info,sizeof(SStanceInfo));
}

//------------------------------------------------------
void CActor::SetStance(EStance desiredStance)
{
	m_desiredStance = desiredStance;

/*
	//Player should not change stance if the physical cylinder collider can not change too
	if (desiredStance != m_stance)
	{
		if (!TrySetStance(desiredStance))
			return;
	}

	if (m_pAnimatedCharacter && !m_pAnimatedCharacter->InStanceTransition() && (desiredStance != m_stance))
		m_pAnimatedCharacter->RequestStance( desiredStance, GetStanceInfo(desiredStance)->name );
*/
}


//------------------------------------------------------
IEntity *CActor::LinkToVehicle(EntityId vehicleId) 
{
	// did this link actually change, or are we just re-linking?
	bool changed=((m_linkStats.linkID!=vehicleId)||gEnv->pSystem->IsSerializingFile())?true:false;

	m_linkStats = SLinkStats(vehicleId,LINKED_VEHICLE);
	
	IVehicle *pVehicle = m_linkStats.GetLinkedVehicle();
	IEntity *pLinked = pVehicle?pVehicle->GetEntity():NULL;
  
	if (m_pAnimatedCharacter)
	{
		SAnimatedCharacterParams params = m_pAnimatedCharacter->GetParams();
		
		bool enabled = pLinked?true:false;
		if (enabled)
		{
//			params.flags &= ~eACF_EnableMovementProcessing;
			params.flags |= eACF_NoLMErrorCorrection;
		}
		else
		{
//			params.flags |= eACF_EnableMovementProcessing;
			params.flags &= ~eACF_NoLMErrorCorrection;
		}
		
		if(gEnv->bServer)
		{
			if(enabled)
			{
				if (changed)
					GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Linked);
			}
			else if(IPhysicalEntity *pPhys = GetEntity()->GetPhysics())
			{
				pe_type type = pPhys->GetType();
				if(type == PE_LIVING)
					GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
				else if(type == PE_ARTICULATED)
					GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
			}
		}

		m_pAnimatedCharacter->SetParams( params );
		m_pAnimatedCharacter->ForceRefreshPhysicalColliderMode();
		m_pAnimatedCharacter->RequestPhysicalColliderMode(enabled ? eColliderMode_Disabled : eColliderMode_Undefined, eColliderModeLayer_Game, "Actor::LinkToVehicle");
	}
  
  if (pLinked)  
    pLinked->AttachChild(GetEntity(), ENTITY_XFORM_USER|IEntity::ATTACHMENT_KEEP_TRANSFORMATION);
  else
    GetEntity()->DetachThis(IEntity::ATTACHMENT_KEEP_TRANSFORMATION,/*ENTITY_XFORM_USER*/0);
  
	return pLinked;
}

IEntity *CActor::LinkToVehicleRemotely(EntityId vehicleId)
{
	m_linkStats = SLinkStats(vehicleId,LINKED_VEHICLE);

	return m_linkStats.GetLinked();
}

IEntity *CActor::LinkToEntity(EntityId entityId, bool bKeepTransformOnDetach) 
{
	m_linkStats = SLinkStats(entityId,LINKED_FREELOOK);

	IEntity *pLinked = m_linkStats.GetLinked();

	if (m_pAnimatedCharacter)
	{
		SAnimatedCharacterParams params = m_pAnimatedCharacter->GetParams();

		bool enabled = pLinked?true:false;
		if (enabled)
		{
			params.flags &= ~eACF_EnableMovementProcessing;
			params.flags |= eACF_NoLMErrorCorrection;
		}
		else
		{
			params.flags |= eACF_EnableMovementProcessing;
			params.flags &= ~eACF_NoLMErrorCorrection;
		}

		m_pAnimatedCharacter->SetParams( params );
		m_pAnimatedCharacter->ForceRefreshPhysicalColliderMode();
		m_pAnimatedCharacter->RequestPhysicalColliderMode(enabled ? eColliderMode_Disabled : eColliderMode_Undefined, eColliderModeLayer_Game, "Actor::LinkToEntity");
	}

  if (pLinked)
    pLinked->AttachChild(GetEntity(), 0);
  else
		GetEntity()->DetachThis(bKeepTransformOnDetach ? IEntity::ATTACHMENT_KEEP_TRANSFORMATION : 0, bKeepTransformOnDetach ? ENTITY_XFORM_USER : 0);

	return pLinked;
}

void CActor::ProcessEvent(SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_HIDE:
	case ENTITY_EVENT_INVISIBLE:
		{
			IItem *pItem=GetCurrentItem();
			if (pItem)
				pItem->GetEntity()->Hide(true);
			if(!IsPlayer() && m_pWeaponAM)
				m_pWeaponAM->HideAllAttachments(true);
		}	
		break;
	case ENTITY_EVENT_UNHIDE:
	case ENTITY_EVENT_VISIBLE:
		{
			IItem *pItem=GetCurrentItem();
			if (pItem)
				pItem->GetEntity()->Hide(false);
			if(!IsPlayer() && m_pWeaponAM)
				m_pWeaponAM->HideAllAttachments(false);
			GetGameObject()->RequestRemoteUpdate(eEA_Physics | eEA_GameClientDynamic | eEA_GameServerDynamic | eEA_GameClientStatic | eEA_GameServerStatic);
		}	
		break;
	case ENTITY_EVENT_START_GAME:
		UpdateAnimGraph( m_pAnimatedCharacter? m_pAnimatedCharacter->GetAnimationGraphState() : 0 );
		GetGameObject()->RequestRemoteUpdate(eEA_Physics | eEA_GameClientDynamic | eEA_GameServerDynamic | eEA_GameClientStatic | eEA_GameServerStatic);
		break;
  case ENTITY_EVENT_RESET:
    Reset(event.nParam[0]==1);
		GetGameObject()->RequestRemoteUpdate(eEA_Physics | eEA_GameClientDynamic | eEA_GameServerDynamic | eEA_GameClientStatic | eEA_GameServerStatic);
    break;
	case ENTITY_EVENT_ANIM_EVENT:
		{
			const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
			ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(event.nParam[1]);
			if (pAnimEvent && pCharacter)
				AnimationEvent(pCharacter, *pAnimEvent);
		}
		break;
  }  
}

void CActor::Update(SEntityUpdateContext& ctx, int slot)
{
	Vec3 cp=GetEntity()->GetWorldPos();
	//CryLogAlways("%s::Update(current: %.2f,%.2f,%.2f)", GetEntity()->GetName(), cp.x,cp.y,cp.z);
	// ScreenFX only on client actor
	if (IsClient() && m_screenEffects == 0)
	{
		m_screenEffects = new CScreenEffects(this);
		m_autoZoomInID = m_screenEffects->GetUniqueID();
		m_autoZoomOutID = m_screenEffects->GetUniqueID();
		m_saturationID = m_screenEffects->GetUniqueID();
		m_hitReactionID = m_screenEffects->GetUniqueID();
	}

	if (GetEntity()->IsHidden() && !(GetEntity()->GetFlags() & ENTITY_FLAG_UPDATE_HIDDEN))
		return;
  
	if (m_sleepTimer>0.0f && gEnv->bServer)
	{
		pe_status_dynamics dynStat;
		dynStat.submergedFraction = 0;
		if (GetLinkedVehicle())
		{
			m_sleepTimer-=ctx.fFrameTime;
		}
		else
		{
			IPhysicalEntity* pEnt = GetEntity()->GetPhysics();
			dynStat.nContacts = 0;
			if (!pEnt || !pEnt->GetStatus(&dynStat) || dynStat.nContacts>2 || dynStat.energy<100.0f || dynStat.submergedFraction>0.1f)
				m_sleepTimer-=ctx.fFrameTime;
			if (dynStat.nContacts>=4 && dynStat.energy<dynStat.mass*sqr(0.3f) && m_sleepTimer<m_sleepTimerOrg-1)
				if (m_sleepTimerOrg>10.0f) // tranquilized mode
				{
					if (m_sleepTimer<m_sleepTimerOrg-2)
					{
						ICharacterInstance *pCharacter=GetEntity()->GetCharacter(0);
						if (pCharacter && pCharacter->GetISkeletonPose())
							pCharacter->GetISkeletonPose()->GoLimp();
					}
				} else
					m_sleepTimer = 0;
		}
		//float waterLevel = gEnv->p3DEngine->GetWaterLevel(cp);
		if (m_sleepTimer<=0.0f)
		{
			m_sleepTimer=0.0f;
			if(!(IsClient() && g_pGame->GetHUD() && g_pGame->GetHUD()->IsFakeDead()))	//fake death handles standup seperately
			{
				if (dynStat.submergedFraction>0.5f)
				{
					float waterLevel = gEnv->p3DEngine->GetWaterLevel(&cp);
					float terrainLevel = gEnv->p3DEngine->GetTerrainElevation(cp.x,cp.y);
					//Prevent AI dying in shallow water
					if(waterLevel>(terrainLevel+0.5f))
					{
						SetHealth(0);
						CreateScriptEvent("kill",0);

						// drop item is not managed inside kill event
						IItem* currentItem = GetCurrentItem();
						if(currentItem)
							DropItem(currentItem->GetEntityId());
					}
					else 
						StandUp();
				}
				else
					StandUp();
			}
		}
	}

	if (m_frozenAmount>0.f && !IsFrozen())
		SetFrozenAmount(m_frozenAmount - g_pGameCVars->g_frostDecay*ctx.fFrameTime);
  
/*
	// remove this if AI is not supposed to unfreeze
	if (m_frozenAmount>0.0f && m_pGameFramework->IsServer() && !IsPlayer())
	{
		m_frozenAmount=CLAMP(m_frozenAmount-ctx.fFrameTime/5.0f, 0.0f, 1.0f); // max 3secs frozen (will be reduced by sworkarounding mouse)
		if (m_frozenAmount<=0.0f)
			g_pGame->GetGameRules()->FreezeEntity(GetEntityId(), false, false);
	}
*/
	UpdateZeroG(ctx.fFrameTime);
	
	if (GetHealth() > 0.0f)
		UpdateFootSteps(ctx.fFrameTime);
	
	//if (!m_pAnimatedCharacter)
	//	GameWarning("%s has no AnimatedCharacter!", GetEntity()->GetName());

	if (GetHealth() > 0.0f || (gEnv->bMultiplayer && GetSpectatorMode() != eASM_None))
	{
		// Only update stance for alive characters. Dead characters never request any stance changes
		// but if a different stance is requested for whatever reason (currently it happens after QL)
		// and the animation graph has different death animations for different stances (like for the
		// Hunter currently) then some other death animation may play again to better match the state.
		
		// NOTE: MP spectators seem to need their stance updated (they are dead) otherwise the spectator cam
		//	intersects with the environment.
		
		UpdateStance();
	}

	float threat = 0;
	if (gEnv->pAISystem)
	{
		SAIDetectionLevels levels;
		gEnv->pAISystem->GetDetectionLevels(0, levels);
		threat = max(levels.vehicleThreat, levels.puppetThreat);
	}

	if (threat >= 0.9f)
		InitiateCombat();

	if (m_combatTimer > 0.0f)
	{
		if (m_enterCombat)
		{
			m_enterCombat = false;
			m_inCombat = true;
			//// Entered combat, do an effect if we're the client
			//if (IsClient() && GetScreenEffects() != 0)
			//{
			//	CWaveBlend *blend = new CWaveBlend();
			//	CPostProcessEffect *effect = new CPostProcessEffect(GetEntityId(),"Global_Saturation", .8f);
			//	GetScreenEffects()->ClearBlendGroup(m_saturationID, false);
			//	GetScreenEffects()->StartBlend(effect, blend, 1.0f/15.0f, m_saturationID);
			//}
		}
		
		m_combatTimer -= ctx.fFrameTime;
	}
	
	if (m_combatTimer < 0.0f)
	{
		m_combatTimer = 0.0f;
		m_inCombat = false;
		//// Not in combat
		//if (IsClient() && GetScreenEffects() != 0)
		//{
		//	CWaveBlend *blend = new CWaveBlend();
		//	CPostProcessEffect *effect = new CPostProcessEffect(GetEntityId(), "Global_Saturation", 1.0f);
		//	GetScreenEffects()->ClearBlendGroup(m_saturationID, false);
		//	GetScreenEffects()->StartBlend(effect, blend, 1.0f/5.0f, m_saturationID);
		//}
	}
	//should be this at the end of the update loop?
	//if yes, a PostUpdate function should be created.
	if (IsClient())
	{
		if (m_screenEffects != 0)
		{
			m_screenEffects->Update(ctx.fFrameTime);
		}
	}
	
	// NOTE Sep 13, 2007: <pvl> UpdateGrab() moved into an animation system callback -
	// due to complexities in update ordering previous frame's bone positions
	// were still used by the GrabHandler when updated from here.
	//UpdateGrab(ctx.fFrameTime);
	UpdateAnimGraph( m_pAnimatedCharacter?m_pAnimatedCharacter->GetAnimationGraphState():NULL );

	//
	// get stats table
	if (!m_actorStats)
	{
		IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
		if (pScriptTable)
			pScriptTable->GetValue("actorStats", m_actorStats);
	}
	UpdateScriptStats(m_actorStats);

	EntityId currentItemId=GetCurrentItemId();
	if (currentItemId!=m_lastItemId)
	{
		HSCRIPTFUNCTION pfnCurrentItemChanged=0;
		IScriptTable *pTable=GetEntity()->GetScriptTable();
		if (pTable && (pTable->GetValueType("CurrentItemChanged")==svtFunction) &&
			pTable->GetValue("CurrentItemChanged", pfnCurrentItemChanged))
		{
			Script::CallMethod(pTable, pfnCurrentItemChanged, ScriptHandle(currentItemId), ScriptHandle(m_lastItemId));
			gEnv->pScriptSystem->ReleaseFunc(pfnCurrentItemChanged);
		}

		m_lastItemId=currentItemId;
	}
}

void CActor::UpdateScriptStats(SmartScriptTable &rTable)
{
	CScriptSetGetChain stats(rTable);
	stats.SetValue("stance",m_stance);
	stats.SetValue("thirdPerson",IsThirdPerson());

	SActorStats *pStats = GetActorStats();
	if (pStats)
	{
		//REUSE_VECTOR(rTable, "velocity", pStats->velocity);
	
		stats.SetValue("inAir",pStats->inAir);
		stats.SetValue("onGround",pStats->onGround);

		//stats.SetValue("inWater",pStats->inWater);
		//pStats->headUnderWater.SetDirtyValue(stats, "headUnderWater");
		//stats.SetValue("waterLevel",pStats->waterLevel);
		//stats.SetValue("bottomDepth",pStats->bottomDepth);

		stats.SetValue("flatSpeed",pStats->speedFlat);
		//stats.SetValue("speedModule",pStats->speed);

		stats.SetValue("godMode",IsGod());
		stats.SetValue("inFiring",pStats->inFiring);		
		pStats->inFreefall.SetDirtyValue(stats, "inFreeFall");
		pStats->isHidden.SetDirtyValue(stats, "isHidden");
		pStats->isShattered.SetDirtyValue(stats, "isShattered");
	}
}

bool CActor::UpdateStance()
{
	if (m_stance != m_desiredStance)
	{
		// If character is animated, postpone stance change until state transition is finished (i.e. in steady state).
		if ((m_pAnimatedCharacter != NULL) && m_pAnimatedCharacter->InStanceTransition())
			return false;

		if (!TrySetStance(m_desiredStance))
			return false;

		StanceChanged(m_stance);

		m_stance = m_desiredStance;

		// Request new animation stance.
		// AnimatedCharacter has it's own understanding of stance, which might be in conflict.
		// Ideally the stance should be maintained only in one place. Currently the Actor (gameplay) rules.
		if (m_pAnimatedCharacter != NULL)
			m_pAnimatedCharacter->RequestStance(m_stance, GetStanceInfo(m_stance)->name);

		IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();
		if (pPhysEnt != NULL)
		{
			pe_action_awake aa;
			aa.bAwake = 1;
			pPhysEnt->Action(&aa);
		}
	}

	return true;
}

//------------------------------------------------------

bool CActor::TrySetStance(EStance stance)
{
	//  if (stance == STANCE_NULL)
	//	  return true;

	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();
	int result = 0;
	if (pPhysEnt)
	{
		const SStanceInfo *sInfo = GetStanceInfo(stance);

		pe_player_dimensions playerDim;
		playerDim.heightEye = 0.0f;
		playerDim.heightCollider = sInfo->heightCollider;
		playerDim.sizeCollider = sInfo->size;
		playerDim.heightPivot = sInfo->heightPivot;
		playerDim.maxUnproj = max(0.0f,sInfo->heightPivot);
		playerDim.bUseCapsule = sInfo->useCapsule;

		result = pPhysEnt->SetParams(&playerDim);
	}

	return (result != 0);
}



void CActor::SetStats(SmartScriptTable &rTable)
{
	SActorStats *pStats = GetActorStats();
	if (pStats)
	{
		rTable->GetValue("inFiring",pStats->inFiring);
	}
}

//------------------------------------------------------------------------
void CActor::OnAction(const ActionId& actionId, int activationMode, float value)
{
	IItem *pItem = GetCurrentItem();
	if (pItem)
		pItem->OnAction(GetGameObject()->GetEntityId(), actionId, activationMode, value);
}

//------------------------------------------------------------------------
void CActor::CreateScriptEvent(const char *event,float value,const char *str)
{
	IEntity *pEntity = GetEntity(); 
  IScriptTable* pScriptTable = pEntity ? pEntity->GetScriptTable() : 0;

	if (pScriptTable)
	{
		HSCRIPTFUNCTION scriptEvent(NULL);	
		pScriptTable->GetValue("ScriptEvent", scriptEvent);

		if (scriptEvent)
			Script::Call(gEnv->pScriptSystem,scriptEvent,pScriptTable,event,value,str);

		gEnv->pScriptSystem->ReleaseFunc(scriptEvent);
	}
}

bool CActor::CreateCodeEvent(SmartScriptTable &rTable)
{
	const char *event = NULL;
  if (!rTable->GetValue("event",event))
    return false;

	if (!strcmp(event,"grabObject"))
	{
		if (!m_pGrabHandler)
			CreateGrabHanlder();

		if (m_pGrabHandler)
			return m_pGrabHandler->SetGrab(rTable);
	}
	else if (!strcmp(event,"dropObject"))
	{
		if (m_pGrabHandler)
			return m_pGrabHandler->SetDrop(rTable);
	}
	else if (!strcmp(event,"replaceMaterial"))
	{
		const char *strMat = NULL;
		rTable->GetValue("material",strMat);
		ReplaceMaterial(strMat);

    int cloaked = 0;
    if (rTable->GetValue("cloak", cloaked))
      OnCloaked(cloaked!=0);

    return true;
	}

	return false;
}

void CActor::AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event)
{
	// Luciano - to do: use a proper customizable parameter type for CreateScriptEvent, now only float are allowed
	CreateScriptEvent("animationevent",atof(event.m_CustomParameter),event.m_EventName);
}


void CActor::RequestFacialExpression(const char* pExpressionName /* = NULL */) 
{
	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	IFacialInstance* pFacialInstance = (pCharacter ? pCharacter->GetFacialInstance() : 0);
	IFacialAnimSequence* pSequence = (pFacialInstance ? pFacialInstance->LoadSequence(pExpressionName) : 0);
	if (pFacialInstance)
		pFacialInstance->PlaySequence(pSequence, eFacialSequenceLayer_AIExpression);
}

void CActor::PrecacheFacialExpression(const char* pExpressionName)
{
	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	IFacialInstance* pFacialInstance = (pCharacter ? pCharacter->GetFacialInstance() : 0);
	if (pFacialInstance)
		pFacialInstance->PrecacheFacialExpression(pExpressionName);
}

void CActor::FullSerialize( TSerialize ser )
{	
	assert(ser.GetSerializationTarget() != eST_Network);

	ser.BeginGroup("CActor");
	int oldHealth = (int)m_health;
	ser.Value("health", m_health);		
	ser.Value("maxHealth", m_maxHealth);
	if(ser.IsReading() && oldHealth <= 0 && m_health > 0)
		Revive(true);
	if(ser.IsReading() && m_health <= 0 && oldHealth > 0)
	{
		Kill();
		RagDollize(false);
	}

	ser.Value("sleepTimer", m_sleepTimer);
	if(IPhysicalEntity *pPhys = GetEntity()->GetPhysics())
	{
		if(ser.IsReading() && m_sleepTimer)
		{
			if(pPhys->GetType() != PE_ARTICULATED)
			{
				float sleep = m_sleepTimer;
				m_sleepTimer = 0.0f;
				Fall();
				m_sleepTimer = sleep; //was reset
			}	
		}
	}

	ser.EndGroup();

	m_linkStats.Serialize(ser);

	// NOTE Okt 13, 2007: <pvl> if we're reading and we already have a handler
	// let's get rid of it now.  If a handler is in the save we're loading then
	// 'serializeGrab' will be true and with m_pGrabHandler==0 it will get recreated.
	if (ser.IsReading () && m_pGrabHandler)
	{
		SAFE_DELETE (m_pGrabHandler);
	}

	bool serializeGrab(m_pGrabHandler?true:false);
	ser.Value("serializeGrab", serializeGrab);

	m_serializeLostHelmet = m_lostHelmet;
	ser.Value("LostHelmet", m_serializeLostHelmet);
	m_serializelostHelmetObj = m_lostHelmetObj;
	ser.Value("LostHelmetObj", m_serializelostHelmetObj);
	ser.Value("LostHelmetAttachmentPosition", m_lostHelmetPos);
	ser.Value("LostHelmetMaterial", m_lostHelmetMaterial);

	//FIXME:not sure how safe this is
	if (ser.IsReading() && serializeGrab && !m_pGrabHandler)
		CreateGrabHanlder();

	if (m_pGrabHandler)
		m_pGrabHandler->Serialize(ser);
}

void CActor::PostSerialize()
{
	//helmet serialization
	if(m_serializeLostHelmet != m_lostHelmet) //sync helmet status
	{
		if(m_serializeLostHelmet && !m_lostHelmet)
		{
			if(IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_serializeLostHelmet)) //helmet lost AND on the character (character was reset)
			{
				if(IMaterial *pMat = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(m_lostHelmetMaterial.c_str()))
					pEntity->SetMaterial(pMat);

				//get rid of new helmet attachment
				ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
				IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
				IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(m_lostHelmetPos.c_str());
				if(pAttachment && pAttachment->GetIAttachmentObject())
					pAttachment->ClearBinding();

				m_lostHelmet = m_serializeLostHelmet;
			}
			else
				LooseHelmet();
		}
		else if(m_lostHelmet && !m_serializeLostHelmet)
			ResetHelmetAttachment();
	}
	else if(m_lostHelmet)	//reset material for dropped helmets
	{
		if(IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_lostHelmet))
		{
			if(IMaterial *pMat = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(m_lostHelmetMaterial.c_str()))
				pEntity->SetMaterial(pMat);
		}
	}

  if(ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0))
    if(ISkeletonPose *pSkelPose = pCharacter->GetISkeletonPose())
      pSkelPose->SetForceSkeletonUpdate(2);
}

void CActor::SetChannelId(uint16 id)
{
}

void CActor::SetHealth( int health )
{
	if (health <= 0)
	{
		if (IsGod() > 0) // handled in CPlayer
			return;

		if (IsClient() == false)
		{
			if (gEnv->pAISystem && GetEntity() && GetEntity()->GetAI())
				gEnv->pAISystem->DebugReportDeath(GetEntity()->GetAI());
		}
	}

	if (gEnv->bServer && !gEnv->bClient && gEnv->pSystem->IsDedicated() && g_pGameCVars->sv_pacifist && health < m_health)
	{
		return;
	}

	int prevHealth=(int)m_health;
	m_health = float(min(health, m_maxHealth));
	if (m_health!=prevHealth && m_health<=0)
	{
		IItem *pItem = GetCurrentItem();
		IWeapon *pWeapon = pItem ? pItem->GetIWeapon() : NULL;

		if (pWeapon)
			pWeapon->StopFire();
	}
}

void CActor::DamageInfo(EntityId shooterID, EntityId weaponID, float damage, const char *damageType)
{
  if (strstr(damageType, "bullet") && IsClient())
	{
		IEntity *pShooter = gEnv->pEntitySystem->GetEntity(shooterID);
		if (pShooter)
		{
			InitiateCombat();
			Vec3 shooterPos = pShooter->GetWorldPos();
			Vec3 myPos = GetEntity()->GetWorldPos();
			Vec3 dirToShooter = (shooterPos - myPos).normalize();
			SMovementState ms;
			if (IMovementController *pMC = GetMovementController())
			{
				pMC->GetMovementState(ms);
				Vec3 viewDir = ms.aimDirection.normalize();
				Matrix33 viewMat = Matrix33::CreateRotationVDir(viewDir);
				Vec3 viewSide = viewMat.GetColumn(0).normalize();
				float dot = viewDir.Dot(dirToShooter);
				float sideDot = viewSide.Dot(dirToShooter);
				
				// hitting from front: dot = 1, hitting from right: sideDot = 1
				float factor = g_pGameCVars->hr_rotateFactor;
				float dotAngle = g_pGameCVars->hr_dotAngle;
				float fovamt = g_pGameCVars->hr_fovAmt;
				float fovtime = g_pGameCVars->hr_fovTime;
				float rotateTime = g_pGameCVars->hr_rotateTime;

				if (m_screenEffects != 0)
				{
					m_screenEffects->CamShake(Vec3(0,0,0), Vec3(0, sideDot * factor,0), rotateTime, rotateTime);
					float newFOV = 1.0f + (dot * fovamt);
					CFOVEffect *zOut = new CFOVEffect(GetEntity()->GetId(), newFOV);
					CLinearBlend *blendOut = new CLinearBlend(1);
					CFOVEffect *zIn = new CFOVEffect(GetEntity()->GetId(), 1.0f);
					CLinearBlend *blendIn = new CLinearBlend(1);
					float speed = 1.0f/fovtime;

					//float speed = (1.0f/fovtime) * (newFOV - m_screenEffects->GetCurrentFOV())/(newFOV - 1.0f);
					//speed = 1.0f/fabs(speed);
	
					//if (m_screenEffects->HasJobs(m_hitReactionID))
					//	speed = m_screenEffects->GetAdjustedSpeed(m_hitReactionID);
					m_screenEffects->ClearBlendGroup(m_hitReactionID);
					m_screenEffects->ClearBlendGroup(m_autoZoomInID);
					m_screenEffects->StartBlend(zOut, blendOut, speed, m_hitReactionID);
					m_screenEffects->StartBlend(zIn, blendIn, 1.0f , m_hitReactionID);
				}
			}
		}
	}  
	/*
	else if (!strcmp("fall", damageType))
	{
	}
	*/
}

void CActor::SetFrozenAmount(float amount)
{ 
  m_frozenAmount = max(0.f, min(1.f, amount)); 

  if (g_pGameCVars->cl_debugFreezeShake)
    CryLog("SetFrozenAmount <%s> -> %.2f", GetEntity()->GetName(), m_frozenAmount);
}

void CActor::AddFrost(float frost)
{
  // add scaling/multipliers here if needed    
  
  if (!IsFrozen())
    SetFrozenAmount(m_frozenAmount+frost);
}

bool CActor::IsFrozen() 
{ 
  if (SActorStats* pStats = GetActorStats())
    return pStats->isFrozen.Value();
  
  return false;
}

void CActor::SetMaxHealth( int maxHealth )
{
	m_maxHealth = maxHealth;
	SetHealth(maxHealth);
}

void CActor::Kill()
{
	if (m_pAnimatedCharacter)
		m_pAnimatedCharacter->SetParams( m_pAnimatedCharacter->GetParams().ModifyFlags(0,eACF_EnableMovementProcessing));

	if(g_pGame->GetHUD())
		g_pGame->GetHUD()->ActorDeath(this);

	m_sleepTimer = 0.0f;

	if (IVehicle* pVehicle = GetLinkedVehicle())
	{
		if (IVehicleSeat* pVehicleSeat = pVehicle->GetSeatForPassenger(GetEntityId()))
			pVehicleSeat->OnPassengerDeath();
	}

	RequestFacialExpression( NULL );
}

void CActor::SetParams(SmartScriptTable &rTable,bool resetFirst)
{
	SmartScriptTable animTable;

	if (rTable->GetValue("stance",animTable))
	{
		IScriptTable::Iterator iter = animTable->BeginIteration();
		int stance;

		while(animTable->MoveNext(iter))
		{
			SmartScriptTable stanceTable;

			if (iter.value.CopyTo(stanceTable))
			{
				if (stanceTable->GetValue("stanceId",stance))
				{
					SStanceInfo sInfo;
					const char *name;
					{
						CScriptSetGetChain stanceTableChain(stanceTable);
						stanceTableChain.GetValue("normalSpeed",sInfo.normalSpeed);
						stanceTableChain.GetValue("maxSpeed",sInfo.maxSpeed);
						stanceTableChain.GetValue("heightCollider",sInfo.heightCollider);
						stanceTableChain.GetValue("heightPivot",sInfo.heightPivot);
						stanceTableChain.GetValue("size",sInfo.size);
						stanceTableChain.GetValue("modelOffset",sInfo.modelOffset);
	
						stanceTableChain.GetValue("viewOffset",sInfo.viewOffset);
						sInfo.leanLeftViewOffset = sInfo.leanRightViewOffset = sInfo.viewOffset;
						stanceTableChain.GetValue("leanLeftViewOffset",sInfo.leanLeftViewOffset);
						stanceTableChain.GetValue("leanRightViewOffset",sInfo.leanRightViewOffset);

						stanceTableChain.GetValue("weaponOffset",sInfo.weaponOffset);
							sInfo.leanLeftWeaponOffset = sInfo.leanRightWeaponOffset = sInfo.weaponOffset;
						stanceTableChain.GetValue("leanLeftWeaponOffset",sInfo.leanLeftWeaponOffset);
							stanceTableChain.GetValue("leanRightWeaponOffset",sInfo.leanRightWeaponOffset);
						stanceTableChain.GetValue("useCapsule",sInfo.useCapsule);
						if (stanceTableChain.GetValue("name",name))
						{
							strcpy(sInfo.name,name);
						}
					}
					SetupStance((EStance)stance,&sInfo);
				}
			}
		}

		animTable->EndIteration(iter);
	}

	SActorParams *pParams(GetActorParams());
	if (pParams)
	{
		rTable->GetValue("maxGrabMass",pParams->maxGrabMass);
		rTable->GetValue("maxGrabVolume",pParams->maxGrabVolume);
		rTable->GetValue("nanoSuitActive",pParams->nanoSuitActive);
	}
}

bool CActor::IsClient() const
{
	return m_isClient;
}

bool CActor::SetAspectProfile( EEntityAspects aspect, uint8 profile )
{
	bool res(false);

	if (aspect == eEA_Physics)
	{
		/*CryLog("%s::SetProfile(%d): %s (was: %d %s)", GetEntity()->GetName(),
			profile, profile==eAP_Alive?"alive":(profile==eAP_Ragdoll?"ragdoll":(profile==eAP_Spectator?"spectator":(profile==eAP_Frozen?"frozen":"unknown"))),
			m_currentPhysProfile, m_currentPhysProfile==eAP_Alive?"alive":(m_currentPhysProfile==eAP_Ragdoll?"ragdoll":(m_currentPhysProfile==eAP_Spectator?"spectator":(m_currentPhysProfile==eAP_Frozen?"frozen":"unknown"))));
*/
		if (m_currentPhysProfile==profile && !gEnv->pSystem->IsSerializingFile()) //rephysicalize when loading savegame
			return true;

		bool wasFrozen=(m_currentPhysProfile==eAP_Frozen);

		switch (profile)
		{
		case eAP_NotPhysicalized:
			{
				SEntityPhysicalizeParams params;
				params.type = PE_NONE;
				GetEntity()->Physicalize(params);
			}
			res=true;
			break;
		case eAP_Spectator:
		case eAP_Alive:
			{
				// if we were asleep, we just want to wakeup
				if (profile==eAP_Alive && (m_currentPhysProfile==eAP_Sleep))
				{
					ICharacterInstance *pCharacter=GetEntity()->GetCharacter(0);
					if (pCharacter && pCharacter->GetISkeletonAnim())
					{
						IPhysicalEntity *pPhysicalEntity=0;
						Matrix34 delta(IDENTITY);

						pCharacter->GetISkeletonPose()->StandUp(GetEntity()->GetWorldTM(), false, pPhysicalEntity, delta);

						if (pPhysicalEntity)
						{
							IEntityPhysicalProxy *pPhysicsProxy=static_cast<IEntityPhysicalProxy *>(GetEntity()->GetProxy(ENTITY_PROXY_PHYSICS));
							if (pPhysicsProxy)
							{
								GetEntity()->SetWorldTM(delta);
								pPhysicsProxy->AssignPhysicalEntity(pPhysicalEntity);
							}
						}
						m_pAnimatedCharacter->ForceTeleportAnimationToEntity();
						m_pAnimatedCharacter->ForceRefreshPhysicalColliderMode();
					}
				}
				else
				{
					Physicalize(wasFrozen?STANCE_PRONE:STANCE_NULL);

					if (profile==eAP_Spectator)
					{
						if (ICharacterInstance *pCharacter=GetEntity()->GetCharacter(0))
							pCharacter->GetISkeletonPose()->DestroyCharacterPhysics(1);
						m_pAnimatedCharacter->ForceRefreshPhysicalColliderMode();
						m_pAnimatedCharacter->RequestPhysicalColliderMode( eColliderMode_Spectator, eColliderModeLayer_Game, "Actor::SetAspectProfile");
					}
					else if (profile==eAP_Alive)
					{
						if (m_currentPhysProfile==eAP_Spectator)
						{
							m_pAnimatedCharacter->RequestPhysicalColliderMode( eColliderMode_Undefined, eColliderModeLayer_Game, "Actor::SetAspectProfile");
							if (IPhysicalEntity *pPhysics=GetEntity()->GetPhysics())
							{
								if (ICharacterInstance *pCharacter=GetEntity()->GetCharacter(0))
								{
									pCharacter->GetISkeletonPose()->DestroyCharacterPhysics(2);

									if (IPhysicalEntity *pCharPhysics=pCharacter->GetISkeletonPose()->GetCharacterPhysics())
									{
										pe_params_articulated_body body;
										body.pHost=pPhysics;
										pCharPhysics->SetParams(&body);
									}
								}
							}
						}
					}
				}
			}
			res=true;
			break;
		case eAP_Linked:
			// make sure we are alive, for when we transition from ragdoll to linked...
			if (!GetEntity()->GetPhysics() || GetEntity()->GetPhysics()->GetType()!=PE_LIVING)
				Physicalize();
			res=true;
			break;
		case eAP_Sleep:
			RagDollize(true);
			res=true;
			break;
		case eAP_Ragdoll:
			// killed while sleeping?
			if (m_currentPhysProfile==eAP_Sleep) 
				GoLimp();
			else
				RagDollize(false);
			res=true;
			break;
		case eAP_Frozen:
			if (!GetEntity()->GetPhysics() || ((GetEntity()->GetPhysics()->GetType()!=PE_LIVING) && (GetEntity()->GetPhysics()->GetType()!=PE_ARTICULATED)))
				Physicalize();
			Freeze(true);
			res=true;
			break;
		}

		IPhysicalEntity *pPE=GetEntity()->GetPhysics();
		pe_player_dynamics pdyn;

		if (profile!=eAP_Frozen && wasFrozen)
		{
			Freeze(false);

			if (profile==eAP_Alive)
			{
				EStance stance;
				if (!TrySetStance(stance=STANCE_STAND))
					if (!TrySetStance(stance=STANCE_CROUCH))
					{
						pdyn.bActive=0;
						pPE->SetParams(&pdyn);

						if (!TrySetStance(stance=STANCE_PRONE))
							stance=STANCE_NULL;

						pdyn.bActive=1;
						pPE->SetParams(&pdyn);
					}
					
				if (stance!=STANCE_NULL)
				{
					m_stance=STANCE_NULL;
					m_desiredStance=stance;

					UpdateStance();
				}

				GetGameObject()->ChangedNetworkState(IPlayerInput::INPUT_ASPECT);
			}
		}

		if (res)
			ProfileChanged(profile);

		m_currentPhysProfile = profile;
	}

	return res;
}

void CActor::ProfileChanged( uint8 newProfile )
{
	//inform scripts when the profile changes
	switch(newProfile)
	{
	case eAP_Alive:
		CreateScriptEvent("profileChanged",0,"alive");
		break;
	case eAP_Ragdoll:
		CreateScriptEvent("profileChanged",0,"ragdoll");
		break;
	}
}

bool CActor::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags )
{
	if (aspect == eEA_Physics)
	{
		uint8 currentProfile = 255;

		pe_type type = PE_NONE;
		switch (profile)
		{
		case eAP_NotPhysicalized:
			type = PE_NONE;
			break;
		case eAP_Spectator:
			type = PE_LIVING;
			break;
		case eAP_Alive:
			type = PE_LIVING;
			break;
		case eAP_Sleep:
			type = PE_ARTICULATED;
			break;
		case eAP_Frozen:
			type = PE_RIGID;
			break;
		case eAP_Ragdoll:
			type = PE_ARTICULATED;
			break;
		case eAP_Linked: 	//if actor is attached to a vehicle - don't serialize actor physics additionally
			return true;
			break;
		default:
			return false;
		}

		// TODO: remove this when craig fixes it in the network system
		if (profile==eAP_Spectator)
		{
			int x=0;	
			ser.Value("unused", x, 'skip');
		}
		else if (profile==eAP_Sleep)
		{
			int x=0;	
			ser.Value("unused1", x, 'skip');
			ser.Value("unused2", x, 'skip');
		}


		ser.Value("ActorMass", GetActorStats()->mass, 'aMas');  //needed for physicalization

		if (type == PE_NONE)
			return true;

		IEntityPhysicalProxy * pEPP = (IEntityPhysicalProxy *) GetEntity()->GetProxy(ENTITY_PROXY_PHYSICS);
		if (ser.IsWriting())
		{
			if (!pEPP || !pEPP->GetPhysicalEntity() || pEPP->GetPhysicalEntity()->GetType() != type)
			{
				gEnv->pPhysicalWorld->SerializeGarbageTypedSnapshot( ser, type, 0 );
				return true;
			}
		}
		else if (!pEPP)
		{
			return false;
		}

		pEPP->SerializeTyped( ser, type, pflags );
	}

	return true;
}

void CActor::HandleEvent( const SGameObjectEvent& event )
{
	if (event.event == eCGE_Recoil)
	{
		//Disable recoil while in a vehicle, if not camera shakes all over the place!!
		if(IsClient() && GetLinkedVehicle() && !IsThirdPerson())
			return;

		const float recoilDuration = 0.2f;
		const float recoilDistance = 0.1f;
/*
		if (event.param != NULL)
		{
			recoilDuration = *((float*)(event.param)) * 0.1f;
			recoilDistance = *((float*)(event.param)) * 0.01f;
		}
*/
		m_pAnimatedCharacter->TriggerRecoil(recoilDuration, recoilDistance);
	}
	else	if (event.event == eCGE_OnShoot)
	{
		SActorStats *pStats = GetActorStats();
		if (pStats)
			pStats->inFiring = 10.0f;
	}
	else if (event.event == eCGE_Ragdoll)
	{
		GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
	}
	else if (event.event == eCGE_EnableFallAndPlay)
	{
		RagDollize(true);
	}
	else if (event.event == eCGE_DisableFallAndPlay)
	{
		assert(false);
	}
	else if (event.event == eCGE_EnablePhysicalCollider)
	{
		m_pAnimatedCharacter->RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "Actor::HandleEvent");
	}
	else if (event.event == eCGE_DisablePhysicalCollider)
	{
		m_pAnimatedCharacter->RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "Actor::HandleEvent");
	}
	else if (event.event == eCGE_RebindAnimGraphInputs)
	{
		BindInputs( m_pAnimatedCharacter?m_pAnimatedCharacter->GetAnimationGraphState():NULL );
	}
	else if (event.event == eCGE_BeginReloadLoop)
	{
		SetAnimationInput( "Action", "reload" );
	}
	else if (event.event == eCGE_EndReloadLoop)
	{
		SetAnimationInput( "Action", "idle" );
	}
	else if (event.event == eGFE_BecomeLocalPlayer)
	{
		m_isClient = true;
		GetGameObject()->EnablePrePhysicsUpdate( ePPU_Always );

		// always update client's character
		if (ICharacterInstance * pCharacter = GetEntity()->GetCharacter(0))
			pCharacter->SetFlags(pCharacter->GetFlags() | CS_FLAG_UPDATE_ALWAYS);
	}
}

void CActor::BindInputs( IAnimationGraphState * pAGState )
{
	if (pAGState)
	{
		m_inputHealth = pAGState->GetInputId("Health");
		m_inputStance = pAGState->GetInputId("Stance");
		m_inputFiring = pAGState->GetInputId("Firing");
		m_inputWaterLevel = pAGState->GetInputId("waterLevel");

		m_pMovementController->BindInputs( pAGState );
	}
}

void CActor::ResetAnimGraph()
{
	SetStance(STANCE_RELAXED);
}

void CActor::UpdateAnimGraph( IAnimationGraphState * pState )
{
	if (pState)
	{
		pState->SetInput( m_inputHealth, m_health );

		SActorStats *pStats = GetActorStats();
		if (pStats)
		{
			pState->SetInput(m_inputFiring, pStats->inFiring);
			pState->SetInput(m_inputWaterLevel, pStats->relativeWaterLevel);

			// NOTE: freefall & parachute was moved to ChangeParachuteState() in Player.cpp.
		}
	}
	//state.pHealth = &m_health;

	//const char * p = GetStanceInfo(m_stance)->name;
	//state.pStance = &p;
}

void CActor::QueueAnimationState( const char * state )
{
	if (m_pAnimatedCharacter)
		m_pAnimatedCharacter->PushForcedState( state );
}

void CActor::ChangeAnimGraph( const char *graph, int layer )
{
	if (m_pAnimatedCharacter)
	{
		m_pAnimatedCharacter->ChangeGraph(graph, layer);
		BindInputs(m_pAnimatedCharacter->GetAnimationGraphState());
	}
}

int CActor::GetBoneID(int ID,int slot) const
{
	if (m_boneIDs[ID]<0)
	{
		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
		if (!pCharacter)
			return -1;

		//TODO:this could be done much better
		char boneStr[64];
		switch(ID)
		{
		case BONE_BIP01:	strcpy(boneStr,"Bip01");break;
		case BONE_SPINE:	strcpy(boneStr,"Bip01 Spine");break;
		case BONE_SPINE2:	strcpy(boneStr,"Bip01 Spine2");break;
		case BONE_SPINE3:	strcpy(boneStr,"Bip01 Spine3");break;
		case BONE_HEAD:		strcpy(boneStr,"Bip01 Head");break;
		case BONE_EYE_R:	strcpy(boneStr,"eye_right_bone");break;
		case BONE_EYE_L:	strcpy(boneStr,"eye_left_bone");break;
		case BONE_WEAPON: strcpy(boneStr,"weapon_bone");break;
		case BONE_FOOT_R:	strcpy(boneStr,"Bip01 R Foot");break;
		case BONE_FOOT_L: strcpy(boneStr,"Bip01 L Foot");break;
		case BONE_ARM_R: strcpy(boneStr,"Bip01 R Forearm");break;
		case BONE_ARM_L: strcpy(boneStr,"Bip01 L Forearm");break;
		case BONE_CALF_R: strcpy(boneStr,"Bip01 R Calf");break;
		case BONE_CALF_L: strcpy(boneStr,"Bip01 L Calf");break;

		}

		m_boneIDs[ID] = pCharacter->GetISkeletonPose()->GetJointIDByName(boneStr);
	}

	return m_boneIDs[ID];
}

Vec3 CActor::GetLocalEyePos(int slot) const
{

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (pCharacter)
	{
		int id_right = GetBoneID(BONE_EYE_R);
		int id_left = GetBoneID(BONE_EYE_L);
		if (id_right>-1 && id_left>-1)
		{
			Vec3 reye = pCharacter->GetISkeletonPose()->GetAbsJointByID(id_right).t;
			Vec3 leye = pCharacter->GetISkeletonPose()->GetAbsJointByID(id_left).t;
			return (reye+leye)*0.5f;
		}
	}
	return GetStanceInfo(m_stance)->viewOffset;
}

Vec3 CActor::GetLocalEyePos2(int slot) const
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);

	if (pCharacter)
	{
		int id_pelvis = GetBoneID(1); //how the hell do I get the pelvis ID in this system
		int id_right = GetBoneID(BONE_EYE_R);
		int id_left = GetBoneID(BONE_EYE_L);

		Vec3 PelvisPos=Vec3(ZERO);
		if (id_pelvis>-1)
		//	PelvisPos = Quat::CreateRotationZ(-gf_PI/2)*pCharacter->GetISkeleton()->GetAbsJointByID(id_pelvis).t;
		PelvisPos = pCharacter->GetISkeletonPose()->GetAbsJointByID(id_pelvis).t;

		if (id_right>-1 && id_left>-1)
		{
			Vec3 reye = pCharacter->GetISkeletonPose()->GetAbsJointByID(id_right).t;
			Vec3 leye = pCharacter->GetISkeletonPose()->GetAbsJointByID(id_left).t;
			PelvisPos.z=0;
			return ((reye+leye)*0.5f - PelvisPos);
		}
	}

	return GetStanceInfo(m_stance)->viewOffset;
}


void CActor::UpdateZeroG(float frameTime)
{
	SActorStats *pStats = GetActorStats();
	if (!pStats)
		return;

	pStats->nextZeroGCheck -= frameTime;
	if (pStats->nextZeroGCheck > 0.01f)
		return;

	if (IsPlayer())
		pStats->nextZeroGCheck = 0.5f;
	else
		pStats->nextZeroGCheck = 1.0f + (cry_rand()/(float)RAND_MAX)*0.5f;

	//CryLogAlways("next ZeroG check:%.1f",pStats->nextZeroGCheck);

	Vec3 wpos(GetEntity()->GetWorldPos());
	Vec3 checkOffset(1,1,1);

	IPhysicalEntity **ppList = NULL;
	int	numEntities = gEnv->pPhysicalWorld->GetEntitiesInBox(wpos-checkOffset,wpos+checkOffset,ppList,ent_areas);

	pStats->inZeroG = false;
	pStats->zeroGUp.Set(0,0,0);

	for (int i=0;i<numEntities;++i)
	{
		pe_status_contains_point scp;
		scp.pt = wpos;
		
		if (ppList[i]->GetStatus(&scp))
		{
			pe_params_foreign_data fd;
			if (ppList[i]->GetParams(&fd) != 0)
			{
				//check for all zeroG areas to compute the average up vector for the gyroscope.
				if (fd.iForeignData == ZEROG_AREA_ID)
				{
/*
					pe_simulation_params simpar;
					if ((ppList[i]->GetParams(&simpar) != 0) && simpar.gravity.IsZero())
					{
*/
						pe_status_pos sp;
						if (ppList[i]->GetStatus(&sp) != 0)
						{
							//AABB bbox(sp.BBox);

							pStats->zeroGUp += sp.q.GetColumn2();
							pStats->zeroGUp.NormalizeSafe(Vec3(0,0,1));

							//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(wpos, ColorB(0,0,255,255), wpos + pStats->zeroGUp * 2.0f, ColorB(0,0,255,255));

							pStats->inZeroG = true;
						}
/*
					}
					else
					{
						pStats->inZeroG = false;
					}
*/
				}
			}
		}
	}

	if (pStats->zeroGUp.len2()<0.01f)
		pStats->zeroGUp.Set(0,0,1);
}

void CActor::ProcessBonesRotation(ICharacterInstance *pCharacter,float frameTime)
{
	ProcessIKLimbs(pCharacter,frameTime);
}

// when these rules change,the method CheckVirtualInventoryRestrictions needs to be updated as welll
bool CActor::CheckInventoryRestrictions(const char *itemClassName)
{
	bool noLimit = false;

	if(gEnv->pConsole->GetCVar("i_noweaponlimit")->GetIVal() != 0)
		noLimit = true;

	const char *itemCategory = m_pItemSystem->GetItemCategory(itemClassName);
	if (!itemCategory)
	{
		GameWarning("Item class %s has no category", itemClassName);
		return false;
	}

	if (!strcmp(itemCategory,"medium") || !strcmp(itemCategory,"heavy"))
	{
		IInventory *pInventory=GetInventory();
		if (pInventory)
		{
			int mediumCount = pInventory->GetCountOfCategory("medium");
			int heavyCount = pInventory->GetCountOfCategory("heavy");
			
			if ((mediumCount + heavyCount) >= 2 && !noLimit)
			{
				if (pInventory->GetCountOfClass(itemClassName) == 0)
					return false;
			}
		}
	}

	return true;
}

bool CActor::CheckVirtualInventoryRestrictions(const std::vector<string> &inventory, const char *itemClassName)
{
	bool noLimit = false;

	if(gEnv->pConsole->GetCVar("i_noweaponlimit")->GetIVal() != 0)
		noLimit = true;

	const char *itemCategory = m_pItemSystem->GetItemCategory(itemClassName);
	if (!itemCategory)
	{
		GameWarning("Item class %s has no category", itemClassName);
		return false;
	}

	if (!strcmp(itemCategory,"medium") || !strcmp(itemCategory,"heavy"))
	{
		int mediumCount=0;
		int heavyCount=0;

		for (std::vector<string>::const_iterator it=inventory.begin(); it!=inventory.end(); ++it)
		{
			const char *category=m_pItemSystem->GetItemCategory(*it);
			
			if (!stricmp(category, "medium"))
				++mediumCount;
			else if (!stricmp(category, "heavy"))
				++heavyCount;
		}

		if ((mediumCount + heavyCount) >= 2 && !noLimit)
			return false;
	}

	return true;
}


//grabbing and such
bool CActor::CanPickUpObject(IEntity *obj, float& heavyness, float& volume)
{
	if (!obj)
		return false;

	if (InZeroG())
		return true;

	IPhysicalEntity *pEnt(obj->GetPhysics());
	if (!pEnt)
		return false;

	float mass(0);
	pe_status_dynamics dynStat;
	if (pEnt->GetStatus(&dynStat))
		mass = dynStat.mass;
	/*pe_simulation_params sp;	
	if (pEnt->GetParams(&sp))
		mass = sp.mass;	*/
	
	AABB lBounds;
	obj->GetLocalBounds(lBounds);
	Vec3 delta(lBounds.min - lBounds.max);
	volume = fabs(delta.x * delta.y * delta.z);

	bool canPickUp = false;
	float strength(GetActorStrength());
	SActorParams *pParams(GetActorParams());

	if (pParams && mass <= pParams->maxGrabMass*strength && volume <= pParams->maxGrabVolume)
	{
		canPickUp = true;
		heavyness = 0.3f;

		if(mass > 30.0f)
			heavyness = 0.6f;

		if(GetActorClass() == CPlayer::GetActorClassType())
		{
			CNanoSuit *pSuit = ((CPlayer*)this)->GetNanoSuit();
			if(pSuit)
			{
				if(strength > 1.5f && !(mass <= pParams->maxGrabMass && volume <= pParams->maxGrabVolume) && pSuit->GetSuitEnergy() > 25.0f)
					heavyness = 1.0f;
			}
		}
	}

	return canPickUp;
}

bool CActor::CanPickUpObject(float mass, float volume)
{
	float strength(GetActorStrength());
	SActorParams *pParams(GetActorParams());
	if (pParams && mass <= pParams->maxGrabMass*strength && volume <= pParams->maxGrabVolume*strength)
		return true;

	return false;
}

float CActor::GetActorStrength() const
{
	return g_pGameCVars->cl_strengthscale;
}

void CActor::UpdateGrab(float frameTime)
{
	if (m_pGrabHandler)
		m_pGrabHandler->Update(frameTime);
}

//IK stuff
void CActor::SetIKPos(const char *pLimbName, const Vec3& goalPos, int priority)
{
	int limbID = GetIKLimbIndex(pLimbName);
	if (limbID > -1)
	{
		Vec3 pos(goalPos);
		m_IKLimbs[limbID].SetWPos(GetEntity(),pos,ZERO,0.5f,0.5f,priority);
	}
}

void CActor::CreateIKLimb(int characterSlot, const char *limbName, const char *rootBone, const char *midBone, const char *endBone, int flags)
{
	for (int i=0;i<m_IKLimbs.size();++i)
	{
		if (!strcmp(limbName,m_IKLimbs[i].name))
			return;
	}

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(characterSlot);
	if (pCharacter)
	{
		SIKLimb newLimb;
		newLimb.SetLimb(characterSlot,limbName,pCharacter->GetISkeletonPose()->GetJointIDByName(rootBone),pCharacter->GetISkeletonPose()->GetJointIDByName(midBone),pCharacter->GetISkeletonPose()->GetJointIDByName(endBone),flags);

		if (newLimb.endBoneID>-1 && newLimb.rootBoneID>-1)
			m_IKLimbs.push_back(newLimb);
	}
}

int CActor::GetIKLimbIndex(const char *limbName)
{
	for (int i=0;i<m_IKLimbs.size();++i)
		if (!strcmp(limbName,m_IKLimbs[i].name))
			return i;

	return -1;
}

void CActor::ProcessIKLimbs(ICharacterInstance *pCharacter,float frameTime)
{
	//first thing: restore the original animation pose if there was some IK
	//FIXME: it could get some optimization.
	for (int i=0;i<m_IKLimbs.size();++i)
		m_IKLimbs[i].Update(GetEntity(),frameTime);

	if (m_pGrabHandler)
	{
		m_pGrabHandler->ProcessIKLimbs(pCharacter);
	}
}

IAnimationGraphState * CActor::GetAnimationGraphState()
{
	if (m_pAnimatedCharacter)
		return m_pAnimatedCharacter->GetAnimationGraphState();
	else
		return NULL;
}

void CActor::SetFacialAlertnessLevel(int alertness)
{
	if (m_pAnimatedCharacter)
		m_pAnimatedCharacter->SetFacialAlertnessLevel(alertness);
}

Vec3 CActor::GetAIAttentionPos()
{
	IPipeUser* pPipeUser = CastToIPipeUserSafe(GetEntity()->GetAI());
	if (pPipeUser)
	{
		IAIObject* pTarget = pPipeUser->GetAttentionTarget();
		if (pTarget)
			return pTarget->GetPos();
	}

	return ZERO;
}

//------------------------------------------------------------------------
void CActor::SelectNextItem(int direction, bool keepHistory, const char *category)
{
	IInventory *pInventory = GetInventory();
	if (!pInventory)
		return;

	if (pInventory->GetCount() < 1)
		return;

	int startSlot = -1;
	int delta = direction;
	EntityId currentItemId = pInventory->GetCurrentItem();

	if (currentItemId)
		startSlot = pInventory->FindItem(currentItemId);

	int skip = pInventory->GetCount(); // maximum number of interactions
	while(skip)
	{
		int slot = startSlot+delta;

		if (slot<0)
			slot = pInventory->GetCount()-1;
		else if (slot >= pInventory->GetCount())
			slot = 0;

		if (startSlot==slot)
			return;

		EntityId itemId = pInventory->GetItem(slot);
		IItem *pItem = m_pItemSystem->GetItem(itemId);

		if (pItem && pItem->CanSelect() && !pItem->GetDualWieldMasterId() && (!category || !strcmp(m_pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName()), category)))
		{
			SelectItem(pItem->GetEntityId(), true);

			return;
		}

		startSlot = slot;
		--skip;
	}
}

//------------------------------------------------------------------------
CItem *CActor::GetItem(EntityId itemId) const
{
	return static_cast<CItem *>(m_pItemSystem->GetItem(itemId));
}

//------------------------------------------------------------------------
CItem *CActor::GetItemByClass(IEntityClass* pClass) const
{
	IInventory *pInventory=GetInventory();
	if (pInventory)
	{
		return static_cast<CItem *>(m_pItemSystem->GetItem(pInventory->GetItemByClass(pClass)));
	}
	return 0;
}

//------------------------------------------------------------------------
CWeapon *CActor::GetWeapon(EntityId itemId) const
{
	CItem *pItem = static_cast<CItem *>(m_pItemSystem->GetItem(itemId));
	if (pItem)
		return static_cast<CWeapon *>(pItem->GetIWeapon());

	return 0;
}

//------------------------------------------------------------------------
CWeapon *CActor::GetWeaponByClass(IEntityClass* pClass) const
{
	CItem* pItem = GetItemByClass(pClass);
	if (pItem)
		return static_cast<CWeapon *>(pItem->GetIWeapon());
	return 0;
}


//------------------------------------------------------------------------
void CActor::SelectLastItem(bool keepHistory, bool forceNext /* = false */)
{
	IInventory *pInventory = GetInventory();
	if (!pInventory)
		return;

	EntityId itemId = pInventory->GetLastItem();
	IItem *pItem = m_pItemSystem->GetItem(itemId);

	if (pItem)
		SelectItem(pItem->GetEntityId(), keepHistory);
	else if(forceNext)
		SelectNextItem(1,keepHistory,NULL); //Prevent binoculars to get stuck under certain circumstances
	else
		m_pItemSystem->SetActorItem(this, (EntityId)0, keepHistory);
}

//------------------------------------------------------------------------
void CActor::SelectItemByName(const char *name, bool keepHistory)
{
	IInventory *pInventory = GetInventory();
	if (!pInventory)
		return;

	if (pInventory->GetCount() < 1)
		return;

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
	EntityId itemId = pInventory->GetItemByClass(pClass);
	IItem *pItem = m_pItemSystem->GetItem(itemId);

	if (pItem)
		SelectItem(pItem->GetEntityId(), keepHistory);
}

//------------------------------------------------------------------------
void CActor::SelectItem(EntityId itemId, bool keepHistory)
{
	if (IItem * pItem = m_pItemSystem->GetItem(itemId))
	{
		IInventory *pInventory = GetInventory();
		if (!pInventory)
			return;

		if (pInventory->GetCount() < 1)
			return;

		if (pInventory->FindItem(itemId) < 0)
		{
			//GameWarning("Trying to select an item which is not in %s's inventory!", GetEntity()->GetName());
			return;
		}

		if(pItem->GetEntityId() == pInventory->GetHolsteredItem()) //unholster selected weapon
			pInventory->HolsterItem(false);

		m_pItemSystem->SetActorItem(this, pItem->GetEntityId());
	}
}

//------------------------------------------------------------------------
void CActor::HolsterItem(bool holster)
{
	IInventory *pInventory = GetInventory();
	if (!pInventory)
		return;

	pInventory->HolsterItem(holster);
}

//------------------------------------------------------------------------
bool CActor::UseItem(EntityId itemId)
{
	if (GetHealth()<=0)
		return false;

	CItem *pItem=GetItem(itemId);
	if (!pItem)
		return false;

	if (!pItem->CanUse(GetEntityId()))
		return false;

	if (gEnv->bServer || (pItem->GetEntity()->GetFlags()&(ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_SERVER_ONLY)))
		pItem->Use(GetEntityId());
	else
		GetGameObject()->InvokeRMI(SvRequestUseItem(), ItemIdParam(itemId), eRMI_ToServer);

	return true;
}

//------------------------------------------------------------------------
bool CActor::PickUpItem(EntityId itemId, bool sound)
{
	IItem *pItem = m_pItemSystem->GetItem(itemId);
	if (!pItem || GetHealth()<=0)
		return false;

	if(IsClient())
	{
		if(EntityId offHandId = GetInventory()->GetItemByClass(CItem::sOffHandClass))
		{
			if(IItem* pItem = GetItem(offHandId))
			{
				COffHand *pOffHand = static_cast<COffHand*> (pItem);
				if(pOffHand->GetOffHandState()!=eOHS_PICKING_ITEM2)
					return false;
			}
		}
	}
	
	float heavyness(0); //this is used to select a strength-sound intensity based on mass/volume (not well designed)
	float volume(0);

	if (pItem->GetEntity()->GetPhysics() && !CanPickUpObject(pItem->GetEntity(), heavyness, volume))  //try to pick up ...
	{
		pItem->PickUp(GetEntityId(), true);
		g_pGame->GetHUD()->TextMessage("You need more strength to pick this up!");
		DropItem(pItem->GetEntityId(), false);
		return false;
	}
	if (gEnv->bServer || (pItem->GetEntity()->GetFlags()&(ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_SERVER_ONLY)))
	{
		pItem->PickUp(GetEntityId(), true);

		if(heavyness == 1.0f) // nanosuit has to be used
		{
			if(GetActorClass() == CPlayer::GetActorClassType())
			{
				CNanoSuit *pSuit = ((CPlayer*)this)->GetNanoSuit();
				if(pSuit)
					pSuit->SetSuitEnergy(pSuit->GetSuitEnergy()-25.0f);
			}
		}

		if(GetActorClass() == CPlayer::GetActorClassType())
		{
			CNanoSuit *pSuit = ((CPlayer*)this)->GetNanoSuit();
			if(pSuit)
			{
				if(pSuit->GetMode() == NANOMODE_STRENGTH)
				{
					if(heavyness > 0.33f)
						pSuit->PlaySound(STRENGTH_LIFT_SOUND, heavyness);
				}
			}
		}

		m_pGameplayRecorder->Event(GetEntity(), GameplayEvent(eGE_ItemPickedUp, 0, 0, (void *)pItem->GetEntityId()));
	}
	else
		GetGameObject()->InvokeRMI(SvRequestPickUpItem(), ItemIdParam(itemId), eRMI_ToServer);

	return true;
}

//------------------------------------------------------------------------
bool CActor::DropItem(EntityId itemId, float impulseScale, bool selectNext, bool bydeath)
{
	CItem *pItem = static_cast<CItem*>(m_pItemSystem->GetItem(itemId));
	if (!pItem)
		return false;

	//Fix editor reseting issue
	//Player dies - Don't drop weapon
	//m_noDrop is only true when leaving the game mode into the editor (see EVENT_RESET in Item.cpp)
	if(IsClient() && gEnv->pSystem->IsEditor() && ((GetHealth()<=0)||pItem->m_noDrop))
	{
		return false;
	}

	if(IsClient())
	{
		if(COffHand* pOffHand = static_cast<COffHand*>(GetItemByClass(CItem::sOffHandClass)))
			if(pOffHand->GetOffHandState()&(eOHS_SWITCHING_GRENADE|eOHS_TRANSITIONING|eOHS_HOLDING_GRENADE|eOHS_THROWING_GRENADE))
				return false;
	}

	bool bOK = false;
	if (pItem->CanDrop())
	{
		bool performCloakFade = IsCloaked();
		if (pItem->IsDualWield())
			performCloakFade = false;

		if (gEnv->bServer)
		{
			pItem->Drop(impulseScale, selectNext, bydeath);

			if (!bydeath)
				m_pGameplayRecorder->Event(GetEntity(), GameplayEvent(eGE_ItemDropped, 0, 0, (void *)itemId));
		}
		else
			GetGameObject()->InvokeRMI(SvRequestDropItem(), DropItemParams(itemId, impulseScale, selectNext, bydeath), eRMI_ToServer);

		if (performCloakFade)
			pItem->CloakEnable(false, true);

		bOK = true;
	}
	return bOK;
}

//---------------------------------------------------------------------
void CActor::DropAttachedItems()
{
	//Drop weapons attached to the back
	if(gEnv->bServer && GetWeaponAttachmentManager())
	{
		CWeaponAttachmentManager::TAttachedWeaponsList weaponList = GetWeaponAttachmentManager()->GetAttachedWeapons();
		CWeaponAttachmentManager::TAttachedWeaponsList::iterator it = weaponList.begin();
		while(it!=weaponList.end())
		{
			CItem* pItemBack = static_cast<CItem*>(m_pItemSystem->GetItem(*it));
			if(pItemBack)
			{
				pItemBack->Drop(1.0f,false,true);
				m_pGameplayRecorder->Event(GetEntity(), GameplayEvent(eGE_ItemDropped, 0, 0, (void *)pItemBack->GetEntityId()));	
			}

			it++;
		}
	}
}
//------------------------------------------------------------------------
IItem *CActor::GetCurrentItem(bool includeVehicle/*=false*/) const
{
  if (EntityId itemId = GetCurrentItemId(includeVehicle))
	  return m_pItemSystem->GetItem(itemId);

  return 0;
}

//------------------------------------------------------------------------
EntityId CActor::GetCurrentItemId(bool includeVehicle/*=false*/) const
{
  if (includeVehicle)
  {
    if (IVehicle* pVehicle = GetLinkedVehicle())
    {
      if (EntityId itemId = pVehicle->GetCurrentWeaponId(GetEntityId()))
        return itemId;
    }
  }
  
	IInventory *pInventory = GetInventory();
	if (!pInventory)
		return 0;

	return pInventory->GetCurrentItem();
}

//------------------------------------------------------------------------
IItem *CActor::GetHolsteredItem() const
{
	IInventory *pInventory = GetInventory();
	if (!pInventory)
		return 0;

	return m_pItemSystem->GetItem(pInventory->GetHolsteredItem());
}

//------------------------------------------------------------------------
IInventory *CActor::GetInventory() const
{
	if (!m_pInventory)
		m_pInventory = (IInventory*) GetGameObject()->AcquireExtension("Inventory");
	return m_pInventory;
}

//------------------------------------------------------------------------
IInteractor *CActor::GetInteractor() const
{
	if (!m_pInteractor)
		m_pInteractor = (IInteractor*) GetGameObject()->AcquireExtension("Interactor");
	return m_pInteractor;
}

//------------------------------------------------------------------------
EntityId CActor::NetGetCurrentItem() const
{
	IInventory *pInventory = GetInventory();
	if (!pInventory)
		return 0;

	return pInventory->GetCurrentItem();
}

//------------------------------------------------------------------------
void CActor::NetSetCurrentItem(EntityId id)
{	
	SelectItem(id, false);
}

//------------------------------------------------------------------------
void CActor::UpdateFootSteps(float frameTime)
{
	//player/human footsteps are overloaded in player.cpp
}

void CActor::InitiateCombat()
{
	if (!m_enterCombat && !m_inCombat)
		m_enterCombat = true;
	ExtendCombat();
}

void CActor::ExtendCombat()
{
	if (m_inCombat || m_enterCombat)
	{
		m_combatTimer = 5.0f;
	}
}

void CActor::PostUpdate(float frameTime)
{
	if (m_screenEffects != 0)
	{
		m_screenEffects->PostUpdate(frameTime);
	}
}

void CActor::ReplaceMaterial(const char *strMaterial)
{
	//FIXME:check a bit more the case of replaceMaterial being called twice before reset it with strMaterial == NULL
	IMaterial *mat(NULL);
	if (strMaterial)
	{
		mat = gEnv->p3DEngine->GetMaterialManager()->FindMaterial(strMaterial);
		if (!mat)
				mat = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(strMaterial);
	}

	//replace material
	if (mat)
	{
		for (int i = 0; i < GetEntity()->GetSlotCount(); i++)
		{
			SEntitySlotInfo slotInfo;
			if (GetEntity()->GetSlotInfo(i, slotInfo))
			{
				if (slotInfo.pCharacter)
				{
					SetMaterialRecursive(slotInfo.pCharacter, false, mat);
				}
			}
		}

		IItem *curItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(GetInventory()->GetCurrentItem());
		if (curItem)
			curItem->Cloak(true, mat);
	}
	//restore original material
	else
	{
		for (int i = 0; i < GetEntity()->GetSlotCount(); i++)
		{
			SEntitySlotInfo slotInfo;
			if (GetEntity()->GetSlotInfo(i, slotInfo))
			{
				if (slotInfo.pCharacter)
				{
					SetMaterialRecursive(slotInfo.pCharacter, true);
				}
			}
		}

		IItem *curItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(GetInventory()->GetCurrentItem());
		if (curItem)
			curItem->Cloak(false);
		
		m_testOldMats.clear();
		m_attchObjMats.clear();
	}
	m_pReplacementMaterial = mat;
}

void CActor::SetMaterialRecursive(ICharacterInstance *charInst, bool undo, IMaterial *newMat)
{
	if (!charInst || (!undo && !newMat))
		return;
	if ((!undo && m_testOldMats.find(charInst) != m_testOldMats.end()) || (undo && m_testOldMats.find(charInst) == m_testOldMats.end()))
		return;

	if (undo)
		charInst->SetMaterial(m_testOldMats[charInst]);
	else
	{
		IMaterial *oldMat = charInst->GetMaterial();
		if (newMat != oldMat)
		{
			m_testOldMats[charInst] = oldMat;
			charInst->SetMaterial(newMat);
		}
	}
	for (int i = 0; i < charInst->GetIAttachmentManager()->GetAttachmentCount(); i++)
	{
		IAttachment *attch = charInst->GetIAttachmentManager()->GetInterfaceByIndex(i);
		if (attch)
		{
			IAttachmentObject *obj = attch->GetIAttachmentObject();
			if (obj)
			{
				SetMaterialRecursive(obj->GetICharacterInstance(), undo, newMat);
				if (/*!obj->GetICharacterInstance() &&*/ ((!undo && m_attchObjMats.find(obj) == m_attchObjMats.end()) || undo && m_attchObjMats.find(obj) != m_attchObjMats.end()))
				{

					if (undo)
						obj->SetMaterial(NULL);
					else
					{
						IMaterial *oldMat = obj->GetMaterial();
						if (oldMat != newMat)
						{
							m_attchObjMats[obj] = obj->GetMaterial();
							obj->SetMaterial(newMat);
						}
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CActor::SerializeSpawnInfo(TSerialize ser )
{
	ser.Value("teamId", m_teamId, 'team');
}

void CActor::SerializeLevelToLevel( TSerialize &ser )
{
	GetInventory()->SerializeInventoryForLevelChange(ser);
	if(GetActorClass() == CPlayer::GetActorClassType())
	{
		CPlayer *pPlayer = static_cast<CPlayer*>(this);
		pPlayer->GetNanoSuit()->Serialize(ser, 0);
	}
}

//------------------------------------------------------------------------
ISerializableInfoPtr CActor::GetSpawnInfo()
{
	struct SInfo : public ISerializableInfo
	{
		int teamId;
		void SerializeWith( TSerialize ser )
		{
			ser.Value("teamId", teamId, 'team');
		}
	};

	SInfo *p = new SInfo();

	CGameRules *pGameRules=g_pGame->GetGameRules();
	p->teamId = pGameRules?pGameRules->GetTeam(GetEntityId()):0;
	return p;
}

//------------------------------------------------------------------------
void CActor::SetSleepTimer(float timer)
{
	m_sleepTimerOrg=m_sleepTimer=timer;
}


//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, SvRequestDropItem)
{
	CItem *pItem = GetItem(params.itemId);
	if (!pItem)
	{
		GameWarning("[gamenet] Failed to drop item. Item not found!");
		return false;
	}

	//CryLogAlways("%s::SvRequestDropItem(%s)", GetEntity()->GetName(), pItem->GetEntity()->GetName());

	pItem->Drop(params.impulseScale, params.selectNext, params.byDeath);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, SvRequestPickUpItem)
{
	CItem *pItem = GetItem(params.itemId);
	if (!pItem)
	{
		// this may occur if the item has been deleted but the client has not yet been informed
		GameWarning("[gamenet] Failed to pickup item. Item not found!");
		return true;
	}

	if (GetHealth()<=0)
		return true;
/*
	// probably should check for ownerId==clientChannelOwnerId
	IActor *pChannelActor=m_pGameFramework->GetIActorSystem()->GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel));
	assert(pChannelActor);
*/
	EntityId ownerId=pItem->GetOwnerId();
	if (!ownerId)
		pItem->PickUp(GetEntityId(), true);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, SvRequestUseItem)
{
	if (!IsFrozen())
		UseItem(params.itemId);

	return true;
}

//------------------------------------------------------------------------
void CActor::NetReviveAt(const Vec3 &pos, const Quat &rot, int teamId)
{
	if (IVehicle *pVehicle=GetLinkedVehicle())
	{
		if (IVehicleSeat *pSeat=pVehicle->GetSeatForPassenger(GetEntityId()))
			pSeat->Exit(false);
	}

	// stop using any mounted weapons before reviving
	CItem *pItem=static_cast<CItem *>(GetCurrentItem());
	if (pItem)
	{
		if (pItem->IsMounted())
		{
			pItem->StopUse(GetEntityId());
			pItem=0;
		}
	}

	SetHealth(GetMaxHealth());

	m_teamId=teamId;
	g_pGame->GetGameRules()->OnRevive(this, pos, rot, m_teamId);

	Revive();

	GetEntity()->SetWorldTM(Matrix34::Create(Vec3(1,1,1), rot, pos));

	// This will cover the case when the ClPickup RMI comes in before we're revived
	{
		if (m_netLastSelectablePickedUp)
			pItem=static_cast<CItem *>(m_pItemSystem->GetItem(m_netLastSelectablePickedUp));
		m_netLastSelectablePickedUp=0;

		if (pItem)
		{
			bool soundEnabled=pItem->IsSoundEnabled();
			pItem->EnableSound(false);
			pItem->Select(false);
			pItem->EnableSound(soundEnabled);

			m_pItemSystem->SetActorItem(this, (EntityId)0);
			SelectItem(pItem->GetEntityId(), true);
		}
	}

	if (IsClient())
	{
		SupressViewBlending(); // no view bleding when respawning // CActor::Revive resets it.
		if (g_pGame->GetHUD())
			g_pGame->GetHUD()->GetRadar()->Reset();
	}
}

//------------------------------------------------------------------------
void CActor::NetReviveInVehicle(EntityId vehicleId, int seatId, int teamId)
{
	// stop using any mounted weapons before reviving
	CItem *pItem=static_cast<CItem *>(GetCurrentItem());
	if (pItem)
	{
		if (pItem->IsMounted())
		{
			pItem->StopUse(GetEntityId());
			pItem=0;
		}
	}

	SetHealth(GetMaxHealth());

	m_teamId=teamId;
	g_pGame->GetGameRules()->OnReviveInVehicle(this, vehicleId, seatId, m_teamId);

	Revive();

	// fix our physicalization, since it's need for some vehicle stuff, and it will be set correctly before the end of the frame
	// make sure we are alive, for when we transition from ragdoll to linked...
	if (!GetEntity()->GetPhysics() || GetEntity()->GetPhysics()->GetType()!=PE_LIVING)
		Physicalize();

	IVehicle *pVehicle=m_pGameFramework->GetIVehicleSystem()->GetVehicle(vehicleId);
	assert(pVehicle);
	if(pVehicle)
	{
		IVehicleSeat *pSeat=pVehicle->GetSeatById(seatId);
		if (pSeat && (!pSeat->GetPassenger() || pSeat->GetPassenger()==GetEntityId()))
			pSeat->Enter(GetEntityId(), false);
	}

	// This will cover the case when the ClPickup RMI comes in before we're revived
	if (m_netLastSelectablePickedUp)
		pItem=static_cast<CItem *>(m_pItemSystem->GetItem(m_netLastSelectablePickedUp));
	m_netLastSelectablePickedUp=0;

	if (pItem)
	{
		bool soundEnabled=pItem->IsSoundEnabled();
		pItem->EnableSound(false);
		pItem->Select(false);
		pItem->EnableSound(soundEnabled);

		m_pItemSystem->SetActorItem(this, (EntityId)0);
		SelectItem(pItem->GetEntityId(), true);
	}

	if (IsClient())
	{
		SupressViewBlending(); // no view bleding when respawning // CActor::Revive resets it.
		if (g_pGame->GetHUD())
			g_pGame->GetHUD()->GetRadar()->Reset();
	}
}

//------------------------------------------------------------------------
void CActor::NetKill(EntityId shooterId, uint16 weaponClassId, int damage, int material, int hit_type)
{
	static char weaponClassName[129]={0};
	m_pGameFramework->GetNetworkSafeClassName(weaponClassName, 128, weaponClassId);

	g_pGame->GetGameRules()->OnKill(this, shooterId, weaponClassName, damage, material, hit_type);

	m_netLastSelectablePickedUp=0;

	if (GetHealth()>0)
		SetHealth(0);

	Kill();

	g_pGame->GetGameRules()->OnKillMessage(GetEntityId(), shooterId, weaponClassName, damage, material, hit_type);

	CHUD *pHUD=g_pGame->GetHUD();
	if (!pHUD)
		return;

	bool ranked=pHUD->GetPlayerRank(shooterId)!=0 || pHUD->GetPlayerRank(GetEntityId())!=0;

	if(IsClient() && gEnv->bMultiplayer && shooterId != GetEntityId() && g_pGameCVars->g_deathCam != 0)
	{
		// use the spectator target to store who killed us (used for the MP death cam - not quite spectator mode but similar...).
		if(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(shooterId))
		{
			SetSpectatorTarget(shooterId);
			
			// Also display the name of the enemy who shot you...
			if(g_pGame->GetGameRules()->GetTeam(shooterId) != g_pGame->GetGameRules()->GetTeam(GetEntityId()))
				SAFE_HUD_FUNC(GetTagNames()->AddEnemyTagName(shooterId));

			// ensure full body is displayed (otherwise player is headless)
			if(!IsThirdPerson())
				ToggleThirdPerson();
		}
	}

	if (IsClient())
	{
		IActor *pActor=shooterId?m_pGameFramework->GetIActorSystem()->GetActor(shooterId):0;
		if (!pActor || shooterId==GetEntityId())
			pHUD->BattleLogEvent(eBLE_Warning, "@mp_BLYouDied");
		else if (IGameRules *pGameRules = g_pGame->GetGameRules())
		{
			IActor *pActor=m_pGameFramework->GetIActorSystem()->GetActor(shooterId);
			if (pActor && ranked)
				g_pGame->GetHUD()->BattleLogEvent(eBLE_Warning, "@mp_BLKilledYouRank", pActor->GetEntity()->GetName(), pHUD->GetPlayerRank(shooterId, true));
			else
				g_pGame->GetHUD()->BattleLogEvent(eBLE_Warning, "@mp_BLKilledYou", pActor?pActor->GetEntity()->GetName():weaponClassName);
		}
	}
	else
	{
		bool display=true;
		bool clientShooter=(shooterId == m_pGameFramework->GetClientActorId());

		if (!clientShooter)
		{
			display=false;

			IActor *pClientActor=m_pGameFramework->GetClientActor();
			if (pClientActor)
			{
				float distSq=(pClientActor->GetEntity()->GetWorldPos()-GetEntity()->GetWorldPos()).len2();
				if (distSq<=40.0f*40.0f)
					display=true;
			}
		}

		if (display)
		{
			IActor *pActor=shooterId?m_pGameFramework->GetIActorSystem()->GetActor(shooterId):0;

			if (clientShooter)
			{
				if (ranked)
					pHUD->BattleLogEvent(eBLE_Information, "@mp_BLYouKilledRank", GetEntity()->GetName(), pHUD->GetPlayerRank(GetEntityId(), true));
				else
					pHUD->BattleLogEvent(eBLE_Information, "@mp_BLYouKilled", GetEntity()->GetName());
			}
			else if (pActor && shooterId!=GetEntityId())
			{
				IEntity *pEntity=gEnv->pEntitySystem->GetEntity(shooterId);
				if (ranked)
					pHUD->BattleLogEvent(eBLE_Information, "@mp_BLPlayerKilledRank", pEntity->GetName(), pHUD->GetPlayerRank(shooterId, true), GetEntity()->GetName(), pHUD->GetPlayerRank(GetEntityId(), true));
				else
					pHUD->BattleLogEvent(eBLE_Information, "@mp_BLPlayerKilled", pEntity->GetName(), GetEntity()->GetName());
			}
			else
			{
				if (ranked)
					pHUD->BattleLogEvent(eBLE_Information, "@mp_BLPlayerDiedRank", GetEntity()->GetName(), pHUD->GetPlayerRank(GetEntityId(), true));
				else
					pHUD->BattleLogEvent(eBLE_Information, "@mp_BLPlayerDied", GetEntity()->GetName());
			}
		}
	}
}

//------------------------------------------------------------------------
void CActor::NetSimpleKill()
{
	if (GetHealth()>0)
		SetHealth(0);

	Kill();
}

//------------------------------------------------------------------------
bool CActor::LooseHelmet(Vec3 hitDir, Vec3 hitPos, bool simulate)
{
	if(gEnv->bMultiplayer) // this feature is SP only
		return false;

	if(GetActorClass() == CPlayer::GetActorClassType())
	{
		CNanoSuit *pSuit = ((CPlayer*)this)->GetNanoSuit();
		if(pSuit)
			return false;
	}

	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	if(!pCharacter)
		return false;
	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	//get helmet attachment
	bool hasProtection = true;
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName("helmet");
	if(!pAttachment)
	{
		hasProtection = false;
		pAttachment = pAttachmentManager->GetInterfaceByName("hat");

		if(simulate)
			return false;
	}

	if(pAttachment)
	{
		IAttachmentObject *pAttachmentObj = pAttachment->GetIAttachmentObject();
		if(pAttachmentObj)
		{
			IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
			pClassRegistry->IteratorMoveFirst();
			IEntityClass* pEntityClass = pClassRegistry->FindClass("Default");
			if(!pEntityClass)
				return false;

			//check hit point and direction (simple test whether helmet or face takes the shot, it's all head in the material system)
			if(hitPos.len() && hitDir.len())
			{
				bool frontHit = false;
				SMovementState sMovementState;
				if(IMovementController *pMC = GetMovementController())
				{		
					pMC->GetMovementState(sMovementState);
					Vec3 faceDir = sMovementState.aimDirection;
					Vec3 vRightDir = faceDir.Cross(Vec3(0,0,1));
					float fFront = -faceDir.Dot(hitDir);
					if(fFront > 0.48f)	//probably face hit - no helmet rolling ?
						frontHit = true;

					Vec3 eyePos = sMovementState.eyePosition;
					float hitDist = eyePos.GetDistance(hitPos);

					if(hitDist < 0.1f || (frontHit && hitDist < 0.33f)) //face hit
						return false;
				}
			}

			if(simulate)
				return true;

			//spawn new helmet entity
			string helmetName(GetEntity()->GetName());
			helmetName.append("_helmet");
			SEntitySpawnParams params;
			params.sName = helmetName.c_str();
			params.nFlags = ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_MODIFIED_BY_PHYSICS | ENTITY_FLAG_SPAWNED;
			params.pClass = pEntityClass;

			IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(params, true);
			if (!pEntity)
				return false;

			IAttachmentObject::EType type = pAttachmentObj->GetAttachmentType();
			if(type != IAttachmentObject::eAttachment_StatObj)
			{
				gEnv->pEntitySystem->RemoveEntity(pEntity->GetId());
				return false;
			}
			IStatObj *pStatObj = pAttachmentObj->GetIStatObj();
			
			//set helmet geometry to new entity
			pEntity->SetStatObj(pStatObj, 0, true, 5);
			IMaterial *pUsedMaterial = pAttachmentObj->GetMaterial();
			if(pUsedMaterial)
			{
				pEntity->SetMaterial(pUsedMaterial);
				m_lostHelmetMaterial = pUsedMaterial->GetName();
			}

			Vec3 pos(GetEntity()->GetWorldPos() + GetLocalEyePos(BONE_EYE_R));
			pos.z += 0.2f;
			pEntity->SetPos(pos);

			SEntityPhysicalizeParams pparams;
			pparams.type = PE_RIGID;
			pparams.nSlot = -1;
			pparams.mass = 5;
			pparams.density = 1.0f;
			pEntity->Physicalize(pparams);

			IPhysicalEntity *pPE = pEntity->GetPhysics();
			if(!pPE)
			{
				gEnv->pEntitySystem->RemoveEntity(pEntity->GetId());
				return false;
			}

			SmartScriptTable props;
			IScriptTable *pEntityScript=pEntity->GetScriptTable();
			if(pEntityScript && pEntityScript->GetValue("Properties", props))
				props->SetValue("bPickable", true);

			//some hit-impulse for the helmet
			if(hitDir.len())
			{
				hitDir.Normalize();
				pe_action_impulse imp;
				hitDir += Vec3(0,0,0.2f);
				float r = cry_frand();
				imp.impulse = hitDir * (30.0f * max(0.1f, r));
				imp.angImpulse = (r <= 0.8f && r > 0.3f)?Vec3(0,-1,0):Vec3(0,1,0);
				pPE->Action(&imp);
			}

			//remove old helmet
			pAttachment->ClearBinding();
			m_lostHelmet = pEntity->GetId();
			m_lostHelmetObj = pStatObj->GetFilePath();
			m_lostHelmetPos = hasProtection?"helmet":"hat";

			//add hair if necessary
			IAttachment *pHairAttachment = pAttachmentManager->GetInterfaceByName("hair");
			if(pHairAttachment)
			{
				if(pHairAttachment->IsAttachmentHidden())
					pHairAttachment->HideAttachment(0);
			}

			if(hasProtection)
				return true;
			return false;
		}
	}

	return false;
}

//------------------------------------------------------------------------
void CActor::ResetHelmetAttachment()
{
	//reattach old helmet to entity if possible
	if(m_lostHelmet)
	{
		ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
		IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
		IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(m_lostHelmetPos.c_str());
		if(pAttachment)
		{
			if(!pAttachment->GetIAttachmentObject())
			{
				IStatObj *pStatObj = gEnv->p3DEngine->LoadStatObj(m_lostHelmetObj.c_str());
				if(pStatObj)
				{
					CCGFAttachment *pStatObjAttachment = new CCGFAttachment;
					pStatObjAttachment->pObj = pStatObj;
					pAttachment->AddBinding(pStatObjAttachment);

					if(IMaterial *pMat = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(m_lostHelmetMaterial.c_str()))
						pStatObjAttachment->SetMaterial(pMat);

					IAttachment *pHairAttachment = pAttachmentManager->GetInterfaceByName("hair");
					if(pHairAttachment)
					{
						if(!pHairAttachment->IsAttachmentHidden())
							pHairAttachment->HideAttachment(1);
					}
				}
			}
		}

		m_lostHelmet = 0;
	}
}

//-----------------------------------------------------------------------
bool CActor::CanRagDollize() const
{
	const SActorStats *pStats = GetActorStats();
	if(!gEnv->bMultiplayer && !m_isClient && pStats && pStats->isGrabbed)
		return false;

	return true;

}
//------------------------------------------------------------------------
void CActor::GetActorMemoryStatistics(ICrySizer * s)
{
	s->AddContainer(m_testOldMats);
	s->AddContainer(m_attchObjMats);
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClSetSpectatorMode)
{
	SetSpectatorMode(params.mode, params.targetId);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClSetSpectatorHealth)
{
	SetSpectatorHealth(params.health);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClRevive)
{
	NetReviveAt(params.pos, params.rot, params.teamId);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClReviveInVehicle)
{
	NetReviveInVehicle(params.vehicleId, params.seatId, params.teamId);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClKill)
{
	NetKill(params.shooterId, params.weaponClassId, (int)params.damage, params.material, params.hit_type);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClSimpleKill)
{
	NetSimpleKill();

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClMoveTo)
{
	GetEntity()->SetWorldTM(Matrix34::Create(Vec3(1,1,1), params.rot, params.pos));

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClSetAmmo)
{
	IInventory *pInventory=GetInventory();
	if (pInventory)
	{
		IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(params.ammo.c_str());
		assert(pClass);

		int capacity = pInventory->GetAmmoCapacity(pClass);
		int current = pInventory->GetAmmoCount(pClass);
		if((!gEnv->pSystem->IsEditor()) && (params.count > capacity))
		{
			//If still there's some place, full inventory to maximum...
			if(current<capacity)
			{
				pInventory->SetAmmoCount(pClass,capacity);
				if(IsClient() && g_pGame->GetHUD() && capacity - current > 0)
				{
					//char buffer[5];
					//itoa(capacity - current, buffer, 10);
					//g_pGame->GetHUD()->DisplayFlashMessage("@grab_ammo", 3, Col_Wheat, true, (string("@")+pClass->GetName()).c_str(), buffer);
					if(g_pGame->GetHUD())
						g_pGame->GetHUD()->DisplayAmmoPickup(pClass->GetName(), capacity - current);
				}
			}
			else
			{
				if(IsClient() && g_pGame->GetHUD())
					g_pGame->GetHUD()->DisplayFlashMessage("@ammo_maxed_out", 2, ColorF(1.0f, 0,0), true, (string("@")+pClass->GetName()).c_str());
			}
		}
		else
		{
			pInventory->SetAmmoCount(pClass, params.count);
			if(IsClient() && g_pGame->GetHUD() && params.count - current > 0)
			{
				/*char buffer[5];
				itoa(params.count - current, buffer, 10);
				g_pGame->GetHUD()->DisplayFlashMessage("@grab_ammo", 3, Col_Wheat, true, (string("@")+pClass->GetName()).c_str(), buffer);*/
				if(g_pGame->GetHUD())
					g_pGame->GetHUD()->DisplayAmmoPickup(pClass->GetName(), params.count - current);
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClAddAmmo)
{
	IInventory *pInventory=GetInventory();
	if (pInventory)
	{
		IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(params.ammo.c_str());
		assert(pClass);

		int capacity = pInventory->GetAmmoCapacity(pClass);
		int current = pInventory->GetAmmoCount(pClass);
		if((!gEnv->pSystem->IsEditor()) && (pInventory->GetAmmoCount(pClass)+params.count > capacity))
		{
			if(IsClient() && g_pGame->GetHUD())
				g_pGame->GetHUD()->DisplayFlashMessage("@ammo_maxed_out", 2, ColorF(1.0f, 0,0), true, (string("@")+pClass->GetName()).c_str());

			//If still there's some place, full inventory to maximum...
			pInventory->SetAmmoCount(pClass,capacity);
			if(capacity != current && IsClient() && g_pGame->GetHUD() && capacity - current > 0)
			{
				/*char buffer[5];
				itoa(capacity - current, buffer, 10);
				g_pGame->GetHUD()->DisplayFlashMessage("@grab_ammo", 3, Col_Wheat, true, (string("@")+pClass->GetName()).c_str(), buffer);*/
				if(g_pGame->GetHUD())
					g_pGame->GetHUD()->DisplayAmmoPickup(pClass->GetName(), capacity - current);
			}
		}
		else
		{
			pInventory->SetAmmoCount(pClass, pInventory->GetAmmoCount(pClass)+params.count);
			if(IsClient() && g_pGame->GetHUD() && params.count - current > 0)
			{
				/*char buffer[5];
				itoa(params.count - current, buffer, 10);
				g_pGame->GetHUD()->DisplayFlashMessage("@grab_ammo", 3, Col_Wheat, true, (string("@")+pClass->GetName()).c_str(), buffer);*/
				if(g_pGame->GetHUD())
					g_pGame->GetHUD()->DisplayAmmoPickup(pClass->GetName(), params.count - current);
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClPickUp)
{
	if (CItem *pItem=GetItem(params.itemId))
	{
		pItem->PickUp(GetEntityId(), params.sound, params.select);

		const char *displayName=pItem->GetDisplayName();

		if (IsClient() && params.sound && g_pGame->GetHUD() && displayName && displayName[0])
			g_pGame->GetHUD()->BattleLogEvent(eBLE_Information, "@mp_BLYouPickedup", displayName);

		if (params.select)
			m_netLastSelectablePickedUp=params.itemId;
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClClearInventory)
{
	GetInventory()->Clear();

	return true;
}

//-----------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClSelectItemByName)
{
	SelectItemByName(params.itemName,true);

	return true;
}
//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClDrop)
{
	CItem *pItem=GetItem(params.itemId);
	if (pItem)
		pItem->Drop(params.impulseScale, params.selectNext, params.byDeath);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClStartUse)
{
	CItem *pItem=GetItem(params.itemId);
	if (pItem)
		pItem->StartUse(GetEntityId());

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClStopUse)
{
	CItem *pItem=GetItem(params.itemId);
	if (pItem)
		pItem->StopUse(GetEntityId());

	return true;
}

//------------------------------------------------------------------------
void CActor::DumpActorInfo()
{
  IEntity* pEntity = GetEntity();

  CryLog("ActorInfo for %s", pEntity->GetName());
  CryLog("=====================================");
  
  Vec3 pos = pEntity->GetWorldPos();
  CryLog("Entity Pos: %.f %.f %.f", pos.x, pos.y, pos.z);
  CryLog("Active: %i", pEntity->IsActive());
  CryLog("Hidden: %i", pEntity->IsHidden());
  CryLog("Invisible: %i", pEntity->IsInvisible());  
  CryLog("Profile: %i", m_currentPhysProfile);
  CryLog("Health: %i", GetHealth());  
  CryLog("Frozen: %.2f", GetFrozenAmount());
  
  if (IPhysicalEntity* pPhysics = pEntity->GetPhysics())
  { 
    CryLog("Physics type: %i", pPhysics->GetType());
    
    pe_status_pos pos;
    if (pPhysics->GetStatus(&pos))
    {
      CryLog("Physics pos: %.f %.f %.f", pos.pos.x, pos.pos.y, pos.pos.z);
    }

    pe_status_dynamics dyn;
    if (pPhysics->GetStatus(&dyn))
    {   
      CryLog("Mass: %.1f", dyn.mass);
      CryLog("Vel: %.2f %.2f %.2f", dyn.v.x, dyn.v.y, dyn.v.z);
    } 
  }  

  if (IVehicle* pVehicle = GetLinkedVehicle())
  {
    CryLog("Vehicle: %s (destroyed: %i)", pVehicle->GetEntity()->GetName(), pVehicle->IsDestroyed());
    
    IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger(GetEntityId());
    CryLog("Seat %i", pSeat ? pSeat->GetSeatId() : 0);
  }

  if (IItem* pItem = GetCurrentItem())
  {
    CryLog("Item: %s", pItem->GetEntity()->GetName());
  }


  CryLog("=====================================");
}

//
//-----------------------------------------------------------------------------
Vec3 CActor::GetWeaponOffsetWithLean(EStance stance, float lean, const Vec3& eyeOffset)
{
	//for player just do it the old way - from stance info
	if(IsPlayer())
		return GetStanceInfo(stance)->GetWeaponOffsetWithLean(lean);

	EntityId itemId = GetInventory()->GetCurrentItem();
	if(itemId)
	{
		if(CWeapon* weap = GetWeapon(itemId))
		{
			if(weap->AIUseEyeOffset())
				return eyeOffset;
			Vec3	overrideOffset;
			if(weap->AIUseOverrideOffset(stance, lean, overrideOffset))
				return overrideOffset;
		}
	}
	return GetStanceInfo(stance)->GetWeaponOffsetWithLean(lean);
}

//-------------------------------------------------------------------
//This function is called from Equipment Manager only for the client actor
void CActor::NotifyInventoryAmmoChange(IEntityClass* pAmmoClass, int amount)
{
	if(!pAmmoClass)
		return;

	/*char buffer[5];
	itoa(amount, buffer, 10);
	SAFE_HUD_FUNC(DisplayFlashMessage("@grab_ammo", 3, Col_Wheat, true, (string("@")+pAmmoClass->GetName()).c_str(), buffer));*/
	if(g_pGame->GetHUD())
		g_pGame->GetHUD()->DisplayAmmoPickup(pAmmoClass->GetName(), amount);
}



