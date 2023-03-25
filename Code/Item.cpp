/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 27:10:2004   11:26 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "ItemSharedParams.h"
#include "Game.h"
#include "GameActions.h"
#include "IGameObject.h"
#include "ISerialize.h"
#include <IEntitySystem.h>

#include "Actor.h"
#include "Player.h"
#include "IActorSystem.h"
#include "IItemSystem.h"
#include "IActionMapManager.h"
#include "ScriptBind_Item.h"
#include "GameRules.h"
#include "HUD/HUD.h"
#include "HUD/HUDCrosshair.h"
#include "GameCVars.h"
#include "Binocular.h"
#include "OffHand.h"
#include "WeaponAttachmentManager.h"


#pragma warning(disable: 4355)	// ´this´ used in base member initializer list

#if defined(USER_alexll)
#define ITEM_DEBUG_MEMALLOC
#endif

#ifdef ITEM_DEBUG_MEMALLOC
int gInstanceCount = 0;
#endif


IEntitySystem *CItem::m_pEntitySystem=0;
IItemSystem *CItem::m_pItemSystem=0;
IGameFramework *CItem::m_pGameFramework=0;
IGameplayRecorder*CItem::m_pGameplayRecorder=0;

IEntityClass* CItem::sOffHandClass = 0;
IEntityClass* CItem::sFistsClass = 0;
IEntityClass* CItem::sAlienCloak = 0;
IEntityClass* CItem::sSOCOMClass = 0;
IEntityClass* CItem::sDetonatorClass = 0;
IEntityClass* CItem::sC4Class = 0;
IEntityClass* CItem::sBinocularsClass = 0;
IEntityClass* CItem::sGaussRifleClass = 0;
IEntityClass* CItem::sDebugGunClass = 0;
IEntityClass* CItem::sRefWeaponClass = 0;
IEntityClass* CItem::sClaymoreExplosiveClass = 0;
IEntityClass* CItem::sAVExplosiveClass = 0;
IEntityClass* CItem::sDSG1Class = 0;
IEntityClass* CItem::sLAMFlashLight = 0;
IEntityClass* CItem::sLAMRifleFlashLight = 0;
IEntityClass* CItem::sTACGunClass = 0;
IEntityClass* CItem::sTACGunFleetClass = 0;
IEntityClass* CItem::sAlienMountClass = 0;
IEntityClass* CItem::sRocketLauncherClass = 0;

IEntityClass* CItem::sFlashbangGrenade = 0;
IEntityClass* CItem::sExplosiveGrenade = 0;
IEntityClass* CItem::sEMPGrenade = 0;
IEntityClass* CItem::sSmokeGrenade = 0;

IEntityClass* CItem::sIncendiaryAmmo = 0;

IEntityClass*	CItem::sScarGrenadeClass = 0;

//------------------------------------------------------------------------
CItem::CItem()
: m_scheduler(this),	// just to store the pointer.
	m_dualWieldMasterId(0),
	m_dualWieldSlaveId(0),
	m_ownerId(0),
	m_postSerializeMountedOwner(0),
	m_parentId(0),
	m_effectGenId(0),
	m_pForcedArms(0),
  m_hostId(0),
	m_pEntityScript(0),
	m_modifying(false),
	m_transitioning(false),
	m_frozen(false),
	m_cloaked(false),
	m_serializeCloaked(false),
	m_enableAnimations(true),
	m_noDrop(false),
	m_constraintId(0),
	m_useFPCamSpacePP(true),
	m_serializeActivePhysics(0),
	m_serializeDestroyed(false),
	m_bPostPostSerialize(false)
{
#ifdef ITEM_DEBUG_MEMALLOC
	++gInstanceCount;
#endif
	memset(m_animationTime, 0, sizeof(m_animationTime));
	memset(m_animationEnd, 0, sizeof(m_animationTime));
	memset(m_animationSpeed, 0, sizeof(m_animationSpeed));
}

//------------------------------------------------------------------------
CItem::~CItem()
{
	AttachArms(false, false);
	AttachToBack(false);

	//Auto-detach from the parent
	if(m_parentId)
	{
		CItem *pParent= static_cast<CItem*>(m_pItemSystem->GetItem(m_parentId));
		if(pParent)
		{
			//When destroyed the item parent should not be busy
			pParent->SetBusy(false);

			for (TAccessoryMap::iterator it=pParent->m_accessories.begin(); it!=pParent->m_accessories.end(); ++it)
			{
				if(GetEntityId()==it->second)
				{
					pParent->AttachAccessory(it->first.c_str(),false,true);
					break;
				}
			}
		}
	}

	GetGameObject()->ReleaseProfileManager(this);

	if (GetOwnerActor() && GetOwnerActor()->GetInventory())
		GetOwnerActor()->GetInventory()->RemoveItem(GetEntityId());

	if(!(GetISystem()->IsSerializingFile() && GetGameObject()->IsJustExchanging()))
		for (TAccessoryMap::iterator it=m_accessories.begin(); it!=m_accessories.end(); ++it)
			gEnv->pEntitySystem->RemoveEntity(it->second);

	if(m_pItemSystem)
		m_pItemSystem->RemoveItem(GetEntityId());

	 Quiet();

#ifdef ITEM_DEBUG_MEMALLOC
	 --gInstanceCount;
#endif
}

//------------------------------------------------------------------------
bool CItem::Init( IGameObject *pGameObject )
{
	SetGameObject(pGameObject);

#ifdef ITEM_DEBUG_MEMALLOC
	CGame::DumpMemInfo("CItem::Init Instance=%d %p Id=%d Class=%s", gInstanceCount, GetGameObject(), GetEntityId(), gEnv->pEntitySystem->GetEntity(GetEntityId())->GetClass()->GetName());
#endif

	m_pEntityScript = GetEntity()->GetScriptTable();

	if (!m_pGameFramework)
	{
		m_pEntitySystem = gEnv->pEntitySystem;
		m_pGameFramework= gEnv->pGame->GetIGameFramework();
		m_pGameplayRecorder = m_pGameFramework->GetIGameplayRecorder();
		m_pItemSystem = m_pGameFramework->GetIItemSystem();
		sOffHandClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("OffHand");
		sFistsClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Fists");
		sAlienCloak = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AlienCloak");
		sSOCOMClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("SOCOM");
		sDetonatorClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Detonator");
		sC4Class = gEnv->pEntitySystem->GetClassRegistry()->FindClass("C4");
		sBinocularsClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Binoculars");
		sGaussRifleClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("GaussRifle");
		sDebugGunClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DebugGun");
		sRefWeaponClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("RefWeapon");
		sClaymoreExplosiveClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("claymoreexplosive");
		sAVExplosiveClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("avexplosive");
		sDSG1Class = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DSG1");
		sLAMFlashLight			= gEnv->pEntitySystem->GetClassRegistry()->FindClass("LAMFlashLight");
		sLAMRifleFlashLight	= gEnv->pEntitySystem->GetClassRegistry()->FindClass("LAMRifleFlashLight");
		sTACGunClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("TACGun");
		sTACGunFleetClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("TACGun_Fleet");
		sAlienMountClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AlienMount");
		sRocketLauncherClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("LAW");

		sFlashbangGrenade = gEnv->pEntitySystem->GetClassRegistry()->FindClass("flashbang");
		sEMPGrenade       = gEnv->pEntitySystem->GetClassRegistry()->FindClass("empgrenade");
		sSmokeGrenade     = gEnv->pEntitySystem->GetClassRegistry()->FindClass("smokegrenade");
		sExplosiveGrenade = gEnv->pEntitySystem->GetClassRegistry()->FindClass("explosivegrenade");

		sIncendiaryAmmo   = gEnv->pEntitySystem->GetClassRegistry()->FindClass("incendiarybullet");

		sScarGrenadeClass   = gEnv->pEntitySystem->GetClassRegistry()->FindClass("scargrenade");
	}

	if (!GetGameObject()->CaptureProfileManager(this))
		return false;

	// bind to network
	if (0 == (GetEntity()->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY)))
	{
		if (!GetGameObject()->BindToNetwork())
		{
			GetGameObject()->ReleaseProfileManager(this);
			return false;
		}
	}

	// register with item system
	m_pItemSystem->AddItem(GetEntityId(), this);
	m_pItemSystem->CacheItemGeometry(GetEntity()->GetClass()->GetName());

	// attach script bind
	g_pGame->GetItemScriptBind()->AttachTo(this);

	m_sharedparams=g_pGame->GetItemSharedParamsList()->GetSharedParams(GetEntity()->GetClass()->GetName(), true);

	m_noDrop = false;

	if (GetEntity()->GetScriptTable())
	{
		SmartScriptTable props;
		GetEntity()->GetScriptTable()->GetValue("Properties", props);
		ReadProperties(props);
	}

#ifdef ITEM_DEBUG_MEMALLOC
	CGame::DumpMemInfo("CItem::Init End %p Id=%d Class=%s", GetGameObject(), GetEntityId(), gEnv->pEntitySystem->GetEntity(GetEntityId())->GetClass()->GetName());
#endif

	if(!IsMounted())
		GetEntity()->SetFlags(GetEntity()->GetFlags()|ENTITY_FLAG_ON_RADAR);

	return true;
}

//------------------------------------------------------------------------
void CItem::Reset()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

#ifdef ITEM_DEBUG_MEMALLOC
	CGame::DumpMemInfo("CItem::Reset Start %p Id=%d Class=%s", GetGameObject(), GetEntityId(), gEnv->pEntitySystem->GetEntity(GetEntityId())->GetClass()->GetName());
#endif

	if (IsModifying())
		ResetAccessoriesScreen(GetOwnerActor());

	ResetOwner();  
  m_scheduler.Reset();
  
	m_params=SParams();
//	m_mountparams=SMountParams();
	m_enableAnimations = true;
	// detach any effects
	TEffectInfoMap temp = m_effects;
	TEffectInfoMap::iterator end = temp.end();
	for (TEffectInfoMap::iterator it=temp.begin(); it!=end;++it)
		AttachEffect(it->second.slot, it->first, false);
	m_effectGenId=0;

#ifdef ITEM_DEBUG_MEMALLOC
	CGame::DumpMemInfo("    CItem::Read ItemParams Start %p Id=%d Class=%s", GetGameObject(), GetEntityId(), gEnv->pEntitySystem->GetEntity(GetEntityId())->GetClass()->GetName());
#endif

	// read params
	m_sharedparams=0; // decrease refcount to force a deletion of old parameters in case we are reloading item scripts
	m_sharedparams=g_pGame->GetItemSharedParamsList()->GetSharedParams(GetEntity()->GetClass()->GetName(), true);
	const IItemParamsNode *root = m_pItemSystem->GetItemParams(GetEntity()->GetClass()->GetName());
	ReadItemParams(root);

#ifdef ITEM_DEBUG_MEMALLOC
	CGame::DumpMemInfo("    CItem::Read ItemParams End %p Id=%d Class=%s", GetGameObject(), GetEntityId(), gEnv->pEntitySystem->GetEntity(GetEntityId())->GetClass()->GetName());
#endif

	m_stateTable[0] = GetEntity()->GetScriptTable();
	if (!!m_stateTable[0])
	{
		m_stateTable[0]->GetValue("Server", m_stateTable[1]);
		m_stateTable[0]->GetValue("Client", m_stateTable[2]);
	}

	Quiet();

	ReAttachAccessories();
	AccessoriesChanged();

	for (TAccessoryMap::iterator it=m_accessories.begin(); it!=m_accessories.end(); ++it)
		FixAccessories(GetAccessoryParams(it->first), true);

	InitRespawn();

	Cloak(false);

	m_noDrop = false;

	if(m_params.has_first_select)
		m_stats.first_selection = true; //Reset (just in case)

	OnReset();

#ifdef ITEM_DEBUG_MEMALLOC
	CGame::DumpMemInfo("  CItem::Reset End %p Id=%d Class=%s", GetGameObject(), GetEntityId(), gEnv->pEntitySystem->GetEntity(GetEntityId())->GetClass()->GetName());
	CryLogAlways(" ");
#endif

}

//------------------------------------------------------------------------
void CItem::ResetOwner()
{
  if (m_ownerId)
  {
    if (m_stats.used)
      StopUse(m_ownerId);

    CActor *pOwner=GetOwnerActor();

    if (!pOwner || pOwner->GetInventory()->FindItem(GetEntityId())<0)
      SetOwnerId(0);
  }
}

//------------------------------------------------------------------------
void CItem::PostInit( IGameObject * pGameObject )
{
	// prevent ai from automatically disabling weapons
	for (int i=0; i<4;i++)
		pGameObject->SetUpdateSlotEnableCondition(this, i, eUEC_WithoutAI);

	Reset();
	//InitialSetup();
	PatchInitialSetup();	
	InitialSetup();		//Must be called after Patch
}

//------------------------------------------------------------------------
void CItem::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CItem::Update( SEntityUpdateContext& ctx, int slot )
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if(m_bPostPostSerialize)
	{
		PostPostSerialize();
		m_bPostPostSerialize = false;
	}

	if (m_frozen || IsDestroyed())
		return;

	switch (slot)
	{
	case eIUS_Scheduler:
		m_scheduler.Update(ctx.fFrameTime);
		break;
	}

	// update mounted
	if (slot==eIUS_General)
	{
		if (m_stats.mounted)
  		UpdateMounted(ctx.fFrameTime);
	}
}

//------------------------------------------------------------------------
bool CItem::SetAspectProfile( EEntityAspects aspect, uint8 profile )
{
	//CryLogAlways("%s::SetProfile(%d: %s)", GetEntity()->GetName(), profile, profile==eIPhys_Physicalized?"Physicalized":"NotPhysicalized");

	if (aspect == eEA_Physics)
	{
		switch (profile)
		{
		case eIPhys_PhysicalizedStatic:
			{
				SEntityPhysicalizeParams params;
				params.type = PE_STATIC;
				params.nSlot = eIGS_ThirdPerson;

				GetEntity()->Physicalize(params);

				return true;
			}
			break;
		case eIPhys_PhysicalizedRigid:
			{
				SEntityPhysicalizeParams params;
				params.type = PE_RIGID;
				params.nSlot = eIGS_ThirdPerson;
				params.mass = m_params.mass;

				pe_params_buoyancy buoyancy;
				buoyancy.waterDamping = 1.5;
				buoyancy.waterResistance = 1000;
				buoyancy.waterDensity = 1;
				params.pBuoyancy = &buoyancy;

				GetEntity()->Physicalize(params);

				IPhysicalEntity *pPhysics = GetEntity()->GetPhysics();
				if (pPhysics)
				{
					pe_action_awake action;
					action.bAwake = m_ownerId!=0;
					pPhysics->Action(&action);
				}
			}
			return true;
		case eIPhys_NotPhysicalized:
			{
				IEntityPhysicalProxy *pPhysicsProxy = GetPhysicalProxy();
				if (pPhysicsProxy)
				{
					SEntityPhysicalizeParams params;
					params.type = PE_NONE;
					params.nSlot = eIGS_ThirdPerson;
					pPhysicsProxy->Physicalize(params);
				}
			}
			return true;
		}
	}

	return false;
}

uint8 CItem::GetDefaultProfile( EEntityAspects aspect )
{
	if (aspect == eEA_Physics)
	{
		if (m_properties.pickable)
		{
			if (m_properties.physics)
				return eIPhys_PhysicalizedRigid;
			return eIPhys_PhysicalizedStatic;
		}
		return eIPhys_NotPhysicalized;
	}
	else
	{
		return 0;
	}
}

//------------------------------------------------------------------------
void CItem::HandleEvent( const SGameObjectEvent &evt )
{
	if (evt.event == eCGE_PostFreeze)
	{
		Freeze(evt.param != 0);

		// re-apply mounted weapon view limits after unfreezing, since the freezing will disrupt them...
		if ((evt.param == 0) && IsMounted() && GetOwnerId())
			ApplyViewLimit(GetOwnerId(), true);
	}
	else if (evt.event == eCGE_PostShatter)
		m_stats.health = 0.0f;
}

//------------------------------------------------------------------------
void CItem::ProcessEvent(SEntityEvent &event)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	switch (event.event)
	{
	case ENTITY_EVENT_TIMER:
		{
			switch (event.nParam[0])
			{
			case eIT_Flying:
				m_stats.flying = false;
				if (IsServer())
					IgnoreCollision(false);

				//Add an small impulse, sometimes item keeps floating in the air
				IEntityPhysicalProxy *pPhysics = GetPhysicalProxy();
				if (pPhysics)
					pPhysics->AddImpulse(-1, Vec3(0.0f,0.0f,0.0f), Vec3(0.0f,0.0f,-1.0f)*m_params.drop_impulse, false, 1.0f);
				break;
			}
      break;
		}
	case ENTITY_EVENT_PICKUP:
		{
			if (event.nParam[0] == 1 && GetIWeapon())
			{
				// check if the entity picking up the item is cloaked
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(event.nParam[1]);

				if(!pEntity)
					return;

				IEntityRenderProxy * pOwnerRP = (IEntityRenderProxy*) pEntity->GetProxy(ENTITY_PROXY_RENDER);
				IEntityRenderProxy * pItemRP = (IEntityRenderProxy*) GetEntity()->GetProxy(ENTITY_PROXY_RENDER);

				if (pItemRP && pOwnerRP)
				{ 
					pItemRP->SetMaterialLayersMask( pOwnerRP->GetMaterialLayersMask() );
					pItemRP->SetMaterialLayersBlend( pOwnerRP->GetMaterialLayersBlend() );
				}

				for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); it++)
				{
					EntityId cur = (EntityId)it->second;
					IItem *attachment = m_pItemSystem->GetItem(cur);
					if (attachment)
					{
						pItemRP = (IEntityRenderProxy*) attachment->GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
						if (pItemRP && pOwnerRP)
						{
							pItemRP->SetMaterialLayersMask( pOwnerRP->GetMaterialLayersMask() );
							pItemRP->SetMaterialLayersBlend( pOwnerRP->GetMaterialLayersBlend() );
						}
					}
				}
			}
		}
		break;
  case ENTITY_EVENT_RESET:

		Reset();

		if (gEnv->pSystem->IsEditor() && !m_stats.mounted)
		{
			IInventory *pInventory=GetActorInventory(GetOwnerActor());

			if (event.nParam[0]) // entering game mode in editor
				m_editorstats=SEditorStats(GetOwnerId(), pInventory?pInventory->GetCurrentItem()==GetEntityId():0);
			else // leaving game mode
			{

				if (m_editorstats.ownerId)
				{
					m_noDrop=true;
					if(IsDualWieldMaster())
						ResetDualWield();

					AttachToBack(false);

					int iValue = gEnv->pConsole->GetCVar("i_noweaponlimit")->GetIVal();
					int iFlags = gEnv->pConsole->GetCVar("i_noweaponlimit")->GetFlags();
					gEnv->pConsole->GetCVar("i_noweaponlimit")->SetFlags(iFlags|VF_NOT_NET_SYNCED);
					gEnv->pConsole->GetCVar("i_noweaponlimit")->Set(1);

					PickUp(m_editorstats.ownerId, false, false, false);

					gEnv->pConsole->GetCVar("i_noweaponlimit")->Set(iValue);
					gEnv->pConsole->GetCVar("i_noweaponlimit")->SetFlags(iFlags|~VF_NOT_NET_SYNCED);

					IItemSystem *pItemSystem=g_pGame->GetIGameFramework()->GetIItemSystem();

					if (m_editorstats.current && pInventory && pInventory->GetCurrentItem()==GetEntityId())
					{
						//if (pInventory)
						pInventory->SetCurrentItem(0);
						pItemSystem->SetActorItem(GetActor(m_editorstats.ownerId), GetEntityId(), false);
					}
					else if (pInventory && pInventory->GetCurrentItem()==GetEntityId())
						pItemSystem->SetActorItem(GetActor(m_editorstats.ownerId), (EntityId)0, false);

				}
				else
				{
					if(GetIWeapon() && !GetParentId())
						Drop(0,false,false);

					SetOwnerId(0);

					RemoveAllAccessories();
					PatchInitialSetup();
					InitialSetup();
				}
			}
		}
    break;
	}
}

//------------------------------------------------------------------------
bool CItem::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags )
{
	if (aspect == eEA_Physics)
	{
		pe_type type = PE_NONE;
		switch (profile)
		{
		case eIPhys_PhysicalizedStatic:
			type = PE_STATIC;
			break;
		case eIPhys_PhysicalizedRigid:
			type = PE_RIGID;
			break;
		case eIPhys_NotPhysicalized:
			type = PE_NONE;
			break;
		default:
			return false;
		}

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

//------------------------------------------------------------------------
void CItem::FullSerialize( TSerialize ser )
{
	assert(ser.GetSerializationTarget() != eST_Network);
	if(ser.IsReading())
	{
		AttachToBack(false);
		m_actionSuffix.clear();
	}

	m_stats.Serialize(ser);

	EntityId ownerId = m_ownerId;
	ser.Value("ownerId", ownerId);
	ser.Value("parentId", m_parentId);
	ser.Value("hostId", m_hostId);
	ser.Value("masterID", m_dualWieldMasterId);
	ser.Value("slaveID", m_dualWieldSlaveId);
	m_serializeCloaked = m_cloaked;
	ser.Value("m_cloaked", m_serializeCloaked);
	m_serializeActivePhysics = Vec3(0,0,0);
	m_serializeDestroyed = IsDestroyed();
	m_serializeRigidPhysics = true;
	ser.Value("m_serializeDestroyed", m_serializeDestroyed);

	if(ser.IsWriting())
	{
		if(IPhysicalEntity *pPhys = GetEntity()->GetPhysics())
		{
			m_serializeRigidPhysics = (pPhys->GetType()==PE_RIGID);
			pe_status_dynamics dyn;
			if (pPhys->GetStatus(&dyn))
			{
				if(dyn.v.len() > 0.1f)
					m_serializeActivePhysics = dyn.v.GetNormalized();
			}
		}
	}

	ser.Value("m_serializeActivePhysics", m_serializeActivePhysics);
	ser.Value("m_serializeRigidPhysics", m_serializeRigidPhysics);

	m_actionSuffixSerializationHelper = m_actionSuffix;
	ser.Value("actionSuffix", m_actionSuffixSerializationHelper);

	if (ser.IsReading() && m_stats.mounted && m_params.usable)
	{
		if(m_ownerId)
		{
			IActor *pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_ownerId);
			CPlayer *pPlayer = static_cast<CPlayer*> (pActor);
			if(pPlayer)
			{
				if(m_stats.used && ownerId == m_ownerId)
					pPlayer->UseItem(GetEntityId());
				else
					StopUse(m_ownerId);
			}
			m_stats.used = false;
		}

		m_postSerializeMountedOwner = ownerId;
	}
	else
		m_ownerId = ownerId;

	//serialize attachments
	int attachmentAmount = m_accessories.size();
	ser.Value("attachmentAmount", attachmentAmount);
	if(ser.IsWriting())
	{
		TAccessoryMap::iterator it = m_accessories.begin();
		for(; it != m_accessories.end(); ++it)
		{
			string name((it->first).c_str());
			EntityId id = it->second;
			ser.BeginGroup("Accessory");
			ser.Value("Name", name);
			ser.Value("Id", id);
			ser.EndGroup();
		}
	}
	else if(ser.IsReading())
	{
		m_accessories.clear();
		string name;
		for(int i = 0; i < attachmentAmount; ++i)
		{
			EntityId id = 0;
			ser.BeginGroup("Accessory");
			ser.Value("Name", name);
			ser.Value("Id", id);
#ifndef ITEM_USE_SHAREDSTRING
			m_accessories[name] = id;
#else
			m_accessories[ItemString(name)] = id;
#endif
			ser.EndGroup();
		}
	}

	if(ser.IsReading())
	{
		SetViewMode(m_stats.viewmode);

		//Back attachments
		if(m_stats.backAttachment!=eIBA_Unknown)
		{
			AttachToBack(true);
		}
	}

	//Extra ammo given by some accessories
	{
		ser.BeginGroup("AccessoryAmmo");
		if(ser.IsReading())
			m_bonusAccessoryAmmo.clear();
		TAccessoryAmmoMap::iterator it = m_bonusAccessoryAmmo.begin();
		int ammoTypeAmount = m_bonusAccessoryAmmo.size();
		ser.Value("AccessoryAmmoAmount", ammoTypeAmount);
		for(int i = 0; i < ammoTypeAmount; ++i, ++it)
		{
			string name;
			int amount = 0;
			if(ser.IsWriting())
			{
				name = it->first->GetName();
				amount = it->second;
			}
			ser.BeginGroup("Ammo");
			ser.Value("AmmoName", name);
			ser.Value("Bullets", amount);
			ser.EndGroup();
			if(ser.IsReading())
			{
				IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
				assert(pClass);
				if(pClass)
					m_bonusAccessoryAmmo[pClass] = amount;
			}
		}
		ser.EndGroup();
	}
}

//------------------------------------------------------------------------
void CItem::PostSerialize()
{
	CActor* pOwner = GetOwnerActor();

	if(m_ownerId)
	{
		EntityId owner = m_ownerId;
		//Select(m_stats.selected);
		if(pOwner)
		{

			//if(pOwner->GetActorClass() == CPlayer::GetActorClassType())
			//{
				if (!m_stats.mounted && !IsDualWield())
				{
					EntityId holstered = pOwner->GetInventory()->GetHolsteredItem();

					if(GetEntity()->GetClass() != CItem::sBinocularsClass &&
						GetEntity()->GetClass() != CItem::sOffHandClass)
					{			  
						if(m_stats.selected)
						{
							m_bPostPostSerialize = true;

							if(holstered != GetEntityId())
							{
								Drop(0,false,false);
								PickUp(owner, false, true, false);
								if(pOwner->GetEntity()->IsHidden())
									Hide(true); //Some AI is hidden in the levels by designers
							}      
						}
						else
						{ 
							if(holstered != GetEntityId())
								AttachToHand(false, true);
						}
					}
				}
				else if (IsDualWield())
				{
					m_bPostPostSerialize = true;
				}
			//}
		}
		else
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Item %s has ownerId %i but owner actor does not exist!", GetEntity()->GetName(), m_ownerId);
	}

	if (m_stats.mounted && !m_hostId)
	{
		Vec3 olddir=m_stats.mount_dir;
		Vec3 oldaimdir=m_stats.mount_last_aimdir;
		MountAt(GetEntity()->GetWorldPos());

		m_stats.mount_dir=olddir;
		m_stats.mount_last_aimdir=oldaimdir;
	}

	ReAttachAccessories();
	AccessoriesChanged();

	//this fix breaks holding NPC serialization, when "use" was pressed during saving
	/*if(pOwner && this == pOwner->GetCurrentItem() && !pOwner->GetLinkedVehicle())
	{
		pOwner->HolsterItem(true);	//this "fixes" old attachments that are not replaced still showing up in the model ..
		pOwner->HolsterItem(false);
	}*/

	//Fix incorrect view mode (in same cases) and not physicalized item after dropping/picking (in same cases too)
	if(!pOwner && m_stats.dropped)
	{
		SetViewMode(eIVM_ThirdPerson);
		
		Pickalize(true, false);
		GetEntity()->EnablePhysics(true);

		Physicalize(true, m_serializeRigidPhysics);

		if(m_serializeActivePhysics.len())	//this fixes objects being frozen in air because they were rephysicalized
		{
			IEntityPhysicalProxy *pPhysics = GetPhysicalProxy();
			if (pPhysics)
				pPhysics->AddImpulse(-1, m_params.drop_impulse_pos, m_serializeActivePhysics*m_params.drop_impulse, true, 1.0f);
			m_serializeActivePhysics = Vec3(0,0,0);
		}

	}

	m_actionSuffix = m_actionSuffixSerializationHelper;
	if(pOwner && pOwner->IsPlayer() && IsSelected() && !stricmp(m_actionSuffixSerializationHelper.c_str(), "akimbo_"))
	{
		PlayAction(g_pItemStrings->idle);
	}
	else
		m_actionSuffix.clear();
	m_actionSuffixSerializationHelper.clear();

	if(m_postSerializeMountedOwner)
	{
		IActor *pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_postSerializeMountedOwner);
		CPlayer *pPlayer = static_cast<CPlayer*> (pActor);
		if(pPlayer && m_params.usable)
		{
			m_stats.used = false;
			m_ownerId = m_postSerializeMountedOwner;
			StopUse(pPlayer->GetEntityId());
			pPlayer->UseItem(GetEntityId());
			assert(m_ownerId);
		}
		m_postSerializeMountedOwner = 0;		
	}

	if(m_serializeCloaked)
		CloakSync(false);
	else
		CloakEnable(false, false);

	if(m_serializeDestroyed)
		OnDestroyed();
}

//------------------------------------------------------------------------
void CItem::PostPostSerialize()
{
	if(IsSelected() && GetOwnerId() == LOCAL_PLAYER_ENTITY_ID)
	{
		AttachArms(true, m_stats.fp);
		if(IsDualWield())
		{
			for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); it++)
			{
				EntityId cur = (EntityId)it->second;
				IItem *attachment = m_pItemSystem->GetItem(cur);
				if (attachment)
					attachment->OnParentSelect(true);
			}
		}
	}
}

//------------------------------------------------------------------------
void CItem::SerializeLTL( TSerialize ser )
{
	ser.BeginGroup("ItemLTLSerialization");

	if(ser.IsReading()) //dual wield is handled by the inventory LTL serialization
	{
		SetDualWieldMaster(0);
		SetDualWieldSlave(0);
	}

	//serialize attachments
	int attachmentAmount = m_accessories.size();
	ser.Value("attachmentAmount", attachmentAmount);
	if(ser.IsWriting())
	{
		TAccessoryMap::iterator it = m_accessories.begin();
		for(; it != m_accessories.end(); ++it)
		{
			string name((it->first.c_str()));
			ser.BeginGroup("Accessory");
			ser.Value("Name", name);
			ser.EndGroup();
		}
	}
	else if(ser.IsReading())
	{
		while(m_accessories.size() > 0)
		{
			RemoveAccessory((m_accessories.begin())->first);
		}
		assert(m_accessories.size() == 0);

		string name;
		CActor *pActor = GetOwnerActor();
		for(int i = 0; i < attachmentAmount; ++i)
		{
			ser.BeginGroup("Accessory");
			ser.Value("Name", name);
			ser.EndGroup();

			if(pActor)
				AddAccessory(name.c_str());
		}
		if(attachmentAmount)
		{
			ReAttachAccessories();
			AccessoriesChanged();

			if(!IsSelected())
			{
				Hide(true);
				TAccessoryMap::const_iterator it=m_accessories.begin();
				TAccessoryMap::const_iterator end=m_accessories.end();
				for(;it!=end;++it)
				{
					if(CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(it->second)))
						pItem->Hide(true);
				}
			}
		}

		/*
		if(GetEntity()->GetClass() == CItem::sOffHandClass)
		{
			COffHand *pOffHand = static_cast<COffHand*>(this);
			pOffHand->PostSerReset();
		}
		*/
	}

	ser.EndGroup();
}

//------------------------------------------------------------------------
void CItem::SetOwnerId(EntityId ownerId)
{
	m_ownerId = ownerId;

	GetGameObject()->ChangedNetworkState(ASPECT_OWNER_ID);
}

//------------------------------------------------------------------------
EntityId CItem::GetOwnerId() const
{
	return m_ownerId;
}

//------------------------------------------------------------------------
void CItem::SetParentId(EntityId parentId)
{
	m_parentId = parentId;

	GetGameObject()->ChangedNetworkState(ASPECT_OWNER_ID);
}

//------------------------------------------------------------------------
EntityId CItem::GetParentId() const
{
	return m_parentId;
}

//------------------------------------------------------------------------
void CItem::SetHand(int hand)
{
	m_stats.hand = hand;

	int idx = 0;
	if (hand == eIH_Right)
		idx = 1;
	else if (hand == eIH_Left)
		idx = 2;

	if (m_fpgeometry[idx].name.empty())
		idx = 0;

	bool result=false;
	bool ok=true;

	CItem *pParent=m_parentId?static_cast<CItem*>(m_pItemSystem->GetItem(m_parentId)):NULL;
	if (pParent)
			ok = pParent->IsSelected();

	if (m_stats.mounted || (pParent && pParent->GetStats().mounted))
		ok=true;

	if (m_stats.viewmode&eIVM_FirstPerson && ok)
	{
		SGeometry &geometry = m_fpgeometry[idx];
		result=SetGeometry(eIGS_FirstPerson, geometry.name, geometry.position, geometry.angles, geometry.scale);
	}

	if (idx == 0)
	{
		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(eIGS_FirstPerson);
		if (!pCharacter)
			return;
		
	/*	if (hand == eIH_Left)
			pCharacter->SetScale(Vec3(-1,1,1));
		else
			pCharacter->SetScale(Vec3(1,1,1));
			*/
	}

	if (result)
		PlayAction(m_idleAnimation[eIGS_FirstPerson], 0, true, (eIPAF_Default|eIPAF_NoBlend)&~eIPAF_Owner);
}

//------------------------------------------------------------------------
void CItem::Use(EntityId userId)
{
	if (m_params.usable && m_stats.mounted)
	{
		if (!m_ownerId)
			StartUse(userId);
		else if (m_ownerId == userId)
			StopUse(userId);
	}
}

//------------------------------------------------------------------------
struct CItem::SelectAction
{
	void execute(CItem *_item)
	{
		_item->SetBusy(false);
		_item->ForcePendingActions();
		if(_item->m_stats.first_selection)
			_item->m_stats.first_selection = false;
	}
};

void CItem::Select(bool select)
{
	if(!m_ownerId)
		select = false;

	m_stats.selected=select;

	CheckViewChange();

	for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); it++)
	{
		EntityId cur = (EntityId)it->second;
		IItem *attachment = m_pItemSystem->GetItem(cur);
		if (attachment)
		{
			attachment->OnParentSelect(select);
		}
	}

  IAISystem* pAISystem = gEnv->pAISystem;

	CWeaponAttachmentManager* pWAM = GetOwnerActor()?GetOwnerActor()->GetWeaponAttachmentManager():NULL;

	if (select)
	{
		if (IsDualWield())
		{
			//SetActionSuffix(m_params.dual_wield_suffix.c_str());
			if(m_dualWieldSlaveId)
				SetDualWieldSlave(m_dualWieldSlaveId);
			if(m_dualWieldMasterId)
				SetDualWieldMaster(m_dualWieldMasterId);
		}

		Hide(false);

		if (!m_stats.mounted && GetOwner())
		  GetEntity()->SetWorldTM(GetOwner()->GetWorldTM());	// move somewhere near the owner so the sound can play
		float speedOverride = -1.0f;
		CActor *owner = GetOwnerActor();
		if (owner && owner->GetActorClass() == CPlayer::GetActorClassType())
		{
			CPlayer* pPlayer = (CPlayer *)owner;
			if(pPlayer->GetNanoSuit())
			{
				ENanoMode curMode = pPlayer->GetNanoSuit()->GetMode();
				if (curMode == NANOMODE_SPEED)
					speedOverride = 1.75f;
			}
		}

		const char* select_animation;
	
		//Only the LAW has 2 different select animations
		if(m_params.has_first_select && m_stats.first_selection)
			select_animation = g_pItemStrings->first_select;
		else
			select_animation = g_pItemStrings->select;


		if (speedOverride > 0.0f)
			PlayAction(select_animation, 0, false, eIPAF_Default|eIPAF_NoBlend, speedOverride);
		else
			PlayAction(select_animation, 0, false, eIPAF_Default|eIPAF_NoBlend);

		//ForceSkinning(true);
		uint selectBusyTimer = 0;
		if (m_params.select_override == 0.0f)
			selectBusyTimer = MAX(250, GetCurrentAnimationTime(eIGS_FirstPerson)) - 250;
		else
			selectBusyTimer = (uint)m_params.select_override*1000;
		SetBusy(true);
		GetScheduler()->TimerAction(selectBusyTimer, CSchedulerAction<SelectAction>::Create(), false);

		if (m_stats.fp)
			AttachArms(true, true);
		else
			AttachToHand(true);

		AttachToBack(false);

    if (owner)
    {
			// update smart objects states
			if (pAISystem)
			{
				IEntity* pOwnerEntity = owner->GetEntity();
				pAISystem->ModifySmartObjectStates( pOwnerEntity, GetEntity()->GetClass()->GetName() );
				pAISystem->ModifySmartObjectStates( pOwnerEntity, "WeaponDrawn" );
			}

      //[kirill] make sure AI gets passed the new weapon properties
      if(GetIWeapon() && owner->GetEntity() && owner->GetEntity()->GetAI())
        owner->GetEntity()->GetAI()->SetWeaponDescriptor(GetIWeapon()->GetAIWeaponDescriptor());
    }    
	}
	else
	{
		GetScheduler()->Reset(true);

		if (!m_stats.mounted && !AttachToBack(true))
		{
			SetViewMode(0);
			Hide(true);
		}

		// set no-weapon pose on actor (except for the Offhand)
		CActor *pOwner = GetOwnerActor();
		if (pOwner && (GetEntity()->GetClass()!=CItem::sOffHandClass) && g_pItemStrings)
			pOwner->PlayAction(g_pItemStrings->idle, ITEM_DESELECT_POSE);

		EnableUpdate(false);

		ReleaseStaticSounds();
		if(!m_stats.dropped) //This is done already in CItem::Drop (could cause problems with dual socom)
			ResetAccessoriesScreen(pOwner);

		if(!m_stats.dropped)
			AttachToHand(false);
		AttachArms(false, false);

		if (m_stats.mounted)
			m_stats.fp=false; // so that OnEnterFirstPerson is called next select

		// update smart objects states
		if ( pAISystem && pOwner )
		{
			CryFixedStringT<256> tmpString( "-WeaponDrawn," );
			tmpString += GetEntity()->GetClass()->GetName();
			pAISystem->ModifySmartObjectStates( pOwner->GetEntity(), tmpString.c_str() );
		}
	}

	if (IItem *pSlave=GetDualWieldSlave())
		pSlave->Select(select);

	// ensure attachments get cloaked
	if (select)
	{
		CloakSync(false);
		FrostSync(false);
		WetSync(false);
	}
	else
		CloakEnable(false, false);

	if(CActor *pOwner = GetOwnerActor())
	{
		if(g_pGame && g_pGame->GetIGameFramework())		
			if(pOwner == g_pGame->GetIGameFramework()->GetClientActor())
				SAFE_HUD_FUNC(UpdateCrosshair(select?this:NULL));	//crosshair might change
	}

	OnSelected(select);
}
//------------------------------------------------------------------------
void CItem::Drop(float impulseScale, bool selectNext, bool byDeath)
{
	bool isDWSlave=IsDualWieldSlave();
	bool isDWMaster=IsDualWieldMaster();
		
	CActor *pOwner = GetOwnerActor();
	IInventory * pInventory = pOwner?pOwner->GetInventory():NULL;
	
	if (isDWMaster)
	{
		IItem *pSlave=GetDualWieldSlave();
		if (pSlave)
		{
			pSlave->Drop(impulseScale, false, byDeath);
			ResetDualWield();
			
			if (!byDeath)
				Select(true);

			if (IsServer() && pOwner)
			{
				GetGameObject()->SetNetworkParent(0);
				if ((GetEntity()->GetFlags()&(ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_SERVER_ONLY)) == 0)
					pOwner->GetGameObject()->InvokeRMIWithDependentObject(CActor::ClDrop(), CActor::DropItemParams(GetEntityId(), impulseScale, selectNext, byDeath), eRMI_ToAllClients|eRMI_NoLocalCalls, GetEntityId());
			}
			m_pItemSystem->DropActorAccessory(pOwner, GetEntity()->GetId());

			//AI drop both socoms
			if (!byDeath || (pOwner && !pOwner->IsPlayer()))
				return;
		}
	}

	ResetDualWield();

	if (pOwner)
	{
		if (pInventory && pInventory->GetCurrentItem() == GetEntity()->GetId())
			pInventory->SetCurrentItem(0);

		if (IsServer()) // don't send slave drops over network, the master is sent instead
		{
			GetGameObject()->SetNetworkParent(0);

			if (!isDWSlave)
			{
				if ((GetEntity()->GetFlags()&(ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_SERVER_ONLY)) == 0)
					pOwner->GetGameObject()->InvokeRMIWithDependentObject(CActor::ClDrop(), CActor::DropItemParams(GetEntityId(), impulseScale, selectNext, byDeath), eRMI_ToAllClients|eRMI_NoLocalCalls, GetEntityId());
			}
		}
	}

	bool wasHidden = GetEntity()->IsHidden();

	m_stats.dropped = true;

	bool performCloakFade = m_cloaked;

	Select(false);
	if (performCloakFade)
	{
		// marcok: select calls Cloak already, but here we make sure we uncloak "smoothly"
		CloakEnable(false, true);
	}
	SetViewMode(eIVM_ThirdPerson);
	AttachToHand(false,true);
	AttachToBack(false);

	// AI should ignore collisions from this item for a while
	// to not 'scare' himself and the friends around him
	gEnv->pAISystem->IgnoreCollisionEventsFrom( GetEntity()->GetId(), 2.0f );

	Hide(false);
	GetEntity()->EnablePhysics(true);
	Physicalize(true, true);

	if (gEnv->bServer)
		IgnoreCollision(true);

	EntityId ownerId = GetOwnerId();

	if (pOwner && gEnv->bServer)
	{
		// bump network priority for a moment
		pOwner->GetGameObject()->Pulse('bang');
		GetGameObject()->Pulse('bang');

		if (IMovementController * pMC = pOwner->GetMovementController())
		{
			SMovementState moveState;
			pMC->GetMovementState(moveState);

			Vec3 dir(ZERO);
			Vec3 vel(ZERO);
			dir.Set(0.0f,0.0f,-1.0f);

			if(pOwner->IsPlayer())
			{
				if(GetEntity()->GetPhysics())
				{
					Vec3 pos = moveState.eyePosition;
					dir = moveState.aimDirection;

					if (IPhysicalEntity *pPE=pOwner->GetEntity()->GetPhysics())
					{
						pe_status_dynamics sv;
						if (pPE->GetStatus(&sv))
						{
							if (sv.v.len2()>0.0f)
							{
								float dot=sv.v.GetNormalized().Dot(dir);
								if (dot<0.0f)
									dot=0.0f;
								vel=sv.v*dot;
							}
						}
					}

					//Update position if it was hidden before dropping it
					if(wasHidden)
					{
						Matrix34 newPos(Matrix34::CreateIdentity());
						newPos.SetTranslation(pos+(dir*0.2f));
						GetEntity()->SetWorldTM(newPos);
					}
					
					ray_hit hit;

					if(gEnv->pPhysicalWorld->RayWorldIntersection(pos-dir*0.25f, dir, ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid,
						rwi_stop_at_pierceable|14,&hit, 1, GetEntity()->GetPhysics()))
					{
						pos += (dir*MIN((hit.dist-0.25f), 0.5f));
						dir = Vec3(0.0f,0.0f,-1.0f);
					}
					else
						pos += dir*0.75f; // offset 75cm to avoid camera intersection

					pos -= moveState.upDirection*0.275f;

					Matrix34 worldTM = Matrix34(Matrix33::CreateRotationVDir(dir)*Matrix33::CreateRotationXYZ(Ang3(DEG2RAD(m_params.drop_angles))));
					worldTM.SetTranslation(pos);
					GetEntity()->SetWorldTM(worldTM);
				}
			}

			IEntityPhysicalProxy *pPhysics = GetPhysicalProxy();
			if (pPhysics)
			{
				if (vel.len2()>0.0f)
				{
					if (IPhysicalEntity *pPE=pPhysics->GetPhysicalEntity())
					{
						pe_action_set_velocity asv;
						asv.v=vel;

						pPE->Action(&asv);
					}
				}
				
				pPhysics->AddImpulse(-1, m_params.drop_impulse_pos, dir*m_params.drop_impulse*impulseScale, true, 1.0f);
			}
		}
	}

	if (GetOwnerId())
	{
		if(pInventory)
				pInventory->RemoveItem(GetEntity()->GetId());
		SetOwnerId(0);
	}

	Pickalize(true, true);
	EnableUpdate(false);

	if (IsServer() && g_pGame->GetGameRules())
		g_pGame->GetGameRules()->OnItemDropped(GetEntityId(), pOwner?pOwner->GetEntityId():0);

	if (pOwner && pOwner->IsClient())
	{
		if(!isDWSlave)
			ResetAccessoriesScreen(pOwner);

		if (CanSelect() && selectNext && pOwner->GetHealth()>0 && !isDWSlave)
		{
			CItem				*pLastItem	 = pInventory?static_cast<CItem*>(m_pItemSystem->GetItem(pInventory->GetLastItem())):NULL;
			CBinocular	*pBinoculars = static_cast<CBinocular*>(pOwner->GetItemByClass(CItem::sBinocularsClass));
			
			if (pLastItem && pLastItem->CanSelect() &&(!pBinoculars || pBinoculars->GetEntityId()!=pLastItem->GetEntityId()))
				pOwner->SelectLastItem(false);
			else
				pOwner->SelectNextItem(1, false);
		}
		if (CanSelect())
			m_pItemSystem->DropActorItem(pOwner, GetEntity()->GetId());
		else
			m_pItemSystem->DropActorAccessory(pOwner, GetEntity()->GetId());
	}

	Cloak(false);

	Quiet();

	OnDropped(ownerId);
}

//------------------------------------------------------------------------
void CItem::PickUp(EntityId pickerId, bool sound, bool select, bool keepHistory)
{
	CActor *pActor=GetActor(pickerId);
	if (!pActor)
		return;

	//1) First check inventory restrictions (only in the server)
	if(IsServer())
	{
		if (!pActor->CheckInventoryRestrictions(GetEntity()->GetClass()->GetName()))
		{
			g_pGame->GetGameRules()->SendTextMessage(eTextMessageError, "@mp_CannotCarryMore", eRMI_ToClientChannel, pActor->GetChannelId());
			return;
		}
		else if(!CheckAmmoRestrictions(pickerId))
		{
			g_pGame->GetGameRules()->SendTextMessage(eTextMessageCenter, "@ammo_maxed_out", eRMI_ToClientChannel, pActor->GetChannelId(), (string("@")+GetEntity()->GetClass()->GetName()).c_str());
			return;
		}
	}
	
	//2) Update some item properties
	TriggerRespawn();

	GetEntity()->EnablePhysics(false);
	Physicalize(false, false);

	bool soundEnabled = IsSoundEnabled();
	EnableSound(sound);

	SetViewMode(0);		
	SetOwnerId(pickerId);

	CopyRenderFlags(GetOwner());

	Hide(true);
	m_stats.dropped = false;
	m_stats.brandnew = false;

	{
		// Beni - Is this needed?
		// move the entity to picker position
		Matrix34 tm(pActor->GetEntity()->GetWorldTM());
		tm.AddTranslation(Vec3(0,0,2));
		GetEntity()->SetWorldTM(tm);
	}

	//3) Inventory changes....
	bool alone = true;
	bool slave = false;
	IInventory *pInventory = GetActorInventory(pActor);
	if (!pInventory)
	{
		GameWarning("Actor '%s' has no inventory, when trying to pickup '%s'!",pActor->GetEntity()->GetName(),GetEntity()->GetName());
		return;
	}

	if (IsServer() && pActor->IsPlayer())
	{
		// go through through accessories map and give a copy of each to the player
		for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); it++)
		{
			const char *name=it->first.c_str();
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			EntityId accessoryId = pInventory->GetItemByClass(pClass);
			if(!accessoryId)
				g_pGame->GetIGameFramework()->GetIItemSystem()->GiveItem(pActor, name, false, false, true);
			else if(CItem* pAccessory = static_cast<CItem*>(m_pItemSystem->GetItem(it->second)))
				pAccessory->OnPickedUp(pickerId,true);
				
		}

		for(TInitialSetup::iterator it = m_initialSetup.begin(); it != m_initialSetup.end(); it++)
		{
			const char *name=it->c_str();
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			EntityId accessoryId = pInventory->GetItemByClass(pClass);
			if(!accessoryId)
				g_pGame->GetIGameFramework()->GetIItemSystem()->GiveItem(pActor, name, false, false, true);
		}
	}

	CItem *pBuddy=0;
	int n = pInventory->GetCount();
	for (int i=0; i<n; i++)
	{
		EntityId itemId = pInventory->GetItem(i);
		IItem *pItem = m_pItemSystem->GetItem(itemId);
		if (!pItem)
			continue;

		if (pItem != this)
		{
			if (!pItem->IsDualWield() && pItem->SupportsDualWield(GetEntity()->GetClass()->GetName()))
			{
				bool wasSelected = false;
				if (pItem->GetEntity()->GetId() == pInventory->GetCurrentItem())
				{
					pItem->Select(false);
					wasSelected = true;
				}
				EnableUpdate(true, eIUS_General);

				if(!pInventory->IsSerializingForLevelChange())			
				{
					SetDualWieldMaster(pItem->GetEntity()->GetId());
					pItem->SetDualWieldSlave(GetEntity()->GetId());
					pItem->SetDualSlaveAccessory();
					slave = true;
				}
				if (wasSelected)
					pItem->Select(true);

				//Set the same fire mode for both weapons
				IWeapon *pMasterWeapon = pItem->GetIWeapon();
				IWeapon *pSlaveWeapon = GetIWeapon();
				if(pMasterWeapon && pSlaveWeapon)
				{
					if(pMasterWeapon->GetCurrentFireMode()!=pSlaveWeapon->GetCurrentFireMode())
					{
						pSlaveWeapon->ChangeFireMode();
					}
				}

				break;
			}
			else if (pItem->GetEntity()->GetClass() == GetEntity()->GetClass())
			{
				pBuddy=static_cast<CItem *>(pItem);
				alone=false;
			}
		}
	}

	OnPickedUp(pickerId, m_params.unique && !alone);	// some weapons will get ammo when picked up
																										// this will dictate the result of CanSelect() below
																										// so we'll grab the ammo before trying to select

	//4) Add item to inventory, select it or remove it
	CItem* itemToSelect = NULL;
	if (slave || alone || !m_params.unique)
	{
		// add to inventory
		pInventory->AddItem(GetEntity()->GetId());

		itemToSelect = this;

		EnableSound(soundEnabled);

		if (IsServer() && g_pGame->GetGameRules())
			g_pGame->GetGameRules()->OnItemPickedUp(GetEntityId(), pickerId);

		PlayAction(g_pItemStrings->pickedup);

		//AI back weapon attachments
		if(!gEnv->bMultiplayer && !IsSelected())
		{
			AttachToBack(true);
		}
	}
	else if (!slave && m_params.unique && !alone)
	{
		if (IsServer() && !IsDemoPlayback())
			RemoveEntity();

		if (pBuddy)
		{
			pBuddy->PlayAction(g_pItemStrings->pickedup);
			if(m_params.select_on_pickup)
				itemToSelect = pBuddy; //Usually only for explosive weapons
		}
	}

	if(itemToSelect)
	{
		SPlayerStats *pStats = NULL;
		if(pActor->GetActorClassType() == CPlayer::GetActorClassType())
			pStats = static_cast<SPlayerStats*>(pActor->GetActorStats());
		if (select && !pActor->GetLinkedVehicle() && !pActor->ShouldSwim() && (!pStats || !pStats->isOnLadder))
		{
			if(CanSelect() && !slave)
				m_pItemSystem->SetActorItem(GetOwnerActor(), itemToSelect->GetEntity()->GetId(), keepHistory);
			else
				m_pItemSystem->SetActorAccessory(GetOwnerActor(), itemToSelect->GetEntity()->GetId(), keepHistory);

			pActor->GetGameObject()->ChangedNetworkState(CPlayer::ASPECT_CURRENT_ITEM);
		}
	}


	if (IsServer())
	{
		GetGameObject()->SetNetworkParent(pickerId);
		if ((GetEntity()->GetFlags()&(ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_SERVER_ONLY)) == 0)
		{
			bool remoteSelect = m_stats.selected;
			if(m_params.select_on_pickup)
				remoteSelect = true;
			pActor->GetGameObject()->InvokeRMIWithDependentObject(CActor::ClPickUp(), CActor::PickItemParams(GetEntityId(), remoteSelect, sound), eRMI_ToAllClients|eRMI_NoLocalCalls, GetEntityId());

			const char *displayName=GetDisplayName();

			if (pActor->IsClient() && sound && g_pGame->GetHUD() && displayName && displayName[0])
				g_pGame->GetHUD()->BattleLogEvent(eBLE_Information, "@mp_BLYouPickedup", displayName);
		}
	}
}

//------------------------------------------------------------------------
void CItem::Physicalize(bool enable, bool rigid)
{
	int profile=eIPhys_NotPhysicalized;
	if (enable)
		profile=rigid?eIPhys_PhysicalizedRigid:eIPhys_PhysicalizedStatic;

	if (IsServer())
		GetGameObject()->SetAspectProfile(eEA_Physics, profile);
}

//------------------------------------------------------------------------
void CItem::Pickalize(bool enable, bool dropped)
{
	if (enable)
	{
		m_stats.flying = dropped;
		m_stats.dropped = true;
		m_stats.pickable = true;

		if(dropped)
		{
			GetEntity()->KillTimer(eIT_Flying);
			GetEntity()->SetTimer(eIT_Flying, m_params.fly_timer);
		}
	}
	else
	{
		m_stats.flying = false;
		m_stats.pickable = false;
	}
}

//------------------------------------------------------------------------
void CItem::IgnoreCollision(bool ignore)
{
	IPhysicalEntity *pPE=GetEntity()->GetPhysics();
	if (!pPE)
		return;

	if (ignore)
	{
		CActor *pActor=GetOwnerActor();
		if (!pActor)
			return;

		IPhysicalEntity *pActorPE=pActor->GetEntity()->GetPhysics();
		if (!pActorPE)
			return;

		pe_action_add_constraint ic;
		ic.flags=constraint_inactive|constraint_ignore_buddy;
		ic.pBuddy=pActorPE;
		ic.pt[0].Set(0,0,0);
		m_constraintId=pPE->Action(&ic);
	}
	else
	{
		pe_action_update_constraint up;
		up.bRemove=true;
		up.idConstraint = m_constraintId;
		m_constraintId=0;
		pPE->Action(&up);
	}
}

//------------------------------------------------------------------------
void CItem::AttachArms(bool attach, bool shadow)
{
	if (!m_params.arms)
		return;

	CActor *pOwner = static_cast<CActor *>(GetOwnerActor());
	if (!pOwner)
		return;

	if (attach)
		SetGeometry(eIGS_Arms, 0);
	else
		ResetCharacterAttachment(eIGS_FirstPerson, ITEM_ARMS_ATTACHMENT_NAME);

	SetFPWeapon(attach?0.1f:0.0f,attach);

	if (shadow && !m_stats.mounted)
	{
		ICharacterInstance *pOwnerCharacter = pOwner->GetEntity()->GetCharacter(0);
		if (!pOwnerCharacter)
			return;

		IAttachmentManager *pAttachmentManager = pOwnerCharacter->GetIAttachmentManager();
		IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(m_params.attachment[m_stats.hand].c_str());

		if (!pAttachment)
		{
			GameWarning("Item owner '%s' doesn't have third-person item attachment point '%s'!", pOwner->GetEntity()->GetName(), m_params.attachment[m_stats.hand].c_str());
			return;
		}

		if (!shadow)
			pAttachment->ClearBinding();
		else if (IStatObj *pStatObj=GetEntity()->GetStatObj(eIGS_ThirdPerson))
		{
			CCGFAttachment *pCGFAttachment = new CCGFAttachment();
			pCGFAttachment->pObj = pStatObj;

			IEntityRenderProxy * pOwnerRP = (IEntityRenderProxy*) pOwner->GetEntity()->GetProxy(ENTITY_PROXY_RENDER);

			//if (pOwnerActor->GetReplacementMaterial())
			{
				//pAttachment->HideInShadow(1);
				//pAttachment->GetIAttachmentObject()->SetMaterial(pOwnerActor->GetReplacementMaterial());
			}
			pAttachment->AddBinding(pCGFAttachment);
			pAttachment->HideAttachment(1);
			pAttachment->HideInShadow(0);
		}
	}
}

//----------------------------------------------------------------------
void CItem::SetFPWeapon(float dt, bool registerByPosition /*=false*/)
{
	if(m_useFPCamSpacePP && IsOwnerFP() && !IsMounted())
	{
		if(ICharacterInstance *pWepChar = GetEntity()->GetCharacter(0))
			pWepChar->SetFPWeapon(dt);

		IEntityRenderProxy* pProxy = GetRenderProxy();
		if(pProxy && pProxy->GetRenderNode())
			pProxy->GetRenderNode()->SetRndFlags(ERF_REGISTER_BY_POSITION,registerByPosition);
	}
}
//------------------------------------------------------------------------
void CItem::Impulse(const Vec3 &position, const Vec3 &direction, float impulse)
{
	if (direction.len2() <= 0.001f)
		return;

	IEntityPhysicalProxy *pPhysicsProxy = GetPhysicalProxy();
	if (pPhysicsProxy)
		pPhysicsProxy->AddImpulse(-1, position, direction.GetNormalized()*impulse, true, 1);
}

//------------------------------------------------------------------------
bool CItem::CanPickUp(EntityId userId) const
{
	if (m_params.pickable && m_stats.pickable && !m_stats.flying && !m_frozen && (!m_ownerId || m_ownerId==userId) && !m_stats.selected && !m_stats.used && !GetEntity()->IsHidden())
	{
		CActor *pActor = GetActor(userId);
		IInventory *pInventory=GetActorInventory(pActor);
		
		//Kits stuff (all kits have the same uniqueId, player can only have one)
		uint8 uniqueId = m_pItemSystem->GetItemUniqueId(GetEntity()->GetClass()->GetName());

		if (pInventory && (pInventory->GetCountOfUniqueId(uniqueId)>0))
		{
			//A bit workaround (kits have uniqueId 1)
			if(pActor->IsClient() && (uniqueId==1))
				g_pGame->GetGameRules()->OnTextMessage(eTextMessageCenter, "@mp_CannotCarryMoreKit");
			return false;
		}

		if (pInventory && pInventory->FindItem(GetEntityId())==-1)
			return true;
	}
	return false;
}

//------------------------------------------------------------------------
bool CItem::CanDrop() const
{
	if (m_params.droppable)
		return true;

	return false;
}

//------------------------------------------------------------------------
bool CItem::CanUse(EntityId userId) const
{
	bool bUsable=m_params.usable && m_properties.usable && IsMounted() && (!m_stats.used || (m_ownerId == userId) && !GetEntity()->IsHidden());
	if (!bUsable)
		return (false);

	if (IsMounted())
	{	
		// if not a machine gun on a vehicle, check if we're in front of it 
		CActor *pActor = GetActor(userId);
		if (!pActor)
			return (true); 
		if (pActor->GetLinkedVehicle())
			return (true);

		Vec3 vActorDir=pActor->GetEntity()->GetWorldTM().TransformVector(FORWARD_DIRECTION);
		float fDot=vActorDir*m_stats.mount_dir;

		if (fDot<0)
			return (false);		
	}

	return (true);
}

//------------------------------------------------------------------------
bool CItem::IsMounted() const
{
	return m_stats.mounted;
}


//------------------------------------------------------------------------
void CItem::SetMountedAngleLimits(float min_pitch, float max_pitch, float yaw_range)
{
	m_mountparams.min_pitch = min(min_pitch,max_pitch);//(min_pitch<=0.0f)?min_pitch:0.0f; //Assert values
	m_mountparams.max_pitch = max(min_pitch,max_pitch);//(max_pitch>=0.0f)?max_pitch:0.0f;
	m_mountparams.yaw_range = yaw_range;
}


//------------------------------------------------------------------------
Vec3 CItem::GetMountedAngleLimits() const
{
	if(m_stats.mounted)
		return Vec3(m_mountparams.min_pitch, m_mountparams.max_pitch, m_mountparams.yaw_range);
	else 
		return ZERO;
}

//------------------------------------------------------------------------
bool CItem::IsUsed() const
{
	return m_stats.used;
}

//------------------------------------------------------------------------
bool CItem::InitRespawn()
{
	if (IsServer() && m_respawnprops.respawn)
	{
		CGameRules *pGameRules=g_pGame->GetGameRules();
		assert(pGameRules);
		if (pGameRules)
			pGameRules->CreateEntityRespawnData(GetEntityId());

		return true;
	}

	return false;
};

//------------------------------------------------------------------------
void CItem::TriggerRespawn()
{
	if (!m_stats.brandnew || !IsServer())
		return;
	
	if (m_respawnprops.respawn)
	{

		CGameRules *pGameRules=g_pGame->GetGameRules();
		assert(pGameRules);
		if (pGameRules)
			pGameRules->ScheduleEntityRespawn(GetEntityId(), m_respawnprops.unique, m_respawnprops.timer);
	}
}

//------------------------------------------------------------------------
void CItem::EnableSelect(bool enable)
{
	m_stats.selectable = enable;
}

//------------------------------------------------------------------------
bool CItem::CanSelect() const
{
	if(g_pGameCVars->g_proneNotUsableWeapon_FixType == 1)
		if(GetOwnerActor() && GetOwnerActor()->GetStance() == STANCE_PRONE && GetParams().prone_not_usable)
			return false;

	return m_params.selectable && m_stats.selectable;
}

//------------------------------------------------------------------------
bool CItem::IsSelected() const
{
	return m_stats.selected;
}

//------------------------------------------------------------------------
void CItem::EnableSound(bool enable)
{
	m_stats.sound_enabled = enable;
}

//------------------------------------------------------------------------
bool CItem::IsSoundEnabled() const
{
	return m_stats.sound_enabled;
}

//------------------------------------------------------------------------
void CItem::MountAt(const Vec3 &pos)
{
	if (!m_params.mountable)
		return;

	m_stats.mounted = true;

	SetViewMode(eIVM_FirstPerson);
	
	Matrix34 tm(GetEntity()->GetWorldTM());
	tm.SetTranslation(pos);
	GetEntity()->SetWorldTM(tm);

	m_stats.mount_dir = GetEntity()->GetWorldTM().TransformVector(FORWARD_DIRECTION);
}

//------------------------------------------------------------------------
void CItem::MountAtEntity(EntityId entityId, const Vec3 &pos, const Ang3 &angles)
{
	if (!m_params.mountable)
		return;

	IEntity *pHost = m_pEntitySystem->GetEntity(entityId);
	if (!pHost)
		return;

	m_hostId = entityId;
	m_stats.mounted = true;

	SetViewMode(eIVM_FirstPerson);

	pHost->AttachChild(GetEntity(), 0);

	Matrix34 tm = Matrix33(Quat::CreateRotationXYZ(angles));
	tm.SetTranslation(pos);
	GetEntity()->SetLocalTM(tm);

	m_stats.mount_dir = GetEntity()->GetWorldTM().TransformVector(FORWARD_DIRECTION);
}


//------------------------------------------------------------------------
IEntity *CItem::GetOwner() const
{
	if (!m_ownerId)
		return 0;

	return m_pEntitySystem->GetEntity(m_ownerId);
}

//------------------------------------------------------------------------
CActor *CItem::GetOwnerActor() const
{
	if(!m_pGameFramework)
		return NULL;
	return static_cast<CActor *>(m_pGameFramework->GetIActorSystem()->GetActor(m_ownerId));
}

//------------------------------------------------------------------------
CActor *CItem::GetActor(EntityId actorId) const
{
	return static_cast<CActor *>(m_pGameFramework->GetIActorSystem()->GetActor(actorId));
}

//------------------------------------------------------------------------
IInventory *CItem::GetActorInventory(IActor *pActor) const
{
	if (!pActor)
		return 0;

	return pActor->GetInventory();
}

//------------------------------------------------------------------------
CItem *CItem::GetActorItem(IActor *pActor) const
{
	if (!pActor)
		return 0;

	IInventory *pInventory=pActor->GetInventory();
	if (!pInventory)
		return 0;

	EntityId id = pInventory->GetCurrentItem();
	if (!id)
		return 0;

	return static_cast<CItem *>(m_pItemSystem->GetItem(id));
}

//------------------------------------------------------------------------
EntityId CItem::GetActorItemId(IActor *pActor) const
{
	if (!pActor)
		return 0;

	IInventory *pInventory=pActor->GetInventory();
	if (!pInventory)
		return 0;

	EntityId id = pInventory->GetCurrentItem();
	if (!id)
		return 0;

	return id;
}

//------------------------------------------------------------------------
CActor *CItem::GetActorByNetChannel(INetChannel *pNetChannel) const
{
	return static_cast<CActor *>(m_pGameFramework->GetIActorSystem()->GetActorByChannelId(m_pGameFramework->GetGameChannelId(pNetChannel)));
}

//------------------------------------------------------------------------
const char *CItem::GetDisplayName() const
{
	return m_params.display_name.c_str();
}

//------------------------------------------------------------------------
void CItem::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	const char* temp = actionId.c_str();
	CallScriptEvent(eISET_Client, "OnAction", actorId, temp, activationMode, value, 0, 0, 0);
}

//------------------------------------------------------------------------
void CItem::StartUse(EntityId userId)
{
	if (!m_params.usable || m_ownerId)
		return;

	// holster user item here
	SetOwnerId(userId);

	CActor* pOwner=GetOwnerActor();
	if (!pOwner)
		return;
	
	m_pItemSystem->SetActorItem(pOwner, GetEntityId(), true);

	m_stats.used = true;

	ApplyViewLimit(userId, true);

	EnableUpdate(true, eIUS_General);
	RequireUpdate(eIUS_General);

	// TODO: precreate this table
	SmartScriptTable locker(gEnv->pScriptSystem);
	locker->SetValue("locker", ScriptHandle(GetEntityId()));
	locker->SetValue("lockId", ScriptHandle(GetEntityId()));
	locker->SetValue("lockIdx", 1);
	pOwner->GetGameObject()->SetExtensionParams("Interactor", locker);

	pOwner->LinkToMountedWeapon(GetEntityId());
	SAFE_HUD_FUNC(GetCrosshair()->SetUsability(0));

	//Don't draw legs for the FP player (prevents legs clipping in the view)
	if (pOwner->IsClient() && !pOwner->IsThirdPerson())
	{
		ICharacterInstance* pChar = pOwner->GetEntity()->GetCharacter(0);
		if(pChar)
			pChar->HideMaster(1);
	}
		
	if(pOwner->GetAnimatedCharacter())
		pOwner->GetAnimatedCharacter()->SetNoMovementOverride(true);

	if (IsServer())
		pOwner->GetGameObject()->InvokeRMI(CActor::ClStartUse(), CActor::ItemIdParam(GetEntityId()), eRMI_ToAllClients|eRMI_NoLocalCalls);
}

//------------------------------------------------------------------------
void CItem::StopUse(EntityId userId)
{
	if (userId != m_ownerId)
		return;

	CActor *pActor = GetOwnerActor();
	if (!pActor)
		return;

	//Draw legs again for the FP player
	if (pActor->IsClient())
	{
		ICharacterInstance* pChar = pActor->GetEntity()->GetCharacter(0);
		if(pChar)
			pChar->HideMaster(0);
	}

	if (pActor->GetHealth()>0)
		pActor->SelectLastItem(true);

	pActor->GetAnimationGraphState()->SetInput("Action","idle");

	ApplyViewLimit(userId, false);

  if (m_stats.mounted)
  {
    AttachArms(false, false);
  }
		
	EnableUpdate(false);

	m_stats.used = false;

	SetOwnerId(0);

	CloakEnable(false, pActor->IsCloaked());

	// TODO: precreate this table
	SmartScriptTable locker(gEnv->pScriptSystem);
	locker->SetValue("locker", ScriptHandle(GetEntityId()));
	locker->SetValue("lockId", ScriptHandle(0));
	locker->SetValue("lockIdx", 0);
	pActor->GetGameObject()->SetExtensionParams("Interactor", locker);

	pActor->LinkToMountedWeapon(0);

	if(pActor->GetAnimatedCharacter())
		pActor->GetAnimatedCharacter()->SetNoMovementOverride(false);

	if (IsServer())
		pActor->GetGameObject()->InvokeRMI(CActor::ClStopUse(), CActor::ItemIdParam(GetEntityId()), eRMI_ToAllClients|eRMI_NoLocalCalls);
}

//------------------------------------------------------------------------
void CItem::ApplyViewLimit(EntityId userId, bool apply)
{
	CActor *pActor = GetActor(userId);
	if (!pActor)
		return;

	SActorParams *pParams = pActor->GetActorParams();

	if (apply)
	{
		SActorParams *pParams = pActor->GetActorParams();

		pParams->viewPivot = GetEntity()->GetWorldPos();
		pParams->viewDistance = -m_mountparams.eye_distance;
		pParams->viewHeightOffset = m_mountparams.eye_height;
		pParams->vLimitDir = m_stats.mount_dir;
		pParams->vLimitRangeH = DEG2RAD(m_mountparams.yaw_range);
		//pParams->vLimitRangeV = DEG2RAD((m_mountparams.max_pitch-m_mountparams.min_pitch)*0.5f);
		pParams->vLimitRangeVUp = DEG2RAD(m_mountparams.max_pitch);
		pParams->vLimitRangeVDown = DEG2RAD(m_mountparams.min_pitch);
		pParams->speedMultiplier = 0.0f;
	}
	else
	{
		pParams->viewPivot.zero();
		pParams->viewDistance = 0.0f;
		pParams->viewHeightOffset = 0.0f;
		pParams->vLimitDir.zero();
		pParams->vLimitRangeH = 0.0f;
		pParams->vLimitRangeV = 0.0f;
		pParams->vLimitRangeVUp = pParams->vLimitRangeVDown = 0.0f;
		pParams->speedMultiplier = 1.0f;
	}

}

//------------------------------------------------------------------------
void CItem::UseManualBlending(bool enable)
{
  IActor* pActor = GetOwnerActor();
  if (!pActor)
    return;

  if (ICharacterInstance* pCharInstance = pActor->GetEntity()->GetCharacter(0))
  {
    if (ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim())
    { 
      pSkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TurnSpeed, 0.f, enable);
    }        
  } 
}

//------------------------------------------------------------------------
bool CItem::AttachToHand(bool attach, bool checkAttachment)
{
	//Skip mounted and offhand
	if (m_stats.mounted || (GetEntity()->GetClass()==CItem::sOffHandClass))
    return false;

	IEntity *pOwner = GetOwner();
	if (!pOwner)
		return false;

	ICharacterInstance *pOwnerCharacter = pOwner->GetCharacter(0);
	if (!pOwnerCharacter)
		return false;

	IAttachmentManager *pAttachmentManager = pOwnerCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(m_params.attachment[m_stats.hand].c_str());

	if (!pAttachment)
	{
		GameWarning("Item owner '%s' doesn't have third-person item attachment point '%s'!", pOwner->GetName(), m_params.attachment[m_stats.hand].c_str());
		return false;
	}

	if (!attach)
	{
		if(checkAttachment)
		{
			if(IAttachmentObject *pAO = pAttachment->GetIAttachmentObject())
			{
				//The item can only be detached by itself
				if(pAO->GetAttachmentType()==IAttachmentObject::eAttachment_Entity)
				{
					CEntityAttachment* pEA = static_cast<CEntityAttachment*>(pAO);
					if(pEA->GetEntityId()!=GetEntityId())
						return false;
				}
			}
		}

		pAttachment->ClearBinding();
	}
	else
	{
		CEntityAttachment *pEntityAttachment = new CEntityAttachment();
		pEntityAttachment->SetEntityId(GetEntityId());

		pAttachment->AddBinding(pEntityAttachment);
		pAttachment->HideAttachment(0);
	}

	return true;
}

//------------------------------------------------------------------------
bool CItem::AttachToBack(bool attach)
{
	if (gEnv->bMultiplayer || !m_params.attach_to_back)
		return false;

	IEntity *pOwner = GetOwner();
	if (!pOwner)
		return false;

	CActor* pActor = GetOwnerActor();
	CWeaponAttachmentManager* pWAM = pActor?pActor->GetWeaponAttachmentManager():NULL;

	//Do not attach on drop
	if(attach && m_stats.dropped)
		return false;

	if(!pWAM)
		return false;

	if(SActorStats* pStats = pActor->GetActorStats())
		if(pStats->isGrabbed && attach)
			return false;

	ICharacterInstance *pOwnerCharacter = pOwner->GetCharacter(0);
	if (!pOwnerCharacter)
		return false;

	IAttachmentManager *pAttachmentManager = pOwnerCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = NULL;

	bool isCloaked = pActor->GetReplacementMaterial() != 0;

	CloakSync(false);
	FrostSync(false);
	WetSync(false);

	if(attach)
	{
		/*if(SupportsDualWield(GetEntity()->GetClass()->GetName()))
		{
			if(IsDualWieldMaster())
			{
				pAttachment = pAttachmentManager->GetInterfaceByName(m_params.bone_attachment_01.c_str());
				m_stats.backAttachment = eIBA_Primary;
			}
			else if(IsDualWieldSlave())
			{
				pAttachment = pAttachmentManager->GetInterfaceByName(m_params.bone_attachment_02.c_str());
				m_stats.backAttachment = eIBA_Secondary;
			}
			else
			{
				pAttachment = pAttachmentManager->GetInterfaceByName(m_params.bone_attachment_01.c_str());
				m_stats.backAttachment = eIBA_Primary;
			}
		}
		else*/
		{
			pAttachment = pAttachmentManager->GetInterfaceByName(m_params.bone_attachment_01.c_str());
			m_stats.backAttachment = eIBA_Primary;
			if(pAttachment && pAttachment->GetIAttachmentObject())
			{
				pAttachment = pAttachmentManager->GetInterfaceByName(m_params.bone_attachment_02.c_str());
				m_stats.backAttachment = eIBA_Secondary;
			}
		}
	}
	else if(m_stats.backAttachment == eIBA_Primary)
	{
		pAttachment = pAttachmentManager->GetInterfaceByName(m_params.bone_attachment_01.c_str());
	}
	else if(m_stats.backAttachment == eIBA_Secondary)
	{
		pAttachment = pAttachmentManager->GetInterfaceByName(m_params.bone_attachment_02.c_str());
	}
	else
	{
		return false;
	}
	
	if (!pAttachment)
	{
		if(m_stats.backAttachment == eIBA_Primary)
			GameWarning("Item owner '%s' doesn't have third-person item attachment point '%s'!", pOwner->GetName(), m_params.bone_attachment_01.c_str());
		else
			GameWarning("Item owner '%s' doesn't have third-person item attachment point '%s'!", pOwner->GetName(), m_params.bone_attachment_02.c_str());

		m_stats.backAttachment = eIBA_Unknown;
		return false;
	}

	if (!attach)
	{
		eItemBackAttachment temp = m_stats.backAttachment;
		m_stats.backAttachment = eIBA_Unknown;
		if(IAttachmentObject *pAO = pAttachment->GetIAttachmentObject())
		{
			//The item can only be detached by itself
			if(pAO->GetAttachmentType()==IAttachmentObject::eAttachment_Entity)
			{
				CEntityAttachment* pEA = static_cast<CEntityAttachment*>(pAO);
				if(pEA->GetEntityId()!=GetEntityId())
					return false;
			}
		}
		pAttachment->ClearBinding();
		if(temp == eIBA_Primary)
			pWAM->SetWeaponAttachment(false,m_params.bone_attachment_01.c_str(),GetEntityId());
		else
			pWAM->SetWeaponAttachment(false,m_params.bone_attachment_02.c_str(),GetEntityId());	
	}
	else
	{
		if(GetOwnerActor() && GetOwnerActor()->IsPlayer())
		{
			SetViewMode(0);
			Hide(true);

			if (IStatObj *pStatObj=GetEntity()->GetStatObj(eIGS_ThirdPerson))
			{
				CCGFAttachment *pCGFAttachment = new CCGFAttachment();
				pCGFAttachment->pObj = pStatObj;

				pAttachment->AddBinding(pCGFAttachment);
				pAttachment->HideAttachment(1);
				if (isCloaked)
					pAttachment->GetIAttachmentObject()->SetMaterial(pActor->GetReplacementMaterial());
				else
					pAttachment->HideInShadow(0);
			}
		}
		else
		{
			SetViewMode(eIVM_ThirdPerson);

			CEntityAttachment *pEntityAttachment = new CEntityAttachment();
			pEntityAttachment->SetEntityId(GetEntityId());

			pAttachment->AddBinding(pEntityAttachment);
			pAttachment->HideAttachment(0);

			//After QS/QL, hide if owner is hidden
			if(pOwner->IsHidden())
				Hide(true);
			else 
				Hide(false);
		}
		if(m_stats.backAttachment == eIBA_Primary)
			pWAM->SetWeaponAttachment(true,m_params.bone_attachment_01.c_str(),GetEntityId());
		else
			pWAM->SetWeaponAttachment(true,m_params.bone_attachment_02.c_str(),GetEntityId());	
	}

	return true;
}

//------------------------------------------------------------------------
void CItem::RequireUpdate(int slot)
{
	if (slot==-1)
		for (int i=0;i<4;i++)
			GetGameObject()->ForceUpdateExtension(this, i);	
	else
		GetGameObject()->ForceUpdateExtension(this, slot);
}

//------------------------------------------------------------------------
void CItem::EnableUpdate(bool enable, int slot)
{
	if (enable)
	{
		if (slot==-1)
			for (int i=0;i<4;i++)
				GetGameObject()->EnableUpdateSlot(this, i);
		else
			GetGameObject()->EnableUpdateSlot(this, slot);

	}
	else
	{
		if (slot==-1)
		{
			for (int i=0;i<4;i++)
				GetGameObject()->DisableUpdateSlot(this, i);
		}
		else
			GetGameObject()->DisableUpdateSlot(this, slot);
	}
}

//------------------------------------------------------------------------
void CItem::Hide(bool hide)
{
	GetEntity()->SetFlags(GetEntity()->GetFlags()&~ENTITY_FLAG_UPDATE_HIDDEN);

	if ((hide && m_stats.fp) || IsServer())
		GetEntity()->SetFlags(GetEntity()->GetFlags()|ENTITY_FLAG_UPDATE_HIDDEN);	

	GetEntity()->Hide(hide);
}

//------------------------------------------------------------------------
void CItem::HideArms(bool hide)
{
	HideCharacterAttachment(eIGS_FirstPerson, ITEM_ARMS_ATTACHMENT_NAME, hide);
}

//------------------------------------------------------------------------
void CItem::HideItem(bool hide)
{
	HideCharacterAttachmentMaster(eIGS_FirstPerson, ITEM_ARMS_ATTACHMENT_NAME, hide);

	IEntity *pOwner = GetOwner();
	if (!pOwner)
		return;

	ICharacterInstance *pOwnerCharacter = pOwner->GetCharacter(0);
	if (!pOwnerCharacter)
		return;

	IAttachmentManager *pAttachmentManager = pOwnerCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(m_params.attachment[m_stats.hand].c_str());
	if (pAttachment)
		pAttachment->HideAttachment(hide?1:0);
}

//------------------------------------------------------------------------
void CItem::Freeze(bool freeze)
{
	if (freeze)
	{
		m_frozen = true;
		for (int i=0; i<eIGS_Last; i++)
		{
			ICharacterInstance *pCharacter = GetEntity()->GetCharacter(i);
			if (pCharacter)
				pCharacter->SetAnimationSpeed(0);
		}

		Quiet();
	}
	else
	{
		m_frozen = false;
		for (int i=0; i<eIGS_Last; i++)
		{
			ICharacterInstance *pCharacter = GetEntity()->GetCharacter(i);
			if (pCharacter)
				pCharacter->SetAnimationSpeed(m_animationSpeed[i]);
		}
	}

	if (IsDualWieldMaster() && IsServer())
		g_pGame->GetGameRules()->FreezeEntity(GetDualWieldSlaveId(), freeze, true);
}

void CItem::Cloak(bool cloak, IMaterial *cloakMat)
{
	if (cloak == m_cloaked || IsMounted())
		return;

	// check if the actor is cloaked
	CActor* pOwner = GetOwnerActor();
	if (!pOwner)
		return;
	if (!pOwner->GetEntity())
		return;

	IEntityRenderProxy * pOwnerRP = (IEntityRenderProxy*) pOwner->GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	IEntityRenderProxy * pItemRP = (IEntityRenderProxy*) GetEntity()->GetProxy(ENTITY_PROXY_RENDER);

	if (pItemRP && pOwnerRP)
	{ 
		IRenderNode *pOwnerRN =  pOwnerRP->GetRenderNode();
		IRenderNode *pItemRN =  pItemRP->GetRenderNode();

		//pItemRP->SetMaterialLayersMask( pOwnerRP->GetMaterialLayersMask() );
		//pItemRP->SetMaterialLayersBlend( pOwnerRP->GetMaterialLayersBlend() );
	}

	for (TAccessoryMap::iterator it=m_accessories.begin(); it!=m_accessories.end(); ++it)
	{
		if (CItem *pAccessory=static_cast<CItem *>(m_pItemSystem->GetItem(it->second)))
			pAccessory->Cloak(cloak, cloakMat);
	}

/*
	if(cloak)	//when switching view there are some errors without this check
	{
    CActor* pActor = GetOwnerActor();
    if (pActor && pActor->GetActorClass() == CPlayer::GetActorClassType())
    {
      CPlayer *plr = (CPlayer *)pActor;
      if(plr->GetNanoSuit() && plr->GetNanoSuit()->GetMode() != NANOMODE_CLOAK)
			{
				if (CItem *pSlave=static_cast<CItem *>(GetDualWieldSlave()))
					pSlave->Cloak(cloak, cloakMat);
        return;
			}
    }
	}
*/

	SEntitySlotInfo slotInfo;
	

	if (GetEntity()->GetSlotInfo(CItem::eIGS_FirstPerson, slotInfo))
	{
		if (slotInfo.pCharacter)
			SetMaterialRecursive(slotInfo.pCharacter, !cloak, NULL);
	}

	slotInfo=SEntitySlotInfo();
	if (GetEntity()->GetSlotInfo(CItem::eIGS_ThirdPerson, slotInfo))
	{
		if (slotInfo.pStatObj)
			GetEntity()->SetSlotMaterial(CItem::eIGS_ThirdPerson, NULL);
		else if (slotInfo.pCharacter)
			SetMaterialRecursive(slotInfo.pCharacter, !cloak, NULL);
	}

	if (CItem *pSlave=static_cast<CItem *>(GetDualWieldSlave()))
		pSlave->Cloak(cloak, cloakMat);

	m_cloaked = cloak;
}

void CItem::CloakEnable(bool enable, bool fade)
{
	IEntityRenderProxy * pItemRP = (IEntityRenderProxy*) GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	if (pItemRP)
	{
		uint8 mask = pItemRP->GetMaterialLayersMask();
		uint32 blend = pItemRP->GetMaterialLayersBlend();
		if (!fade)
		{
			blend = (blend & 0xffff00ff) | (enable?0xff00 : 0x0000);
		}
		else
		{
			blend = (blend & 0xffff00ff) | (enable?0x0000 : 0xff00);
		}

		if (enable)
			mask = mask|MTL_LAYER_CLOAK;
		else
			mask = mask&~MTL_LAYER_CLOAK;
		ApplyMaterialLayerSettings(mask, blend);
	}
	m_cloaked = enable;
}

void CItem::CloakSync(bool fade)
{
	// check if the actor is cloaked
	CActor* pOwner = GetOwnerActor();

	if(!pOwner)
		return;

	IEntityRenderProxy* pOwnerRP = (IEntityRenderProxy*) pOwner->GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	if (pOwnerRP)
	{
		uint8 ownerMask = pOwnerRP->GetMaterialLayersMask();
		uint32 ownerBlend = pOwnerRP->GetMaterialLayersBlend();
		bool isCloaked = (ownerMask&MTL_LAYER_CLOAK) != 0;
		CloakEnable(isCloaked, fade);
	}

	if (CItem* pSlave=static_cast<CItem*>(GetDualWieldSlave()))
		pSlave->CloakSync(fade);
}

void CItem::FrostEnable(bool enable, bool fade)
{
	IEntityRenderProxy * pItemRP = (IEntityRenderProxy*) GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	if (pItemRP)
	{
		uint8 mask = pItemRP->GetMaterialLayersMask();
		uint32 blend = pItemRP->GetMaterialLayersBlend();
		if (!fade)
		{
			blend = (blend & 0xffffff00) | (enable?0xff : 0x00);
		}

		if (enable)
			mask = mask|MTL_LAYER_DYNAMICFROZEN;
		else
			mask = mask&~MTL_LAYER_DYNAMICFROZEN;
		ApplyMaterialLayerSettings(mask, blend);
	}
}

void CItem::FrostSync(bool fade)
{
	// check if the actor is cloaked
	CActor* pOwner = GetOwnerActor();

	if(!pOwner)
		return;

	IEntityRenderProxy* pOwnerRP = (IEntityRenderProxy*) pOwner->GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	if (pOwnerRP)
	{
		uint8 ownerMask = pOwnerRP->GetMaterialLayersMask();
		uint32 ownerBlend = pOwnerRP->GetMaterialLayersBlend();
		bool isCloaked = (ownerMask&MTL_LAYER_DYNAMICFROZEN) != 0;
		FrostEnable(isCloaked, fade);
	}

	if (CItem* pSlave=static_cast<CItem*>(GetDualWieldSlave()))
		pSlave->FrostSync(fade);
}

void CItem::WetEnable(bool enable, bool fade)
{
	IEntityRenderProxy * pItemRP = (IEntityRenderProxy*) GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	if (pItemRP)
	{
		uint8 mask = pItemRP->GetMaterialLayersMask();
		uint32 blend = pItemRP->GetMaterialLayersBlend();
		if (!fade)
		{
			blend = (blend & 0xff00ffff) | (enable?0xff0000 : 0x000000);
		}

		if (enable)
			mask = mask|MTL_LAYER_WET;
		else
			mask = mask&~MTL_LAYER_WET;
		ApplyMaterialLayerSettings(mask, blend);
	}
}

void CItem::WetSync(bool fade)
{
	// check if the actor is cloaked
	CActor* pOwner = GetOwnerActor();

	if(!pOwner)
		return;

	IEntityRenderProxy* pOwnerRP = (IEntityRenderProxy*) pOwner->GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	if (pOwnerRP)
	{
		uint8 ownerMask = pOwnerRP->GetMaterialLayersMask();
		uint32 ownerBlend = pOwnerRP->GetMaterialLayersBlend();
		bool isCloaked = (ownerMask&MTL_LAYER_WET) != 0;
		WetEnable(isCloaked, fade);
	}

	if (CItem* pSlave=static_cast<CItem*>(GetDualWieldSlave()))
		pSlave->WetSync(fade);
}

void CItem::ApplyMaterialLayerSettings(uint8 mask, uint32 blend)
{
	IEntityRenderProxy * pItemRP = (IEntityRenderProxy*) GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	if (pItemRP)
	{ 
		pItemRP->SetMaterialLayersMask(mask);
		pItemRP->SetMaterialLayersBlend(blend);
	}

	for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); it++)
	{
		EntityId cur = (EntityId)it->second;
		IItem *attachment = m_pItemSystem->GetItem(cur);
		if (attachment)
		{
			pItemRP = (IEntityRenderProxy*) attachment->GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
			if (pItemRP)
			{
				pItemRP->SetMaterialLayersMask(mask);
				pItemRP->SetMaterialLayersBlend(blend);
			}
		}
	}
}


void CItem::SetMaterialRecursive(ICharacterInstance *charInst, bool undo, IMaterial *newMat)
{
	if (!charInst || (!undo && !newMat))
		return;

	if (undo)
		charInst->SetMaterial(NULL);
	else
	{
		IMaterial *oldMat = charInst->GetMaterial();
		if (newMat != oldMat)
		{
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
				if (!obj->GetICharacterInstance())
				{
					if (undo)
						obj->SetMaterial(NULL);
					else
					{
						IMaterial *oldMat = obj->GetMaterial();
						if (oldMat != newMat)
						{
							obj->SetMaterial(newMat);
						}
					}
				}
			}
		}
	}
}

void CItem::TakeAccessories(EntityId receiver)
{
	IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(receiver);
	IInventory *pInventory = GetActorInventory(pActor);

	if (pInventory)
	{
		for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); it++)
		{
			const EntityId id = it->second;
			const char* name = it->first;
			if (!pInventory->GetCountOfClass(name))
				g_pGame->GetIGameFramework()->GetIItemSystem()->GiveItem(pActor, name, false, false, false);
		}
	}
}

void CItem::AddAccessoryAmmoToInventory(IEntityClass* pAmmoType, int count, CActor *pOwner /*= NULL */)
{
	if(!pOwner)
		pOwner = GetOwnerActor();
	IInventory *pInventory=GetActorInventory(pOwner);
	if (!pInventory)
		return;

	int capacity = pInventory->GetAmmoCapacity(pAmmoType);
	int current = pInventory->GetAmmoCount(pAmmoType);
	if((!gEnv->pSystem->IsEditor()) && ((current+count) > capacity))
	{
		if(pOwner->IsClient())
			SAFE_HUD_FUNC(DisplayFlashMessage("@ammo_maxed_out", 2, ColorF(1.0f, 0,0), true, (string("@")+pAmmoType->GetName()).c_str()));

		//If still there's some place, full inventory to maximum...
		if(current<capacity)
		{
			pInventory->SetAmmoCount(pAmmoType,capacity);
			if (IsServer())
				pOwner->GetGameObject()->InvokeRMI(CActor::ClSetAmmo(), CActor::AmmoParams(pAmmoType->GetName(), capacity), eRMI_ToRemoteClients);
		}
	}
	else
	{
		int newCount = current+count;
		pInventory->SetAmmoCount(pAmmoType, newCount);

		if (IsServer())
			pOwner->GetGameObject()->InvokeRMI(CActor::ClSetAmmo(), CActor::AmmoParams(pAmmoType->GetName(), newCount), eRMI_ToRemoteClients);
		if(pOwner->IsClient())
		{
			/*char buffer[5];
			itoa(count, buffer, 10);
			SAFE_HUD_FUNC(DisplayFlashMessage("@grab_ammo", 3, Col_Wheat, true, (string("@")+pAmmoType->GetName()).c_str(), buffer));*/
			if(g_pGame->GetHUD())
				g_pGame->GetHUD()->DisplayAmmoPickup(pAmmoType->GetName(), count);
		}
	}
}

int CItem::GetAccessoryAmmoCount(IEntityClass* pAmmoType)
{
	IInventory *pInventory=GetActorInventory(GetOwnerActor());
	if (!pInventory)
		return 0;

	return pInventory->GetAmmoCount(pAmmoType);
}

bool CItem::CheckAmmoRestrictions(EntityId pickerId)
{
	if(g_pGameCVars->i_unlimitedammo != 0)
		return true;

	if(gEnv->pSystem->IsEditor())
		return true;

	IActor* pPicker = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pickerId);
	if(pPicker)
	{

		IInventory *pInventory = pPicker->GetInventory();
		if(pInventory)
		{
			if(pInventory->GetCountOfClass(GetEntity()->GetClass()->GetName()) == 0)
				return true;

			if(!m_bonusAccessoryAmmo.empty())
			{
				for (TAccessoryAmmoMap::const_iterator it=m_bonusAccessoryAmmo.begin(); it!=m_bonusAccessoryAmmo.end(); ++it)
				{
					int invAmmo  = pInventory->GetAmmoCount(it->first);
					int invLimit = pInventory->GetAmmoCapacity(it->first);

					if(invAmmo>=invLimit)
						return false;
				}
			}
		}
	}

	return true;
}

void CItem::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	m_params.GetMemoryStatistics(s);
	m_mountparams.GetMemoryStatistics(s);
	m_camerastats.GetMemoryStatistics(s);
	m_properties.GetMemoryStatistics(s);
	s->AddContainer(m_damageLevels);
	s->AddContainer(m_effects);
	s->AddContainer(m_activelayers);
	s->AddContainer(m_accessories);
	s->AddContainer(m_initialSetup);
	s->AddContainer(m_instanceActions);
	m_scheduler.GetMemoryStatistics(s);
	for (int i=0; i<eIGS_Last; i++)
		s->Add(m_idleAnimation[i]);
	s->Add(m_actionSuffix);
	for (int i=0; i<3; i++)
		m_fpgeometry[i].GetMemoryStatistics(s);
	for (int i=0; i<eIGS_Last; i++)
		s->Add(m_geometry[i]);

	for (TInitialSetup::iterator iter = m_initialSetup.begin(); iter != m_initialSetup.end(); ++iter)
		s->Add(*iter);
	for (TEffectInfoMap::iterator iter = m_effects.begin(); iter != m_effects.end(); ++iter)
		iter->second.GetMemoryStatistics(s);
	for (TInstanceActionMap::iterator iter = m_instanceActions.begin(); iter != m_instanceActions.end(); ++iter)
		iter->second.GetMemoryStatistics(s);
}


SItemStrings* g_pItemStrings = 0;

SItemStrings::~SItemStrings()
{
	g_pItemStrings = 0;
}

SItemStrings::SItemStrings()
{
	g_pItemStrings = this;

	activate = "activate";
	begin_reload = "begin_reload";
	cannon = "cannon";
	change_firemode = "change_firemode";
	change_firemode_zoomed = "change_firemode_zoomed";
	crawl = "crawl";
	deactivate = "deactivate";
	deselect = "deselect";
	destroy = "destroy";
	enter_modify = "enter_modify";
	exit_reload_nopump = "exit_reload_nopump";
	exit_reload_pump = "exit_reload_pump";
	fire = "fire";
	idle = "idle";
	idle_relaxed = "idle_relaxed";
	idle_raised = "idle_raised";
	jump_end = "jump_end";
	jump_idle = "jump_idle";
	jump_start = "jump_start";
	leave_modify = "leave_modify";
	left_item_attachment = "left_item_attachment";
	lock = "lock";
	lower = "lower";
	modify_layer = "modify_layer";
	nw = "nw";
	offhand_on = "offhand_on";
	offhand_off = "offhand_off";
	offhand_on_akimbo = "offhand_on_akimbo";
	offhand_off_akimbo = "offhand_off_akimbo";
	pickedup = "pickedup";
	pickup_weapon_left = "pickup_weapon_left";
	raise = "raise";
	reload_shell = "reload_shell";
	right_item_attachment = "right_item_attachment";
	run_forward = "run_forward";
	SCARSleepAmmo = "SCARSleepAmmo";
	SCARTagAmmo = "SCARTagAmmo";
	select = "select";
	select_grenade = "select_grenade";
	swim_idle = "swim_idle";
	swim_idle_2 = "swim_idle_underwater";
	swim_forward = "swim_forward";
	swim_forward_2 = "swim_forward_underwater";
	swim_backward = "swim_backward";
	speed_swim = "speed_swim";
	turret = "turret";  
  enable_light = "enable_light";
  disable_light = "disable_light";
  use_light = "use_light";
	first_select = "first_select";
	LAM = "LAM";
	LAMRifle = "LAMRifle";
	LAMFlashLight = "LAMFlashLight";
	LAMRifleFlashLight = "LAMRifleFlashLight";
	Silencer = "Silencer";
	SOCOMSilencer = "SOCOMSilencer";
	lever_layer_1 = "lever_layer_1";
	lever_layer_2 = "lever_layer_2";

};

