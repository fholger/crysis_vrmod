/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Radar Scanning Fire Mode Implementation

-------------------------------------------------------------------------
History:
- 18:1:2007   15:17 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCAN_H__
#define __SCAN_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Weapon.h"
#include "ItemParamReader.h"
#include "Single.h"


class CScan : public IFireMode
{
public:
	CScan();
	~CScan();

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
	virtual void CancelReload() { };
	virtual bool CanCancelReload() { return false; };

	virtual bool AllowZoom() const { return true; };
	virtual void Cancel() {};

	virtual float GetRecoil() const { return 0.0f; };
	virtual float GetSpread() const { return 0.0f; };
	virtual float GetMinSpread() const { return 0.0f; };
	virtual float GetMaxSpread() const { return 0.0f; };
	virtual const char *GetCrosshair() const { return ""; };
	virtual float GetHeat() const { return 0.0f; };
	virtual bool	CanOverheat() const {return false;};

	virtual bool CanFire(bool considerAmmo=true) const { return !m_scanning && !m_pWeapon->IsBusy(); };
	virtual void StartFire();
	virtual void StopFire();
	virtual bool IsFiring() const { return false; };

	virtual void NetShoot(const Vec3 &hit, int ph);
	virtual void NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int ph) {};
	virtual void NetEndReload() {};

	virtual void NetStartFire();
	virtual void NetStopFire();

	virtual EntityId GetProjectileId() const { return 0; };
	virtual EntityId RemoveProjectileId() { return 0;};
	virtual void SetProjectileId(EntityId id) { };

	virtual const char *GetType() const;
	virtual IEntityClass* GetAmmoType() const { return 0; };
	virtual int GetDamage(float distance=0.0f) const { return 0; };
	virtual const char *GetDamageType() const { return ""; };

	virtual float GetSpinUpTime() const { return 0.0f; };
	virtual float GetSpinDownTime() const { return 0.0f; };
	virtual float GetNextShotTime() const { return 0.0f; };
	virtual void SetNextShotTime(float time) {};
	virtual float GetFireRate() const { return 0.0f; };

	virtual void Enable(bool enable) { m_enabled = enable; };
	virtual bool IsEnabled() const { return m_enabled; };

	virtual Vec3 GetFiringPos(const Vec3 &probableHit) const { return ZERO;}
	virtual Vec3 GetFiringDir(const Vec3 &probableHit, const Vec3& firingPos) const { return ZERO;}
	virtual void SetName(const char *name) {  m_name = name; };
	virtual const char *GetName() { return m_name.empty()?0:m_name.c_str();};

	virtual bool HasFireHelper() const { return false; }
	virtual Vec3 GetFireHelperPos() const { return Vec3(ZERO); }
	virtual Vec3 GetFireHelperDir() const { return FORWARD_DIRECTION; }

	virtual int GetCurrentBarrel() const { return 0; }
	virtual void Serialize(TSerialize ser) {};
	virtual void PostSerialize() {};

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



protected:
	typedef struct SScanParams
	{
		SScanParams() { Reset(); };
		void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
		{
			CItemParamReader reader(params);

			ResetValue(range,			75.0f);
			ResetValue(delay,			1.0f);
			ResetValue(duration,	10.0f);
			ResetValue(tagDelay, 5.0f);
		}

		float range;
		float delay;
		float duration;
		float tagDelay;

		void GetMemoryStatistics(ICrySizer * s)
		{
		}
	} SScanParams;

	typedef struct SScanActions
	{
		SScanActions() { Reset(); };
		void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
		{
			CItemParamReader reader(params);

			ResetValue(spin_up,			"spin_up");
			ResetValue(spin_down,		"spin_down");
			ResetValue(scan,				"scan");
		}

		ItemString spin_up;
		ItemString spin_down;
		ItemString scan;

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(spin_up);
			s->Add(spin_down);
			s->Add(scan);
		}
	} SScanActions;

	SScanParams		m_scanparams;
	SScanActions	m_scanactions;

	CWeapon			*m_pWeapon;
	bool				m_enabled;
	string			m_name;

	tSoundID		m_scanLoopId;

	bool				m_scanning;
	float				m_delayTimer;
	float				m_durationTimer;
	float       m_tagEntitiesDelay;
};

#endif //__SCAN_H__
