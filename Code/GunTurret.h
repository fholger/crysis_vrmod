/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: GunTurret Implementation

-------------------------------------------------------------------------
History:
- 24:05:2006   14:00 : Created by Michael Rauh

*************************************************************************/
#ifndef __GunTurret_H__
#define __GunTurret_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IItemSystem.h>
#include "Weapon.h"
#include "Single.h"



class CGunTurret : public CWeapon, public IWeaponFiringLocator
{
	static const int ASPECT_GOALORIENTATION = eEA_GameServerDynamic;
	static const int ASPECT_STATEBITS = eEA_GameServerStatic;

protected:
	typedef struct SGunTurretParams
	{
		SGunTurretParams() 
		{ 
			mg_range = 20.0f;
			rocket_range=20.0f;
      aim_tolerance = 20.0f;
      prediction = 1.f;
			turn_speed = 1.5f;
			min_pitch = -30.f;
			max_pitch = 75.f;
			yaw_range = 90.f;
			species = 0;
			team = 0;
			enabled = true;
			searching = false;
      search_only = false;
			tac_range = 20.0f;
			search_speed = 1.0f;
			update_target_time = 1.5f;
			abandon_target_time = 0.5f;
			TAC_check_time = 0.5f;
      burst_time = 0.f;
      burst_pause = 0.f;
      sweep_time = 0.f;
      light_fov = 0.f;
      find_cloaked = true;
		}

		bool Reset(IScriptTable* params)
		{
			SmartScriptTable turret;
			if (!params->GetValue("GunTurret", turret))
				return false;

			turret->GetValue("bSurveillance", surveillance);
			turret->GetValue("bVehiclesOnly", vehicles_only);
			turret->GetValue("bEnabled", enabled);
			turret->GetValue("bSearching", searching);
      turret->GetValue("bSearchOnly", search_only);
			turret->GetValue("TurnSpeed", turn_speed);
			turret->GetValue("MinPitch", min_pitch);
			turret->GetValue("MaxPitch", max_pitch);
			turret->GetValue("YawRange", yaw_range);
			turret->GetValue("MGRange", mg_range);
			turret->GetValue("RocketRange", rocket_range);
			turret->GetValue("TACDetectRange",tac_range);
      turret->GetValue("AimTolerance", aim_tolerance);
      turret->GetValue("Prediction", prediction);
			turret->GetValue("SearchSpeed",search_speed);
			turret->GetValue("UpdateTargetTime",update_target_time);
			turret->GetValue("AbandonTargetTime",abandon_target_time);
			turret->GetValue("TACCheckTime",TAC_check_time);
      turret->GetValue("BurstTime", burst_time);
      turret->GetValue("BurstPause", burst_pause);
      turret->GetValue("SweepTime", sweep_time);
      turret->GetValue("LightFOV", light_fov);
      turret->GetValue("bFindCloaked", find_cloaked);

			params->GetValue("species", species);

      Limit(min_pitch, -90.f, 90.f);
      Limit(max_pitch, -90.f, 90.f);
      Limit(yaw_range, 0.f, 360.f);
      Limit(prediction, 0.f, 1.f);

			return true;
		}

		bool		surveillance;
		bool		vehicles_only;
		float		mg_range;    
		float		rocket_range;    
		float   tac_range;
    float   aim_tolerance;
    float   prediction; 
		float		turn_speed;
		float		min_pitch;
		float		max_pitch;
		float		yaw_range;
		int			species;
		int			team;
		bool        enabled;
		bool        searching;
    bool    search_only;
		float       search_speed;
		float       update_target_time;
		float       abandon_target_time;
		float       TAC_check_time;
    float   burst_time;
    float   burst_pause;
    float   sweep_time;
    float   light_fov;
    bool    find_cloaked;
	} SGunTurretParams;

  typedef struct SSearchParams
  {
    SSearchParams() { Reset(); };
    void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
    {
      CItemParamReader reader(params);
      ResetValue(light_helper, "");

      if (defaultInit)
      {
        hints.resize(0);
        light_helper.clear();
        light_texture.clear();
        light_material.clear();
        light_color = Vec3(1,1,1);
        light_diffuse_mul = 1.f;
        light_hdr_dyn = 0.f;
      }

      if (params)
      {
        const IItemParamsNode *phints = params->GetChild("hints");
        if (phints && phints->GetChildCount())
        {
          Vec2 h;
          int n = phints->GetChildCount();
          hints.resize(n);

          for (int i=0; i<n; ++i)
          {
            const IItemParamsNode *hint = phints->GetChild(i);
            if (hint->GetAttribute("x", h.x) && hint->GetAttribute("y", h.y))
            {
              Limit(h.x, -1.f, 1.f);
              Limit(h.y, -1.f, 1.f);
              hints[i]=h;
            }
          }
        }                
        if (const IItemParamsNode* pLight = params->GetChild("light"))
        {
          light_helper = pLight->GetAttribute("helper");
          light_texture = pLight->GetAttribute("texture");
          light_material = pLight->GetAttribute("material");
          pLight->GetAttribute("color", light_color);
          pLight->GetAttribute("diffuse_mul", light_diffuse_mul);
          pLight->GetAttribute("hdr_dyn", light_hdr_dyn);
        }
      }
    }

    std::vector<Vec2> hints;
    ItemString light_helper;
    ItemString light_texture;
    ItemString light_material;
    Vec3   light_color;
    float  light_diffuse_mul;
    float  light_hdr_dyn;

  } SSearchParams;

  typedef struct SFireParams
  {
    SFireParams() { Reset(); };
    void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
    { 
      CItemParamReader reader(params);
      ResetValue(deviation_speed, 0.f);
      ResetValue(deviation_amount, 0.f);
      ResetValue(randomness, 0.f);

      if (defaultInit)
      {
        hints.resize(0);        
      }

      if (params)
      {        
        const IItemParamsNode *phints = params->GetChild("hints");
        if (phints && phints->GetChildCount())
        {
          Vec2 h;
          int n = phints->GetChildCount();
          hints.resize(n);

          for (int i=0; i<n; ++i)
          {
            const IItemParamsNode *hint = phints->GetChild(i);
            if (hint->GetAttribute("x", h.x) && hint->GetAttribute("y", h.y))
            { 
              hints[i]=h;
            }
          }
        }   
      }
    }

    std::vector<Vec2> hints;
    float deviation_speed;
    float deviation_amount;
    float randomness;
  } SFireParams;

	enum ETargetClass//the higher the value the more the priority
	{
		eTC_NotATarget,
		eTC_Player,
		eTC_Vehicle,
		eTC_TACProjectile
	};

  template<typename T> struct TRandomVal
  {
    TRandomVal() { Range(0); }    
    
    ILINE void Range(T r) { range = r; New(); }
    ILINE void New() { val = Random(-range, range); }
    ILINE T Val() { return val; }
    
    T val;    
    T range; 
  };

  enum ERandomVals
  {
    eRV_UpdateTarget = 0,
    eRV_AbandonTarget,
    eRV_BurstTime,
    eRV_BurstPause,
    eRV_Last
  };

public:
	CGunTurret();
	virtual ~CGunTurret(){}

	// IGameObjectExtension
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );
	//~ IGameObjectExtension

	// IItem
	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	virtual void Update(SEntityUpdateContext& ctx, int update);
	virtual void OnReset();
	virtual void OnHit(float damage, const char* damageType);
	virtual void OnDestroyed();
	virtual void OnRepaired();
	virtual void DestroyedGeometry(bool use);
  virtual void SetCharacterAttachmentLocalTM(int slot, const char *name, const Matrix34 &tm);
  virtual bool SetAspectProfile( EEntityAspects aspect, uint8 profile );
  virtual void HandleEvent(const SGameObjectEvent&);
	virtual void PostInit(IGameObject * pGameObject);
	virtual bool CanPickUp(EntityId userId) const { return false; };
  // ~IItem

	// IWeapon
	virtual void StartFire(bool sec);
	virtual void StopFire(bool sec);
	virtual void StartFire() { CWeapon::StartFire(); };
	virtual void StopFire() { CWeapon::StopFire(); };
	virtual bool IsFiring(bool sec);
	virtual Vec3 GetFiringPos(const Vec3 &probableHit) const;

	virtual void SetDestinationEntity(EntityId targetId){ m_destinationId = targetId; }
	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CWeapon::GetMemoryStatistics(s); }
	// ~IWeapon

  // IWeaponFiringLocator
  virtual bool GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit) { return false; }
  virtual bool GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos) { return false; }
	virtual bool GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos);
	virtual bool GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos);
  virtual bool GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir) { return false; }
  virtual void WeaponReleased() {}
// IWeaponFiringLocator

protected:
	virtual bool ReadItemParams(const IItemParamsNode *params);

private:
	bool IsOperational();
	Vec3 GetTargetPos(IEntity* pEntity) const;
	Vec3 PredictTargetPos(IEntity* pTarget, bool sec);//sec - weapon to use
  Vec3 GetSweepPos(IEntity* pTarget, const Vec3& shootPos);
	bool GetTargetAngles(const Vec3& targetPos, float& z, float& x) const;

	bool IsTargetDead(IActor* pTarget) const;
	bool IsTargetHostile(IActor *pTarget) const;
	bool IsTargetSpectating(IActor* pTarget) const;
	bool IsTACBullet(IEntity* pTarget) const;
	ETargetClass GetTargetClass(IEntity* pTarget)const;

	bool IsInRange(const Vec3& pos, ETargetClass cl)const;
	bool IsTargetAimable(float angleYaw, float anglePitch) const;
	bool IsTargetShootable(IEntity* pTarget);
  bool RayCheck(IEntity* pTarget, const Vec3& pos, const Vec3& dir) const;
  bool IsTargetCloaked(IActor* pTarget) const;

	bool IsTargetRocketable(const Vec3 &pos) const;
	bool IsTargetMGable(const Vec3 &pos) const;
	bool IsAiming(const Vec3& pos, float treshold) const;
	IEntity *ResolveTarget(IEntity *pTarget) const;

	IEntity *GetClosestTarget();
	IEntity *GetClosestTACShell();

	void    OnTargetLocked(IEntity* pTarget);
	void    Activate(bool active);
	void    ServerUpdate(SEntityUpdateContext& ctx, int update);
	void		UpdateEntityProperties();
	void    UpdateGoal(IEntity* pTarget, float deltaTime);
	void    UpdateOrientation(float deltaTime);
	void    UpdateSearchingGoal(float deltaTime);
	void    UpdatePhysics();
	void    ChangeTargetTo(IEntity* pTarget);
  bool    UpdateBurst(float deltaTime);
  void    UpdateDeviation(float deltaTime, const Vec3& shootPos);

  void    ReadProperties(IScriptTable *pScriptTable);

	ILINE Vec3 GetWeaponPos() const;
  ILINE Vec3 GetWeaponDir() const;
  ILINE float GetYaw() const;
  ILINE float GetPitch() const;
  ILINE float GetBurstTime() const;

  void    DrawDebug();

	SGunTurretParams m_turretparams;
  SSearchParams m_searchparams;
  SFireParams m_fireparams;
  TRandomVal<float> m_randoms[eRV_Last];
	
	IFireMode *m_fm2;

	EntityId m_targetId;
	EntityId m_destinationId;

	float m_checkTACTimer;
	float	m_updateTargetTimer;
  float m_abandonTargetTimer;
  float m_burstTimer;
  float m_pauseTimer;
  float m_rayTimer;

  Vec3  m_deviationPos;
	float m_goalYaw;
	float	m_goalPitch;
	
  uint8 m_searchHint;
  uint8 m_fireHint;

	tSoundID m_turretSound,m_cannonSound,m_lightSound;
  uint m_lightId;  

  Matrix34 m_barrelRotation;

  string m_radarHelper;
  string m_barrelHelper;
  string m_fireHelper;
  string m_rocketHelper;

	Vec3   m_radarHelperPos;
	Vec3   m_barrelHelperPos;
	Vec3   m_fireHelperPos;
	Vec3   m_rocketHelperPos;

	bool   m_destroyed;
  bool   m_canShoot;
};

#endif // __GunTurret_H__
