/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 16:10:2006   16:56 : Created by Benito Gangoso Rodriguez

*************************************************************************/
#include "StdAfx.h"
#include "Lam.h"
#include "Actor.h"
#include "Player.h"
#include "ParticleParams.h"
#include "Weapon.h"
#include "GameCVars.h"

#include <IMaterialEffects.h>

#define  CAMERA_DISTANCE_SQR	100
#define  LASER_UPDATE_TIME		0.1f
#define  LASER_UPDATE_TIME_FP 0.15f

uint CLam::s_lightCount = 0;		//Init light count
VectorSet<CLam*> CLam::s_lasers;
uint CLam::s_curLaser = 0;
float CLam::s_laserUpdateTimeError = 0.0f;
int CLam::s_lastUpdateFrameId = 0;


CLam::CLam()
{
	m_laserActivated = false;
	m_lightActivated = false;
	m_lightWasOn = false;
	m_lightWasOn = false;
  m_lightSoundId = INVALID_SOUNDID;
	m_pLaserEntityId = 0;
	m_dotEffectSlot = -1;
	m_laserEffectSlot = -1;
	m_laserHelperFP.clear();
	m_lastLaserHitPt.Set(0,0,0);
	m_lastLaserHitSolid = m_lastLaserHitViewPlane = false;
	m_allowUpdate = false;
	m_updateTime = 0.0f;
	m_smoothLaserLength = -1.0f;
	m_lastZPos = 0.0f;
	m_lightActiveSerialize = m_laserActiveSerialize = false;
	s_lasers.insert(this);
}

CLam::~CLam()
{
	s_lasers.erase(this);
	OnReset();
}

//-------------------------------------------------------------------------
bool CLam::Init(IGameObject * pGameObject )
{
	if (!CItem::Init(pGameObject))
		return false;

	//Initialize light/laser info
	m_laserActivated = false;

	if(m_lightActivated)
		s_lightCount--;		

	m_lightActivated = false;
	m_lightID[0] = m_lightID[1] = 0;
	
	m_lightWasOn = false;
	m_lightWasOn = false;
	
	return true;
}

//------------------------------------------------------------------------
bool CLam::ReadItemParams(const IItemParamsNode *root)
{
  if (!CItem::ReadItemParams(root))
    return false;

  const IItemParamsNode* pLamParams = root->GetChild("lam");
  m_lamparams.Reset(pLamParams);    

  return true;
}

//-------------------------------------------------------------------------
void CLam::Reset()
{
	CItem::Reset();

	ActivateLight(false);
	ActivateLaser(false);

	m_lightWasOn = false;
	m_laserWasOn = false;
	m_lastLaserHitPt.Set(0,0,0);
	m_lastLaserHitSolid = false;
	m_smoothLaserLength = -1.0f;
	m_allowUpdate = false;
	m_updateTime = 0.0f;

	DestroyLaserEntity();
	m_laserHelperFP.clear();
}

//-------------------------------------------------------------------------
void CLam::ActivateLaser(bool activate, bool aiRequest /* = false */)
{
	if (m_laserActivated == activate)
		return;

	CItem  *pParent = NULL;
	EntityId ownerId = 0;
	bool ok = false;
  
	if (IItem *pOwnerItem = m_pItemSystem->GetItem(GetParentId()))
	{
		pParent = (CItem *)pOwnerItem;
		IWeapon *pWeapon = pOwnerItem->GetIWeapon();		
		if(pWeapon)
			ownerId = pOwnerItem->GetOwnerId();

		ok = true;
	}
  else
  {
    pParent = this;
    ownerId = GetOwnerId();
  }

  IActor *pOwnerActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(ownerId);
  if(!pOwnerActor)
    return;

	if(activate && !aiRequest && !pOwnerActor->IsPlayer())
		return;

	//Special FP stuff
	if(pOwnerActor->IsPlayer() && !m_lamparams.isLaser)
		return;
	
	m_laserActivated = activate;

	//Activate or deactivate effect??
	if (!m_laserActivated)
	{
    AttachLAMLaser(false, eIGS_FirstPerson);
    AttachLAMLaser(false, eIGS_ThirdPerson);
	}
	else
	{
    bool tp = pOwnerActor->IsThirdPerson();
		if(!tp && ok)
		{
			SAccessoryParams *params = pParent->GetAccessoryParams(GetEntity()->GetClass()->GetName());
			if (!params)
				return;

			m_laserHelperFP.clear();
			m_laserHelperFP = params->attach_helper.c_str();
			m_laserHelperFP.replace("_LAM","");
		}
    AttachLAMLaser(true, tp?eIGS_ThirdPerson:eIGS_FirstPerson);      
	}

	if (m_laserActivated || m_lightActivated)
		GetGameObject()->EnablePostUpdates(this);
	if (!m_laserActivated && !m_lightActivated)
		GetGameObject()->DisablePostUpdates(this);
}

//-------------------------------------------------------------------------
void CLam::ActivateLight(bool activate, bool aiRequest /* = false */)
{
	//GameWarning("CLam::ActivateLight(%i)", activate);
	
	CItem  *pParent = NULL;
  EntityId ownerId = 0;
	
	if (IItem *pOwnerItem = m_pItemSystem->GetItem(GetParentId()))
	{
		pParent = (CItem *)pOwnerItem;
		IWeapon *pWeapon = pOwnerItem->GetIWeapon();		
		if(pWeapon)
			ownerId = pOwnerItem->GetOwnerId();
	}
	else   
  {
    pParent = this;
    ownerId = GetOwnerId();
  }

  IActor *pOwnerActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(ownerId);
  if (activate && !pOwnerActor)
    return;

	//Special FP stuff
	if(pOwnerActor && pOwnerActor->IsPlayer() && !m_lamparams.isFlashLight)
		return;

	//For AI must be deactivated by default (if they don't request)
	if(activate && !m_lightWasOn && !aiRequest && !pOwnerActor->IsPlayer())
		return;

	m_lightActivated = activate;

	//Activate or deactivate effect
	if (!m_lightActivated)
	{
    AttachLAMLight(false, pParent, eIGS_FirstPerson);
    AttachLAMLight(false, pParent, eIGS_ThirdPerson);
    
		//GameWarning("Global light count = %d", s_lightCount);		
	}
	else
	{
    uint8 id = pOwnerActor->IsThirdPerson() ? 1 : 0;
    if (m_lightID[id] == 0)
    {
      AttachLAMLight(true, pParent, id?eIGS_ThirdPerson:eIGS_FirstPerson);      
    }		
	}

	if (m_laserActivated || m_lightActivated)
		GetGameObject()->EnablePostUpdates(this);
	if (!m_laserActivated && !m_lightActivated)
		GetGameObject()->DisablePostUpdates(this);

}

//-------------------------------------------------------------------------
void CLam::OnAttach(bool attach)
{
	CItem::OnAttach(attach);

	if(CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(GetParentId())))
	{
		if(pItem->GetOwnerActor() && pItem->GetOwnerActor()->IsPlayer())
		{
			ActivateLaser(attach);
			ActivateLight(attach);
		}
	}
}

//-------------------------------------------------------------------------
void CLam::OnParentSelect(bool select)
{
  CItem::OnParentSelect(select);

	ActivateLaser(select);
	ActivateLight(select);

  //Track previous state if deselected
  if(!select)
    SaveLastState();

}

//-------------------------------------------------------------------------
void CLam::OnSelect(bool select)
{
  CItem::OnSelect(select);
}

//-------------------------------------------------------------------------
void CLam::AttachLAMLaser(bool attach, eGeometrySlot slot)
{
	if(slot==eIGS_ThirdPerson)
	{
		if(attach)
		{
			CreateLaserEntity();
			SetLaserGeometry(m_lamparams.laser_geometry_tp);
			UpdateLaserScale(1.0f,m_pEntitySystem->GetEntity(m_pLaserEntityId));
			m_lastLaserHitPt.Set(0,0,0);
			m_lastLaserHitSolid = false;
			CreateLaserDot(m_lamparams.laser_dot[slot],slot);

		}
		else
		{
			DestroyLaserEntity();
		}
	}
	else if(slot==eIGS_FirstPerson)
	{
		if(attach)
		{
			CreateLaserEntity();
			CreateLaserDot(m_lamparams.laser_dot[slot],slot);
		}
		else
		{
			DestroyLaserEntity();
		}
	}
}

//-------------------------------------------------------------------------
void CLam::AttachLAMLight(bool attach, CItem* pLightAttach, eGeometrySlot slot)
{	
  //GameWarning("CLam::AttachLight");

  int id = (slot==eIGS_FirstPerson) ? 0 : 1;

  if (attach)
  {
    if (m_lamparams.light_range[id] == 0.f)
      return;

		Vec3 color = m_lamparams.light_color[id] * m_lamparams.light_diffuse_mul[id];
    float specular = 1.0f/m_lamparams.light_diffuse_mul[id];
		
    string helper;
    Vec3 dir(-1,0,0);
		Vec3 localOffset(0.0f,0.0f,0.0f);

    if (this != pLightAttach)
    {
      SAccessoryParams *params = pLightAttach->GetAccessoryParams(GetEntity()->GetClass()->GetName());
      if (!params)
        return;

      helper = params->attach_helper.c_str();
			if(slot==eIGS_FirstPerson)
				helper.append("_light");
       
			//Assets don't have same orientation for pistol/rifle.. 8/
      dir = (m_lamparams.isLamRifle && id==0) ? Vec3(-0.1f,-1.0f,0.0f) : Vec3(-1.0f,-0.1f,0.0f);
			dir.Normalize();
    }

		bool fakeLight = false;
		bool castShadows = false;

		//Some MP/SP restrictions for lights
		IRenderNode *pCasterException = NULL;
		if(CActor *pOwner = pLightAttach->GetOwnerActor())
		{
			if(gEnv->bMultiplayer)
			{
				if(!pOwner->IsClient())
					fakeLight = true;
				else
					castShadows = true;
			}
			else
			{
				if(pOwner->IsPlayer())
					castShadows = true;
				//castShadows = false; //Not for now
			}

			if(castShadows)
			{
				if(IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(pOwner->GetEntity()->GetProxy(ENTITY_PROXY_RENDER)))
					pCasterException = pRenderProxy->GetRenderNode();
			}
		}
		
    m_lightID[id] = pLightAttach->AttachLightEx(slot, 0, true, fakeLight, castShadows, pCasterException, m_lamparams.light_range[id], color, specular, m_lamparams.light_texture[id], m_lamparams.light_fov[id], helper.c_str(), localOffset, dir, m_lamparams.light_material[id].c_str(), m_lamparams.light_hdr_dyn[id]);

    if (m_lightID[id])
      ++s_lightCount;

    // sounds
    pLightAttach->PlayAction(g_pItemStrings->enable_light);

    if (m_lightSoundId == INVALID_SOUNDID)      
      m_lightSoundId = pLightAttach->PlayAction(g_pItemStrings->use_light);

    //Detach the non-needed light 
    uint8 other = id^1;
    if (m_lightID[other])
    {
      pLightAttach->AttachLightEx(other, m_lightID[other], false, true),
        m_lightID[other] = 0;
      --s_lightCount;
    }
  }
  else
  {
    if (m_lightID[id])    
    {
      pLightAttach->AttachLightEx(slot, m_lightID[id], false, true);           
      m_lightID[id] = 0;
      --s_lightCount;

      PlayAction(g_pItemStrings->disable_light);
      StopSound(m_lightSoundId);    
      m_lightSoundId = INVALID_SOUNDID;      
    }        
  }  
	
	//GameWarning("Global light count = %d", s_lightCount);
}


//--------------------------------------------------------------------------
void CLam::SaveLastState()
{
	if(m_lightActivated)
		m_lightWasOn = true;
	else
		m_lightWasOn = false;

	if(m_laserActivated)
		m_laserWasOn = true;
	else
		m_laserWasOn = false;
}

//---------------------------------------------------------------------------
void CLam::CreateLaserEntity()
{
	if(m_pLaserEntityId)
	{
		//Check if entity is valid
		IEntity *pEntity = m_pEntitySystem->GetEntity(m_pLaserEntityId);
		if(!pEntity)
			m_pLaserEntityId = 0;
	}

	if (!m_pLaserEntityId)
	{
		SEntitySpawnParams spawnParams;
		spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
		spawnParams.sName = "LAMLaser";
		spawnParams.nFlags = (GetEntity()->GetFlags() | ENTITY_FLAG_NO_SAVE) & ~ENTITY_FLAG_CASTSHADOW;

		IEntity *pNewEntity =gEnv->pEntitySystem->SpawnEntity(spawnParams);
		//assert(pNewEntity && "Laser entity could no be spawned!!");

		if(pNewEntity)
		{
			pNewEntity->FreeSlot(0);
			pNewEntity->FreeSlot(1);

			m_pLaserEntityId = pNewEntity->GetId();

			if(IEntity* pEntity = GetEntity())
				pEntity->AttachChild(pNewEntity);

			IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)pNewEntity->GetProxy(ENTITY_PROXY_RENDER);
			IRenderNode * pRenderNode = pRenderProxy?pRenderProxy->GetRenderNode():NULL;

			if(pRenderNode)
				pRenderNode->SetRndFlags(ERF_RENDER_ALWAYS,true);

		}
	}
}

//-------------------------------------------------------------------------
void CLam::DestroyLaserEntity()
{
	if (m_pLaserEntityId)
		gEnv->pEntitySystem->RemoveEntity(m_pLaserEntityId);
	m_pLaserEntityId = 0;
}

//--------------------------------------------------------------------------
void CLam::SetLaserGeometry(const char *name)
{
	IEntity *pEntity = m_pEntitySystem->GetEntity(m_pLaserEntityId);
	//assert(pEntity && "CLam::SetLaserGeometry : Laser entity not found in entity system");
	if(pEntity)
		m_laserEffectSlot = pEntity->LoadGeometry(-1, name);
}

//------------------------------------------------------------------------
void CLam::CreateLaserDot(const char *name, eGeometrySlot slot)
{
	IParticleEffect * pEffect = gEnv->p3DEngine->FindParticleEffect(name);
	
	if(pEffect)
	{
		IEntity *pEntity = m_pEntitySystem->GetEntity(m_pLaserEntityId);
		//assert(pEntity && "CLam::CreateLaserDot : Laser entity not found!!");
		if(pEntity)
			m_dotEffectSlot = pEntity->LoadParticleEmitter(-1,pEffect);
	}
}

//---------------------------------------------------------------------------
void CLam::PostUpdate(float frameTime)
{
	if (IItem *pOwnerItem = m_pItemSystem->GetItem(GetParentId()))
	{
		CItem* pParentItem = (CItem *)pOwnerItem;
		if(!pParentItem->IsOwnerFP() && pParentItem->IsSelected())
			UpdateTPLaser(frameTime,pParentItem);
		else if(pParentItem->IsOwnerFP())
			UpdateFPLaser(frameTime,pParentItem);
	}

}

//------------------------------------------------------------------
void CLam::UpdateAILightAndLaser(const Vec3& pos, const Vec3& dir, float lightRange, float fov, float laserRange)
{
	if (!gEnv->pAISystem)
		return;

	IAIObject* pUserAI = 0;
	if (IItem *pOwnerItem = m_pItemSystem->GetItem(GetParentId()))
	{
		CItem* pParentItem = (CItem *)pOwnerItem;
		CActor* pActor = pParentItem->GetOwnerActor();
		if (pActor && pActor->GetEntity())
			pUserAI = pActor->GetEntity()->GetAI();
	}

	if (lightRange > 0.0001f)
	{
		gEnv->pAISystem->DynSpotLightEvent(pos, dir, lightRange, DEG2RAD(fov)/2, AILE_FLASH_LIGHT, pUserAI, 1.0f);
		if (pUserAI)
			gEnv->pAISystem->DynOmniLightEvent(pUserAI->GetPos() + dir*0.75f, 1.5f, AILE_FLASH_LIGHT, pUserAI, 2.0f);
	}

	// [Mikko] 4.10.2007 Removed as per design request.
/*	if (laserRange > 0.0001f)
	{
		gEnv->pAISystem->DynSpotLightEvent(pos, dir, max(0.0f, laserRange - 0.1f), DEG2RAD(0.25f), AILE_LASER, pUserAI, 2.0f);
	}*/
}

//------------------------------------------------------------------
void CLam::UpdateTPLaser(float frameTime, CItem* parent)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	const int frameId = gEnv->pRenderer->GetFrameID();

	if (s_lastUpdateFrameId != frameId)
	{
		// Check how many LAMs to update this frame.
		float dt = frameTime; // + s_laserUpdateTimeError;

		const int n = s_lasers.size();

		int nActive = 0;
		for (int i = 0; i < n; ++i)
		{
			if (!s_lasers[i]->IsLaserActivated() && !s_lasers[i]->IsLightActivated())
				continue;
			nActive++;
		}

		float updatedPerSecond = (nActive / LASER_UPDATE_TIME) + s_laserUpdateTimeError;
		int updateCount = (int)floorf(updatedPerSecond * dt);
		if(dt==0.0f)
			s_laserUpdateTimeError = 0.0f;
		else
			s_laserUpdateTimeError = updatedPerSecond - updateCount/dt;

		s_curLaser %= n;
		for (int i = 0, j = 0; i < n && j < updateCount ; ++i)
		{
			s_curLaser = (s_curLaser + 1) % n;
			if (!s_lasers[s_curLaser]->IsLaserActivated() && !s_lasers[s_curLaser]->IsLightActivated())
				continue;
			s_lasers[s_curLaser]->SetAllowUpdate();
			++j;
		}

		s_lastUpdateFrameId = frameId;
	}

	IEntity* pRootEnt = GetEntity();
	if (!pRootEnt)
		return;

	IEntity *pLaserEntity = m_pEntitySystem->GetEntity(m_pLaserEntityId);
//	if(!pLaserEntity)
//		return;

	const CCamera& camera = gEnv->pRenderer->GetCamera();

	Vec3   lamPos = pRootEnt->GetWorldPos(); //pLaserEntity->GetParent()->GetWorldPos();
	Vec3   dir = pRootEnt->GetWorldRotation().GetColumn1(); //pLaserEntity->GetParent()->GetWorldRotation().GetColumn1();

	bool charNotVisible = false;

	float  dsg1Scale = 1.0f;

	//If character not visible, laser is not correctly updated
	if(parent)
	{
		if(CActor* pOwner = parent->GetOwnerActor())
		{
			ICharacterInstance* pCharacter = pOwner->GetEntity()->GetCharacter(0);
			if(pCharacter && !pCharacter->IsCharacterVisible())
				charNotVisible = true;
		}
		if(parent->GetEntity()->GetClass()==CItem::sDSG1Class)
			dsg1Scale = 3.0f;
	}
 
//	if (!pLaserEntity->GetParent())
//		return;

	Vec3 hitPos(0,0,0);
	float laserLength = 0.0f;

	// workaround??: Use player movement controller locations, or else the laser
	// pops all over the place when character out of the screen.
	CActor *pActor = parent->GetOwnerActor();
	if (pActor && (!pActor->IsPlayer() || charNotVisible))
	{
		if (IMovementController* pMC = pActor->GetMovementController())
		{
			SMovementState state;
			pMC->GetMovementState(state);
			if(!charNotVisible)
				lamPos = state.weaponPosition;
			else
			{
				float oldZPos = lamPos.z;
				lamPos = state.weaponPosition;
				if(m_lastZPos>0.0f)
					lamPos.z = m_lastZPos; //Stabilize somehow z position (even if not accurate)
				else
					lamPos.z = oldZPos;
			}
			const float angleMin = DEG2RAD(3.0f);
			const float angleMax = DEG2RAD(7.0f);
			const float thr = cosf(angleMax);
			float dot = dir.Dot(state.aimDirection);
			if (dot > thr)
			{
				float a = acos_tpl(dot);
				float u = 1.0f - clamp((a - angleMin) / (angleMax - angleMin), 0.0f, 1.0f);
				dir = dir + u * (state.aimDirection - dir);
				dir.Normalize();
			}
		}
	}

	if(!charNotVisible)
		m_lastZPos = lamPos.z;

	lamPos += (dir*0.10f);

	if (m_allowUpdate)
	{
		m_allowUpdate = false;

		IPhysicalEntity* pSkipEntity = NULL;
		if(parent->GetOwner())
			pSkipEntity = parent->GetOwner()->GetPhysics();

		const float range = m_lamparams.laser_range[eIGS_ThirdPerson]*dsg1Scale;

		// Use the same flags as the AI system uses for visbility.
		const int objects = ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid|ent_independent; //ent_living;
		const int flags = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (10 & rwi_pierceability_mask) | (geom_colltype14 << rwi_colltype_bit);

		ray_hit hit;
		if (gEnv->pPhysicalWorld->RayWorldIntersection(lamPos, dir*range, objects, flags,
			&hit, 1, &pSkipEntity, pSkipEntity ? 1 : 0))
		{
			laserLength = hit.dist;
			m_lastLaserHitPt = hit.pt;
			m_lastLaserHitSolid = true;
		}
		else
		{
			m_lastLaserHitSolid = false;
			m_lastLaserHitPt = lamPos + dir * range;
			laserLength = range + 0.1f;
		}

		// Hit near plane
		if (dir.Dot(camera.GetViewdir()) < 0.0f)
		{
			Plane nearPlane;
			nearPlane.SetPlane(camera.GetViewdir(), camera.GetPosition());
			nearPlane.d -= camera.GetNearPlane()+0.15f;
			Ray ray(lamPos, dir);
			Vec3 out;
			m_lastLaserHitViewPlane = false;
			if (Intersect::Ray_Plane(ray, nearPlane, out))
			{
				float dist = Distance::Point_Point(lamPos, out);
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
		laserLength = Distance::Point_Point(m_lastLaserHitPt, lamPos);
		hitPos = lamPos + dir * laserLength;
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

	float laserAIRange = 0.0f;
	if (m_laserActivated && pLaserEntity)
	{
		// Orient the laser towards the point point.
		Matrix34 parentTMInv;
		parentTMInv = pRootEnt->GetWorldTM().GetInverted();

		Vec3 localDir = parentTMInv.TransformPoint(hitPos);
		float finalLaserLen = localDir.NormalizeSafe();
		Matrix33 rot;
		rot.SetIdentity();
		rot.SetRotationVDir(localDir);
		pLaserEntity->SetLocalTM(rot);

		laserAIRange = finalLaserLen;

		const float assetLength = 2.0f;
		finalLaserLen = CLAMP(finalLaserLen,0.01f,m_lamparams.laser_max_len*dsg1Scale);
		float scale = finalLaserLen / assetLength; 

		// Scale the laser based on the distance.
		if (m_laserEffectSlot >= 0)
		{
			Matrix33 scl;
			scl.SetIdentity();
			scl.SetScale(Vec3(1,scale,1));
			pLaserEntity->SetSlotLocalTM(m_laserEffectSlot, scl);
		}

		if (m_dotEffectSlot >= 0)
		{
			if (m_lastLaserHitSolid)
			{
				Matrix34 mt = Matrix34::CreateTranslationMat(Vec3(0,finalLaserLen,0));
				if(m_lastLaserHitViewPlane)
					mt.Scale(Vec3(0.2f,0.2f,0.2f));
				pLaserEntity->SetSlotLocalTM(m_dotEffectSlot, mt);
			}
			else
			{
				Matrix34 scaleMatrix;
				scaleMatrix.SetIdentity();
				scaleMatrix.SetScale(Vec3(0.001f,0.001f,0.001f));
				pLaserEntity->SetSlotLocalTM(m_dotEffectSlot, scaleMatrix);
			}
		}
	}

	float lightAIRange = 0.0f;
	if (m_lightActivated)
	{
		float range = clamp(m_smoothLaserLength, 0.5f, m_lamparams.light_range[eIGS_ThirdPerson]);
		lightAIRange = range * 1.5f;

		if (m_lightID[eIGS_ThirdPerson] && m_smoothLaserLength > 0.0f)
		{
			CItem* pLightEffect = this;
			if (IItem *pOwnerItem = m_pItemSystem->GetItem(GetParentId()))
				pLightEffect = (CItem *)pOwnerItem;
			pLightEffect->SetLightRadius(range, m_lightID[eIGS_ThirdPerson]);
		}
	}


	if (laserAIRange > 0.0001f || lightAIRange > 0.0001f)
		UpdateAILightAndLaser(lamPos, dir, lightAIRange, m_lamparams.light_fov[eIGS_ThirdPerson], laserAIRange);

}

//------------------------------------------------------------------
void CLam::UpdateFPLaser(float frameTime, CItem* parent)
{
	Vec3 lamPos, dir;
	
	if (m_laserActivated)
		AdjustLaserFPDirection(parent,dir,lamPos);
	else
	{
		// Lam Light
		lamPos = parent->GetSlotHelperPos(eIGS_FirstPerson,m_laserHelperFP.c_str(),true);
		Quat   lamRot = Quat(parent->GetSlotHelperRotation(eIGS_FirstPerson,m_laserHelperFP.c_str(),true));
		dir = lamRot.GetColumn1();
	}

	float  len = m_lamparams.laser_range[eIGS_FirstPerson];

	dir.Normalize();

	const float nearClipPlaneLimit = 10.0f;

	Vec3 hitPos(0,0,0);
	float laserLength = 0.0f;
	float dotScale = 1.0f;
	{
		IPhysicalEntity* pSkipEntity = NULL;
		if(parent->GetOwner())
			pSkipEntity = parent->GetOwner()->GetPhysics();

		const int objects = ent_all;
		const int flags = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (10 & rwi_pierceability_mask) | (geom_colltype14 << rwi_colltype_bit);

		ray_hit hit;	
		if (gEnv->pPhysicalWorld->RayWorldIntersection(lamPos, dir*m_lamparams.laser_range[eIGS_FirstPerson], objects,
			flags, &hit, 1, &pSkipEntity, pSkipEntity?1:0))
		{

			//Clamp distance below near clip plane limits, if not dot will be overdrawn during rasterization
			if(hit.dist>nearClipPlaneLimit)
			{
				laserLength = nearClipPlaneLimit;
				m_lastLaserHitPt = lamPos + (nearClipPlaneLimit*dir);
			}
			else
			{
				laserLength = hit.dist;
				m_lastLaserHitPt = hit.pt;
			}
			m_lastLaserHitSolid = true;
			if(parent->GetOwnerActor() && parent->GetOwnerActor()->GetActorParams())
				dotScale *= max(0.3f,parent->GetOwnerActor()->GetActorParams()->viewFoVScale);

		}
		else
		{
			m_lastLaserHitSolid = false;
			m_lastLaserHitPt = lamPos - (dir*3.0f);
			laserLength = 3.0f;
		}
		hitPos = m_lastLaserHitPt;
		if(g_pGameCVars->i_debug_projectiles!=0)
		 gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(hitPos, 0.2f, ColorB(255,0,0));
	}

	if (m_laserActivated && m_dotEffectSlot >= 0)
	{
		Matrix34 worldMatrix = GetEntity()->GetWorldTM();

		if(laserLength<=0.7f)
			hitPos = lamPos+(0.7f*dir);

		CPlayer* pPlayer = static_cast<CPlayer*>(parent->GetOwnerActor());
		SPlayerStats* pStats = pPlayer?static_cast<SPlayerStats*>(pPlayer->GetActorStats()):NULL;
		if(pStats && pStats->bLookingAtFriendlyAI)
		{
			hitPos = lamPos+(2.0f*dir);
			laserLength = 2.0f;
		}

		if(laserLength<=2.0f)
			dotScale *= min(1.0f,(0.35f + ((laserLength-0.7f)*0.5f)));

		IEntity* pDotEntity = m_pEntitySystem->GetEntity(m_pLaserEntityId);
		if(pDotEntity)
		{
			Matrix34 finalMatrix = Matrix34::CreateTranslationMat(hitPos-(0.2f*dir));
			pDotEntity->SetWorldTM(finalMatrix);
			Matrix34 localScale = Matrix34::CreateIdentity();
			localScale.SetScale(Vec3(dotScale,dotScale,dotScale));
			pDotEntity->SetSlotLocalTM(m_dotEffectSlot,localScale);
		}
	}

	if (m_laserActivated || m_lightActivated)
	{
		float laserAIRange = m_laserActivated ? laserLength : 0.0f;
		float lightAIRange = m_lightActivated ? min(laserLength, m_lamparams.light_range[eIGS_FirstPerson] * 1.5f) : 0.0f;
		UpdateAILightAndLaser(lamPos, dir, lightAIRange, m_lamparams.light_fov[eIGS_FirstPerson], laserAIRange);
	}

}

//------------------------------------------------------------------
void CLam::AdjustLaserFPDirection(CItem* parent, Vec3 &dir, Vec3 &pos)
{
	pos = parent->GetSlotHelperPos(eIGS_FirstPerson,m_laserHelperFP.c_str(),true);
	Quat   lamRot = Quat(parent->GetSlotHelperRotation(eIGS_FirstPerson,m_laserHelperFP.c_str(),true));
	dir = -lamRot.GetColumn0();

	if(!m_lamparams.isLamRifle)
		dir = lamRot.GetColumn1();

	CActor *pActor = parent->GetOwnerActor();
	IMovementController * pMC = pActor ? pActor->GetMovementController() : NULL;
	if (pMC)
	{ 
		SMovementState info;
		pMC->GetMovementState(info);


		CWeapon* pWep = static_cast<CWeapon*>(parent->GetIWeapon());
		if(pWep && (pWep->IsReloading() || (!pActor->CanFire() && !pWep->IsZoomed())))
			return;

		if(dir.Dot(info.fireDirection)<0.985f)
			return;
	
		CCamera& camera = gEnv->pSystem->GetViewCamera();
		pos = camera.GetPosition();
		dir = camera.GetMatrix().GetColumn1();
		dir.Normalize();
	}
}

//-------------------------------------------------------------------
void CLam::UpdateLaserScale(float scaleLenght,IEntity* pLaserEntity)
{
	if(pLaserEntity)
	{
		Matrix34 tm;
		tm.SetIdentity();
		tm.SetScale(Vec3(1.0f,scaleLenght,1.0f));
		pLaserEntity->SetLocalTM(tm);
	}
}

//------------------------------------------------------------------
void CLam::PickUp(EntityId pickerId, bool sound, bool select, bool keepHistory)
{
	if(gEnv->bServer && m_lamparams.giveExtraAccessory)
	{
		CActor *pActor=GetActor(pickerId);
		if (pActor && pActor->IsPlayer())
		{
			IInventory *pInventory=GetActorInventory(pActor);
			if (pInventory)
			{
				if (!m_lamparams.isLamRifle	&& !pInventory->GetItemByClass(CItem::sLAMFlashLight) && gEnv->bMultiplayer)
					m_pItemSystem->GiveItem(pActor, m_lamparams.extraAccessoryName.c_str(), false, false, false);
				else if(m_lamparams.isLamRifle	&& !pInventory->GetItemByClass(CItem::sLAMRifleFlashLight) && gEnv->bMultiplayer)
					m_pItemSystem->GiveItem(pActor, m_lamparams.extraAccessoryName.c_str(), false, false, false);
			}
		}
	}

	CItem::PickUp(pickerId,sound,select,keepHistory);
}

//------------------------------------------------------------------
void CLam::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->Add(m_laserHelperFP);
	m_lamparams.GetMemoryStatistics(s);
}

//------------------------------------------------------------------
void CLam::FullSerialize(TSerialize ser)
{
	CItem::FullSerialize(ser);

	if(ser.IsReading())
	{
		ActivateLight(false);
		ActivateLaser(false);
		m_lastLaserHitPt.Set(0,0,0);
		m_lastLaserHitSolid = false;
		m_smoothLaserLength = -1.0f;
		DestroyLaserEntity();
		m_laserHelperFP.clear();
		m_allowUpdate = false;
		m_updateTime = 0.0f;
	}

	m_laserActiveSerialize = m_laserActivated;
	ser.Value("laserActivated", m_laserActiveSerialize);
	m_lightActiveSerialize = m_lightActivated;
	ser.Value("lightActivated", m_lightActiveSerialize);
}

//------------------------------------------------------------------
void CLam::PostSerialize()
{
	/*if(m_laserActiveSerialize)
	{
		if(m_laserActivated)
			ActivateLaser(false);
		ActivateLaser(true);
	}
	if(m_lightActiveSerialize)
	{
		if(m_lightActivated)
			ActivateLight(false);
		ActivateLight(true);
	}*/
}

