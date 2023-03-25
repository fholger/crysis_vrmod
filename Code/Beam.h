/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Beam Fire Mode Implementation

-------------------------------------------------------------------------
History:
- 19:12:2005   12:16 : Created by Márcio Martins

*************************************************************************/
#ifndef __BEAM_H__
#define __BEAM_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Single.h"


class CBeam :
	public CSingle
{
	struct SBeamEffectParams
	{
		SBeamEffectParams() { Reset(); };
		void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
		{
			CItemParamReader reader(params);
		
			if (defaultInit)
			{
				effect[0].clear(); effect[1].clear();
				helper[0].clear(); helper[1].clear();
				scale[0]=scale[1]=1.0f;
			}

			if (params)
			{
				const IItemParamsNode *fp=params->GetChild("firstperson");
				if (fp)
				{
					effect[0] = fp->GetAttribute("effect");
					helper[0] = fp->GetAttribute("helper");
					fp->GetAttribute("scale", scale[0]);
				}

				const IItemParamsNode *tp=params->GetChild("thirdperson");
				if (tp)
				{
					effect[1] = tp->GetAttribute("effect");
					helper[1] = tp->GetAttribute("helper");
					tp->GetAttribute("scale", scale[1]);
				}

				PreLoadAssets();
			}
		};

		void PreLoadAssets()
		{
			for (int i = 0; i < 2; i++)
				gEnv->p3DEngine->FindParticleEffect(effect[i]);
		}

		void GetMemoryStatistics(ICrySizer * s)
		{
			for (int i=0; i<2; i++)
			{
				s->Add(effect[i]);
				s->Add(helper[i]);
			}
		}

		float	scale[2];
		string effect[2];
		string helper[2];

	};

	struct SBeamParams
	{
		SBeamParams() { Reset(); };
		void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
		{
			CItemParamReader reader(params);

			ResetValue(hit_effect, "");
			ResetValue(hit_effect_scale, 1.0f);
			ResetValue(hit_decal, "");
			ResetValue(hit_decal_size, 0.45f);
      ResetValue(hit_decal_size_min, 0.f);
			ResetValue(hit_decal_lifetime, 60.0f);
			ResetValue(range,			75.0f);
			ResetValue(tick,			0.25f);
			ResetValue(ammo_tick,	0.15f);
			ResetValue(ammo_drain,2);

			PreLoadAssets();
		};

		void PreLoadAssets()
		{
			gEnv->p3DEngine->FindParticleEffect(hit_effect);
		}
		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(hit_effect);
			s->Add(hit_decal);
		}

		string	hit_effect;
		float		hit_effect_scale;
		string	hit_decal;
		float		hit_decal_size;
    float		hit_decal_size_min;
		float		hit_decal_lifetime;

		float		range;
		float		tick;
		float		ammo_tick;
		int			ammo_drain;

	};
	struct SBeamActions
	{
		SBeamActions() { Reset(); };
		void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
		{
			CItemParamReader reader(params);

			ResetValue(blast,			"blast");
			ResetValue(hit,				"hit");
		};

		string	blast;
		string	hit;

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(blast);
			s->Add(hit);
		}
	};
public:
	CBeam();
	virtual ~CBeam();

	// IFireMode
	virtual void Update(float frameTime, uint frameId);
	virtual void GetMemoryStatistics(ICrySizer * s);

	virtual void ResetParams(const struct IItemParamsNode *params);
	virtual void PatchParams(const struct IItemParamsNode *patch);

	virtual void Activate(bool activate);

	//virtual bool OutOfAmmo() const;
	//virtual bool CanReload() const;

	virtual bool CanFire(bool considerAmmo = true) const;
	virtual void StartFire();
	virtual void StopFire();

	virtual void NetStartFire();
	virtual void NetStopFire();
	virtual const char *GetType() const { return "Beam"; };

	//~IFireMode

	virtual void Serialize(TSerialize ser) {};

	virtual void DecalLine(const Vec3 &org0, const Vec3 &org1, const Vec3 &hit0, const Vec3 &hit1, float step);
	virtual void Decal(const ray_hit &rayhit, const Vec3 &dir);
  virtual void Hit(ray_hit &hit, const Vec3 &dir);
	virtual void Tick(ray_hit &hit, const Vec3 &dir);
	virtual void TickDamage(ray_hit &hit, const Vec3 &dir, uint16 seq);

	virtual bool Shoot(bool resetAnimation, bool autoreload = true , bool noSound = false ) { return true; };
	virtual void NetShoot(const Vec3 &hit, int predictionHandle){};
	virtual void NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int predictionHandle){};

protected:
	SBeamParams				m_beamparams;
	SBeamActions			m_beamactions;
	SBeamEffectParams	m_effectparams;
	SBeamEffectParams	m_hitbeameffectparams;

	uint							m_effectId;
	uint							m_hitbeameffectId;
	tSoundID					m_fireLoopId;
	tSoundID					m_hitSoundId;
	bool							m_lastHitValid;
	bool							m_remote;
	float							m_tickTimer;
	float							m_ammoTimer;
	float							m_spinUpTimer;

	Vec3							m_lastHit;
	Vec3							m_lastOrg;

	bool              m_viewFP;

};


#endif //__BEAM_H__