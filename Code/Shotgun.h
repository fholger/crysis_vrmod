// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "Single.h"
#pragma once

class CShotgun :
	public CSingle
{
	struct BeginReloadLoop;
	struct SliderBack;
	struct ZoomedCockAction;
	class ReloadOneShellAction;
	class ReloadEndAction;
	class ScheduleReload;

protected:
	typedef struct SShotgunParams
	{
		SShotgunParams() { Reset(); };
		void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
		{
			CItemParamReader reader(params);
			ResetValue(pellets,			10);
			ResetValue(pelletdamage,				20);
			ResetValue(spread,			.1f);
		}

		short pellets;
		short	pelletdamage;
		float spread;

	} SShotgunParams;
public:
	CShotgun(void);
	~CShotgun(void);
	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CSingle::GetMemoryStatistics(s); }
	virtual void Activate(bool activate);
	virtual void Reload(int zoomed);
	virtual void StartReload(int zoomed);
	virtual void ReloadShell(int zoomed);
	virtual void EndReload(int zoomed);
	using CSingle::EndReload;
	virtual void CancelReload();
	virtual bool CanCancelReload() { return false; };

	virtual bool CanFire(bool considerAmmo) const;

	virtual bool Shoot(bool resetAnimation, bool autoreload = true , bool noSound = false );
	virtual void NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int ph);

	virtual void ResetParams(const struct IItemParamsNode *params);
	virtual void PatchParams(const struct IItemParamsNode *patch);

	virtual const char* GetType() const;
protected:
	SShotgunParams m_shotgunparams;
	bool					 m_reload_pump;
	bool					 m_break_reload;
	bool					 m_reload_was_broken;

	int            m_max_shells;

};
