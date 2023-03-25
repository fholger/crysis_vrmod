/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 12:04:2006   17:22 : Created by Márcio Martins
- 18:02:2007	 13:30 : Refactored Offhand by Benito G.R.

*************************************************************************/

#include "StdAfx.h"
#include "OffHand.h"
#include "Actor.h"
#include "Throw.h"
#include "GameRules.h"
#include <IWorldQuery.h>
#include "Fists.h"
#include "GameActions.h"
#include "Melee.h"

#include "HUD/HUD.h"
#include "HUD/HUDCrosshair.h"
#include "WeaponSystem.h"
#include "Projectile.h"

#define KILL_NPC_TIMEOUT	7.25f
#define TIME_TO_UPDATE_CH 0.25f

#define MAX_CHOKE_SOUNDS	5

#define MAX_GRENADE_TYPES 4

#define INPUT_DEF     0
#define INPUT_USE			1
#define INPUT_LBM     2
#define INPUT_RBM			3

#define GRAB_TYPE_ONE_HANDED	0
#define GRAB_TYPE_TWO_HANDED  1
#define GRAB_TYPE_NPC					2

#define ITEM_NO_EXCHANGE			0
#define ITEM_CAN_PICKUP				1
#define ITEM_CAN_EXCHANGE			2

#define OFFHAND_RANGE					2.5f

//Sounds tables
namespace 
{
	const char gChokeSoundsTable[MAX_CHOKE_SOUNDS][64] =
	{
		"Languages/dialog/ai_korean_soldier_1/choke_01.mp3",
		"Languages/dialog/ai_korean_soldier_2/choke_02.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_03.mp3",
		"Languages/dialog/ai_korean_soldier_3/choke_04.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_05.mp3"
	};
	const char gDeathSoundsTable[MAX_CHOKE_SOUNDS][64] =
	{
		"Languages/dialog/ai_korean_soldier_1/choke_grab_00.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_grab_01.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_grab_02.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_grab_03.mp3",
		"Languages/dialog/ai_korean_soldier_1/choke_grab_04.mp3"
	};
}

//========================Scheduled offhand actions =======================//
namespace
{
	//This class help us to select the correct action
	class FinishOffHandAction
	{
	public:
		FinishOffHandAction(EOffHandActions _eOHA,COffHand *_pOffHand)
		{
			eOHA = _eOHA;
			pOffHand = _pOffHand;
		}
		void execute(CItem *cItem)
		{
			pOffHand->FinishAction(eOHA);
		}

	private:

		EOffHandActions eOHA;
		COffHand				*pOffHand;
	};

	//End finish grenade action (switch/throw)
	class FinishGrenadeAction
	{
	public:
		FinishGrenadeAction(COffHand *_pOffHand, CItem *_pMainHand)
		{
			pOffHand  = _pOffHand;
			pMainHand = _pMainHand;
		}
		void execute(CItem *cItem)
		{
			//pOffHand->HideItem(true);
			float timeDelay = 0.1f;	//ms

			if(pMainHand && !pMainHand->IsDualWield())
			{
				pMainHand->ResetDualWield();		//I can reset, because if DualWield it's not possible to switch grenades (see PreExecuteAction())
				pMainHand->PlayAction(g_pItemStrings->offhand_off, 0, false, CItem::eIPAF_Default|CItem::eIPAF_NoBlend);
				timeDelay = (pMainHand->GetCurrentAnimationTime(CItem::eIGS_FirstPerson) + 50)*0.001f;
			}
			else if(!pOffHand->GetOwnerActor()->ShouldSwim())
			{
				if(pMainHand && pMainHand->IsDualWield())
					pMainHand->Select(true);
				else
					pOffHand->GetOwnerActor()->HolsterItem(false);
			}

			if(pOffHand->GetOffHandState()==eOHS_SWITCHING_GRENADE)
			{
				int grenadeType = pOffHand->GetCurrentFireMode();
				pOffHand->AttachGrenadeToHand(grenadeType);
			}

			pOffHand->SetOffHandState(eOHS_TRANSITIONING);

			//Offhand goes back to initial state
			pOffHand->SetResetTimer(timeDelay);
			pOffHand->RequireUpdate(eIUS_General);
		}

	private:
		COffHand *pOffHand;
		CItem    *pMainHand;
	};
}

//=====================~Scheduled offhand actions======================//

TActionHandler<COffHand> COffHand::s_actionHandler;

COffHand::COffHand():
m_currentState(eOHS_INIT_STATE),
m_mainHand(NULL),
m_mainHandWeapon(NULL),
m_nextGrenadeThrow(-1.0f),
m_lastFireModeId(0),
m_range(OFFHAND_RANGE),
m_usable(false),
m_heldEntityId(0),
m_pickingTimer(-1.0f),
m_resetTimer(-1.0f),
m_preHeldEntityId(0),
m_grabbedNPCSpecies(eGCT_UNKNOWN),
m_killTimeOut(-1.0f),
m_killNPC(false),
m_effectRunning(false),
m_grabbedNPCInitialHealth(0),
m_npcWasDead(false),
m_lastCHUpdate(0.0f),
m_heldEntityMass(0.0f),
m_prevMainHandId(0),
m_checkForConstraintDelay(2),
m_startPickUp(false),
m_pRockRN(NULL),
m_bCutscenePlaying(false),
m_restoreStateAfterLoading(false)
{
	m_useFPCamSpacePP = false; //Offhand doesn't need it
	m_forceThrow = false;
	m_fGrenadeToggleTimer = -1.0f;
	m_fGrenadeThrowTimer = -1.0f;
	m_holdScale.Set(1.0f,1.0f,1.0f);

	RegisterActions();
}

//=============================================================
COffHand::~COffHand()
{
	if(m_heldEntityId)
	{
		DrawNear(false, m_heldEntityId);
		if(IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_heldEntityId))
		{
			if(ICharacterInstance* pChar = pEntity->GetCharacter(0))
				pChar->SetFlags(pChar->GetFlags()&(~ENTITY_SLOT_RENDER_NEAREST));

			if(IEntityRenderProxy* pProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER))
			{
				if(IRenderNode* pRenderNode = pProxy->GetRenderNode())
					pRenderNode->SetRndFlags(ERF_RENDER_ALWAYS,false);
			}
		}
	}
}

//============================================================
void COffHand::RegisterActions()
{
	if(s_actionHandler.GetNumHandlers() == 0)
	{
#define ADD_HANDLER(action, func) s_actionHandler.AddHandler(actions.action, &COffHand::func)
		const CGameActions& actions = g_pGame->Actions();

		ADD_HANDLER(use,OnActionUse);
		ADD_HANDLER(xi_use,OnActionUse);
		ADD_HANDLER(attack1,OnActionAttack);
		ADD_HANDLER(zoom,OnActionDrop);
		ADD_HANDLER(xi_zoom,OnActionDrop);
		ADD_HANDLER(grenade,OnActionThrowGrenade);
		ADD_HANDLER(xi_grenade,OnActionXIThrowGrenade);
		ADD_HANDLER(handgrenade,OnActionSwitchGrenade);
		ADD_HANDLER(xi_handgrenade,OnActionXISwitchGrenade);
		ADD_HANDLER(special,OnActionSpecial);
	#undef ADD_HANDLER
	}
}
//=============================================================
void COffHand::Reset()
{
	CWeapon::Reset();

	if(m_heldEntityId)
	{
		//Prevent editor-reset issues
		if(m_currentState&(eOHS_GRABBING_NPC|eOHS_HOLDING_NPC|eOHS_THROWING_NPC))
		{
			ThrowNPC(false);
		}
		else if(m_currentState&(eOHS_PICKING|eOHS_HOLDING_OBJECT|eOHS_THROWING_OBJECT|eOHS_MELEE))
		{
			IgnoreCollisions(false,m_heldEntityId);
			DrawNear(false,m_heldEntityId);
		}
	}

	m_nextGrenadeThrow = -1.0f;
	m_lastFireModeId   = 0;
	m_pickingTimer     = -1.0f;
	m_heldEntityId		 = 0;
	m_preHeldEntityId  = 0;
	m_constraintId     = 0;
	m_resetTimer			 = -1.0f;
	m_killTimeOut			 = -1.0f;
	m_killNPC					 = false;
	m_effectRunning    = false;
	m_npcWasDead       = false;
	m_grabbedNPCSpecies = eGCT_UNKNOWN;
	m_lastCHUpdate			= 0.0f;
	m_heldEntityMass		= 0.0f;
	m_prevMainHandId	= 0;
	m_checkForConstraintDelay = 2;
	m_pRockRN = NULL;
	m_bCutscenePlaying = false;
	m_forceThrow = false;
	m_restoreStateAfterLoading = false;

	DrawSlot(eIGS_Aux0,false);
	DrawSlot(eIGS_Aux1,false);
		
	SetOffHandState(eOHS_INIT_STATE);

	for(int i=0; i<eOHSound_LastSound; i++)
	{
		m_sounds[i] = INVALID_SOUNDID;
	}
}

//=============================================================
void COffHand::PostInit(IGameObject *pGameObject)
{
	CWeapon::PostInit(pGameObject);

	m_lastFireModeId = 0;
	SetCurrentFireMode(0);
	HideItem(true);
	//EnableUpdate(true,eIUS_General);
}

//============================================================
bool COffHand::ReadItemParams(const IItemParamsNode *root)
{
	if (!CWeapon::ReadItemParams(root))
		return false;

	m_grabTypes.clear();

	//Read offHand grab types
	if (const IItemParamsNode* pickabletypes = root->GetChild("pickabletypes"))
	{
		int n = pickabletypes->GetChildCount();
		for (int i = 0; i<n; ++i)
		{
			const IItemParamsNode* pt = pickabletypes->GetChild(i);

			SGrabType grabType;
			grabType.helper = pt->GetAttribute("helper");
			grabType.pickup = pt->GetAttribute("pickup");
			grabType.idle = pt->GetAttribute("idle");
			grabType.throwFM = pt->GetAttribute("throwFM");

			if (strcmp(pt->GetName(), "onehanded") == 0)
			{
				grabType.twoHanded = false;
				m_grabTypes.push_back(grabType);
			}
			else if(strcmp(pt->GetName(), "twohanded") == 0)
			{
				grabType.twoHanded = true;
				m_grabTypes.push_back(grabType);
			}
		}
	}

	return true;
}
//============================================================
void COffHand::FullSerialize(TSerialize ser)
{
	CWeapon::FullSerialize(ser);

	EntityId oldHeldId = m_heldEntityId;

	ser.Value("m_lastFireModeId", m_lastFireModeId);
	ser.Value("m_usable", m_usable);
	ser.Value("m_currentState", m_currentState);
	ser.Value("m_preHeldEntityId", m_preHeldEntityId);
	ser.Value("m_startPickUp", m_startPickUp);
	ser.Value("m_heldEntityId", m_heldEntityId);
	ser.Value("m_constraintId", m_constraintId);
	ser.Value("m_grabType", m_grabType);
	ser.Value("m_grabbedNPCSpecies", m_grabbedNPCSpecies);
	ser.Value("m_killTimeOut", m_killTimeOut);
	ser.Value("m_effectRunning",m_effectRunning);
	ser.Value("m_grabbedNPCInitialHealth",m_grabbedNPCInitialHealth);
	ser.Value("m_prevMainHandId",m_prevMainHandId);
	ser.Value("m_holdScale",m_holdScale);

	//PATCH ====================
	if(ser.IsReading())
	{
		m_mainHandIsDualWield = false;
	}

	ser.Value("m_mainHandIsDualWield", m_mainHandIsDualWield);

	//============================

	if(ser.IsReading() && m_heldEntityId != oldHeldId)
	{
		IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(oldHeldId);
		if(pActor)
		{
			if(pActor->GetEntity()->GetCharacter(0))
				pActor->GetEntity()->GetCharacter(0)->SetFlags(pActor->GetEntity()->GetCharacter(0)->GetFlags()&(~ENTITY_SLOT_RENDER_NEAREST));
			if(IEntityRenderProxy* pProxy = (IEntityRenderProxy*)pActor->GetEntity()->GetProxy(ENTITY_PROXY_RENDER))
			{
				if(IRenderNode* pRenderNode = pProxy->GetRenderNode())
					pRenderNode->SetRndFlags(ERF_RENDER_ALWAYS,false);
			}
		}
		else
			DrawNear(false, oldHeldId);
	}
}

//============================================================
bool COffHand::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	return true;
}

//============================================================
void COffHand::PostSerialize()
{
	m_restoreStateAfterLoading = true; //Will be cleared on first update
}

//==========================================================
void COffHand::PostPostSerialize()
{
	RemoveAllAccessories();

	SetCurrentFireMode(m_lastFireModeId);

	bool needsReset = false;

	if(m_currentState&(eOHS_THROWING_OBJECT|eOHS_HOLDING_GRENADE|eOHS_THROWING_GRENADE|eOHS_TRANSITIONING))
	{
		needsReset = true;
	}
	else if(m_currentState==eOHS_SWITCHING_GRENADE)
	{
		m_currentState = eOHS_INIT_STATE;
		ProcessOffHandActions(eOHA_SWITCH_GRENADE,INPUT_DEF,eAAM_OnPress,1.0f);
	}
	else if(m_currentState==eOHS_PICKING_ITEM)
	{
		//If picking an item...
		if(m_heldEntityId || (m_preHeldEntityId && m_startPickUp))
		{
			if(!m_heldEntityId)
				m_heldEntityId = m_preHeldEntityId;

			SelectGrabType(gEnv->pEntitySystem->GetEntity(m_heldEntityId));

			CActor* pPlayer = GetOwnerActor();
			if(pPlayer)
			{
				m_currentState = eOHS_PICKING_ITEM2;
				pPlayer->PickUpItem(m_heldEntityId,true);
				IgnoreCollisions(false,m_heldEntityId);
			}
		}
		SetOffHandState(eOHS_INIT_STATE);		
	}
	else if((m_heldEntityId || m_preHeldEntityId) && m_currentState!=eOHS_TRANSITIONING)
	{
		if(m_preHeldEntityId && !m_heldEntityId && m_startPickUp)
		{
			m_currentState = eOHS_PICKING;
			m_heldEntityId = m_preHeldEntityId;
		} 

		if(m_heldEntityId)
		{
			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_heldEntityId);
			if(!pEntity)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Offhand held entity did not exist anymore! Perhaps it was a boid ... ");
				needsReset = true;
			}
			else
			{
				SelectGrabType(gEnv->pEntitySystem->GetEntity(m_heldEntityId));

				//If holding an object or NPC
				if(m_currentState&(eOHS_HOLDING_OBJECT|eOHS_PICKING|eOHS_THROWING_OBJECT|eOHS_MELEE))
				{
					IgnoreCollisions(false, m_heldEntityId);
					DrawNear(false);

					//Do grabbing again
					m_currentState = eOHS_INIT_STATE;
					m_preHeldEntityId = m_heldEntityId;
					PreExecuteAction(eOHA_USE,eAAM_OnPress,true);
					PickUpObject();
				}
				else if(m_currentState&(eOHS_HOLDING_NPC|eOHS_GRABBING_NPC|eOHS_THROWING_NPC))
				{
					//Do grabbing again
					m_currentState = eOHS_INIT_STATE;
					CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_heldEntityId));
					bool isDead = false;
					if(pActor && ((pActor->GetActorStats() && pActor->GetActorStats()->isRagDoll) || pActor->GetHealth() <= 0))
						isDead = true;
					if(isDead)//don't pickup ragdolls
					{
						pActor->Revive(); //will be ragdollized after serialization...
						pActor->SetAnimationInput( "Action", "idle" );
						pActor->CreateScriptEvent("kill",0);
						needsReset = true;
					}
					else
					{
						m_preHeldEntityId = m_heldEntityId;
						PreExecuteAction(eOHA_USE,eAAM_OnPress,true);
						PickUpObject(true);
						if(pActor)
							pActor->GetAnimatedCharacter()->ForceRefreshPhysicalColliderMode();
					}
				}
			}
		}

	}
	else if(m_currentState!=eOHS_INIT_STATE)
	{
		needsReset = true;
	}

	if(needsReset)
	{
		m_mainHand = static_cast<CItem*>(GetOwnerActor()->GetCurrentItem());
		m_mainHandWeapon = m_mainHand? static_cast<CWeapon*>(m_mainHand->GetIWeapon()):NULL;
		m_mainHandIsDualWield = m_mainHand?m_mainHand->IsDualWield():false;
		m_currentState = eOHS_TRANSITIONING;
		FinishAction(eOHA_RESET);
	}

	if(m_ownerId == LOCAL_PLAYER_ENTITY_ID)
	{
		if(!m_stats.selected)
		{
			SetHand(eIH_Left);					//This will be only done once after loading
			Select(true);	//this can select the wrong crosshair, should be removed if save
			Select(false);		
		}
	}

	m_restoreStateAfterLoading = false;
}

//=============================================================
bool COffHand::CanSelect() const
{
	return false;
}

//=============================================================
void COffHand::Select(bool select)
{
	CWeapon::Select(select);
}

//=============================================================
void COffHand::Update(SEntityUpdateContext &ctx, int slot)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );

	bool keepUpdating = false;

	CWeapon::Update(ctx, slot);

	if (slot==eIUS_General)
	{
		if(m_resetTimer>=0.0f)
		{
			m_resetTimer -= ctx.fFrameTime;
			if(m_resetTimer<0.0f)
			{
				SetOffHandState(eOHS_INIT_STATE);
				m_resetTimer = -1.0f;
			}
			else
			{
				keepUpdating = true;
			}
		}
		if (m_nextGrenadeThrow>=0.0f)
		{
			//Grenade throw fire rate
			m_nextGrenadeThrow -= ctx.fFrameTime;
			keepUpdating = true;
		}
		if(m_fGrenadeToggleTimer>=0.0f)
		{
			m_fGrenadeToggleTimer+=ctx.fFrameTime;
			if(m_fGrenadeToggleTimer>1.0f)
			{
				StartSwitchGrenade(true);
				m_fGrenadeToggleTimer = 0.0f;
			}
			keepUpdating = true;
		}

		if(m_fGrenadeThrowTimer>=0.0f)
		{
			m_fGrenadeThrowTimer+=ctx.fFrameTime;
			if(m_fGrenadeThrowTimer>0.5f)
			{
				m_fGrenadeThrowTimer = -1.0f;
			}
			else
			{
				keepUpdating = true;
			}
		}

		if(m_pickingTimer>=0.0f)
		{
			m_pickingTimer -= ctx.fFrameTime;

			if(m_pickingTimer<0.0f)
			{
				PerformPickUp();
			}
			else
			{
				keepUpdating = true;
			}
		}
		if(m_killTimeOut>=0.0f)
		{
			m_killTimeOut -= ctx.fFrameTime;
			if(m_killTimeOut<0.0f)
			{
				m_killTimeOut = -1.0f;
				m_killNPC = true;
			}
			else
			{
				keepUpdating = true;
			}
		}

		if(keepUpdating)
			RequireUpdate(eIUS_General);
		else
			EnableUpdate(false,eIUS_General);
	}
}

//=============================================================
void COffHand::UpdateFPView(float frameTime)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );
	
	if (m_stats.selected)
	{
		CItem::UpdateFPView(frameTime);

		if (m_fm)
			m_fm->UpdateFPView(frameTime);

		CheckViewChange();
	}

	if(m_restoreStateAfterLoading)
		PostPostSerialize();

	m_lastCHUpdate += frameTime;

	if(m_currentState==eOHS_INIT_STATE)
	{
		if(!gEnv->bMultiplayer)
			UpdateCrosshairUsabilitySP();
		else
			UpdateCrosshairUsabilityMP();

		m_weaponLowered = false;
		
		//Fix offhand floating on spawn (not really nice fix...)
		if(m_stats.hand==0)
		{
 			SetHand(eIH_Left);					//This will be only done once after loading
			Select(true);Select(false);
		}
		//=========================================================
	}
	else if(m_heldEntityId && m_currentState&(eOHS_HOLDING_OBJECT|eOHS_PICKING|eOHS_THROWING_OBJECT|eOHS_PICKING_ITEM|eOHS_MELEE))
	{

		if (m_usable)
		{
			if (g_pGame->GetHUD())
				g_pGame->GetHUD()->GetCrosshair()->SetUsability(false, "");
			m_usable=false;
		}

		UpdateHeldObject();

		if(m_grabType==GRAB_TYPE_TWO_HANDED)
			UpdateWeaponLowering(frameTime);
		else if(CActor* pActor = GetOwnerActor())
		{
			if(CItem* pItem=static_cast<CItem*>(pActor->GetCurrentItem()))
			{
				if(CWeapon*pWeapon = static_cast<CWeapon*>(pItem->GetIWeapon()))
					LowerWeapon(pWeapon->IsWeaponLowered());
			}

		}
	}
}

//============================================================

void COffHand::UpdateCrosshairUsabilitySP()
{

	//Only update a few times per second
	if(m_lastCHUpdate>TIME_TO_UPDATE_CH)
		m_lastCHUpdate = 0.0f;
	else
		return;

	CActor *pActor=GetOwnerActor();
	if (pActor)
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
		bool isLadder = pPlayer->IsLadderUsable();

		bool onLadder = false;
		if(SPlayerStats* pStats = static_cast<SPlayerStats*>(pPlayer->GetActorStats()))
			onLadder = pStats->isOnLadder;

		if (CanPerformPickUp(pActor, NULL) || (isLadder && !onLadder))
		{
			if (CHUD* pHUD = g_pGame->GetHUD())
			{
				IItem *pItem = m_pItemSystem->GetItem(m_crosshairId);
				if(IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_crosshairId))
					pHUD->GetCrosshair()->SetUsability(1, "@grab_enemy");
				else if(isLadder)
					pHUD->GetCrosshair()->SetUsability(1, "@use_ladder");
				else if(!pItem)
					pHUD->GetCrosshair()->SetUsability(1, "@grab_object");
				else if(pItem)
				{
					CryFixedStringT<128> itemName = "@";
					itemName.append(pItem->GetEntity()->GetClass()->GetName());
					if(!strcmp(pItem->GetEntity()->GetClass()->GetName(), "CustomAmmoPickup"))
					{
						SmartScriptTable props;
						if (pItem->GetEntity()->GetScriptTable() && pItem->GetEntity()->GetScriptTable()->GetValue("Properties", props))
						{
							const char *name = NULL;
							props->GetValue("AmmoName", name);
							itemName = "@";
							itemName.append(name);
						}
					}

					if(pItem->GetIWeapon())
					{
						IEntityClass *pItemClass = pItem->GetEntity()->GetClass();
						bool isSocom = strcmp(pItemClass->GetName(),"SOCOM")?false:true;
						IItem *pCurrentItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pPlayer->GetInventory()->GetItemByClass(pItemClass));
						if((!isSocom && pCurrentItem) ||
								(isSocom && pCurrentItem && pCurrentItem->IsDualWield()))
						{
							if(pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
								pHUD->GetCrosshair()->SetUsability(true, "@game_take_ammo_from", itemName.c_str());
							else
								pHUD->GetCrosshair()->SetUsability(2, "@weapon_ammo_full", itemName.c_str());
						}
						else
						{
							int typ = CanPerformPickUp(GetOwnerActor(),NULL,true);
							if(m_preHeldEntityId && !pPlayer->CheckInventoryRestrictions(m_pEntitySystem->GetEntity(m_preHeldEntityId)->GetClass()->GetName()))
							{
								IItem *pExchangedItem = GetExchangeItem(pPlayer);
								if(pExchangedItem)
								{
									pHUD->GetCrosshair()->SetUsability(true, "@game_exchange_weapon",
										pExchangedItem->GetEntity()->GetClass()->GetName(), itemName.c_str());
								}
								else
									pHUD->GetCrosshair()->SetUsability(2, "@inventory_full");
							}
							else
							{
								pHUD->GetCrosshair()->SetUsability(true, "@pick_weapon", itemName.c_str());
							}
						}
					}
					else
					{
						if(pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
							pHUD->GetCrosshair()->SetUsability(1, "@pick_item", itemName.c_str());
						else
							pHUD->GetCrosshair()->SetUsability(2, "@weapon_ammo_full", itemName.c_str());
					}
				}
			}
			m_usable=true;
		}
		else if (m_usable)
		{
			if (g_pGame->GetHUD())
				g_pGame->GetHUD()->GetCrosshair()->SetUsability(false, "");
			m_usable=false;
		}
	}
}

//==========================================================
void COffHand::UpdateCrosshairUsabilityMP()
{
	//Only update a few times per second
	if(m_lastCHUpdate>TIME_TO_UPDATE_CH)
		m_lastCHUpdate = 0.0f;
	else
		return;

	CActor *pActor=GetOwnerActor();
	if (pActor)
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
		bool isLadder = pPlayer->IsLadderUsable();

		bool onLadder = false;
		if(SPlayerStats* pStats = static_cast<SPlayerStats*>(pPlayer->GetActorStats()))
			onLadder = pStats->isOnLadder;

		IMovementController* pMC = pActor->GetMovementController();
		if(!pMC)
			return;
	
		SMovementState info;
		pMC->GetMovementState(info);
		if (CheckItemsInProximity(info.eyePosition,info.eyeDirection,false) || (isLadder && !onLadder))
		{
			//Offhand pick ups are disabled in MP (check only for items)
			if(CHUD* pHUD = g_pGame->GetHUD())
			{
				if(isLadder)
				{
					pHUD->GetCrosshair()->SetUsability(1, "@use_ladder");
					m_usable=true;
					return;
				}
				
				if(CItem *pItem = static_cast<CItem*>(m_pItemSystem->GetItem(m_crosshairId)))
				{
					CryFixedStringT<128> itemName = pItem->GetEntity()->GetClass()->GetName();
					if(!strcmp(itemName.c_str(), "CustomAmmoPickup"))
					{
						SmartScriptTable props;
						if (pItem->GetEntity()->GetScriptTable() && pItem->GetEntity()->GetScriptTable()->GetValue("Properties", props))
						{
							const char *name = NULL;
							props->GetValue("AmmoName", name);
							itemName = name;
						}
					}

					if(pItem->GetIWeapon())
					{
						IEntityClass *pItemClass = pItem->GetEntity()->GetClass();
						bool isSocom = strcmp(pItemClass->GetName(),"SOCOM")?false:true;
						IItem *pCurrentItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pPlayer->GetInventory()->GetItemByClass(pItemClass));
						if((!isSocom && pCurrentItem) ||
							(isSocom && pCurrentItem && pCurrentItem->IsDualWield()))
						{
							if(pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
								pHUD->GetCrosshair()->SetUsability(true, "@game_take_ammo_from", itemName.c_str());
							else
								pHUD->GetCrosshair()->SetUsability(2, "@weapon_ammo_full", itemName.c_str());
						}
						else
						{
							int typ = CanPerformPickUp(GetOwnerActor(),NULL,true);
							if(m_preHeldEntityId && !pPlayer->CheckInventoryRestrictions(m_pEntitySystem->GetEntity(m_preHeldEntityId)->GetClass()->GetName()))
							{
								IItem *pExchangedItem = GetExchangeItem(pPlayer);
								if(pExchangedItem)
								{
									pHUD->GetCrosshair()->SetUsability(true, "@game_exchange_weapon",
										pExchangedItem->GetEntity()->GetClass()->GetName(), itemName.c_str());
								}
								else
									pHUD->GetCrosshair()->SetUsability(2, "@inventory_full");
							}
							else
							{
								pHUD->GetCrosshair()->SetUsability(true, "@pick_weapon", itemName.c_str());
							}
						}
					}
					else
					{
						if(pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
							pHUD->GetCrosshair()->SetUsability(1, "@pick_item", itemName.c_str());
						else
							pHUD->GetCrosshair()->SetUsability(2, "@weapon_ammo_full", itemName.c_str());
					}
				}
			}
			m_usable=true;
		}
		else if (m_usable)
		{
			if (g_pGame->GetHUD())
				g_pGame->GetHUD()->GetCrosshair()->SetUsability(false, "");
			m_usable=false;
		}
	}
}

//=============================================================
void COffHand::UpdateHeldObject()
{
	IEntity *pEntity=m_pEntitySystem->GetEntity(m_heldEntityId);
	if(pEntity)
	{
		if(m_checkForConstraintDelay>=0)
			m_checkForConstraintDelay --;

		IPhysicalEntity* pPE = pEntity->GetPhysics();
		if((pPE && m_constraintId && m_checkForConstraintDelay<0) ||
			 (pPE && !m_constraintId && pPE->GetType()==PE_ARTICULATED))
		{
			pe_status_constraint state;
			state.id = m_constraintId;

			//The constraint was removed (the object was broken/destroyed)
			//m_constraintId == 0 means a boid that died in player hands (and now it's a ragdoll,which could cause collision issues)
			if(!m_constraintId || !pPE->GetStatus(&state))
			{
				if(m_mainHand && m_mainHand->IsBusy())
					m_mainHand->SetBusy(false);
			
				if(m_currentState!=eOHS_THROWING_OBJECT)
				{
					if(m_currentState==eOHS_MELEE)
						GetScheduler()->Reset();
					m_currentState = eOHS_HOLDING_OBJECT;
					OnAction(GetOwnerId(),"use",eAAM_OnPress,0.0f);
					OnAction(GetOwnerId(),"use",eAAM_OnRelease,0.0f);
				}
				else
				{
					OnAction(GetOwnerId(),"use",eAAM_OnRelease,0.0f);
				}
				m_constraintId = 0;
				return;
			}

		}

		//Update entity WorldTM 
		int id=m_stats.fp?eIGS_FirstPerson:eIGS_ThirdPerson;

		Matrix34 finalMatrix(Matrix34(GetSlotHelperRotation(id, "item_attachment", true)));
		finalMatrix.Scale(m_holdScale);	
		finalMatrix.SetTranslation(GetSlotHelperPos(id, "item_attachment", true));
		finalMatrix = finalMatrix * m_holdOffset;

		//This is need it for breakable/joint-constraints stuff
		if(IPhysicalEntity* pPhys = pEntity->GetPhysics())
		{
			pe_action_set_velocity v;
			v.v = Vec3(0.01f,0.01f,0.01f);
			pPhys->Action(&v);

			//For boids
			if(pPhys->GetType()==PE_PARTICLE)
			{
				pEntity->SetSlotLocalTM(0,IDENTITY);
			}

		}
		//====================================
		pEntity->SetWorldTM(finalMatrix,ENTITY_XFORM_USER);

	}
}

//===========================================================
void COffHand::UpdateGrabbedNPCState()
{
	//Get actor
	CActor  *pActor  = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_heldEntityId));
	CActor  *pPlayer = GetOwnerActor();
	SActorStats *pStats = pPlayer?pPlayer->GetActorStats():NULL;

	if(pActor && pStats)
	{
		RunEffectOnGrabbedNPC(pActor);

		// workaround for playing a different facial animation after 5 seconds
		pStats->grabbedTimer += gEnv->pTimer->GetFrameTime();
		if ( m_grabbedNPCSpecies == eGCT_HUMAN && pStats->grabbedTimer > 5.2f )
			pActor->SetAnimationInput( "Action", "grabStruggleFP2" );
		else
			pActor->SetAnimationInput( "Action", "grabStruggleFP" );

		//Actor died while grabbed
		if((pActor->GetHealth()<= 0 && !m_npcWasDead)||pActor->IsFallen()||(pStats->isRagDoll))
		{
			if(m_mainHand && m_mainHand->IsBusy())
				m_mainHand->SetBusy(false);
			//Already throwing (do nothing)
			if(m_currentState&(eOHS_GRABBING_NPC|eOHS_HOLDING_NPC))
			{
				//Drop NPC
				m_currentState = eOHS_HOLDING_NPC;
				OnAction(GetOwnerId(),"use",eAAM_OnPress,0.0f);
				OnAction(GetOwnerId(),"use",eAAM_OnRelease,0.0f);
			}
			else if(m_currentState&eOHS_THROWING_NPC)
			{
				OnAction(GetOwnerId(),"use",eAAM_OnRelease,0.0f);
			}
			
			if(!pStats->isRagDoll)
				m_npcWasDead = true;

		}
	}

}

//============================================================
void COffHand::PostFilterView(struct SViewParams &viewParams)
{
	//This should be only be called when grabbing/holding/throwing an NPC from CPlayer::PostUpdateView()
	IEntity *pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);

	UpdateGrabbedNPCState();
	UpdateGrabbedNPCWorldPos(pEntity, &viewParams);
}

//===============================================
void COffHand::PostUpdate(float frameTime)
{
	//Update character position here when the game is paused, if I don't do so, character goes invisible!
	//IGameObject::PostUpdate() is updated when the game is paused.
	//PostUpdated are enabled/disabled when grabbing/throwing a character
	if(g_pGame->GetIGameFramework()->IsGamePaused() && m_currentState&(eOHS_GRABBING_NPC|eOHS_HOLDING_NPC|eOHS_THROWING_NPC))
	{
		//This should be only be called when grabbing/holding/throwing an NPC from CPlayer::PostUpdateView()
		IEntity *pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);

		//UpdateGrabbedNPCState();
		UpdateGrabbedNPCWorldPos(pEntity, NULL);
	}

}

//============================================================
void COffHand::UpdateGrabbedNPCWorldPos(IEntity *pEntity, struct SViewParams *viewParams)
{
	if(pEntity)
	{
		Matrix34 neckFinal = Matrix34::CreateIdentity();

		if(viewParams)
		{
			SPlayerStats *stats = static_cast<SPlayerStats*>(GetOwnerActor()->GetActorStats());
			Quat wQuat = (viewParams->rotation*Quat::CreateRotationXYZ(stats->FPWeaponAnglesOffset * gf_PI/180.0f));
			wQuat *= Quat::CreateSlerp(viewParams->currentShakeQuat,IDENTITY,0.5f);
			wQuat.Normalize();

			Vec3 itemAttachmentPos  = GetSlotHelperPos(0,"item_attachment",false);
			itemAttachmentPos = stats->FPWeaponPos + wQuat * itemAttachmentPos;

			neckFinal.SetRotation33(Matrix33(viewParams->rotation*Quat::CreateRotationZ(gf_PI)));
			neckFinal.SetTranslation(itemAttachmentPos);

			ICharacterInstance *pCharacter=pEntity->GetCharacter(0);
			assert(pCharacter && "COffHand::UpdateGrabbedNPCWorldPos --> Actor entity has no character!!");
			if(!pCharacter)
				return;
			ISkeletonPose *pSkeletonPose=pCharacter->GetISkeletonPose();
			assert(pSkeletonPose && "COffHand::UpdateGrabbedNPCWorldPos --> Actor entity has no skeleton!!");
			if(!pSkeletonPose)
				return;

			int neckId = 0;
			Vec3 specialOffset(0.0f,-0.07f,-0.09f);

			switch(m_grabbedNPCSpecies)
			{
			case eGCT_HUMAN:  neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
				specialOffset.Set(0.0f,0.0f,0.0f);
				break;

			case eGCT_ALIEN:  neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
				specialOffset.Set(0.0f,0.0f,-0.09f);
				break;

			case eGCT_TROOPER: neckId = pSkeletonPose->GetJointIDByName("Bip01 Head");
				break;
			}

			Vec3 neckLOffset(pSkeletonPose->GetAbsJointByID(neckId).t);
			//Vec3 charOffset(pEntity->GetSlotLocalTM(0,false).GetTranslation());
			//if(m_grabbedNPCSpecies==eGCT_TROOPER)
			//charOffset.Set(0.0f,0.0f,0.0f);
			//Vec3 charOffset(0.0f,0.0f,0.0f);		//For some reason the above line didn't work with the trooper...

			//float white[4] = {1,1,1,1};
			//gEnv->pRenderer->Draw2dLabel( 100, 50, 2, white, false, "neck: %f %f %f", neckLOffset.x,neckLOffset.y,neckLOffset.z );
			//gEnv->pRenderer->Draw2dLabel( 100, 70, 2, white, false, "char: %f %f %f", charOffset.x,charOffset.y,charOffset.z );

			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(neckFinal.GetTranslation(),0.08f,ColorB(255,0,0));

			neckFinal.AddTranslation(Quat(neckFinal)*-(neckLOffset+specialOffset));
			m_lastNPCMatrix = neckFinal;
		}
		else
		{
			Vec3 itemAttachmentPos  = GetSlotHelperPos(0,"item_attachment",true);
			neckFinal = m_lastNPCMatrix;
			neckFinal.SetTranslation(itemAttachmentPos);

			if(ICharacterInstance *pCharacter=pEntity->GetCharacter(0))
			{
				ISkeletonPose *pSkeletonPose=pCharacter->GetISkeletonPose();
				assert(pSkeletonPose && "COffHand::UpdateGrabbedNPCWorldPos --> Actor entity has no skeleton!!");
				if(!pSkeletonPose)
					return;

				int neckId = 0;
				Vec3 specialOffset(0.0f,-0.07f,-0.09f);

				switch(m_grabbedNPCSpecies)
				{
					case eGCT_HUMAN:  neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
						specialOffset.Set(0.0f,0.0f,0.0f);
						break;

					case eGCT_ALIEN:  neckId = pSkeletonPose->GetJointIDByName("Bip01 Neck");
						specialOffset.Set(0.0f,0.0f,-0.09f);
						break;

					case eGCT_TROOPER: neckId = pSkeletonPose->GetJointIDByName("Bip01 Head");
						break;
				}

				Vec3 neckLOffset(pSkeletonPose->GetAbsJointByID(neckId).t);
				neckFinal.AddTranslation(Quat(neckFinal)*-(neckLOffset+specialOffset));
			}
			
		}

		float EntRotZ = RAD2DEG(Quat(neckFinal).GetRotZ());

		pEntity->SetWorldTM(neckFinal);
		
		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(neckFinal.GetTranslation(),0.08f,ColorB(0,255,0));
	}
}

//=============================================================
void COffHand::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	s_actionHandler.Dispatch(this,actorId,actionId,activationMode,value);
}

//-------------
bool COffHand::OnActionUse(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_USE,INPUT_USE,activationMode);
}

//------------
bool COffHand::OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_USE,INPUT_LBM,activationMode);
}

//------------
bool COffHand::OnActionDrop(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_USE,INPUT_RBM,activationMode);
}

//------------
bool COffHand::OnActionThrowGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_THROW_GRENADE,INPUT_DEF,activationMode);
}

//------------
bool COffHand::OnActionXIThrowGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(activationMode==eAAM_OnPress)
	{
		m_fGrenadeThrowTimer = 0.0f;
		RequireUpdate(eIUS_General);
	}
	else if(activationMode==eAAM_OnRelease)
	{
		if(m_fGrenadeThrowTimer>=0.0f)
		{
			ProcessOffHandActions(eOHA_THROW_GRENADE,INPUT_DEF,eAAM_OnPress);
			ProcessOffHandActions(eOHA_THROW_GRENADE,INPUT_DEF,eAAM_OnRelease);
			m_fGrenadeThrowTimer = -1.0f;
		}
	}
	return true;
}

//-----------
bool COffHand::OnActionSwitchGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_SWITCH_GRENADE,INPUT_DEF,activationMode);
}

//-----------
bool COffHand::OnActionXISwitchGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if(activationMode==eAAM_OnPress)
	{
		m_fGrenadeToggleTimer = 0.0f;
		RequireUpdate(eIUS_General);
	}
	else if(activationMode==eAAM_OnRelease)
	{
		m_fGrenadeToggleTimer = -1.0f;
	}
	return true;
}

//-----------
bool COffHand::OnActionSpecial(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	return ProcessOffHandActions(eOHA_MELEE_ATTACK,INPUT_DEF,activationMode);
}

//----------
bool COffHand::ProcessOffHandActions(EOffHandActions eOHA, int input, int activationMode, float value /*= 0.0f*/)
{
	//Test if action is possible
	if(!EvaluateStateTransition(eOHA,activationMode,input))
		return false;

	if(!PreExecuteAction(eOHA,activationMode))
		return false;

	if(eOHA==eOHA_SWITCH_GRENADE)
	{
		StartSwitchGrenade(false,(value!=0.0f)?true:false);
	}
	else if(eOHA==eOHA_THROW_GRENADE)
	{
		if (!m_fm->OutOfAmmo() && GetFireModeIdx(m_fm->GetName())<MAX_GRENADE_TYPES)
		{
			PerformThrow(activationMode,0,GetCurrentFireMode());
		}
	}
	else if(eOHA==eOHA_MELEE_ATTACK)
	{
		MeleeAttack();
	}
	else if(eOHA==eOHA_USE)
	{
		if(m_currentState&eOHS_INIT_STATE)
		{
			int typ = CanPerformPickUp(GetOwnerActor(),NULL,true);
			if(typ==OH_GRAB_ITEM)
			{
				StartPickUpItem();
			}
			else if(typ==OH_GRAB_OBJECT)
			{
				PickUpObject();
			}
			else if(typ==OH_GRAB_NPC)
			{
				PickUpObject(true);
			}
			else if(typ==OH_NO_GRAB)
			{
				CancelAction();
				return false;
			}

		}
		else if(m_currentState&(eOHS_HOLDING_OBJECT|eOHS_THROWING_OBJECT))
		{
			ThrowObject(activationMode);
		}
		else if(m_currentState&(eOHS_HOLDING_NPC|eOHS_THROWING_NPC))
		{
			ThrowObject(activationMode,true);
		}
	}

	return true;
}

//==================================================================
bool COffHand::EvaluateStateTransition(int requestedAction, int activationMode, int	inputMethod)
{
	switch(requestedAction)
	{
		case eOHA_SWITCH_GRENADE: if(activationMode==eAAM_OnPress && m_currentState==eOHS_INIT_STATE)
															{
																return true;
															}
															break;

		case eOHA_THROW_GRENADE:	if(activationMode==eAAM_OnPress && m_currentState==eOHS_INIT_STATE)
															{
																//Don't allow throwing grenades under water.
																if(CPlayer* pPlayer = static_cast<CPlayer*>(GetOwnerActor()))
																{
																	if(SPlayerStats* pStats = static_cast<SPlayerStats*>(pPlayer->GetActorStats()))
																	{
																		if((pStats->worldWaterLevel+0.1f) > pStats->FPWeaponPos.z)
																			return false;
																	}
																}

																//Don't throw if there's no ammo (or not fm)
																if(m_fm && !m_fm->OutOfAmmo() && m_nextGrenadeThrow<=0.0f)
																	return true;
															}
															else if(activationMode==eAAM_OnRelease && m_currentState==eOHS_HOLDING_GRENADE)
															{
																return true;
															}
															break;
		case eOHA_USE:						
															//Evaluate mouse inputs first
															if(inputMethod==INPUT_LBM)
															{
																//Two handed object throw/drop is handled with the mouse now...
																if(activationMode==eAAM_OnPress && m_currentState&eOHS_HOLDING_OBJECT && m_grabType==GRAB_TYPE_TWO_HANDED)
																{
																	m_forceThrow = true;
																	return true;
																}
																else if(activationMode==eAAM_OnRelease && m_currentState&eOHS_THROWING_OBJECT && m_grabType==GRAB_TYPE_TWO_HANDED)
																{
																	return true;
																}
															}
															else if(inputMethod==INPUT_RBM)
															{
																//Two handed object throw/drop is handled with the mouse now...
																if(activationMode==eAAM_OnPress && m_currentState&eOHS_HOLDING_OBJECT)
																{
																	m_forceThrow = false;
																	return true;
																}
																else if(activationMode==eAAM_OnRelease && m_currentState&eOHS_THROWING_OBJECT)
																{
																	return true;
																}
															}
															else if(activationMode==eAAM_OnPress && (m_currentState&(eOHS_INIT_STATE|eOHS_HOLDING_OBJECT|eOHS_HOLDING_NPC)))
															{
																m_forceThrow = true;
																return true;
															}
															else if(activationMode==eAAM_OnRelease && (m_currentState&(eOHS_THROWING_OBJECT|eOHS_THROWING_NPC)))
															{
																return true;
															}
															break;

		case eOHA_MELEE_ATTACK:		if(activationMode==eAAM_OnPress && m_currentState==eOHS_HOLDING_OBJECT && m_grabType==GRAB_TYPE_ONE_HANDED)
															{
																return true;
															}
															break;
	}

	return false;
}

//==================================================================
bool COffHand::PreExecuteAction(int requestedAction, int activationMode, bool forceSelect)
{
	if(m_currentState!=eOHS_INIT_STATE && requestedAction != eOHA_REINIT_WEAPON)
		return true;

	//We have to test the main weapon/item state, in some cases the offhand could not perform the action
	CActor *pActor = GetOwnerActor();
	if(!pActor || pActor->GetHealth()<=0 || (pActor->IsSwimming() && requestedAction!=eOHA_USE))
		return false;

	CItem			*pMain = GetActorItem(pActor);
	CWeapon		*pMainWeapon = static_cast<CWeapon *>(pMain?pMain->GetIWeapon():NULL);

	bool exec = true;

	if(pMain && pMain->IsMounted())
		return false;

	if(pMainWeapon)
		exec &= !pMainWeapon->IsModifying() && !pMainWeapon->IsReloading() && !pMainWeapon->IsSwitchingFireMode() && !pMainWeapon->IsFiring();


	if(pMainWeapon && (pMainWeapon->IsZoomed() || pMainWeapon->IsZooming()) && (requestedAction==eOHA_THROW_GRENADE))
	{
		pMainWeapon->ExitZoom();
		pMainWeapon->ExitViewmodes();
	}

	if(exec)
	{
		if(!gEnv->bMultiplayer || (requestedAction != eOHA_SWITCH_GRENADE))
		{
			SetHand(eIH_Left);		//Here??

			if ((GetEntity()->IsHidden() || forceSelect) && activationMode==eAAM_OnPress)
			{
				m_stats.fp=!m_stats.fp;	

				GetScheduler()->Lock(true);
				Select(true);
				GetScheduler()->Lock(false);
				SetBusy(false);
			}
		}
		m_mainHand = pMain;
		m_mainHandWeapon = pMainWeapon;
		m_mainHandIsDualWield = false;

		if(requestedAction==eOHA_THROW_GRENADE)
		{
			if(m_mainHand && m_mainHand->TwoHandMode()==1)
			{
				GetOwnerActor()->HolsterItem(true);
				m_mainHand = m_mainHandWeapon = NULL;
			}
			else if(m_mainHand && m_mainHand->IsDualWield() && m_mainHand->GetDualWieldSlave())
			{
				m_mainHand = static_cast<CItem*>(m_mainHand->GetDualWieldSlave());
				m_mainHandIsDualWield = true;
				m_mainHand->Select(false);
			}
		}			
	}
	else if (requestedAction == eOHA_REINIT_WEAPON)
	{
		m_mainHand = pMain;
		m_mainHandWeapon = pMainWeapon;
	}

	return exec;
}

void COffHand::NetStartFire()
{
	if (GetEntity()->IsHidden()) // this is need for network triggered grenade throws to trigger updates and what not..
	{
		m_stats.fp=!m_stats.fp;

		GetScheduler()->Lock(true);
		Select(true);
		GetScheduler()->Lock(false);
		SetBusy(false);
	}

	CWeapon::NetStartFire();

	AttachGrenadeToHand(GetCurrentFireMode(),false,true);
}

void COffHand::NetStopFire()
{
	CWeapon::NetStopFire();

	AttachGrenadeToHand(GetCurrentFireMode(),false,false);
}


//=============================================================================
//This function seems redundant...
void COffHand::CancelAction()
{
	SetOffHandState(eOHS_INIT_STATE);
}

//=============================================================================
void COffHand::FinishAction(EOffHandActions eOHA)
{
	switch(eOHA)
	{
		case eOHA_SWITCH_GRENADE: EndSwitchGrenade();
															break;

		case eOHA_PICK_ITEM:			EndPickUpItem();
															break;

		case eOHA_GRAB_NPC:				m_currentState = eOHS_HOLDING_NPC;
															break;

		case eOHA_THROW_NPC:			GetScheduler()->TimerAction(300, CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_RESET,this)), true);
															ThrowNPC();
															m_currentState = eOHS_TRANSITIONING;
															break;
		
		case eOHA_PICK_OBJECT:		m_currentState = eOHS_HOLDING_OBJECT;
															break;

		case eOHA_THROW_OBJECT:		{
															// after it's thrown, wait 500ms to enable collisions again
															GetScheduler()->TimerAction(500, CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_RESET,this)), true);
															DrawNear(false);

															IEntity *pEntity=m_pEntitySystem->GetEntity(m_heldEntityId);
															if (pEntity)
															{
																CActor* pPlayer = GetOwnerActor();

																if (pPlayer && pPlayer->GetActorClass() == CPlayer::GetActorClassType())
																{
																	static_cast<CPlayer*>(pPlayer)->NotifyObjectGrabbed(false, m_heldEntityId, false);
																}

																IPhysicalEntity* pPhys = pEntity->GetPhysics();
																if(pPhys && pPhys->GetType()==PE_PARTICLE)
																	pEntity->SetSlotLocalTM(0,m_intialBoidLocalMatrix);

															}												
															m_currentState = eOHS_TRANSITIONING;
															}
															break;
		
		case eOHA_RESET:					//Reset main weapon status and reset offhand
															{
																if (m_prevMainHandId && !GetOwnerActor()->IsSwimming())
																{
																	GetOwnerActor()->SelectItem(m_prevMainHandId, false);
																	m_mainHand = static_cast<CItem*>(GetOwnerActor()->GetCurrentItem());
																	m_mainHandWeapon = static_cast<CWeapon *>(m_mainHand?m_mainHand->GetIWeapon():NULL);
																}
																SetCurrentFireMode(m_lastFireModeId);
																float timeDelay = 0.1f; 
																if(!m_mainHand)
																{
																	SActorStats *pStats = GetOwnerActor()->GetActorStats();
																	if(!GetOwnerActor()->ShouldSwim() && !m_bCutscenePlaying && (pStats && !pStats->inFreefall))
																		GetOwnerActor()->HolsterItem(false);
																}
																else if(!m_mainHandIsDualWield && !m_prevMainHandId)
																{
																	m_mainHand->ResetDualWield();
																	m_mainHand->PlayAction(g_pItemStrings->offhand_off, 0, false, CItem::eIPAF_Default|CItem::eIPAF_NoBlend);
																	timeDelay = (m_mainHand->GetCurrentAnimationTime(CItem::eIGS_FirstPerson)+50)*0.001f;
																}
																else if(m_mainHandIsDualWield)
																{
																	m_mainHand->Select(true);
																}
																SetResetTimer(timeDelay);
																RequireUpdate(eIUS_General);
																m_prevMainHandId = 0;

																//turn off collision with thrown objects
																if(m_heldEntityId)
																	IgnoreCollisions(false,m_heldEntityId);

															}

															break;

		case	eOHA_FINISH_MELEE:	if(m_heldEntityId)
															{
																m_currentState = eOHS_HOLDING_OBJECT;
															}
															break;

		case eOHA_FINISH_AI_THROW_GRENADE:
															{
																// Reset the main weapon after a grenade throw.
																CActor *pActor = GetOwnerActor();
																if (pActor)
																{
																	CItem			*pMain = GetActorItem(pActor);
																	if (pMain)
																		pMain->PlayAction(g_pItemStrings->idle, 0, false, CItem::eIPAF_Default|CItem::eIPAF_NoBlend);
																}
															}
															break;
	}
}

void COffHand::Freeze(bool freeze)
{
	CWeapon::Freeze(freeze);

	if (!freeze && m_currentState==eOHS_HOLDING_GRENADE)
	{
		FinishAction(eOHA_RESET);
		CancelAction();
	}
}

//==============================================================================
void COffHand::SetOffHandState(EOffHandStates eOHS)
{
	m_currentState = eOHS;

	if(eOHS == eOHS_INIT_STATE)
	{
		m_mainHand = m_mainHandWeapon = NULL;
		m_heldEntityId = m_preHeldEntityId = 0;
		m_mainHandIsDualWield = false;
		Select(false);
	}
}

//==============================================================================
void COffHand::StartSwitchGrenade(bool xi_switch, bool fakeSwitch)
{
	//Iterate different firemodes 
	int firstMode = GetCurrentFireMode();
	int newMode = GetNextFireMode(GetCurrentFireMode());

	while (newMode != firstMode)
	{
		//Fire mode idx>2 means its a throw object/npc firemode
		if (GetFireMode(newMode)->OutOfAmmo() || newMode>=MAX_GRENADE_TYPES )
			newMode = GetNextFireMode(newMode);
		else
		{
			m_lastFireModeId = newMode;
			break;
		}
	}

	//We didn't find a fire mode with ammo
	if (newMode == firstMode)
	{
		CancelAction();

		if(m_ownerId == LOCAL_PLAYER_ENTITY_ID && g_pGame->GetHUD())
			g_pGame->GetHUD()->FireModeSwitch(true);

		return;
	}
	else if(!fakeSwitch)		
		RequestFireMode(newMode);

	//No animation in multiplayer or when using the gamepad
	if(gEnv->bMultiplayer || xi_switch)
	{
		m_currentState = eOHS_SWITCHING_GRENADE;
		SetResetTimer(0.3f); //Avoid spamming keyboard issues
		RequireUpdate(eIUS_General);
		return;
	}

	//Unzoom weapon if neccesary
	if(m_mainHandWeapon && (m_mainHandWeapon->IsZoomed() || m_mainHandWeapon->IsZooming()))
	{
		m_mainHandWeapon->ExitZoom();
		m_mainHandWeapon->ExitViewmodes();
	}

	m_mainHandIsDualWield = false;

	//A new grenade type/fire mode was chosen
	if(m_mainHand && (m_mainHand->TwoHandMode()!=1) && !m_mainHand->IsDualWield())
	{

		if(m_mainHandWeapon && m_mainHandWeapon->GetEntity()->GetClass()==CItem::sFistsClass)
		{
			CFists* pFists = static_cast<CFists*>(m_mainHandWeapon);
			pFists->RequestAnimState(CFists::eFAS_FIGHT);
		}

		if(m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
		{
			m_mainHandWeapon->RaiseWeapon(false,true);
			m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson,g_pItemStrings->idle);
		}

		//Play deselect left hand on main item
		m_mainHand->PlayAction(g_pItemStrings->offhand_on);
		m_mainHand->SetActionSuffix("akimbo_");

		GetScheduler()->TimerAction(m_mainHand->GetCurrentAnimationTime(eIGS_FirstPerson),
			CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_SWITCH_GRENADE, this)), false);

	}
	else
	{
		//No main item or holstered (wait 100ms)
		if(m_mainHand && m_mainHand->IsDualWield() && m_mainHand->GetDualWieldSlave())
		{
			m_mainHand = static_cast<CItem*>(m_mainHand->GetDualWieldSlave());
			m_mainHand->Select(false);
			m_mainHandIsDualWield = true;
		}
		else
		{
			GetOwnerActor()->HolsterItem(true);
			m_mainHand = m_mainHandWeapon = NULL;
		}
		GetScheduler()->TimerAction(100, CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_SWITCH_GRENADE,this)),false);
	}

	//Change offhand state
	m_currentState = eOHS_SWITCHING_GRENADE;

	if(fakeSwitch)
		AttachGrenadeToHand(firstMode);
	else
		AttachGrenadeToHand(newMode);
}

//==============================================================================
void COffHand::EndSwitchGrenade()
{
	//Play select grenade animation (and un-hide grenade geometry)
	PlayAction(g_pItemStrings->select_grenade);
	//HideItem(false);
	GetScheduler()->TimerAction(GetCurrentAnimationTime(CItem::eIGS_FirstPerson),
		CSchedulerAction<FinishGrenadeAction>::Create(FinishGrenadeAction(this,m_mainHand)), false);
}

//==============================================================================
void COffHand::PerformThrow(float speedScale)
{
	if (!m_fm)
		return;

	m_fm->StartFire();

	CThrow *pThrow = static_cast<CThrow *>(m_fm);
	pThrow->SetSpeedScale(speedScale);

	m_fm->StopFire();
	pThrow->ThrowingGrenade(true);

	// Schedule to revert back to main weapon.
	GetScheduler()->TimerAction(2000,
		CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_FINISH_AI_THROW_GRENADE, this)), false);
}

//===============================================================================
void COffHand::PerformThrow(int activationMode, EntityId throwableId, int oldFMId /* = 0 */, bool isLivingEnt /*=false*/)
{
	if (!m_fm)
		return;

	if(activationMode==eAAM_OnPress)
		m_currentState = eOHS_HOLDING_GRENADE;

	//Throw objects...
	if (throwableId && activationMode == eAAM_OnPress)
	{
		if(!isLivingEnt)
		{
			m_currentState = eOHS_THROWING_OBJECT;
			CThrow *pThrow = static_cast<CThrow *>(m_fm);
			pThrow->SetThrowable(throwableId, m_forceThrow, CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_THROW_OBJECT,this)));
		}
		else
		{
			m_currentState = eOHS_THROWING_NPC;
			CThrow *pThrow = static_cast<CThrow *>(m_fm);
			pThrow->SetThrowable(throwableId, true, CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_THROW_NPC,this)));
		}
		m_forceThrow = false;

		// enable leg IK again
		CActor* pActor = GetOwnerActor();
		if (pActor && pActor->IsClient() && m_grabType==GRAB_TYPE_TWO_HANDED)
		{
			if (ICharacterInstance* pCharacter = pActor->GetEntity()->GetCharacter(0))
			{
				if (ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose())
				{
					pSkeletonPose->EnableFootGroundAlignment(true);
				}
			}
		}
	}
	//--------------------------

	if (activationMode==eAAM_OnPress)
	{
		if (!m_fm->IsFiring() && m_nextGrenadeThrow < 0.0f)
		{
			if(m_currentState==eOHS_HOLDING_GRENADE)
				AttachGrenadeToHand(GetCurrentFireMode());

			m_fm->StartFire();
			SetBusy(false);
			if (m_mainHand && m_fm->IsFiring())
			{
				if(!(m_currentState&(eOHS_THROWING_NPC|eOHS_THROWING_OBJECT)))
				{
					if(m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
					{
						m_mainHandWeapon->RaiseWeapon(false,true);
						m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson,g_pItemStrings->idle);
					}

					m_mainHand->PlayAction(g_pItemStrings->offhand_on);
					m_mainHand->SetActionSuffix("akimbo_");
				}
			}
			if(!throwableId)
			{
				if(m_mainHandWeapon && m_mainHandWeapon->GetEntity()->GetClass()==CItem::sFistsClass)
				{
					CFists* pFists = static_cast<CFists*>(m_mainHandWeapon);
					pFists->RequestAnimState(CFists::eFAS_FIGHT);
				}
			}
		}

	}
	else if (activationMode==eAAM_OnRelease && m_nextGrenadeThrow <= 0.0f)
	{
		CThrow *pThrow = static_cast<CThrow *>(m_fm);
		if(m_currentState!=eOHS_HOLDING_GRENADE)
		{
			pThrow->ThrowingGrenade(false);
		}
		else if(m_currentState==eOHS_HOLDING_GRENADE)
		{
			m_currentState = eOHS_THROWING_GRENADE;
		}
		else
		{
			CancelAction();
			return;
		}

		m_nextGrenadeThrow = 60.0f/m_fm->GetFireRate();
		m_fm->StopFire();
		pThrow->ThrowingGrenade(true);

		if (m_fm->IsFiring() && m_currentState==eOHS_THROWING_GRENADE)
		{
			GetScheduler()->TimerAction(GetCurrentAnimationTime(CItem::eIGS_FirstPerson),CSchedulerAction<FinishGrenadeAction>::Create(FinishGrenadeAction(this,m_mainHand)),false);
		}
	}
}

//--------------
int COffHand::CanPerformPickUp(CActor *pActor, IPhysicalEntity *pPhysicalEntity /*=NULL*/, bool getEntityInfo /*= false*/)
{
	if (!pActor || !pActor->IsClient())
		return OH_NO_GRAB;

	IMovementController * pMC = pActor->GetMovementController();
	if(!pMC)
		return OH_NO_GRAB;

	SMovementState info;
	pMC->GetMovementState(info);

	if(gEnv->bMultiplayer)
		return CheckItemsInProximity(info.eyePosition,info.eyeDirection,getEntityInfo);

	EStance playerStance = pActor->GetStance();

	if(!getEntityInfo)
	{ 
		//Prevent pick up message while can not pick up
		IItem *pItem = pActor->GetCurrentItem(false);
		CWeapon * pMainWeapon = pItem?static_cast<CWeapon*>(pItem->GetIWeapon()):NULL;
		if (pMainWeapon)
		{
			if(pMainWeapon->IsBusy() || pMainWeapon->IsModifying() || pMainWeapon->IsReloading())
				return OH_NO_GRAB;
		}
	}

	if (!pPhysicalEntity)
	{
		const ray_hit *pRay = pActor->GetGameObject()->GetWorldQuery()->GetLookAtPoint(m_range);
		if (pRay)
			pPhysicalEntity = pRay->pCollider;
		else
			return CheckItemsInProximity(info.eyePosition,info.eyeDirection,getEntityInfo);
	}

	IEntity *pEntity = m_pEntitySystem->GetEntityFromPhysics(pPhysicalEntity);

	//if (pMC)
	{
		m_crosshairId = 0;
		Vec3 pos = info.eyePosition;
		float lenSqr = 0.0f;
		bool  breakable = false;
		pe_params_part pPart;

		//Check if entity is in range
		if (pEntity)
		{
			lenSqr=(pos-pEntity->GetWorldPos()).len2();
			if(pPhysicalEntity->GetType()==PE_RIGID && (!strcmp(pEntity->GetClass()->GetName(), "Default")))
			{
				//Procedurally breakable object (most likely...)
				//I need to adjust the distance, since the pivot of the entity could be anywhere
				pPart.ipart = 0;
				if(pPhysicalEntity->GetParams(&pPart) && pPart.pPhysGeom)
				{
					lenSqr-=pPart.pPhysGeom->origin.len2();
					breakable = true;
				}
			}

		}

		if (lenSqr<m_range*m_range)
		{
			if(pEntity)
			{
				// check if we have to pickup with two hands or just on hand
				SelectGrabType(pEntity);
				m_crosshairId = pEntity->GetId();

				if(getEntityInfo)
					m_preHeldEntityId = pEntity->GetId();

				//1.- Player can grab some NPCs
				//Let the actor decide if it can be grabbed
				if(CActor *pActorAI = static_cast<CActor*>(gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId())))
				{
					if(((playerStance!=STANCE_STAND)&&(playerStance!=STANCE_ZEROG))||pActor->IsSwimming())
						return OH_NO_GRAB;

					//Check Player position vs AI position
					if(pActorAI->GetActorSpecies()==eGCT_HUMAN)
					{
						float playerZ = pActor->GetEntity()->GetWorldPos().z;
						Vec3 aiPos     = pActorAI->GetEntity()->GetWorldPos();
						if(aiPos.z-playerZ>1.0f)
							return OH_NO_GRAB;

						Line aim = Line(info.eyePosition,info.eyeDirection);

						float dst=LinePointDistanceSqr(aim, aiPos, 0.75f);
						if(dst<0.6f)
							return OH_NO_GRAB;

						if(((SPlayerStats *)pActorAI->GetActorStats())->isStandingUp)
							return OH_NO_GRAB;
					}

					if(pEntity->GetAI() && pActor->GetEntity() && !pActorAI->GetLinkedVehicle()) 
					{
						//Check script table (maybe is not possible to grab)
						SmartScriptTable props;
						SmartScriptTable propsDamage;
						IScriptTable* pScriptTable = pEntity->GetScriptTable();
						if(pScriptTable && pScriptTable->GetValue("Properties", props))
						{
							if(props->GetValue("Damage", propsDamage))
							{
								int noGrab = 0;
								if(propsDamage->GetValue("bNoGrab", noGrab) && noGrab!=0)
									return OH_NO_GRAB;

								float customGrabDistance;
								if(propsDamage->GetValue("customGrabDistance", customGrabDistance))
								{
									if(lenSqr> customGrabDistance * customGrabDistance)
										return OH_NO_GRAB;
								}
							}
						}

						if(pActorAI->GetActorSpecies()!=eGCT_UNKNOWN && pActorAI->GetHealth()>0 && !pActorAI->IsFallen() && pEntity->GetAI()->IsHostile(pActor->GetEntity()->GetAI(),false))
							return OH_GRAB_NPC;
						else
							return OH_NO_GRAB;
					}
					return OH_NO_GRAB;
				}

				//2. -if it's an item, let the item decide if it can be picked up or not
				if (CItem *pItem=static_cast<CItem *>(m_pItemSystem->GetItem(pEntity->GetId())))
				{
					if(pItem->CanPickUp(pActor->GetEntityId()))
						return OH_GRAB_ITEM;
					else 
						return OH_NO_GRAB;
				}

				//Items have priority over the rest of pickables
				if(CheckItemsInProximity(info.eyePosition,info.eyeDirection,getEntityInfo)==OH_GRAB_ITEM)
					return OH_GRAB_ITEM;
				
				if(pActor->IsSwimming() || playerStance==STANCE_PRONE)
					return OH_NO_GRAB;

				//3. -If we found a helper, it has to be pickable
				if(m_hasHelper && pPhysicalEntity->GetType()==PE_RIGID)
				{
					SmartScriptTable props;
					IScriptTable *pEntityScript=pEntity->GetScriptTable();
					if (pEntityScript && pEntityScript->GetValue("Properties", props))
					{
						//If it's not pickable, ignore helper
						int pickable=0;
						if (props->GetValue("bPickable", pickable) && !pickable)
							return OH_NO_GRAB;
					}
					return OH_GRAB_OBJECT;
				}

				//4. Pick boid object
				if ((pPhysicalEntity->GetType()==PE_PARTICLE || (pPhysicalEntity->GetType()==PE_ARTICULATED && m_grabType==GRAB_TYPE_TWO_HANDED)) && m_hasHelper)
					return OH_GRAB_OBJECT;

				//5. -Procedurally breakable object (most likely...)
				if(breakable)
				{
					//Set "hold" matrix
					if(pPart.pPhysGeom->V<0.35f && pPart.pPhysGeom->Ibody.len()<0.1)
					{
						m_holdOffset.SetTranslation(pPart.pPhysGeom->origin+Vec3(0.0f,-0.15f,0.0f));
						m_holdOffset.InvertFast();
						return OH_GRAB_OBJECT;
					}

				}
				
				//6.- Temp? solution for spawned rocks (while they don't have helpers)
				if(pPhysicalEntity->GetType()==PE_RIGID && !strcmp(pEntity->GetClass()->GetName(), "rock"))
				{
					m_grabType = GRAB_TYPE_ONE_HANDED;
					return OH_GRAB_OBJECT;
				}

				//7.- Legacy system...
				SmartScriptTable props;
				IScriptTable *pEntityScript=pEntity->GetScriptTable();
				if (pPhysicalEntity->GetType()==PE_RIGID && pEntityScript && pEntityScript->GetValue("Properties", props))
				{
					int pickable=0;
					int usable = 0;
					if (props->GetValue("bPickable", pickable) && !pickable)
						return false;
					else if (pickable)
						if(props->GetValue("bUsable", usable) && !usable)
							return OH_GRAB_OBJECT;

					return false;
				}

				if(getEntityInfo)
					m_preHeldEntityId = 0;

				return OH_NO_GRAB;//CheckItemsInProximity(info.eyePosition, info.eyeDirection, getEntityInfo);
			}
			else if (pPhysicalEntity->GetType()==PE_STATIC)
			{
				//Rocks and small static vegetation marked as pickable
				IRenderNode *pRenderNode=0;
				pe_params_foreign_data pfd;
				if (pPhysicalEntity->GetParams(&pfd) && pfd.iForeignData==PHYS_FOREIGN_ID_STATIC)
					pRenderNode=static_cast<IRenderNode *>(pfd.pForeignData);

				if (pRenderNode && pRenderNode->GetRndFlags()&ERF_PICKABLE)
				{
					if(getEntityInfo)
					{
						m_grabType = GRAB_TYPE_ONE_HANDED;
						m_pRockRN = pRenderNode;
						m_preHeldEntityId = 0;
					}
					return OH_GRAB_OBJECT;
				}
			}
		}
	}
	return CheckItemsInProximity(info.eyePosition,info.eyeDirection,getEntityInfo);
}

//========================================================================================
int COffHand::CheckItemsInProximity(Vec3 pos, Vec3 dir, bool getEntityInfo)
{
	float sizeX = 1.2f;
	if(gEnv->bMultiplayer)
		sizeX = 1.75;
	float sizeY = sizeX;
	float sizeZup = 0.5f;
	float sizeZdown = 1.75f;

	SEntityProximityQuery query;
	query.box = AABB(Vec3(pos.x-sizeX,pos.y-sizeY,pos.z-sizeZdown),
		Vec3(pos.x+sizeX,pos.y+sizeY,pos.z+sizeZup));
	query.nEntityFlags = ~0; // Filter by entity flag.

	float minDstSqr = 0.2f;
	EntityId nearItemId = 0;
	int count=gEnv->pEntitySystem->QueryProximity(query);
	for(int i=0; i<query.nCount; i++)
	{
		EntityId id=query.pEntities[i]->GetId();

		if (CItem *pItem=static_cast<CItem*>(m_pItemSystem->GetItem(id)))
		{
			if (pItem->GetOwnerId()!=GetOwnerId())
			{
				Line line(pos,dir);
				AABB bbox;
				pItem->GetEntity()->GetWorldBounds(bbox);
				Vec3 itemPos = bbox.GetCenter();
				if (dir.Dot(itemPos-pos)>0.0f)
				{
					float dstSqr = LinePointDistanceSqr(line,itemPos);
					if(dstSqr<0.2f)
					{
						if((dstSqr<minDstSqr) && pItem->CanPickUp(GetOwnerId()))
						{
							minDstSqr = dstSqr;
							nearItemId = id;
						}
					}
				}
			}
		}
	}

	if(IEntity* pEntity = m_pEntitySystem->GetEntity(nearItemId))
	{
		AABB bbox;
		pEntity->GetWorldBounds(bbox);
		Vec3 itemPos = bbox.GetCenter();
		Vec3 dir = (pos-itemPos);

		IPhysicalEntity* phys = pEntity->GetPhysics();

		ray_hit hit;
		if(!gEnv->pPhysicalWorld->RayWorldIntersection(itemPos, dir, ent_static|ent_rigid|ent_sleeping_rigid, 
			rwi_stop_at_pierceable|rwi_ignore_back_faces, &hit, 1, phys?&phys:NULL, phys?1:0))
		{
			//If nothing in between...
			m_crosshairId = nearItemId;

			SelectGrabType(pEntity);
			if(getEntityInfo)
				m_preHeldEntityId = m_crosshairId;

			return OH_GRAB_ITEM;
		}
	}
	
	return OH_NO_GRAB;
}
//==========================================================================================
bool COffHand::PerformPickUp()
{
	//If we are here, we must have the entity ID
	m_heldEntityId = m_preHeldEntityId;
	m_preHeldEntityId =  0;
	m_startPickUp = false;
	IEntity *pEntity = NULL;
	
	if(m_heldEntityId)
		pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
	else if(m_pRockRN)
	{
		m_heldEntityId = SpawnRockProjectile(m_pRockRN);
		m_pRockRN = NULL;
		if(!m_heldEntityId)
			return false;

		pEntity = m_pEntitySystem->GetEntity(m_heldEntityId);
		SelectGrabType(pEntity);
		m_grabType = GRAB_TYPE_ONE_HANDED; //Force for now
	}

	if(pEntity)
	{
		CActor* pActor = GetOwnerActor();
		// Send event to entity.
		SEntityEvent entityEvent;
		entityEvent.event = ENTITY_EVENT_PICKUP;
		entityEvent.nParam[0] = 1;
		if (pActor)
			entityEvent.nParam[1] = pActor->GetEntityId();
		pEntity->SendEvent( entityEvent );

		m_holdScale = pEntity->GetScale();

		IgnoreCollisions(true,m_heldEntityId);
		DrawNear(true);

		if(pActor && pActor->IsPlayer())
		{
			m_heldEntityMass = 1.0f;
			if(IPhysicalEntity* pPhy = pEntity->GetPhysics())
			{
				pe_status_dynamics dynStat;
				if (pPhy->GetStatus(&dynStat))
					m_heldEntityMass = dynStat.mass;
				if(pPhy->GetType()==PE_PARTICLE)
					m_intialBoidLocalMatrix = pEntity->GetSlotLocalTM(0,false);
			}
			// only if we're picking up a normal (non Item) object
			if ((m_currentState & eOHS_PICKING) && pActor->GetActorClass() == CPlayer::GetActorClassType())
			{
				CPlayer* pPlayer = static_cast<CPlayer*> (pActor);
				pPlayer->NotifyObjectGrabbed(true, m_heldEntityId, false, (m_grabType==GRAB_TYPE_TWO_HANDED));
			}
		}

		// disable leg IK if we are grabbing something with two hands
		if (pActor->IsClient() && m_grabType==GRAB_TYPE_TWO_HANDED)
		{
			if (ICharacterInstance* pCharacter = pActor->GetEntity()->GetCharacter(0))
			{
				if (ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose())
				{
					pSkeletonPose->EnableFootGroundAlignment(false);
				}
			}
		}

		SetDefaultIdleAnimation(eIGS_FirstPerson, m_grabTypes[m_grabType].idle);

		m_checkForConstraintDelay = 2; //Wait 2 frames for checking the constraint (issue with sys_physics_cpu 1)

		return true;
	}

	return false;
}

//===========================================================================================
void COffHand::IgnoreCollisions(bool ignore, EntityId entityId /*=0*/)
{
	if (!m_heldEntityId && !entityId)
		return;

	IEntity *pEntity=m_pEntitySystem->GetEntity(m_heldEntityId?m_heldEntityId:entityId);
	if (!pEntity)
		return;

	IPhysicalEntity *pPE=pEntity->GetPhysics();
	if (!pPE)
		return;

	if(pPE->GetType()==PE_PARTICLE)
	{
		m_constraintId = 0;
		return;
	}

	if (ignore)
	{
		if(pEntity->IsHidden())
			return;

		//The constraint doesn't work with Items physicalized as static
		if(pPE->GetType()==PE_STATIC)
		{
			IItem *pItem = m_pItemSystem->GetItem(pEntity->GetId());
			if(pItem)
			{
				pItem->Physicalize(true,true);
				pPE = pEntity->GetPhysics();
				assert(pPE);
			}
		}

		CActor *pActor = GetOwnerActor();
		if (!pActor)
			return;

		pe_action_add_constraint ic;
		ic.flags=constraint_inactive|constraint_ignore_buddy;
		ic.pBuddy=pActor->GetEntity()->GetPhysics();
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

//==========================================================================================
void COffHand::DrawNear(bool drawNear, EntityId entityId /*=0*/)
{
	if (!m_heldEntityId && !entityId)
		return;

	IEntity *pEntity=m_pEntitySystem->GetEntity(m_heldEntityId?m_heldEntityId:entityId);
	if (!pEntity)
		return;

	int nslots=pEntity->GetSlotCount();
	for (int i=0;i<nslots;i++)
	{
		if (pEntity->GetSlotFlags(i)&ENTITY_SLOT_RENDER)
		{
			if (drawNear)
			{
				pEntity->SetSlotFlags(i, pEntity->GetSlotFlags(i)|ENTITY_SLOT_RENDER_NEAREST);
				if(IEntityRenderProxy* pProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER))
				{
					if(IRenderNode* pRenderNode = pProxy->GetRenderNode())
						pRenderNode->SetRndFlags(ERF_REGISTER_BY_POSITION,true);
				}
				if(IEntityPhysicalProxy* pPhysicsProxy = static_cast<IEntityPhysicalProxy*>(pEntity->GetProxy(ENTITY_PROXY_PHYSICS)))
					pPhysicsProxy->DephysicalizeFoliage(i);

			}
			else
			{
				pEntity->SetSlotFlags(i, pEntity->GetSlotFlags(i)&(~ENTITY_SLOT_RENDER_NEAREST));
				if(IEntityRenderProxy* pProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER))
				{
					if(IRenderNode* pRenderNode = pProxy->GetRenderNode())
						pRenderNode->SetRndFlags(ERF_REGISTER_BY_POSITION,false);
				}
				if(IEntityPhysicalProxy* pPhysicsProxy = static_cast<IEntityPhysicalProxy*>(pEntity->GetProxy(ENTITY_PROXY_PHYSICS)))
					pPhysicsProxy->PhysicalizeFoliage(i);

			}
		}
	}
}

//=========================================================================================
void COffHand::SelectGrabType(IEntity* pEntity)
{
	CActor *pActor=GetOwnerActor();

	assert(pActor && "COffHand::SelectGrabType: No OwnerActor, probably something bad happened");
	if (!pActor)
		return;

	if (pEntity)
	{
		// iterate over the grab types and see if this object supports one
		m_grabType = GRAB_TYPE_ONE_HANDED;
		const TGrabTypes::const_iterator end = m_grabTypes.end();
		for (TGrabTypes::const_iterator i = m_grabTypes.begin(); i != end; ++i, ++m_grabType)
		{
			SEntitySlotInfo slotInfo;
			for(int n=0; n<pEntity->GetSlotCount(); n++)
			{
				if(!pEntity->IsSlotValid(n))
					continue;

				bool ok = pEntity->GetSlotInfo(n,slotInfo) && (pEntity->GetSlotFlags(n)&ENTITY_SLOT_RENDER);
				if (ok && slotInfo.pStatObj)
				{
					//Iterate two times (normal helper name, and composed one)
					for(int j=0;j<2;j++)
					{
						string helper;
						helper.clear();
						if(j==0)
						{
							helper.append(slotInfo.pStatObj->GetGeoName());helper.append("_");helper.append((*i).helper.c_str());
						}
						else
							helper.append((*i).helper.c_str());

						//It is already a subobject, we have to search in the parent
						if(slotInfo.pStatObj->GetParentObject())
						{
							IStatObj::SSubObject* pSubObj = slotInfo.pStatObj->GetParentObject()->FindSubObject(helper.c_str());
							if (pSubObj)
							{
								m_holdOffset = pSubObj->tm;
								m_holdOffset.OrthonormalizeFast();
								m_holdOffset.InvertFast();
								m_hasHelper = true;
								return;						
							}
						}
						else{
							IStatObj::SSubObject* pSubObj = slotInfo.pStatObj->FindSubObject(helper.c_str());
							if (pSubObj)
							{
								m_holdOffset = pSubObj->tm;
								m_holdOffset.OrthonormalizeFast();
								m_holdOffset.InvertFast();
								m_hasHelper = true;
								return;						
							}
						}
					}
				}
				else if(ok && slotInfo.pCharacter)
				{
					//Grabbing helpers for boids/animals
					IAttachmentManager *pAM = slotInfo.pCharacter->GetIAttachmentManager();
					if(pAM)
					{
						IAttachment *pAttachment = pAM->GetInterfaceByName((*i).helper.c_str());
						if(pAttachment)
						{
							m_holdOffset = Matrix34(pAttachment->GetAttAbsoluteDefault().q);
							m_holdOffset.SetTranslation(pAttachment->GetAttAbsoluteDefault().t);
							m_holdOffset.OrthonormalizeFast();
							m_holdOffset.InvertFast();
							if(m_grabType==GRAB_TYPE_TWO_HANDED)
								m_holdOffset.AddTranslation(Vec3(0.1f,0.0f,-0.12f));
							m_hasHelper = true;
							return;
						}
					}
				}
			}
		}

		// when we come here, we haven't matched any of the predefined helpers ... so try to make a 
		// smart decision based on how large the object is
		//float volume(0),heavyness(0);
		//pActor->CanPickUpObject(pEntity, heavyness, volume);

		// grabtype 0 is onehanded and 1 is twohanded
		//m_grabType = (volume>0.08f) ? 1 : 0;
		m_holdOffset.SetIdentity();
		m_hasHelper = false;
		m_grabType = GRAB_TYPE_TWO_HANDED;
	}
}

//========================================================================================================
void COffHand::StartPickUpItem()
{	
	CPlayer *pPlayer = static_cast<CPlayer*>(GetOwnerActor());
	assert(pPlayer && "COffHand::StartPickUpItem -> No player found!!");

	bool drop_success=false;

	IEntity* pPreHeldEntity = m_pEntitySystem->GetEntity(m_preHeldEntityId);

	// determine position of entity which will be picked up
	if (!pPreHeldEntity)
		return;

	if(!pPlayer->CheckInventoryRestrictions(pPreHeldEntity->GetClass()->GetName()))
	{
		//Can not carry more heavy/medium weapons
		//Drop existing weapon and pick up the other
		
		IItem* pItem = GetExchangeItem(pPlayer);
		if(pItem)
		{
			if(pPlayer->DropItem(pItem->GetEntityId(), 8.0f, true))
				drop_success=true;
		}

		if(!drop_success)
		{
			g_pGame->GetGameRules()->OnTextMessage(eTextMessageError, "@mp_CannotCarryMore");
			CancelAction();
			return;
		}
	}
	else if(IItem* pItem =m_pItemSystem->GetItem(m_preHeldEntityId))
	{
		if(!pItem->CheckAmmoRestrictions(pPlayer->GetEntityId()))
		{
			CryFixedStringT<128> itemName = pItem->GetEntity()->GetClass()->GetName();
			if(!stricmp(itemName.c_str(), "CustomAmmoPickup"))
			{
				SmartScriptTable props;
				if (pItem->GetEntity()->GetScriptTable() && pItem->GetEntity()->GetScriptTable()->GetValue("Properties", props))
				{
					const char *name = NULL;
					if(props->GetValue("AmmoName", name))
						itemName = name;
				}
			}
			if(g_pGame->GetHUD())
			{
				CryFixedStringT<128> temp = "@";
				temp.append(itemName.c_str());
				g_pGame->GetHUD()->DisplayFlashMessage("@ammo_maxed_out", 2, ColorF(1.0f, 0,0), true, temp.c_str());
			}
			CancelAction();
			return;
		}
	}

	//No animation in MP
	if(gEnv->bMultiplayer)
	{
		m_currentState = eOHS_PICKING_ITEM2;
		pPlayer->PickUpItem(m_preHeldEntityId,true);
		CancelAction();
		return;
	}

	//Unzoom weapon if neccesary
	if(m_mainHandWeapon && (m_mainHandWeapon->IsZoomed() || m_mainHandWeapon->IsZooming()))
	{
		m_mainHandWeapon->ExitZoom();
		m_mainHandWeapon->ExitViewmodes();
	}

	if(m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
	{
		m_mainHandWeapon->RaiseWeapon(false,true);
		m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson,g_pItemStrings->idle);
	}

	//Everything seems ok, start the action...
	m_currentState = eOHS_PICKING_ITEM;
	m_pickingTimer = 0.3f;
	m_mainHandIsDualWield = false;
	pPlayer->NeedToCrouch(pPreHeldEntity->GetWorldPos());

	if (m_mainHand && (m_mainHand->TwoHandMode() == 1 || drop_success))
	{
		GetOwnerActor()->HolsterItem(true);
		m_mainHand = m_mainHandWeapon = NULL;
	}
	else
	{
		if (m_mainHand)
		{
			if(m_mainHand->IsDualWield() && m_mainHand->GetDualWieldSlave())
			{
				m_mainHand = static_cast<CItem*>(m_mainHand->GetDualWieldSlave());
				m_mainHand->Select(false);
				m_mainHandIsDualWield = true;
			}
			else
			{
				m_mainHand->PlayAction(g_pItemStrings->offhand_on);
				m_mainHand->SetActionSuffix("akimbo_");
			}
		}
	}

	PlayAction(g_pItemStrings->pickup_weapon_left,0,false,eIPAF_Default|eIPAF_RepeatLastFrame);
	GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson)+100, CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_PICK_ITEM,this)), false);
	RequireUpdate(eIUS_General);
	m_startPickUp = true;
}

//=========================================================================================================
void COffHand::EndPickUpItem()
{
	IFireMode* pReloadFM = 0;
	EntityId prevWeaponId = 0;
	if (m_mainHandWeapon && IsServer() && m_mainHand)
	{
		prevWeaponId = m_mainHand->GetEntityId();
		pReloadFM = m_mainHandWeapon->GetFireMode(m_mainHandWeapon->GetCurrentFireMode());
		if (pReloadFM)
		{
			int fmAmmo = pReloadFM->GetAmmoCount();
			int invAmmo = GetInventoryAmmoCount(pReloadFM->GetAmmoType());
			if (pReloadFM && (pReloadFM->GetAmmoCount() != 0 || GetInventoryAmmoCount(pReloadFM->GetAmmoType()) != 0))
			{
				pReloadFM = 0;
			}
		}
	}

	//Restore main weapon
	if (m_mainHand)
	{
		if(m_mainHandIsDualWield)
		{
			m_mainHand->Select(true);
		}
		else
		{
			m_mainHand->ResetDualWield();
			m_mainHand->PlayAction(g_pItemStrings->offhand_off, 0, false, eIPAF_Default|eIPAF_NoBlend);
		}
	}
	else
	{
		GetOwnerActor()->HolsterItem(false);
	}

	//Pick-up the item, and reset offhand
	m_currentState = eOHS_PICKING_ITEM2;

	GetOwnerActor()->PickUpItem(m_heldEntityId, true);
	IgnoreCollisions(false,m_heldEntityId);

	SetOffHandState(eOHS_INIT_STATE);

	// if we were in a reload position and weapon hasn't changed, try to reload it
	// this will fail automatically, if we didn't get any new ammo
	// it will only occur on the server due to checks at the start of this function
	if (pReloadFM)
	{
		IItem* pItem = GetOwnerActor()->GetCurrentItem();
		if (pItem && pItem->GetEntityId() == prevWeaponId)
		{
			if (IWeapon* pWeapon = pItem->GetIWeapon())
			{
				pWeapon->Reload(false);
			}
		}
	}
}

//=======================================================================================
void COffHand::PickUpObject(bool isLivingEnt /* = false */)
{	
	//Grab NPCs-----------------
	if(isLivingEnt)
		if(!GrabNPC())
			CancelAction();
	//-----------------------

	//Don't pick up in prone
	CPlayer *pPlayer = static_cast<CPlayer*>(GetOwnerActor());
	if(pPlayer && pPlayer->GetStance()==STANCE_PRONE)
	{
		CancelAction();
		return;
	}

	//Unzoom weapon if neccesary
	if(m_mainHandWeapon && (m_mainHandWeapon->IsZoomed() || m_mainHandWeapon->IsZooming()))
	{
		m_mainHandWeapon->ExitZoom();
		m_mainHandWeapon->ExitViewmodes();
	}

	// if two handed or dual wield we use the fists as the mainhand weapon
	if (m_mainHand && (m_grabTypes[m_grabType].twoHanded || m_mainHand->TwoHandMode() >= 1))
	{
		if (m_grabTypes[m_grabType].twoHanded)
		{
			GetOwnerActor()->HolsterItem(true);
			m_mainHand = m_mainHandWeapon = NULL;
		}
		else
		{
			m_prevMainHandId = m_mainHand->GetEntityId();
			GetOwnerActor()->SelectItemByName("Fists",false);
			m_mainHand = GetActorItem(GetOwnerActor());
			m_mainHandWeapon = static_cast<CWeapon *>(m_mainHand?m_mainHand->GetIWeapon():NULL);
		}
	}

	if (m_mainHand)
	{

		if(m_mainHandWeapon && m_mainHandWeapon->IsWeaponRaised())
		{
			m_mainHandWeapon->RaiseWeapon(false,true);
			m_mainHandWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson,g_pItemStrings->idle);
		}

		if(m_mainHand->IsDualWield() && m_mainHand->GetDualWieldSlave())
		{
			m_mainHand = static_cast<CItem*>(m_mainHand->GetDualWieldSlave());
			m_mainHand->Select(false);
			m_mainHandIsDualWield = true;
		}
		else
		{
			m_mainHand->PlayAction(g_pItemStrings->offhand_on);
			m_mainHand->SetActionSuffix("akimbo_");
		}


		if(m_mainHandWeapon)
		{
			IFireMode *pFireMode = m_mainHandWeapon->GetFireMode(m_mainHandWeapon->GetCurrentFireMode());
			if(pFireMode)
			{
				pFireMode->SetRecoilMultiplier(1.5f);		//Increase recoil for the weapon
			}
		}
	}
	if(!isLivingEnt)
	{
		m_currentState	= eOHS_PICKING;
		m_pickingTimer	= 0.3f;
		SetDefaultIdleAnimation(CItem::eIGS_FirstPerson,m_grabTypes[m_grabType].idle);
		PlayAction(m_grabTypes[m_grabType].pickup);
		GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson), CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_PICK_OBJECT,this)), false);
		m_startPickUp = true;
	}
	else
	{
		m_currentState	= eOHS_GRABBING_NPC;
		m_grabType			= GRAB_TYPE_NPC;
		SetDefaultIdleAnimation(CItem::eIGS_FirstPerson,m_grabTypes[m_grabType].idle);
		PlayAction(m_grabTypes[m_grabType].pickup);
		GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson), CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_GRAB_NPC,this)), false);
	}
	RequireUpdate(eIUS_General);
}

//=========================================================================================
void COffHand::ThrowObject(int activationMode, bool isLivingEnt /*= false*/)
{
	if (activationMode == eAAM_OnPress)
	{
		m_lastFireModeId = GetCurrentFireMode();
		if (m_heldEntityId)
			SetCurrentFireMode(GetFireModeIdx(m_grabTypes[m_grabType].throwFM));
	}

	PerformThrow(activationMode, m_heldEntityId, m_lastFireModeId, isLivingEnt);

	if(m_mainHandWeapon)
	{
		IFireMode *pFireMode = m_mainHandWeapon->GetFireMode(m_mainHandWeapon->GetCurrentFireMode());
		if(pFireMode)
		{
			pFireMode->SetRecoilMultiplier(1.0f);		//Restore normal recoil for the weapon
		}
	}

}

//==========================================================================================
bool COffHand::GrabNPC()
{

	CActor  *pPlayer = GetOwnerActor();

	assert(pPlayer && "COffHand::GrabNPC --> Offhand has no owner actor (player)!");
	if(!pPlayer)
		return false;

	//Do not grab in prone
	if(pPlayer->GetStance()==STANCE_PRONE)
		return false;
	
	//Get actor
	CActor  *pActor  = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_preHeldEntityId));
	
	assert(pActor && "COffHand::GrabNPC -> No actor found!");
	if(!pActor)
		return false;

	IEntity *pEntity = pActor->GetEntity();
	assert(pEntity && "COffHand::GrabNPC -> Actor has no Entity");
	if(!pEntity || !pEntity->GetCharacter(0))
		return false;

	//The NPC holster his weapon
	bool mounted = false;
	CItem *currentItem = static_cast<CItem*>(pActor->GetCurrentItem());
	if(currentItem)
	{
		if(currentItem->IsMounted() && currentItem->IsUsed())
		{
			currentItem->StopUse(pActor->GetEntityId());
			mounted = true;
		}
	}

	if(!mounted)
	{
		pActor->HolsterItem(false); //AI sometimes has holstered a weapon and selected a different one...
		pActor->HolsterItem(true);
	}


	if ( IAnimationGraphState* pAGState = pActor->GetAnimationGraphState() )
	{
		char value[256];
		IAnimationGraphState::InputID actionInputID = pAGState->GetInputId( "Action" );
		pAGState->GetInput( actionInputID, value );
		if ( strcmp( value, "grabStruggleFP" ) != 0 )
		{
			// this is needed to make sure the transition is super fast
			if ( ICharacterInstance* pCharacter = pEntity->GetCharacter(0) )
				if ( ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim() )
					pSkeletonAnim->StopAnimationsAllLayers();
		}
		
		// just in case clear the Signal
		pAGState->SetInput( "Signal", "none" );
		pAGState->SetInput( actionInputID, "grabStruggleFP" );
	}
	if( SActorStats* pStats = pPlayer->GetActorStats() )
		pStats->grabbedTimer = 0.0f;

	// this needs to be done before sending signal "OnFallAndPlay" to make sure
	// in case AG state was FallAndPlay we leave it before AI is disabled
	if ( IAnimationGraphState* pAGState = pActor->GetAnimationGraphState() )
	{
		pAGState->ForceTeleportToQueriedState();
		pAGState->Update();
	}

	if(IAISystem *pAISystem=gEnv->pAISystem)
	{
		IAIActor* pAIActor = CastToIAIActorSafe(pEntity->GetAI());
		if(pAIActor)
		{
			IAISignalExtraData *pEData = pAISystem->CreateSignalExtraData();	
			pEData->point = Vec3(0,0,0);
			pAIActor->SetSignal(1,"OnFallAndPlay",0,pEData);
		}
	}

	//Modify Enemy Render Flags
	pEntity->GetCharacter(0)->SetFlags(pEntity->GetCharacter(0)->GetFlags()|ENTITY_SLOT_RENDER_NEAREST);
	if(IEntityRenderProxy* pProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER))
	{
		if(IRenderNode* pRenderNode = pProxy->GetRenderNode())
			pRenderNode->SetRndFlags(ERF_RENDER_ALWAYS,true);
	}

	//Disable IK
	SPlayerStats *stats = static_cast<SPlayerStats*>(pActor->GetActorStats());
	stats->isGrabbed = true;

	m_heldEntityId = m_preHeldEntityId;
	m_preHeldEntityId = 0;
	m_grabbedNPCSpecies = pActor->GetActorSpecies();
	m_grabType = GRAB_TYPE_NPC;
	m_killTimeOut = KILL_NPC_TIMEOUT;
	m_killNPC = m_effectRunning = m_npcWasDead =false;
	m_grabbedNPCInitialHealth = pActor->GetHealth();

	if (pPlayer->GetActorClass() == CPlayer::GetActorClassType())
	{
		static_cast<CPlayer*>(pPlayer)->NotifyObjectGrabbed(true, m_heldEntityId, true);
	}

	//Hide attachments on the back
	if(CWeaponAttachmentManager* pWAM = pActor->GetWeaponAttachmentManager())
		pWAM->HideAllAttachments(true);

	if(m_grabbedNPCSpecies==eGCT_HUMAN)
		PlaySound(eOHSound_Choking_Human,true);

	RequireUpdate(eIUS_General);
	GetGameObject()->EnablePostUpdates(this); //needed, if I pause the game before throwing the NPC

	return true;
}

//=============================================================================================
void COffHand::ThrowNPC(bool kill /*= true*/)
{
	//Get actor
 	CActor  *pActor  = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_heldEntityId));

	assert(pActor && "COffHand::Throw -> No actor found!");
	if(!pActor)
		return;

	IEntity *pEntity = pActor->GetEntity();
	assert(pEntity && "COffHand::Throw -> Actor has no Entity");
	if(!pEntity)
		return;

	SPlayerStats *stats = static_cast<SPlayerStats*>(pActor->GetActorStats());
	stats->isGrabbed = false;

	//Un-Hide attachments on the back
	if(CWeaponAttachmentManager* pWAM = pActor->GetWeaponAttachmentManager())
		pWAM->HideAllAttachments(false);

	CPlayer *pPlayer = static_cast<CPlayer*>(GetOwnerActor());

	if(kill)
	{
		UpdateGrabbedNPCWorldPos(pEntity,NULL);
		pActor->HolsterItem(false);
		IItem *currentItem = pActor->GetCurrentItem();

		{
			int prevHealth = pActor->GetHealth();
			int health = prevHealth-100;
			
			//In strenght mode, always kill
			if(pPlayer && pPlayer->GetNanoSuit() && pPlayer->GetNanoSuit()->GetMode()==NANOMODE_STRENGTH)
				health = 0;
			if(health<=0 || (m_grabbedNPCSpecies!=eGCT_HUMAN) || m_bCutscenePlaying)
			{
				pActor->SetHealth(0);
				if(currentItem && m_grabbedNPCSpecies==eGCT_HUMAN)
					pActor->DropItem(currentItem->GetEntityId(),0.5f,false, true);
				
				//Don't kill if it was already dead
				if(!stats->isRagDoll || prevHealth>0 || m_grabbedNPCSpecies==eGCT_HUMAN)
				{
					pActor->SetAnimationInput( "Action", "idle" );
					pActor->CreateScriptEvent("kill",0);
				}
			}
			else
			{
				if(pEntity->GetAI())
				{
					pActor->SetHealth(health);
					pActor->SetAnimationInput( "Action", "idle" );
					//pEntity->GetAI()->Event(AIEVENT_ENABLE,0);
					pActor->Fall();
					PlaySound(eOHSound_Kill_Human, true);
				}
			}

			IgnoreCollisions(true,m_heldEntityId);
		}
	}
	else
	{
		pActor->SetAnimationInput( "Action", "idle" );
	}

	/*if(m_grabbedNPCSpecies==eGCT_TROOPER)
	{
		PlaySound(eOHSound_Choking_Trooper,false);
	}
	else*/ 
	if(m_grabbedNPCSpecies==eGCT_HUMAN)
	{
		PlaySound(eOHSound_Choking_Human, false);
	}

	m_killTimeOut = -1.0f;
	m_killNPC = m_effectRunning = m_npcWasDead = false;
	m_grabbedNPCInitialHealth = 0;

	//Restore Enemy RenderFlags
	if(pEntity->GetCharacter(0))
		pEntity->GetCharacter(0)->SetFlags(pEntity->GetCharacter(0)->GetFlags()&(~ENTITY_SLOT_RENDER_NEAREST));
	if(IEntityRenderProxy* pProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER))
	{
		if(IRenderNode* pRenderNode = pProxy->GetRenderNode())
			pRenderNode->SetRndFlags(ERF_RENDER_ALWAYS,false);
	}

	if (pPlayer && pPlayer->GetActorClass() == CPlayer::GetActorClassType())
	{
		static_cast<CPlayer*>(pPlayer)->NotifyObjectGrabbed(false, pEntity->GetId(), true);
	}

	GetGameObject()->DisablePostUpdates(this); //Disable again

}

//==============================================================================
void COffHand::RunEffectOnGrabbedNPC(CActor* pNPC)
{

	//Under certain conditions, different things could happen to the grabbed NPC (die, auto-destruct...)
	if(m_grabbedNPCSpecies==eGCT_TROOPER)
	{
		if(m_killNPC && m_effectRunning)
		{
			pNPC->SetHealth(0);
			//PlaySound(eOHSound_Choking_Trooper,false);
			m_killNPC = false;
		}
		else if(m_killNPC || (pNPC->GetHealth()<m_grabbedNPCInitialHealth && !m_effectRunning))
		{
			SGameObjectEvent event(eCGE_InitiateAutoDestruction, eGOEF_ToScriptSystem);
			pNPC->GetGameObject()->SendEvent(event);
			m_effectRunning = true;
			m_killTimeOut = KILL_NPC_TIMEOUT-0.75f;
			m_killNPC = false;
		}
	}
	else if(m_grabbedNPCSpecies==eGCT_HUMAN && (pNPC->GetHealth()<m_grabbedNPCInitialHealth))
	{
		//Release the guy at first hit/damage... 8/
		if(m_currentState&(eOHS_HOLDING_NPC|eOHS_THROWING_NPC))
		{
			pNPC->SetHealth(0);
		}
	}

}

//========================================================================================
void COffHand::PlaySound(EOffHandSounds sound, bool play)
{
	if(!gEnv->pSoundSystem)
		return;

	bool repeating = false;
	uint idx = 0;
	const char* soundName = NULL;

	switch(sound)
	{
		case eOHSound_Choking_Trooper:
			soundName = "Sounds/alien:trooper:choke";
			repeating = true;
			break;

		case eOHSound_Choking_Human:
			idx = Random(MAX_CHOKE_SOUNDS);
			idx = CLAMP(idx,0,MAX_CHOKE_SOUNDS-1);
			if(idx>=0 && idx<MAX_CHOKE_SOUNDS)
				soundName = gChokeSoundsTable[idx];
			//repeating = true;
			break;

		case eOHSound_Kill_Human:
			idx = Random(MAX_CHOKE_SOUNDS);
			idx = CLAMP(idx,0,MAX_CHOKE_SOUNDS-1);
			if(idx>=0 && idx<MAX_CHOKE_SOUNDS)
				soundName = gDeathSoundsTable[idx];
			break;

		default:
			break;
	}

	if(!soundName)
		return;

	if(play)
	{
		ISound *pSound = NULL;
		if(repeating && m_sounds[sound])
			if(pSound = gEnv->pSoundSystem->GetSound(m_sounds[sound]))
				if(pSound->IsPlaying())
					return;

		if(!pSound)
			pSound = gEnv->pSoundSystem->CreateSound(soundName,0);
		if(pSound)
		{
			pSound->SetSemantic(eSoundSemantic_Player_Foley);
			if(repeating)
				m_sounds[sound] = pSound->GetId();
			pSound->Play();
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
//==========================================================================================
void COffHand::MeleeAttack()
{
	if(m_melee)
	{
		if(CActor* pOwner = GetOwnerActor())
			if(SPlayerStats* stats = static_cast<SPlayerStats*>(pOwner->GetActorStats()))
				if(stats->bLookingAtFriendlyAI)
					return;

		CMelee* melee = static_cast<CMelee*>(m_melee);

		m_currentState = eOHS_MELEE;
		m_melee->Activate(true);
		if(melee)
		{
			melee->IgnoreEntity(m_heldEntityId);
			//Most 1-handed objects have a mass between 1.0f and 10.0f
			//Scale melee damage/impulse based on mass of the held object
			float massScale = m_heldEntityMass*0.1f;
			if(massScale<0.2f)
				massScale*=0.1f; //Scale down even more for small object...
			melee->MeleeScale(min(massScale,1.0f)); 
		}
		m_melee->StartFire();
		m_melee->StopFire();

		GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson)+100, CSchedulerAction<FinishOffHandAction>::Create(FinishOffHandAction(eOHA_FINISH_MELEE,this)), false);
	}
}

//=======================================================================================
float COffHand::GetObjectMassScale()
{
	if(m_currentState&eOHS_HOLDING_NPC)
		return 0.65f;
	if(m_currentState&eOHS_HOLDING_OBJECT)
	{
		if(m_grabType == GRAB_TYPE_TWO_HANDED)
			return 0.65f;
		else
		{
			float mass = CLAMP(m_heldEntityMass,1.0f,10.0f);
			return (1.0f - (0.025f*mass));
		}
	}

	return 1.0f;
}

//=========================================================================================
bool COffHand::IsHoldingEntity()
{
	bool ret = false;
	if(m_currentState&(eOHS_GRABBING_NPC|eOHS_HOLDING_NPC|eOHS_THROWING_NPC|eOHS_PICKING|eOHS_HOLDING_OBJECT|eOHS_THROWING_OBJECT|eOHS_MELEE))
		ret = true;

	return ret;
}

//==============================================================
void COffHand::GetAvailableGrenades(std::vector<string> &grenades)
{
	if(!GetFireMode(0)->OutOfAmmo())
	{
		grenades.push_back(GetFireMode(0)->GetName());
	}

	if(!GetFireMode(1)->OutOfAmmo())
	{
		grenades.push_back(GetFireMode(1)->GetName());
	}

	if(!GetFireMode(2)->OutOfAmmo())
	{
		grenades.push_back(GetFireMode(2)->GetName());
	}

	if(!GetFireMode(3)->OutOfAmmo())
	{
		grenades.push_back(GetFireMode(3)->GetName());
	}
}

//==============================================================
int COffHand::CanExchangeWeapons(IItem *pItem, IItem **pExchangeItem)
{
	CPlayer* pPlayer = static_cast<CPlayer*>(GetOwnerActor());

	if(!pPlayer)
		return ITEM_NO_EXCHANGE;

	if(!pPlayer->CheckInventoryRestrictions(pItem->GetEntity()->GetClass()->GetName()))
	{
		//Can not carry more heavy/medium weapons
		IItem* pItem = GetExchangeItem(pPlayer);
		if(pItem)
		{
			*pExchangeItem = pItem;
			//can replace medium or heavy weapon
			return ITEM_CAN_EXCHANGE;
		}
		else
			return ITEM_NO_EXCHANGE;
	}

	return ITEM_CAN_PICKUP;
}

//==========================================================================
IItem* COffHand::GetExchangeItem(CPlayer *pPlayer)
{
	if(!pPlayer)
		return NULL;

	IItem *pItem = pPlayer->GetCurrentItem();
	const char *itemCategory = NULL;
	if(pItem)
	{
		itemCategory = m_pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName());
		if (!strcmp(itemCategory,"medium") || !strcmp(itemCategory,"heavy"))
			return pItem;
		else
		{
			int i = pPlayer->GetInventory()->GetCount();
			for(int w = i-1; w > -1; --w)
			{
				pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pPlayer->GetInventory()->GetItem(w));
				itemCategory = m_pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName());
				if (!strcmp(itemCategory,"medium") || !strcmp(itemCategory,"heavy"))
					return pItem;
			}
		}
	}
	return NULL;
}

//========================================================
EntityId COffHand::SpawnRockProjectile(IRenderNode* pRenderNode)
{
	Matrix34 statObjMtx;
	IStatObj *pStatObj=pRenderNode->GetEntityStatObj(0,0,&statObjMtx);
	assert(pStatObj);
	if (!pStatObj)
		return 0;

	pRenderNode->SetRndFlags(ERF_HIDDEN, true);
	pRenderNode->Dephysicalize();

	float scale=statObjMtx.GetColumn(0).GetLength();

	IEntityClass* pClass = m_pEntitySystem->GetClassRegistry()->FindClass("rock");
	if(!pClass)
		return 0;
	CProjectile *pRock=g_pGame->GetWeaponSystem()->SpawnAmmo(pClass);
	assert(pRock);
	if(!pRock)
		return 0;
	IEntity* pEntity = pRock->GetEntity();
	assert(pEntity);
	if (!pEntity)
		return 0;

	pEntity->SetStatObj(pStatObj, 0,true);
	pEntity->SetSlotFlags(0, pEntity->GetSlotFlags(0)|ENTITY_SLOT_RENDER);

	IEntityRenderProxy *pRenderProxy=static_cast<IEntityRenderProxy *>(pEntity->GetProxy(ENTITY_PROXY_RENDER));
	if (pRenderProxy)
	{
		pRenderProxy->GetRenderNode()->SetLodRatio(255);
		pRenderProxy->GetRenderNode()->SetViewDistRatio(255);
		pRenderProxy->GetRenderNode()->SetRndFlags(pRenderProxy->GetRenderNode()->GetRndFlags()|ERF_PICKABLE);
	}

	pRock->SetParams(GetOwnerId(), GetEntityId(), GetEntityId(), GetCurrentFireMode(), 75, 0);
	pRock->SetSequence(GenerateShootSeqN());

	pRock->Launch(statObjMtx.GetTranslation(), Vec3(0.0f,0.0f,0.0f), Vec3(0.0f,0.0f,0.0f), 1.0f);


	SEntityPhysicalizeParams params;
	params.type = PE_RIGID;
	params.nSlot = 0;
	params.mass = 2.0f;

	pe_params_buoyancy buoyancy;
	buoyancy.waterDamping = 1.5;
	buoyancy.waterResistance = 10;
	buoyancy.waterDensity = 0;
	params.pBuoyancy = &buoyancy;

	pEntity->Physicalize(params);

	return pEntity->GetId();
}

//==============================================================
//Handle entering cinematic (called from HUD)

void COffHand::OnBeginCutScene()
{
	if(IsHoldingEntity())
	{
		if(m_currentState&(eOHS_THROWING_NPC|eOHS_THROWING_OBJECT))
		{
			OnAction(GetOwnerId(),"use",eAAM_OnRelease,0.0f);
		}
		else
		{
			OnAction(GetOwnerId(),"use",eAAM_OnPress,0.0f);
			OnAction(GetOwnerId(),"use",eAAM_OnRelease,0.0f);
		}
	}

	m_bCutscenePlaying = true;
}

void COffHand::OnEndCutScene()
{
	m_bCutscenePlaying = false;
}

//==============================================================
void COffHand::AttachGrenadeToHand(int grenade, bool fp /*=true*/, bool attach /*=true*/)
{
	//Attach grenade to hand
	if(fp)
	{
		if(grenade==0)
			DoSwitchAccessory("OffhandGrenade");
		else if(grenade==1)
			DoSwitchAccessory("OffhandSmoke");
		else if(grenade==2)
			DoSwitchAccessory("OffhandFlashbang");
		else if(grenade==3)
			DoSwitchAccessory("OffhandNanoDisruptor");
	}
	else
	{
		ICharacterInstance* pOwnerCharacter = GetOwnerActor()?GetOwnerActor()->GetEntity()->GetCharacter(0):NULL;
		if(!pOwnerCharacter)
			return;

		IAttachmentManager *pAttachmentManager = pOwnerCharacter->GetIAttachmentManager();
		IAttachment *pAttachment = pAttachmentManager?pAttachmentManager->GetInterfaceByName(m_params.attachment[eIH_Left].c_str()):NULL;

		if(pAttachment)
		{
			//If there's an attachment, clear it
			if(!attach)
			{
				pAttachment->ClearBinding();
			}
			else
			{
				//If not it means we need to attach
				int slot = eIGS_Aux0;
				if((grenade == 1) || (grenade == 2))
					slot = eIGS_Aux1;

				if (IStatObj *pStatObj=GetEntity()->GetStatObj(slot))
				{
					CCGFAttachment *pCGFAttachment = new CCGFAttachment();
					pCGFAttachment->pObj = pStatObj;

					pAttachment->AddBinding(pCGFAttachment);
				}
			}

		}
	}

}

//==============================================================
EntityId	COffHand::GetHeldEntityId() const
{
	if(m_currentState&(eOHS_HOLDING_NPC|eOHS_HOLDING_OBJECT))
		return m_heldEntityId;

	return 0;
}
