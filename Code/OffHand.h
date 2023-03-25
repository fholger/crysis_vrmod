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

#ifndef __OFFHAND_H__
#define __OFFHAND_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <IItemSystem.h>
#include "Weapon.h"
#include "Player.h"
#include "IViewSystem.h"

#define OH_NO_GRAB					0
#define OH_GRAB_OBJECT			1
#define OH_GRAB_ITEM				2
#define OH_GRAB_NPC					3

enum EOffHandActions
{
	eOHA_NO_ACTION				= 0,
	eOHA_SWITCH_GRENADE		= 1,
	eOHA_THROW_GRENADE		= 2,
	eOHA_USE							= 3,
	eOHA_PICK_ITEM				= 4,
	eOHA_PICK_OBJECT			= 5,
	eOHA_THROW_OBJECT			= 6,
	eOHA_GRAB_NPC					= 7,
	eOHA_THROW_NPC				= 8,
	eOHA_RESET						= 9,
	eOHA_FINISH_AI_THROW_GRENADE = 10,
	eOHA_MELEE_ATTACK			= 11,
	eOHA_FINISH_MELEE			= 12,
	eOHA_REINIT_WEAPON		= 13,
};

enum EOffHandStates
{
	eOHS_INIT_STATE					= 0x00000001,
	eOHS_SWITCHING_GRENADE  = 0x00000002,
	eOHS_HOLDING_GRENADE		= 0x00000004,
	eOHS_THROWING_GRENADE   = 0x00000008,
	eOHS_PICKING						= 0x00000010,
	eOHS_PICKING_ITEM				= 0x00000020,
	eOHS_PICKING_ITEM2			= 0x00000040,
	eOHS_HOLDING_OBJECT			= 0x00000080,
	eOHS_THROWING_OBJECT		= 0x00000100,
	eOHS_GRABBING_NPC				= 0x00000200,
	eOHS_HOLDING_NPC				= 0x00000400,
	eOHS_THROWING_NPC				= 0x00000800,
	eOHS_TRANSITIONING			= 0x00001000,
	eOHS_MELEE							= 0x00002000
};

enum EOffHandSounds
{
	eOHSound_Choking_Trooper	= 0,
	eOHSound_Choking_Human		= 1,
	eOHSound_Kill_Human				= 2, 
	eOHSound_LastSound				= 3
};


class COffHand : public CWeapon
{

	typedef struct SGrabType
	{
		ItemString	helper;
		ItemString	pickup;
		ItemString	idle;
		ItemString	throwFM;
		bool		twoHanded;
	};

	typedef std::vector<SGrabType>				TGrabTypes;

public:

	COffHand();
	virtual ~COffHand();

	virtual void Update(SEntityUpdateContext &ctx, int slot);
	virtual void PostUpdate(float frameTime);
	virtual void PostInit(IGameObject * pGameObject );
	virtual void Reset();

	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);

	virtual bool CanSelect() const;
	virtual void Select(bool select);
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );
	virtual void PostSerialize();

	virtual void MeleeAttack();

	virtual void PostFilterView(struct SViewParams &viewParams);

	//Only needed because is used in CFists
	virtual void EnterWater(bool enter) {}

	virtual void UpdateFPView(float frameTime);

	//AIGrenades (for AI)
	virtual void PerformThrow(float speedScale);

	//Memory Statistics
	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CWeapon::GetMemoryStatistics(s); }

	void  SetOffHandState(EOffHandStates eOHS);
	ILINE int  GetOffHandState() { return m_currentState; } 
	void  FinishAction(EOffHandActions eOHA);
	virtual void Freeze(bool freeze);

	bool IsHoldingEntity();

	void OnBeginCutScene();
	void OnEndCutScene();

	void GetAvailableGrenades(std::vector<string> &grenades);

	ILINE void SetResetTimer(float t) { m_resetTimer = t;}
	ILINE int32 GetGrabType() const { return m_grabType; }

	float GetObjectMassScale();  //Scale for mouse sensitivity

	virtual bool ReadItemParams(const IItemParamsNode *root);

	void	SelectGrabType(IEntity* pEntity);

	bool EvaluateStateTransition(int requestedAction, int activationMode, int inputMethod);
	bool PreExecuteAction(int requestedAction, int activationMode, bool forceSelect =false);
	void CancelAction();

	void IgnoreCollisions(bool ignore, EntityId entityId=0);
	void DrawNear(bool drawNear, EntityId entityId=0);
	bool PerformPickUp();
	int  CanPerformPickUp(CActor *pActor, IPhysicalEntity *pPhysicalEntity = NULL, bool getEntityInfo = false);
	int  CheckItemsInProximity(Vec3 pos, Vec3 dir, bool getEntityInfo);

	void UpdateCrosshairUsabilitySP();
	void UpdateCrosshairUsabilityMP();
	void UpdateHeldObject();
	void UpdateGrabbedNPCState();
	void UpdateGrabbedNPCWorldPos(IEntity *pEntity, struct SViewParams *viewParams);

	void StartSwitchGrenade(bool xi_switch = false, bool fakeSwitch = false);
	void EndSwitchGrenade();

	//Offhand (for Player)
	void PerformThrow(int activationMode, EntityId throwableId, int oldFMId = -1, bool isLivingEnt = false);

	void StartPickUpItem();
	void EndPickUpItem();

	void PickUpObject(bool isLivingEnt = false);
	void ThrowObject(int activationMode, bool isLivingEnt = false);

	void NetStartFire();
	void NetStopFire();

	bool GrabNPC();
	void ThrowNPC(bool kill = true);

	//Special stuff for grabbed NPCs
	void RunEffectOnGrabbedNPC(CActor* pNPC);
	void PlaySound(EOffHandSounds sound, bool play);

	EntityId	GetHeldEntityId() const;

	void AttachGrenadeToHand(int grenade, bool fp = true, bool attach = true);

	virtual void ForcePendingActions() {}

private:

	EntityId SpawnRockProjectile(IRenderNode *pRenderNode);

	int		CanExchangeWeapons(IItem *pItem, IItem **pExchangeItem);
	IItem *GetExchangeItem(CPlayer *pPlayer);

	void	PostPostSerialize();

	//Grenade info
	int					m_lastFireModeId;
	float				m_nextGrenadeThrow;		

	float				m_lastCHUpdate;

	//All what we need for grabbing
	TGrabTypes		m_grabTypes;
	uint32				m_grabType;
	EntityId			m_heldEntityId, m_preHeldEntityId, m_crosshairId;
	Matrix34			m_holdOffset;
	Vec3          m_holdScale;
	int						m_constraintId;
	bool					m_hasHelper;
	int						m_grabbedNPCSpecies;
	float					m_heldEntityMass;
	
	float					m_killTimeOut;
	bool					m_killNPC;
	bool					m_effectRunning;
	bool					m_npcWasDead;
	bool					m_startPickUp;
	int						m_grabbedNPCInitialHealth;
	bool          m_forceThrow;

	tSoundID			m_sounds[eOHSound_LastSound];

	float					m_range;
	float					m_pickingTimer;
	float					m_resetTimer;

	int						m_usable;

	int           m_checkForConstraintDelay;
	bool          m_bCutscenePlaying;

	float					m_fGrenadeToggleTimer;
	float					m_fGrenadeThrowTimer;

	//Current state and pointers to actor main item(weapon) while offHand is selected
	int						m_currentState;
	CItem					*m_mainHand;
	CWeapon				*m_mainHandWeapon;
	EntityId			m_prevMainHandId;

	IRenderNode*  m_pRockRN;

	Matrix34      m_lastNPCMatrix;
	Matrix34			m_intialBoidLocalMatrix;

	bool					m_mainHandIsDualWield;
	bool					m_restoreStateAfterLoading;

	//Input actions
	static TActionHandler<COffHand> s_actionHandler;

	bool ProcessOffHandActions(EOffHandActions eOHA, int input, int activationMode, float value = 0.0f);
	bool OnActionUse(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionDrop(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionThrowGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionXIThrowGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionSwitchGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionXISwitchGrenade(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	bool OnActionSpecial(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	void RegisterActions();
};

#endif
