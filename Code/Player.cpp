/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 29:9:2004: Created by Filippo De Luca

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include "GameActions.h"
#include "Player.h"
#include "PlayerView.h"
#include "GameUtils.h"

#include "Weapon.h"
#include "WeaponSystem.h"
#include "OffHand.h"
#include "Fists.h"
#include "GameRules.h"

#include <IViewSystem.h>
#include <IItemSystem.h>
#include <IPhysics.h>
#include <ICryAnimation.h>
#include "IAISystem.h"
#include "IAgent.h"
#include <IVehicleSystem.h>
#include <ISerialize.h>
#include <ISound.h>
#include "IMaterialEffects.h"

#include <IRenderAuxGeom.h>
#include <IWorldQuery.h>

#include <IGameTokens.h>

#include <IDebugHistory.h>

#include <IMusicSystem.h>
#include <StringUtils.h>


#include "PlayerMovementController.h"

#include "HUD/HUD.h"
#include "HUD/HUDRadar.h"
#include "HUD/HUDCrosshair.h"
#include "HUD/HUDVehicleInterface.h"

#include "PlayerMovement.h"
#include "PlayerRotation.h"
#include "PlayerInput.h"
#include "NetPlayerInput.h"
#include "AIDemoInput.h"

#include "CryCharAnimationParams.h"

#include "VehicleClient.h"

#include "AVMine.h"
#include "Claymore.h"

#include "Binocular.h"
#include "SoundMoods.h"

// enable this to check nan's on position updates... useful for debugging some weird crashes
#define ENABLE_NAN_CHECK

#ifdef ENABLE_NAN_CHECK
#define CHECKQNAN_FLT(x) \
	assert(((*(unsigned*)&(x))&0xff000000) != 0xff000000u && (*(unsigned*)&(x) != 0x7fc00000))
#else
#define CHECKQNAN_FLT(x) (void*)0
#endif

#define CHECKQNAN_VEC(v) \
	CHECKQNAN_FLT(v.x); CHECKQNAN_FLT(v.y); CHECKQNAN_FLT(v.z)

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

#define RANDOM() ((((float)cry_rand()/(float)RAND_MAX)*2.0f)-1.0f)
#define RANDOMR(a,b) ((float)a + ((cry_rand()/(float)RAND_MAX)*(float)(b-a)))

#undef CALL_PLAYER_EVENT_LISTENERS
#define CALL_PLAYER_EVENT_LISTENERS(func) \
{ \
	if (m_playerEventListeners.empty() == false) \
	{ \
		CPlayer::TPlayerEventListeners::const_iterator iter = m_playerEventListeners.begin(); \
		CPlayer::TPlayerEventListeners::const_iterator cur; \
		while (iter != m_playerEventListeners.end()) \
		{ \
			cur = iter; \
			++iter; \
			(*cur)->func; \
		} \
	} \
}

static bool		m_merciTimeStarted = false;
static bool		m_merciHealthUpdated = false;	//client only mercy time
static float	m_merciTimeLastHit = 0.0f;

//--------------------
//this function will be called from the engine at the right time, since bones editing must be placed at the right time.
int PlayerProcessBones(ICharacterInstance *pCharacter,void *pPlayer)
{
//	return 1; //freezing and bone processing is not working very well.

	//FIXME: do something to remove gEnv->pTimer->GetFrameTime()
	//process bones specific stuff (IK, torso rotation, etc)
	float timeFrame = gEnv->pTimer->GetFrameTime();

	ISkeletonAnim* pISkeletonAnim = pCharacter->GetISkeletonAnim();
	uint32 numAnim = pISkeletonAnim->GetNumAnimsInFIFO(0);
	if (numAnim)
		((CPlayer *)pPlayer)->ProcessBonesRotation(pCharacter, timeFrame);

	return 1;
}
//--------------------

CPlayer::TAlienInterferenceParams CPlayer::m_interferenceParams;
uint CPlayer::s_ladderMaterial = 0;

#define SafePhysEntGetStatus(pPhysEnt, status)		\
if ((pPhysEnt != NULL) &&													\
		(pPhysEnt->GetStatus(status) == 0))						\
{																									\
	int t = status.type;														\
	memset(&status, 0, sizeof(status));							\
	status.type = t;																\
}																									\

#define SafePhysEntGetParams(pPhysEnt, params)		\
if ((pPhysEnt != NULL) &&													\
		(pPhysEnt->GetParams(params) == 0))						\
{																									\
	int t = params.type;														\
	memset(&params, 0, sizeof(params));							\
	params.type = t;																\
}																									\


CPlayer::CPlayer()
{
	m_pInteractor = 0;

	for(int i = 0; i < ESound_Player_Last; ++i)
		m_sounds[i] = 0;
	m_sprintTimer = 0.0f;
	m_bSpeedSprint = false;
	m_bHasAssistance = false;
	m_timedemo = false;
	m_ignoreRecoil = false;

	m_bDemoModeSpectator = false;

	m_pNanoSuit = NULL;

	m_nParachuteSlot=0;
	m_fParachuteMorph=0;

	m_pVehicleClient = 0;

	m_pVoiceListener = NULL;

	m_lastRequestedVelocity.Set(0,0,0);

	m_openingParachute = false;

	m_drownEffectDelay = 0.0f;
	m_underwaterBubblesDelay = 0.0f;
	m_stickySurfaceTimer = 0.0f;
	m_fLowHealthSoundMood = 0.0f;

	m_sufferingHighLatency = false;

	m_bConcentration = false;
	m_fConcentrationTimer = -1.0f;

	m_viewQuatFinal.SetIdentity();
	m_baseQuat.SetIdentity();
	m_eyeOffset.Set(0,0,0);
	m_weaponOffset.Set(0,0,0);

	m_pDebugHistoryManager = NULL;
	m_pSoundProxy = 0;
	m_bVoiceSoundPlaying = false;
	m_bVoiceSoundRecursionFlag = false;

	m_vehicleViewDir.Set(0,1,0);
}

CPlayer::~CPlayer()
{
	if (m_pDebugHistoryManager != NULL) 
	{
		m_pDebugHistoryManager->Release();
		delete m_pDebugHistoryManager;
	}

	StopLoopingSounds();

	if(IsClient())
	{
		ResetScreenFX();

		InitInterference();

		SAFE_HUD_FUNC(PlayerIdSet(0));
	}

	m_pPlayerInput.reset();
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	if(pCharacter)
		pCharacter->GetISkeletonPose()->SetPostProcessCallback0(0,0);
	if(m_pNanoSuit)
		delete m_pNanoSuit;

	if (m_pVehicleClient)
	{
		g_pGame->GetIGameFramework()->GetIVehicleSystem()->RegisterVehicleClient(0);		
		delete m_pVehicleClient;
	}
  
	if(m_pVoiceListener)
		GetGameObject()->ReleaseExtension("VoiceListener");
	if (m_pInteractor)
		GetGameObject()->ReleaseExtension("Interactor");

	if (gEnv->pSoundSystem && IsClient())
		gEnv->pSoundSystem->RemoveEventListener(this);
}

bool CPlayer::Init(IGameObject * pGameObject)
{
	if (!CActor::Init(pGameObject))
		return false;

	Revive(true);

	if(GetEntityId() == LOCAL_PLAYER_ENTITY_ID && !m_pNanoSuit) //client player always has a nanosuit (else the HUD doesn't work)
		m_pNanoSuit = new CNanoSuit();

	if(IEntityRenderProxy* pProxy = (IEntityRenderProxy*)GetEntity()->GetProxy(ENTITY_PROXY_RENDER))
	{
		if(IRenderNode* pRenderNode = pProxy->GetRenderNode())
			pRenderNode->SetRndFlags(ERF_REGISTER_BY_POSITION,true);
	}

	return true;
}

void CPlayer::PostInit( IGameObject * pGameObject )
{
	CActor::PostInit(pGameObject);

	ResetAnimGraph();

	if (gEnv->bMultiplayer && !gEnv->bServer)
	{
		GetGameObject()->SetUpdateSlotEnableCondition( this, 0, eUEC_VisibleOrInRange );
		//GetGameObject()->ForceUpdateExtension(this, 0);
	}
}

void CPlayer::InitClient(int channelId )
{
	GetGameObject()->InvokeRMI(CActor::ClSetSpectatorMode(), CActor::SetSpectatorModeParams(GetSpectatorMode(), 0), eRMI_ToClientChannel|eRMI_NoLocalCalls, channelId);
	
	CActor::InitClient(channelId);
}

void CPlayer::InitLocalPlayer()
{
	GetGameObject()->SetUpdateSlotEnableCondition( this, 0, eUEC_WithoutAI );

	m_pVehicleClient = new CVehicleClient;
	m_pVehicleClient->Init();

	IVehicleSystem* pVehicleSystem = g_pGame->GetIGameFramework()->GetIVehicleSystem();
	assert(pVehicleSystem);

	pVehicleSystem->RegisterVehicleClient(m_pVehicleClient);

	if (gEnv->pSoundSystem && IsClient() && !gEnv->bMultiplayer)
		gEnv->pSoundSystem->AddEventListener(this, true);
}

void CPlayer::InitInterference()
{
  m_interferenceParams.clear();
      
  IEntityClassRegistry* pRegistry = gEnv->pEntitySystem->GetClassRegistry();
  IEntityClass* pClass = 0;

  if (pClass = pRegistry->FindClass("Trooper"))    
    m_interferenceParams.insert(std::make_pair(pClass,SAlienInterferenceParams(5.f)));

  if (pClass = pRegistry->FindClass("Scout"))    
    m_interferenceParams.insert(std::make_pair(pClass,SAlienInterferenceParams(20.f)));

  if (pClass = pRegistry->FindClass("Hunter"))    
    m_interferenceParams.insert(std::make_pair(pClass,SAlienInterferenceParams(40.f)));      
}

void CPlayer::BindInputs( IAnimationGraphState * pAGState )
{
	CActor::BindInputs(pAGState);

	if (pAGState)
	{
		m_inputAction = pAGState->GetInputId("Action");
		m_inputItem = pAGState->GetInputId("Item");
		m_inputUsingLookIK = pAGState->GetInputId("UsingLookIK");
		m_inputAiming = pAGState->GetInputId("Aiming");
		m_inputVehicleName = pAGState->GetInputId("Vehicle");
		m_inputVehicleSeat = pAGState->GetInputId("VehicleSeat");
	}

	ResetAnimGraph();
}

void CPlayer::ResetAnimGraph()
{
	if (m_pAnimatedCharacter)
	{
		IAnimationGraphState* pAGState = m_pAnimatedCharacter->GetAnimationGraphState();
		if (pAGState != NULL)
		{
			pAGState->SetInput( m_inputItem, "nw" );
			m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( "Signal", "none" );
			m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( m_inputVehicleName, "none" );
			m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( m_inputVehicleSeat, 0 );
			m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( "waterLevel", 0 );
			//marcok: m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( m_inputHealth, GetHealth() );
		}
		
		m_pAnimatedCharacter->SetParams( m_pAnimatedCharacter->GetParams().ModifyFlags( eACF_ImmediateStance, 0 ) );
	}

	SetStance(STANCE_RELAXED);
}

void CPlayer::ProcessEvent(SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_HIDE || event.event == ENTITY_EVENT_UNHIDE)
	{
		//
	}
	else
	if (event.event == ENTITY_EVENT_INVISIBLE || event.event == ENTITY_EVENT_VISIBLE)
	{
		//
	}	
	else if (event.event == ENTITY_EVENT_XFORM)
	{
		if(gEnv->bMultiplayer)
		{
			// if our local player is spectating this one, move it to this position
			CPlayer* pPlayer = (CPlayer*)gEnv->pGame->GetIGameFramework()->GetClientActor();
			if(pPlayer && pPlayer->GetSpectatorMode() == CPlayer::eASM_Follow && pPlayer->GetSpectatorTarget() == GetEntityId())
			{
				// local player is spectating us. Move them to our position
				pPlayer->MoveToSpectatorTargetPosition();
			}
		}

		int flags = event.nParam[0];
		if (flags & ENTITY_XFORM_ROT && !(flags & (ENTITY_XFORM_USER|ENTITY_XFORM_PHYSICS_STEP)))
		{
			if (flags & (ENTITY_XFORM_TRACKVIEW|ENTITY_XFORM_EDITOR|ENTITY_XFORM_TIMEDEMO))
				m_forcedRotation = true;
			else
				m_forcedRotation = false;

			Quat rotation = GetEntity()->GetRotation();

			if ((m_linkStats.linkID == 0) || ((m_linkStats.flags & LINKED_FREELOOK) == 0))
				{
					if(!gEnv->pSystem->IsSerializingFile())
					{
						m_linkStats.viewQuatLinked = m_linkStats.baseQuatLinked = rotation;
						m_viewQuatFinal = m_viewQuat = m_baseQuat = rotation;
					}
				}
			}

		if (m_timedemo && !(flags&ENTITY_XFORM_TIMEDEMO))
		{
			// Restore saved position.
			GetEntity()->SetPos(m_lastKnownPosition);
		}
		m_lastKnownPosition = GetEntity()->GetPos();
	}
	else if (event.event == ENTITY_EVENT_PREPHYSICSUPDATE)
	{
		if (m_pPlayerInput.get())
			m_pPlayerInput->PreUpdate();

		IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)(GetEntity()->GetProxy(ENTITY_PROXY_RENDER));
		if ((pRenderProxy != NULL) && pRenderProxy->IsCharactersUpdatedBeforePhysics())
			PrePhysicsUpdate();
	}

	CActor::ProcessEvent(event);

	// needs to be after CActor::ProcessEvent()
	if (event.event == ENTITY_EVENT_RESET)
	{
		//don't do that! equip them like normal weapons
		/*if (g_pGame->GetIGameFramework()->IsServer() && IsClient() && GetISystem()->IsDemoMode()!=2)
		{
			m_pItemSystem->GiveItem(this, "OffHand", false, false, false);
			m_pItemSystem->GiveItem(this, "Fists", false, false, false);
		}*/
	}

	if (event.event == ENTITY_EVENT_PRE_SERIALIZE)
	{
		SEntityEvent event(ENTITY_EVENT_RESET);
		ProcessEvent(event);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::SetViewRotation( const Quat &rotation )
{
	m_baseQuat = rotation;

	m_viewQuat = rotation;
	m_viewQuatFinal = rotation;
}

//////////////////////////////////////////////////////////////////////////
Quat CPlayer::GetViewRotation() const
{
	return m_viewQuatFinal;
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::EnableTimeDemo( bool bTimeDemo )
{
	if (bTimeDemo)
	{
		m_ignoreRecoil = true;
		m_viewAnglesOffset.Set(0,0,0);
	}
	else
	{
		m_ignoreRecoil = false;
		m_viewAnglesOffset.Set(0,0,0);
	}
	m_timedemo = bTimeDemo;
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::SetViewAngleOffset(const Vec3 &offset)
{
	if (!m_ignoreRecoil)
		m_viewAnglesOffset = Ang3(offset);
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::Draw(bool draw)
{
	if (!GetEntity())
		return;

	uint32 slotFlags = GetEntity()->GetSlotFlags(0);

	if (draw)
		slotFlags |= ENTITY_SLOT_RENDER;
	else
		slotFlags &= ~ENTITY_SLOT_RENDER;

	GetEntity()->SetSlotFlags(0,slotFlags);
}


//FIXME: this function will need some cleanup
//MR: thats true, check especially client/server 
void CPlayer::UpdateFirstPersonEffects(float frameTime)
{

	//=========================alien interference effect============================
  bool doInterference = g_pGameCVars->hud_enableAlienInterference && !m_interferenceParams.empty();
  if (doInterference)		
	{
		if(CScreenEffects *pSFX = GetScreenEffects())
		{
			//look whether there is an alien around
			float aiStrength = g_pGameCVars->hud_alienInterferenceStrength;
      float interferenceRatio = 0.f;      
      CHUDRadar *pRad = SAFE_HUD_FUNC_RET(GetRadar());
      
			if(pRad && aiStrength > 0.0f)
			{
				const std::vector<EntityId> *pNearbyEntities = pRad->GetNearbyEntities();
        
				if(pNearbyEntities && !pNearbyEntities->empty())
				{
          Vec3 vPos = GetEntity()->GetWorldPos();
          CNanoSuit* pSuit = GetNanoSuit();      
          
					std::vector<EntityId>::const_iterator it = pNearbyEntities->begin();
					std::vector<EntityId>::const_iterator itEnd = pNearbyEntities->end();
          					          
					while (it != itEnd)
					{
						IEntity *pTempEnt = gEnv->pEntitySystem->GetEntity(*it);            
            IEntityClass* pClass = pTempEnt ? pTempEnt->GetClass() : 0;

						if(pClass)
						{ 
              TAlienInterferenceParams::const_iterator it = m_interferenceParams.find(pClass);
              if (it != m_interferenceParams.end())
						  {
                float minDistSq = sqr(it->second.maxdist);
                float distSq = vPos.GetSquaredDistance(pTempEnt->GetWorldPos());
              
                if (distSq < minDistSq)
							  {
                  IActor* pActor = m_pGameFramework->GetIActorSystem()->GetActor(pTempEnt->GetId());
                  if (pActor && pActor->GetHealth() > 0)
                  {
                  float ratio = 1.f - sqrt(distSq)/it->second.maxdist; // linear falloff
                
                  if (ratio > interferenceRatio)                  
                    interferenceRatio = ratio;                  
                }                
							}
						}
						}
						++it;
					}
				}
			}

			if(interferenceRatio != 0.f)
			{	
        float strength = interferenceRatio * aiStrength;                    
			  gEnv->pSystem->GetI3DEngine()->SetPostEffectParam("AlienInterference_Amount", 0.75f*strength);
        SAFE_HUD_FUNC(StartInterference(20.0f*strength, 10.0f*strength, 100.0f, 0.f));
        
        if (!stl::find(m_clientPostEffects, EEffect_AlienInterference))
        {
					m_clientPostEffects.push_back(EEffect_AlienInterference);
					PlaySound(ESound_Fear);
        }
			}
			else
        doInterference = false;			
		}
	}

  if (!doInterference && !m_clientPostEffects.empty() && GetScreenEffects())
	{
    // turn off
		std::vector<EClientPostEffect>::iterator it = m_clientPostEffects.begin();
		for(; it != m_clientPostEffects.end(); ++it)
		{
			if((*it) == EEffect_AlienInterference)
			{
        float aiStrength = g_pGameCVars->hud_alienInterferenceStrength;
        gEnv->pSystem->GetI3DEngine()->SetPostEffectParam("AlienInterference_Amount", 0.0f);
        SAFE_HUD_FUNC(StartInterference(20.0f*aiStrength, 10.0f*aiStrength, 100.0f, 3.f));

        PlaySound(ESound_Fear, false);
				m_clientPostEffects.erase(it);
				break;
			}
		}
	}

	//===========================Stop firing weapon while sprinting/prone moving==============

	if(IItem *pItem = GetCurrentItem())
	{
		if(pItem->GetIWeapon() && !CanFire())
			pItem->GetIWeapon()->StopFire();
	}

	//========================================================================================

	CFists *pFists = static_cast<CFists*>(GetItemByClass(CItem::sFistsClass));
	COffHand *pOffHand = static_cast<COffHand*>(GetItemByClass(CItem::sOffHandClass));

	//update freefall/parachute effects
	bool freefallChanged(m_stats.inFreefallLast!=m_stats.inFreefall.Value());
	if (m_stats.inFreefall && pFists)
  {
		bool freeFall = false;
		char buff[32];
		buff[0] = 0;
		if (m_stats.inFreefall.Value()==1)
			if (freefallChanged)
			{
				strcpy(buff,"freefall_start");
				if(pOffHand && pOffHand->IsHoldingEntity())
				{
					//Force drop
					pOffHand->OnAction(GetEntityId(),"use",eAAM_OnPress,0);
					pOffHand->OnAction(GetEntityId(),"use",eAAM_OnRelease,0);
				}
			}
			else
			{
				strcpy(buff,"freefall_idle");
				freeFall = true;
			}
		else if (m_stats.inFreefall.Value()==2)
			if (freefallChanged)
			{
				if (m_pNanoSuit)
					m_pNanoSuit->PlaySound(ESound_FreeFall,1,true); //stop sound
				strcpy(buff,"parachute_start");
			}
			else
				strcpy(buff,"parachute_idle");

		pFists->EnterFreeFall(true);
		if (freefallChanged && m_stats.inFreefall.Value()==1)
		{
			if (m_pNanoSuit)
				m_pNanoSuit->PlaySound(ESound_FreeFall);
			CItem *currentItem = (CItem *)GetCurrentItem();
			if (!currentItem || pFists->GetEntityId()!=currentItem->GetEntityId())
			{
				if (currentItem) 
					GetInventory()->SetLastItem(currentItem->GetEntityId());
				// we have another item selected
				if(pFists->GetEntity()->GetCharacter(0))
					pFists->GetEntity()->GetCharacter(0)->GetISkeletonPose()->SetDefaultPose();

				// deselect it
				if (currentItem)
					currentItem->PlayAction(g_pItemStrings->deselect, CItem::eIPAF_FirstPerson, false, CItem::eIPAF_Default | CItem::eIPAF_RepeatLastFrame);
				// schedule to start swimming after deselection is finished
				pFists->EnableAnimations(false);
				gEnv->pGame->GetIGameFramework()->GetIItemSystem()->SetActorItem(this,pFists->GetEntityId());
				pFists->EnableAnimations(true);
				//fists->SetBusy(true);
			}
			else
			{
				// we already have the fists up
				GetInventory()->SetLastItem(pFists->GetEntityId());

				//fists->SetBusy(true);
			}
		}

		//play animation
		pFists->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson,buff);
		pFists->PlayAction(buff);
  }
	else if (freefallChanged && !m_stats.inFreefall && pFists)
	{
		//fists->SetBusy(false);

		if (m_pNanoSuit)
			m_pNanoSuit->PlaySound(ESound_FreeFall,1,true); //stop sound

	  //reactivate last item
		if(!GetLinkedVehicle())
		{
			EntityId lastItemId = GetInventory()->GetLastItem();
			CItem *lastItem = (CItem *)gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(lastItemId);
			pFists->PlayAction(g_pItemStrings->idle);
			pFists->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
			pFists->ResetAnimation();
			pFists->GetScheduler()->Reset();
			pFists->EnterFreeFall(false);

			if (lastItemId && lastItem && lastItemId != pFists->GetEntityId() && !GetInventory()->GetCurrentItem() && !m_stats.isOnLadder)
			{
				lastItem->ResetAnimation();
				COffHand * pOffHand=static_cast<COffHand*>(GetWeaponByClass(CItem::sOffHandClass));
				if(!pOffHand || (pOffHand->GetOffHandState()==eOHS_INIT_STATE))
					gEnv->pGame->GetIGameFramework()->GetIItemSystem()->SetActorItem(this, lastItemId);
			}
		}
  }

	m_stats.inFreefallLast = m_stats.inFreefall.Value();
	m_bUnderwater = (m_stats.headUnderWaterTimer > 0.0f);
}

void CPlayer::Update(SEntityUpdateContext& ctx, int updateSlot)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)(GetEntity()->GetProxy(ENTITY_PROXY_RENDER));
	if ((pRenderProxy == NULL) || !pRenderProxy->IsCharactersUpdatedBeforePhysics())
		PrePhysicsUpdate();

	IEntity* pEnt = GetEntity();
	if (pEnt->IsHidden() && !(GetEntity()->GetFlags() & ENTITY_FLAG_UPDATE_HIDDEN))
		return;

	if (gEnv->bServer && !IsClient() && IsPlayer())
	{
		if (INetChannel *pNetChannel=m_pGameFramework->GetNetChannel(GetChannelId()))
		{
			if (pNetChannel->GetContextViewState()>=eCVS_InGame)
			{
				if (pNetChannel->IsSufferingHighLatency(gEnv->pTimer->GetAsyncTime()))
					SufferingHighLatency(true);
				else
					SufferingHighLatency(false);
			}
		}
	}

	if (IPhysicalEntity *pPE = pEnt->GetPhysics())
	{
		if (pPE->GetType() == PE_LIVING)
			m_stats.isRagDoll = false;
		else if(pPE && m_stats.isRagDoll)
		{
			if(IsClient() && m_fDeathTime && m_bRagDollHead)
			{
				float timeSinceDeath = gEnv->pTimer->GetFrameStartTime().GetSeconds() - m_fDeathTime;
				pe_params_part params;
				int headBoneID = GetBoneID(BONE_HEAD);
				/*/if(timeSinceDeath > 3.0f && timeSinceDeath < 8.0f) //our physics system cannot handle this dynamic size change
				{
					float timeDelta = timeSinceDeath - 3.0f;
					if(headBoneID > -1)
					{
						params.partid  = headBoneID;
						if (pPE && pPE->GetParams(&params) != 0)
						{
							if(params.scale > 3.0f)
							{
								params.scale = 8.0f - timeDelta;
								pPE->SetParams(&params);
							}
						}
					}
				}
				else*/ if(timeSinceDeath > 10.0f)
				{
					m_fDeathTime = 0.0f;
					params.flagsAND = (geom_colltype_ray|geom_floats);
					params.scale = 1.0f; //disable head scaling
					pPE->SetParams(&params);
					m_bRagDollHead = false;
				}
			}
		}
	}

	if (m_stats.isStandingUp)
	{
		m_actions=0;

		if (ICharacterInstance *pCharacter=pEnt->GetCharacter(0))
			if (ISkeletonPose *pSkeletonPose=pCharacter->GetISkeletonPose())
				if (pSkeletonPose->StandingUp()>=3.0f || pSkeletonPose->StandingUp()<0.0f)
				{
					m_stats.followCharacterHead=0;
					m_stats.isStandingUp=false;

					//Don't do this (at least for AI)...
					if (IsPlayer() && GetHolsteredItem())
						HolsterItem(false);
				}
	}

	CActor::Update(ctx,updateSlot);

	const float frameTime = ctx.fFrameTime;

	bool client(IsClient());

	EAutoDisablePhysicsMode adpm = eADPM_Never;
	if (m_stats.isRagDoll)
		adpm = eADPM_Never;
	else if (client || (gEnv->bMultiplayer && gEnv->bServer))
		adpm = eADPM_Never;
	else if (IsPlayer())
		adpm = eADPM_WhenInvisibleAndFarAway;
	else
		adpm = eADPM_WhenAIDeactivated;
	GetGameObject()->SetAutoDisablePhysicsMode(adpm);
	
	if (GetHealth()<=0)
	{
		// if the player gets killed while para shooting, remove it
		ChangeParachuteState(0);		
	}

	if (!m_stats.isRagDoll && GetHealth()>0 && !m_stats.isFrozen)
	{
		if(m_pNanoSuit)
			m_pNanoSuit->Update(frameTime);

		//		UpdateAsLiveAndMobile(ctx); // Callback for inherited CPlayer classes

		UpdateStats(frameTime);

		if(client)
		{
			float healthThrHigh = g_pGameCVars->g_playerLowHealthThreshold2;
			float healthThrLow = g_pGameCVars->g_playerLowHealthThreshold;
			if(gEnv->bMultiplayer)
			{
				healthThrLow = g_pGameCVars->g_playerLowHealthThresholdMultiplayer;
				healthThrHigh = g_pGameCVars->g_playerLowHealthThreshold2Multiplayer;
			}
			static float lastHealth = 0;
			if(m_health < healthThrHigh && !g_pGameCVars->g_godMode)
			{
				if(m_health < healthThrLow)
				{
					if(!m_merciTimeStarted)
						m_merciTimeLastHit = gEnv->pTimer->GetFrameStartTime().GetSeconds();
					m_merciTimeStarted = true;
					lastHealth = m_health;
				}

				if(m_merciTimeStarted)
				{
					float health = lastHealth;
					if(m_merciHealthUpdated && m_health < healthThrLow)
						health = m_health;
					else if(frameTime < 1.0f)
						health = lastHealth + GetNanoSuit()->GetHealthRegenRate() * frameTime;
					lastHealth = health;

					m_merciHealthUpdated = false;

					if(health < healthThrLow)
						SetPainEffect(0.5f);
					else
					{
						float delta = healthThrHigh - healthThrLow;
						float percentage = (health - healthThrLow) / delta;
						SetPainEffect((1.0f - percentage) * 0.5f);
					}
				}
			}
			else
			{
				SetPainEffect(-1.0f);
				m_merciTimeStarted = false;
				m_merciTimeLastHit = 0.0f;
			}
		}
/*
		{
			float weaponLevel = GetStanceInfo(GetStance())->weaponOffset.z - 0.4f;
			float eyeLevel = GetStanceInfo(GetStance())->viewOffset.z - 0.4f;
			bool weaponWellBelowWater = (weaponLevel + 0.05f) < -m_stats.waterLevel;
			bool weaponWellAboveWater = (weaponLevel - 0.05f) > -m_stats.waterLevel;
			bool hasWeaponEquipped = GetCurrentItemId() != 0;
			if (!hasWeaponEquipped && weaponWellAboveWater)
				HolsterItem(false);
			else if (hasWeaponEquipped && weaponWellBelowWater)
				HolsterItem(true);
		}
/**/

		if(m_stats.bSprinting)
		{
			if(!m_sprintTimer)
				m_sprintTimer = gEnv->pTimer->GetFrameStartTime().GetSeconds();
			else if((m_stats.inWaterTimer <= 0.0f) && m_stance == STANCE_STAND && !m_sounds[ESound_Run] && (gEnv->pTimer->GetFrameStartTime().GetSeconds() - m_sprintTimer > 3.0f))
				PlaySound(ESound_Run);
			else if(m_sounds[ESound_Run] && ((m_stats.headUnderWaterTimer > 0.0f) || m_stance == STANCE_PRONE || m_stance == STANCE_CROUCH))
			{
				PlaySound(ESound_Run, false);
				m_sprintTimer = 0.0f;
			}

			// Report super sprint to AI system.
			if (!m_bSpeedSprint)
			{
				if (m_pNanoSuit && m_pNanoSuit->GetMode() == NANOMODE_SPEED && m_pNanoSuit->GetSprintMultiplier(false) > 1.0f)
				{
					if (GetEntity() && GetEntity()->GetAI())
						GetEntity()->GetAI()->Event(AIEVENT_PLAYER_STUNT_SPRINT, 0);
					CALL_PLAYER_EVENT_LISTENERS(OnSpecialMove(this, IPlayerEventListener::eSM_SpeedSprint));
					m_bSpeedSprint = true;
				}
				else 
					m_bSpeedSprint = false;
			}
		}
		else 
		{
			if(m_sounds[ESound_Run])
			{
				PlaySound(ESound_Run, false);
				PlaySound(ESound_StopRun);
			}
			m_sprintTimer = 0.0f;
			m_bSpeedSprint = false;
		}
	
		if(client)
		{
			UpdateFirstPersonFists();
			UpdateFirstPersonEffects(frameTime);
		}

		UpdateParachute(frameTime);

		//Vec3 camPos(pEnt->GetSlotWorldTM(0) * GetStanceViewOffset(GetStance()));
		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(camPos, 0.05f, ColorB(0,255,0,255) );
	}

	// need to create this even when player is dead, otherwise spectators don't see tank turrets rotate etc until they spawn in.
	if (client)
		GetGameObject()->AttachDistanceChecker();

	if (m_pPlayerInput.get())
		m_pPlayerInput->Update();
	else
	{
		// init input systems if required
		if (GetGameObject()->GetChannelId())
		{
			if (client) //|| ((demoMode == 2) && this == gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetOriginalDemoSpectator()))
			{
				if ( GetISystem()->IsDedicated() )
					m_pPlayerInput.reset( new CDedicatedInput(this) );
				else
					m_pPlayerInput.reset( new CPlayerInput(this) );
			}
			else
				m_pPlayerInput.reset( new CNetPlayerInput(this) );
		} else if (IsDemoPlayback())
			m_pPlayerInput.reset( new CNetPlayerInput(this) );

		if (m_pPlayerInput.get())
			GetGameObject()->EnablePostUpdates(this);
	}

	if (IsClient())
	{
		if (!m_pInteractor)
			m_pInteractor = GetGameObject()->AcquireExtension("Interactor");
	}

	// small workaround for ded server: fake a view update
	if (gEnv->bMultiplayer && gEnv->bServer && !IsClient())
	{
		SViewParams viewParams;
		UpdateView(viewParams);
	}

	UpdateWeaponRaising();

	// if spectating, send health of the spectator target to our client when it changes
	if(gEnv->bServer && gEnv->bMultiplayer && g_pGame->GetGameRules() && m_stats.spectatorMode == CActor::eASM_Follow)
	{
		IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_stats.spectatorTarget);
		if (pActor)
		{
			int health = pActor->GetHealth();
			if(health != m_stats.spectatorHealth)
			{			
				GetGameObject()->InvokeRMI(CActor::ClSetSpectatorHealth(), CActor::SetSpectatorHealthParams(health), eRMI_ToOwnClient | eRMI_NoLocalCalls);
				m_stats.spectatorHealth = health;
			}
		}

		// also, after the player we are spectating dies (or goes into spectator mode), wait 3s then switch to new target
		CActor* pCActor = static_cast<CActor*>(pActor);
		if(pCActor && pCActor->GetActorClass() == CPlayer::GetActorClassType())
		{
			CPlayer* pTargetPlayer = static_cast<CPlayer*>(pCActor);
			float timeSinceDeath = gEnv->pTimer->GetFrameStartTime().GetSeconds() - pTargetPlayer->GetDeathTime();
			if(pTargetPlayer && (pTargetPlayer->GetHealth() <= 0 && timeSinceDeath > 3.0f) || pTargetPlayer->GetSpectatorMode() != eASM_None)
			{
				m_stats.spectatorTarget = 0; // else if no other players found, HUD will continue to display previous name...
				g_pGame->GetGameRules()->RequestNextSpectatorTarget(this, 1);
			}
		}
		else if(!pActor)
		{
			// they might have disconnected. At any rate, they don't exist, so pick another...
			m_stats.spectatorTarget = 0;	// else if no other players found, HUD will continue to display previous name...
			g_pGame->GetGameRules()->RequestNextSpectatorTarget(this, 1);
		}
	}

	UpdateSounds(ctx.fFrameTime);
}

void CPlayer::UpdateSounds(float fFrameTime)
{
	if(IsClient())
	{
		bool bConcentration = false;
		if(STANCE_CROUCH == m_stance || STANCE_PRONE == m_stance || m_stats.inRest > 8.0f)
		{
			bConcentration = true;
		}
		if(RAD2DEG(gEnv->pRenderer->GetCamera().GetFov()) < 40.0f)
		{
			bConcentration = true;
		}
		// Don't set concentration sound mood while binoculars are used
		IItem *pCurrentItem = GetCurrentItem();
		if(pCurrentItem && !strcmp(pCurrentItem->GetEntity()->GetClass()->GetName(),"Binoculars"))
		{
			bConcentration = false;
		}
		if(bConcentration != m_bConcentration)
		{
			if(bConcentration)
			{
				// Start countdown
				m_fConcentrationTimer = 0.0f;
			}
			else
			{
				SAFE_SOUNDMOODS_FUNC(AddSoundMood(SOUNDMOOD_LEAVE_CONCENTRATION));
				m_fConcentrationTimer = -1.0f;
			}
			m_bConcentration = bConcentration;
		}
		if(m_bConcentration && m_fConcentrationTimer != -1.0f)
		{
			m_fConcentrationTimer += fFrameTime;
			if(m_fConcentrationTimer >= 2.0f)
			{
				SAFE_SOUNDMOODS_FUNC(AddSoundMood(SOUNDMOOD_ENTER_CONCENTRATION));
				m_fConcentrationTimer = -1.0f;
			}
		}
	}

	if(gEnv->bMultiplayer && gEnv->bServer && gEnv->bClient && !IsClient())
	{
		if(g_pGame->GetIGameFramework()->IsGameStarted())
		{
			// make sure we have a voice listener. Bit workaround but I can't find
			//	any better place to initialise this that is actually reliable with round resets.
			if(!m_pVoiceListener)
				CreateVoiceListener();
		}
	}
}

void CPlayer::UpdateParachute(float frameTime)
{
	//FIXME:check if the player has the parachute
	if (m_parachuteEnabled && (m_stats.inFreefall.Value()==1) && (m_actions&ACTION_JUMP))
	{
		ChangeParachuteState(3);
		if (IsClient())
			GetGameObject()->InvokeRMI(CPlayer::SvRequestParachute(), CPlayer::NoParams(), eRMI_ToServer);
	}
	else if (m_stats.inFreefall.Value()==2)
	{
		// update parachute morph
		m_fParachuteMorph+=frameTime;
		if (m_fParachuteMorph>1.0f)
			m_fParachuteMorph=1.0f;
		if (m_nParachuteSlot) 
		{			
			ICharacterInstance *pCharacter=GetEntity()->GetCharacter(m_nParachuteSlot);				
			//if (pCharacter)
			//	pCharacter->GetIMorphing()->SetLinearMorphSequence(m_fParachuteMorph);
		}
	}
	else if(m_stats.inFreefall.Value() == 3)
	{
		if(m_openingParachute)
		{
			m_openParachuteTimer -= frameTime;
		}
		if(m_openingParachute && m_openParachuteTimer<0.0f)
		{
			ChangeParachuteState(2);
			m_openingParachute = false;
		}
	}
	else if (m_stats.inFreefall.Value()==-1)
	{
		ChangeParachuteState(0);
	}
}

void CPlayer::UpdateWeaponRaising()
{
	uint8 pose = (uint8)eWeaponRaisedPose_None;

	EntityId currentItem = GetCurrentItemId(true);
	CWeapon* pWeapon = GetWeapon(currentItem);
	if (pWeapon != NULL)
	{
		bool raised = false;
		if (pWeapon->IsDualWield())
		{
			CWeapon* pWeaponMaster = static_cast<CWeapon*>(pWeapon->GetDualWieldMaster());
			CWeapon* pWeaponSlave = static_cast<CWeapon*>(pWeapon->GetDualWieldSlave());

			if (pWeaponMaster == NULL)
				pWeaponMaster = pWeapon;

			if (pWeaponSlave && pWeaponSlave->IsWeaponRaised())
				pose |= (uint8)eWeaponRaisedPose_DualLft;

			if (pWeaponMaster && pWeaponMaster->IsWeaponRaised())
				pose |= (uint8)eWeaponRaisedPose_DualRgt;

			if (pose != eWeaponRaisedPose_None)
				raised = true;
		}
		else if (pWeapon->IsWeaponRaised())
		{		
			raised = true;
		}

		if (raised)
		{
			pose |= pWeapon->GetRaisePose();
		}
	}

	if(m_pAnimatedCharacter)
		m_pAnimatedCharacter->SetWeaponRaisedPose((EWeaponRaisedPose)pose);
}

bool CPlayer::ShouldUsePhysicsMovement()
{

	//swimming use physics, for everyone
	if (m_stats.inWaterTimer>0.01f)
		return true;

	if (m_stats.inAir>0.01f && InZeroG())
		return true;

	//players
	if (IsPlayer())
	{
		//the client will be use physics always but when in thirdperson
		if (IsClient())
		{
			if (!IsThirdPerson()/* || m_stats.inAir>0.01f*/)
				return true;
			else
				return false;
		}

		//other clients will use physics always
		return true;
	}

	//in demo playback - use physics for recorded entities
	if(IsDemoPlayback())
		return true;
	
	//AIs in normal conditions are supposed to use LM
	return false;
}

void CPlayer::ProcessCharacterOffset()
{
	if (m_linkStats.CanMoveCharacter() && !m_stats.followCharacterHead)
	{
		IEntity *pEnt = GetEntity();

		if (IsClient() && !IsThirdPerson() && !m_stats.isOnLadder) 
		{
			float lookDown(m_viewQuatFinal.GetColumn1() * m_baseQuat.GetColumn2());
			float offsetZ(m_modelOffset.z);
			m_modelOffset = GetStanceInfo(m_stance)->viewOffset;

//*
			//to make sure the character doesn't sink in the ground
			if (m_stats.onGround > 0.0f)
				m_modelOffset.z = offsetZ;

			if (m_stats.inAir > 0.0f)
				m_modelOffset.z = -1;
/**/

			//FIXME:this is unavoidable, the character offset must be adjusted for every stance/situation. 
			//more elegant ways to do this (like check if the hip bone position is inside the view) apparently dont work, its easier to just tweak the offset manually.
			if (m_stats.inFreefall)
			{
				m_modelOffset.y -= max(-lookDown,0.0f)*0.4f;
			}
			else if (ShouldSwim())
			{
				m_modelOffset.y -= max(-lookDown,0.0f)*0.8f;
			}
			else if (m_stance == STANCE_CROUCH)
			{
				m_modelOffset.y -= max(-lookDown,0.0f)*0.6f;
			}
			else if (m_stance == STANCE_PRONE)
			{
				m_modelOffset.y -= 0.1f + max(-lookDown,0.0f)*0.4f;
			}
			else if (m_stats.jumped)
			{
				CNanoSuit *pSuit = GetNanoSuit();
				if ((pSuit != NULL) && (pSuit->GetMode() == NANOMODE_STRENGTH))
					m_modelOffset.y -= max(-lookDown,0.0f) * 0.85f;
				else
					m_modelOffset.y -= max(-lookDown,0.0f) * 0.75f;
			}
			else
			{
				m_modelOffset.y -= max(-lookDown,0.0f)*0.6f;
			}
		}

	/*
		SEntitySlotInfo slotInfo; 
		pEnt->GetSlotInfo( 0, slotInfo );

		Matrix34 LocalTM=Matrix34(IDENTITY);	
		if (slotInfo.pLocalTM)
			LocalTM=*slotInfo.pLocalTM;	

		LocalTM.m03 += m_modelOffset.x;
		LocalTM.m13 += m_modelOffset.y;
		LocalTM.m23 += m_modelOffset.z;
	  pEnt->SetSlotLocalTM(0,LocalTM);
	*/

		m_modelOffset.z = 0.0f;

		//DebugGraph_AddValue("ModelOffsetX", m_modelOffset.x);
		//DebugGraph_AddValue("ModelOffsetY", m_modelOffset.y);

		GetAnimatedCharacter()->SetExtraAnimationOffset(QuatT(m_modelOffset, IDENTITY));
	}
}

void CPlayer::PrePhysicsUpdate()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	// TODO: This whole function needs to be optimized.
	// TODO: Especially when characters are dead, alot of stuff here can be skipped.

	if (!m_pAnimatedCharacter)
		return;

	IEntity* pEnt = GetEntity();
	if (pEnt->IsHidden() && !(GetEntity()->GetFlags() & ENTITY_FLAG_UPDATE_HIDDEN))
		return;

	Debug();

	//workaround - Avoid collision with grabbed NPC - Beni
	/*if(m_pHumanGrabEntity && !m_throwingNPC)
	{
		IMovementController * pMC = GetMovementController();
		if(pMC)
		{
			SMovementState info;
			pMC->GetMovementState(info);

			Matrix34 prePhysics = m_pHumanGrabEntity->GetWorldTM();
			prePhysics.AddTranslation(info.eyeDirection*0.5f);
			m_pHumanGrabEntity->SetWorldTM(prePhysics);
		}

	}*/

	if (m_pMovementController)
	{
		if (g_pGame->GetCVars()->g_enableIdleCheck)
		{
			// if (gEnv->pGame->GetIGameFramework()->IsEditing() == false || pVar->GetIVal() == 2410)
			{
				IActorMovementController::SStats stats;
				if (m_pMovementController->GetStats(stats) && stats.idle == true)
				{
					if (GetGameObject()->IsProbablyVisible()==false && GetGameObject()->IsProbablyDistant() )
					{
						CPlayerMovementController* pMC = static_cast<CPlayerMovementController*> (m_pMovementController);
						float frameTime = gEnv->pTimer->GetFrameTime();
						pMC->IdleUpdate(frameTime);
						return;
					}
				}		
			}
		}
  }

	bool client(IsClient());
	float frameTime = gEnv->pTimer->GetFrameTime();

	//FIXME:
	// small workaround to make first person ignore animation speed, and everything else use it
	// will need to be reconsidered for multiplayer
	SAnimatedCharacterParams params = m_pAnimatedCharacter->GetParams();
	params.flags &= ~(eACF_AlwaysAnimation | eACF_PerAnimGraph | eACF_AlwaysPhysics | eACF_ConstrainDesiredSpeedToXY | eACF_NoLMErrorCorrection);

	if (ShouldUsePhysicsMovement())
	{
		params.flags |= eACF_AlwaysPhysics | eACF_ImmediateStance | eACF_NoLMErrorCorrection;

		if ((gEnv->bMultiplayer && !client) || IsThirdPerson())
		{	
			params.physErrorInnerRadiusFactor = 0.05f;
			params.physErrorOuterRadiusFactor = 0.2f;
			params.physErrorMinOuterRadius = 0.2f;
			params.physErrorMaxOuterRadius = 0.5f;
			params.flags |= eACF_UseHumanBlending;
			params.flags &= ~(eACF_NoLMErrorCorrection | eACF_AlwaysPhysics);
			params.flags |= eACF_PerAnimGraph;
		}
	}
	else
		params.flags |= eACF_ImmediateStance | eACF_PerAnimGraph | eACF_UseHumanBlending | eACF_ConstrainDesiredSpeedToXY;

	bool firstperson = IsClient() && !IsThirdPerson();
	if (firstperson)
	{
		params.flags &= ~(eACF_AlwaysAnimation | eACF_PerAnimGraph);
		params.flags |= eACF_AlwaysPhysics;
	}

	params.flags &= ~eACF_Frozen;
	if (IsFrozen())
		params.flags |= eACF_Frozen;

	m_pAnimatedCharacter->SetParams(params);

	bool fpMode = true;
	if (!IsPlayer())
		fpMode = false;
	m_pAnimatedCharacter->GetAnimationGraphState()->SetFirstPersonMode( fpMode );

	if (m_pMovementController)
	{
		SActorFrameMovementParams frameMovementParams;
		if (m_pMovementController->Update(frameTime, frameMovementParams))
		{
/*
#ifdef _DEBUG
			if(m_pMovementDebug)
				m_pMovementDebug->AddValue(frameMovementParams.desiredVelocity.len());
			if(m_pDeltaXDebug)
				m_pDeltaXDebug->AddValue(frameMovementParams.desiredVelocity.x);
			if(m_pDeltaYDebug)
				m_pDeltaYDebug->AddValue(frameMovementParams.desiredVelocity.y);
#endif
*/

      if (m_linkStats.CanRotate())
			{
				Quat baseQuatBackup(m_baseQuat);

				// process and apply movement requests
				CPlayerRotation playerRotation( *this, frameMovementParams, frameTime );
				playerRotation.Process();
				playerRotation.Commit(*this);

				if (m_forcedRotation)
				{
					//m_baseQuat = baseQuatBackup;
					m_forcedRotation = false;
				}
			}
			else
			{
				Quat baseQuatBackup(m_baseQuat);

				// process and apply movement requests
				frameMovementParams.deltaAngles.x = 0.0f;
				frameMovementParams.deltaAngles.y = 0.0f;
				frameMovementParams.deltaAngles.z = 0.0f;
				CPlayerRotation playerRotation( *this, frameMovementParams, frameTime );
				playerRotation.Process();
				playerRotation.Commit(*this);

				if (m_forcedRotation)
				{
					//m_baseQuat = baseQuatBackup;
					m_forcedRotation = false;
				}
			}

			if (m_linkStats.CanMove())
			{
				CPlayerMovement playerMovement( *this, frameMovementParams, frameTime );
				playerMovement.Process(*this);
				playerMovement.Commit(*this);

				if(m_stats.forceCrouchTime>0.0f)
					m_stats.forceCrouchTime -= frameTime;

				if (/*m_stats.inAir &&*/ m_stats.inZeroG)
					SetStance(STANCE_ZEROG);
				else if (ShouldSwim())
					SetStance(STANCE_SWIM);
				else if (m_stats.bSprinting && m_stance != STANCE_PRONE && m_pNanoSuit && m_pNanoSuit->GetMode()==NANOMODE_SPEED && m_pNanoSuit->GetSuitEnergy()>10.0f)
					SetStance(STANCE_STAND);
				else if (m_stats.grabbedHeavyEntity==2) //Holding and NPC
					SetStance(STANCE_STAND);
				else if(m_stats.forceCrouchTime>0.0f)
					SetStance(STANCE_CROUCH);
				else if (frameMovementParams.stance != STANCE_NULL)
				{
					//Check if it's possible to change to prone (most probably we come from crouch here)
					if(frameMovementParams.stance==STANCE_PRONE)
					{
						CItem* pItem = static_cast<CItem*>(GetCurrentItem());
						if(m_stats.grabbedHeavyEntity || (pItem && pItem->GetParams().prone_not_usable))
							SetStance(STANCE_CROUCH);
						else
							SetStance(frameMovementParams.stance);
					}
					else
						SetStance(frameMovementParams.stance);
				}

			}
			else
			{
				frameMovementParams.desiredVelocity = ZERO;
				CPlayerMovement playerMovement( *this, frameMovementParams, frameTime );
				playerMovement.Process(*this);
				playerMovement.Commit(*this);
			}

			if (m_linkStats.CanDoIK() || (gEnv->bMultiplayer && GetLinkedVehicle()))
				SetIK(frameMovementParams);
		}
	}

	//offset the character so its hip is at entity's origin
	ICharacterInstance *pCharacter = pEnt ? pEnt->GetCharacter(0) : NULL;

	if (pCharacter)
	{
		ProcessCharacterOffset();

		if (client||(IsPlayer()&&gEnv->bServer))
			pCharacter->GetISkeletonPose()->SetForceSkeletonUpdate(4);

		if (client)
		{
			// clear the players look target every frame
			if (m_pMovementController)
			{
				CMovementRequest mr;
				mr.ClearLookTarget();
				m_pMovementController->RequestMovement( mr );
			}
		}
	}
	//

	if (m_pMovementController)
		m_pMovementController->PostUpdate(frameTime);
}

IActorMovementController * CPlayer::CreateMovementController()
{
	return new CPlayerMovementController(this);
}

void CPlayer::SetIK( const SActorFrameMovementParams& frameMovementParams )
{
	if (!m_pAnimatedCharacter)
		return;

//	if (!IsThirdPerson() && !IsPlayer())
//		return;

	IAnimationGraphState * pGraph = m_pAnimatedCharacter?m_pAnimatedCharacter->GetAnimationGraphState():NULL;
	if (!pGraph)
		return;

	SMovementState curMovementState;
	m_pMovementController->GetMovementState(curMovementState);

	IEntity * pEntity = GetEntity();
	pGraph->SetInput( m_inputUsingLookIK, int(frameMovementParams.lookIK || frameMovementParams.aimIK) );
	if (ICharacterInstance * pCharacter = pEntity->GetCharacter(0))
	{
		ISkeletonPose * pSkeletonPose = pCharacter->GetISkeletonPose();
		if (frameMovementParams.lookIK && !m_stats.isGrabbed && (!GetAnimatedCharacter() || GetAnimatedCharacter()->IsLookIkAllowed()))
		{
			if(IsClient())		
			{
				float lookIKBlends[5];
				// Emphasize the head more for the player.
				lookIKBlends[0] = 0.04f;		// spine 1
				lookIKBlends[1] = 0.06f;		// spine 2
				lookIKBlends[2] = 0.08f;		// spine 3
				lookIKBlends[3] = 0.10f;		// neck
				lookIKBlends[4] = 0.85f;		// head   // 0.60f

				pSkeletonPose->SetLookIK( true, /*gf_PI*0.7f*/ DEG2RAD(80), frameMovementParams.lookTarget,0 );
			}
			else
			{
				if(GetLinkedVehicle())
				{
					IMovementController* pMC = GetMovementController();
					if(pMC)
					{
						// don't allow spine to move in vehicles else the hands won't be on the wheel
						float lookIKBlends[5];
						lookIKBlends[0] = 0.00f;		// spine 1
						lookIKBlends[1] = 0.00f;		// spine 2
						lookIKBlends[2] = 0.00f;		// spine 3
						lookIKBlends[3] = 0.00f;		// neck
						lookIKBlends[4] = 0.7f;		// head 

						SMovementState info;
						pMC->GetMovementState(info);
						Vec3 target = info.eyePosition + 5.0f * m_vehicleViewDir;
						pSkeletonPose->SetLookIK(true, DEG2RAD(110), target, lookIKBlends);
					}
				}
				else
					pSkeletonPose->SetLookIK( true, DEG2RAD(120), frameMovementParams.lookTarget, 0);
			}
		}
		else
			pSkeletonPose->SetLookIK( false, 0, frameMovementParams.lookTarget );

		Vec3 aimTarget = frameMovementParams.aimTarget;
		bool aimEnabled = frameMovementParams.aimIK && (!GetAnimatedCharacter() || GetAnimatedCharacter()->IsLookIkAllowed());
		if (IsPlayer())
		{
			SMovementState info;
			m_pMovementController->GetMovementState(info);
			aimTarget = info.eyePosition + info.aimDirection * 5.0f; // If this is too close the aiming will fade out.
			aimEnabled = true;

			// TODO: This should probably be moved somewhere else and not done every frame.
			ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
			if (pCharacter)
				pCharacter->GetISkeletonPose()->SetAimIKFadeOut(0);
		}
		else
		{
			// Even for AI we need to guarantee a valid aim target position (only needed for LAW/Rockets though).
			// We should not set aimEnabled to true though, because that will force aiming for all weapons.
			// The AG templates will communicate an animation synched "force aim" flag to the animation playback.
			if (!aimEnabled)
			{
				if (frameMovementParams.lookIK)
				{
					// Use look target
					aimTarget = frameMovementParams.lookTarget;
				}
				else
				{
					// Synthesize target
					SMovementState info;
					m_pMovementController->GetMovementState(info);
					aimTarget = info.eyePosition + info.aimDirection * 5.0f; // If this is too close the aiming will fade out.
				}
			}
		}
		pSkeletonPose->SetAimIK( aimEnabled, aimTarget );
		pGraph->SetInput( m_inputAiming, aimEnabled ? 1 : 0 );


/*
		if (frameMovementParams.aimIK)
		{
			ICVar *pg_aimDebug = gEnv->pConsole->GetCVar("g_aimdebug");
			if (pg_aimDebug && pg_aimDebug->GetIVal()!=0)
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(  frameMovementParams.aimTarget, 0.5f, ColorB(255,0,255,255) );
		}
*/
	}
}

void CPlayer::UpdateView(SViewParams &viewParams)
{
	if (!m_pAnimatedCharacter)
		return;

	if (viewParams.groundOnly)
	{
		if (m_stats.inAir > 0.0f)
		{
			float factor = m_stats.inAir;
			Limit(factor, 0.0f, 0.2f);
			factor = 1.0f - factor/0.2f;
			factor = factor*factor;

			viewParams.currentShakeQuat.SetSlerp(IDENTITY, viewParams.currentShakeQuat, factor);
			viewParams.currentShakeShift *= factor;
		}
	}

	CPlayerView playerView(*this,viewParams);
	playerView.Process(viewParams);
	playerView.Commit(*this,viewParams);	

	if (!IsThirdPerson())
  {
    float animControlled = m_pAnimatedCharacter->FilterView(viewParams);
		
    if (animControlled >= 1.f)
    { 
      m_baseQuat = m_viewQuat = m_viewQuatFinal = viewParams.rotation;
      m_lastAnimContPos = viewParams.position;
      m_lastAnimContRot = viewParams.rotation;
    }
    else if (animControlled>0.f && m_lastAnimControlled > animControlled)
    {
      m_viewQuat = m_viewQuatFinal = m_lastAnimContRot;
      viewParams.position = m_lastAnimContPos;
      viewParams.rotation = m_lastAnimContRot;
    }    

    m_lastAnimControlled = animControlled;

		if (CItem *pItem=GetItem(GetInventory()->GetCurrentItem()))
		{
			if (pItem->FilterView(viewParams))
				m_baseQuat = m_viewQuat = m_viewQuatFinal = viewParams.rotation;
		}
  }

	viewParams.blend = m_viewBlending;
	m_viewBlending=true;	// only disable blending for one frame

	//store the view matrix, without vertical component tough, since its going to be used by the VectorToLocal function.
	Vec3 forward(viewParams.rotation.GetColumn1());
	Vec3 up(m_baseQuat.GetColumn2());
	Vec3 right(-(up % forward));

	m_clientViewMatrix.SetFromVectors(right,up%right,up,viewParams.position);
	m_clientViewMatrix.OrthonormalizeFast();

	// finally, update the network system
	if (gEnv->bMultiplayer)
		g_pGame->GetIGameFramework()->GetNetContext()->ChangedFov( GetEntityId(), viewParams.fov );
}

void CPlayer::PostUpdateView(SViewParams &viewParams)
{

	Vec3 shakeVec(viewParams.currentShakeShift*0.85f);
	m_stats.FPWeaponPos = viewParams.position + m_stats.FPWeaponPosOffset + shakeVec;;

	Quat wQuat(viewParams.rotation*Quat::CreateRotationXYZ(m_stats.FPWeaponAnglesOffset * gf_PI/180.0f));
	wQuat *= Quat::CreateSlerp(viewParams.currentShakeQuat,IDENTITY,0.5f);
	wQuat.Normalize();

	m_stats.FPWeaponAngles = Ang3(wQuat);	
	m_stats.FPSecWeaponPos = m_stats.FPWeaponPos;
	m_stats.FPSecWeaponAngles = m_stats.FPWeaponAngles;

	
	if (CItem *pItem=GetItem(GetInventory()->GetCurrentItem()))
		pItem->PostFilterView(viewParams);

	//Update grabbed NPC if needed
	COffHand * pOffHand=static_cast<COffHand*>(GetWeaponByClass(CItem::sOffHandClass));
	if(pOffHand && (pOffHand->GetOffHandState()&(eOHS_GRABBING_NPC|eOHS_HOLDING_NPC|eOHS_THROWING_NPC)))
		pOffHand->PostFilterView(viewParams);

}

void CPlayer::RegisterPlayerEventListener(IPlayerEventListener *pPlayerEventListener)
{
	stl::push_back_unique(m_playerEventListeners, pPlayerEventListener);
}

void CPlayer::UnregisterPlayerEventListener(IPlayerEventListener *pPlayerEventListener)
{
	stl::find_and_erase(m_playerEventListeners, pPlayerEventListener);
}

IEntity *CPlayer::LinkToVehicle(EntityId vehicleId) 
{
	IEntity *pLinkedEntity = CActor::LinkToVehicle(vehicleId);

	if (pLinkedEntity)
	{
		CHECKQNAN_VEC(m_modelOffset);
		m_modelOffset.Set(0,0,0);
		GetAnimatedCharacter()->SetExtraAnimationOffset(QuatT(m_modelOffset, IDENTITY));
		CHECKQNAN_VEC(m_modelOffset);

//		ResetAnimations();

		IVehicle *pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(vehicleId);
		
		if (IVehicleSeat *pSeat = pVehicle->GetSeatForPassenger(GetEntity()->GetId()))
		{
			CALL_PLAYER_EVENT_LISTENERS(OnEnterVehicle(this,pVehicle->GetEntity()->GetClass()->GetName(),pSeat->GetSeatName(),m_stats.isThirdPerson));
		}

		// don't interpolate to vehicle camera (otherwise it intersects the vehicle)
		if(IsClient())
			SupressViewBlending();
	}
	else
	{
    if (IsThirdPerson() && !g_pGameCVars->goc_enable)
      ToggleThirdPerson();

		if (g_pGameCVars->goc_enable && !IsThirdPerson())
			ToggleThirdPerson();

		CALL_PLAYER_EVENT_LISTENERS(OnExitVehicle(this));
		m_vehicleViewDir.Set(0,1,0);

		// don't interpolate back from vehicle camera (otherwise you see your own legs)
		if(IsClient())
			SupressViewBlending();
	}

	return pLinkedEntity;
}

IEntity *CPlayer::LinkToEntity(EntityId entityId, bool bKeepTransformOnDetach) 
{
#ifdef __PLAYER_KEEP_ROTATION_ON_ATTACH
	Quat rotation = GetEntity()->GetRotation();
#endif
	IEntity *pLinkedEntity = CActor::LinkToEntity(entityId, bKeepTransformOnDetach);

	if (pLinkedEntity)
	{
		CHECKQNAN_VEC(m_modelOffset);
		m_modelOffset.Set(0,0,0);
		GetAnimatedCharacter()->SetExtraAnimationOffset(QuatT(m_modelOffset, IDENTITY));
		CHECKQNAN_VEC(m_modelOffset);

#ifdef __PLAYER_KEEP_ROTATION_ON_ATTACH
		if (bKeepTransformOnDetach)
		{
			m_linkStats.viewQuatLinked = m_linkStats.baseQuatLinked = rotation;
			m_viewQuatFinal = m_viewQuat = m_baseQuat = rotation;
		}
		else
#endif
		{
			m_baseQuat.SetIdentity();
			m_viewQuat.SetIdentity();
			m_viewQuatFinal.SetIdentity();
		}
	}

	return pLinkedEntity;
}

void CPlayer::LinkToMountedWeapon(EntityId weaponId) 
{
	m_stats.mountedWeaponID = weaponId;

	if(!m_pAnimatedCharacter)
		return;

	SAnimatedCharacterParams params = m_pAnimatedCharacter->GetParams();
	
	if (weaponId)
	{
		if (!IsClient())
			params.flags &= ~eACF_EnableMovementProcessing;
		params.flags |= eACF_NoLMErrorCorrection;
	}
	else
	{
		if (!IsClient())
			params.flags |= eACF_EnableMovementProcessing;
		params.flags &= ~eACF_NoLMErrorCorrection;
	}
	
	m_pAnimatedCharacter->SetParams( params );
}

void CPlayer::SufferingHighLatency(bool highLatency)
{
	if (highLatency && !m_sufferingHighLatency)
	{
		if (IsClient() && !gEnv->bServer)
			g_pGameActions->FilterNoConnectivity()->Enable(true);
	}
	else if (!highLatency && m_sufferingHighLatency)
	{
		if (IsClient() && !gEnv->bServer)
			g_pGameActions->FilterNoConnectivity()->Enable(false);

		// deal with vehicles here as well
	}

	// the following is done each frame on the server, and once on the client, when we're suffering high latency
	if (highLatency)
	{
		if (m_pPlayerInput.get())
			m_pPlayerInput->Reset();

		if (IVehicle *pVehicle=GetLinkedVehicle())
		{
			if (IActor *pActor=pVehicle->GetDriver())
			{
				if (pActor->GetEntityId()==GetEntityId())
				{
					if (IVehicleMovement *pMovement=pVehicle->GetMovement())
						pMovement->ResetInput();
				}
			}
		}
	}

	m_sufferingHighLatency = highLatency;
}

void CPlayer::SetViewInVehicle(Quat viewRotation)
{
	m_vehicleViewDir = viewRotation * Vec3(0,1,0); 
	GetGameObject()->ChangedNetworkState(IPlayerInput::INPUT_ASPECT);
}

void CPlayer::StanceChanged(EStance last)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	CHECKQNAN_VEC(m_modelOffset);
	if(IsPlayer())
	{
		float delta(GetStanceInfo(last)->modelOffset.z - GetStanceInfo(m_stance)->modelOffset.z);
		if (delta>0.0f)
			m_modelOffset.z -= delta;
	}
	CHECKQNAN_VEC(m_modelOffset);

	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();

	//this is to keep track of the eyes height
	if (pPhysEnt)
	{
		pe_player_dimensions playerDim;
		if (pPhysEnt->GetParams(&playerDim) != 0)
			m_stats.heightPivot = playerDim.heightPivot;
	}

	if(GetEntity() && GetEntity()->GetAI())
	{
		IAIActor* pAIActor = CastToIAIActorSafe(GetEntity()->GetAI());
		if (pAIActor)
		{
			int iGroupID = pAIActor->GetParameters().m_nGroup;
			IAIObject* pLeader = gEnv->pAISystem->GetLeaderAIObject(iGroupID);
			if(pLeader==GetEntity()->GetAI()) // if the leader is changing stance
			{
				IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
				pData->iValue = m_stance;
				gEnv->pAISystem->SendSignal(SIGNALFILTER_LEADER,1,"OnChangeStance",pLeader,pData);
			}
		}
	}

	bool player(IsPlayer());

/*
	//TODO:move the dive impulse in the processmovement function, I want all the movement related there.
	//and remove the client check!
	if (pPhysEnt && player && m_stance == STANCE_PRONE && m_stats.speedFlat>1.0)
	{
		pe_action_impulse actionImp;

		Vec3 diveDir(m_stats.velocity.GetNormalized());
		diveDir += m_baseQuat.GetColumn2() * 0.35f;

		actionImp.impulse = diveDir.GetNormalized() * m_stats.mass * 3.0f;
		actionImp.iApplyTime = 0;
		pPhysEnt->Action(&actionImp);
	}
*/

	CALL_PLAYER_EVENT_LISTENERS(OnStanceChanged(this, m_stance));
/*
	if (!player)
		m_stats.waitStance = 1.0f;
	else if (m_stance == STANCE_PRONE || last == STANCE_PRONE)
		m_stats.waitStance = 0.5f;
*/
}

float CPlayer::CalculatePseudoSpeed(bool wantSprint) const
{
	CNanoSuit* pSuit = GetNanoSuit();
	bool inSpeedMode = ((pSuit != NULL) && (pSuit->GetMode() == NANOMODE_SPEED));
	bool hasEnergy = (pSuit == NULL) || (pSuit->GetSuitEnergy() > (0.2f * NANOSUIT_ENERGY)); // AI has no suit and can sprint endlessly.

	if (!wantSprint)
		return 1.0f;
	else if (!inSpeedMode || !hasEnergy)
		return 2.0f;
	else
		return 3.0f;
}

float CPlayer::GetStanceMaxSpeed(EStance stance) const
{
	return GetStanceInfo(stance)->maxSpeed * m_params.speedMultiplier * GetZoomSpeedMultiplier();
}

float CPlayer::GetStanceNormalSpeed(EStance stance) const
{
	return GetStanceInfo(stance)->normalSpeed * m_params.speedMultiplier * GetZoomSpeedMultiplier();
}

void CPlayer::SetParams(SmartScriptTable &rTable,bool resetFirst)
{
	//not sure about this
	if (resetFirst)
	{
		m_params = SPlayerParams();
	}

	CActor::SetParams(rTable,resetFirst);

	CScriptSetGetChain params(rTable);
	params.GetValue("sprintMultiplier",m_params.sprintMultiplier);
	params.GetValue("strafeMultiplier",m_params.strafeMultiplier);
	params.GetValue("backwardMultiplier",m_params.backwardMultiplier);
	params.GetValue("grabMultiplier",m_params.grabMultiplier);
	params.GetValue("afterburnerMultiplier",m_params.afterburnerMultiplier);

	params.GetValue("inertia",m_params.inertia);
	params.GetValue("inertiaAccel",m_params.inertiaAccel);

	params.GetValue("jumpHeight",m_params.jumpHeight);

	params.GetValue("slopeSlowdown",m_params.slopeSlowdown);

	params.GetValue("leanShift",m_params.leanShift);
	params.GetValue("leanAngle",m_params.leanAngle);

	params.GetValue("thrusterImpulse",m_params.thrusterImpulse);
	params.GetValue("thrusterStabilizeImpulse",m_params.thrusterStabilizeImpulse);

	params.GetValue("gravityBootsMultipler",m_params.gravityBootsMultipler);

	params.GetValue("weaponBobbingMultiplier",m_params.weaponBobbingMultiplier);
	params.GetValue("weaponInertiaMultiplier",m_params.weaponInertiaMultiplier);

	params.GetValue("viewPivot",m_params.viewPivot);
	params.GetValue("viewDistance",m_params.viewDistance);
	params.GetValue("viewHeightOffset",m_params.viewHeightOffset);

	params.GetValue("speedMultiplier",m_params.speedMultiplier);

	//view related
	params.GetValue("viewLimitDir",m_params.vLimitDir);
	params.GetValue("viewLimitYaw",m_params.vLimitRangeH);
	params.GetValue("viewLimitPitch",m_params.vLimitRangeV);

	params.GetValue("hudOffset", m_params.hudOffset);
	params.GetValue("hudAngleOffset", (Vec3 &)m_params.hudAngleOffset);

	params.GetValue("viewFoVScale",m_params.viewFoVScale);
	params.GetValue("viewSensitivity",m_params.viewSensitivity);

	//TODO:move to SetStats()
	int followHead=m_stats.followCharacterHead.Value();
	params.GetValue("followCharacterHead", followHead);
	m_stats.followCharacterHead=followHead;

	//
	SmartScriptTable nanoTable;
	if (m_pNanoSuit && params.GetValue("nanoSuit",nanoTable))
	{
		m_pNanoSuit->SetParams(nanoTable,resetFirst);
	}
}

//fill the status table for the scripts
bool CPlayer::GetParams(SmartScriptTable &rTable)
{
	CScriptSetGetChain params(rTable);

	params.SetValue("sprintMultiplier", m_params.sprintMultiplier);
	params.SetValue("strafeMultiplier", m_params.strafeMultiplier);
	params.SetValue("backwardMultiplier", m_params.backwardMultiplier);
	params.SetValue("grabMultiplier",m_params.grabMultiplier);

	params.SetValue("jumpHeight", m_params.jumpHeight);
	params.SetValue("leanShift", m_params.leanShift);
	params.SetValue("leanAngle", m_params.leanAngle);
	params.SetValue("thrusterImpulse", m_params.thrusterImpulse);
	params.SetValue("thrusterStabilizeImpulse", m_params.thrusterStabilizeImpulse);
	params.SetValue("gravityBootsMultiplier", m_params.gravityBootsMultipler);


	params.SetValue("weaponInertiaMultiplier",m_params.weaponInertiaMultiplier);
	params.SetValue("weaponBobbingMultiplier",m_params.weaponBobbingMultiplier);

	params.SetValue("speedMultiplier",m_params.speedMultiplier);

	REUSE_VECTOR(rTable, "viewPivot", m_params.viewPivot);

	params.SetValue("viewDistance",m_params.viewDistance);
	params.SetValue("viewHeightOffset",m_params.viewHeightOffset);

	REUSE_VECTOR(rTable, "hudOffset", m_params.hudOffset);
	REUSE_VECTOR(rTable, "hudAngleOffset", m_params.hudAngleOffset);

	//view related
	REUSE_VECTOR(rTable, "viewLimitDir",m_params.vLimitDir);
	params.SetValue("viewLimitYaw",m_params.vLimitRangeH);
	params.SetValue("viewLimitPitch",m_params.vLimitRangeV);

	params.SetValue("viewFoVScale",m_params.viewFoVScale);
	params.SetValue("viewSensitivity",m_params.viewSensitivity);

	return true;
}
void CPlayer::SetStats(SmartScriptTable &rTable)
{
	CActor::SetStats(rTable);

	rTable->GetValue("inFiring",m_stats.inFiring);
}

//fill the status table for the scripts
void CPlayer::UpdateScriptStats(SmartScriptTable &rTable)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	CActor::UpdateScriptStats(rTable);

	CScriptSetGetChain stats(rTable);

	m_stats.isFrozen.SetDirtyValue(stats, "isFrozen");
	//stats.SetValue("isWalkingOnWater",m_stats.isWalkingOnWater);
	
	//stats.SetValue("shakeAmount",m_stats.shakeAmount);	
	m_stats.followCharacterHead.SetDirtyValue(stats, "followCharacterHead");
	m_stats.firstPersonBody.SetDirtyValue(stats, "firstPersonBody");
	m_stats.isOnLadder.SetDirtyValue(stats, "isOnLadder");

	stats.SetValue("gravityBoots",GravityBootsOn());
	m_stats.inFreefall.SetDirtyValue(stats, "inFreeFall");

	//nanosuit stats
	//FIXME:create a CNanoSuit::GetStats instead?
	//stats.SetValue("nanoSuitHeal", (m_pNanoSuit)?m_pNanoSuit->GetHealthRegenRate():0);
	stats.SetValue("nanoSuitArmor",(m_pNanoSuit)?m_pNanoSuit->GetSlotValue(NANOSLOT_ARMOR):0);
	stats.SetValue("nanoSuitStrength", (m_pNanoSuit)?m_pNanoSuit->GetSlotValue(NANOSLOT_STRENGTH):0);
	//stats.SetValue("cloakState",(m_pNanoSuit)?m_pNanoSuit->GetCloak()->GetState():0);	
	//stats.SetValue("visualDamp",(m_pNanoSuit)?m_pNanoSuit->GetCloak()->GetVisualDamp():0);
	stats.SetValue("soundDamp",(m_pNanoSuit)?m_pNanoSuit->GetCloak()->GetSoundDamp():0);
	//stats.SetValue("heatDamp",(m_pNanoSuit)?m_pNanoSuit->GetCloak()->GetHeatDamp():0);
}

//------------------------------------------------------------------------
bool CPlayer::ShouldSwim()
{
	if (m_stats.relativeBottomDepth < 1.3f)
		return false;

	if ((m_stats.relativeBottomDepth < 2.0f) && (m_stats.inWaterTimer > -1.0f) && (m_stats.inWaterTimer < -0.0f))
		return false;

	if (m_stats.relativeWaterLevel > 3.0f)
		return false;

	if ((m_stats.relativeWaterLevel > 0.1f) && (m_stats.inWaterTimer < -2.0f))
		return false;

	if ((m_stats.velocity.z < -1.0f) && (m_stats.relativeWaterLevel > 0.0f) && (m_stats.inWaterTimer < -2.0f))
		return false;

	if (GetLinkedVehicle() != NULL)
		return false;

	if (m_stats.isOnLadder)
		return false;

	return true;
}

//------------------------------------------------------------------------
void CPlayer::UpdateSwimStats(float frameTime)
{
	bool isClient(IsClient());

	Vec3 localReferencePos = ZERO;
	int spineID = GetBoneID(BONE_SPINE3);
	if (spineID > -1)
	{
		ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
		ISkeletonPose* pSkeletonPose = (pCharacter != NULL) ? pCharacter->GetISkeletonPose() : NULL;
		if (pSkeletonPose != NULL)
		{
			localReferencePos = pSkeletonPose->GetAbsJointByID(spineID).t;
			localReferencePos.x = 0.0f;
			localReferencePos.y = 0.0f;
			if (!localReferencePos.IsValid())
				localReferencePos = ZERO;
			if (localReferencePos.GetLengthSquared() > (2.0f * 2.0f))
				localReferencePos = ZERO;
		}
	}

	Vec3 referencePos = GetEntity()->GetWorldPos() + GetEntity()->GetWorldRotation() * localReferencePos;
	float worldWaterLevel = gEnv->p3DEngine->GetWaterLevel(&referencePos);
	float worldBottomLevel = gEnv->p3DEngine->GetBottomLevel (referencePos, 10.0f);
	m_stats.worldWaterLevelDelta = CLAMP(worldWaterLevel - m_stats.worldWaterLevel, -0.5f, +0.5f); // In case worldWaterLevel is reset or fucked up, preventing huge deltas.
	m_stats.worldWaterLevel = worldWaterLevel;
	float playerWaterLevel = -WATER_LEVEL_UNKNOWN;
	float bottomDepth = 0;
	Vec3 surfacePos(referencePos.x, referencePos.y, m_stats.worldWaterLevel);
	Vec3 bottomPos(referencePos.x, referencePos.y, worldBottomLevel);

/*
	Vec3 bottomPos(referencePos.x, referencePos.y, gEnv->p3DEngine->GetTerrainElevation(referencePos.x, referencePos.y));

	if (bottomPos.z > referencePos.z)
		bottomPos.z = referencePos.z - 100.0f;

	ray_hit hit;
	int rayFlags = geom_colltype_player<<rwi_colltype_bit | rwi_stop_at_pierceable;
	if (gEnv->pPhysicalWorld->RayWorldIntersection(referencePos + Vec3(0,0,0.2f), 
																									bottomPos - referencePos - Vec3(0,0,0.4f), 
																									ent_terrain|ent_static|ent_sleeping_rigid|ent_rigid, 
																									rayFlags, &hit, 1))
	{
		bottomPos = hit.pt;
	}
*/

	playerWaterLevel = referencePos.z - surfacePos.z;
	bottomDepth = surfacePos.z - bottomPos.z;

	m_stats.relativeWaterLevel = CLAMP(playerWaterLevel, -5, 5);
	m_stats.relativeBottomDepth = bottomDepth;

	Vec3 localHeadPos = GetLocalEyePos() + Vec3(0, 0, 0.2f);
	if (m_stance == STANCE_PRONE)
		localHeadPos = Vec3(0, 0, 0.4f);

	Vec3 worldHeadPos = GetEntity()->GetWorldPos() + localHeadPos;
	float headWaterLevel = worldHeadPos.z - surfacePos.z;

	// when inside a vehicle (like a boat or amphibious APC) we always assume to be 'above water'
	if (GetLinkedVehicle())
		headWaterLevel = 0.3f;

	if (headWaterLevel < 0.0f)
	{
		if (m_stats.headUnderWaterTimer < 0.0f)
		{
			PlaySound(ESound_DiveIn);
			m_stats.headUnderWaterTimer = 0.0f;
		}
		m_stats.headUnderWaterTimer += frameTime;
	}
	else
	{
		if (m_stats.headUnderWaterTimer > 0.0f)
		{
			PlaySound(ESound_DiveOut);
			m_stats.headUnderWaterTimer = 0.0f;
		}
		m_stats.headUnderWaterTimer -= frameTime;
	}

	UpdateUWBreathing(frameTime, referencePos /*worldHeadPos*/);

	// Update inWater timer (positive is in water, negative is out of water).
	if (ShouldSwim())
	{
		//by design : AI cannot swim and drowns no matter what
		if((GetHealth() > 0) && !isClient && !gEnv->bMultiplayer)
		{
			// apply damage same way as all the other kinds
			HitInfo hitInfo;
			hitInfo.damage = max(1.0f, 30.0f * frameTime);
			hitInfo.targetId = GetEntity()->GetId();
			g_pGame->GetGameRules()->ServerHit(hitInfo);
//			SetHealth(GetHealth() - max(1.0f, 30.0f * frameTime));
			CreateScriptEvent("splash",0);
		}

		if (m_stats.inWaterTimer < 0.0f)
		{
			m_stats.inWaterTimer = 0.0f;
			if(m_stats.inAir>0.0f)
				CreateScriptEvent("jump_splash",m_stats.worldWaterLevel-GetEntity()->GetWorldPos().z,"NoSound");
		}
		else
		{
			m_stats.inWaterTimer += frameTime;

			if (m_stats.inWaterTimer > 1000.0f)
				m_stats.inWaterTimer = 1000.0f;
		}
	}
	else
	{
		if (m_stats.inWaterTimer > 0.0f)
		{
			m_stats.inWaterTimer = 0.0f;
		}
		else
		{
			m_stats.inWaterTimer -= frameTime;

			if (m_stats.inWaterTimer < -1000.0f)
				m_stats.inWaterTimer = -1000.0f;
		}
	}

	// This is currently not used anywhere (David: and I don't see what this would be used for. Our hero is not Jesus).
	m_stats.isWalkingOnWater = false;

	// Set underwater level for sound listener.
	if (gEnv->pSoundSystem && isClient)
	{
		CCamera& camera = gEnv->pSystem->GetViewCamera();
		float cameraWaterLevel = (camera.GetPosition().z - surfacePos.z);

		IListener* pListener = gEnv->pSoundSystem->GetListener(LISTENERID_STANDARD);
		pListener->SetUnderwater(cameraWaterLevel); // TODO: Make sure listener interface expects zero at surface and negative under water.

		PlaySound(ESound_Underwater, (cameraWaterLevel < 0.0f));
	}

	// DEBUG RENDERING
	bool debugSwimming = (g_pGameCVars->cl_debugSwimming != 0);
	if (debugSwimming && (playerWaterLevel > -10.0f) && (playerWaterLevel < 10.0f))
	{
		Vec3 vRight(m_baseQuat.GetColumn0());

		static ColorF referenceColor(1,1,1,1);
		static ColorF surfaceColor1(0,0.5f,1,1);
		static ColorF surfaceColor0(0,0,0.5f,0);
		static ColorF bottomColor(0,0.5f,0,1);

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(referencePos, 0.1f, referenceColor);

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(referencePos, surfaceColor1, surfacePos, surfaceColor1, 2.0f);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(surfacePos, 0.2f, surfaceColor1);
		gEnv->pRenderer->DrawLabel(referencePos + vRight * 0.5f, 1.5f, "WaterLevel %3.2f (HWL %3.2f) (SurfaceZ %3.2f)", playerWaterLevel, headWaterLevel, surfacePos.z);
		gEnv->pRenderer->DrawLabel(referencePos + vRight * 0.5f - Vec3(0,0,-0.2f), 1.5f, "InWaterTimer %3.2f", m_stats.inWaterTimer);

		static int lines = 16;
		static float radius0 = 0.5f;
		static float radius1 = 1.0f;
		static float radius2 = 2.0f;
		for (int i = 0; i < lines; ++i)
		{
			float radians = ((float)i / (float)lines) * gf_PI2;
			Vec3 offset0(radius0 * cry_cosf(radians), radius0 * cry_sinf(radians), 0);
			Vec3 offset1(radius1 * cry_cosf(radians), radius1 * cry_sinf(radians), 0);
			Vec3 offset2(radius2 * cry_cosf(radians), radius2 * cry_sinf(radians), 0);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(surfacePos+offset0, surfaceColor0, surfacePos+offset1, surfaceColor1, 2.0f);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(surfacePos+offset1, surfaceColor1, surfacePos+offset2, surfaceColor0, 2.0f);
		}

		if (bottomDepth > 0.0f)
		{
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(referencePos, bottomColor, bottomPos, bottomColor, 2.0f);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(bottomPos, 0.2f, bottomColor);
			gEnv->pRenderer->DrawLabel(bottomPos + Vec3(0,0,0.5f) - vRight * 0.5f, 1.5f, "BottomDepth %3.3f", bottomDepth);
		}
	}
}

//------------------------------------------------------------------------
void CPlayer::UpdateUWBreathing(float frameTime, Vec3 worldBreathPos)
{
	if (!IsPlayer())
		return;

	bool debugSwimming = (g_pGameCVars->cl_debugSwimming != 0);

	bool breath = (m_stats.headUnderWaterTimer > 0.0f);

	static float drowningStartDelay = 60.0f;
	if ((m_stats.headUnderWaterTimer > drowningStartDelay) && (GetHealth() > 0))	// player drowning
	{
		static float energyDrainDuration = 30.0f;
		float energyDrainRate = NANOSUIT_ENERGY / energyDrainDuration;
		float energy = m_pNanoSuit->GetSuitEnergy();
		energy -= (m_pNanoSuit->GetEnergyRechargeRate() + energyDrainRate) * frameTime;
		if (gEnv->bServer)
			m_pNanoSuit->SetSuitEnergy(energy);

		if (energy <= 0)
		{
			breath = false;

			static float drownEffectDelay = 1.0f;
			m_drownEffectDelay -= frameTime;
			if (m_drownEffectDelay < 0.0f)
			{
				static float healthDrainDuration = 10.0f;
				float healthDrainRate = (float)GetMaxHealth() / healthDrainDuration;
				float damage = (int)(healthDrainRate * drownEffectDelay);
				damage += m_pNanoSuit->GetHealthRegenRate() * drownEffectDelay;
				if (gEnv->bServer)
				{
					HitInfo hitInfo(GetEntityId(), GetEntityId(), GetEntityId(), -1, 0, 0, -1, 0, ZERO, ZERO, ZERO);
					hitInfo.SetDamage(damage);

					if (CGameRules* pGameRules=g_pGame->GetGameRules())
						pGameRules->ServerHit(hitInfo);
				}

				m_drownEffectDelay = drownEffectDelay; // delay until effect is retriggered (sound and screen flashing).

				PlaySound(ESound_Drowning, true);

				IMaterialEffects* pMaterialEffects = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects();
				SMFXRunTimeEffectParams params;
				params.pos = GetEntity()->GetWorldPos();
				params.soundSemantic = eSoundSemantic_HUD;
				TMFXEffectId id = pMaterialEffects->GetEffectIdByName("player_fx", "player_damage_armormode");
				pMaterialEffects->ExecuteEffect(id, params);
			}
		}
	}
	else
	{
		m_drownEffectDelay = 0.0f;
	}

	if (breath && (m_stats.headUnderWaterTimer > 0.0f))
	{
		static float breathInDuration = 2.0f;
		static float breathOutDuration = 2.0f;
		if (m_underwaterBubblesDelay >= breathInDuration)
		{
			PlaySound(ESound_UnderwaterBreathing, false); // This sound is fake-looping to keep a handle. Need to release the handle here before we start a new one.
			PlaySound(ESound_UnderwaterBreathing, true); // Start new 'one-shot' instance.
			m_underwaterBubblesDelay = -0.01f;
		}
		else if (m_underwaterBubblesDelay <= -breathOutDuration)
		{
			if (!IsClient() || IsThirdPerson())
				SpawnParticleEffect("misc.underwater.player_breath", worldBreathPos, GetEntity()->GetWorldRotation().GetColumn1());
			else
				SpawnParticleEffect("misc.underwater.player_breath_FP", worldBreathPos, GetEntity()->GetWorldRotation().GetColumn1());

			m_underwaterBubblesDelay = +0.01f;
		}

		float breathDebugFraction = 0.0f;
		if (m_underwaterBubblesDelay >= 0.0f)
		{
			m_underwaterBubblesDelay += frameTime;
			breathDebugFraction = 1.0f - m_underwaterBubblesDelay / breathInDuration;
		}
		else
		{
			m_underwaterBubblesDelay -= frameTime;
			breathDebugFraction = -m_underwaterBubblesDelay / breathOutDuration;
		}

		if (debugSwimming)
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(worldBreathPos, 0.2f + 0.2f * breathDebugFraction, ColorB(128,196,255,255));
	}
	else
	{
		m_underwaterBubblesDelay = 0.0f;
		PlaySound(ESound_UnderwaterBreathing, false);
	}

}

//------------------------------------------------------------------------
//TODO: Clean up this whole function, unify this with CAlien via CActor.
void CPlayer::UpdateStats(float frameTime)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	if (!GetEntity())
		return;

	bool isClient(IsClient());
	//update god mode status
	if (isClient)
	{
		SAFE_HUD_FUNC(SetGODMode(g_pGameCVars->g_godMode));

		//FIXME:move this when the UpdateDraw function in player.lua gets moved here
		/*if (m_stats.inWater>0.1f)
			m_stats.firstPersonBody = 0;//disable first person body when swimming
		else*/
			m_stats.firstPersonBody = (uint8)g_pGameCVars->cl_fpBody;
	}	

	IAnimationGraphState* pAGState = GetAnimationGraphState();

	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();

	if (!pPhysEnt)
		return;

  if (gEnv->pSystem->IsDedicated() && GetLinkedVehicle())
  {
    // leipzig: force inactive (was active on ded. servers)
    pe_player_dynamics paramsGet;
    if (pPhysEnt->GetParams(&paramsGet))
    {      
      if (paramsGet.bActive && m_pAnimatedCharacter)
				m_pAnimatedCharacter->RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "Player::UpdateStats");
    }			
  }  

  bool isPlayer(IsPlayer());

	// Update always the logical representation.
	// [Mikko] The logical representation used to be updated later in the function
	// but since an animation driven movement can cause the physics to be disabled,
	// We update these valued before handling the non-collider case below.
	CHECKQNAN_VEC(m_modelOffset);
	Interpolate(m_modelOffset,GetStanceInfo(m_stance)->modelOffset,5.0f,frameTime);
	CHECKQNAN_VEC(m_modelOffset);

	CHECKQNAN_VEC(m_eyeOffset);
	//players use a faster interpolation, and using only the Z offset. A bit different for AI.
	if (isPlayer)
		Interpolate(m_eyeOffset,GetStanceViewOffset(m_stance),15.0f,frameTime);
	else
		Interpolate(m_eyeOffset,GetStanceViewOffset(m_stance,&m_stats.leanAmount,true),5.0f,frameTime);
	CHECKQNAN_VEC(m_eyeOffset);

	CHECKQNAN_VEC(m_weaponOffset);
	Interpolate(m_weaponOffset,GetWeaponOffsetWithLean(m_stance, m_stats.leanAmount, m_eyeOffset),5.0f,frameTime);
	CHECKQNAN_VEC(m_weaponOffset);
	
	pe_player_dynamics simPar;

	if (pPhysEnt->GetParams(&simPar) == 0)
	{
		int simParType = simPar.type;
		memset(&simPar, 0, sizeof(pe_player_dynamics));
		simPar.type = simParType;
	}

	//if (GetLinkedVehicle())
	const SAnimationTarget * pTarget = NULL;
	if(GetAnimationGraphState())
		pTarget = GetAnimationGraphState()->GetAnimationTarget();
	bool forceForAnimTarget = false;
	if (pTarget && pTarget->doingSomething)
		forceForAnimTarget = true;

	UpdateSwimStats(frameTime);

	//FIXME:
	if ((m_stats.spectatorMode || (!simPar.bActive && !isClient) || m_stats.flyMode || GetLinkedVehicle()) && !forceForAnimTarget)
	{
		ChangeParachuteState(0);

		m_stats.velocity = m_stats.velocityUnconstrained.Set(0,0,0);
		m_stats.speed = m_stats.speedFlat = 0.0f;
		m_stats.fallSpeed = 0.0f;
		m_stats.inFiring = 0;
		m_stats.jumpLock = 0;
		m_stats.inWaterTimer = -1000.0f;
		m_stats.groundMaterialIdx = -1;

		pe_player_dynamics simParSet;
		simParSet.bSwimming = true;
		simParSet.gravity.zero();
		pPhysEnt->SetParams(&simParSet);

		bool shouldReturn = false;

		if (!m_stats.isOnLadder && !IsFrozen())  //Underwater ladders ... 8$
		{
/*
			// We should not clear any of these. They should be maintained properly.
			// The worldWaterLevel is used to calculate the surface wave delta (for sticky surface).
			// Also, ShouldSwim() will anyway prevent swimming while in a vehicle, etc.
			m_stats.relativeWaterLevel = 0.0f;
			m_stats.relativeBottomDepth = 0.0f;
			m_stats.headUnderWaterTimer = 0.0f;
			m_stats.worldWaterLevel = 0.0f;
*/
			shouldReturn = true;
		}
		else
		{
      if (m_stats.isOnLadder)
      {
        assert((m_stats.ladderTop - m_stats.ladderBottom).GetLengthSquared() > 0.0001f);
			  m_stats.upVector = (m_stats.ladderTop - m_stats.ladderBottom).GetNormalized();
      }
      
      if(!ShouldSwim())
				shouldReturn = true;
		}

		if(shouldReturn)
		{
			//UpdateDrowning(frameTime);
			return;
		}

	}

	//retrieve some information about the status of the player
	pe_status_dynamics dynStat;
	pe_status_living livStat;

	int dynStatType = dynStat.type;
	memset(&dynStat, 0, sizeof(pe_status_dynamics));
	dynStat.type = dynStatType;

	int livStatType = livStat.type;
	memset(&livStat, 0, sizeof(pe_status_living));
	livStat.groundSlope = Vec3(0,0,1);
	livStat.type = livStatType;

	pPhysEnt->GetStatus(&dynStat);
	pPhysEnt->GetStatus(&livStat);

	m_stats.physCamOffset = livStat.camOffset;
	m_stats.gravity = simPar.gravity;

	{
		float groundNormalBlend = CLAMP(frameTime / 0.15f, 0.0f, 1.0f);
		m_stats.groundNormal = LERP(m_stats.groundNormal, livStat.groundSlope, groundNormalBlend);
	}

	if (livStat.groundSurfaceIdxAux > 0)
		m_stats.groundMaterialIdx = livStat.groundSurfaceIdxAux;
	else
		m_stats.groundMaterialIdx = livStat.groundSurfaceIdx;

	Vec3 ppos(GetWBodyCenter());

	bool bootableSurface(false);

	bool groundMatBootable(IsMaterialBootable(m_stats.groundMaterialIdx));

	if (GravityBootsOn() && m_stats.onGroundWBoots>=0.0f)
	{
		bootableSurface = true;

		Vec3 surfaceNormal(0,0,0);
		int surfaceNum(0);

		ray_hit hit;
		int rayFlags = (COLLISION_RAY_PIERCABILITY & rwi_pierceability_mask);

		Vec3 vUp(m_baseQuat.GetColumn2());


		if (m_stats.onGroundWBoots>0.001f)
		{
			Vec3 testSpots[5];
			testSpots[0] = vUp * -1.3f;

			Vec3 offset(dynStat.v * 0.35f);
			Vec3 vRight(m_baseQuat.GetColumn0());
			Vec3 vForward(m_baseQuat.GetColumn1());
			
			testSpots[1] = testSpots[0] + vRight * 0.75f + offset;
			testSpots[2] = testSpots[0] + vForward * 0.75f + offset;
			testSpots[3] = testSpots[0] - vRight * 0.75f + offset;
			testSpots[4] = testSpots[0] - vForward * 0.75f + offset;

			for(int i=0;i<5;++i)
			{
				if (gEnv->pPhysicalWorld->RayWorldIntersection(ppos, testSpots[i], ent_terrain|ent_static|ent_rigid, rayFlags, &hit, 1, &pPhysEnt, 1) && IsMaterialBootable(hit.surface_idx))
				{
					surfaceNormal += hit.n;
					//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(hit.pt, ColorB(0,255,0,100), hit.pt + hit.n, ColorB(255,0,0,100));
					++surfaceNum;
				}
			}
		}
		else
		{
			if (gEnv->pPhysicalWorld->RayWorldIntersection(ppos, vUp * -1.3f, ent_terrain|ent_static|ent_rigid, rayFlags, &hit, 1, &pPhysEnt, 1) && IsMaterialBootable(hit.surface_idx))
			{
				surfaceNormal = hit.n;
				surfaceNum = 1;
			}
		}

		Vec3 gravityVec;

		if (surfaceNum)
		{
			Vec3 newUp(surfaceNormal/(float)surfaceNum);

			m_stats.upVector = Vec3::CreateSlerp(m_stats.upVector,newUp.GetNormalized(),min(3.0f*frameTime, 1.0f));
      
			gravityVec = -m_stats.upVector * 9.81f;
			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(ppos, ColorB(255,255,0,255), ppos - gravityVec, ColorB(255,255,0,255));
		}
		else 
		{
			m_stats.upVector = m_upVector;      
			gravityVec = -m_stats.upVector * 9.81f;
		}

		if (!livStat.bFlying || m_stats.onGroundWBoots>0.001f)
		{
			if (groundMatBootable)
				m_stats.onGroundWBoots = 0.5f;

			livStat.bFlying = false;

			pe_player_dynamics newGravity;
			newGravity.gravity = gravityVec;
			pPhysEnt->SetParams(&newGravity);
		}
	}
	else if (InZeroG())
	{
		m_stats.upVector = m_upVector;    

		m_stats.jumped = false;
		if(pAGState)
		{
			char action[256];
			pAGState->GetInput(m_inputAction, action);
			if ( strcmp(action,"jumpMP")==0 || strcmp(action,"jumpMPStrength")==0 )
				pAGState->SetInput(m_inputAction, "idle");
		}
	}
	else
	{
		if (m_stance != STANCE_PRONE)
			m_stats.upVector.Set(0,0,1);
		else
		{
			m_stats.upVector.Set(0,0,1);
			//m_stats.upVector = m_stats.groundNormal;
		}
	}

	//
	if (m_stats.forceUpVector.len2()>0.01f)
	{
		m_stats.upVector = m_stats.forceUpVector;    
		m_stats.forceUpVector.zero(); 
	}
	//


	DebugGraph_AddValue("PhysVelo", livStat.vel.GetLength());
	DebugGraph_AddValue("PhysVeloX", livStat.vel.x);
	DebugGraph_AddValue("PhysVeloY", livStat.vel.y);
	DebugGraph_AddValue("PhysVeloZ", livStat.vel.z);

	DebugGraph_AddValue("PhysVeloUn", livStat.velUnconstrained.GetLength());
	DebugGraph_AddValue("PhysVeloUnX", livStat.velUnconstrained.x);
	DebugGraph_AddValue("PhysVeloUnY", livStat.velUnconstrained.y);
	DebugGraph_AddValue("PhysVeloUnZ", livStat.velUnconstrained.z);

	bool onGround = false;
	bool isFlying = (livStat.bFlying != 0);
	bool isStuck = (livStat.bStuck != 0);
	// if not flying then character is currently touching ground.
	// if flying but was not flying character is seen as on ground still, unless he has jumped.
	// wasFlying tracks the last frame, since flying and stuck altenates/toggles when living entity is stuck, in which case we wanna move and jump as if on ground.
	// Given the stuck timeout wasFlying might be redundant.
	if (!isFlying || (!m_stats.wasFlying && !m_stats.jumped) || (m_stats.stuckTimeout > 0.0f))
	{
		if (livStat.groundHeight > -1E10f) // Sanity check to prohibit "false landing" on quickload (physics/isFlying not updated in first frame)
			onGround = true; 
	}
	
	if (isStuck)
		m_stats.stuckTimeout = 0.3f;
	else
		m_stats.stuckTimeout = max(0.0f, m_stats.stuckTimeout - frameTime);

	m_stats.wasStuck = isStuck;
	m_stats.wasFlying = isFlying;

	if (livStat.groundHeight > -1E10f)
	{
		float feetHeight = GetEntity()->GetWorldPos().z;
		float feetElevation = (feetHeight - livStat.groundHeight);
		// Only consider almost on ground if not jumped and not yet having been well in the air.
		if ((feetElevation < 0.1f) && !m_stats.jumped && (m_stats.inAir == 0.0f))
			onGround = true;
	}

//*
	DebugGraph_AddValue("OnGround", onGround ? 1.0f : 0.0f);
	DebugGraph_AddValue("Jumping", m_stats.jumped ? 1.0f : 0.0f);
	DebugGraph_AddValue("Flying", isFlying ? 1.0f : 0.0f);
	DebugGraph_AddValue("StuckTimer", m_stats.stuckTimeout);
	DebugGraph_AddValue("InAirTimer", m_stats.inAir);
	DebugGraph_AddValue("OnGroundTimer", m_stats.onGround);
	DebugGraph_AddValue("InWaterTimer", m_stats.inWaterTimer);
/**/

	//update status table
	if (!onGround && !m_stats.spectatorMode && !GetLinkedVehicle())
	{
		if (ShouldSwim())
		{
			if(pAGState)
			{
				char action[256];
				pAGState->GetInput(m_inputAction, action);
				if ( strcmp(action,"jumpMP")==0 || strcmp(action,"jumpMPStrength")==0 )
					pAGState->SetInput(m_inputAction, "idle");
			}
		}
		else
		{
			//no freefalling in zeroG
			if (!InZeroG() && !m_stats.inFreefall && (m_stats.fallSpeed > (g_pGameCVars->pl_fallDamage_Normal_SpeedFatal - 1.0f))) //looks better if happening earlier
			{
				float terrainHeight = gEnv->p3DEngine->GetTerrainZ((int)GetEntity()->GetWorldPos().x, (int)GetEntity()->GetWorldPos().y);
				float playerHeight = GetEntity()->GetWorldPos().z;
				if(playerHeight > terrainHeight + 2.5f)
				{
					//CryLogAlways("%f %f %f", terrainHeight, playerHeight, playerHeight-terrainHeight);
					ChangeParachuteState(1);
				}
			}

			m_stats.inAir += frameTime;
			// Danny - changed this since otherwise AI has a fit going down stairs - it seems that when off
			// the ground it can only decelerate.
			// If you revert this (as it's an ugly workaround) - test AI on stairs!
			static float minTimeForOffGround = 0.5f;
			if (m_stats.inAir > minTimeForOffGround || isPlayer)
				m_stats.onGround = 0.0f;

			if (m_stats.isOnLadder)
				m_stats.inAir = 0.0f;
		}
	}
	else
	{
		if (bootableSurface && m_stats.onGroundWBoots<0.001f && !groundMatBootable)
		{
			bootableSurface = false;
		}
		else
		{
			bool landed = false;
			// Requiring a jump to land prevents landing when simply falling of a cliff.
			// Keep commented until when the reason for putting the jump condition in reappears.
			if (/*m_stats.jumped &&*/ (m_stats.inAir > 0.0f) && onGround && !m_stats.isOnLadder)
				landed = true;

			// This is needed when jumping while standing directly under something that causes an immediate land,
			// before and without even being airborne for one frame.
			// PlayerMovement set m_stats.onGround=0.0f when the jump is triggered,
			// which prevents the on ground timer from before the jump to be inherited
			// and incorrectly and prematurely used to identify landing in MP.
			bool jumpFailed = (m_stats.jumped && (m_stats.onGround > 0.0f) && (livStat.velUnconstrained.z <= 0.0f));
		
			if (jumpFailed)
				m_stats.jumped = false;

			if (landed)
			{
				if (m_stats.inAir > 0.3f)
					m_stats.landed = true; // This is only used for landing camera bobbing in PlayerView.cpp.

				m_stats.jumped = false;
				m_stats.jumpLock = 0.2f;
				Landed(m_stats.fallSpeed); // fallspeed might be incorrect on a dedicated server (pos is synced from client, but also smoothed).
			}
			else
			{
				m_stats.landed = false;
			}

			m_stats.onGround += frameTime;
			m_stats.inAir = 0.0f;

			if (landed || ShouldSwim() || jumpFailed)
			{
				if(pAGState)
				{
					char action[256];
					pAGState->GetInput(m_inputAction, action);
					if ( strcmp(action,"jumpMP")==0 || strcmp(action,"jumpMPStrength")==0 )
						pAGState->SetInput(m_inputAction, "idle");
				}
			}

			if (landed && m_stats.fallSpeed)
			{
				CreateScriptEvent("landed",m_stats.fallSpeed);
				m_stats.fallSpeed = 0.0f;

				Vec3 playerPos = GetEntity()->GetWorldPos();
				
				if(playerPos.z<m_stats.worldWaterLevel)
					CreateScriptEvent("jump_splash",m_stats.worldWaterLevel-playerPos.z);
			}

			ChangeParachuteState(0);
		}
	}

	Vec3 prevVelocityU = m_stats.velocityUnconstrained;
	m_stats.velocity = livStat.vel-livStat.velGround;//dynStat.v;
	m_stats.velocityUnconstrained = livStat.velUnconstrained;

	// On dedicated server the player can still be flying this frame as well, 
	// since synced pos from client is interpolated/smoothed and will not land immediately,
	// even though the velocity is set to zero.
	// Thus we need to use the velocity change instead of landing to identify impact.
	m_stats.downwardsImpactVelocity = max(0.0f, min(m_stats.velocityUnconstrained.z, 0.0f) - min(prevVelocityU.z, 0.0f));

	// If you land while in a vehicle, don't apply damage.
	if (GetLinkedVehicle() != NULL)
		m_stats.downwardsImpactVelocity = 0.0f;

	// If you land in deep enough water, don't apply damage. 
	// (In MP the remote ShouldSwim() might lag behind though, so you will get damage unless you have a parachute).
	if (ShouldSwim())
		m_stats.downwardsImpactVelocity = 0.0f;

	// Zero downwards impact velocity used for fall damage calculations if player was in water within the last 0.5 seconds.
	// Strength jumping straight up and down should theoretically land with a safe velocity, 
	// but together with the water surface stickyness the velocity can sometimes go above the safe impact velocity threshold.
	if (m_stats.inWaterTimer > -0.5f)
		m_stats.downwardsImpactVelocity = 0.0f;

	// If you land in with a parachute, don't apply damage.
	// You will still get damage if you don't open a parachute in time.
	if (m_stats.inFreefall.Value() == 2)
		m_stats.downwardsImpactVelocity = 0.0f;

	// Apply Fall Damage
	assert(NumberValid(m_stats.downwardsImpactVelocity));
	if (m_stats.downwardsImpactVelocity > 0.0f)
	{
		float velSafe = g_pGameCVars->pl_fallDamage_Normal_SpeedSafe;
		float velFatal = g_pGameCVars->pl_fallDamage_Normal_SpeedFatal;

		float velFraction = 1.0f;
		if (velFatal > velSafe)
			velFraction = (m_stats.downwardsImpactVelocity - velSafe) / (velFatal - velSafe);
		assert(NumberValid(velFraction));

		if (velFraction > 0.0f)
		{
			velFraction = powf(velFraction, g_pGameCVars->pl_fallDamage_SpeedBias);

			CNanoSuit* pSuit = GetNanoSuit();
			float maxEnergy = (pSuit != NULL) ? NANOSUIT_ENERGY : 0.0f;
			float damagePerEnergy = 1.0f;

			if (IGameRules *pGameRules=g_pGame->GetGameRules())
			{
				if (IScriptTable *pGameRulesScript=pGameRules->GetEntity()->GetScriptTable())
				{
					HSCRIPTFUNCTION pfnGetEnergyAbsorptionValue = 0;
					if (pGameRulesScript->GetValue("GetEnergyAbsorptionValue", pfnGetEnergyAbsorptionValue) && pfnGetEnergyAbsorptionValue)
					{
						Script::CallReturn(gEnv->pScriptSystem, pfnGetEnergyAbsorptionValue, pGameRulesScript, damagePerEnergy);
						gEnv->pScriptSystem->ReleaseFunc(pfnGetEnergyAbsorptionValue);
					}
				}
			}

			float maxHealth = (float)GetMaxHealth();
			float maxDamage = maxHealth + (maxEnergy * damagePerEnergy);

			HitInfo hit;
			hit.damage = velFraction * maxDamage;
			hit.targetId = GetEntity()->GetId();
			hit.shooterId = GetEntity()->GetId();

			if (m_stats.inFreefall.Value() == 1)
				hit.damage = 1000.0f;

			// Apply actual damage only on server.
			if (gEnv->bServer)
			{
				g_pGame->GetGameRules()->ServerHit(hit);

				// workaround PATCH2 : Apply previous downward velocity to make ragdoll fall with continuous velocity.
				// Ragdoll is spawned in mid air (safely inside collider), but with zero velocity (entity collides to full stop).
				IPhysicalEntity* pRagDoll = GetEntity()->GetPhysics();
				if (pRagDoll->GetType() == PE_ARTICULATED)
				{
					// Restore previous velocity
					pe_action_set_velocity v;
					v.v = Vec3(0, 0, -m_stats.downwardsImpactVelocity);
					for(v.ipart=0; v.ipart<15; v.ipart++)
						pRagDoll->Action(&v);
				}
			}

			if (g_pGameCVars->pl_debugFallDamage != 0)
			{
				char* side = gEnv->bServer ? "Server" : "Client";

				char* color = "";
				if (velFraction < 0.33f)
					color = "$6"; // Yellow
				else if (velFraction < 0.66f)
					color = "$8"; // Orange
				else
					color = "$4"; // Red

				CryLogAlways("%s[%s][%s] ImpactVelo=%3.2f, FallSpeed=%3.2f, FallDamage=%3.1f (ArmorMode=%2.0f%%, NonArmorMode=%2.0f%%)", 
					color, side, GetEntity()->GetName(), m_stats.downwardsImpactVelocity, m_stats.fallSpeed, 
					hit.damage, hit.damage / maxDamage * 100.0f, hit.damage / maxHealth * 100.0f);
			}
		}
		else if (g_pGameCVars->pl_debugFallDamage != 0)
		{
			if (m_stats.downwardsImpactVelocity > 0.5f)
			{
				char* side = gEnv->bServer ? "Server" : "Client";
				char* color = "$3"; // Green
				CryLogAlways("%s[%s][%s] ImpactVelo=%3.2f, FallSpeed=%3.2f, FallDamage: NONE", 
					color, side, GetEntity()->GetName(), m_stats.downwardsImpactVelocity, m_stats.fallSpeed);
			}
		}
	}


	//calculate the flatspeed from the player ground orientation
	Vec3 flatVel(m_baseQuat.GetInverted()*m_stats.velocity);
	if(!m_stats.isOnLadder)
		flatVel.z = 0;
	m_stats.speedFlat = flatVel.len();

	if (m_stats.inAir && m_stats.velocity*m_stats.gravity>0.0f && (m_stats.inWaterTimer <= 0.0f) && !m_stats.isOnLadder)
	{
		if (!m_stats.fallSpeed)
			CreateScriptEvent("fallStart",0);

		m_stats.fallSpeed = -m_stats.velocity.z;
	}
	else
	{
		m_stats.fallSpeed = 0.0f;
		//CryLogAlways( "[player] end falling %f", ppos.z);
	}

	if (g_pGameCVars->pl_debugFallDamage == 2)
	{
		Vec3 pos = GetEntity()->GetWorldPos();
		char* side = gEnv->bServer ? "Sv" : "Cl";
		CryLogAlways("[%s] liv.vel=%0.1f,%0.1f,%3.2f liv.velU=%0.1f,%0.1f,%3.2f impactVel=%3.2f posZ=%3.2f (liv.velReq=%0.1f,%0.1f,%3.2f) (fallspeed=%3.2f) gt=%3.3f, pt=%3.3f", 
								side, 
								livStat.vel.x, livStat.vel.y, livStat.vel.z, 
								livStat.velUnconstrained.x, livStat.velUnconstrained.y, livStat.velUnconstrained.z, 
								m_stats.downwardsImpactVelocity, 
								/*pos.x, pos.y,*/ pos.z, 
								livStat.velRequested.x, livStat.velRequested.y, livStat.velRequested.z, 
								m_stats.fallSpeed, 
								gEnv->pTimer->GetCurrTime(), gEnv->pPhysicalWorld->GetPhysicsTime());
	}

	m_stats.mass = dynStat.mass;

	if (m_stats.speedFlat>0.1f)
	{
		m_stats.inMovement += frameTime;
		m_stats.inRest = 0;
	}
	else
	{
		m_stats.inMovement = 0;
		m_stats.inRest += frameTime;
	}

	if (ShouldSwim())
	{
		m_stats.inAir = 0.0f;
		ChangeParachuteState(0);
	}

	/*if (m_stats.inAir>0.001f)
	Interpolate(m_modelOffset,GetStanceInfo(m_stance)->modelOffset,3.0f,frameTime,3.0f);
	else*/

	if (livStat.groundHeight>-0.001f)
		m_groundElevation = livStat.groundHeight - ppos*GetEntity()->GetRotation().GetColumn2();

	if (m_stats.inAir<0.5f)
	{
		/*Vec3 touch_point = ppos + GetEntity()->GetRotation().GetColumn2()*m_groundElevation;
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(touch_point, ColorB(0,255,255,255), touch_point - GetEntity()->GetRotation().GetColumn2()*m_groundElevation, ColorB(0,255,255,255));   
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(touch_point,0.12f,ColorB(0,255,0,100) );*/

		//Interpolate(m_modelOffset.z,m_groundElevation,5.0f,frameTime);
		//m_modelOffset.z = m_groundElevation;
	}



	//
	pe_player_dimensions ppd;
/*	switch (m_stance)
	{
	default:
	case STANCE_STAND:ppd.heightHead = 1.6f;break;
	case STANCE_CROUCH:ppd.heightHead = 0.8f;break;
	case STANCE_PRONE:ppd.heightHead = 0.35f;break;
	}
	*/
	ppd.headRadius = 0.0f;
	pPhysEnt->SetParams(&ppd);

	pe_player_dynamics simParSet;
	bool shouldSwim = ShouldSwim();
	simParSet.bSwimming = (m_stats.spectatorMode || m_stats.flyMode || shouldSwim || (InZeroG()&&!bootableSurface) || m_stats.isOnLadder);
	//set gravity to 0 in water
	if ((shouldSwim && m_stats.relativeWaterLevel <= 0.0f) || m_stats.isOnLadder)
		simParSet.gravity.zero();

	pPhysEnt->SetParams(&simParSet);

	//update some timers
	m_stats.inFiring = max(0.0f,m_stats.inFiring - frameTime);
	m_stats.jumpLock = max(0.0f,m_stats.jumpLock - frameTime);
//	m_stats.waitStance = max(0.0f,m_stats.waitStance - frameTime);

	if (m_stats.onGroundWBoots < 0.0f)
		m_stats.onGroundWBoots = min(m_stats.onGroundWBoots + frameTime,0.0f);
	else
		m_stats.onGroundWBoots = max(m_stats.onGroundWBoots - frameTime,0.0f);

	//m_stats.thrusterSprint = min(m_stats.thrusterSprint + frameTime, 1.0f);
}


//
//-----------------------------------------------------------------------------
void CPlayer::ToggleThirdPerson()
{
	m_stats.isThirdPerson = !m_stats.isThirdPerson;

	CALL_PLAYER_EVENT_LISTENERS(OnToggleThirdPerson(this,m_stats.isThirdPerson));
}

int CPlayer::IsGod()
{
	if (!m_pGameFramework->CanCheat())
		return 0;

	int godMode(g_pGameCVars->g_godMode);

	// Demi-Gods are not Gods
	if (godMode == 3)
		return 0;

	//check if is the player
	if (IsClient())
		return godMode;

	//check if is a squadmate
	IAIActor* pAIActor = CastToIAIActorSafe(GetEntity()->GetAI());
	if (pAIActor)
	{
		int group=pAIActor->GetParameters().m_nGroup;
		if(group>= 0 && group<10)
			return (godMode==2?1:0);
	}
	return 0;
}

bool CPlayer::IsThirdPerson() const
{
	//force thirdperson view for non-clients
	if (!IsClient())
		return true;
	
  return m_stats.isThirdPerson;	
}


void CPlayer::Revive( bool fromInit )
{
	CActor::Revive(fromInit);

	ResetScreenFX();

  InitInterference();

	if (CNanoSuit *pSuit=GetNanoSuit())
	{
		bool active=pSuit->IsActive(); // CNanoSuit::Reset resets the active flag
		
		pSuit->Reset(this);

		if (active || IsPlayer())
			ActivateNanosuit(true);
	}

	m_parachuteEnabled = false; // no parachute by default
	m_openParachuteTimer = -1.0f;
	m_openingParachute = false;

	m_bSwimming = false;
	m_actions = 0;
	m_forcedRotation = false;
	m_bStabilize = false;
	m_fSpeedLean = 0.0f;
	m_bRagDollHead = false;

	m_frozenAmount = 0.0f;

	m_viewAnglesOffset.Set(0,0,0);
	
  if (IsClient() && IsThirdPerson())
    ToggleThirdPerson();

	// HAX: to fix player spawning and floating in dedicated server: Marcio fix me?
	if (gEnv->pSystem->IsEditor() == false)  // AlexL: in editor, we never keep spectator mode
	{
		uint8 spectator=m_stats.spectatorMode;
		m_stats = SPlayerStats();
		m_stats.spectatorMode=spectator;
	}
	else
	{
		m_stats = SPlayerStats();
	}

	m_headAngles.Set(0,0,0);
	m_eyeOffset.Set(0,0,0);
	m_eyeOffsetView.Set(0,0,0);
	m_modelOffset.Set(0,0,0);
	m_weaponOffset.Set(0,0,0);
	m_groundElevation = 0.0f;

	m_velocity.Set(0,0,0);
	m_bobOffset.Set(0,0,0);

	m_FPWeaponOffset.Set(0,0,0);
	m_FPWeaponAngleOffset.Set(0,0,0);
	m_FPWeaponLastDirVec.Set(0,0,0);

	m_feetWpos[0] = ZERO;
	m_feetWpos[1] = ZERO;
	m_lastAnimContPos = ZERO;

	m_angleOffset.Set(0,0,0);

	//m_baseQuat.SetIdentity();
	m_viewQuatFinal = m_baseQuat = m_viewQuat = GetEntity()->GetRotation();
	m_clientViewMatrix.SetIdentity();

	m_turnTarget = GetEntity()->GetRotation();
	m_lastRequestedVelocity.Set(0,0,0);
  m_lastAnimControlled = 0.f;

	m_viewRoll = 0;
	m_upVector.Set(0,0,1);

	m_viewBlending = true;
	m_fDeathTime = 0.0f;
	
	m_fLowHealthSoundMood = 0.0f;

	m_bConcentration = false;
	m_fConcentrationTimer = -1.0f;

	m_bVoiceSoundPlaying = false;

	GetEntity()->SetFlags(GetEntity()->GetFlags() | (ENTITY_FLAG_CASTSHADOW));
	GetEntity()->SetSlotFlags(0,GetEntity()->GetSlotFlags(0)|ENTITY_SLOT_RENDER);

	if (m_pPlayerInput.get())
		m_pPlayerInput->Reset();

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);

	if (pCharacter)
		pCharacter->EnableStartAnimation(true);

	ResetAnimations();
	// if we're coming from initialize, then we're not part of the game object yet -- therefore we can't
	// receive events from the animation graph, and hence, we'll delay the initial update until PostInit()
	// is called...
	//if (!fromInit)
	//	UpdateAnimGraph();

	//	m_nanoSuit.Reset(this);
	//	m_nanoSuit.Activate(m_params.nanoSuitActive);

	if (!fromInit || GetISystem()->IsSerializingFile() == 1)
		ResetAnimGraph();

	if(!fromInit && IsClient())
	{
		if(CHUD *pHUD = g_pGame->GetHUD())
		{
			pHUD->BreakHUD(0);
			pHUD->GetCrosshair()->SetUsability(0);
		}
		
		if(gEnv->bMultiplayer)
		{
			if(GetHealth() > 0 || (g_pGame && g_pGame->GetGameRules() && !g_pGame->GetGameRules()->IsPlayerActivelyPlaying(GetEntityId())))
			{
				// Only cancel respawn cycle timer if we're 'leaving the game' again and going to spectator mode.
				// Still want it shown if we're 3rd-person spectating and waiting to respawn...
				SAFE_HUD_FUNC(ShowReviveCycle(false));
			}
				
			IView *pView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetViewByEntityId(GetEntityId());
			if(pView)
				pView->ResetShaking();
				
				// stop menu music
				if(gEnv->pMusicSystem)
				{
					gEnv->pMusicSystem->EndTheme(EThemeFade_FadeOut, 0, true);
				}

			// cancel voice recording (else channel can be stuck open)
			g_pGame->GetIGameFramework()->EnableVoiceRecording(false);
		}

		SetPainEffect(-1.0f);
	}

	if(!GetInventory()->GetCurrentItem() && !GetISystem()->IsSerializingFile())
	{
		CFists *pFists = static_cast<CFists*>(GetItemByClass(CItem::sFistsClass));
		if(pFists)
			g_pGame->GetIGameFramework()->GetIItemSystem()->SetActorItem(this, pFists->GetEntityId());
	}

	if (!fromInit && m_pNanoSuit)
	{
		m_pNanoSuit->SetMode(NANOMODE_DEFENSE, true, true);
		m_pNanoSuit->SetCloakLevel(CLOAKMODE_REFRACTION);
		//m_pNanoSuit->ActivateMode(NANOMODE_CLOAK, false);	// cloak must be picked up or bought

		if (GetEntity()->GetAI())	//just for the case the player was still cloaked (happens in editor when exiting game cloaked)
			gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER,1, "OnNanoSuitUnCloak", GetEntity()->GetAI());
	}

	// marcio: reset pose on the dedicated server (dedicated server doesn't update animationgraphs)
	if (!gEnv->bClient && gEnv->bServer)
	{
		if (ICharacterInstance *pCharacter=GetEntity()->GetCharacter(0))
			pCharacter->GetISkeletonPose()->SetDefaultPose();
	}

	//Restore near fov to default value (60.0f) and FP weapon position, just in case
	if (IsClient())
		ResetFPView();
}

void CPlayer::Kill()
{
	if (CNanoSuit *pSuit=GetNanoSuit())
		pSuit->Death();

	// notify any claymores/mines that this player has died
	//	(they will be removed 30s later)
	RemoveAllExplosives(EXPLOSIVE_REMOVAL_TIME);

	CActor::Kill();
}

#if 0 // AlexL 14.03.2007: no more bootable materials for now. and scriptable doesn't provide custom params anyway (after optimization)

bool CPlayer::IsMaterialBootable(int matId) const
{
	ISurfaceType *pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(matId);
	IScriptTable *pTable = pSurfaceType ? pSurfaceType->GetScriptTable() : NULL;

	bool GBootable(false);

	if (pTable)
		pTable->GetValue("GBootable",GBootable);

	return GBootable;
}
#endif

Vec3 CPlayer::GetStanceViewOffset(EStance stance,float *pLeanAmt,bool withY) const
{	
	if ((m_stats.inFreefall == 1) || (m_isClient && m_stats.isGrabbed))
		return GetLocalEyePos2();

	Vec3 offset(GetStanceInfo(m_stance)->viewOffset);

	//apply leaning
	float leanAmt;
	if (!pLeanAmt)
		leanAmt = m_stats.leanAmount;
	else
		leanAmt = *pLeanAmt;

	if(IsPlayer())
	{
		if (leanAmt*leanAmt>0.01f)
		{
			offset.x += leanAmt * m_params.leanShift;
			if (stance != STANCE_PRONE)
				offset.z -= fabs(leanAmt) * m_params.leanShift * 0.33f;
		}

		if (m_stats.bSprinting && stance == STANCE_CROUCH && m_stats.inMovement > 0.1f)
			offset.z += 0.1f;
	}
	else
	{
		offset = GetStanceInfo(stance)->GetViewOffsetWithLean(leanAmt);
	}

	if (!withY)
		offset.y = 0.0f;

	return offset;
}		

void CPlayer::RagDollize( bool fallAndPlay )
{
	if (m_stats.isRagDoll && !gEnv->pSystem->IsSerializingFile())
	{
		if(!IsPlayer() && !fallAndPlay)
			DropAttachedItems();
		return;
	}

	ResetAnimations();

	CActor::RagDollize( fallAndPlay );

	m_stats.followCharacterHead = 1;

	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();

	if (pPhysEnt)
	{
		pe_simulation_params sp;
		sp.gravity = sp.gravityFreefall = m_stats.gravity;
		//sp.damping = 1.0f;
		sp.dampingFreefall = 0.0f;
		sp.mass = m_stats.mass * 2.0f;
		if(sp.mass <= 0.0f)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Tried ragdollizing player with 0 mass.");
			sp.mass = 80.0f;
		}

		pPhysEnt->SetParams(&sp);

		if(IsClient() && m_stats.inWaterTimer < 0.0f)
		{
			float terrainHeight = gEnv->p3DEngine->GetTerrainZ((int)GetEntity()->GetWorldPos().x, (int)GetEntity()->GetWorldPos().y);
			if(GetEntity()->GetWorldPos().z - terrainHeight < 3.0f)
			{
				int headBoneID = GetBoneID(BONE_HEAD);
				if(headBoneID > -1)
				{
					pe_params_part params;
					params.partid  = headBoneID;
					params.scale = 8.0f; //make sure the camera doesn't clip through the ground
					params.flagsAND = ~(geom_colltype_ray|geom_floats);
					pPhysEnt->SetParams(&params);
					m_bRagDollHead = true;
				}
			}
		}

		if (gEnv->bMultiplayer)
		{
			pe_params_part pp;
			pp.flagsAND = ~(geom_colltype_player|geom_colltype_vehicle|geom_colltype6);
			pp.flagsColliderAND=pp.flagsColliderOR = geom_colltype_debris;
			pPhysEnt->SetParams(&pp);
		}
	}

	if (!fallAndPlay)
	{
		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
		if (pCharacter)
			pCharacter->EnableStartAnimation(false);
		if(!IsPlayer())
			DropAttachedItems();
	}
	else
	{
		if (ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0))
			pCharacter->GetISkeletonPose()->Fall();
	}
}

void CPlayer::PostPhysicalize()
{
	CActor::PostPhysicalize();

	if (m_pAnimatedCharacter)
	{
		//set inertia, it will get changed again soon, with slidy surfaces and such
		SAnimatedCharacterParams params = m_pAnimatedCharacter->GetParams();
		params.SetInertia(m_params.inertia,m_params.inertiaAccel);
		params.timeImpulseRecover = GetTimeImpulseRecover();
		params.flags |= eACF_EnableMovementProcessing | eACF_ZCoordinateFromPhysics | eACF_ConstrainDesiredSpeedToXY;
		m_pAnimatedCharacter->SetParams(params);
	}

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	if (!pCharacter)
		return;
	pCharacter->GetISkeletonPose()->SetPostProcessCallback0(PlayerProcessBones,this);
	pe_simulation_params sim;
	sim.maxLoggedCollisions = 5;
	pe_params_flags flags;
	flags.flagsOR = pef_log_collisions;
	pCharacter->GetISkeletonPose()->GetCharacterPhysics()->SetParams(&sim);
	pCharacter->GetISkeletonPose()->GetCharacterPhysics()->SetParams(&flags);
	

	//set a default offset for the character, so in the editor the bbox is correct
	if(m_pAnimatedCharacter)
	{
		m_pAnimatedCharacter->SetExtraAnimationOffset(QuatT(GetStanceInfo(STANCE_STAND)->modelOffset, IDENTITY));

		// Physicalize() was overriding some things for spectators on loading (eg gravity). Forcing a collider mode update
		//	will reinit it properly.
		if(GetSpectatorMode() != CActor::eASM_None)
		{
			EColliderMode mode = m_pAnimatedCharacter->GetPhysicalColliderMode();
			m_pAnimatedCharacter->ForceRefreshPhysicalColliderMode();
			m_pAnimatedCharacter->RequestPhysicalColliderMode(mode, eColliderModeLayer_Game, "Player::PostPhysicalize" );
		}
	}

//	if (m_pGameFramework->IsMultiplayer())
//		GetGameObject()->ForceUpdateExtension(this, 0);
}

void CPlayer::UpdateAnimGraph(IAnimationGraphState * pState)
{
	CActor::UpdateAnimGraph( pState );
}

void CPlayer::PostUpdate(float frameTime)
{
	CActor::PostUpdate(frameTime);
	if (m_pPlayerInput.get())
		m_pPlayerInput->PostUpdate();
}

void CPlayer::PostRemoteSpawn()
{
	CActor::PostRemoteSpawn();

	if(gEnv->bMultiplayer && !IsClient())
	{
		CreateVoiceListener();
	}
}

void CPlayer::CreateVoiceListener()
{
	if(gEnv->bMultiplayer && gEnv->bClient && !IsClient())
	{
		// remote players need a voice listener
		if(!m_pVoiceListener)
		{
			m_pVoiceListener = GetGameObject()->AcquireExtension("VoiceListener");
		}
	}
}

void CPlayer::CameraShake(float angle,float shift,float duration,float frequency,Vec3 pos,int ID,const char* source) 
{
	float angleAmount(max(-90.0f,min(90.0f,angle)) * gf_PI/180.0f);
	float shiftAmount(shift);
  
  if (IVehicle* pVehicle = GetLinkedVehicle())
  {
    if (IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger(GetEntityId()))    
      pSeat->OnCameraShake(angleAmount, shiftAmount, pos, source);    
  }

	Ang3 shakeAngle(\
		RANDOMR(0.0f,1.0f)*angleAmount*0.15f, 
		(angleAmount*min(1.0f,max(-1.0f,RANDOM()*7.7f)))*1.15f,
		RANDOM()*angleAmount*0.05f
		);

	Vec3 shakeShift(RANDOM()*shiftAmount,0,RANDOM()*shiftAmount);

	IView *pView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetViewByEntityId(GetEntityId());
	if (pView)
		pView->SetViewShake(shakeAngle,shakeShift,duration,frequency,0.5f,ID);

	/*//if a position is defined, execute directional shake
	if (pos.len2()>0.01f)
	{
	Vec3 delta(pos - GetEntity()->GetWorldPos());
	delta.NormalizeSafe();

	float dotSide(delta * m_viewQuatFinal.GetColumn0());
	float dotFront(delta * m_viewQuatFinal.GetColumn1() - delta * m_viewQuatFinal.GetColumn2());

	float randomRatio(0.5f);
	dotSide += RANDOM() * randomRatio;
	dotFront += RANDOM() * randomRatio;

	m_viewShake.angle.Set(dotFront*shakeAngle, -dotSide*shakeAngle*RANDOM()*0.5f, dotSide*shakeAngle);
	}
	else
	{
	m_viewShake.angle.Set(RANDOMR(0.0f,1.0f)*shakeAngle, RANDOM()*shakeAngle*0.15f, RANDOM()*shakeAngle*0.75f);
	}*/
}
  

void CPlayer::ResetAnimations()
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);

	if (pCharacter)
	{
		pCharacter->GetISkeletonAnim()->StopAnimationsAllLayers();

//		if (m_pAnimatedCharacter)
//			m_pAnimatedCharacter->GetAnimationGraphState()->Pause(true, eAGP_StartGame);
		//disable any IK
		//pCharacter->SetLimbIKGoal(LIMB_LEFT_LEG);
		//pCharacter->SetLimbIKGoal(LIMB_RIGHT_LEG);
		//pCharacter->SetLimbIKGoal(LIMB_LEFT_ARM);
		//pCharacter->SetLimbIKGoal(LIMB_RIGHT_ARM);

		//
		for (int i=0;i<BONE_ID_NUM;++i)
		{
			int boneID = GetBoneID(i);
			/*if (boneID>-1)
				pCharacter->GetISkeleton()->SetPlusRotation(boneID, IDENTITY);*/
		}

		pCharacter->GetISkeletonPose()->SetLookIK(false,0,ZERO);
	}
}

void CPlayer::SetHealth(int health )
{
	if(m_stats.isGrabbed)
		health -=1;  //Trigger automatic thrown

	float oldHealth = m_health;
	CActor::SetHealth(health);

	if(IsClient())
	{
		float fHealth = m_health / m_maxHealth * 100.0f + 1.0f;

		const float fMinThreshold = 20.0f;
		const float fMaxThreshold = 30.0f;

		if(fHealth < fMaxThreshold && (m_fLowHealthSoundMood == 0.0f || fHealth < m_fLowHealthSoundMood))
		{
			float fPercent = 100.0f;
			if(fHealth >= fMinThreshold)
			{
				fPercent = (MAX(fHealth,0.0f) - fMinThreshold) * 100.0f / (fMaxThreshold - fMinThreshold);
			}
			SAFE_SOUNDMOODS_FUNC(AddSoundMood(SOUNDMOOD_LOWHEALTH,fPercent));
			m_fLowHealthSoundMood = fHealth;
		}
		else if(fHealth > fMaxThreshold && m_fLowHealthSoundMood > 0.0f)
		{
			m_fLowHealthSoundMood = 0.0f;
		}
	}

	if (health <= 0)
	{
		const bool bIsGod = IsGod() > 0;
		if (IsClient())
		{
			SAFE_HUD_FUNC(ActorDeath(this));
	
				if (bIsGod)
				{
				SAFE_HUD_FUNC(TextMessage("GodMode:died!"));
			}
		}
		if (bIsGod)		// report GOD death
			CALL_PLAYER_EVENT_LISTENERS(OnDeath(this, true));
	}

	if (GetHealth() <= 0)	//client deathFX are triggered in the lua gamerules
	{
		StopLoopingSounds();

		ResetAnimations();
		SetDeathTimer();

		if(m_stats.isOnLadder)
			RequestLeaveLadder(eLAT_Die);

		if(IsClient())
		{
			SAFE_HUD_FUNC(GetRadar()->Reset());
			SAFE_HUD_FUNC(BreakHUD(2));
			SAFE_HUD_FUNC(OnSetActorItem(NULL,NULL));

			if(IItem *pItem = GetCurrentItem(false))
				pItem->Select(false);
		}
		// report normal death
		CALL_PLAYER_EVENT_LISTENERS(OnDeath(this, false));
		if (gEnv->bMultiplayer == false && IsClient())
			g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(GetEntity(), eGE_Death);

		SendMusicLogicEvent(eMUSICLOGICEVENT_PLAYER_KILLED);
	}
	else if(IsClient())	//damageFX
	{
		if(m_health != oldHealth)
			m_merciHealthUpdated = true;

		if(m_health < oldHealth)
		{
			if(m_merciTimeStarted)
				m_merciTimeLastHit = gEnv->pTimer->GetFrameStartTime().GetSeconds();

			IMaterialEffects* pMaterialEffects = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects();

			const float healthThrHi = g_pGameCVars->g_playerLowHealthThreshold*1.25f;
			const float healthThrLow = g_pGameCVars->g_playerLowHealthThreshold;

			if(!g_pGameCVars->g_godMode && m_health < healthThrHi)
			{
				SMFXRunTimeEffectParams params;
				params.pos = GetEntity()->GetWorldPos();
				params.soundSemantic = eSoundSemantic_Player_Foley;
				if(m_health <= healthThrLow && oldHealth > healthThrLow)
				{
					IAIObject* pAI = GetEntity() ? GetEntity()->GetAI() : 0;
					if (pAI)
					{
						pAI->Event(AIEVENT_LOWHEALTH, 0);
					}

					/*TMFXEffectId id = pMaterialEffects->GetEffectIdByName("player_fx", "player_damage_2");
					TMFXEffectId id2 = pMaterialEffects->GetEffectIdByName("player_fx", "player_damage_1");
					pMaterialEffects->StopEffect(id);
					pMaterialEffects->StopEffect(id2);
					pMaterialEffects->ExecuteEffect(id, params);*/
				}
				/*else if(m_health <= healthThrHi && oldHealth > healthThrHi)
				{
					TMFXEffectId id = pMaterialEffects->GetEffectIdByName("player_fx", "player_damage_1");
					TMFXEffectId id2 = pMaterialEffects->GetEffectIdByName("player_fx", "player_damage_2");
					pMaterialEffects->StopEffect(id);
					pMaterialEffects->StopEffect(id2);
					pMaterialEffects->ExecuteEffect(id, params);
				}*/
			}
			SendMusicLogicEvent(eMUSICLOGICEVENT_PLAYER_WOUNDED);
		}
	}

	GetGameObject()->ChangedNetworkState(ASPECT_HEALTH);
}

void CPlayer::SerializeXML( XmlNodeRef& node, bool bLoading )
{
}

void CPlayer::SetAuthority( bool auth )
{
}

//------------------------------------------------------------------------
void CPlayer::Freeze(bool freeze)
{
	//CryLogAlways("%s::Freeze(%s) was: %s", GetEntity()->GetName(), freeze?"true":"false", m_stats.isFrozen?"true":"false");

	if (m_stats.isFrozen==freeze && !gEnv->pSystem->IsSerializingFile())
		return;

	SActorStats* pStats = GetActorStats();
	if (pStats)
		pStats->isFrozen = freeze;

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	if (!pCharacter)
	{
		GameWarning("CPlayer::Freeze: no character instance");
		return;
	}

	if (freeze)
	{	
		if (m_pAnimatedCharacter)
		{
			m_pAnimatedCharacter->GetAnimationGraphState()->Pause(freeze, eAGP_Freezing);
			UpdateAnimGraph(m_pAnimatedCharacter->GetAnimationGraphState());
		}

		SetFrozenAmount(1.f);

		if (GetEntity()->GetAI() && GetGameObject()->GetChannelId()==0)
			GetEntity()->GetAI()->Event(AIEVENT_DISABLE, 0);

		IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();
		assert(pPhysicalEntity);      
		if (!pPhysicalEntity)
		{
			GameWarning("CPlayer::Freeze: no physical entity");
			return;
		}

		pe_status_dynamics dyn;
		if (pPhysicalEntity->GetStatus(&dyn) == 0)
			dyn.mass = 1.0f; // TODO: Should this be zero, or something?

		ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();
		ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
		pSkeletonPose->DestroyCharacterPhysics();

		bool zeroMass=m_stats.isOnLadder || (m_linkStats.GetLinkedVehicle()!=0);

		SEntityPhysicalizeParams params;
		params.nSlot = 0;
		params.mass = zeroMass?0.0f:dyn.mass;
		params.type = PE_RIGID;
		GetEntity()->Physicalize(params);

		pPhysicalEntity = GetEntity()->GetPhysics();

		//pSkeletonPose->BuildPhysicalEntity(pPhysicalEntity, params.mass, -1, 1);

		pe_params_flags flags;
		flags.flagsOR = pef_log_collisions;
		pPhysicalEntity->SetParams(&flags);

		if (m_stats.isOnLadder)
		{
			pe_simulation_params sp;
			sp.gravity=ZERO;
			sp.gravityFreefall=ZERO;

			pPhysicalEntity->SetParams(&sp);
		}

		pe_action_awake awake;
		awake.bAwake=1;

		pPhysicalEntity->Action(&awake);

		if (pPhysicalEntity)
		{
			pe_action_move actionMove;
			actionMove.dir = Vec3(0,0,0);
			pPhysicalEntity->Action(&actionMove);
		}

		// stop all layers
		for (int i=0; i<16; i++)
		{
			pSkeletonAnim->SetLayerUpdateMultiplier(i,0);
			pSkeletonAnim->StopAnimationInLayer(i,0.0f); 
		}
		pCharacter->EnableStartAnimation(false);

		// frozen guys shouldn't blink
		pCharacter->EnableProceduralFacialAnimation(false);

		if(IsClient())
		{
			SAFE_SOUNDMOODS_FUNC(AddSoundMood(SOUNDMOOD_ENTER_FREEZE));
		}
	}
	else
	{	
		SetFrozenAmount(0.f);

		if (GetEntity()->GetAI() && GetGameObject()->GetChannelId()==0)
			GetEntity()->GetAI()->Event(AIEVENT_ENABLE, 0);

		// restart all layers
		for (int i=0; i<16; i++)
			pCharacter->GetISkeletonAnim()->SetLayerUpdateMultiplier(i, 1.0f);

		pCharacter->EnableStartAnimation(true);

		// start blinking again
		pCharacter->EnableProceduralFacialAnimation(true);

		if(IsClient())
		{
			SAFE_SOUNDMOODS_FUNC(AddSoundMood(SOUNDMOOD_LEAVE_FREEZE));
		}

		if (m_pAnimatedCharacter)
		{
			m_pAnimatedCharacter->GetAnimationGraphState()->Pause(freeze, eAGP_Freezing);
			UpdateAnimGraph(m_pAnimatedCharacter->GetAnimationGraphState());
		}
	}

	if (IVehicle *pVehicle=GetLinkedVehicle())
	{
		IVehicleSeat *pSeat=pVehicle->GetSeatForPassenger(GetEntityId());
		if (pSeat)
		{
			if (!freeze)
				pSeat->ForceAnimGraphInputs();

			if (pSeat->IsDriver())
			{
				SVehicleMovementEventParams params;
				params.fValue = freeze?1.0f:0.0f;
				pVehicle->GetMovement()->OnEvent(IVehicleMovement::eVME_Freeze, params);
			}
		}
	}
	
	m_params.vLimitDir.zero();

	if (IsPlayer())
	{
		if (freeze)
			m_params.vLimitDir = m_viewQuat.GetColumn1();
		else if (CItem *pCurrentItem=static_cast<CItem *>(GetCurrentItem()))
		{
			if (pCurrentItem->IsMounted())
				pCurrentItem->ApplyViewLimit(GetEntityId(), true);
		}
	}

	if (m_pAnimatedCharacter)
	{
		SAnimatedCharacterParams params = m_pAnimatedCharacter->GetParams();

		if (freeze)
			params.flags &= ~eACF_EnableMovementProcessing;
		else
			params.flags |= eACF_EnableMovementProcessing;

		m_pAnimatedCharacter->SetParams(params);
	}

	if (!freeze)
	{
		if(m_stats.isExitingLadder && IsClient())
		{
			RequestLeaveLadder(eLAT_ReachedEnd);
		}
		else if (m_stats.isOnLadder)
		{
			// HAX: this will make sure the input changes are noticed by the animation graph,
			// and the animations will be re-started...
			UpdateLadderAnimation(eLS_Exit, eLDIR_Stationary);
			m_pAnimatedCharacter->GetAnimationGraphState()->Update();
			UpdateLadderAnimation(eLS_Climb, eLDIR_Up);
		}
	}

	m_stats.followCharacterHead = (freeze?2:0);
}

float CPlayer::GetMassFactor() const
{
	//code regarding currentItem only
	EntityId itemId = GetInventory()->GetCurrentItem();
	if (itemId)
	{
		float mass = 0;
		if(CWeapon* weap = GetWeapon(itemId))
			mass = weap->GetParams().mass;
		else if(CItem* item = GetItem(itemId))
			mass = item->GetParams().mass;
		float massFactor = 1.0f - (mass / (m_params.maxGrabMass*GetActorStrength()*1.75f));
		if (m_stats.inZeroG)
			massFactor += (1.0f - massFactor) * 0.5f;
		return massFactor;
	}
	return 1.0f;
}

bool	CPlayer::IsFiringProne() const
{
	if(!IsPlayer())
		return false;

	if(m_stance != STANCE_PRONE)
		return false;

	EntityId itemId = GetInventory()->GetCurrentItem();
	if (itemId)
	{
		if(CWeapon* weap = GetWeapon(itemId))
			return weap->IsFiring();
	}

	return false;
}

float CPlayer::GetFrozenAmount(bool stages/*=false*/) const
{
	if (stages && IsPlayer())
	{
		float steps = g_pGameCVars->cl_frozenSteps;

		// return the next step value
		return m_frozenAmount>0 ? min(((int)(m_frozenAmount * steps))+1.f, steps)*1.f/steps : 0;
	}
	
	return m_frozenAmount;
}

void CPlayer::SetAngles(const Ang3 &angles) 
{
	Matrix33 rot(Matrix33::CreateRotationXYZ(angles));
	CMovementRequest mr;
	mr.SetLookTarget( GetEntity()->GetWorldPos() + 20.0f * rot.GetColumn(1) );
	m_pMovementController->RequestMovement(mr);
}

Ang3 CPlayer::GetAngles() 
{
	if(IsClient() && GetLinkedVehicle())
		return Ang3(m_clientViewMatrix);

	return Ang3(m_viewQuatFinal.GetNormalized());
}

void CPlayer::AddAngularImpulse(const Ang3 &angular,float deceleration,float duration)
{
	m_stats.angularImpulse = angular;
	m_stats.angularImpulseDeceleration = deceleration;
	m_stats.angularImpulseTimeMax = m_stats.angularImpulseTime = duration;
}

void CPlayer::SelectNextItem(int direction, bool keepHistory, const char *category)
{
	if (m_health && m_stats.animationControlled || ShouldSwim() || /*m_bSprinting || */m_stats.inFreefall)
		return;

	COffHand* pOffHand = static_cast<COffHand*>(GetWeaponByClass(CItem::sOffHandClass));
	if(pOffHand && pOffHand->IsSelected())
	{
		//Grenade throw can be interrupted by switching weapons
		if((pOffHand->GetOffHandState()==eOHS_HOLDING_GRENADE) && !category)
		{
			pOffHand->SetOffHandState(eOHS_INIT_STATE);
			pOffHand->SetResetTimer(0.25f); //Just in case...
			pOffHand->RequireUpdate(eIUS_General);
		}
		else
			return;
	}

	EntityId oldItemId=GetCurrentItemId();

	CActor::SelectNextItem(direction,keepHistory,category);

	if (GetCurrentItemId() && oldItemId!=GetCurrentItemId())
		m_bSprinting=false; // force the weapon disabling code to be 

	GetGameObject()->ChangedNetworkState(ASPECT_CURRENT_ITEM);
}

void CPlayer::HolsterItem(bool holster)
{
	CActor::HolsterItem(holster);

	GetGameObject()->ChangedNetworkState(ASPECT_CURRENT_ITEM);
}

void CPlayer::SelectLastItem(bool keepHistory, bool forceNext /* = false */)
{
	CActor::SelectLastItem(keepHistory, forceNext);

	GetGameObject()->ChangedNetworkState(ASPECT_CURRENT_ITEM);
}

void CPlayer::SelectItemByName(const char *name, bool keepHistory)
{
	CActor::SelectItemByName(name, keepHistory);

	GetGameObject()->ChangedNetworkState(ASPECT_CURRENT_ITEM);
}

void CPlayer::SelectItem(EntityId itemId, bool keepHistory)
{
	CActor::SelectItem(itemId, keepHistory);

	GetGameObject()->ChangedNetworkState(ASPECT_CURRENT_ITEM);
}

bool CPlayer::SetAspectProfile(EEntityAspects aspect, uint8 profile )
{
	uint8 currentphys=GetGameObject()->GetAspectProfile(eEA_Physics);
	if (aspect == eEA_Physics)
	{
		if (profile==eAP_Alive && currentphys==eAP_Sleep)
		{
			m_stats.isStandingUp=true;
			m_stats.isRagDoll=false;
		}
	}

	bool res=CActor::SetAspectProfile(aspect, profile);

	if (aspect == eEA_Physics)
	{
		// transition frozen->linked: need to relink so animgraph and collider modes get updated
		if (profile==eAP_Linked && currentphys==eAP_Frozen)
			if (m_linkStats.linkID)
				LinkToVehicle(m_linkStats.linkID);
	}

	return res;
}

bool CPlayer::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if (!CActor::NetSerialize(ser, aspect, profile, flags))
		return false;

	if (aspect == ASPECT_HEALTH)
	{
		ser.Value("health", m_health, 'hlth');
		bool isFrozen = m_stats.isFrozen;
		ser.Value("frozen", isFrozen, 'bool');
		ser.Value("frozenAmount", m_frozenAmount, 'frzn');
	}    
	if (aspect == ASPECT_CURRENT_ITEM)
	{
		bool reading=ser.IsReading();
		bool hasWeapon=false;
		if (!reading)
			hasWeapon=NetGetCurrentItem()!=0;

		ser.Value("hasWeapon", hasWeapon, 'bool');
		ser.Value("currentItemId", static_cast<CActor*>(this), &CActor::NetGetCurrentItem, &CActor::NetSetCurrentItem, 'eid');

		if (reading && hasWeapon && NetGetCurrentItem()==0) // fix the case where this guy's weapon might not have been bound on this client yet
			ser.FlagPartialRead();
	}

	if(m_pNanoSuit)													// nanosuit needs to be serialized before input
		m_pNanoSuit->Serialize(ser, aspect);	// because jumping/punching/sprinting energy consumption will vary with suit settings

	if (aspect == IPlayerInput::INPUT_ASPECT)
	{
		SSerializedPlayerInput serializedInput;
		if (m_pPlayerInput.get() && ser.IsWriting())
			m_pPlayerInput->GetState(serializedInput);

		serializedInput.Serialize(ser);

		if (m_pPlayerInput.get() && ser.IsReading())
		{
			m_pPlayerInput->SetState(serializedInput);
		}

		ser.Value("VehicleViewRotation", m_vehicleViewDir, 'dir0');
	}
	return true;
}

void CPlayer::FullSerialize( TSerialize ser )
{
	CActor::FullSerialize(ser);

	m_pMovementController->Serialize(ser);

	if(ser.IsReading())
		ResetScreenFX();

	ser.BeginGroup( "BasicProperties" );
	//ser.EnumValue("stance", this, &CPlayer::GetStance, &CPlayer::SetStance, STANCE_NULL, STANCE_LAST);		
	// skip matrices... not supported
	ser.Value( "velocity", m_velocity );
	ser.Value( "feetWpos0", m_feetWpos[0] );
	ser.Value( "feetWpos1", m_feetWpos[1] );
	// skip animation to play for now
	// skip currAnimW
	ser.Value( "eyeOffset", m_eyeOffset );
	ser.Value( "bobOffset", m_bobOffset );
	ser.Value( "FPWeaponLastDirVec", m_FPWeaponLastDirVec );
	ser.Value( "FPWeaponOffset", m_FPWeaponOffset );
	ser.Value( "FPWeaponAngleOffset", m_FPWeaponAngleOffset );
	ser.Value( "viewAnglesOffset", m_viewAnglesOffset );
	ser.Value( "angleOffset", m_angleOffset );
	ser.Value( "modelOffset", m_modelOffset );
	ser.Value( "headAngles", m_headAngles );
	ser.Value( "viewRoll", m_viewRoll );
	ser.Value( "upVector", m_upVector );
	ser.Value( "hasHUD", m_bHasHUD);
	ser.Value( "viewQuat", m_viewQuat );
	ser.Value( "viewQuatFinal", m_viewQuatFinal );
	ser.Value( "baseQuat", m_baseQuat );
	ser.Value( "weaponOffset", m_weaponOffset ); 
  ser.Value( "frozenAmount", m_frozenAmount );
	if(IsClient())
	{
		ser.Value( "ragDollHead", m_bRagDollHead);
		ser.Value("ConcentrationSoundMood", m_bConcentration);
		ser.Value("ConcentrationSoundMoodTimer", m_fConcentrationTimer);
	}
	ser.EndGroup();

	//serializing stats
	m_stats.Serialize(ser, ~uint32(0));
	//nanosuit
	
	bool haveNanoSuit = (m_pNanoSuit)?true:false;
	ser.Value("haveNanoSuit", haveNanoSuit);
	if(haveNanoSuit)
	{
		if(!m_pNanoSuit)
			m_pNanoSuit = new CNanoSuit();

		if(ser.IsReading())
			m_pNanoSuit->Reset(this);
		m_pNanoSuit->Serialize(ser, ~uint32(0));
	}

	//input-action
	bool hasInput = (m_pPlayerInput.get())?true:false;
	bool hadInput = hasInput;
	ser.Value("PlayerInputExists", hasInput);
	if(hasInput)
	{
		if(ser.IsReading() && !(m_pPlayerInput.get()))
			m_pPlayerInput.reset( new CPlayerInput(this) );
		((CPlayerInput*)m_pPlayerInput.get())->SerializeSaveGame(ser);
	}
	else if(hadInput && ser.IsReading())
		m_pPlayerInput.reset(NULL);

	ser.Value("mountedWeapon", m_stats.mountedWeaponID);
	if(m_stats.mountedWeaponID && this == g_pGame->GetIGameFramework()->GetClientActor()) //re-mounting is done in the item
	{
		if(g_pGame->GetHUD())
			g_pGame->GetHUD()->GetCrosshair()->SetUsability(true); //doesn't update after loading
	}

	ser.Value("parachuteEnabled", m_parachuteEnabled);

	// perform post-reading fixups
	if (ser.IsReading())
	{
		// fixup matrices here

		//correct serialize the parachute
		int8 freefall(m_stats.inFreefall.Value());
		m_stats.inFreefall = -1;
		ChangeParachuteState(freefall);
	}

	if (IsClient())
	{
		// Staging params
		if (ser.IsWriting())
		{
			m_stagingParams.Serialize(ser);
		}
		else
		{
			SStagingParams stagingParams;
			stagingParams.Serialize(ser);
			if (stagingParams.bActive || stagingParams.bActive != m_stagingParams.bActive)
				StagePlayer(stagingParams.bActive, &stagingParams);
		}
	}
}

void CPlayer::PostSerialize()
{
	CActor::PostSerialize();
	m_drownEffectDelay = 0.0f;

	StopLoopingSounds();
	m_bVoiceSoundPlaying = false;

  if (GetLinkedVehicle())
  {
    CFists *pFists = static_cast<CFists*>(GetItemByClass(CItem::sFistsClass));
    if (pFists && pFists->IsSelected())
      pFists->Select(false);
  }

	if (IsClient())
	{
		if(m_pVehicleClient)
			m_pVehicleClient->Reset();
		SupressViewBlending();
		m_merciTimeStarted = false;
		m_merciHealthUpdated = true;

		if(m_stats.grabbedHeavyEntity) //fix special case when saved while throwing & looking down
		{
			COffHand *pOffHand = static_cast<COffHand*>(GetWeaponByClass(CItem::sOffHandClass));
			if(pOffHand && pOffHand->GetHeldEntityId() == 0)
				m_stats.grabbedHeavyEntity = false;
		}
	}
}

void SPlayerStats::Serialize(TSerialize ser, unsigned aspects)
{
	assert( ser.GetSerializationTarget() != eST_Network );
	ser.BeginGroup("PlayerStats");

	if (ser.GetSerializationTarget() != eST_Network)
	{
		//when reading, reset the structure first.
		if (ser.IsReading())
			*this = SPlayerStats();

		ser.Value("inAir", inAir);
		ser.Value("onGround", onGround);
		inFreefall.Serialize(ser, "inFreefall");
		if(ser.IsReading())
			inFreefallLast = !inFreefall;
		ser.Value("landed", landed);
		ser.Value("jumped", jumped);
		ser.Value("inMovement", inMovement);
		ser.Value("inRest", inRest);
		ser.Value("inWater", inWaterTimer);
		ser.Value("waterLevel", relativeWaterLevel);
		ser.Value("flatSpeed", speedFlat);
		ser.Value("gravity", gravity);
		//ser.Value("mass", mass);		//serialized in Actor::SerializeProfile already ...
		ser.Value("bobCycle", bobCycle);
		ser.Value("leanAmount", leanAmount);
		ser.Value("shakeAmount", shakeAmount);
		ser.Value("physCamOffset", physCamOffset);
		ser.Value("fallSpeed", fallSpeed);
		ser.Value("isFiring", isFiring);
		ser.Value("isRagDoll", isRagDoll);
		ser.Value("isWalkingOnWater", isWalkingOnWater);
		followCharacterHead.Serialize(ser, "followCharacterHead");
		firstPersonBody.Serialize(ser, "firstPersonBody");
		ser.Value("velocity", velocity);
		ser.Value("velocityUnconstrained", velocityUnconstrained);
		ser.Value("wasStuck", wasStuck);
		ser.Value("wasFlying", wasFlying);
		ser.Value("stuckTimeout", stuckTimeout);

		ser.Value("upVector", upVector);
		ser.Value("groundNormal", groundNormal);
		ser.Value("FPWeaponPos", FPWeaponPos);
		ser.Value("FPWeaponAngles", FPWeaponAngles);
		ser.Value("FPSecWeaponPos", FPSecWeaponPos);
		ser.Value("FPSecWeaponAngles", FPSecWeaponAngles);
		ser.Value("isThirdPerson", isThirdPerson);
    isFrozen.Serialize(ser, "isFrozen"); //this is already serialized by the gamerules ..
    isHidden.Serialize(ser, "isHidden");

		//FIXME:serialize cheats or not?
		//int godMode(g_pGameCVars->g_godMode);
		//ser.Value("godMode", godMode);
		//g_pGameCVars->g_godMode = godMode;
		//ser.Value("flyMode", flyMode);
		//

		//ser.Value("thrusterSprint", thrusterSprint);

		isOnLadder.Serialize(ser, "isOnLadder");
		ser.Value("exitingLadder", isExitingLadder);
		ser.Value("ladderTop", ladderTop);
		ser.Value("ladderBottom", ladderBottom);
		ser.Value("ladderOrientation", ladderOrientation);
		ser.Value("ladderUpDir", ladderUpDir);
		ser.Value("playerRotation", playerRotation);

		ser.Value("forceCrouchTime", forceCrouchTime);    

		ser.Value("grabbedHeavyObject", grabbedHeavyEntity);

	}

	ser.EndGroup();
}

bool CPlayer::CreateCodeEvent(SmartScriptTable &rTable)
{
	const char *event = NULL;
	rTable->GetValue("event",event);

	if (event && !strcmp(event,"ladder"))
	{
		m_stats.ladderTop.zero();
		m_stats.ladderBottom.zero();

		rTable->GetValue("ladderTop",m_stats.ladderTop);
		rTable->GetValue("ladderBottom",m_stats.ladderBottom);

		m_stats.isOnLadder = (m_stats.ladderTop.len2()>0.01f && m_stats.ladderBottom.len2()>0.01f);

		if(m_pAnimatedCharacter)
		{
			if (m_stats.isOnLadder)
				m_pAnimatedCharacter->RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "Player::CreateCodeEvent");
			else
				m_pAnimatedCharacter->RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "Player::CreateCodeEvent");
		}
		return true;
	}
	else if (event && !strcmp(event,"addSuitEnergy") && m_pNanoSuit)
	{
		float amount(0.0f);
		rTable->GetValue("amount",amount);

		m_pNanoSuit->SetSuitEnergy(m_pNanoSuit->GetSuitEnergy() + amount);

		return true;
	}
/* kirill - needed for debugging AI aiming/accuracy 
	else if (event && !strcmp(event,"aiHitDebug"))
	{
		if(!IsPlayer())
			return true;
		ScriptHandle id;
		id.n = 0;
		rTable->GetValue("shooterId",id);
		IEntity *pEntity((id.n)?gEnv->pEntitySystem->GetEntity(id.n):NULL);
		if(pEntity && pEntity->GetAI())
		{
			IAIObject *pAIObj(pEntity->GetAI());
				if(pAIObj->GetProxy())
					++(pAIObj->GetProxy()->DebugValue());
		}
		return true;
	}
*/
	else
		return CActor::CreateCodeEvent(rTable);
}

void CPlayer::PlayAction(const char *action,const char *extension, bool looping) 
{
	if (!m_pAnimatedCharacter)
		return;

	if (extension==NULL || strcmp(extension,"ignore")!=0)
	{
	if (extension && extension[0])
			strncpy(m_params.animationAppendix,extension,32);
		else
			strcpy(m_params.animationAppendix,"nw");

		m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( m_inputItem, m_params.animationAppendix );
	}

	if (looping)
		m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( "Action", action );
	else
		m_pAnimatedCharacter->GetAnimationGraphState()->SetInput( "Signal", action );
}

void CPlayer::AnimationControlled(bool activate)
{
	if (m_stats.animationControlled != activate)
	{
		m_stats.animationControlled = activate;
		m_stats.followCharacterHead = activate?1:0;

		HolsterItem(activate);

		/*
		//before the sequence starts, remove any local offset of the player model.
		if (activate)
		{
			if (m_forceWorldTM.GetTranslation().len2()<0.01f)
				m_forceWorldTM = pEnt->GetWorldTM();

			pEnt->SetWorldTM(m_forceWorldTM * Matrix34::CreateTranslationMat(-GetEntity()->GetSlotLocalTM(0,false).GetTranslation()));
		}
		//move the player at the exact position animation brought him during the sequence.
		else
		{
			Vec3 localPos(GetStanceInfo(m_stance)->modelOffset);

			int bip01ID = GetBoneID(BONE_BIP01);
			if (bip01ID>-1)
			{
				Vec3 bipPos = pEnt->GetCharacter(0)->GetISkeleton()->GetAbsJPositionByID(bip01ID);
				bipPos.z = 0;
				localPos -= bipPos;
			}

			pEnt->SetWorldTM(pEnt->GetSlotWorldTM(0) * Matrix34::CreateTranslationMat(-localPos));

			m_forceWorldTM.SetIdentity();
		}*/
	}
}

void CPlayer::HandleEvent( const SGameObjectEvent& event )
{
	/*else if (!stricmp(event.event, "CameraFollowHead"))
	{
		m_stats.followCharacterHead = 1;
	}
	else if (!stricmp(event.event, "NormalCamera"))
	{
		m_stats.followCharacterHead = 0;
	}
	else*/
	if (event.event == eCGE_AnimateHands)
	{
		CreateScriptEvent("AnimateHands",0,(const char *)event.param);
	}
	else if (event.event == eCGE_PreFreeze)
	{
		if (event.param)
			GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Frozen);
	}
	else if (event.event == eCGE_PostFreeze)
	{
		if (event.param)
		{
			if (gEnv->pAISystem)
				gEnv->pAISystem->ModifySmartObjectStates(GetEntity(), "Frozen");
		}
		else
		{
			if (gEnv->bServer && GetHealth()>0)
			{
				if (!m_linkStats.linkID)
					GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
				else
					GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Linked);
			}
		}

		if (GetCurrentItemId(false))
		{
			g_pGame->GetGameRules()->FreezeEntity(GetCurrentItemId(false), event.param!=0, true);
			CItem *pOffHand = static_cast<CItem*>(GetWeaponByClass(CItem::sOffHandClass));
			if(pOffHand && pOffHand->IsSelected())
				g_pGame->GetGameRules()->FreezeEntity(pOffHand->GetEntityId(), event.param!=0, true);
		}

		if (m_pNanoSuit)
		{
			if (event.param && m_pNanoSuit->GetMode()==NANOMODE_CLOAK)
				m_pNanoSuit->SetCloak(false, true);
			else if (!event.param && m_pNanoSuit->GetMode()==NANOMODE_CLOAK)
			{
				if (m_pNanoSuit->GetSuitEnergy()>0.0f)
					m_pNanoSuit->SetCloak(true, false);
			}
			m_pNanoSuit->Activate(event.param==0);
		}

		if (event.param==0)
		{
			if (m_stats.inFreefall.Value()>0)
				UpdateFreefallAnimationInputs(true);
		}
	}
	else if (event.event == eCGE_PreShatter)
	{
		// see also CPlayer::HandleEvent
		if (gEnv->bServer)
			GetGameObject()->SetAspectProfile(eEA_Physics, eAP_NotPhysicalized);

		m_stats.isHidden=true;
		m_stats.isShattered=true;

		SetHealth(0);
		GetEntity()->SetSlotFlags(0, GetEntity()->GetSlotFlags(0)&(~ENTITY_SLOT_RENDER));
	}
	else if (event.event == eCGE_OpenParachute)
	{
		m_openParachuteTimer = 1.0f;
		m_openingParachute = true;
	}
	else
	{
		CActor::HandleEvent(event);
		if (event.event == eGFE_BecomeLocalPlayer)
		{
			SetActorModel();
			InitActorAttachments();
			if (m_pNanoSuit)
			{
				m_pNanoSuit->SelectSuitMaterial();
			}
			Physicalize();
		}
	}
}

void CPlayer::UpdateGrab(float frameTime)
{
	CActor::UpdateGrab(frameTime);
}

float CPlayer::GetActorStrength() const
{
	float strength = 1.0f;
	if(m_pNanoSuit)
		strength += (m_pNanoSuit->GetSlotValue(NANOSLOT_STRENGTH)*0.01f)*g_pGameCVars->cl_strengthscale;
	return strength;
}

void CPlayer::ProcessBonesRotation(ICharacterInstance *pCharacter,float frameTime)
{
	CActor::ProcessBonesRotation(pCharacter,frameTime);

	if (!IsPlayer() || m_stats.isFrozen || m_stats.isRagDoll || !pCharacter || m_linkStats.GetLinked())
		return;

	//apply procedural leaning, for the third person character.
	Ang3 headAnglesGoal(0,m_viewRoll * 3.0f,0);
	Interpolate(m_headAngles,headAnglesGoal,10.0f,frameTime,30.0f);

	if (m_headAngles.y*m_headAngles.y > 0.001f)
	{
		Ang3 headAngles(m_headAngles);

		headAngles.y *= 0.25f;

		int16 id[3];
		id[0] = GetBoneID(BONE_SPINE);
		id[1] = GetBoneID(BONE_SPINE2);
		id[2] = GetBoneID(BONE_SPINE3);

		float leanValues[3];
		leanValues[0] = m_headAngles.y * 0.5f;
		leanValues[1] = m_headAngles.y * 0.5f;
		leanValues[2] = -m_headAngles.y * 0.5f;

		for (int i=0;i<3;++i)
		{
			QuatT lQuat;
			int16 JointID = id[i];

			if (JointID>-1)
			{
				lQuat = pCharacter->GetISkeletonPose()->GetRelJointByID(JointID);
				lQuat.q *= Quat::CreateRotationAA(leanValues[i],Vec3(0.0f,1.0f,0.0f));

				pCharacter->GetISkeletonPose()->SetPostProcessQuat(JointID, lQuat);
			}
		}
	}
}

//TODO: clean it up, less redundancy, more efficiency etc..
void CPlayer::ProcessIKLegs(ICharacterInstance *pCharacter,float frameTime)
{
	if (GetGameObject()->IsProbablyDistant() && !GetGameObject()->IsProbablyVisible())
		return;

	static bool bOnce = true;
	static int nDrawIK = 0;
	static int nNoIK = 0;
	if (bOnce)
	{
		bOnce = false;
		gEnv->pConsole->Register( "player_DrawIK",&nDrawIK,0 );
		gEnv->pConsole->Register( "player_NoIK",&nNoIK,0 );
	}

	if (nNoIK)
	{
		//pCharacter->SetLimbIKGoal(LIMB_LEFT_LEG);
		//pCharacter->SetLimbIKGoal(LIMB_RIGHT_LEG);
		return;
	}

	float stretchLen(0.9f);

	switch(m_stance)
	{
	default: break;
	case STANCE_STAND: stretchLen = 1.1f;break;
	}

	//Vec3 localCenter(GetEntity()->GetSlotLocalTM(0,false).GetTranslation());
	int32 id = GetBoneID(BONE_BIP01);
	Vec3 localCenter(0,0,0);
	if (id>=0)
		//localCenter = bip01->GetBonePosition();
		localCenter = pCharacter->GetISkeletonPose()->GetAbsJointByID(id).t;

	Vec3 feetLpos[2];

	Matrix33 transMtx(GetEntity()->GetSlotWorldTM(0));
	transMtx.Invert();

	for (int i=0;i<2;++i)
	{
	//	int limb = (i==0)?LIMB_LEFT_LEG:LIMB_RIGHT_LEG;
		feetLpos[i] = Vec3(ZERO); //pCharacter->GetLimbEndPos(limb);

		Vec3 feetWpos = GetEntity()->GetSlotWorldTM(0) * feetLpos[i];
		ray_hit hit;

		float testExcursion(localCenter.z);

		int rayFlags = (COLLISION_RAY_PIERCABILITY & rwi_pierceability_mask);
		if (gEnv->pPhysicalWorld->RayWorldIntersection(feetWpos + m_baseQuat.GetColumn2()*testExcursion, m_baseQuat.GetColumn2()*(testExcursion*-0.95f), ent_terrain|ent_static/*|ent_rigid*/, rayFlags, &hit, 1))
		{		
			m_feetWpos[i] = hit.pt; 
			Vec3 footDelta = transMtx * (m_feetWpos[i] - feetWpos);
			Vec3 newLPos(feetLpos[i] + footDelta);

			//pCharacter->SetLimbIKGoal(limb,newLPos,ik_leg,0,transMtx * hit.n);

			//CryLogAlways("%.1f,%.1f,%.1f",hit.n.x,hit.n.y,hit.n.z);
		}
		//else
		//pCharacter->SetLimbIKGoal(limb);
	}
}

void CPlayer::ChangeParachuteState(int8 newState)
{
	bool changed(m_stats.inFreefall.Value()!=newState);
	if (changed)
	{
		switch(newState)
		{
		case 0:
			{
				//add some view kickback on landing
				if (m_stats.inFreefall.Value()>0)
					AddAngularImpulse(Ang3(-0.5f,RANDOM()*0.5f,RANDOM()*0.35f),0.0f,0.5f);

				//remove the parachute, if one was loaded. additional sounds should go in here
				if (m_nParachuteSlot)				
				{		
					int flags = GetEntity()->GetSlotFlags(m_nParachuteSlot)&~ENTITY_SLOT_RENDER;			
					GetEntity()->SetSlotFlags(m_nParachuteSlot,flags );		
				}

				if (IsClient())
				{
					if(CHUD *pHUD = g_pGame->GetHUD())
					{
						if(pHUD->GetVehicleInterface()->GetHUDType() == CHUDVehicleInterface::EHUD_PARACHUTE)
							pHUD->OnExitVehicle(this);
					}
				}
			}
			break;

		case 2:
			{
				IEntity *pEnt = GetEntity();
				// load and draw the parachute
				if (!m_nParachuteSlot)								
					m_nParachuteSlot=pEnt->LoadCharacter(10,"Objects/Vehicles/Parachute/parachute_opening.chr");				
				if (m_nParachuteSlot) // check if it was correctly loaded...dont wanna modify another character slot
				{			
					m_fParachuteMorph=0;
					ICharacterInstance *pCharacter=pEnt->GetCharacter(m_nParachuteSlot);				
					//if (pCharacter)
					//{
					//	pCharacter->GetIMorphing()->SetLinearMorphSequence(m_fParachuteMorph);					
					//}
					int flags = pEnt->GetSlotFlags(m_nParachuteSlot)|ENTITY_SLOT_RENDER;
					pEnt->SetSlotFlags(m_nParachuteSlot,flags );
				}

				AddAngularImpulse(Ang3(1.35f,RANDOM()*0.5f,RANDOM()*0.5f),0.0f,1.5f);

				if (IPhysicalEntity *pPE = pEnt->GetPhysics())
				{
					pe_action_impulse actionImp;
					actionImp.impulse = Vec3(0,0,9.81f) * m_stats.mass;
					actionImp.iApplyTime = 0;
					pPE->Action(&actionImp);
				}

				//if (IsClient())
					//SAFE_HUD_FUNC(OnEnterVehicle(this,"Parachute","Open",m_stats.isThirdPerson));
			}
			break;

		case 1:
			if (IsClient() && m_parachuteEnabled)
				SAFE_HUD_FUNC(OnEnterVehicle(this,"Parachute","Closed",m_stats.isThirdPerson));
			break;

		case 3://This is opening the parachute
			if (IsClient())
				SAFE_HUD_FUNC(OnEnterVehicle(this,"Parachute","Open",m_stats.isThirdPerson));
			break;
		}
	
		m_stats.inFreefall = newState;

		UpdateFreefallAnimationInputs();
	}
}

void CPlayer::UpdateFreefallAnimationInputs(bool force/* =false */)
{
	if (force)
	{
		SetAnimationInput("Action", "idle");
		GetAnimationGraphState()->Update();
	}

	if (m_stats.inFreefall.Value() == 1)
		SetAnimationInput("Action", "freefall");
	else if ((m_stats.inFreefall.Value() == 3) || (m_stats.inFreefall.Value() == 2)) //3 means opening, 2 already opened 
		SetAnimationInput("Action", "parachute");
	else if (m_stats.inFreefall.Value() == 0)
		SetAnimationInput("Action", "idle");
}


void CPlayer::Landed(float fallSpeed)
{
	if(IEntity* pEntity= GetLinkedEntity())
		return;

	static const int objTypes = ent_all;    
	static const unsigned int flags = rwi_stop_at_pierceable|rwi_colltype_any;
	ray_hit hit;
	Vec3 down = Vec3(0,0,-1.0f);
	IPhysicalEntity *phys = GetEntity()->GetPhysics();
	IMaterialEffects *mfx = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects();
	int matID = mfx->GetDefaultSurfaceIndex();
	int col = gEnv->pPhysicalWorld->RayWorldIntersection(GetEntity()->GetWorldPos(), (down * 5.0f), objTypes, flags, &hit, 1, phys);
	if (col)
	{
		matID = hit.surface_idx;
	}
	
	TMFXEffectId effectId = mfx->GetEffectId("bodyfall", matID);
	if (effectId != InvalidEffectId)
	{
		SMFXRunTimeEffectParams params;
		Vec3 direction = Vec3(0,0,0);
		if (IMovementController *pMV = GetMovementController())
		{
			SMovementState state;
			pMV->GetMovementState(state);
			direction = state.aimDirection;
		}
		params.pos = GetEntity()->GetWorldPos() + direction;
		params.soundSemantic = eSoundSemantic_Player_Foley;
		float landFallParamVal = (fallSpeed > 7.5f) ? 0.75f : 0.25f;
		if(m_pNanoSuit && m_pNanoSuit->GetMode() == NANOMODE_DEFENSE)
			landFallParamVal = 0.0f;
		params.AddSoundParam("landfall", landFallParamVal);
		const float speedParamVal = min(fabsf((m_stats.velocity.z/10.0f)), 1.0f);
		params.AddSoundParam("speed", speedParamVal);

		mfx->ExecuteEffect(effectId, params);
	}

	if(m_stats.inZeroG && GravityBootsOn() && m_pNanoSuit)
	{
		m_pNanoSuit->PlaySound(ESound_GBootsLanded, min(fabsf((m_stats.velocity.z/10.0f)), 1.0f));
		if(IsClient())
			if (gEnv->pInput) gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.1f, 0.2f, 1.0f*clamp_tpl(fallSpeed*0.1f, 0.0f, 1.0f) ) );
	}
	else if(IsClient())
	{
		if(fallSpeed > 7.0f)
			PlaySound(ESound_Fall_Drop);
		if (gEnv->pInput) 
			gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.09f + (fallSpeed * 0.01f), 0.5f, 0.9f*clamp_tpl(fallSpeed*0.2f, 0.1f, 1.0f) ) );
	}
}

//------------------------------------------------------------------------
bool CPlayer::IsCloaked() const
{
	if (CNanoSuit *pSuit = GetNanoSuit())
	{
		if (pSuit->GetMode() == NANOMODE_CLOAK)
		{
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
// animation-based footsteps sound playback
void CPlayer::UpdateFootSteps(float frameTime)
{
	if (GetLinkedEntity())
		return;

	SActorStats *pStats = GetActorStats();

	if(!pStats || (!pStats->onGround && !pStats->inZeroG))
		return;

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	ISkeletonAnim *pSkeletonAnim = NULL;
	ISkeletonPose *pSkeletonPose = NULL;
	if (pCharacter)
	{
		pSkeletonAnim = pCharacter->GetISkeletonAnim();
		pSkeletonPose = pCharacter->GetISkeletonPose();
	}
	else
		return;

	uint32 numAnimsLayer0 = pSkeletonAnim->GetNumAnimsInFIFO(0);
	if(!numAnimsLayer0)
		return;

	IAnimationSet* pIAnimationSet = pCharacter->GetIAnimationSet();
	CAnimation& animation=pSkeletonAnim->GetAnimFromFIFO(0,0);
	float normalizedtime = animation.m_fAnimTime; //0.0f=left leg on ground, 0.5f=right leg on ground, 1.0f=left leg on ground

	//which foot is next ?
	EBonesID footID = BONE_FOOT_L;
	if(normalizedtime > 0.0f && normalizedtime <= 0.5f)
		footID = BONE_FOOT_R;

	if(footID == m_currentFootID)	//don't play the same sound twice ...
		return;

	float relativeSpeed = pSkeletonAnim->GetCurrentVelocity().GetLength() / 7.0f; //hardcoded :-( 7m/s max speed

	if(relativeSpeed > 0.03f)	//if fast enough => play sound
	{
		int boneID = GetBoneID(m_currentFootID);

		//CryLogAlways("%f", relativeSpeed);

		// setup sound params
		SMFXRunTimeEffectParams params;
		params.angle = GetEntity()->GetWorldAngles().z + g_PI/2.0f;
		if(boneID == -1)	//no feet found
			params.pos = GetEntity()->GetWorldPos();
		else	//set foot position
		{
			params.pos = params.decalPos = GetEntity()->GetSlotWorldTM(0) * pSkeletonPose->GetAbsJointByID(GetBoneID(m_currentFootID, 0)).t;
		}
		params.AddSoundParam("speed", relativeSpeed);
		params.soundSemantic = eSoundSemantic_Physics_Footstep;

		if(m_stats.relativeWaterLevel < 0.0f)
			CreateScriptEvent("splash",0);
		
		bool playFirstPerson = false;
		if(IsClient() && !IsThirdPerson())
			playFirstPerson = true;
		params.playSoundFP = playFirstPerson;

		bool inWater = false;

		//create effects
		EStance stance = GetStance();
		IMaterialEffects* pMaterialEffects = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects();
		TMFXEffectId effectId = InvalidEffectId;
		float feetWaterLevel = gEnv->p3DEngine->GetWaterLevel(&params.pos);
		if (feetWaterLevel != WATER_LEVEL_UNKNOWN && feetWaterLevel>params.pos.z)
		{
			effectId = pMaterialEffects->GetEffectIdByName("footsteps", "water");
			inWater = true;
		}
		else
		{
			effectId = pMaterialEffects->GetEffectId("footstep", pStats->groundMaterialIdx);
		}

		TMFXEffectId gearEffectId = InvalidEffectId;

		float gearEffectPossibility = relativeSpeed*1.3f;
		if(stance == STANCE_CROUCH || stance == STANCE_PRONE)
		{
			gearEffectPossibility *= 10.0f;
			if(stance == STANCE_PRONE)
				gearEffectPossibility *= 0.5f;
		}
		if(cry_frand() < gearEffectPossibility)
			gearEffectId = pMaterialEffects->GetEffectIdByName("footsteps", "gear");

		TMFXEffectId gearSearchEffectId = InvalidEffectId;
		if (GetStance() == STANCE_STEALTH && cry_frand() < 0.3f)
		{
			if (IAIObject* pAI = GetEntity()->GetAI())
				if (pAI->GetProxy() && pAI->GetProxy()->GetAlertnessState() > 0)
					gearSearchEffectId = pMaterialEffects->GetEffectIdByName("footsteps", "gear_search");
		}

		Vec3 proxyOffset = Vec3(ZERO);
		Matrix34 tm = GetEntity()->GetWorldTM();
		tm.Invert();
		params.soundProxyOffset = tm.TransformVector(params.pos - GetEntity()->GetWorldPos());

		//play effects
		pMaterialEffects->ExecuteEffect(effectId, params);
		if(gearEffectId != InvalidEffectId)
			pMaterialEffects->ExecuteEffect(gearEffectId, params);
		if(gearSearchEffectId != InvalidEffectId)
			pMaterialEffects->ExecuteEffect(gearSearchEffectId, params);

		//switch foot
		m_currentFootID = footID;	

		if (!gEnv->bMultiplayer && gEnv->pAISystem)
		{
			float pseudoSpeed = 0.0f;
			if (m_stats.velocity.GetLengthSquared() > sqr(0.01f))
				pseudoSpeed = CalculatePseudoSpeed(m_stats.bSprinting);

			//handle AI sound recognition *************************************************
			float footstepRadius = 20.0f;
			float proneMult = 0.2f;
			float crouchMult = 0.5f;
			float movingMult = 2.5f;

			IMaterialManager *mm = gEnv->p3DEngine->GetMaterialManager();
			ISurfaceType *surface = mm->GetSurfaceTypeManager()->GetSurfaceType(pStats->groundMaterialIdx);
			if (surface) 
			{
				const ISurfaceType::SSurfaceTypeAIParams *aiParams = surface->GetAIParams();
				if (aiParams)
				{
					footstepRadius = aiParams->fFootStepRadius;
					crouchMult = aiParams->crouchMult;
					proneMult = aiParams->proneMult;
					movingMult = aiParams->movingMult;
				}
			}

			if (stance == STANCE_CROUCH)
				footstepRadius *= crouchMult;
			else if (stance == STANCE_PRONE)
				footstepRadius *= proneMult;
			else if (relativeSpeed > 0.1f)
			{
				// Scale foot steps based on speed, pseudo speed is in range [1.0..3.0].
				float u = clamp(pseudoSpeed - 1.0f, 0.0f, 2.0f) / 2.0f;
				footstepRadius *= 1.0f + (movingMult - 1.0f) * u;
			}

			// Handle nano-suit sound dampening
			CPlayer *pPlayer = (this->GetActorClass() == CPlayer::GetActorClassType()) ? (CPlayer*) this : 0;
			float soundDamp = 1.0f;
			if (pPlayer != 0)
				if (CNanoSuit *pSuit = pPlayer->GetNanoSuit())
					soundDamp = pSuit->GetCloak()->GetSoundDamp();
			footstepRadius *= soundDamp;

			if (GetEntity()->GetAI())
				gEnv->pAISystem->SoundEvent(GetEntity()->GetWorldPos(), footstepRadius, AISE_MOVEMENT, GetEntity()->GetAI());
			else
				gEnv->pAISystem->SoundEvent(GetEntity()->GetWorldPos(), footstepRadius, AISE_MOVEMENT, NULL);
		}
	}
}

void CPlayer::SwitchDemoModeSpectator(bool activate)
{
	if(!IsDemoPlayback())
		return;

	m_bDemoModeSpectator = activate;

	m_stats.isThirdPerson = !activate;
	if(activate)
		m_stats.firstPersonBody = (uint8)g_pGameCVars->cl_fpBody;
	CItem *pItem = GetItem(GetInventory()->GetCurrentItem());
	if(pItem)
		pItem->UpdateFPView(0);

	IVehicle* pVehicle = GetLinkedVehicle();
	if (pVehicle)
	{
		IVehicleSeat* pVehicleSeat = pVehicle->GetSeatForPassenger( GetEntityId() );
		if (pVehicleSeat)
			pVehicleSeat->SetView( activate ? pVehicleSeat->GetNextView(InvalidVehicleViewId) : InvalidVehicleViewId );
	}

	if (activate)
	{
		IScriptSystem * pSS = gEnv->pScriptSystem;
		pSS->SetGlobalValue( "g_localActor", GetGameObject()->GetEntity()->GetScriptTable() );
		pSS->SetGlobalValue( "g_localActorId", ScriptHandle( GetGameObject()->GetEntityId() ) );
	}

	m_isClient = activate;
}

void CPlayer::ActivateNanosuit(bool active)
{
	if(active)
	{
		if(!m_pNanoSuit)	//created on first activation
			m_pNanoSuit = new CNanoSuit();

		m_pNanoSuit->Reset(this);
		m_pNanoSuit->Activate(true);
	}
	else if(m_pNanoSuit)
	{
		m_pNanoSuit->Activate(false);
	}
}

void CPlayer::SetFlyMode(uint8 flyMode)
{
	if (m_stats.spectatorMode)
		return;

	m_stats.flyMode = flyMode;

	if (m_stats.flyMode>2)
		m_stats.flyMode = 0;

	if(m_pAnimatedCharacter)
		m_pAnimatedCharacter->RequestPhysicalColliderMode((m_stats.flyMode==2)?eColliderMode_Disabled:eColliderMode_Undefined, eColliderModeLayer_Game, "Player::SetFlyMode");
}

void CPlayer::SetSpectatorMode(uint8 mode, EntityId targetId)
{
	//CryLog("%s setting spectator mode %d, target %d", GetEntity()->GetName(), mode, targetId);

	uint8 oldSpectatorMode=m_stats.spectatorMode;
	bool server=gEnv->bServer;
	if(server)
		m_stats.spectatorHealth = -1;	// set this in all cases to trigger a send if necessary

	if(gEnv->bClient)
		m_pPlayerInput.reset();

	if (mode && !m_stats.spectatorMode)
	{
		if (IVehicle *pVehicle=GetLinkedVehicle())
		{
			if (IVehicleSeat *pSeat=pVehicle->GetSeatForPassenger(GetEntityId()))
				pSeat->Exit(false);
		}

		Revive(false);

		if (server)
		{
			GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Spectator);
			GetGameObject()->InvokeRMI(CActor::ClSetSpectatorMode(), CActor::SetSpectatorModeParams(mode, targetId), eRMI_ToAllClients|eRMI_NoLocalCalls);
		}

		Draw(false);

		m_stats.spectatorTarget = targetId;
		m_stats.spectatorMode=mode;
		m_stats.inAir=0.0f;
		m_stats.onGround=0.0f;

		// spectators should be dead
		m_health=-1;
		GetGameObject()->ChangedNetworkState(ASPECT_HEALTH);

		if(mode == CActor::eASM_Follow)
			MoveToSpectatorTargetPosition();
	}
	else if (!mode && m_stats.spectatorMode)
	{
		if (server)
		{
			GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
			GetGameObject()->InvokeRMI(CActor::ClSetSpectatorMode(), CActor::SetSpectatorModeParams(mode, targetId), eRMI_ToAllClients|eRMI_NoLocalCalls);
		}

		Draw(true);

		m_stats.spectatorTarget = 0;
		m_stats.spectatorMode=mode;
		m_stats.inAir=0.0f;
		m_stats.onGround=0.0f;
	}
	else if (oldSpectatorMode!=mode || m_stats.spectatorTarget != targetId)
	{
		m_stats.spectatorTarget = targetId;
		m_stats.spectatorMode=mode;

		if (server)
		{
			//SetHealth(GetMaxHealth());
			GetGameObject()->InvokeRMI(CActor::ClSetSpectatorMode(), CActor::SetSpectatorModeParams(mode, targetId), eRMI_ToClientChannel|eRMI_NoLocalCalls, GetChannelId());			
		}

		if(mode == CActor::eASM_Follow)
			MoveToSpectatorTargetPosition();
	}
	/*
	// switch on/off spectator HUD
	if (IsClient())
	SAFE_HUD_FUNC(Show(mode==0));
	*/
}

void CPlayer::MoveToSpectatorTargetPosition()
{
	// called when our target entity moves.
	IEntity* pTarget = gEnv->pEntitySystem->GetEntity(m_stats.spectatorTarget);
	if(!pTarget)
		return;

	if(g_pGameCVars->g_spectate_FixedOrientation)
	{
		Matrix34 tm = pTarget->GetWorldTM();
		tm.AddTranslation(Vec3(0,0,2));
		GetEntity()->SetWorldTM(tm);
	}
	else
	{
		// only copy position of target, not orientation
		Matrix34 ownTm = GetEntity()->GetWorldTM();
		Vec3 trans = pTarget->GetWorldTM().GetTranslation();
		trans += Vec3(0,0,2);
		ownTm.SetTranslation(trans);
		GetEntity()->SetWorldTM(ownTm);
	}
}

void CPlayer::Stabilize(bool stabilize)
{
	m_bStabilize = stabilize;
	if(!stabilize && m_pNanoSuit)
		m_pNanoSuit->PlaySound(ESound_ZeroGThruster, 1.0f, true);
}

bool CPlayer::UseItem(EntityId itemId)
{
	const bool bOK = CActor::UseItem(itemId);
	if (bOK)
	{
		CALL_PLAYER_EVENT_LISTENERS(OnItemUsed(this, itemId));
	}
	return bOK;
}

bool CPlayer::PickUpItem(EntityId itemId, bool sound)
{
	const bool bOK = CActor::PickUpItem(itemId, sound);
	if (bOK)
	{
		CALL_PLAYER_EVENT_LISTENERS(OnItemPickedUp(this, itemId));
	}
	return bOK;
}

bool CPlayer::DropItem(EntityId itemId, float impulseScale, bool selectNext, bool byDeath)
{
	const bool bOK = CActor::DropItem(itemId, impulseScale, selectNext, byDeath);
	if (bOK)
	{
		CALL_PLAYER_EVENT_LISTENERS(OnItemDropped(this, itemId));
	}
	return bOK;
}

void CPlayer::UpdateUnfreezeInput(const Ang3 &deltaRotation, const Vec3 &deltaMovement, float mult)
{
	// unfreeze with mouse shaking and movement keys  
	float deltaRot = (abs(deltaRotation.x) + abs(deltaRotation.z)) * mult;
	float deltaMov = abs(deltaMovement.x) + abs(deltaMovement.y);

	static float color[] = {1,1,1,1};    

	if (g_pGameCVars->cl_debugFreezeShake)
	{    
		gEnv->pRenderer->Draw2dLabel(100,50,1.5,color,false,"frozenAmount: %f (actual: %f)", GetFrozenAmount(true), m_frozenAmount);
		gEnv->pRenderer->Draw2dLabel(100,80,1.5,color,false,"deltaRotation: %f (freeze mult: %f)", deltaRot, mult);
		gEnv->pRenderer->Draw2dLabel(100,110,1.5,color,false,"deltaMovement: %f", deltaMov);
	}

	float freezeDelta = deltaRot*g_pGameCVars->cl_frozenMouseMult + deltaMov*g_pGameCVars->cl_frozenKeyMult;

	if (freezeDelta >0)    
	{
		if (CNanoSuit *pSuit = GetNanoSuit())
		{
			if(pSuit && pSuit->IsActive())
			{
				// add suit strength
				float strength = pSuit->GetSlotValue(NANOSLOT_STRENGTH);
				strength = max(-0.75f, (strength-50)/50.f) * freezeDelta;
				freezeDelta += strength;

				if (g_pGameCVars->cl_debugFreezeShake)
				{ 
					gEnv->pRenderer->Draw2dLabel(100,140,1.5,color,false,"freezeDelta: %f (suit mod: %f)", freezeDelta, strength);
				}
			}
		}

		GetGameObject()->InvokeRMI(SvRequestUnfreeze(), UnfreezeParams(freezeDelta), eRMI_ToServer);

		float prevAmt = GetFrozenAmount(true);
		m_frozenAmount-=freezeDelta;
    float newAmt = GetFrozenAmount(true);
		if (freezeDelta > g_pGameCVars->cl_frozenSoundDelta || newAmt!=prevAmt)
			CreateScriptEvent("unfreeze_shake", newAmt!=prevAmt ? freezeDelta : 0);
	}
}

void CPlayer::SpawnParticleEffect(const char* effectName, const Vec3& pos, const Vec3& dir)
{
	IParticleEffect *pEffect = gEnv->p3DEngine->FindParticleEffect(effectName, "CPlayer::SpawnParticleEffect");
	if (pEffect == NULL)
		return;

	pEffect->Spawn(true, IParticleEffect::ParticleLoc(pos, dir, 1.0f));
}

void CPlayer::PlaySound(EPlayerSounds sound, bool play, bool param /*= false*/, const char* paramName /*=NULL*/, float paramValue /*=0.0f*/)
{
	if(!gEnv->pSoundSystem)
		return;
	if(!IsClient()) //currently this is only supposed to be heared by the client (not 3D, not MP)
		return;


	bool repeating = false;
	uint32 nFlags = 0;

	ESoundSemantic soundSemantic = eSoundSemantic_Player_Foley;

	const char* soundName = NULL;
	switch(sound)
	{
	case ESound_Run:
		soundName = "sounds/physics:player_foley:run_feedback";
		soundSemantic = eSoundSemantic_Player_Foley_Voice;
		repeating = true;
		break;
	case ESound_StopRun:
		soundName = "sounds/physics:player_foley:catch_breath_feedback";
		soundSemantic = eSoundSemantic_Player_Foley_Voice;
		break;
	case ESound_Jump:
		soundName = "Sounds/physics:player_foley:jump_feedback";
		soundSemantic = eSoundSemantic_Player_Foley_Voice;
		if (gEnv->pInput && play) gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.05f, 0.05f, 0.1f) );
		break;
	case ESound_Fall_Drop:
		soundName = "Sounds/physics:player_foley:bodyfall_feedback";
		soundSemantic = eSoundSemantic_Player_Foley_Voice;
		if (gEnv->pInput && play) gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.2f, 0.3f, 0.2f) );
		break;
	case ESound_Melee:
		soundName = "Sounds/physics:player_foley:melee_feedback";
		soundSemantic = eSoundSemantic_Player_Foley_Voice;
		if (gEnv->pInput && play) gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.15f, 0.6f, 0.2f) );
		break;
	case ESound_Fear:
		soundName = "Sounds/physics:player_foley:alien_feedback";
		repeating = true;
		break;
	case ESound_Choking:
		soundName = "Languages/dialog/ai_korean01/choking_01.wav";
		repeating = true;
		break;
	case ESound_Hit_Wall:
		soundName = "Sounds/physics:player_foley:body_hits_wall";
		repeating = false;
		param = true;
		break;
	case ESound_UnderwaterBreathing:
		soundName = "sounds/physics:player_foley:underwater_feedback";
		soundSemantic = eSoundSemantic_Player_Foley_Voice;
		repeating = true; // fake that it's looping only here in game code, so that we keep a handle to stop it early when drowning.
		break;
	case ESound_Underwater:
		soundName = "sounds/environment:amb_natural:ambience_underwater";
		repeating = true;
		break;
	case ESound_Drowning:
		soundName = "sounds/physics:player_foley:drown_feedback";
		soundSemantic = eSoundSemantic_Player_Foley_Voice;
		repeating = false;
		break;
	case ESound_DiveIn:
		soundName = "Sounds/physics:player_foley:dive_in";
		repeating = false;
		break;
	case ESound_DiveOut:
		soundName = "Sounds/physics:player_foley:dive_out";
		repeating = false;
		break;

	case ESound_Thrusters:
		soundName = "sounds/interface:suit:thrusters_1p";
		if (!IsThirdPerson())
			nFlags |= FLAG_SOUND_RELATIVE;
		repeating = true;
		break;
	case ESound_ThrustersDash:
		//soundName = "sounds/interface:suit:suit_deep_freeze";
		soundName = "sounds/interface:suit:thrusters_boost_activate";
		if (!IsThirdPerson())
			nFlags |= FLAG_SOUND_RELATIVE;
		repeating = true;
		break;
	case ESound_ThrustersDash02:
		soundName = "sounds/interface:suit:suit_speed_activate";
		if (!IsThirdPerson())
			nFlags |= FLAG_SOUND_RELATIVE;
		repeating = false;
		break;
	case ESound_ThrustersDashFail:
		soundName = "sounds/interface:hud:key_error";
		if (!IsThirdPerson())
			nFlags |= FLAG_SOUND_RELATIVE;
		repeating = false;
		break;
	case ESound_ThrustersDashRecharged:
		//soundName = "sounds/interface:suit:suit_gravity_boots_deactivate";
		soundName = "sounds/interface:suit:thrusters_boost_recharge";
		if (!IsThirdPerson())
			nFlags |= FLAG_SOUND_RELATIVE;
		repeating = false;
		break;
	case ESound_ThrustersDashRecharged02:
		soundName = "sounds/interface:suit:suit_speed_stop";
		soundSemantic = eSoundSemantic_Player_Foley_Voice;
		if (!IsThirdPerson())
			nFlags |= FLAG_SOUND_RELATIVE;
		repeating = false;
		break;
	case ESound_TacBulletFeedBack:
		soundName = "sounds/physics:bullet_impact:tac_bullet_impact";
		soundSemantic = eSoundSemantic_Player_Foley;
		if (!IsThirdPerson())
			nFlags |= FLAG_SOUND_RELATIVE;
		repeating = false;
	default:
		break;
	}

	if(!soundName)
		return;

	if(play)
	{
		// don't play voice-type foley sounds
		if (soundSemantic == eSoundSemantic_Player_Foley_Voice && m_bVoiceSoundPlaying)
			return;

		ISound *pSound = NULL;
		if(repeating && m_sounds[sound])
			if(pSound = gEnv->pSoundSystem->GetSound(m_sounds[sound]))
				if(pSound->IsPlaying())
				{
					if (param)
						pSound->SetParam(paramName, paramValue);
					return;
				}

		if(!pSound)
			pSound = gEnv->pSoundSystem->CreateSound(soundName, nFlags);

		if(pSound)
		{
			pSound->SetSemantic(soundSemantic);

			if(repeating)
				m_sounds[sound] = pSound->GetId();

			
			IEntity *pEntity = GetEntity();
			CRY_ASSERT(pEntity);

			if(pEntity)
			{
				IEntitySoundProxy* pSoundProxy = (IEntitySoundProxy*)pEntity->CreateProxy(ENTITY_PROXY_SOUND);
				CRY_ASSERT(pSoundProxy);

				if (param)
					pSound->SetParam(paramName, paramValue);

				if(pSoundProxy)
					pSoundProxy->PlaySound(pSound);  
			}
		}
	}
	else if(repeating && m_sounds[sound])
	{
		ISound *pSound = gEnv->pSoundSystem->GetSound(m_sounds[sound]);
		if(pSound)
			pSound->Stop();
		m_sounds[sound] = 0;
	}
}

//===========================LADDERS======================

// NB this will store the details of the usable ladder in m_stats
bool CPlayer::IsLadderUsable()
{
	IPhysicalEntity* pPhysicalEntity;

	const ray_hit *pRay = GetGameObject()->GetWorldQuery()->GetLookAtPoint(2.0f);

	//Check collision with a ladder
	if (pRay)
		pPhysicalEntity = pRay->pCollider;
	else 
		return false;

	//Check the Material (first time)
	if(!CPlayer::s_ladderMaterial)
	{
		ISurfaceType *ladderSurface = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceTypeByName("mat_ladder");
		if(ladderSurface)
		{
			CPlayer::s_ladderMaterial = ladderSurface->GetId();
		}
	}

	//Is it a ladder?
	if(pPhysicalEntity && pRay->surface_idx == CPlayer::s_ladderMaterial)
	{
		IStatObj* staticLadder = NULL;
		Matrix34 worldTM;
		Matrix34 partLocalTM;

		//Get Static object from physical entity
		pe_params_part ppart;
		ppart.partid  = pRay->partid;
		ppart.pMtx3x4 = &partLocalTM;
		if (pPhysicalEntity->GetParams( &ppart ))
		{
			if (ppart.pPhysGeom && ppart.pPhysGeom->pGeom)
			{
				void *ptr = ppart.pPhysGeom->pGeom->GetForeignData(0);

				pe_status_pos ppos;
				ppos.pMtx3x4 = &worldTM;
				if (pPhysicalEntity->GetStatus(&ppos) != 0)
				{
					worldTM = worldTM * partLocalTM;
					staticLadder = (IStatObj*)ptr;
				}
			}
		}

		if(!staticLadder)
			return false;

		//Check OffHand 
		if(COffHand* pOffHand = static_cast<COffHand*>(GetItemByClass(CItem::sOffHandClass)))
			if(pOffHand->GetOffHandState()!=eOHS_INIT_STATE)
				return false;	

		Vec3 ladderOrientation = worldTM.GetColumn1().normalize();
		Vec3 ladderUp = worldTM.GetColumn2().normalize();

		//Is ladder vertical? 
		if(ladderUp.z<0.85f)
		{
			return false;
		}

		AABB box = staticLadder->GetAABB();
		Vec3 center = worldTM.GetTranslation() + 0.5f*ladderOrientation;

		m_stats.ladderTop = center + (ladderUp*(box.max.z - box.min.z));
		m_stats.ladderBottom = center;
		m_stats.ladderOrientation = ladderOrientation;
		m_stats.ladderUpDir = ladderUp;

		//Test angle between player and ladder	
		IMovementController* pMC = GetMovementController();
		if(pMC)
		{
			SMovementState info;
			pMC->GetMovementState(info);

			//Predict if the player try to enter the top of the ladder from the opposite side
			float zDistance = m_stats.ladderTop.z - pRay->pt.z;
			if(zDistance<1.0f && ladderOrientation.Dot(-info.eyeDirection)<0.0f)
			{
				return true;
			}
// 			else if(ladderOrientation.Dot(-info.eyeDirection)<0.5f)
// 			{
// 				return false;
// 			}
		}	
		return true;
	}
	else
	{
		return false;
	}
}

void CPlayer::RequestGrabOnLadder(ELadderActionType reason)
{
	if(gEnv->bServer)
	{
		GrabOnLadder(reason);
	}
	else
	{
		GetGameObject()->InvokeRMI(SvRequestGrabOnLadder(), LadderParams(m_stats.ladderTop, m_stats.ladderBottom, m_stats.ladderOrientation, reason), eRMI_ToServer);
	}
}

void CPlayer::GrabOnLadder(ELadderActionType reason)
{
	HolsterItem(true);

	m_stats.isOnLadder = true;
	m_stats.isExitingLadder = false;
	m_stats.ladderAction = reason;

	if(m_pAnimatedCharacter)
		m_pAnimatedCharacter->RequestPhysicalColliderMode(eColliderMode_PushesPlayersOnly, eColliderModeLayer_Game, "Player::GrabOnLadder");
	
	// SNH: moved this from IsLadderUsable
	if (IsClient() || gEnv->bServer)	
	{
		IMovementController* pMC = GetMovementController();
		if(pMC)
		{
			const ray_hit *pRay = GetGameObject()->GetWorldQuery()->GetLookAtPoint(2.0f);
			SMovementState info;
			pMC->GetMovementState(info);

			//Predict if the player try to enter the top of the ladder from the opposite side
			if(pRay)
			{
				float zDistance = m_stats.ladderTop.z - pRay->pt.z;
				if(zDistance<1.0f && m_stats.ladderOrientation.Dot(-info.eyeDirection)<0.0f)
				{
					//Move the player in front of the ladder
					Matrix34 entryPos = GetEntity()->GetWorldTM();
					entryPos.SetTranslation(m_stats.ladderTop-(m_stats.ladderUpDir*2.5f));
					GetEntity()->SetWorldTM(entryPos);

					//Player orientation
					GetEntity()->SetRotation(Quat(Matrix33::CreateOrientation(-m_stats.ladderOrientation,m_stats.ladderUpDir,g_PI)));
					m_stats.playerRotation = GetEntity()->GetRotation();

				}
				else
				{
					//Move the player in front of the ladder
					Matrix34 entryPos = GetEntity()->GetWorldTM();
					entryPos.SetTranslation(Vec3(m_stats.ladderBottom.x,m_stats.ladderBottom.y,min(max(entryPos.GetTranslation().z,m_stats.ladderBottom.z),m_stats.ladderTop.z-2.4f)));
					GetEntity()->SetWorldTM(entryPos);

					//Player orientation
					GetEntity()->SetRotation(Quat(Matrix33::CreateOrientation(-m_stats.ladderOrientation,m_stats.ladderUpDir,g_PI)));
					m_stats.playerRotation = GetEntity()->GetRotation();

				}
			}
		}
	}

	if (gEnv->bServer)
		GetGameObject()->InvokeRMI(ClGrabOnLadder(), LadderParams(m_stats.ladderTop, m_stats.ladderBottom, m_stats.ladderOrientation, reason), eRMI_ToRemoteClients);

	if(g_pGame->GetHUD())
	{
		if(IItem *pFists = GetItemByClass(CItem::sFistsClass))
			g_pGame->GetHUD()->SetFireMode(pFists, NULL);
	}
}

void CPlayer::RequestLeaveLadder(ELadderActionType reason)
{
	if(gEnv->bServer)
	{
		LeaveLadder(reason);
	}
	else
	{
		GetGameObject()->InvokeRMI(SvRequestLeaveLadder(), LadderParams(m_stats.ladderTop, m_stats.ladderBottom, m_stats.ladderOrientation, reason), eRMI_ToServer);
	}
}

void CPlayer::LeaveLadder(ELadderActionType reason)
{
	//If playing animation controlled exit, not leave until anim finishes
	if(m_stats.isExitingLadder && reason!=eLAT_ReachedEnd && reason!=eLAT_Die)
		return;

	if(reason==eLAT_ExitTop)
	{
		m_stats.isExitingLadder = true;
		UpdateLadderAnimation(CPlayer::eLS_ExitTop,eLDIR_Up);
	}
	else
	{
		m_stats.isExitingLadder = false;
		m_stats.isOnLadder  = false;
		m_stats.ladderAction = reason;

		if(m_pAnimatedCharacter)
			m_pAnimatedCharacter->RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "Player::LeaveLadder");

		HolsterItem(false);

		// SNH: moved this here from CPlayerMovement::ProcessExitLadder
		{
			Matrix34 finalPlayerPos = GetEntity()->GetWorldTM();

			Vec3 rightDir = m_stats.ladderUpDir.Cross(m_stats.ladderOrientation);
			rightDir.Normalize();

			switch(reason)
			{
				// top (was 0)
			case eLAT_ReachedEnd:
				break;

			case eLAT_StrafeRight:
				finalPlayerPos.AddTranslation(rightDir*0.3f);
				break;

			case eLAT_StrafeLeft:
				finalPlayerPos.AddTranslation(-rightDir*0.3f);
				break;

			case eLAT_Jump:
				finalPlayerPos.AddTranslation(m_stats.ladderOrientation*0.3f);
				break;
			}
			if(reason!=eLAT_ReachedEnd)
				GetEntity()->SetWorldTM(finalPlayerPos);	
		}
		UpdateLadderAnimation(CPlayer::eLS_Exit,CPlayer::eLDIR_Stationary);
	}

	if (gEnv->bServer)
		GetGameObject()->InvokeRMI(ClLeaveLadder(), LadderParams(m_stats.ladderTop, m_stats.ladderBottom, m_stats.ladderOrientation, reason), eRMI_ToAllClients);

}


//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, SvRequestGrabOnLadder)
{
	if(IsLadderUsable() && m_stats.ladderTop.IsEquivalent(params.topPos) && m_stats.ladderBottom.IsEquivalent(params.bottomPos))
	{
		GrabOnLadder(static_cast<ELadderActionType>(params.reason));
	}
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, SvRequestLeaveLadder)
{
	if(m_stats.isOnLadder)
	{
		if(m_stats.ladderTop.IsEquivalent(params.topPos) && m_stats.ladderBottom.IsEquivalent(params.bottomPos))
		{
			LeaveLadder(static_cast<ELadderActionType>(params.reason));
		}
	}
	
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, ClGrabOnLadder)
{
	// other players should always be attached to the ladder they are told to
	if(!IsClient())
	{
		m_stats.ladderTop = params.topPos;
		m_stats.ladderBottom = params.bottomPos;
		m_stats.ladderOrientation = params.ladderOrientation;
		m_stats.ladderUpDir = Vec3(0,0,1);
	}

	if(!IsClient() || (IsLadderUsable() && m_stats.ladderTop.IsEquivalent(params.topPos) && m_stats.ladderBottom.IsEquivalent(params.bottomPos)))
	{
		GrabOnLadder(static_cast<ELadderActionType>(params.reason));
	}
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, ClLeaveLadder)
{
	// probably not worth checking the positions here - just get off the ladder whatever.
	if(m_stats.isOnLadder)
	{
		LeaveLadder(static_cast<ELadderActionType>(params.reason));	
	}
	return true;
}

//-----------------------------------------------------------------------
bool CPlayer::UpdateLadderAnimation(ELadderState eLS, ELadderDirection eLDIR, float time /*=0.0f*/)
{
	switch(eLS)
	{
			case eLS_ExitTop:
				m_pAnimatedCharacter->GetAnimationGraphState()->SetInput("Signal","exit_ladder_top");
				//break;

			case eLS_Exit:			
				m_pAnimatedCharacter->GetAnimationGraphState()->SetInput("Action","idle");
				break;

			case eLS_Climb:
				m_pAnimatedCharacter->GetAnimationGraphState()->SetInput("Action","climbLadderUp");
				break;

			default:
				break;
	}
	//Manual animation Update

	if(eLS==eLS_Climb)
	{			
		if (ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0))
		{
			ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();
			assert(pSkeletonAnim);

			if (uint32 numAnimsLayer = pSkeletonAnim->GetNumAnimsInFIFO(0))
			{
				CAnimation &animation = pSkeletonAnim->GetAnimFromFIFO(0, 0);
				if (animation.m_AnimParams.m_nFlags & CA_MANUAL_UPDATE)
				{
					pSkeletonAnim->ManualSeekAnimationInFIFO(0, 0, time, eLDIR == eLDIR_Up);
					return true;
				}
			}
		}
	}
	
	return false;
}

//-----------------------------------------------------------------------
void CPlayer::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	CActor::GetActorMemoryStatistics(s);
	if (m_pNanoSuit)
		m_pNanoSuit->GetMemoryStatistics(s);
	if (m_pPlayerInput.get())
		m_pPlayerInput->GetMemoryStatistics(s);
	s->AddContainer(m_clientPostEffects);
  
	if (m_pDebugHistoryManager)
    m_pDebugHistoryManager->GetMemoryStatistics(s);
}


void CPlayer::Debug()
{
	bool debug = (g_pGameCVars->pl_debug_movement != 0);

	const char* filter = g_pGameCVars->pl_debug_filter->GetString();
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

	bool showReqVelo = true;
	m_pDebugHistoryManager->LayoutHelper("ReqVelo", NULL, showReqVelo, -20, 20, 0, 5, 0.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("ReqVeloX", NULL, showReqVelo, -20, 20, -5, 5, 1.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("ReqVeloY", NULL, showReqVelo, -20, 20, -5, 5, 2.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("ReqVeloZ", NULL, showReqVelo, -20, 20, -5, 5, 3.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("ReqRotZ", NULL, showReqVelo, -360, 360, -5, 5, 4.0f, 0.0f);

	m_pDebugHistoryManager->LayoutHelper("PhysVelo", NULL, showReqVelo, -20, 20, 0, 5, 0.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("PhysVeloX", NULL, showReqVelo, -20, 20, -5, 5, 1.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("PhysVeloY", NULL, showReqVelo, -20, 20, -5, 5, 2.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("PhysVeloZ", NULL, showReqVelo, -20, 20, -5, 5, 3.0f, 1.0f);

	m_pDebugHistoryManager->LayoutHelper("PhysVeloUn", NULL, showReqVelo, -20, 20, 0, 5, 0.0f, 2.0f);
	m_pDebugHistoryManager->LayoutHelper("PhysVeloUnX", NULL, showReqVelo, -20, 20, -5, 5, 1.0f, 2.0f);
	m_pDebugHistoryManager->LayoutHelper("PhysVeloUnY", NULL, showReqVelo, -20, 20, -5, 5, 2.0f, 2.0f);
	m_pDebugHistoryManager->LayoutHelper("PhysVeloUnZ", NULL, showReqVelo, -20, 20, -5, 5, 3.0f, 2.0f);

/*
	m_pDebugHistoryManager->LayoutHelper("InputMoveX", NULL, showReqVelo, -1, 1, -1, 1, 0.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("InputMoveY", NULL, showReqVelo, -1, 1, -1, 1, 1.0f, 1.0f);
/**/

/*
	bool showVelo = true;
	m_pDebugHistoryManager->LayoutHelper("Velo", NULL, showVelo, -20, 20, 0, 8, 0.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("VeloX", NULL, showVelo, -20, 20, -5, 5, 1.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("VeloY", NULL, showVelo, -20, 20, -5, 5, 2.0f, 0.0f);
	m_pDebugHistoryManager->LayoutHelper("VeloZ", NULL, showVelo, -20, 20, -5, 5, 3.0f, 0.0f);
/**/

/*
	m_pDebugHistoryManager->LayoutHelper("Axx", NULL, showVelo, -20, 20, -1, 1, 0.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("AxxX", NULL, showVelo, -20, 20, -1, 1, 1.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("AxxY", NULL, showVelo, -20, 20, -1, 1, 2.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("AxxZ", NULL, showVelo, -20, 20, -1, 1, 3.0f, 1.0f);
/**/

	//m_pDebugHistoryManager->LayoutHelper("ModelOffsetX", NULL, true, 0, 1, -0.5, 0.5, 5.0f, 0.5f);
	//m_pDebugHistoryManager->LayoutHelper("ModelOffsetY", NULL, true, 0, 1, 0, 1, 5.0f, 1.5f, 1.0f, 0.5f);

//*
	bool showJump = true;
	m_pDebugHistoryManager->LayoutHelper("OnGround", NULL, showJump, 0, 1, 0, 1, 5.0f, 0.5f, 1.0f, 0.5f);
	m_pDebugHistoryManager->LayoutHelper("Jumping", NULL, showJump, 0, 1, 0, 1, 5.0f, 1.0f, 1.0f, 0.5f);
	m_pDebugHistoryManager->LayoutHelper("Flying", NULL, showJump, 0, 1, 0, 1, 5.0f, 1.5f, 1.0f, 0.5f);
	m_pDebugHistoryManager->LayoutHelper("StuckTimer", NULL, showJump, 0, 0.5, 0, 0.5, 5.0f, 2.0f, 1.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("InAirTimer", NULL, showJump, 0, 5, 0, 5, 4.0f, 2.0f, 1.0f, 1.0f);
	m_pDebugHistoryManager->LayoutHelper("InWaterTimer", NULL, showJump, -5, 5, -0.5, 0.5, 4, 3);
	m_pDebugHistoryManager->LayoutHelper("OnGroundTimer", NULL, showJump, 0, 5, 0, 5, 4.0f, 1.0f, 1.0f, 1.0f);
/**/


//*
	m_pDebugHistoryManager->LayoutHelper("GroundSlope", NULL, true, 0, 90, 0, 90, 0, 3);
	m_pDebugHistoryManager->LayoutHelper("GroundSlopeMod", NULL, true, 0, 90, 0, 90, 1, 3);
/**/

	//m_pDebugHistoryManager->LayoutHelper("ZGDashTimer", NULL, showVelo, -20, 20, -0.5, 0.5, 5.0f, 0.5f);
/*
	m_pDebugHistoryManager->LayoutHelper("StartTimer", NULL, showVelo, -20, 20, -0.5, 0.5, 5.0f, 0.5f);
	m_pDebugHistoryManager->LayoutHelper("DodgeFraction", NULL, showVelo, 0, 1, 0, 1, 5.0f, 1.5f, 1.0f, 0.5f);
	m_pDebugHistoryManager->LayoutHelper("RampFraction", NULL, showVelo, 0, 1, 0, 1, 5.0f, 2.0f, 1.0f, 0.5f);
	m_pDebugHistoryManager->LayoutHelper("ThrustAmp", NULL, showVelo, 0, 5, 0, 5, 5.0f, 2.5f, 1.0f, 0.5f);
*/
}

void CPlayer::DebugGraph_AddValue(const char* id, float value) const
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

//Try to predict if the player needs to go to crouch stance to pick up a weapon/item
bool CPlayer::NeedToCrouch(const Vec3& pos)
{
	if (IMovementController *pMC = GetMovementController())
	{
		SMovementState state;
		pMC->GetMovementState(state);

		// determine height delta (0 is at player feet ... greater zero is higher and means shorter crouch time) 
		float delta = pos.z - GetEntity()->GetWorldPos().z;
		Limit(delta, 0.0f, 0.8f);
		delta /= 0.8f;

		//Maybe this "prediction" is not really accurate...
		//If the player is looking down, probably the item is on the ground
		if(state.aimDirection.z<-0.7f && GetStance()!=STANCE_PRONE)
		{
			m_stats.forceCrouchTime = 0.75f * (1.0f - delta);
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------

void CPlayer::RemoveAllExplosives(float timeDelay, uint8 typeId)
{
	if(gEnv->bMultiplayer && gEnv->bServer)
	{
		int i=0;
		int n=3;
		
		if (typeId!=0xff)
		{
			i=typeId;
			n=typeId+1;
		}

		for (int i=0;i<3;i++)
		{
			std::list<EntityId> &explosives=m_explosiveList[i];

			for(std::list<EntityId>::iterator it = explosives.begin(); it != explosives.end(); ++it)
			{
				IEntity* pExplosive = gEnv->pEntitySystem->GetEntity(*it);
				if(pExplosive)
					pExplosive->SetTimer(CProjectile::ePTIMER_LIFETIME, (int)timeDelay);
			}
		}
	}
}

void CPlayer::RemoveExplosiveEntity(EntityId entityId)
{
	gEnv->pEntitySystem->RemoveEntity(entityId);
}

void CPlayer::RecordExplosivePlaced(EntityId entityId, uint8 typeId)
{
	int limit=0;
	bool debug = (g_pGameCVars->g_debugMines != 0);

	if (typeId==0)
		limit=g_pGameCVars->g_claymore_limit;
	else if (typeId==1)
		limit=g_pGameCVars->g_avmine_limit;

	std::list<EntityId> &explosives=m_explosiveList[typeId];

	if (limit && explosives.size()>=limit)
	{
		// remove the oldest mine.
		EntityId explosiveId = explosives.front();
		explosives.pop_front();
		if(debug)
			CryLog("%s: Explosive(%d) force removed: %d", GetEntity()->GetName(), typeId, explosiveId);
		gEnv->pEntitySystem->RemoveEntity(explosiveId);
	}
	explosives.push_back(entityId);

	if(debug)
		CryLog("%s: Explosive(%d) placed: %d, now %d", GetEntity()->GetName(), typeId, entityId, explosives.size());
}

void CPlayer::RecordExplosiveDestroyed(EntityId entityId, uint8 typeId)
{
	bool debug = (g_pGameCVars->g_debugMines != 0);
	
	std::list<EntityId> &explosives=m_explosiveList[typeId];

	std::list<EntityId>::iterator it = std::find(explosives.begin(), explosives.end(), entityId);
	if(it != explosives.end())
	{
		explosives.erase(it);
		if(debug)
			CryLog("%s: Explosive(%d) destroyed: %d, now %d", GetEntity()->GetName(), typeId, entityId, explosives.size());
	}
	else
	{
		if(debug)
			CryLog("%s: Explosive(%d) destroyed but not in list: %d", GetEntity()->GetName(), typeId, entityId);
	}
}

//------------------------------------------------------------------------
void CPlayer::EnterFirstPersonSwimming( )
{
	CFists *pFists = static_cast<CFists*>(GetItemByClass(CItem::sFistsClass));
	if(!pFists)
		return;

	COffHand *pOffHand = static_cast<COffHand*>(GetItemByClass(CItem::sOffHandClass));
	//Drop object or NPC
	if (pOffHand)
	{
		if(pOffHand->IsHoldingEntity())
		{
			//Force drop
			if(pOffHand->GetOffHandState()==eOHS_MELEE)
			{
				pOffHand->GetScheduler()->Reset();
				pOffHand->SetOffHandState(eOHS_HOLDING_OBJECT);
			}
			pOffHand->OnAction(GetEntityId(),"use",eAAM_OnPress,0);
			pOffHand->OnAction(GetEntityId(),"use",eAAM_OnRelease,0);
		}
		else if(pOffHand->GetOffHandState()==eOHS_HOLDING_GRENADE)
			pOffHand->FinishAction(eOHA_RESET);
	}

	//Entering the water (select fists...)
	CItem *currentItem = static_cast<CItem*>(GetCurrentItem());
	if(!currentItem)
	{
		//Player has no item selected... just select fists
		pFists->EnableAnimations(false);
		SelectItem(pFists->GetEntityId(),false);
		pFists->EnableAnimations(true);
	}
	else if(pFists->GetEntityId()==currentItem->GetEntityId())
	{
		//Fists already selected
		GetInventory()->SetLastItem(pFists->GetEntityId());
	}
	else
	{
		//Deselect current item and select fists
		currentItem->Select(false);
		pFists->EnableAnimations(false);
		SelectItem(pFists->GetEntityId(),false);
		pFists->EnableAnimations(true);
	}
	pFists->EnterWater(true);
	pFists->RequestAnimState(CFists::eFAS_SWIM_IDLE);

	SendMusicLogicEvent(eMUSICLOGICEVENT_PLAYER_SWIM_ENTER);

}

//------------------------------------------------------------------------
void CPlayer::ExitFirstPersonSwimming()
{
	CFists *pFists = static_cast<CFists*>(GetItemByClass(CItem::sFistsClass));
	if(!pFists)
		return;

	// we probably left water, so reactivate last item
	EntityId lastItemId = GetInventory()->GetLastItem();
	CItem *lastItem = (CItem *)gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(lastItemId);

	pFists->EnterWater(false);
	pFists->ResetAnimation();
	pFists->GetScheduler()->Reset();
	pFists->RequestAnimState(CFists::eFAS_NOSTATE);
	pFists->RequestAnimState(CFists::eFAS_FIGHT);

	if (GetLinkedVehicle())
		HolsterItem(false);

	if (lastItemId && lastItem && (lastItemId != pFists->GetEntityId()))
	{
		CBinocular *pBinoculars = static_cast<CBinocular*>(GetItemByClass(CItem::sBinocularsClass));
		if ((pBinoculars == NULL) || (pBinoculars->GetEntityId() != lastItemId))
		{
			//Select last item, except binoculars
			pFists->Select(false);
			SelectItem(lastItemId, true);
		}
	}

	if (GetLinkedVehicle())
		HolsterItem(true);   

	UpdateFirstPersonSwimmingEffects(false,0.0f);

	m_bUnderwater = false;

	SendMusicLogicEvent(eMUSICLOGICEVENT_PLAYER_SWIM_LEAVE);
}

//------------------------------------------------------------------------
void CPlayer::UpdateFirstPersonSwimming()
{
	bool swimming = false;

	if (ShouldSwim())
		swimming = true;

	if (m_stats.isOnLadder)
	{
		UpdateFirstPersonSwimmingEffects(false,0.0f);
		return;
	}
		
	if(swimming && !m_bSwimming)
	{
		EnterFirstPersonSwimming();
	}
	else if(!swimming && m_bSwimming)
	{
		ExitFirstPersonSwimming();
		m_bSwimming = swimming;
		return;
	}
	else if (swimming && m_bSwimming)
	{
		CFists *pFists = static_cast<CFists*>(GetItemByClass(CItem::sFistsClass));
		if(!pFists)
			return;

		//Retrieve some player info...
		pe_status_dynamics dyn;
		IPhysicalEntity *phys = GetEntity()->GetPhysics();

		// only thing used is dyn.v, which is set to zero by default constructor.
		if (phys)
			phys->GetStatus(&dyn);

		Vec3 direction(0,0,1);
		if (IMovementController *pMC = GetMovementController())
		{
			SMovementState state;
			pMC->GetMovementState(state);
			direction = state.aimDirection;
		}

		// dyn.v is zero by default constructor, meaning it's safe to use even if the GetParams call fails.
		bool moving = (dyn.v.GetLengthSquared() > 1.0f);
		float  dotP = dyn.v.GetNormalized().Dot(direction);
		bool underWater = m_stats.headUnderWaterTimer > 0.0f;

		if (moving && fabs_tpl(dotP)>0.1f)
		{
			if (dotP >= 0.1f)
			{
				CNanoSuit *pSuit = GetNanoSuit();
				if(pSuit && pSuit->GetMode()==NANOMODE_SPEED && m_stats.bSprinting)
					pFists->RequestAnimState(CFists::eFAS_SWIM_SPEED);
				else if(underWater)
					pFists->RequestAnimState(CFists::eFAS_SWIM_FORWARD_UW);
				else
					pFists->RequestAnimState(CFists::eFAS_SWIM_FORWARD);

				m_stats.bSprinting = false;
			}
			else
			{
				pFists->RequestAnimState(CFists::eFAS_SWIM_BACKWARD);
			}
		}
		else
		{
			// idling in water
			if (underWater)
				pFists->RequestAnimState(CFists::eFAS_SWIM_IDLE_UW);
			else
				pFists->RequestAnimState(CFists::eFAS_SWIM_IDLE);
		}

		UpdateFirstPersonSwimmingEffects(false,dyn.v.len2());
	}
	m_bSwimming = swimming;

}
//------------------------------------------------------------------------
void CPlayer::UpdateFirstPersonSwimmingEffects(bool exitWater, float velSqr)
{
	//CScreenEffects *pSE = GetScreenEffects();
	//if (pSE && (((m_stats.headUnderWaterTimer < 0.0f) && m_bUnderwater)||exitWater)) //going in and out of water
	//{
	//	pSE->ClearBlendGroup(14);
	//	CPostProcessEffect *blend = new CPostProcessEffect(GetEntityId(), "WaterDroplets_Amount", 1.0f);
	//	CWaveBlend *wave = new CWaveBlend();
	//	CPostProcessEffect *blend2 = new CPostProcessEffect(GetEntityId(), "WaterDroplets_Amount", 0.0f);
	//	CWaveBlend *wave2 = new CWaveBlend();
	//	pSE->StartBlend(blend, wave, 1.0f/.25f, 14);
	//	pSE->StartBlend(blend2, wave2, 1.0f/3.0f, 14);
	//	SAFE_HUD_FUNC(PlaySound(ESound_WaterDroplets));
	//}
	//else if(pSE && (m_stats.headUnderWaterTimer > 0.0f) && !m_bUnderwater)
	//{
	//	pSE->ClearBlendGroup(14);
	//	CPostProcessEffect *blend = new CPostProcessEffect(GetEntityId(), "WaterDroplets_Amount", 0.0f);
	//	CWaveBlend *wave = new CWaveBlend();
	//	pSE->StartBlend(blend,wave,4.0f,14);
	//	SAFE_HUD_FUNC(PlaySound(ESound_WaterDroplets));	
	//}

	//if(velSqr>40.0f && pSE)
	//{
	//	//Only when sprinting in speed mode under water
	//	pSE->ClearBlendGroup(98);
	//	gEnv->p3DEngine->SetPostEffectParam("FilterRadialBlurring_Radius", 0.3f);
	//	gEnv->p3DEngine->SetPostEffectParam("FilterRadialBlurring_Amount", 1.0f);

	//	CPostProcessEffect *pBlur = new CPostProcessEffect(GetEntityId(),"FilterRadialBlurring_Amount", 0.0f);
	//	CLinearBlend *pLinear = new CLinearBlend(1.0f);
	//	pSE->StartBlend(pBlur, pLinear, 1.0f, 98);
	//}

	m_bUnderwater = (m_stats.headUnderWaterTimer > 0.0f);
}

//------------------------------------------------------------------------
void CPlayer::UpdateFirstPersonFists()
{
	if(m_stats.inFreefall)
	{
		return;
	}
	else if (ShouldSwim() || m_bSwimming)
	{
		UpdateFirstPersonSwimming();
		return;
	}

	COffHand *pOffHand = static_cast<COffHand*>(GetItemByClass(CItem::sOffHandClass));
	CFists   *pFists   = static_cast<CFists*>(GetItemByClass(CItem::sFistsClass));

	if(pFists && pFists->IsSelected() && pOffHand && !pOffHand->IsSelected())
	{

		//Retrieve some player info...
		pe_status_dynamics dyn;
		IPhysicalEntity *phys = GetEntity()->GetPhysics();

		// only thing used is dyn.v, which is set to zero by default constructor.
		if (phys)
			phys->GetStatus(&dyn);

		int stance = GetStance();

		//Select fists state...
		if(pFists->GetCurrentAnimState()==CFists::eFAS_JUMPING && (m_stats.landed || (m_stats.onGround>0.0001f && m_stats.inAir<=0.5f)))
			pFists->RequestAnimState(CFists::eFAS_LANDING);

		// dyn.v is zero by default constructor, meaning it's safe to use even if the GetParams call fails.
		if(stance==STANCE_PRONE && GetPlayerInput() && 
			(GetPlayerInput()->GetMoveButtonsState()&CPlayerInput::eMBM_Forward))
			pFists->RequestAnimState(CFists::eFAS_CRAWL);
		else if(stance==STANCE_PRONE)
			pFists->RequestAnimState(CFists::eFAS_RELAXED);
		else if(stance!=STANCE_PRONE && pFists->GetCurrentAnimState()==CFists::eFAS_CRAWL)
			pFists->RequestAnimState(CFists::eFAS_RELAXED);
		

		if(m_stats.bSprinting && stance==STANCE_STAND)
		{
			//m_stats.bSprinting = false;		//Need to do this, or you barely saw the fists on screen...
			pFists->RequestAnimState(CFists::eFAS_RUNNING);
		}
		else if(pFists->GetCurrentAnimState()==CFists::eFAS_RUNNING)
		{
			if(m_stats.jumped)
				pFists->RequestAnimState(CFists::eFAS_JUMPING);
			else
				pFists->RequestAnimState(CFists::eFAS_RELAXED);
		}

	}		
}

bool CPlayer::HasHitAssistance()
{
	return m_bHasAssistance;
}

//------------------------------------------------------------------------
int32 CPlayer::GetArmor() const
{
	if(m_pNanoSuit && m_pNanoSuit->GetMode() == NANOMODE_DEFENSE)
		return int32((m_pNanoSuit->GetSuitEnergy()/NANOSUIT_ENERGY)*g_pGameCVars->g_suitArmorHealthValue);
	return 0;
}

//------------------------------------------------------------------------
int32 CPlayer::GetMaxArmor() const
{
	if(m_pNanoSuit && m_pNanoSuit->GetMode() == NANOMODE_DEFENSE)
		return int32(g_pGameCVars->g_suitArmorHealthValue);
	return int32(0);
}

//-----------------------------------------------------------------------
bool CPlayer::CanFire()
{
	//AI can always
	if(!IsPlayer())
		return true;

	//For the player

	float velSqr = m_stats.velocity.len2();

	//1- Player can not fire while sprinting
	if(m_stats.bSprinting && velSqr>0.2f &&
		GetPlayerInput() && (GetPlayerInput()->GetActions()&ACTION_SPRINT))
		return false;

	//2- Player can not fire while moving in prone
	//if((GetStance()==STANCE_PRONE) && (velSqr>0.03f))
	//{
		//if(GetPlayerInput() && GetPlayerInput()->GetMoveButtonsState()!=0)
 			//return false;
	//}

	//3- Player can not fire while his weapon is underwater level
	//	(doesn't apply in vehicles - CVehicleWeapon::CanFire() takes care of this,
	//	 else it breaks in 3rd person view)
	if (!GetLinkedVehicle() && m_stats.worldWaterLevel > m_stats.FPWeaponPos.z)
		return false;

	return true;
}

//-----------------------------------------------------------------------
bool CPlayer::IsSprinting()
{
	//Only for the player
	return (IsPlayer() && m_stats.bSprinting && (GetPlayerInput() && (GetPlayerInput()->GetMoveButtonsState() || (GetPlayerInput()->GetActions() & ACTION_MOVE))));
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, SvRequestUnfreeze)
{
	if (params.delta>0.0f && params.delta <=1.0f && GetHealth()>0)
	{
		SetFrozenAmount(m_frozenAmount-params.delta);

		if (m_frozenAmount <= 0.0f)
			g_pGame->GetGameRules()->FreezeEntity(GetEntityId(), false, false);

		GetGameObject()->ChangedNetworkState(ASPECT_FROZEN);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, SvRequestHitAssistance)
{
	m_bHasAssistance=params.assistance;
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, ClEMP)
{
	if(CNanoSuit* pSuit = GetNanoSuit())
	{
		pSuit->Activate(false);
		pSuit->Activate(true, params.time);

		if (IsClient())
		{
			if (CHUD* pHUD = g_pGame->GetHUD())
				pHUD->BreakHUD();
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, ClJump)
{
	if (params.strengthJump)
	{
		if (CPlayerMovementController *pPMC=static_cast<CPlayerMovementController *>(GetMovementController()))
			pPMC->StrengthJump(true);
	}

	CMovementRequest request;
	request.SetJump();
	GetMovementController()->RequestMovement(request);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, SvRequestJump)
{
	GetGameObject()->InvokeRMI(ClJump(), params, eRMI_ToOtherClients|eRMI_NoLocalCalls, m_pGameFramework->GetGameChannelId(pNetChannel));
	GetGameObject()->Pulse('bang');

	if (!IsClient())
	{
		if (params.strengthJump)
		{
			if (CPlayerMovementController *pPMC=static_cast<CPlayerMovementController *>(GetMovementController()))
				pPMC->StrengthJump(true);
		}

		CMovementRequest request;
		request.SetJump();
		GetMovementController()->RequestMovement(request);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, SvRequestParachute)
{
	if (!IsClient() && m_parachuteEnabled && (m_stats.inFreefall.Value()==1))
	{
		ChangeParachuteState(3);
	}
	GetGameObject()->InvokeRMI(CPlayer::ClParachute(),CPlayer::NoParams(),eRMI_ToRemoteClients);

	return true;
}

//-------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, ClParachute)
{
	if(!IsClient())
		ChangeParachuteState(3);

	return true;
}

//------------------------------------------------------------------------
void
CPlayer::StagePlayer(bool bStage, SStagingParams* pStagingParams /* = 0 */)
{
	if (IsClient() == false)
		return;

	EStance stance = STANCE_NULL;
	bool bLock = false;
	if (bStage == false)
	{
		m_params.vLimitDir.zero();
		m_params.vLimitRangeH = 0.0f;
		m_params.vLimitRangeV = 0.0f;
	}
	else if (pStagingParams != 0)
	{
		bLock = pStagingParams->bLocked;
		m_params.vLimitDir = pStagingParams->vLimitDir;
		m_params.vLimitRangeH = pStagingParams->vLimitRangeH;
		m_params.vLimitRangeV = pStagingParams->vLimitRangeV;
		stance = pStagingParams->stance;
		if (bLock)
		{
			IPlayerInput* pPlayerInput = GetPlayerInput();
			if(pPlayerInput)
				pPlayerInput->Reset();
		}
	}
	else
	{
		bStage = false;
	}

	if (bLock || m_stagingParams.bLocked)
	{
		g_pGameActions->FilterNoMove()->Enable(bLock);
	}
	m_stagingParams.bActive = bStage;
	m_stagingParams.bLocked = bLock;
	m_stagingParams.vLimitDir = m_params.vLimitDir;
	m_stagingParams.vLimitRangeH = m_params.vLimitRangeH;
	m_stagingParams.vLimitRangeV = m_params.vLimitRangeV;
	m_stagingParams.stance = stance;
	if (stance != STANCE_NULL)
		SetStance(stance);
}

void CPlayer::ResetScreenFX()
{
	if (GetScreenEffects() && IsClient())
	{
		GetScreenEffects()->ClearBlendGroup(14, true);
		//CPostProcessEffect *blend = new CPostProcessEffect(GetEntity()->GetId(), "WaterDroplets_Amount", 0.0f);
		//CWaveBlend *wave = new CWaveBlend();
		//GetScreenEffects()->StartBlend(blend, wave, 20.0f, 14);

		//reset possible death blur
		GetScreenEffects()->ClearBlendGroup(66, true);
		//reset possible alien fear
		GetScreenEffects()->ClearBlendGroup(15, true);

		//gEnv->p3DEngine->SetPostEffectParam("Global_ColorK", 0.0f);

		//gEnv->p3DEngine->SetPostEffectParam("FilterBlurring_Amount", 0.0f);
		SAFE_HUD_FUNC(ShowDeathFX(0));
	}
}

void CPlayer::ResetFPView()
{
	float defaultFov = 60.0f;
	gEnv->pRenderer->EF_Query(EFQ_DrawNearFov,(INT_PTR)&defaultFov);
	g_pGameCVars->i_offset_front = g_pGameCVars->i_offset_right = g_pGameCVars->i_offset_up = 0.0f;
}

void CPlayer::NotifyObjectGrabbed(bool bIsGrab, EntityId objectId, bool bIsNPC, bool bIsTwoHanded /*= false*/)
{
	CALL_PLAYER_EVENT_LISTENERS(OnObjectGrabbed(this, bIsGrab, objectId, bIsNPC, bIsTwoHanded));

	//Set Player stats
	if(bIsGrab)
	{
		if(bIsNPC)
			m_stats.grabbedHeavyEntity = 2;
		else if(bIsTwoHanded)
			m_stats.grabbedHeavyEntity = 1;
	}
	else if(!bIsGrab)
		m_stats.grabbedHeavyEntity = false;
}

void CPlayer::StopLoopingSounds()
{
	//stop sounds
	for(int i = (int)ESound_Player_First+1; i < (int)ESound_Player_Last;++i)
		PlaySound((EPlayerSounds)i, false);
	if(m_pNanoSuit)
	{
		for(int i = (int)NO_SOUND+1; i < (int)ESound_Suit_Last;++i)
			m_pNanoSuit->PlaySound((ENanoSound)i, 0.0f, true);
	}
}


EntityId	CPlayer::GetGrabbedEntityId() const
{
	CWeapon *pWeapon = static_cast<CWeapon*>(GetItemByClass(CItem::sOffHandClass));
	if(pWeapon)	
		return pWeapon->GetHeldEntityId();
	return 0;
}

//----------------------------------------------------------
void CPlayer::SendMusicLogicEvent(EMusicLogicEvents event)
{
	if(IsClient())
		m_pGameFramework->GetMusicLogic()->SetEvent(event);
}

struct RecursionFlagLock
{
	RecursionFlagLock(bool& flag) : m_bFlag(flag) { m_bFlag = true; }
	~RecursionFlagLock() { m_bFlag = false; }
	bool m_bFlag;
};
//----------------------------------------------------------
void CPlayer::OnSoundSystemEvent(ESoundSystemCallbackEvent event,ISound *pSound)
{
	if (m_bVoiceSoundRecursionFlag)
		return;

	RecursionFlagLock recursionFlagLock(m_bVoiceSoundRecursionFlag);

	// called for voice sounds only

	// early returns
	if (event != SOUNDSYSTEM_EVENT_ON_PLAYBACK_STARTED && event != SOUNDSYSTEM_EVENT_ON_PLAYBACK_STOPPED)
		return;

	if (pSound == 0)
		return;

	if (m_pSoundProxy == 0)
		m_pSoundProxy = (IEntitySoundProxy*)(GetEntity()->GetProxy(ENTITY_PROXY_SOUND));

	if (m_pSoundProxy == 0)
		return;

	// is it playing on us?
	ISound* pProxySound = m_pSoundProxy->GetSound(pSound->GetId());
	if (pProxySound != pSound)
		return;

	// some sounds played on the player proxy are not actually player talking, but only played
	// through player's soundproxy for convenience
	// player talking sounds have 'nomad' in their name
	const char* soundName = pSound->GetName();
	if (CryStringUtils::stristr(soundName, "nomad") == 0)
		return;

	if (event == SOUNDSYSTEM_EVENT_ON_PLAYBACK_STARTED)
	{
		// not only loop over known sounds but all sounds on soundproxy
		// stop all player foley sounds and supress further playing
		for(int i = 0; i < ESound_Player_Last; ++i)
		{
			tSoundID soundId = m_sounds[i];
			ISound* pFoleySound = m_pSoundProxy->GetSound(soundId);
			if (pFoleySound && pFoleySound->GetSemantic() == eSoundSemantic_Player_Foley_Voice) 
			{
				pFoleySound->Stop();
				m_sounds[i] = 0;
			}
		}

		/* below's code is dangerous, leave commented out until Tomas is back
		   because we can't know if sound proxy actually stops the sound
			 if so, we would NOT have to increase the index, because the sound got removed
			 but currently there is no way of knowing wether the sound got removed or not
		*/
#if 0
		int i = 0;
		while (pProxySound = m_pSoundProxy->GetSoundByIndex(i))
		{
			if (pProxySound->GetSemantic() == eSoundSemantic_Player_Foley_Voice)
				pProxySound->Stop();
			++i;
		}
#endif

		m_bVoiceSoundPlaying = true;
	}
	else
	{
		m_bVoiceSoundPlaying = false;
	}
}

//--------------------------------------------------------
void CPlayer::AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event)
{
	CActor::AnimationEvent(pCharacter,event);

	//"sound_tp" are sounds that has to be only triggered in thirdperson, never for client
	//Client is supposed to have its own FP sound
	if(IsThirdPerson())
	{
		const char* eventName = event.m_EventName;
		if (gEnv->pSoundSystem && eventName && (stricmp(eventName, "sound_tp") == 0))
		{
			Vec3 offset(0.0f,0.0f,1.0f);
			if (event.m_BonePathName && event.m_BonePathName[0])
			{
				ISkeletonPose* pSkeletonPose = (pCharacter ? pCharacter->GetISkeletonPose() : 0);

				int id = (pSkeletonPose ? pSkeletonPose->GetJointIDByName(event.m_BonePathName) : -1);
				if (pSkeletonPose && id >= 0)
				{
					QuatT boneQuat(pSkeletonPose->GetAbsJointByID(id));
					offset = boneQuat.t;
				}
			}

			int flags = FLAG_SOUND_DEFAULT_3D;
			if (strchr(event.m_CustomParameter, ':') == NULL)
				flags |= FLAG_SOUND_VOICE;

			if(IEntitySoundProxy* pSoundProxy = static_cast<IEntitySoundProxy*>(GetEntity()->GetProxy(ENTITY_PROXY_SOUND)))
				pSoundProxy->PlaySound(event.m_CustomParameter, offset, FORWARD_DIRECTION, flags, eSoundSemantic_Animation, 0, 0);
		}
	}

}

void CPlayer::SetPainEffect(float progress /* = 0.0f */)
{
	if(!IsClient())
		return;

	I3DEngine *pEngine = gEnv->p3DEngine;

	if(progress < 0.0f)
	{
		pEngine->SetPostEffectParam("Global_User_ColorC", 0.0f);
		pEngine->SetPostEffectParam("FilterRadialBlurring_Amount", 0.0f);
		pEngine->SetPostEffectParam("ScreenCondensation_Amount", 0.0f);
	}
	else
	{
		float now = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		float delta = now - m_merciTimeLastHit;

		if(m_merciTimeLastHit != 0 && delta <= 1.0f)		//hit spike
		{
			//if(delta < 0.25f)
			//	progress = progress + (delta * 2.0f);
			//else
				progress = progress + 0.5f - delta*0.5;
		}

		pEngine->SetPostEffectParam("Global_User_ColorC", -0.4f * progress);

		pEngine->SetPostEffectParam("FilterRadialBlurring_Amount", progress);
		pEngine->SetPostEffectParam("FilterRadialBlurring_ScreenPosX", 0.5f);
		pEngine->SetPostEffectParam("FilterRadialBlurring_ScreenPosY", 0.5f);
		pEngine->SetPostEffectParam("FilterRadialBlurring_Radius", 0.6f);

		pEngine->SetPostEffectParam("ScreenCondensation_Amount", progress * 0.5f);
		pEngine->SetPostEffectParam("ScreenCondensation_CenterAmount", 1.0f);
	}

}