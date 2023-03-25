// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "Shotgun.h"
#include "Game.h"
#include "WeaponSystem.h"
#include "Actor.h"
#include "Player.h"
#include "Projectile.h"
#include "GameRules.h"
#include "GameCVars.h"

CShotgun::CShotgun(void)
{
}

CShotgun::~CShotgun(void)
{
}

void CShotgun::Activate(bool activate)
{
	CSingle::Activate(activate);

	m_next_shot = 0.1f;
	m_reload_pump = true;
	m_break_reload = false;
	m_reload_was_broken = false;
	m_pWeapon->RequireUpdate(eIUS_FireMode);
}

void CShotgun::Reload(int zoomed)
{
	StartReload(zoomed);
}

// Move shotgun into reloading pose
struct CShotgun::BeginReloadLoop
{
	BeginReloadLoop(CShotgun *_shotty, int _zoomed): shotty(_shotty), zoomed(_zoomed) {};
	CShotgun *shotty;
	int zoomed;

	void execute(CItem *_this)
	{
		shotty->ReloadShell(zoomed);
	}
};

struct CShotgun::SliderBack
{
	SliderBack(CShotgun *_shotgun): shotgun(_shotgun) {};
	CShotgun *shotgun;

	void execute(CItem *_this)
	{
		_this->StopLayer(shotgun->m_fireparams.slider_layer);
	}
};

void CShotgun::StartReload(int zoomed)
{
	//If reload was broken to shoot, do not start a reload before shooting
	if(m_break_reload)
		return;

	int ammoCount = m_pWeapon->GetAmmoCount(m_fireparams.ammo_type_class);
	if ((ammoCount >= m_fireparams.clip_size) || m_reloading)
		return;

	m_max_shells = m_fireparams.clip_size - ammoCount;

	m_reload_was_broken = false;
	m_reloading = true;
	if (zoomed)
		m_pWeapon->ExitZoom();

	IEntityClass* ammo = m_fireparams.ammo_type_class;
	m_pWeapon->OnStartReload(m_pWeapon->GetOwnerId(), ammo);

	m_pWeapon->PlayAction(g_pItemStrings->begin_reload, 0, false, CItem::eIPAF_Default|CItem::eIPAF_RepeatLastFrame);

	m_reload_pump = ammoCount > 0 ? false : true;
	uint animTime = m_pWeapon->GetCurrentAnimationTime(CItem::eIGS_FirstPerson);
	if(animTime==0)
		animTime = 500; //For DS
	m_pWeapon->GetScheduler()->TimerAction(animTime, CSchedulerAction<BeginReloadLoop>::Create(BeginReloadLoop(this, zoomed)), false);

	m_pWeapon->GetScheduler()->TimerAction((uint)((m_fireparams.reload_time-0.125f)*1000), CSchedulerAction<SliderBack>::Create(this), false);
}

// Reload shells
class CShotgun::ReloadOneShellAction
{
public:
	ReloadOneShellAction(CWeapon *_wep, int _zoomed)
	{
		pWep = _wep;
		rzoomed = _zoomed;
	}
	void execute(CItem *_this)
	{
		CShotgun *fm = (CShotgun *)pWep->GetFireMode(pWep->GetCurrentFireMode());

		if(fm->m_reload_was_broken)
			return;

		IEntityClass* pAmmoType = fm->GetAmmoType();

		if (pWep->IsServer())
		{
			int ammoCount = pWep->GetAmmoCount(pAmmoType);
			pWep->SetAmmoCount(pAmmoType, ammoCount+1);
			pWep->SetInventoryAmmoCount(pAmmoType, pWep->GetInventoryAmmoCount(pAmmoType)-1);
		}

		if (!fm->m_break_reload)
			fm->ReloadShell(rzoomed);
		else
			fm->EndReload(rzoomed);
	}
private:
	CWeapon *pWep;
	int rzoomed;
};

void CShotgun::ReloadShell(int zoomed)
{
	if(m_reload_was_broken)
		return;

	CActor* pOwner = m_pWeapon->GetOwnerActor();
	bool isAI = pOwner && (pOwner->IsPlayer() == false);
	int ammoCount = m_pWeapon->GetAmmoCount(m_fireparams.ammo_type_class);
	if ((ammoCount < m_fireparams.clip_size) && (m_max_shells>0) &&
		(isAI || (m_pWeapon->GetInventoryAmmoCount(m_fireparams.ammo_type_class) > 0)) ) // AI has unlimited ammo
	{
		m_max_shells --;
		
		// reload a shell
		m_pWeapon->PlayAction(g_pItemStrings->reload_shell, 0, false, CItem::eIPAF_Default|CItem::eIPAF_RepeatLastFrame|CItem::eIPAF_RestartAnimation);
		uint animTime = m_pWeapon->GetCurrentAnimationTime(CItem::eIGS_FirstPerson);
		if(animTime==0)
			animTime = 530; //For DS
		m_pWeapon->GetScheduler()->TimerAction(animTime, CSchedulerAction<ReloadOneShellAction>::Create(ReloadOneShellAction(m_pWeapon, zoomed)), false);
		// call this again
	}
	else
	{
		EndReload(zoomed);
	}
}

// Move shotgun out of reloading pose
class CShotgun::ReloadEndAction
{
public:
	ReloadEndAction(CWeapon *_wep, int _zoomed)
	{
		pWep = _wep;
		rzoomed = _zoomed;
	}
	void execute(CItem *_this)
	{
		CShotgun *fm = (CShotgun *)pWep->GetFireMode(pWep->GetCurrentFireMode());
		IEntityClass* ammo = fm->GetAmmoType();
		pWep->OnEndReload(pWep->GetOwnerId(), ammo);

		fm->m_reloading = false;
		fm->m_emptyclip = false;
		fm->m_spinUpTime = fm->m_firing?fm->m_fireparams.spin_up_time:0.0f;

		if (fm->m_break_reload)
		{
			fm->Shoot(true, true);
			fm->m_break_reload=false;
		}
		else if(fm->m_reload_was_broken)
		{
			if(pWep->GetOwnerActor() && pWep->GetOwnerActor()->IsClient())
				pWep->MeleeAttack();
		}

	}
private:
	CWeapon *pWep;
	int		rzoomed;
};

void CShotgun::EndReload(int zoomed)
{
	CActor *pActor = m_pWeapon->GetOwnerActor();

	float speedOverride = 1.0f;
	if(m_reload_was_broken)
		speedOverride = 1.75f;

	uint animTime = 100;

	if (m_reload_pump && !m_reload_was_broken)
		m_pWeapon->PlayAction(g_pItemStrings->exit_reload_pump,0,false,CItem::eIPAF_Default,speedOverride);
	else
		m_pWeapon->PlayAction(g_pItemStrings->exit_reload_nopump,0,false,CItem::eIPAF_Default,speedOverride);

	animTime = m_pWeapon->GetCurrentAnimationTime(CItem::eIGS_FirstPerson);

	if(!m_reload_was_broken)
		m_pWeapon->GetScheduler()->TimerAction(animTime, CSchedulerAction<ReloadEndAction>::Create(ReloadEndAction(m_pWeapon, zoomed)), false);
	else
		m_pWeapon->GetScheduler()->TimerAction(100, CSchedulerAction<ReloadEndAction>::Create(ReloadEndAction(m_pWeapon, zoomed)), false);

}

bool CShotgun::CanFire(bool considerAmmo) const
{
	return (m_next_shot<=0.0f) && (m_spinUpTime<=0.0f) &&
		!m_pWeapon->IsBusy() && !m_pWeapon->IsSwitchingFireMode() && (!considerAmmo || !OutOfAmmo() || !m_fireparams.ammo_type_class || m_fireparams.clip_size == -1);
}

class CShotgun::ScheduleReload
{
public:
	ScheduleReload(CWeapon *wep)
	{
		_pWeapon = wep;
	}
	void execute(CItem *item) 
	{
		_pWeapon->Reload();
	}
private:
	CWeapon *_pWeapon;
};

struct CShotgun::ZoomedCockAction
{
	CShotgun *pShotty;
	ZoomedCockAction(CShotgun *_shotty): pShotty(_shotty) {};
	void execute(CItem *pItem)
	{
		pItem->PlayAction(pShotty->m_actions.cock_sound);
	}
};

bool CShotgun::Shoot(bool resetAnimation, bool autoreload/* =true */, bool noSound /* =false */)
{
	IEntityClass* ammo = m_fireparams.ammo_type_class;
	int ammoCount = m_pWeapon->GetAmmoCount(ammo);

	if (m_fireparams.clip_size==0)
		ammoCount = m_pWeapon->GetInventoryAmmoCount(ammo);

	CActor *pActor = m_pWeapon->GetOwnerActor();
	bool playerIsShooter = pActor?pActor->IsPlayer():false;

	if (!CanFire(true))
	{
		if ((ammoCount <= 0) && (!m_reloading))
		{
			m_pWeapon->PlayAction(m_actions.empty_clip);
			//Auto reload
			m_pWeapon->Reload();
		}	
		return false;
	}
	else if(m_pWeapon->IsWeaponLowered())
	{
		m_pWeapon->PlayAction(m_actions.null_fire);
		return false;
	}

	if (m_reloading)
	{
		if(m_pWeapon->IsBusy())
			m_pWeapon->SetBusy(false);
		
		if(CanFire(true) && !m_break_reload)
		{
			m_break_reload = true;
			m_pWeapon->RequestCancelReload();
		}
		return false;
	}

	// Aim assistance
	m_pWeapon->AssistAiming();

	const char *action = m_actions.fire_cock.c_str();
	bool zoomedCock = m_fireparams.unzoomed_cock && m_pWeapon->IsZoomed() && (ammoCount != 1);
	if (ammoCount == 1 || zoomedCock)
		action = m_actions.fire.c_str();

	m_pWeapon->PlayAction(action, 0, false, CItem::eIPAF_Default|CItem::eIPAF_RestartAnimation|CItem::eIPAF_CleanBlending);
	if(zoomedCock)
		m_pWeapon->GetScheduler()->TimerAction(250, CSchedulerAction<ZoomedCockAction>::Create(this), false);


	Vec3 hit = GetProbableHit(WEAPON_HIT_RANGE);
	Vec3 pos = GetFiringPos(hit);
	Vec3 fdir = ApplySpread(GetFiringDir(hit, pos), GetSpread());
	Vec3 vel = GetFiringVelocity(fdir);
	Vec3 dir;

	CheckNearMisses(hit, pos, fdir, WEAPON_HIT_RANGE, m_shotgunparams.spread);
	
	bool serverSpawn = m_pWeapon->IsServerSpawn(ammo);
	uint16 seqn=m_pWeapon->GenerateShootSeqN();
	uint16 seqr=m_shotgunparams.pellets-1;

	for (int i = 0; i < m_shotgunparams.pellets; i++)
	{
		CProjectile *pAmmo = m_pWeapon->SpawnAmmo(ammo, false);
		if (pAmmo)
		{
			dir = ApplySpread(fdir, m_shotgunparams.spread);      
      int hitTypeId = g_pGame->GetGameRules()->GetHitTypeId(m_fireparams.hit_type.c_str());			
      
			pAmmo->SetParams(m_pWeapon->GetOwnerId(), m_pWeapon->GetHostId(), m_pWeapon->GetEntityId(), m_pWeapon->GetFireModeIdx(GetName()),
				m_shotgunparams.pelletdamage, hitTypeId);
			pAmmo->SetSequence(m_pWeapon->GetShootSeqN());
			pAmmo->SetDestination(m_pWeapon->GetDestination());
			pAmmo->Launch(pos, dir, vel);

			if ((!m_tracerparams.geometry.empty() || !m_tracerparams.effect.empty()) && (ammoCount==GetClipSize() || (ammoCount%m_tracerparams.frequency==0)))
				EmitTracer(pos,hit,false);

			m_projectileId = pAmmo->GetEntity()->GetId();
		}

		m_pWeapon->GenerateShootSeqN();
	}

	m_pWeapon->OnShoot(m_pWeapon->GetOwnerId(), 0, ammo, pos, dir, vel);

	if (m_pWeapon->IsServer())
		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponShot, ammo->GetName(), m_shotgunparams.pellets, (void *)m_pWeapon->GetEntityId()));

	MuzzleFlashEffect(true);
	RejectEffect();

	m_fired = true;
	m_next_shot += m_next_shot_dt;
	m_zoomtimeout = m_next_shot + 0.5f;
	ammoCount--;

	if (playerIsShooter)
	{
		if (pActor->InZeroG())
		{
			IEntityPhysicalProxy *pPhysicsProxy = (IEntityPhysicalProxy*)pActor->GetEntity()->GetProxy(ENTITY_PROXY_PHYSICS);
			SMovementState ms;
			pActor->GetMovementController()->GetMovementState(ms);
			CPlayer *plr = (CPlayer *)pActor;
			if(m_recoilparams.back_impulse > 0.0f)
			{
				Vec3 impulseDir = ms.aimDirection * -1.0f;
				Vec3 impulsePos = ms.pos;
				float impulse = m_recoilparams.back_impulse;
				if(plr->GetNanoSuit() && plr->GetNanoSuit()->GetMode() == NANOMODE_STRENGTH)
					impulse *= 0.75f;
				pPhysicsProxy->AddImpulse(-1, impulsePos, impulseDir * impulse * 100.0f, true, 1.0f);
			}
			if(m_recoilparams.angular_impulse > 0.0f)
			{
				float impulse = m_recoilparams.angular_impulse;
				if(plr->GetNanoSuit() && plr->GetNanoSuit()->GetMode() == NANOMODE_STRENGTH)
					impulse *= 0.5f;
				pActor->AddAngularImpulse(Ang3(0,impulse,0), 1.0f);
			}
		}
		if(pActor->IsClient())
			if (gEnv->pInput) 
				gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.15f, 0.0f, fabsf(m_recoilparams.back_impulse)*3.0f) );
	}
	if (m_fireparams.clip_size != -1  && !g_pGameCVars->i_unlimitedammo)
	{
		if (m_fireparams.clip_size!=0)
			m_pWeapon->SetAmmoCount(ammo, ammoCount);
		else
			m_pWeapon->SetInventoryAmmoCount(ammo, ammoCount);
	}

	if ((ammoCount<1) && !m_fireparams.slider_layer.empty())
	{
		const char *slider_back_layer = m_fireparams.slider_layer.c_str();
		m_pWeapon->PlayLayer(slider_back_layer, CItem::eIPAF_Default|CItem::eIPAF_NoBlend);
	}

	if (OutOfAmmo())
	{
		m_pWeapon->OnOutOfAmmo(ammo);
		if (autoreload)
		{
			m_pWeapon->GetScheduler()->TimerAction(m_pWeapon->GetCurrentAnimationTime(CItem::eIGS_FirstPerson), CSchedulerAction<ScheduleReload>::Create(m_pWeapon), false);
		}
	}

	m_pWeapon->RequestShoot(ammo, pos, dir, vel, hit, 1.0f, 0, seqn, seqr, false);

	return true;
}

//------------------------------------------------------------------------
void CShotgun::NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int ph)
{
	assert(0 == ph);

	IEntityClass* ammo = m_fireparams.ammo_type_class;
	const char *action = m_actions.fire_cock.c_str();

	CActor *pActor = m_pWeapon->GetOwnerActor();
	bool playerIsShooter = pActor?pActor->IsPlayer():false;

	int ammoCount = m_pWeapon->GetAmmoCount(ammo);
	if (m_fireparams.clip_size==0)
		ammoCount = m_pWeapon->GetInventoryAmmoCount(ammo);

	if (ammoCount == 1)
		action = m_actions.fire.c_str();

	m_pWeapon->ResetAnimation();
	m_pWeapon->PlayAction(action, 0, false, CItem::eIPAF_Default|CItem::eIPAF_NoBlend);

	Vec3 pdir;

	// SHOT HERE
	for (int i = 0; i < m_shotgunparams.pellets; i++)
	{
		CProjectile *pAmmo = m_pWeapon->SpawnAmmo(ammo, true);
		if (pAmmo)
		{
			pdir = ApplySpread(dir, m_shotgunparams.spread);
      int hitTypeId = g_pGame->GetGameRules()->GetHitTypeId(m_fireparams.hit_type.c_str());			

			pAmmo->SetParams(m_pWeapon->GetOwnerId(), m_pWeapon->GetHostId(), m_pWeapon->GetEntityId(), m_pWeapon->GetFireModeIdx(GetName()),
				m_shotgunparams.pelletdamage, hitTypeId);
			pAmmo->SetDestination(m_pWeapon->GetDestination());
			pAmmo->SetRemote(true);
			pAmmo->Launch(pos, pdir, vel);

			bool emit = false;
			if(m_pWeapon->GetStats().fp)
				emit = (!m_tracerparams.geometryFP.empty() || !m_tracerparams.effectFP.empty()) && (ammoCount==GetClipSize() || (ammoCount%m_tracerparams.frequency==0));
			else
				emit = (!m_tracerparams.geometry.empty() || !m_tracerparams.effect.empty()) && (ammoCount==GetClipSize() || (ammoCount%m_tracerparams.frequency==0));

			if (emit)
				EmitTracer(pos,hit,false);

			m_projectileId = pAmmo->GetEntity()->GetId();
		}
	}

	m_pWeapon->OnShoot(m_pWeapon->GetOwnerId(), 0, ammo, pos, dir, vel);

	if (m_pWeapon->IsServer())
		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponShot, ammo->GetName(), m_shotgunparams.pellets, (void *)m_pWeapon->GetEntityId()));

	MuzzleFlashEffect(true);
	RejectEffect();

	m_fired = true;
	m_next_shot = 0.0f;

	ammoCount--;
	if (m_pWeapon->IsServer())
	{
		if (m_fireparams.clip_size != -1)
		{
			if (m_fireparams.clip_size!=0)
				m_pWeapon->SetAmmoCount(ammo, ammoCount);
			else
				m_pWeapon->SetInventoryAmmoCount(ammo, ammoCount);
		}
	}

	if ((ammoCount<1) && !m_fireparams.slider_layer.empty())
	{
		const char *slider_back_layer = m_fireparams.slider_layer.c_str();
		m_pWeapon->PlayLayer(slider_back_layer, CItem::eIPAF_Default|CItem::eIPAF_NoBlend);
	}

	m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//------------------------------------------------------------------------
void CShotgun::ResetParams(const struct IItemParamsNode *params)
{
	CSingle::ResetParams(params);

	const IItemParamsNode *shotgun = params?params->GetChild("shotgun"):0;
	m_shotgunparams.Reset(shotgun);
}

//------------------------------------------------------------------------
void CShotgun::PatchParams(const struct IItemParamsNode *patch)
{
	CSingle::PatchParams(patch);

	const IItemParamsNode *shotgun = patch->GetChild("shotgun");
	m_shotgunparams.Reset(shotgun, false);
}

//---------------------------------------------------------------------
void CShotgun::CancelReload()
{
	if(!m_reload_was_broken)
	{
		m_reload_was_broken = true;
		m_pWeapon->GetScheduler()->Reset();
		EndReload(0);
	}
}
//------------------------------------------------------------------------
const char *CShotgun::GetType() const
{
	return "Shotgun";
}
