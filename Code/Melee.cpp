/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 23:3:2006   13:05 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Melee.h"
#include "Game.h"
#include "Item.h"
#include "Weapon.h"
#include "GameRules.h"
#include "Player.h"
#include "BulletTime.h"
#include <IEntitySystem.h>
#include "IMaterialEffects.h"
#include "GameCVars.h"


#include "IRenderer.h"
#include "IRenderAuxGeom.h"	


//std::vector<Vec3> g_points;

//------------------------------------------------------------------------
CMelee::CMelee()
{
	m_noImpulse = false;
}

//------------------------------------------------------------------------
CMelee::~CMelee()
{
}

const float STRENGTH_MULT = 0.018f;

void CMelee::Init(IWeapon *pWeapon, const struct IItemParamsNode *params)
{
	m_pWeapon = static_cast<CWeapon *>(pWeapon);

	if (params)
		ResetParams(params);

	m_attacking = false;
	m_attacked = false;
	m_delayTimer=0.0f;
	m_durationTimer=0.0f;
	m_ignoredEntity = 0;
	m_meleeScale = 1.0f;
}

//------------------------------------------------------------------------
void CMelee::Update(float frameTime, uint frameId)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );

	bool requireUpdate = false;
	
	if (m_attacking)
	{
		requireUpdate = true;
		if (m_delayTimer>0.0f)
		{
			m_delayTimer-=frameTime;
			if (m_delayTimer<=0.0f)
				m_delayTimer=0.0f;
		}
		else
		{
			if (!m_attacked)
			{
				m_attacked = true;

				CActor *pActor = m_pWeapon->GetOwnerActor();
				if(!pActor)
					return;

				Vec3 pos(ZERO);
				Vec3 dir(ZERO);
				IMovementController * pMC = pActor->GetMovementController();
				if (!pMC)
					return;

				float strength = 1.0f;//pActor->GetActorStrength();
				if (pActor->GetActorClass() == CPlayer::GetActorClassType())
				{
					CPlayer *pPlayer = (CPlayer *)pActor;
					if (CNanoSuit *pSuit = pPlayer->GetNanoSuit())
					{
						ENanoMode curMode = pSuit->GetMode();
						if (curMode == NANOMODE_STRENGTH)
						{
							strength = pActor->GetActorStrength();
							strength = strength * (1.0f + 2.0f * pSuit->GetSlotValue(NANOSLOT_STRENGTH)* STRENGTH_MULT);
							if(!pPlayer->IsPlayer() && g_pGameCVars->g_difficultyLevel < 4)
								strength *= g_pGameCVars->g_AiSuitStrengthMeleeMult;
						}
					}
				}

				SMovementState info;
				pMC->GetMovementState(info);
				pos = info.eyePosition;
				dir = info.eyeDirection;
				if (!PerformRayTest(pos, dir, strength, false))
					if(!PerformCylinderTest(pos, dir, strength, false))
						ApplyCameraShake(false);

				m_ignoredEntity = 0;
				m_meleeScale = 1.0f;
				
				m_pWeapon->RequestMeleeAttack(m_pWeapon->GetMeleeFireMode()==this, pos, dir, m_pWeapon->GetShootSeqN());
			}
		}
	}

	if (requireUpdate)
		m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//------------------------------------------------------------------------
void CMelee::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CMelee::ResetParams(const struct IItemParamsNode *params)
{
	const IItemParamsNode *melee = params?params->GetChild("melee"):0;
	const IItemParamsNode *actions = params?params->GetChild("actions"):0;

	m_meleeparams.Reset(melee);
	m_meleeactions.Reset(actions);
}

//------------------------------------------------------------------------
void CMelee::PatchParams(const struct IItemParamsNode *patch)
{
	const IItemParamsNode *melee = patch->GetChild("melee");
	const IItemParamsNode *actions = patch->GetChild("actions");

	m_meleeparams.Reset(melee, false);
	m_meleeactions.Reset(actions, false);
}

//------------------------------------------------------------------------
void CMelee::Activate(bool activate)
{
	m_attacking = m_noImpulse = false;
	m_delayTimer=0.0f;
	m_durationTimer=0.0f;
}

//------------------------------------------------------------------------
bool CMelee::CanFire(bool considerAmmo) const
{
	return !m_attacking;
}

//------------------------------------------------------------------------
struct CMelee::StopAttackingAction
{
	CMelee *_this;
	StopAttackingAction(CMelee *melee): _this(melee) {};
	void execute(CItem *pItem)
	{
		_this->m_attacking = false;

		_this->m_delayTimer = 0.0f;
		_this->m_durationTimer = 0.0f;
		pItem->SetBusy(false);
		// this allows us to blend into the idle animation (for swimming) -- johnn
		//pItem->ResetAnimation();
		//pItem->ReAttachAccessories();
		//pItem->PlayAction(ItemStrings::idle, 0, false, CItem::eIPAF_CleanBlending | CItem::eIPAF_NoBlend | CItem::eIPAF_Default);
		//pItem->PlayAction(pItem->GetDefaultIdleAnimation(0), 0, false, CItem::eIPAF_CleanBlending | CItem::eIPAF_Default);

	}
};

void CMelee::StartFire()
{
	if (!CanFire())
		return;

	//Prevent fists melee exploit 
	if ((m_pWeapon->GetEntity()->GetClass() == CItem::sFistsClass) && m_pWeapon->IsBusy())
		return;

	CActor* pOwner = m_pWeapon->GetOwnerActor();

	if(pOwner)
	{
		if(pOwner->GetStance()==STANCE_PRONE)
			return;

		if(SPlayerStats* stats = static_cast<SPlayerStats*>(pOwner->GetActorStats()))
		{
			if(stats->bLookingAtFriendlyAI)
				return;
		}
	}

	m_attacking = true;
	m_attacked = false;
	m_pWeapon->RequireUpdate(eIUS_FireMode);
	m_pWeapon->ExitZoom();

	bool isClient = pOwner?pOwner->IsClient():false;

	if (g_pGameCVars->bt_end_melee && isClient)
		g_pGame->GetBulletTime()->Activate(false);


	float speedOverride = -1.0f;

	if(CActor* pOwner = m_pWeapon->GetOwnerActor())
	{
		CPlayer *pPlayer = (CPlayer *)pOwner;
		if(CNanoSuit *pSuit = pPlayer->GetNanoSuit())
		{
			ENanoMode curMode = pSuit->GetMode();
			if (curMode == NANOMODE_SPEED && pSuit->GetSuitEnergy() > NANOSUIT_ENERGY * 0.1f)
				speedOverride = 1.5f;
		}
		
		pPlayer->PlaySound(CPlayer::ESound_Melee);
	}

	m_pWeapon->PlayAction(m_meleeactions.attack.c_str(), 0, false, CItem::eIPAF_Default|CItem::eIPAF_CleanBlending, speedOverride);
	m_pWeapon->SetBusy(true);
	
	m_beginPos = m_pWeapon->GetSlotHelperPos(CItem::eIGS_FirstPerson, m_meleeparams.helper.c_str(), true);
	m_pWeapon->GetScheduler()->TimerAction(m_pWeapon->GetCurrentAnimationTime(CItem::eIGS_FirstPerson), CSchedulerAction<StopAttackingAction>::Create(this), true);

	m_delayTimer = m_meleeparams.delay;
	
	if (g_pGameCVars->dt_enable && m_delayTimer < g_pGameCVars->dt_time)
		m_delayTimer = g_pGameCVars->dt_time;

	m_durationTimer = m_meleeparams.duration;

	m_pWeapon->OnMelee(m_pWeapon->GetOwnerId());

	m_pWeapon->RequestStartMeleeAttack(m_pWeapon->GetMeleeFireMode()==this);
}

//------------------------------------------------------------------------
void CMelee::StopFire()
{
}

//------------------------------------------------------------------------
void CMelee::NetStartFire()
{
	m_pWeapon->OnMelee(m_pWeapon->GetOwnerId());

	float speedOverride = -1.0f;

	if(CActor* pOwner = m_pWeapon->GetOwnerActor())
	{
		CPlayer *pPlayer = (CPlayer *)pOwner;
		if(CNanoSuit *pSuit = pPlayer->GetNanoSuit())
		{
			ENanoMode curMode = pSuit->GetMode();
			if (curMode == NANOMODE_SPEED)
				speedOverride = 1.5f;
		}
	}

	m_pWeapon->PlayAction(m_meleeactions.attack.c_str(), 0, false, CItem::eIPAF_Default, speedOverride);
}

//------------------------------------------------------------------------
void CMelee::NetStopFire()
{
}

//------------------------------------------------------------------------
void CMelee::NetShoot(const Vec3 &hit, int ph)
{
}

//------------------------------------------------------------------------
void CMelee::NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int ph)
{
	float strength=GetOwnerStrength();

	if (!PerformRayTest(pos, dir, strength, true))
		if(!PerformCylinderTest(pos, dir, strength, true))
			ApplyCameraShake(false);

	m_ignoredEntity = 0;
	m_meleeScale = 1.0f;
}

//------------------------------------------------------------------------
const char *CMelee::GetType() const
{
	return "Melee";
}

//------------------------------------------------------------------------
int CMelee::GetDamage(float distance) const
{
	// NOTE: in multiplayer m_meleeScale == 1.0,
	// however this is not a problem since we cannot pick up objects in that scenario
	return m_meleeparams.damage*GetOwnerStrength()*m_meleeScale;
}

//------------------------------------------------------------------------
const char *CMelee::GetDamageType() const
{
	return m_meleeparams.hit_type.c_str();
}


//------------------------------------------------------------------------
bool CMelee::PerformRayTest(const Vec3 &pos, const Vec3 &dir, float strength, bool remote)
{
	IEntity *pOwner = m_pWeapon->GetOwner();
	IPhysicalEntity *pIgnore = pOwner?pOwner->GetPhysics():0;

	ray_hit hit;
	int n =gEnv->pPhysicalWorld->RayWorldIntersection(pos, dir.normalized()*m_meleeparams.range, ent_all|ent_water,
			rwi_stop_at_pierceable|rwi_ignore_back_faces,&hit, 1, &pIgnore, pIgnore?1:0);

	//===================OffHand melee (also in PerformCylincerTest)===================
	if(m_ignoredEntity && (n>0))
	{
		if(IEntity* pHeldObject = gEnv->pEntitySystem->GetEntity(m_ignoredEntity))
		{
			IPhysicalEntity *pHeldObjectPhysics = pHeldObject->GetPhysics();
			if(pHeldObjectPhysics==hit.pCollider)
				return false;
		}
	}
	//=================================================================================

	if (n>0)
	{
		Hit(&hit, dir, strength, remote);
		Impulse(hit.pt, dir, hit.n, hit.pCollider, hit.partid, hit.ipart, hit.surface_idx, strength);
	}

	return n>0;
}

//------------------------------------------------------------------------
bool CMelee::PerformCylinderTest(const Vec3 &pos, const Vec3 &dir, float strength, bool remote)
{
	IEntity *pOwner = m_pWeapon->GetOwner();
	IPhysicalEntity *pIgnore = pOwner?pOwner->GetPhysics():0;
	IEntity *pHeldObject = NULL;

	if(m_ignoredEntity)
		pHeldObject = gEnv->pEntitySystem->GetEntity(m_ignoredEntity);
		
	primitives::cylinder cyl;
	cyl.r = 0.25f;
	cyl.axis = dir;
	cyl.hh = m_meleeparams.range/2.0f;
	cyl.center = pos + dir.normalized()*cyl.hh;
	
	float n = 0.0f;
	geom_contact *contacts;
	intersection_params params;
	params.bStopAtFirstTri = false;
	params.bNoBorder = true;
	params.bNoAreaContacts = true;
	n = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(primitives::cylinder::type, &cyl, Vec3(ZERO), 
		ent_rigid|ent_sleeping_rigid|ent_independent|ent_static|ent_terrain|ent_water, &contacts, 0,
		geom_colltype0|geom_colltype_foliage|geom_colltype_player, &params, 0, 0, &pIgnore, pIgnore?1:0);

	int ret = (int)n;

	float closestdSq = 9999.0f;
	geom_contact *closestc = 0;
	geom_contact *currentc = contacts;

	for (int i=0; i<ret; i++)
	{
		geom_contact *contact = currentc;
		if (contact)
		{
			IPhysicalEntity *pCollider = gEnv->pPhysicalWorld->GetPhysicalEntityById(contact->iPrim[0]);
			if (pCollider)
			{
				IEntity *pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pCollider);
				if (pEntity)
				{
					if ((pEntity == pOwner)||(pHeldObject && (pEntity == pHeldObject)))
					{
						++currentc;
						continue;
					}
				}

				float distSq = (pos-currentc->pt).len2();
				if (distSq < closestdSq)
				{
					closestdSq = distSq;
					closestc = contact;
				}
			}
		}
		++currentc;
	}

	if (ret)
	{
		WriteLockCond lockColl(*params.plock, 0);
		lockColl.SetActive(1);
	}

  
	if (closestc)
	{
		IPhysicalEntity *pCollider = gEnv->pPhysicalWorld->GetPhysicalEntityById(closestc->iPrim[0]);

		Hit(closestc, dir, strength, remote);
		Impulse(closestc->pt, dir, closestc->n, pCollider, closestc->iPrim[1], 0, closestc->id[1], strength);
	}

	return closestc!=0;
}

//------------------------------------------------------------------------
void CMelee::Hit(const Vec3 &pt, const Vec3 &dir, const Vec3 &normal, IPhysicalEntity *pCollider, int partId, int ipart, int surfaceIdx, float damageScale, bool remote)
{
	// generate the damage
	IEntity *pTarget = gEnv->pEntitySystem->GetEntityFromPhysics(pCollider);

	// Report punch to AI system.
	// The AI notification must come before the game rules are 
	// called so that the death handler in AIsystem understands that the hit
	// came from the player.
	CActor *pActor = m_pWeapon->GetOwnerActor();
	if (pActor && pActor->GetActorClass() == CPlayer::GetActorClassType())
	{
		CPlayer *pPlayer = (CPlayer *)pActor;
		if (pPlayer && pPlayer->GetNanoSuit())
		{
			if (pPlayer->GetEntity() && pPlayer->GetEntity()->GetAI())
			{
				SAIEVENT AIevent;
				AIevent.targetId = pTarget ? pTarget->GetId() : 0;
				// pPlayer->GetNanoSuit()->GetMode() == NANOMODE_STRENGTH
				pPlayer->GetEntity()->GetAI()->Event(AIEVENT_PLAYER_STUNT_PUNCH, &AIevent);
			}
		}
	}

	bool ok = true;
	if(pTarget)
	{
		if(!gEnv->bMultiplayer && pActor && pActor->IsPlayer())
		{
			IActor* pAITarget = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pTarget->GetId());
			if(pAITarget && pTarget->GetAI() && !pTarget->GetAI()->IsHostile(pActor->GetEntity()->GetAI(),false))
			{
				ok = false;
				m_noImpulse = true;
			}
		}

		if(ok)
		{
			CGameRules *pGameRules = g_pGame->GetGameRules();

			int damage = m_meleeparams.damage;
			if(pActor && !pActor->IsPlayer())
				damage = m_meleeparams.damageAI;

			HitInfo info(m_pWeapon->GetOwnerId(), pTarget->GetId(), m_pWeapon->GetEntityId(),
				m_pWeapon->GetFireModeIdx(GetName()), 0.0f, pGameRules->GetHitMaterialIdFromSurfaceId(surfaceIdx), partId,
				pGameRules->GetHitTypeId(m_meleeparams.hit_type.c_str()), pt, dir, normal);

			info.remote = remote;
			if (!remote)
				info.seq=m_pWeapon->GenerateShootSeqN();
			info.damage = m_meleeparams.damage;

			if (m_pWeapon->GetForcedHitMaterial() != -1)
				info.material=pGameRules->GetHitMaterialIdFromSurfaceId(m_pWeapon->GetForcedHitMaterial());

			pGameRules->ClientHit(info);
		}
	}

	// play effects
	if(ok)
	{
		IMaterialEffects* pMaterialEffects = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects();

		TMFXEffectId effectId = pMaterialEffects->GetEffectId("melee", surfaceIdx);
		if (effectId != InvalidEffectId)
		{
			SMFXRunTimeEffectParams params;
			params.pos = pt;
			params.playflags = MFX_PLAY_ALL | MFX_DISABLE_DELAY;
			params.soundSemantic = eSoundSemantic_Player_Foley;
			pMaterialEffects->ExecuteEffect(effectId, params);
		}
	}

	ApplyCameraShake(true);

	m_pWeapon->PlayAction(m_meleeactions.hit.c_str());
}

//------------------------------------------------------------------------
void CMelee::Impulse(const Vec3 &pt, const Vec3 &dir, const Vec3 &normal, IPhysicalEntity *pCollider, int partId, int ipart, int surfaceIdx, float impulseScale)
{
	if(m_noImpulse)
	{
		m_noImpulse = false;
		return;
	}

	if (pCollider && m_meleeparams.impulse>0.001f)
	{
		bool strengthMode = false;
		CPlayer *pPlayer = (CPlayer *)m_pWeapon->GetOwnerActor();
		if(pPlayer)
		{
			if (CNanoSuit *pSuit = pPlayer->GetNanoSuit())
			{
				ENanoMode curMode = pSuit->GetMode();
				if (curMode == NANOMODE_STRENGTH)
					strengthMode = true;
			}
		}

		pe_status_dynamics dyn;

		if (!pCollider->GetStatus(&dyn))
		{
			if(strengthMode)
				impulseScale *= 3.0f;
		}
		else
			impulseScale *= clamp((dyn.mass * 0.01f), 1.0f, 15.0f);
		
		//[kirill] add impulse to phys proxy - to make sure it's applied to cylinder as well (not only skeleton) - so that entity gets pushed
		// if no pEntity - do it old way
		IEntity * pEntity = (IEntity*) pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);

		if(gEnv->bMultiplayer && pEntity)
		{
			if(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()) == NULL)
				impulseScale *= 0.33f;
		}

		if(pEntity)
		{
			{
				IEntityPhysicalProxy* pPhysicsProxy = (IEntityPhysicalProxy*)pEntity->GetProxy(ENTITY_PROXY_PHYSICS);
				CActor* pActor = (CActor*)g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());

				if (pActor)
				{
					SActorStats* pAS = pActor->GetActorStats();
					if (pAS && pAS->isRagDoll)
					{
						//marcok: talk to me before touching this
						impulseScale = 1.0f; //jan: melee impulses were scaled down, I made sure it still "barely moves"
					}
				}

				// scale impulse up a bit for player 
				pPhysicsProxy->AddImpulse(partId, pt, dir*m_meleeparams.impulse*impulseScale*m_meleeScale, true, 1.f);
			}
		}
		else
		{
			pe_action_impulse ai;
			ai.partid = partId;
			ai.ipart = ipart;
			ai.point = pt;
			ai.iApplyTime = 0;
			ai.impulse = dir*(m_meleeparams.impulse*impulseScale*m_meleeScale);
			pCollider->Action(&ai);
		}

		ISurfaceTypeManager *pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
		int invId = pSurfaceTypeManager->GetSurfaceTypeByName("mat_invulnerable")->GetId();

		// create a physical collision to break trees
		pe_action_register_coll_event collision;

		collision.collMass = 0.005f; // this is actually ignored
		collision.partid[1] = partId;

		// collisions involving partId<-1 are to be ignored by game's damage calculations
		// usually created articially to make stuff break.
		collision.partid[0] = -2;
		collision.idmat[1] = surfaceIdx;
		collision.idmat[0] = invId;
		collision.n = normal;
		collision.pt = pt;
	
		// scar bullet
		// m = 0.0125
		// v = 800
		// energy: 4000
		// in this case the mass of the active collider is a player part
		// so we must solve for v given the same energy as a scar bullet
		Vec3	v = dir;
		float speed = cry_sqrtf(4000.0f/(80.0f*0.5f)); // 80.0f is the mass of the player

		// [marco] Check if an object. Should take lots of time to break stuff if not in nanosuit strength mode;
		// and still creates a very low impulse for stuff that might depend on receiving an impulse.
		IRenderNode *pBrush = (IRenderNode*)pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
		if (pBrush)
		{
			CActor *pActor = m_pWeapon->GetOwnerActor();
			if (pActor && (pActor->GetActorClass() == CPlayer::GetActorClassType()))
			{
				CPlayer *pPlayer = (CPlayer *)pActor;
				if (CNanoSuit *pSuit = pPlayer->GetNanoSuit())
				{
					ENanoMode curMode = pSuit->GetMode();
					if (curMode != NANOMODE_STRENGTH)
						speed =0.003f;
				}
			}
		}

		collision.vSelf = (v.normalized()*speed*m_meleeScale);
		collision.v = Vec3(0,0,0);
		collision.pCollider = pCollider;

		IEntity *pOwner = m_pWeapon->GetOwner();
		if (pOwner && pOwner->GetCharacter(0) && pOwner->GetCharacter(0)->GetISkeletonPose()->GetCharacterPhysics())
		{
			if (ISkeletonPose *pSkeletonPose=pOwner->GetCharacter(0)->GetISkeletonPose())
			{
				if (pSkeletonPose && pSkeletonPose->GetCharacterPhysics())
					pSkeletonPose->GetCharacterPhysics()->Action(&collision);
			}

		}
	}
}

//------------------------------------------------------------------------
void CMelee::Hit(geom_contact *contact, const Vec3 &dir, float damageScale, bool remote)
{
	CActor *pOwner = m_pWeapon->GetOwnerActor();
	if (!pOwner)
		return;

	Vec3 view(0.0f, 1.0f, 0.0f);

	if (IMovementController *pMC = pOwner->GetMovementController())
	{
		SMovementState state;
		pMC->GetMovementState(state);
		view = state.eyeDirection;
	}

	// some corrections to make sure the impulse is always away from the camera, and is not a backface collision
	bool backface = dir.Dot(contact->n)>0;
	bool away = dir.Dot(view.normalized())>0; // away from cam?

	Vec3 normal=contact->n;
	Vec3 ndir=dir;

	if (backface)
	{
		if (away)
			normal = -normal;
		else
			ndir = -dir;
	}
	else
	{
		if (!away)
		{
			ndir = -dir;
			normal = -normal;
		}
	}

	IPhysicalEntity *pCollider = gEnv->pPhysicalWorld->GetPhysicalEntityById(contact->iPrim[0]);

	Hit(contact->pt, ndir, normal, pCollider, contact->iPrim[1], 0, contact->id[1], damageScale, remote);
}

//------------------------------------------------------------------------
void CMelee::Hit(ray_hit *hit, const Vec3 &dir, float damageScale, bool remote)
{
	Hit(hit->pt, dir, hit->n, hit->pCollider, hit->partid, hit->ipart, hit->surface_idx, damageScale, remote);
}

//-----------------------------------------------------------------------
void CMelee::ApplyCameraShake(bool hit)
{
	// Add some camera shake for client even if not hitting
	if(m_pWeapon->GetOwnerActor() && m_pWeapon->GetOwnerActor()->IsClient())
	{
		if(CScreenEffects* pScreenEffects = m_pWeapon->GetOwnerActor()->GetScreenEffects())
		{
			float rotateTime;
			if(!hit)
			{
				rotateTime = g_pGameCVars->hr_rotateTime*1.25f;
				pScreenEffects->CamShake(Vec3(rotateTime*0.5f,rotateTime*0.5f,rotateTime*0.25f), Vec3(0, 0.3f * g_pGameCVars->hr_rotateFactor,0), rotateTime, rotateTime);
			}
			else
			{
				rotateTime = g_pGameCVars->hr_rotateTime*2.0f;
				pScreenEffects->CamShake(Vec3(rotateTime,rotateTime,rotateTime*0.5f), Vec3(0, 0.5f * g_pGameCVars->hr_rotateFactor,0), rotateTime, rotateTime);
			}
		}
	}
}

float CMelee::GetOwnerStrength() const
{
	CActor *pActor = m_pWeapon->GetOwnerActor();
	if(!pActor)
		return 1.0f;

	IMovementController * pMC = pActor->GetMovementController();
	if (!pMC)
		return 1.0f;

	float strength = 1.0f;//pActor->GetActorStrength();
	if (pActor->GetActorClass() == CPlayer::GetActorClassType())
	{
		CPlayer *pPlayer = (CPlayer *)pActor;
		if (CNanoSuit *pSuit = pPlayer->GetNanoSuit())
		{
			ENanoMode curMode = pSuit->GetMode();
			if (curMode == NANOMODE_STRENGTH)
			{
				strength = pActor->GetActorStrength();
				strength = strength * (1.0f + 2.0f * pSuit->GetSlotValue(NANOSLOT_STRENGTH)*STRENGTH_MULT);
			}
		}
	}

	return strength;
}

void CMelee::GetMemoryStatistics( ICrySizer * s )
{
	s->Add(*this);
	s->Add(m_name);
	m_meleeparams.GetMemoryStatistics(s);
	m_meleeactions.GetMemoryStatistics(s);
}
