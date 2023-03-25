/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: C++ Item Implementation
  
 -------------------------------------------------------------------------
  History:
  - 27:10:2004   11:25 : Created by Márcio Martins

*************************************************************************/
#ifndef __ITEM_H__
#define __ITEM_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IScriptSystem.h>
#include <IGameFramework.h>
#include <IItemSystem.h>
#include <IActionMapManager.h>
#include <CryCharAnimationParams.h>
#include <ISound.h>
#include <map>
#include <list>


#include "ItemScheduler.h"
#include "ItemString.h"

#define ITEM_ARMS_ATTACHMENT_NAME		"arms_fp"

#define ITEM_DESELECT_POSE					"nw"

#define ITEM_FIRST_PERSON_TOKEN			"fp"
#define ITEM_THIRD_PERSON_TOKEN			"tp"

struct ICharacterInstance;
struct AnimEventInstance;
struct IAttachmentObject;

enum EItemUpdateSlots
{
  eIUS_General = 0,
	eIUS_Zooming = 1,
	eIUS_FireMode = 2,  
	eIUS_Scheduler = 3,  
};

struct SItemStrings
{
	SItemStrings();
	~SItemStrings();

	// here they are
	ItemString activate;						// "activate";
	ItemString begin_reload;				// "begin_reload";
	ItemString cannon;							// "cannon";
	ItemString change_firemode;		// "change_firemode";
	ItemString change_firemode_zoomed; // "change_firemode_zoomed";
	ItemString crawl;							// "crawl";
	ItemString deactivate;					// "deactivate";
	ItemString deselect;						// "deselect";
	ItemString destroy;						// "destroy";
	ItemString enter_modify;				// "enter_modify";
	ItemString exit_reload_nopump; // "exit_reload_nopump";
	ItemString exit_reload_pump;		// "exit_reload_pump";
	ItemString fire;								// "fire";
	ItemString idle;								// "idle";
	ItemString idle_relaxed;				// "idle_relaxed";
	ItemString idle_raised;				// "idle_raised";
	ItemString jump_end;						// "jump_end";
	ItemString jump_idle;					// "jump_idle";
	ItemString jump_start;					// "jump_start";
	ItemString leave_modify;				// "leave_modify";
	ItemString left_item_attachment; // "left_item_attachment";
	ItemString lock;								// "lock";
	ItemString lower;							// "lower";
	ItemString modify_layer;       // "modify_layer";
	ItemString nw;									// "nw";
	ItemString offhand_on;					// "offhand_on";
	ItemString offhand_off;				// "offhand_off";
	ItemString offhand_on_akimbo;
	ItemString offhand_off_akimbo;
	ItemString pickedup;						// "pickedup";
	ItemString pickup_weapon_left; // "pickup_weapon_left";
	ItemString raise;							// "raise";
	ItemString reload_shell;				// "reload_shell";
	ItemString right_item_attachment;// "right_item_attachment";
	ItemString run_forward;				// "run_forward";
	ItemString SCARSleepAmmo;			// "SCARSleepAmmo";
	ItemString SCARTagAmmo;				// "SCARTagAmmo";
	ItemString select;							// "select";
	ItemString first_select;				// "first_select"; (For the LAW)
	ItemString select_grenade;			// "select_grenade";
	ItemString swim_idle;					// "swim_idle";
	ItemString swim_idle_2;				// "swim_idle_underwater";
	ItemString swim_forward;				// "swim_forward";
	ItemString swim_forward_2;			// "swim_forward_underwater";
	ItemString swim_backward;			// "swim_backward";
	ItemString speed_swim;					// "speed_swim";
	ItemString turret;							// "turret";
  ItemString enable_light;        
  ItemString disable_light;
  ItemString use_light;
	ItemString LAM;
	ItemString LAMRifle;
	ItemString LAMFlashLight;
	ItemString LAMRifleFlashLight;
	ItemString Silencer;
	ItemString SOCOMSilencer;

	ItemString lever_layer_1;
	ItemString lever_layer_2;

};

extern struct SItemStrings* g_pItemStrings;

class CActor;
class CItem :
	public CGameObjectExtensionHelper<CItem, IItem, 64>,
	public IGameObjectProfileManager
{
	friend class CScriptBind_Item;
public:
	struct AttachAction;
	struct DetachAction;
	struct SelectAction;

	//------------------------------------------------------------------------
	// Typedefs
	//------------------------------------------------------------------------
	enum eGeometrySlot
	{
		eIGS_FirstPerson = 0,
		eIGS_ThirdPerson,
		eIGS_Arms,
		eIGS_Aux0,
		eIGS_Owner,
		eIGS_OwnerLooped,
		eIGS_OffHand,
    eIGS_Destroyed,
    eIGS_Aux1,
		eIGS_ThirdPersonAux,
		eIGS_Last,
	};

	enum ePhysicalization
	{
		eIPhys_PhysicalizedRigid,
		eIPhys_PhysicalizedStatic,
		//eIPhys_DemoRecording,
		eIPhys_NotPhysicalized,
	};

	enum ETimer
	{
		eIT_Flying = 0,
		eIT_Last,
	};

	enum ePlayActionFlags
	{
		eIPAF_FirstPerson				= 1 << eIGS_FirstPerson,
		eIPAF_ThirdPerson				= 1 << eIGS_ThirdPerson,
		eIPAF_Arms							= 1 << eIGS_Arms,
		eIPAF_Aux0							= 1 << eIGS_Aux0,
		eIPAF_Owner							= 1 << eIGS_Owner,
		eIPAF_OwnerLooped					= 1 << eIGS_OwnerLooped,
    eIPAF_Destroyed         = 1 << eIGS_Destroyed,
		eIPAF_ForceFirstPerson	= 1 << 15,
		eIPAF_ForceThirdPerson	= 1 << 16,
		eIPAF_NoBlend						= 1 << 17,
		eIPAF_CleanBlending			= 1 << 18,
		eIPAF_Animation					= 1 << 19,
		eIPAF_Sound							= 1 << 20,
	  eIPAF_SoundLooped				= 1 << 21,
		eIPAF_SoundStartPaused	= 1 << 22,
		eIPAF_RestartAnimation	= 1 << 23,
		eIPAF_RepeatLastFrame	  = 1 << 24,
    eIPAF_Effect            = 1 << 25,
		eIPAF_Default						= eIPAF_FirstPerson|eIPAF_ThirdPerson|eIPAF_Owner|eIPAF_OwnerLooped|eIPAF_Sound|eIPAF_Animation|eIPAF_Effect,
	};

	enum eViewMode
	{
		eIVM_FirstPerson = 1,
		eIVM_ThirdPerson = 2,
	};

	enum eScriptEventTable
	{
		eISET_Server				= 1<<1,
		eISET_Client				= 1<<2,
		eISET_Root					= 1<<3,
		eISET_All						= eISET_Client|eISET_Server|eISET_Root,
		eISET_ClientServer	= eISET_Client|eISET_Server
	};

	enum eItemBackAttachment
	{
		eIBA_Primary = 0,
		eIBA_Secondary,
		eIBA_Unknown
	};

	struct SStats
	{
		SStats()
		:	fp(false),
			mounted(false),
			mount_last_aimdir(Vec3(0.0f,0.0f,0.0f)),
			mount_dir(Vec3(0.0f,0.0f,0.0f)),      
			pickable(true),
			selectable(true),
			selected(false),
			dropped(false),
			brandnew(true),      
			flying(false),      
			viewmode(0),
      used(false),
			sound_enabled(true),
			hand(eIH_Right),
      health(0.f),
			first_selection(true),
			backAttachment(eIBA_Unknown)
		{
		};

		void Serialize(TSerialize &ser)
		{
			ser.BeginGroup("ItemStats");
			//int vmode = viewmode;
			//ser.Value("viewmode", vmode);
			bool fly = flying;
			ser.Value("flying", fly);
			bool sel = selected;
			ser.Value("selected", sel);
			bool mount = mounted;
			ser.Value("mounted", mount);
			bool use = used;
			ser.Value("used", use);
			int han = hand;
			ser.Value("hand", han);
			bool drop = dropped;
			ser.Value("dropped", drop);
			//bool firstPerson = fp;
			//ser.Value("firstPerson", firstPerson);
			bool bnew = brandnew;
			ser.Value("brandnew", bnew);      
      float fHealth = health;
      ser.Value("health", fHealth);   
			bool bfirstSelection = first_selection;
			ser.Value("first_selection", bfirstSelection);
			int aback = (int)backAttachment;
			ser.Value("backAttachment", aback);
			bool pick = pickable;
			ser.Value("pickable", pick);
			ser.EndGroup();

			if(ser.IsReading())
			{
				//viewmode = vmode;
				flying = fly;
				selected = sel;
				mounted = mount;
				used = use;
				hand = han;
				dropped = drop;
				brandnew = bnew;        
				//fp = firstPerson;
        health = fHealth;
				first_selection = bfirstSelection;
				backAttachment = (eItemBackAttachment)aback;
				pickable = pick;
			}
		}

		Vec3	mount_dir;
		Vec3	mount_last_aimdir;
		int		hand:3;
		int		viewmode:3;
		float health;
		bool	fp:1;
		bool	mounted:1;
		bool	pickable:1;
		bool	selectable:1;
		bool	selected:1;
		bool	dropped:1;
		bool	brandnew:1;    
		bool	flying:1;
		bool	used:1;
		bool	sound_enabled:1;
		bool  first_selection:1;
		eItemBackAttachment   backAttachment;
	};

	struct SEditorStats
	{
		SEditorStats()
		:	ownerId(0),
			current(false)
		{};

		SEditorStats(EntityId _ownerId, bool _current)
		:	ownerId(_ownerId),
			current(_current)
		{};


		EntityId	ownerId;
		bool			current;
	};

	struct SParams
	{
		SParams()
		: selectable(true),
			droppable(true),
			pickable(true),
			mountable(true),
			usable(true),
			giveable(true),
			unique(true),
			arms(true),
			two_hand(0),      
			mass(3.5f),
			fly_timer(750),
			drop_impulse(12.5f),
			drop_impulse_pos(0.0075f,0,0.0075f),
			drop_angles(0,0,0),
			select_override(0.0f),
			prone_not_usable(false),
			raiseable(false),
			raise_distance(0.5f),
			update_hud(true),
			auto_droppable(false),
			has_first_select(false),
			attach_to_back(false),
			scopeAttachment(0),
			attachment_gives_ammo(false),
			select_on_pickup(false)
		{
			pose = g_pItemStrings->nw;
			attachment[eIH_Right]=g_pItemStrings->right_item_attachment;
			attachment[eIH_Left]=g_pItemStrings->left_item_attachment;
			bone_attachment_01.clear();
			bone_attachment_02.clear();
		};

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(pose);
			for (int i=0; i<eIH_Last; i++)
				s->Add(attachment[i]);
			s->Add(dual_wield_suffix);
			s->Add(dual_wield_pose);
			s->Add(display_name);
		}

		bool    prone_not_usable ;
		bool		raiseable        ;
		bool		selectable       ;
		bool		droppable        ;
		bool		pickable         ;
		bool		mountable        ;
		bool		usable           ;
		bool		giveable         ;
		bool		unique           ;
		bool		arms             ;
		short		two_hand; // 0 = one-hand, 1 = two-hand no grenades, 2 = two-hand w/ grenades

		int			fly_timer;
		float		mass;
		float		drop_impulse;
		float		select_override;
		float		raise_distance;
		Vec3		drop_impulse_pos;
		Vec3		drop_angles;
		bool    update_hud;
		bool		auto_droppable;
		int     scopeAttachment;
		bool    attachment_gives_ammo;

		ItemString	pose;
		ItemString	attachment[eIH_Last];
		ItemString	dual_wield_suffix;
		ItemString	dual_wield_pose;
		ItemString	display_name;

		bool				has_first_select;
		bool				attach_to_back;

		ItemString  bone_attachment_01;
		ItemString  bone_attachment_02;

		bool				select_on_pickup;
	};


	struct SMountParams
	{
		SMountParams()
		:	min_pitch(0.0f),
			max_pitch(80.0f),
			yaw_range(0.0f),
			eye_distance(0.85f),
			eye_height(0.0f),
			body_distance(0.55f)
		{};

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(pivot);
			s->Add(left_hand_helper);
			s->Add(right_hand_helper);
		}

		float		min_pitch;
		float		max_pitch;
		float		yaw_range;
		float		eye_distance;
		float		eye_height;
		float		body_distance;
		ItemString	pivot;
		ItemString	left_hand_helper;
		ItemString	right_hand_helper;
	};

	struct SDamageLevel
	{
		SDamageLevel(): max_health(100), min_health(0), effectId(-1) {};

		int max_health;
		int min_health;
		int effectId;
		float scale;
		ItemString effect;
		ItemString helper;

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(effect);
			s->Add(helper);
		}
	};

	struct SEntityProperties
	{
		SEntityProperties(): hitpoints(0), pickable(true), mounted(false), physics(false), usable(false) { initialSetup.resize(0);};

		int hitpoints;
		bool pickable;
		bool mounted;
		bool physics;
		bool usable;
		string initialSetup;
		
		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(initialSetup);
		}
	};

	struct SRespawnProperties
	{
		SRespawnProperties(): timer(0.0f), unique(false), respawn(false) {};
		float timer;
		bool	unique;
		bool	respawn;
	};

	struct SCameraAnimationStats
	{
		SCameraAnimationStats()
		{
			animating=false;
			position=true;
			rotation=true;
			follow=false;
			reorient=false;
			pos.zero();
			rot.SetIdentity();
		};

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(helper);
		}

		ItemString	helper;
		Vec3		pos;
		Quat		rot;
		bool		animating ;
		bool		position  ;
		bool		rotation  ;
		bool		follow    ;
		bool		reorient  ;
	};

	struct SAccessoryParams
	{
		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(attach_helper);
			s->Add(attach_action);
			s->Add(attach_layer);
			s->Add(detach_action);
			s->AddContainer(firemodes);
			for (std::vector<ItemString>::iterator iter = firemodes.begin(); iter != firemodes.end(); ++iter)
				s->Add(*iter);
			s->Add(switchToFireMode);
			s->Add(zoommode);
		}

		ItemString		attach_helper;
		ItemString		attach_action;
		ItemString		attach_layer;
		ItemString		detach_action;
		ItemString		switchToFireMode;
		ItemString		zoommode;
		std::vector< ItemString >   firemodes;
		const IItemParamsNode *params;
		bool      exclusive;
		bool			client_only;
	};

	struct SAudio
	{
		SAudio():	isstatic(false), sphere(0.0f), airadius(0.0f),issynched(false) {};

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(name);
		}

		ItemString		name;
		float			    airadius;
		float			    sphere;
		bool			    isstatic;
		bool          issynched;
	};

	struct SInstanceAudio
	{
		SInstanceAudio(): id(INVALID_SOUNDID),synch(false) {};
		ItemString		static_name;
		tSoundID	    id;
		bool          synch;

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(static_name);
		}
	};

	struct SLayer
	{
		void GetMemoryStatistics(ICrySizer * s)
		{
			s->AddContainer(bones);
			for (int i=0; i<bones.size(); i++)
				s->Add(bones[i]);
			for (int i=0; i<eIGS_Last; i++)
				s->Add(name[i]);
		}

		std::vector<ItemString>	bones;
		ItemString	name[eIGS_Last];
		int			id[eIGS_Last];
		bool		isstatic;
	};

	struct SAnimation
	{
		SAnimation(): camera_pos(true), camera_rot(true), camera_follow(false), camera_reorient(false) {};

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(name);
			s->Add(camera_helper);
		}

		ItemString	name;
		ItemString	camera_helper;
		float				speed;
		float				blend;
		bool				camera_pos;
		bool				camera_rot;
		bool				camera_follow;
		bool				camera_reorient;
	};

  struct SEffect
  {
    SEffect() {};

    void GetMemoryStatistics(ICrySizer *s)
    {
      s->Add(name);
      s->Add(helper);
    }

    ItemString name;
    ItemString helper;
  };

	struct SAction
	{
		void GetMemoryStatistics(ICrySizer * s)
		{
			for (int i=0; i<eIGS_Last; i++)
			{
				s->AddContainer(animation[i]);
				for (int j=0; j<animation[i].size(); j++)
					animation[i][j].GetMemoryStatistics(s);
			}
		}

		std::vector<SAnimation>	animation[eIGS_Last];
		SAudio			sound[2];
    SEffect     effect[2];
		bool				children;
	};

	struct SInstanceAction
	{
		SInstanceAudio sound[2];

		void GetMemoryStatistics(ICrySizer * s)
		{
			for (int i=0; i<2; i++)
				sound[i].GetMemoryStatistics(s);
		}
	};

	struct SAttachmentHelper
	{
		ItemString	name;
		ItemString	bone;
		int			slot;
		void GetMemoryStatistics(ICrySizer * s) const
		{
			s->Add(*this);
			s->Add(name);
			s->Add(bone);
		}
	};

	struct SEffectInfo
	{
		int			characterSlot;
		int			slot;
		ItemString	helper;

    SEffectInfo() : characterSlot(-1), slot(-1) {}
		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(helper);
		}
	};

	struct SGeometry
	{
		SGeometry(): position(ZERO), angles(ZERO), scale(1.0f) {};

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(name);
		}
		ItemString	name;
		Vec3		position;
		Ang3		angles;
		float		scale;
	};


	typedef std::map<ItemString, SAction>					TActionMap;
	typedef std::map<ItemString, SInstanceAction>	TInstanceActionMap;
	typedef std::map<ItemString, SLayer>					TLayerMap;
	typedef std::map<ItemString, int>							TActiveLayerMap;
	typedef std::vector<SAttachmentHelper>				THelperVector;
	typedef std::map<ItemString, bool>						TDualWieldSupportMap;
	typedef	std::map<uint, SEffectInfo>						TEffectInfoMap;
	typedef std::map<ItemString, EntityId>				TAccessoryMap;
	typedef std::map<ItemString, SAccessoryParams>TAccessoryParamsMap;
	typedef std::vector<ItemString>								TInitialSetup;
	typedef std::vector<SDamageLevel>							TDamageLevelVector;
	typedef std::map<IEntityClass*, int>					TAccessoryAmmoMap;

	static const int ASPECT_OWNER_ID	= eEA_GameServerStatic;

public:
	CItem();
	virtual ~CItem();

	// IItem, IGameObjectExtension
	virtual bool Init( IGameObject * pGameObject );
	virtual void InitClient(int channelId);
	virtual void PostInitClient(int channelId);
	virtual void PostInit( IGameObject * pGameObject );
	virtual void Release();
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );
	virtual void PostSerialize();
	virtual void PostPostSerialize();
	virtual void SerializeLTL( TSerialize ser );
	virtual void SerializeSpawnInfo( TSerialize ser ) {};
	virtual ISerializableInfoPtr GetSpawnInfo() { return 0;};
	virtual void Update( SEntityUpdateContext& ctx, int );
	virtual void PostUpdate( float frameTime ) {};
	virtual void PostRemoteSpawn() {};
	virtual void HandleEvent( const SGameObjectEvent& );
	virtual void ProcessEvent(SEntityEvent& );
	virtual void SetChannelId(uint16 id) {};
	virtual void SetAuthority(bool auth) {}
	virtual void GetMemoryStatistics(ICrySizer * s);

	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	virtual void UpdateFPView(float frameTime);
	virtual void UpdateFPPosition(float frameTime);
	virtual void UpdateFPCharacter(float frameTime);
	virtual bool FilterView(struct SViewParams &viewParams);
	virtual void PostFilterView(struct SViewParams &viewParams);

	virtual bool CheckAmmoRestrictions(EntityId pickerId);

	virtual void EnableAnimations(bool enable) { m_enableAnimations = enable; };

	virtual IWeapon *GetIWeapon() { return 0; };

	virtual void SetOwnerId(EntityId ownerId);
	virtual EntityId GetOwnerId() const;

	virtual void Reset();
  virtual void ResetOwner();

	virtual void RemoveEntity(bool force = false);

	virtual void SetParentId(EntityId parentId);
	virtual EntityId GetParentId() const;

	virtual void SetHand(int hand);
	virtual void Use(EntityId userId);
	virtual void Select(bool select);
	virtual void Drop(float impulseScale=1.0f, bool selectNext=true, bool byDeath=false);
	virtual void PickUp(EntityId pickerId, bool sound, bool select=true, bool keepHistory=true);
	virtual void Physicalize(bool enable, bool rigid);
	virtual void Pickalize(bool enable, bool dropped);
	virtual void IgnoreCollision(bool ignore);
	virtual void ForceArms(ICharacterInstance *pCharacter) { m_pForcedArms = pCharacter; };
	virtual void AttachArms(bool attach, bool shadow);
	virtual void Impulse(const Vec3 &position, const Vec3 &direction, float impulse);

	virtual bool CanPickUp(EntityId userId) const;
	virtual bool CanDrop() const;
	virtual bool CanUse(EntityId userId) const;
	virtual bool IsMountable() const { return m_params.mountable; }
	virtual bool IsMounted() const;
	virtual bool IsUsed() const;

	virtual bool InitRespawn();
	virtual void TriggerRespawn();
	virtual void TakeAccessories(EntityId receiver);

	virtual void Cloak(bool cloak, IMaterial *cloakMat = 0);
	virtual void SetMaterialRecursive(ICharacterInstance *charInst, bool undo, IMaterial *newMat);

	void CloakEnable(bool enable, bool fade);
	void CloakSync(bool fade);
	void FrostEnable(bool enable, bool fade);
	void FrostSync(bool fade);
	void WetEnable(bool enable, bool fade);
	void WetSync(bool fade);
	void ApplyMaterialLayerSettings(uint8 mask, uint32 blend);


	virtual bool IsTwoHand() const;
	virtual int TwoHandMode() const;
	virtual bool SupportsDualWield(const char *itemName) const;
	virtual void ResetDualWield();
	virtual IItem *GetDualWieldSlave() const;
	virtual EntityId GetDualWieldSlaveId() const;
	virtual IItem *GetDualWieldMaster() const;
	virtual EntityId GetDualWieldMasterId() const;
	virtual void SetDualWieldMaster(EntityId masterId);
	virtual void SetDualWieldSlave(EntityId slaveId);
	virtual bool IsDualWield() const;
	virtual bool IsDualWieldMaster() const;
	virtual bool IsDualWieldSlave() const;

	virtual Vec3 GetMountedAngleLimits() const;
	virtual Vec3 GetMountedDir() const {return GetStats().mount_dir;}
	virtual void SetMountedAngleLimits(float min_pitch, float max_pitch, float yaw_range);

	virtual void EnableSelect(bool enable);	
	virtual bool CanSelect() const;
	virtual bool IsSelected() const;
	virtual void OnParentSelect(bool select) {};

	virtual void MountAt(const Vec3 &pos);
	virtual void MountAtEntity(EntityId entityId, const Vec3 &pos, const Ang3 &angles);
	virtual void StartUse(EntityId userId);
	virtual void StopUse(EntityId  userId);
	virtual void ApplyViewLimit(EntityId userId, bool apply);

	virtual void EnableSound(bool enable);
	virtual bool IsSoundEnabled() const;
	virtual bool IsModifying() { return m_modifying || m_transitioning; }
  virtual bool IsDestroyed() { return m_properties.hitpoints > 0 && m_stats.health <= 0.f; }

	virtual void EnterWater(bool enter) {};
	// ~IItem

	// IGameObjectProfileManager
	virtual bool SetAspectProfile( EEntityAspects aspect, uint8 profile );
	virtual uint8 GetDefaultProfile( EEntityAspects aspect );
	// ~IGameObjectProfileManager
  
	// Events
	virtual void OnStartUsing();
	virtual void OnStopUsing();
	virtual void OnSelect(bool select);
	virtual void OnSelected(bool selected);
	virtual void OnEnterFirstPerson();
	virtual void OnEnterThirdPerson();
  virtual void OnReset();
	virtual void OnPickedUp(EntityId actorId, bool destroyed);
	virtual void OnDropped(EntityId actorId);
  
	virtual const SStats &GetStats() const { return m_stats; };
	virtual const SParams &GetParams() const { return m_params; };
	virtual const SEntityProperties &GetProperties() const { return m_properties; };

	// Silhouette purpose: we need to get all the currently attached accessories to highlight them
	const TAccessoryMap *GetAttachedAccessories() const { return &m_accessories; }

public:

	// params
	virtual bool ReadItemParams(const IItemParamsNode *root);
	virtual bool ReadParams(const IItemParamsNode *params);
	virtual bool ReadGeometry(const IItemParamsNode *geometry);
	virtual bool ReadActions(const IItemParamsNode *actions);
	virtual bool ReadAction(const IItemParamsNode *action, SAction *pAction);
	virtual bool ReadDamageLevels(const IItemParamsNode *damagelevels);
	virtual bool ReadLayers(const IItemParamsNode *layers);
	virtual bool ReadLayer(const IItemParamsNode *layer, SLayer *pLayer);
	virtual bool ReadAccessories(const IItemParamsNode *accessories);
	virtual bool ReadAccessoryParams(const IItemParamsNode *accessory, SAccessoryParams *params);
	virtual bool ReadAccessoryAmmo(const IItemParamsNode *ammo);
	virtual bool SetGeometryFromParams(int slot, const IItemParamsNode *geometry);
	virtual int TargetToSlot(const char *name);
	virtual void ReadProperties(IScriptTable *pScriptTable);

	// accessories
	virtual CItem *AddAccessory(const ItemString& name);
	virtual void RemoveAccessory(const ItemString& name);
	virtual void RemoveAllAccessories();
	virtual void AttachAccessory(const ItemString& name, bool attach, bool noanim, bool force = false, bool initialSetup = false);
	virtual void AttachAccessoryPlaceHolder(const ItemString& name, bool attach);
	virtual CItem *GetAccessoryPlaceHolder(const ItemString& name);
	virtual CItem *GetAccessory(const ItemString& name);
	virtual SAccessoryParams *GetAccessoryParams(const ItemString& name);
	virtual bool IsAccessoryHelperFree(const ItemString& helper);
	virtual void InitialSetup();
	virtual void PatchInitialSetup(); 
	virtual void ReAttachAccessories();
	virtual void ReAttachAccessory(const ItemString& name);
	virtual void ReAttachAccessory(EntityId id);
	virtual void AccessoriesChanged();
	virtual void FixAccessories(SAccessoryParams *newParams, bool attach) {};
	virtual void ResetAccessoriesScreen(CActor* pOwner);

	virtual void SwitchAccessory(const ItemString& accessory);
	virtual void DoSwitchAccessory(const ItemString& accessory);
	virtual void ScheduleAttachAccessory(const ItemString& accessoryName, bool attach);
	virtual const char* CurrentAttachment(const ItemString& attachmentPoint);

	virtual void AddAccessoryAmmoToInventory(IEntityClass* pAmmoType, int count, CActor* pOwner = NULL);
	virtual int  GetAccessoryAmmoCount(IEntityClass* pAmmoType);
	virtual bool GivesAmmo() { return (!m_bonusAccessoryAmmo.empty() || m_params.attachment_gives_ammo);}

	// effects
	uint AttachEffect(int slot, uint id, bool attach, const char *effectName=0, const char *helper=0,
		const Vec3 &offset=Vec3Constants<float>::fVec3_Zero, const Vec3 &dir=Vec3Constants<float>::fVec3_OneY, float scale=1.0f, bool prime=true);
  uint AttachLight(int slot, uint id, bool attach, float radius=5.0f, const Vec3 &color=Vec3Constants<float>::fVec3_One,
		const float fSpecularMult=1.0f, const char *projectTexture=0, float projectFov=0, const char *helper=0,
		const Vec3 &offset=Vec3Constants<float>::fVec3_Zero, const Vec3 &dir=Vec3Constants<float>::fVec3_OneY, 
    const char* material=0, float fHDRDynamic=0.f );
	uint AttachLightEx(int slot, uint id, bool attach, bool fakeLight = false , bool castShadows = false, IRenderNode* pCasterException = NULL, float radius=5.0f, const Vec3 &color=Vec3Constants<float>::fVec3_One,
		const float fSpecularMult=1.0f, const char *projectTexture=0, float projectFov=0, const char *helper=0,
		const Vec3 &offset=Vec3Constants<float>::fVec3_Zero, const Vec3 &dir=Vec3Constants<float>::fVec3_OneY, 
		const char* material=0, float fHDRDynamic=0.f );
	void SpawnEffect(int slot, const char *effectName, const char *helper, const Vec3 &offset=Vec3Constants<float>::fVec3_Zero,
		const Vec3 &dir=Vec3Constants<float>::fVec3_OneY, float scale=1.0f);
	IParticleEmitter *GetEffectEmitter(uint id) const;
	void SetEffectWorldTM(uint id, const Matrix34 &tm);
	Matrix34 GetEffectWorldTM(uint it);
  void EnableLight(bool enable, uint id);
	void SetLightRadius(float radius, uint id);

	// misc
	bool AttachToHand(bool attach, bool checkAttachment = false);
	bool AttachToBack(bool attach);
	void EnableUpdate(bool enable, int slot=-1);
	void RequireUpdate(int slot=-1);
	void Hide(bool hide);
	void HideArms(bool hide);
	void HideItem(bool hide);
	virtual void SetBusy(bool busy) { m_scheduler.SetBusy(busy); };
	virtual bool IsBusy() const { return m_scheduler.IsBusy(); };
	CItemScheduler *GetScheduler() { return &m_scheduler; };
	IItemSystem *GetIItemSystem() { return m_pItemSystem; };
	virtual void SetDualSlaveAccessory(bool noNetwork = false);

	IEntity *GetOwner() const;
	CActor *GetOwnerActor() const;
	CActor *GetActor(EntityId actorId) const;
  IInventory *GetActorInventory(IActor *pActor) const;
	CItem *GetActorItem(IActor *pActor) const;
	EntityId GetActorItemId(IActor *pActor) const;
  EntityId GetHostId() const { return m_hostId; }
	CActor *GetActorByNetChannel(INetChannel *pNetChannel) const;

	const char *GetDisplayName() const;

	virtual void ForcePendingActions() {}

	//Special FP weapon render method
	virtual void SetFPWeapon(float dt, bool registerByPosition = false);

	// view
	bool IsOwnerFP();
	bool IsCurrentItem();
	void UpdateMounted(float frameTime);  
	void CheckViewChange();
	void SetViewMode(int mode);
	void CopyRenderFlags(IEntity *pOwner);
	void ResetRenderFlags();
  virtual void UseManualBlending(bool enable);
  virtual bool GetAimBlending(OldBlendSpace& params);
  virtual void UpdateIKMounted(IActor* pActor, const Vec3& vGunXAxis);

	// character attachments
	bool CreateCharacterAttachment(int slot, const char *name, int type, const char *bone);
	void DestroyCharacterAttachment(int slot, const char *name);
	void ResetCharacterAttachment(int slot, const char *name);
	const char *GetCharacterAttachmentBone(int slot, const char *name);
	void SetCharacterAttachment(int slot, const char *name, IEntity *pEntity, int flags);
	void SetCharacterAttachment(int slot, const char *name, IStatObj *pObj, int flags);
	void SetCharacterAttachment(int slot, const char *name, ICharacterInstance *pCharacter, int flags);
	void SetCharacterAttachment(int slot, const char *name, CDLight &light, int flags);
	void SetCharacterAttachment(int slot, const char *name, IEntity *pEntity, int objSlot, int flags);
	virtual void SetCharacterAttachmentLocalTM(int slot, const char *name, const Matrix34 &tm);
	void SetCharacterAttachmentWorldTM(int slot, const char *name, const Matrix34 &tm);
	Matrix34 GetCharacterAttachmentLocalTM(int slot, const char *name);
	Matrix34 GetCharacterAttachmentWorldTM(int slot, const char *name);
	void HideCharacterAttachment(int slot, const char *name, bool hide);
	void HideCharacterAttachmentMaster(int slot, const char *name, bool hide);

	void CreateAttachmentHelpers(int slot);
	void DestroyAttachmentHelpers(int slot);
	const THelperVector& GetAttachmentHelpers();

	// freeze
	virtual void Freeze(bool freeze);

  // damage
  virtual void OnHit(float damage, const char* damageType);
  virtual void OnDestroyed();
  virtual void OnRepaired();
	virtual void DestroyedGeometry(bool use);
	virtual void UpdateDamageLevel();

	// resource
	virtual bool SetGeometry(int slot, const ItemString& name, const Vec3& poffset=Vec3(0,0,0), const Ang3& aoffset=Ang3(0,0,0), float scale=1.0f, bool forceReload=false);
	void SetDefaultIdleAnimation(int slot, const ItemString& actionName);
	const char *GetDefaultIdleAnimation(int slot) { return m_idleAnimation[slot]; };
	void ForceSkinning(bool always);
	void EnableHiddenSkinning(bool force);

	typedef CryFixedStringT<256> TempResourceName;
	void FixResourceName(const ItemString& name, TempResourceName& fixedName, int flags, const char *hand=0, const char *suffix=0, const char *pose=0, const char *pov=0, const char *env=0);
	virtual tSoundID PlayAction(const ItemString& action, int layer=0, bool loop=false, uint flags = eIPAF_Default, float speedOverride = -1.0f);
	void PlayAnimation(const char* animationName, int layer=0, bool loop=false, uint flags = eIPAF_Default);
	void PlayAnimationEx(const char* animationName, int slot=eIGS_FirstPerson, int layer=0, bool loop=false, float blend=0.175f, float speed=1.0f, uint flags = eIPAF_Default);
	void PlayLayer(const ItemString& name, int flags = eIPAF_Default, bool record=true);
	void StopLayer(const ItemString& name, int flags = eIPAF_Default, bool record=true);
	void RestoreLayers();
	void ResetAnimation(int layer=0, uint flags = eIPAF_Default);
	uint GetCurrentAnimationTime(int slot);
	uint GetCurrentAnimationEnd(int slot);
	uint GetCurrentAnimationStart(int slot);
	void DrawSlot(int slot, bool draw, bool near=false);
	Vec3 GetSlotHelperPos(int slot, const char *helper, bool worldSpace, bool relative=false);
	const Matrix33 &GetSlotHelperRotation(int slot, const char *helper, bool worldSpace, bool relative=false);
	void StopSound(tSoundID id);
	void Quiet();
	ISound *GetISound(tSoundID id);
	void ReleaseStaticSound(SInstanceAudio *sound);
	void ReleaseStaticSounds();

	void SetActionSuffix(const char *suffix) { m_actionSuffix = string(suffix); };
	const char *GetActionSuffix(const char *suffix) const { return m_actionSuffix.c_str(); };

	virtual void OnAttach(bool attach);

	IEntitySoundProxy *GetSoundProxy(bool create=false);
	IEntityRenderProxy *GetRenderProxy(bool create=false);
	IEntityPhysicalProxy *GetPhysicalProxy(bool create=false);
		
	EntityId NetGetOwnerId() const;
	void NetSetOwnerId(EntityId id);

	//LAW special stuff
	bool IsAutoDroppable() { return m_params.auto_droppable;}

	struct RequestAttachAccessoryParams
	{
		RequestAttachAccessoryParams() {};
		RequestAttachAccessoryParams(const char *name): accessory(name) {};
		void SerializeWith(TSerialize ser)
		{
			ser.Value("accessory", accessory);
		};

		string accessory;
	};

	struct EmptyParams
	{
		void SerializeWith(TSerialize ser) {};
	};

	DECLARE_SERVER_RMI_NOATTACH_FAST(SvRequestAttachAccessory, RequestAttachAccessoryParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH_FAST(ClAttachAccessory, RequestAttachAccessoryParams, eNRT_ReliableUnordered);
	
	DECLARE_SERVER_RMI_NOATTACH_FAST(SvRequestEnterModify, EmptyParams, eNRT_ReliableUnordered);
	DECLARE_SERVER_RMI_NOATTACH_FAST(SvRequestLeaveModify, EmptyParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH_FAST(ClEnterModify, EmptyParams, eNRT_ReliableUnordered);
	DECLARE_CLIENT_RMI_NOATTACH_FAST(ClLeaveModify, EmptyParams, eNRT_ReliableUnordered);

	// properties
	template<typename T>bool GetEntityProperty(const char *name, T &value) const
	{
		return GetEntityProperty(GetEntity(), name, value);
	}

  template<typename T>bool GetEntityProperty(IEntity* pEntity, const char *name, T &value) const
  {
    SmartScriptTable props;
    IScriptTable* pScriptTable = pEntity->GetScriptTable();
    if (pScriptTable && pScriptTable->GetValue("Properties", props))
      return props->GetValue(name, value);
    return false;
  }
  
	template<typename T>bool GetEntityProperty(const char *table, const char *name, T &value) const
	{
    return GetEntityProperty(GetEntity(), table, name, value);
	}

  template<typename T>bool GetEntityProperty(IEntity* pEntity, const char *table, const char *name, T &value) const
  {
    SmartScriptTable props;
    IScriptTable* pScriptTable = pEntity->GetScriptTable();
    if (pScriptTable && pScriptTable->GetValue("Properties", props))
    {
      SmartScriptTable subprop;
      if (props->GetValue(table, subprop))
        return subprop->GetValue(name, value);
    }
    return false;
  }

  template<typename T>void SetEntityProperty(const char *name, const T &value)
  {
    return SetEntityProperty(GetEntity(), name, value);
  }

  template<typename T>void SetEntityProperty(IEntity* pEntity, const char *name, const T &value)
  {
    SmartScriptTable props;
    IScriptTable* pScriptTable = pEntity->GetScriptTable();
    if (pScriptTable && pScriptTable->GetValue("Properties", props))
      props->SetValue(name, value);    
  }

	// script
	bool CallScriptEvent(IScriptTable *pScriptTable, const char *name)
	{
		IScriptSystem *pSS = pScriptTable->GetScriptSystem();
		if (pScriptTable->GetValueType(name) != svtFunction)
			return false;
		pSS->BeginCall(pScriptTable, name); pSS->PushFuncParam(GetEntity()->GetScriptTable());
		bool result = false;
		pSS->EndCall(result);
		return result;
	}
	template<typename P1>
		bool CallScriptEvent(IScriptTable *pScriptTable, const char *name, P1 &p1)
	{
		IScriptSystem *pSS = pScriptTable->GetScriptSystem();
		if (pScriptTable->GetValueType(name) != svtFunction)
			return false;
		pSS->BeginCall(pScriptTable, name); pSS->PushFuncParam(GetEntity()->GetScriptTable());
		pSS->PushFuncParam(p1);
		bool result = false;
		pSS->EndCall(result);
		return result;
	}
	template<typename P1, typename P2>
		bool CallScriptEvent(IScriptTable *pScriptTable, const char *name, P1 &p1, P2 &p2)
	{
		IScriptSystem *pSS = pScriptTable->GetScriptSystem();
		if (pScriptTable->GetValueType(name) != svtFunction)
			return false;
		pSS->BeginCall(pScriptTable, name); pSS->PushFuncParam(GetEntity()->GetScriptTable());
		pSS->PushFuncParam(p1); pSS->PushFuncParam(p2);
		bool result = false;
		pSS->EndCall(result);
		return result;
	}
	template<typename P1, typename P2, typename P3, typename P4>
		bool CallScriptEvent(IScriptTable *pScriptTable, const char *name, P1 &p1, P2 &p2, P3 &p3)
	{
		IScriptSystem *pSS = pScriptTable->GetScriptSystem();
		if (pScriptTable->GetValueType(name) != svtFunction)
			return false;
		pSS->BeginCall(pScriptTable, name); pSS->PushFuncParam(GetEntity()->GetScriptTable());
		pSS->PushFuncParam(p1); pSS->PushFuncParam(p2); pSS->PushFuncParam(p3);
		bool result = false;
		pSS->EndCall(result);
		return result;
	}
	template<typename P1, typename P2, typename P3, typename P4>
		bool CallScriptEvent(IScriptTable *pScriptTable, const char *name, P1 &p1, P2 &p2, P3 &p3, P4 &p4)
	{
		IScriptSystem *pSS = pScriptTable->GetScriptSystem();
		if (pScriptTable->GetValueType(name) != svtFunction)
			return false;
		pSS->BeginCall(pScriptTable, name); pSS->PushFuncParam(GetEntity()->GetScriptTable());
		pSS->PushFuncParam(p1); pSS->PushFuncParam(p2); pSS->PushFuncParam(p3); pSS->PushFuncParam(p4);
		bool result = false;
		pSS->EndCall(result);
		return result;
	}
	void CallScriptEvent(int tables, const char *name, bool *rserver, bool *rclient, bool *rroot)
	{
		if ((tables&eISET_Root) && !!m_stateTable[0])
		{
			bool r = CallScriptEvent(m_stateTable[0], name);
			if(rroot) *rroot = r;
		}
		if ((tables&eISET_Server) && !!m_stateTable[1] && gEnv->bServer)
		{
			bool s = CallScriptEvent(m_stateTable[1], name);
			if(rserver) *rserver = s;
		}
		if ((tables&eISET_Client) && !!m_stateTable[2] && gEnv->bClient)
		{
			bool c = CallScriptEvent(m_stateTable[2], name);
			if(rclient) *rclient = c;
		}
	}

	template<typename P1>
		void CallScriptEvent(int tables, const char *name, P1 &p1, bool *rserver, bool *rclient, bool *rroot)
	{
		if ((tables&eISET_Root) && !!m_stateTable[0])
		{
			bool r = CallScriptEvent(m_stateTable[0], name, p1);
			if(rroot) *rroot = r;
		}
		if ((tables&eISET_Server) && !!m_stateTable[1] && IsServer())
		{
			bool s = CallScriptEvent(m_stateTable[1], name, p1);
			if(rserver) *rserver = s;
		}
		if ((tables&eISET_Client) && !!m_stateTable[2] && IsClient())
		{
			bool c = CallScriptEvent(m_stateTable[2], name, p1);
			if(rclient) *rclient = c;
		}
	}
	template<typename P1, typename P2>
		void CallScriptEvent(int tables, const char *name, P1 &p1, P2 &p2, bool *rserver, bool *rclient, bool *rroot)
	{
		if ((tables&eISET_Root) && !!m_stateTable[0])
		{
			bool r = CallScriptEvent(m_stateTable[0], name, p1, p2);
			if(rroot) *rroot = r;
		}
		if ((tables&eISET_Server) && !!m_stateTable[1] && IsServer())
		{
			bool s = CallScriptEvent(m_stateTable[1], name, p1, p2);
			if(rserver) *rserver = s;
		}
		if ((tables&eISET_Client) && !!m_stateTable[2] && IsClient())
		{
			bool c = CallScriptEvent(m_stateTable[2], name, p1, p2);
			if(rclient) *rclient = c;
		}
	}
	template<typename P1, typename P2, typename P3>
		void CallScriptEvent(int tables, const char *name, P1 &p1, P2 &p2, P3 &p3, bool *rserver, bool *rclient, bool *rroot)
	{
		if ((tables&eISET_Root) && !!m_stateTable[0])
		{
			bool r = CallScriptEvent(m_stateTable[0], name, p1, p2, p3);
			if(rroot) *rroot = r;
		}
		if ((tables&eISET_Server) && !!m_stateTable[1] && IsServer())
		{
			bool s = CallScriptEvent(m_stateTable[1], name, p1, p2, p3);
			if(rserver) *rserver = s;
		}
		if ((tables&eISET_Client) && !!m_stateTable[2] && IsClient())
		{
			bool c = CallScriptEvent(m_stateTable[2], name, p1, p2, p3);
			if(rclient) *rclient = c;
		}
	}
	template<typename P1, typename P2, typename P3, typename P4>
		void CallScriptEvent(int tables, const char *name, P1 &p1, P2 &p2, P3 &p3, P4 &p4, bool *rserver, bool *rclient, bool *rroot)
	{
		if ((tables&eISET_Root) && !!m_stateTable[0])
		{
			bool r = CallScriptEvent(m_stateTable[0], name, p1, p2, p3, p4);
			if(rroot) *rroot = r;
		}
		if ((tables&eISET_Server) && !!m_stateTable[1] && IsServer())
		{
			bool s = CallScriptEvent(m_stateTable[1], name, p1, p2, p3, p4);
			if(rserver) *rserver = s;
		}
		if ((tables&eISET_Client) && !!m_stateTable[2] && IsClient())
		{
			bool c = CallScriptEvent(m_stateTable[2], name, p1, p2, p3, p4);
			if(rclient) *rclient = c;
		}
	};

	ILINE bool IsServer() {	return gEnv->bServer; }
	ILINE bool IsClient() {	return gEnv->bClient; }
protected:
	// data
	_smart_ptr<class CItemSharedParams> m_sharedparams;

	SParams								m_params;
	SMountParams					m_mountparams;
	SStats								m_stats;
	SEditorStats					m_editorstats;
	SCameraAnimationStats	m_camerastats;
	TEffectInfoMap				m_effects;
	TActiveLayerMap				m_activelayers;
	TAccessoryMap					m_accessories;
	TInstanceActionMap		m_instanceActions;

	TAccessoryAmmoMap			m_bonusAccessoryAmmo;

	TDamageLevelVector		m_damageLevels;

	TInitialSetup					m_initialSetup;
		
	uint									m_effectGenId;

	EntityId							m_dualWieldMasterId;
	EntityId							m_dualWieldSlaveId;

	CItemScheduler				m_scheduler;

	EntityId							m_ownerId;
	EntityId							m_parentId;

	SmartScriptTable			m_stateTable[3];		// root, server, client

	ItemString						m_idleAnimation[eIGS_Last];
	uint									m_animationTime[eIGS_Last];
	uint									m_animationEnd[eIGS_Last];
	float									m_animationSpeed[eIGS_Last];

	string								m_actionSuffix;
	string								m_actionSuffixSerializationHelper;

	ICharacterInstance		*m_pForcedArms;

	SGeometry							m_fpgeometry[3];		// have to save the first person geometry
																						// to support dynamic change of hand
	SRespawnProperties		m_respawnprops;
	SEntityProperties			m_properties;
	EntityId							m_hostId;
	EntityId							m_postSerializeMountedOwner;

	IScriptTable					*m_pEntityScript;

	static IEntitySystem		*m_pEntitySystem;
	static IItemSystem			*m_pItemSystem;
	static IGameFramework		*m_pGameFramework;
	static IGameplayRecorder*m_pGameplayRecorder;

	bool									m_bPostPostSerialize;

protected:
	ItemString						m_geometry[eIGS_Last];
	bool									m_modifying;
	bool									m_transitioning;
	bool									m_cloaked;
	bool									m_serializeCloaked; //used for cloaked serialization
	bool									m_frozen;
	bool									m_enableAnimations;

	int										m_constraintId;

	bool                  m_useFPCamSpacePP; //Enables special processing of FP weapons in camera space (prevent jittering)

	Vec3									m_serializeActivePhysics;
	bool									m_serializeDestroyed;
	bool                  m_serializeRigidPhysics;

public:

	bool									m_noDrop;				//Fix reseting problem in editor
	static IEntityClass*	sOffHandClass;
	static IEntityClass*	sFistsClass;
	static IEntityClass*	sAlienCloak;
	static IEntityClass*	sSOCOMClass;
	static IEntityClass*	sDetonatorClass;
	static IEntityClass*	sC4Class;
	static IEntityClass*	sBinocularsClass;
	static IEntityClass*	sGaussRifleClass;
	static IEntityClass*	sDebugGunClass;
	static IEntityClass*	sRefWeaponClass;
	static IEntityClass*	sClaymoreExplosiveClass;
	static IEntityClass*	sAVExplosiveClass;
	static IEntityClass*	sDSG1Class;
	static IEntityClass*	sTACGunClass;
	static IEntityClass*	sTACGunFleetClass;
	static IEntityClass*	sAlienMountClass;
	static IEntityClass*	sRocketLauncherClass;
	static IEntityClass*	sLAMFlashLight;
	static IEntityClass*	sLAMRifleFlashLight;
	static IEntityClass*	sFlashbangGrenade;
	static IEntityClass*	sExplosiveGrenade;
	static IEntityClass*	sEMPGrenade;
	static IEntityClass*	sSmokeGrenade;

	static IEntityClass*  sIncendiaryAmmo;

	static IEntityClass*  sScarGrenadeClass;

};


#endif //__ITEM_H__
