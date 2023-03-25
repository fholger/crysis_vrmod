/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 19:12:2005   12:10 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Beam.h"
#include "Weapon.h"
#include "Actor.h"
#include "Game.h"
#include "GameRules.h"
#include <ISound.h>
#include <IEntitySystem.h>


//------------------------------------------------------------------------
CBeam::CBeam()
:	m_effectId(0),
	m_fireLoopId(INVALID_SOUNDID),
	m_hitSoundId(INVALID_SOUNDID),
	m_remote(false),
	m_viewFP(true)
{
}

//------------------------------------------------------------------------
CBeam::~CBeam()
{
}

//std::vector<Vec3> gpoints;

//------------------------------------------------------------------------
void CBeam::Update(float frameTime, uint frameId)
{

	//for (std::vector<Vec3>::iterator it=gpoints.begin(); gpoints.end()!=it;++it)
		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(*it, 0.35f, ColorB(255, 128, 0, 255));

	bool keepUpdating=false;

	if(m_firing)
		m_fired = true;		//CSingle::Update overrides m_fired

	CSingle::Update(frameTime, frameId);

	if (m_firing)
	{
		keepUpdating=true;

		int slot = m_pWeapon->GetStats().fp ? CItem::eIGS_FirstPerson : CItem::eIGS_ThirdPerson;
		int id = m_pWeapon->GetStats().fp ? 0 : 1;

		if (m_spinUpTimer>0.0f)
		{
			m_spinUpTimer -= frameTime;

			if (m_spinUpTimer>0.0f)
			{
				m_pWeapon->RequireUpdate(eIUS_FireMode);
				return;
			}

			m_spinUpTimer = 0.0f;

			m_pWeapon->PlayAction(m_beamactions.blast.c_str());
			m_fireLoopId = m_pWeapon->PlayAction(m_actions.fire.c_str(), 0, true);

			ISound *pSound = m_pWeapon->GetSoundProxy()->GetSound(m_fireLoopId);
			if (pSound)
				pSound->SetLoopMode(true);

			SpinUpEffect(false);

			m_effectId = m_pWeapon->AttachEffect(slot, 0, true, m_effectparams.effect[id].c_str(), 
				m_effectparams.helper[id].c_str());
		}

		IEntityClass* ammo = NULL;
		if(m_spinUpTimer==0.0f)
		{
			if (m_fireparams.ammo_type_class && m_fireparams.clip_size>=0)
			{
				if (m_ammoTimer>0.0f)
				{
					m_ammoTimer -= frameTime;
					if (m_ammoTimer <= 0.0f)
					{
						m_ammoTimer = m_beamparams.ammo_tick;

						//Decrease ammo count
						ammo = m_fireparams.ammo_type_class;
						int ammoCount = m_pWeapon->GetAmmoCount(ammo);

						if (m_fireparams.clip_size==0)
							ammoCount = m_pWeapon->GetInventoryAmmoCount(ammo);

						ammoCount=ammoCount-m_beamparams.ammo_drain;
						if(ammoCount>=0)
						{
							if (m_fireparams.clip_size != -1)
							{
								if (m_fireparams.clip_size!=0)
									m_pWeapon->SetAmmoCount(ammo, ammoCount);
								else
									m_pWeapon->SetInventoryAmmoCount(ammo, ammoCount);
							}
						}
						else
						{
							StopFire();
						
							if (m_pWeapon->IsServer())
								m_pWeapon->GetGameObject()->InvokeRMI(CWeapon::ClStopFire(), CWeapon::EmptyParams(), eRMI_ToRemoteClients);

							if(m_pWeapon->GetInventoryAmmoCount(ammo)!=0)
							{
								Reload(m_pWeapon->IsZoomed()||m_pWeapon->IsZooming());
								if (m_pWeapon->IsServer())
									m_pWeapon->GetGameObject()->InvokeRMI(CWeapon::ClReload(), CWeapon::EmptyParams(), eRMI_ToRemoteClients);
							}
						}
					}
				}
			}
		}

		bool hitValid = false;
		ray_hit rayhit;

		Vec3 hit = GetProbableHit(m_beamparams.range, &hitValid, &rayhit);
		Vec3 pos = GetFiringPos(hit);
		Vec3 dir = GetFiringDir(hit, pos);

		m_pWeapon->OnShoot(m_pWeapon->GetOwnerId(), 0, ammo, pos, dir, Vec3(0,0,0));

		if (m_effectId)
		{
			int id = m_pWeapon->GetStats().fp?0:1;

			bool currentView = m_pWeapon->IsOwnerFP();
			//Check view changes and re-attach effect if needed (for vehicles)
			if(m_viewFP!=currentView)
			{
				m_viewFP = currentView;
				m_pWeapon->AttachEffect(m_viewFP?CItem::eIGS_ThirdPerson:CItem::eIGS_FirstPerson, m_effectId, false);
				m_effectId = m_pWeapon->AttachEffect(m_viewFP?CItem::eIGS_FirstPerson:CItem::eIGS_ThirdPerson,
					0, true, m_effectparams.effect[id].c_str(),	m_effectparams.helper[id].c_str());
				if(m_viewFP)
					m_pWeapon->PlayAction(g_pItemStrings->fire,0,false,CItem::eIPAF_Default|CItem::eIPAF_RepeatLastFrame);
			}

			Vec3 epos(m_pWeapon->GetEffectWorldTM(m_effectId).GetTranslation());
			
			if(m_effectparams.scale[id]<1.0f)
			{
				Quat rot(Quat::CreateRotationVDir(dir));
				rot.Normalize();

				float scaleFX = (((hit-pos).len())*m_effectparams.scale[id]);	
				if(scaleFX>1.0f)
					scaleFX = 1.0f;

				Matrix34 etm;
				etm.Set(Vec3(scaleFX,scaleFX,scaleFX),rot,epos);

				if(m_pWeapon->GetStats().fp)
					etm.OrthonormalizeFast();

				m_pWeapon->SetEffectWorldTM(m_effectId, etm);
			}
			else
			{
				Matrix34 etm(Matrix33::CreateRotationVDir(dir));
				etm.AddTranslation(epos);
				m_pWeapon->SetEffectWorldTM(m_effectId, etm);
			}

   
		}

    if (m_fireLoopId != INVALID_SOUNDID)
    {
      ISound *pSound = m_pWeapon->GetSoundProxy()->GetSound(m_fireLoopId);
      if (pSound)
        pSound->SetLineSpec(pos, hit);
    }    

		if (hitValid)
		{
			if (!m_lastHitValid)
			{
				m_hitSoundId = m_pWeapon->PlayAction(m_beamactions.hit.c_str(), 0, true, CItem::eIPAF_Default|CItem::eIPAF_ForceThirdPerson|CItem::eIPAF_SoundLooped|CItem::eIPAF_SoundStartPaused);
				ISound *pSound = m_pWeapon->GetISound(m_hitSoundId);
				if (pSound)
				{
					pSound->SetPosition(hit);
					pSound->SetPaused(false);
				}

				if (!m_hitbeameffectparams.effect[id].empty())
				{
					m_pWeapon->AttachEffect(0, m_effectId, false);
					m_effectId = m_pWeapon->AttachEffect(slot, 0, true, m_hitbeameffectparams.effect[id].c_str(), m_hitbeameffectparams.helper[id].c_str());
				}
			}

			// update sound pos/param
			if (m_hitSoundId != INVALID_SOUNDID)
			{
				ISound *pSound = m_pWeapon->GetISound(m_hitSoundId);
				if (pSound)
				{
          float angle = RAD2DEG(acos_tpl(rayhit.n.Dot(dir)));
					pSound->SetParam("angle", angle, false);				          
					pSound->SetPosition(hit);					

          //float color[] = {1,1,1,1};
          //gEnv->pRenderer->Draw2dLabel(200,300,1.5f,color,false,"angle: %.2f", angle);
				}
			}

			if (!m_beamparams.hit_decal.empty())
			{
				//if (!m_lastHitValid)
				//if (!rayhit.pCollider || !gEnv->pEntitySystem->GetEntityFromPhysics(rayhit.pCollider))
					Decal(rayhit, dir);
				//else
				//{
					//Decal(rayhit, dir);
					//DecalLine(m_lastOrg, pos, m_lastHit, rayhit.pt, m_beamparams.hit_decal_size*0.1f);
				//}
			}

			if (!m_beamparams.hit_effect.empty())
			{
				IParticleEffect *pParticleEffect = gEnv->p3DEngine->FindParticleEffect(m_beamparams.hit_effect.c_str());
				if (pParticleEffect)
					pParticleEffect->Spawn(true, IParticleEffect::ParticleLoc(rayhit.pt, rayhit.n, m_beamparams.hit_effect_scale));
			}

			if (m_tickTimer>0.0f)
			{
				m_tickTimer -= frameTime;
				if (m_tickTimer <= 0.0f)
				{
					Tick(rayhit, dir);

					m_tickTimer = m_beamparams.tick;

					uint16 seq=m_pWeapon->GenerateShootSeqN();

					m_pWeapon->RequestShoot(0, ZERO, ZERO, ZERO, ZERO, 0, 0, seq, 0, false);

					TickDamage(rayhit, dir, seq);
				}
			}

      Hit(rayhit, dir);

			m_lastOrg = pos;
			m_lastHit = rayhit.pt;
			m_lastHitValid = true;
		}
		else
		{
			if (!m_hitbeameffectparams.effect[id].empty() && m_lastHitValid)
			{
				m_pWeapon->AttachEffect(0, m_effectId, false);
				m_effectId = m_pWeapon->AttachEffect(slot, 0, true, m_effectparams.effect[id].c_str(), 
					m_effectparams.helper[id].c_str());
			}

			m_lastHitValid = false;
		}
	}
	else
		m_lastHitValid = false;

	if (!m_lastHitValid)
	{
		if (m_hitSoundId != INVALID_SOUNDID)
		{
			m_pWeapon->StopSound(m_hitSoundId);
			m_hitSoundId = INVALID_SOUNDID;      
		}

		// stop the effect here too
	}

	if (keepUpdating)
		m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//------------------------------------------------------------------------
void CBeam::ResetParams(const struct IItemParamsNode *params)
{
	CSingle::ResetParams(params);

	const IItemParamsNode *beam = params?params->GetChild("beam"):0;
	const IItemParamsNode *effect = params?params->GetChild("effect"):0;
	const IItemParamsNode *hiteffect = params?params->GetChild("hiteffect"):0;
	const IItemParamsNode *actions = params?params->GetChild("actions"):0;

	m_beamparams.Reset(beam);
	m_effectparams.Reset(effect);
	m_hitbeameffectparams.Reset(hiteffect);
	m_beamactions.Reset(actions);
}

//------------------------------------------------------------------------
void CBeam::PatchParams(const struct IItemParamsNode *patch)
{
	CSingle::PatchParams(patch);
	
	const IItemParamsNode *beam = patch?patch->GetChild("beam"):0;
	const IItemParamsNode *effect = patch?patch->GetChild("effect"):0;
	const IItemParamsNode *hiteffect = patch?patch->GetChild("hiteffect"):0;
	const IItemParamsNode *actions = patch?patch->GetChild("actions"):0;
	m_beamparams.Reset(beam, false);
	m_effectparams.Reset(effect, false);
	m_hitbeameffectparams.Reset(hiteffect, false);
	m_beamactions.Reset(actions, false);
}

//------------------------------------------------------------------------
void CBeam::Activate(bool activate)
{
	//gpoints.resize(0);
	CSingle::Activate(activate);

	m_firing = false;
	m_remote = false;

	if (m_fireLoopId != INVALID_SOUNDID)
	{
		m_pWeapon->StopSound(m_fireLoopId);
		m_fireLoopId = INVALID_SOUNDID;
	}

	if (m_hitSoundId != INVALID_SOUNDID)
	{
		m_pWeapon->StopSound(m_hitSoundId);
		m_hitSoundId = INVALID_SOUNDID;
	}

	SpinUpEffect(false);

	if (m_effectId)
	{
		m_pWeapon->AttachEffect(0, m_effectId, false);
		m_effectId=0;
	}

	m_lastHitValid=false;
	m_tickTimer = m_beamparams.tick;
	m_ammoTimer = m_beamparams.ammo_tick;
}

//------------------------------------------------------------------------
//bool CBeam::OutOfAmmo() const
//{
	//return false;
//}

//------------------------------------------------------------------------
//bool CBeam::CanReload() const
//{
	//return false;
//}

//------------------------------------------------------------------------
bool CBeam::CanFire(bool considerAmmo) const
{
	return !m_reloading && (m_next_shot<=0.0f) && (m_spinUpTime<=0.0f) && (m_overheat<=0.0f) &&
		!m_pWeapon->IsBusy() && (!considerAmmo || !OutOfAmmo() || !m_fireparams.ammo_type_class || m_fireparams.clip_size == -1);
}

//------------------------------------------------------------------------
void CBeam::StartFire()
{
	if (!CanFire(true))
		return;

	m_lastHitValid = false;
	m_firing = true;
	m_spinUpTimer = m_fireparams.spin_up_time;
	m_tickTimer = m_beamparams.tick;
	m_ammoTimer = m_beamparams.ammo_tick;

	m_fired = true;			//For recoil

	SpinUpEffect(true);
	m_pWeapon->PlayAction(m_actions.spin_up.c_str(), 0, false, CItem::eIPAF_Default|CItem::eIPAF_CleanBlending);
	m_pWeapon->RequireUpdate(eIUS_FireMode);

	m_pWeapon->RequestStartFire();

	m_viewFP = m_pWeapon->IsOwnerFP();
}

//------------------------------------------------------------------------
void CBeam::StopFire()
{
	//Prevent being stopped if it is not firing
	if(!m_firing)
		return;

	m_firing = false;
	m_fired  = false;

	SpinUpEffect(false);

	if (m_effectId)
	{
		m_pWeapon->AttachEffect(0, m_effectId, false);
		m_effectId=0;
	}

	if (m_fireLoopId != INVALID_SOUNDID)
	{
		m_pWeapon->StopSound(m_fireLoopId);
		m_fireLoopId = INVALID_SOUNDID;
	}

	m_pWeapon->PlayAction(m_actions.spin_down.c_str(), 0, false, CItem::eIPAF_Default|CItem::eIPAF_CleanBlending);
	m_pWeapon->EnableUpdate(false, eIUS_FireMode);

	m_pWeapon->RequestStopFire();
}

//------------------------------------------------------------------------
void CBeam::NetStartFire()
{
	m_lastHitValid = false;
	m_firing = true;
	m_spinUpTimer = m_fireparams.spin_up_time;
	m_tickTimer = m_beamparams.tick;
	m_ammoTimer = m_beamparams.ammo_tick;
	m_remote = true;

	m_fired = true;

	SpinUpEffect(true);
	m_pWeapon->PlayAction(m_actions.spin_up.c_str(), 0, false, CItem::eIPAF_Default|CItem::eIPAF_CleanBlending);
	m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//------------------------------------------------------------------------
void CBeam::NetStopFire()
{
	//Prevent being stopped if it is not firing
	if(!m_firing)
		return;

	m_firing = false;
	m_fired  = false;
	m_remote = false;

	SpinUpEffect(false);

	if (m_effectId)
	{
		m_pWeapon->AttachEffect(CItem::eIGS_FirstPerson, m_effectId, false);
		m_effectId=0;
	}

	if (m_fireLoopId != INVALID_SOUNDID)
	{
		m_pWeapon->StopSound(m_fireLoopId);
		m_fireLoopId = INVALID_SOUNDID;
	}

	m_pWeapon->PlayAction(m_actions.spin_down.c_str(), 0, false, CItem::eIPAF_Default|CItem::eIPAF_CleanBlending);
	m_pWeapon->EnableUpdate(false, eIUS_FireMode);
}

//------------------------------------------------------------------------
void CBeam::DecalLine(const Vec3 &org0, const Vec3 &org1, const Vec3 &hit0, const Vec3 &hit1, float step)
{
	Vec3 hitStepDir = (hit1-hit0);
	float dist = hitStepDir.len();
	if (dist < 2.0f*step)	// don't create line if too close
		return;

	hitStepDir /= dist;
	int steps = (int)((dist-(2.0f*step))/step);
	if (!steps)
		return;

	Vec3 orgStepDir = (org1-org0);
	float orgDist = orgStepDir.len();
	float orgStep = 0.0f;

	if (orgDist>0.05f)
	{
		orgStep = orgDist/(float)steps;
		orgStepDir /= orgDist;
	}
	else
		orgStepDir=Vec3(0,0,0);
	
	float hitCurrStep = step;
	float orgCurrStep = orgStep;

	Vec3 org=org0;
	Vec3 hit=hit0;

//	CryLogAlways("line decals: %d (dist: %.3f)", steps, dist);

	while(steps--)
	{
		hit=hit0+hitStepDir*hitCurrStep;
		org=org0+orgStepDir*orgCurrStep;

		Vec3 dir = (hit-org).normalized()*m_beamparams.range;

		ray_hit rayhit;
		IPhysicalEntity *pSkipEnts[10];
    int nSkip = GetSkipEntities(m_pWeapon, pSkipEnts, 10);

		if (gEnv->pPhysicalWorld->RayWorldIntersection(org, dir, ent_all,
			rwi_stop_at_pierceable|rwi_colltype_any, &rayhit, 1, pSkipEnts, nSkip))
		{
			Decal(rayhit, dir);
			//CryLogAlways("decal: %.3f, %.3f, %.3f", rayhit.pt.x, rayhit.pt.y, rayhit.pt.z);
		}

		hitCurrStep += step;
		orgCurrStep += orgStep;
	}
}


//------------------------------------------------------------------------
void CBeam::Decal(const ray_hit &rayhit, const Vec3 &dir)
{
	CryEngineDecalInfo decal;

	//	gpoints.push_back(rayhit.pt);
	decal.vPos = rayhit.pt;
	decal.vNormal = rayhit.n;
	decal.fSize = m_beamparams.hit_decal_size;
  if (m_beamparams.hit_decal_size_min != 0.f)
    decal.fSize -= Random()*max(0.f, m_beamparams.hit_decal_size-m_beamparams.hit_decal_size_min);
	decal.fLifeTime = m_beamparams.hit_decal_lifetime;
	decal.bAdjustPos = true;

	strcpy(decal.szMaterialName, m_beamparams.hit_decal.c_str());

	decal.fAngle = RAD2DEG(acos_tpl(rayhit.n.Dot(dir)));
	decal.vHitDirection = dir;

	if (rayhit.pCollider)
	{
		if (IRenderNode* pRenderNode = (IRenderNode*)rayhit.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC))
			decal.ownerInfo.pRenderNode = pRenderNode;
		else if (IEntity *pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(rayhit.pCollider))
		{
			IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);;
			if (pRenderProxy)
				decal.ownerInfo.pRenderNode = pRenderProxy->GetRenderNode();
		}
	}

	gEnv->p3DEngine->CreateDecal(decal);
}

//------------------------------------------------------------------------
void CBeam::Hit(ray_hit &hit, const Vec3 &dir)
{
}

//------------------------------------------------------------------------
void CBeam::Tick(ray_hit &hit, const Vec3 &dir)
{
}

//------------------------------------------------------------------------
void CBeam::TickDamage(ray_hit &hit, const Vec3 &dir, uint16 seq)
{	
	if (m_fireparams.damage==0)
		return;

	IEntity *pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider);

	if (pEntity)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();

		HitInfo info(m_pWeapon->GetOwnerId(), pEntity->GetId(), m_pWeapon->GetEntityId(), 
			m_pWeapon->GetFireModeIdx(GetName()), 0.25f, pGameRules->GetHitMaterialIdFromSurfaceId(hit.surface_idx), hit.partid, 
			pGameRules->GetHitTypeId(m_fireparams.hit_type.c_str()), hit.pt, dir, hit.n);

		if (m_pWeapon->GetForcedHitMaterial() != -1)
			info.material=pGameRules->GetHitMaterialIdFromSurfaceId(m_pWeapon->GetForcedHitMaterial());

		info.remote = m_remote;
		if (!m_remote)
			info.seq=seq;
		info.damage = m_fireparams.damage;

		pGameRules->ClientHit(info);
	}
}

void CBeam::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	CSingle::GetMemoryStatistics(s);
	m_beamparams.GetMemoryStatistics(s);
	m_beamactions.GetMemoryStatistics(s);
	m_effectparams.GetMemoryStatistics(s);
}
