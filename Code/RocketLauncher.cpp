/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  Class for specific rocket launcher functionality
-------------------------------------------------------------------------
History:
- 22:06:2007: Created by Benito G.R.

*************************************************************************/

#include "StdAfx.h"
#include "RocketLauncher.h"

#include "Actor.h"
#include "HUD/HUD.h"
#include "HUD/HUDCrosshair.h"
#include "GameRules.h"

#define RL_LASER_RANGE 150.0f
#define RL_LASER_TP_RANGE 8.0f

#define LASER_TIMEOUT 0.15f

CRocketLauncher::CRocketLauncher()
{
	m_dotEffectSlot = m_smokeEffectSlot = -1;
	m_auxSlotUsed = m_auxSlotUsedBQS = m_laserTPOn = m_laserFPOn = false;
	m_lastLaserHitPt.Set(0.0f,0.0f,0.0f);
	m_lastLaserHitSolid = m_lastLaserHitViewPlane = false;
	m_smoothLaserLength = -1.0f;
}

//========================================
void CRocketLauncher::OnReset()
{
	if(m_stats.backAttachment==eIBA_Unknown)
		m_auxSlotUsed = true;

	CWeapon::OnReset();

	if(m_stats.backAttachment==eIBA_Unknown)
	{
		SetViewMode(0);
		DrawSlot(eIGS_ThirdPersonAux,true);
	}
	else
		m_stats.first_selection = false;

	if(m_smokeEffectSlot!=-1)
	{
		GetEntity()->FreeSlot(m_smokeEffectSlot);
		m_smokeEffectSlot = -1;
	}

	ActivateTPLaser(false);
	m_laserFPOn = m_laserTPOn = false;

	m_lastLaserHitPt.Set(0.0f,0.0f,0.0f);
	m_lastLaserHitSolid = false;
	m_smoothLaserLength = -1.0f;

	m_listeners.clear();
}

//=========================================
void CRocketLauncher::ProcessEvent(SEntityEvent &event)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	CWeapon::ProcessEvent(event);

	if(event.event == ENTITY_EVENT_RESET)
	{
		//Exiting game mode
		if(gEnv->pSystem->IsEditor() && !event.nParam[0])
		{
			if(!GetOwner())
			{
				DrawSlot(eIGS_ThirdPerson,false);
				DrawSlot(eIGS_ThirdPersonAux,true);
				m_auxSlotUsed = true;
			}
			ActivateLaserDot(false, false);
		}
	}	
}
//========================================
bool CRocketLauncher::SetAspectProfile(EEntityAspects aspect, uint8 profile)
{
	if(aspect!=eEA_Physics)
		return CWeapon::SetAspectProfile(aspect, profile);

	bool ok = false;
	if(!gEnv->bMultiplayer && gEnv->pSystem->IsSerializingFile() && m_auxSlotUsedBQS)
		ok = true;

	int slot = (m_auxSlotUsed||ok)?eIGS_ThirdPersonAux:eIGS_ThirdPerson;

	if (aspect == eEA_Physics)
	{
		switch (profile)
		{
		case eIPhys_PhysicalizedStatic:
			{
				SEntityPhysicalizeParams params;
				params.type = PE_STATIC;
				params.nSlot = slot;

				GetEntity()->Physicalize(params);

				return true;
			}
			break;
		case eIPhys_PhysicalizedRigid:
			{
				SEntityPhysicalizeParams params;
				params.type = PE_RIGID;
				params.nSlot = slot;
				params.mass = m_params.mass;

				pe_params_buoyancy buoyancy;
				buoyancy.waterDamping = 1.5;
				buoyancy.waterResistance = 1000;
				buoyancy.waterDensity = 0;
				params.pBuoyancy = &buoyancy;

				GetEntity()->Physicalize(params);

				IPhysicalEntity *pPhysics = GetEntity()->GetPhysics();
				if (pPhysics)
				{
					pe_action_awake action;
					action.bAwake = m_ownerId!=0;
					pPhysics->Action(&action);
				}
			}
			return true;
		case eIPhys_NotPhysicalized:
			{
				IEntityPhysicalProxy *pPhysicsProxy = GetPhysicalProxy();
				if (pPhysicsProxy)
				{
					SEntityPhysicalizeParams params;
					params.type = PE_NONE;
					params.nSlot = slot;
					pPhysicsProxy->Physicalize(params);
				}
			}
			return true;
		}
	}

	return false;
}
//=======================================
void CRocketLauncher::Select(bool select)
{
	if(select && m_auxSlotUsed)
	{
		DrawSlot(eIGS_ThirdPersonAux,false);
		m_auxSlotUsed = false;
	}

	CWeapon::Select(select);

	if(select)
	{
		if(IsOwnerFP())
		{
			ActivateLaserDot(true,true);
			m_laserFPOn = true;
		}
		else
		{
			ActivateTPLaser(true);
			m_laserTPOn = true;
		}
	}
	else
	{
		m_laserFPOn = false;
		ActivateTPLaser(false); //Will do the rest (FP and TP)
	}
}

//========================================
void CRocketLauncher::PickUp(EntityId pickerId, bool sound, bool select, bool keepHistory)
{

	CWeapon::PickUp(pickerId,sound,select,keepHistory);

	if(m_auxSlotUsed)
	{
		DrawSlot(eIGS_ThirdPersonAux,false);
		m_auxSlotUsed = false;
	}

	if(GetOwnerActor() && !GetOwnerActor()->IsPlayer())
		m_stats.first_selection = false;
}

//========================================
void CRocketLauncher::FullSerialize(TSerialize ser)
{
	CWeapon::FullSerialize(ser);

	int smoke = m_smokeEffectSlot;

	ser.Value("dotEffectSlot",m_dotEffectSlot);
	ser.Value("auxSlotUsed", m_auxSlotUsed);
	ser.Value("smokeEffect", m_smokeEffectSlot);

	if(ser.IsReading())
	{	
		if((smoke!=-1) && (m_smokeEffectSlot==-1))
			GetEntity()->FreeSlot(smoke);
	}
	else
	{
		m_auxSlotUsedBQS = m_auxSlotUsed;
	}
}

//=========================================
void CRocketLauncher::PostSerialize()
{
	CWeapon::PostSerialize();

	if(m_auxSlotUsed)
	{
		SetViewMode(0);
		DrawSlot(CItem::eIGS_ThirdPersonAux,true);
		ActivateTPLaser(false);
	}
	else
		DrawSlot(CItem::eIGS_ThirdPersonAux,false);

	if(m_smokeEffectSlot>-1)
		Pickalize(false,true);

}

//=========================================
void CRocketLauncher::ActivateLaserDot(bool activate, bool fp)
{
	if(activate)
	{
		IParticleEffect * pEffect = gEnv->p3DEngine->FindParticleEffect(fp?"muzzleflash.LAM.aimdot_FP":"muzzleflash.LAM.aimdot");

		if(pEffect)
		{
			if(m_dotEffectSlot<0)
			{
				m_dotEffectSlot = GetEntity()->LoadParticleEmitter(CItem::eIGS_Aux0,pEffect);
				if(IParticleEmitter* pEmitter = GetEntity()->GetParticleEmitter(m_dotEffectSlot))
				{
					pEmitter->SetRndFlags(pEmitter->GetRndFlags()|ERF_RENDER_ALWAYS);
					pEmitter->SetViewDistUnlimited();
					pEmitter->SetLodRatio(10000);
				}
			}
		}
	}
	else if(m_dotEffectSlot>-1)
	{
		GetEntity()->FreeSlot(m_dotEffectSlot);
		m_dotEffectSlot = -1;
	}
}

//=======================================
void CRocketLauncher::UpdateFPView(float frameTime)
{
	CWeapon::UpdateFPView(frameTime);

	if(m_laserFPOn)
		UpdateDotEffect(frameTime);
}

//=======================================
void CRocketLauncher::Update(SEntityUpdateContext& ctx, int slot)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	CWeapon::Update(ctx,slot);

	if(slot==eIUS_General && m_laserTPOn)
	{
		UpdateTPLaser(ctx.fFrameTime);
		RequireUpdate(eIUS_General);
	}
}

//========================================
void CRocketLauncher::Drop(float impulseScale, bool selectNext, bool byDeath)
{
	CActor* pOwner = GetOwnerActor();
	//Don't let the player drop it if it has not been opened
	if(m_stats.first_selection && pOwner && pOwner->IsPlayer() && pOwner->GetHealth()>0 && !byDeath)
		return;

	if(pOwner && !pOwner->IsPlayer())
	{
		//In this case goes to the clip, no the inventory
		for(TAmmoMap::const_iterator it = m_minDroppedAmmo.begin();it!=m_minDroppedAmmo.end();it++)
			m_ammo[it->first] = it->second;

		m_minDroppedAmmo.clear();
	}

	CWeapon::Drop(impulseScale,selectNext,byDeath);

	if(m_fm && GetAmmoCount(m_fm->GetAmmoType())<=0)
	{
		Pickalize(false,true);
		if(m_smokeEffectSlot==-1)
		{
			IParticleEffect * pEffect = gEnv->p3DEngine->FindParticleEffect("weapon_fx.LAW.empty");

			if(pEffect)
				m_smokeEffectSlot = GetEntity()->LoadParticleEmitter(-1,pEffect);
		}
	}
}

//=========================================
bool CRocketLauncher::CanPickUp(EntityId userId) const
{
	CActor *pActor = GetActor(userId);
	IInventory *pInventory=GetActorInventory(pActor);

	if (m_params.pickable && m_stats.pickable && !m_stats.flying && !m_frozen &&(!m_ownerId || m_ownerId==userId) && !m_stats.selected && !GetEntity()->IsHidden())
	{
		if (pInventory && pInventory->FindItem(GetEntityId())!=-1)
			return false;
	}
	else
		return false;

	uint8 uniqueId = m_pItemSystem->GetItemUniqueId(GetEntity()->GetClass()->GetName());

	//Can not pick up a LAW while I have one already 
	if(pInventory && (pInventory->GetCountOfUniqueId(uniqueId)>0))
	{
		if(pActor->IsClient())
			g_pGame->GetGameRules()->OnTextMessage(eTextMessageCenter, "@mp_CannotCarryMoreLAW");
		return false;
	}

	return true;
		
}
//=========================================
void CRocketLauncher::UpdateDotEffect(float frameTime)
{

	Vec3 laserPos, dir;

	laserPos = GetSlotHelperPos(eIGS_FirstPerson,"scope",true);
	Quat   rot = Quat(GetSlotHelperRotation(eIGS_FirstPerson,"weapon_term",true));
	dir =  rot.GetColumn0();

	dir.Normalize();

	CActor *pActor = GetOwnerActor();
	IMovementController * pMC = pActor ? pActor->GetMovementController() : NULL;
	if (pMC)
	{ 
		SMovementState info;
		pMC->GetMovementState(info);

		if(dir.Dot(info.fireDirection)>0.985f)
		{
			CCamera& camera = gEnv->pSystem->GetViewCamera();
			laserPos = camera.GetPosition();
			dir = camera.GetMatrix().GetColumn1();
			dir.Normalize();
		}		
	}

	float  len = RL_LASER_RANGE;

	const float nearClipPlaneLimit = 10.0f;

	Vec3 hitPos(0,0,0);
	float laserLength = 0.0f;
	float dotScale=1.0f;
	{
		IPhysicalEntity* pSkipEntity = NULL;
		if(GetOwner())
			pSkipEntity = GetOwner()->GetPhysics();

		const int objects = ent_all;
		const int flags = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (10 & rwi_pierceability_mask) | (geom_colltype14 << rwi_colltype_bit);

		ray_hit hit;	
		if (gEnv->pPhysicalWorld->RayWorldIntersection(laserPos, dir*RL_LASER_RANGE, objects,
			flags, &hit, 1, &pSkipEntity, pSkipEntity?1:0))
		{
			//Clamp distance below near clip plane limits, if not dot will be overdrawn during rasterization
			if(hit.dist>nearClipPlaneLimit)
			{
				laserLength = nearClipPlaneLimit;
				hitPos = laserPos + (nearClipPlaneLimit*dir);
			}
			else
			{
				laserLength = hit.dist;
				hitPos = hit.pt;
			}
			if(GetOwnerActor() && GetOwnerActor()->GetActorParams())
				dotScale *= GetOwnerActor()->GetActorParams()->viewFoVScale;
		}
		else
		{
			hitPos = laserPos - (3.0f*dir);
			laserLength = 3.0f;
		}
	}

	if (m_dotEffectSlot>=0)
	{
		//Force it always to be active
		//if(IParticleEmitter* pEmitter = GetEntity()->GetParticleEmitter(m_dotEffectSlot))
			//pEmitter->Hide(false);

		Matrix34 worldMatrix = GetEntity()->GetWorldTM();
		if(laserLength<=0.7f)
			hitPos = laserPos+(0.7f*dir);

		CPlayer* pPlayer = static_cast<CPlayer*>(GetOwnerActor());
		SPlayerStats* pStats = pPlayer?static_cast<SPlayerStats*>(pPlayer->GetActorStats()):NULL;
		if(pStats && pStats->bLookingAtFriendlyAI)
		{
			hitPos = laserPos+(2.0f*dir);
			laserLength = 2.0f;
		}

		if(laserLength<=2.0f)
			dotScale *= min(1.0f,(0.35f + ((laserLength-0.7f)*0.5f)));

		Matrix34 localMatrix = worldMatrix.GetInverted()*Matrix34::CreateTranslationMat(hitPos-(0.2f*dir));
		localMatrix.Scale(Vec3(dotScale,dotScale,dotScale));
		GetEntity()->SetSlotLocalTM(m_dotEffectSlot, localMatrix);
	}


}

//----------------------------------------------------
void CRocketLauncher::ActivateTPLaser(bool activate)
{
	if(activate)
	{
		DrawSlot(CItem::eIGS_Aux1,true);
		ActivateLaserDot(true,false);
		m_laserTPOn = true;
		
		//Force first update
		m_lastUpdate = 0.0f;
		m_smoothLaserLength = -1.0f;
		UpdateTPLaser(0.0f);
		RequireUpdate(eIUS_General);
	}
	else
	{
		DrawSlot(CItem::eIGS_Aux1,false);
		GetEntity()->SetSlotLocalTM(CItem::eIGS_Aux1,Matrix34::CreateIdentity());
		ActivateLaserDot(false,false);
		m_laserTPOn = false;
	}
}

//----------------------------------------------------
void CRocketLauncher::UpdateTPLaser(float frameTime)
{
	
	m_lastUpdate -= frameTime;

	bool allowUpdate = true;
	if(m_lastUpdate<=0.0f)
		m_lastUpdate = LASER_TIMEOUT;
	else
		allowUpdate = false;

	const CCamera& camera = gEnv->pRenderer->GetCamera();

	//If character not visible, laser is not correctly updated
	if(CActor* pOwner = GetOwnerActor())
	{
		ICharacterInstance* pCharacter = pOwner->GetEntity()->GetCharacter(0);
		if(pCharacter && !pCharacter->IsCharacterVisible())
			return;
	}

	Vec3   offset(-0.06f,0.28f,0.115f); //To match scope position in TP LAW model

	Vec3   pos = GetEntity()->GetWorldTM().TransformPoint(offset);
	Vec3   dir = GetEntity()->GetWorldRotation().GetColumn1();

	float  len = RL_LASER_TP_RANGE;

	Vec3 hitPos(0,0,0);
	float laserLength = 0.0f;

	if(allowUpdate)
	{
		IPhysicalEntity* pSkipEntity = NULL;
		if(GetOwner())
			pSkipEntity = GetOwner()->GetPhysics();

		const float range = RL_LASER_TP_RANGE;

		// Use the same flags as the AI system uses for visibility.
		const int objects = ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid|ent_independent; //ent_living;
		const int flags = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (10 & rwi_pierceability_mask) | (geom_colltype14 << rwi_colltype_bit);

		ray_hit hit;
		if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, dir*range, objects, flags,
			&hit, 1, &pSkipEntity, pSkipEntity ? 1 : 0))
		{
			laserLength = hit.dist;
			m_lastLaserHitPt = hit.pt;
			m_lastLaserHitSolid = true;
		}
		else
		{
			m_lastLaserHitSolid = false;
			m_lastLaserHitPt = pos + dir * range;
			laserLength = range + 0.1f;
		}

		// Hit near plane
		if (dir.Dot(camera.GetViewdir()) < 0.0f)
		{
			Plane nearPlane;
			nearPlane.SetPlane(camera.GetViewdir(), camera.GetPosition());
			nearPlane.d -= camera.GetNearPlane()+0.15f;
			Ray ray(pos, dir);
			Vec3 out;
			m_lastLaserHitViewPlane = false;
			if (Intersect::Ray_Plane(ray, nearPlane, out))
			{
				float dist = Distance::Point_Point(pos, out);
				if (dist < laserLength)
				{
					laserLength = dist;
					m_lastLaserHitPt = out;
					m_lastLaserHitSolid = true;
					m_lastLaserHitViewPlane = true;
				}
			}
		}

		hitPos = m_lastLaserHitPt;
	}
	else
	{
		laserLength = Distance::Point_Point(m_lastLaserHitPt, pos);
		hitPos = pos + dir * laserLength;
	}

	if (m_smoothLaserLength < 0.0f)
		m_smoothLaserLength = laserLength;
	else
	{
		if (laserLength < m_smoothLaserLength)
			m_smoothLaserLength = laserLength;
		else
			m_smoothLaserLength += (laserLength - m_smoothLaserLength) * min(1.0f, 10.0f * frameTime);
	}

	const float assetLength = 2.0f;
	m_smoothLaserLength = CLAMP(m_smoothLaserLength,0.01f,RL_LASER_TP_RANGE);
	float scale = m_smoothLaserLength / assetLength; 

	// Scale the laser based on the distance.
	Matrix34 scl;
	scl.SetIdentity();
	scl.SetScale(Vec3(1,scale,1));
	scl.SetTranslation(offset);
	GetEntity()->SetSlotLocalTM(CItem::eIGS_Aux1, scl);

	if (m_dotEffectSlot >= 0)
	{
		if (m_lastLaserHitSolid)
		{
			Matrix34 dotMatrix = Matrix34::CreateTranslationMat(Vec3(0,m_smoothLaserLength,0));
			dotMatrix.AddTranslation(offset);
			if(m_lastLaserHitViewPlane)
				dotMatrix.Scale(Vec3(0.2f,0.2f,0.2f));
			GetEntity()->SetSlotLocalTM(m_dotEffectSlot,dotMatrix);
		}
		else
		{
			Matrix34 scaleMatrix;
			scaleMatrix.SetIdentity();
			scaleMatrix.SetScale(Vec3(0.001f,0.001f,0.001f));
			GetEntity()->SetSlotLocalTM(m_dotEffectSlot, scaleMatrix);
		}
	}

}

//===============================================
void CRocketLauncher::GetAttachmentsAtHelper(const char *helper, std::vector<string> &rAttachments)
{
	//Do nothing...
	//Rocket launcher has an special helper for the scope, but it must be skipped by the HUD
}