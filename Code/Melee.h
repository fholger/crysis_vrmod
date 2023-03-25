/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Beam Fire Mode Implementation

-------------------------------------------------------------------------
History:
- 23:3:2006   13:02 : Created by Márcio Martins

*************************************************************************/
#ifndef __MELEE_H__
#define __MELEE_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Weapon.h"
#include "Fists.h"
#include "ItemParamReader.h"
#include "IGameRulesSystem.h"


#define ResetValue(name, defaultValue) if (defaultInit) name=defaultValue; reader.Read(#name, name)
#define ResetValueEx(name, var, defaultValue) if (defaultInit) var=defaultValue; reader.Read(name, var)

class CMelee :
	public IFireMode
{
	struct StopAttackingAction;

	typedef struct SMeleeParams
	{
		SMeleeParams() { Reset(); };
		void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
		{
			CItemParamReader reader(params);
			ResetValue(helper,				"");
			ResetValue(range,					1.75f);
			ResetValue(damage,				32);
			ResetValue(damageAI,			10);
			ResetValue(crosshair,			"default");
			ResetValue(hit_type,			"melee");
			ResetValue(impulse,				50.0f);
			ResetValue(delay,					0.5f);
			ResetValue(duration,			0.5f);
		}

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(crosshair);
			s->Add(hit_type);
		}

		ItemString	helper;
		float				range;

		short				damage;
		short				damageAI;
		ItemString	crosshair;
		ItemString	hit_type;

		float		impulse;

		float		delay;
		float		duration;

	} SMeleeParams;

	typedef struct SMeleeActions
	{
		SMeleeActions() { Reset(); };
		void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
		{
			CItemParamReader reader(params);
			ResetValue(attack,			"attack");
			ResetValue(hit,					"hit");
		}

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(attack);
			s->Add(hit);
		}

		ItemString	attack;
		ItemString	hit;
	} SMeleeActions;

public:
	CMelee();
	virtual ~CMelee();

	//IFireMode
	virtual void Init(IWeapon *pWeapon, const struct IItemParamsNode *params);
	virtual void Update(float frameTime, uint frameId);
	virtual void PostUpdate(float frameTime) {};
	virtual void UpdateFPView(float frameTime) {};
	virtual void Release();
	virtual void GetMemoryStatistics(ICrySizer * s);

	virtual void ResetParams(const struct IItemParamsNode *params);
	virtual void PatchParams(const struct IItemParamsNode *patch);

	virtual void Activate(bool activate);

	virtual int GetAmmoCount() const { return 0; };
	virtual int GetClipSize() const { return 0; };

	virtual bool OutOfAmmo() const { return false; };
	virtual bool CanReload() const { return false; };
	virtual void Reload(int zoomed) {};
	virtual bool IsReloading() { return false; };
	virtual void CancelReload() {};
	virtual bool CanCancelReload() { return false; };

	virtual bool AllowZoom() const { return true; };
	virtual void Cancel() {};

	virtual float GetRecoil() const { return 0.0f; };
	virtual float GetSpread() const { return 0.0f; };
	virtual float GetMinSpread() const { return 0.0f; }
	virtual float GetMaxSpread() const { return 0.0f; }
	virtual const char *GetCrosshair() const { return ""; };

	virtual bool CanFire(bool considerAmmo=true) const;
	virtual void StartFire();
	virtual void StopFire();
	virtual bool IsFiring() const { return m_attacking; };
	virtual float GetHeat() const { return 0.0f; };
	virtual bool	CanOverheat() const {return false;};

	virtual void NetShoot(const Vec3 &hit, int ph);
	virtual void NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int ph);
	virtual void NetEndReload() {};

	virtual void NetStartFire();
	virtual void NetStopFire();

	virtual EntityId GetProjectileId() const { return 0; };
	virtual EntityId RemoveProjectileId() { return 0; };
	virtual void SetProjectileId(EntityId id) {};

	virtual const char *GetType() const;
	virtual IEntityClass* GetAmmoType() const { return 0; };
	virtual int GetDamage(float distance=0.0f) const;
	virtual const char *GetDamageType() const;

	virtual float GetSpinUpTime() const { return 0.0f; };
	virtual float GetSpinDownTime() const { return 0.0f; };
  virtual float GetNextShotTime() const { return 0.0f; };
	virtual void SetNextShotTime(float time) {};
  virtual float GetFireRate() const { return 0.0f; };

	virtual void Enable(bool enable) { m_enabled = enable; };
	virtual bool IsEnabled() const { return m_enabled; };

	virtual Vec3 GetFiringPos(const Vec3 &probableHit) const {return ZERO;}
	virtual Vec3 GetFiringDir(const Vec3 &probableHit, const Vec3& firingPos) const {return ZERO;}
	virtual void SetName(const char *name) { m_name = name; };
	virtual const char *GetName() { return m_name.empty()?0:m_name.c_str();};

  virtual bool HasFireHelper() const { return false; }
  virtual Vec3 GetFireHelperPos() const { return Vec3(ZERO); }
  virtual Vec3 GetFireHelperDir() const { return FORWARD_DIRECTION; }

  virtual int GetCurrentBarrel() const { return 0; }
	virtual void Serialize(TSerialize ser) {};
	virtual void PostSerialize(){};

	virtual void SetRecoilMultiplier(float recoilMult) { }
	virtual float GetRecoilMultiplier() const { return 1.0f; }

	virtual void PatchSpreadMod(SSpreadModParams &sSMP){};
	virtual void ResetSpreadMod(){};

	virtual void PatchRecoilMod(SRecoilModParams &sRMP){};
	virtual void ResetRecoilMod(){};

	virtual void ResetLock() {};
	virtual void StartLocking(EntityId targetId, int partId) {};
	virtual void Lock(EntityId targetId, int partId) {};
	virtual void Unlock() {};
	//~IFireMode

	virtual bool PerformRayTest(const Vec3 &pos, const Vec3 &dir, float strength, bool remote);
	virtual bool PerformCylinderTest(const Vec3 &pos, const Vec3 &dir, float strength, bool remote);
	virtual void Hit(ray_hit *hit, const Vec3 &dir, float damageScale, bool remote);
	virtual void Hit(geom_contact *contact, const Vec3 &dir, float damageScale, bool remote);
	virtual void Hit(const Vec3 &pt, const Vec3 &dir, const Vec3 &normal, IPhysicalEntity *pCollider, int partId, int ipart, int surfaceIdx, float damageScale, bool remote);
	virtual void Impulse(const Vec3 &pt, const Vec3 &dir, const Vec3 &normal, IPhysicalEntity *pCollider, int partId, int ipart, int surfaceIdx, float impulseScale);

	virtual void ApplyCameraShake(bool hit);

	//Special case when performing melee with an object (offHand)
	//It must ignore also the held entity!
	virtual void IgnoreEntity(EntityId entityId) { m_ignoredEntity = entityId; }
	virtual void MeleeScale(float scale) { m_meleeScale = scale; }
	virtual float GetOwnerStrength() const;

protected:
	CWeapon *m_pWeapon;
	bool		m_enabled;

	bool		m_attacking;
	Vec3		m_last_pos;

	float		m_delayTimer;
	float		m_durationTimer;
	string	m_name;
	Vec3		m_beginPos;
	bool		m_attacked;

	EntityId	m_ignoredEntity;
	float			m_meleeScale;

	SMeleeParams	m_meleeparams;
	SMeleeActions	m_meleeactions;

	bool			m_noImpulse;
};


#endif //__MELEE_H__