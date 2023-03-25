/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 20:04:2006   13:02 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "VehicleWeapon.h"
#include "Actor.h"
#include <IViewSystem.h>
#include <IVehicleSystem.h>
#include "Single.h"
#include "HUD/HUD.h"
#include "HUD/HUDCrosshair.h"
#include "HUD/HUDRadar.h"
#include "HUD/HUDVehicleInterface.h"
#include "GameCVars.h"

//------------------------------------------------------------------------
CVehicleWeapon::CVehicleWeapon()
: m_pVehicle(0)
, m_pPart(0)
, m_timeToUpdate(0.0f)
, m_dtWaterLevelCheck(0.f)
, m_pOwnerSeat(NULL)
, m_pSeatUser(NULL)
, m_pLookAtEnemy(NULL)
{  
}

//------------------------------------------------------------------------
bool CVehicleWeapon::Init( IGameObject * pGameObject )
{
  if (!CWeapon::Init(pGameObject))
    return false;

	m_properties.mounted=true;

  return true;
}

//------------------------------------------------------------------------
void CVehicleWeapon::PostInit( IGameObject * pGameObject )
{
  CWeapon::PostInit(pGameObject); 
}

//------------------------------------------------------------------------
void CVehicleWeapon::Reset()
{
  CWeapon::Reset();
}

//------------------------------------------------------------------------
void CVehicleWeapon::MountAtEntity(EntityId entityId, const Vec3 &pos, const Ang3 &angles)
{
  CWeapon::MountAtEntity(entityId, pos, angles);  
}

//------------------------------------------------------------------------
void CVehicleWeapon::StartUse(EntityId userId)
{
	if (m_ownerId && userId != m_ownerId)
		return; 

  if (GetEntity()->GetParent())
  { 
    m_pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(GetEntity()->GetParent()->GetId());
    assert(m_pVehicle && "Using VehicleWeapons on non-vehicles may lead to unexpected behavior.");

    if (m_pVehicle)
    {   
      m_pPart = m_pVehicle->GetWeaponParentPart(GetEntityId()); 
      m_pOwnerSeat = m_pVehicle->GetWeaponParentSeat(GetEntityId());
      m_pSeatUser = m_pVehicle->GetSeatForPassenger(userId);
    }
  }
  
	SetOwnerId(userId);
  Select(true);	
	m_stats.used = true;

	EnableUpdate(true, eIUS_General);
	RequireUpdate(eIUS_General);

  if (OutOfAmmo(false))
    Reload(false);

  UseManualBlending(true);

	LowerWeapon(false);

	SendMusicLogicEvent(eMUSICLOGICEVENT_WEAPON_MOUNT);

}

//------------------------------------------------------------------------
void CVehicleWeapon::StopUse(EntityId userId)
{
	if (m_ownerId && userId != m_ownerId)
		return;

	SendMusicLogicEvent(eMUSICLOGICEVENT_WEAPON_UNMOUNT);
 
  Select(false);  	
  m_stats.used = false;	

  UseManualBlending(false);

  SetOwnerId(0);

  EnableUpdate(false);

	if(IsZoomed() || IsZooming())
	{
		ExitZoom();
		ExitViewmodes();
	}

	LowerWeapon(false);

}

//------------------------------------------------------------------------
void CVehicleWeapon::StartFire()
{
  if (!CheckWaterLevel())
    return;

	if(!CanFire())
		return;

  CWeapon::StartFire();
}

//------------------------------------------------------------------------
void CVehicleWeapon::Update( SEntityUpdateContext& ctx, int update)
{
  CWeapon::Update(ctx, update);

	if(update==eIUS_General)
  { 
    if (m_fm && m_fm->IsFiring())
    {
      m_dtWaterLevelCheck -= ctx.fFrameTime;      
      
      if (m_dtWaterLevelCheck <= 0.f)
      { 
        if (!CheckWaterLevel())        
          StopFire();          
        
        m_dtWaterLevelCheck = 2.0f;
      }
    }
    
		CheckForFriendlyAI(ctx.fFrameTime);
		CheckForFriendlyPlayers(ctx.fFrameTime);
  }
}

//------------------------------------------------------------------------
bool CVehicleWeapon::CheckWaterLevel() const
{
  // if not submerged at all, skip water level check
  if (m_pVehicle && m_pVehicle->GetStatus().submergedRatio < 0.01f)
    return true;
  
  if (gEnv->p3DEngine->IsUnderWater(GetEntity()->GetWorldPos()))
    return false;

  return true;
}

//------------------------------------------------------------------------
void CVehicleWeapon::SetAmmoCount(IEntityClass* pAmmoType, int count)
{ 
  IActor* pOwner = GetOwnerActor();
  
  if (pOwner && !pOwner->IsPlayer() && count < m_ammo[pAmmoType])
    return;
  
  CWeapon::SetAmmoCount(pAmmoType, count);    
}

//------------------------------------------------------------------------
void CVehicleWeapon::SetInventoryAmmoCount(IEntityClass* pAmmoType, int count)
{
  IActor* pOwner = GetOwnerActor();

  if (pOwner && !pOwner->IsPlayer() && m_pVehicle)
  {
    if (count < m_pVehicle->GetAmmoCount(pAmmoType))
      return;
  }

  CWeapon::SetInventoryAmmoCount(pAmmoType, count);
}

//------------------------------------------------------------------------
bool CVehicleWeapon::FilterView(SViewParams& viewParams)
{ 
  if (m_pOwnerSeat != m_pSeatUser)
    return false;

  if (m_camerastats.animating && !m_camerastats.helper.empty())
  { 
    viewParams.position = GetSlotHelperPos(eIGS_FirstPerson, m_camerastats.helper, true);
    viewParams.rotation = Quat(GetSlotHelperRotation(eIGS_FirstPerson, m_camerastats.helper, true));    
    viewParams.blend = false;
    
    if (g_pGameCVars->v_debugMountedWeapon)
    { 
      Vec3 local = GetSlotHelperPos(eIGS_FirstPerson, m_camerastats.helper, false, true);
      Vec3 entity = GetSlotHelperPos(eIGS_FirstPerson, m_camerastats.helper, false, false);
                 
      float color[] = {1,1,1,1};      
      gEnv->pRenderer->Draw2dLabel(50,400,1.3f,color,false,"cam_pos local(%.3f %.3f %.3f), entity(%.3f %.3f %.3f)", local.x, local.y, local.z, entity.x, entity.y, entity.z);
      
      Ang3 angLocal(GetSlotHelperRotation(eIGS_FirstPerson, m_camerastats.helper, false, true));
      Ang3 angEntity(GetSlotHelperRotation(eIGS_FirstPerson, m_camerastats.helper, false, true));      

      gEnv->pRenderer->Draw2dLabel(50,420,1.3f,color,false,"cam_rot local(%.3f %.3f %.3f), entity(%.3f %.3f %.3f)", angLocal.x, angLocal.y, angLocal.z, angEntity.x, angEntity.y, angEntity.z);
    }
        
    return true;
  }

  return false;
}

//------------------------------------------------------------------------
bool CVehicleWeapon::GetAimBlending(OldBlendSpace& params)
{   
  float anglemin=0.f, anglemax=0.f;
  if (m_pPart && m_pPart->GetRotationLimits(0, anglemin, anglemax))
  { 
    if (!(anglemin == 0.f && anglemax == 0.f)) // no limits
    {
      Ang3 angles( m_pPart->GetLocalTM(true) );

      float limit = isneg(angles.x) ? anglemin : anglemax;
      float ratio = (limit != 0.f) ? min(1.f, angles.x/limit) : 0.f;

      params.m_turn = sgn(angles.x) * ratio;
      
      return true;
    }
  }

  return false;
}

//---------------------------------------------------------------------------
void CVehicleWeapon::UpdateIKMounted(IActor* pActor, const Vec3& vGunXAxis)
{
  // only apply IK when the weapons user is in the weapons owner seat
  if (m_pSeatUser == m_pOwnerSeat)
    CWeapon::UpdateIKMounted(pActor,vGunXAxis);
}

//------------------------------------------------------------------------
void CVehicleWeapon::AttachArms(bool attach, bool shadow)
{
  if (attach && m_pSeatUser != m_pOwnerSeat)
    return;

  CWeapon::AttachArms(attach, shadow);
}

//------------------------------------------------------------------------
bool CVehicleWeapon::CanZoom() const
{
  if (!CWeapon::CanZoom())
    return false;

  if (m_pSeatUser != m_pOwnerSeat)
    return false;

  IActor* pActor = GetOwnerActor();
  if (pActor && pActor->IsThirdPerson())
    return false;

  return true;
}

//---------------------------------------------------------------------
void CVehicleWeapon::UpdateFPView(float frameTime)
{
	CItem::UpdateFPView(frameTime);

	if(GetOwnerId() && m_pVehicle && (m_pVehicle->GetCurrentWeaponId(GetOwnerId(), true) != GetEntityId())) //only update primary weapon
		UpdateCrosshair(frameTime);
	if (m_fm)
		m_fm->UpdateFPView(frameTime);
	if (m_zm)
		m_zm->UpdateFPView(frameTime);
}

//---------------------------------------------------------------------------
void CVehicleWeapon::CheckForFriendlyAI(float frameTime)
{
	CActor* pOwner = GetOwnerActor();

	if(pOwner && m_pVehicle && pOwner->IsPlayer() && !gEnv->bMultiplayer)
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pOwner);
		m_timeToUpdate-=frameTime;
		if(m_timeToUpdate>0.0f)
			return;

		m_timeToUpdate = 0.15f;


		if(IMovementController* pMC = pOwner->GetMovementController())
		{
			SMovementState info;
			pMC->GetMovementState(info);

			LowerWeapon(false);
			SAFE_HUD_FUNC(GetVehicleInterface()->DisplayFriendlyFire(false));

			//Try with boxes first
			bool success = false;			
			if(g_pGame->GetHUD() && g_pGame->GetHUD()->GetRadar())
			{
				const std::vector<EntityId> *entitiesInProximity = g_pGame->GetHUD()->GetRadar()->GetNearbyEntities();
				OBB aimOBB;
				Vec3 aimOBBHalfSize(0.1f, 100.0f, 0.2f);
				aimOBB.SetOBBfromAABB(Matrix33::CreateRotationVDir(info.aimDirection), AABB(-aimOBBHalfSize, aimOBBHalfSize));
				Vec3 aimOBBPos(info.weaponPosition+aimOBBHalfSize.y*info.aimDirection);

				for(int iEntity=0; iEntity<entitiesInProximity->size(); ++iEntity)
				{
					IEntity *pEntity = m_pEntitySystem->GetEntity((*entitiesInProximity)[iEntity]);

					// Check if NULL, this, or the vehicle itself
					if(!pEntity || pEntity==GetEntity() || pEntity==m_pVehicle->GetEntity())
						continue;

					// Check if this is a friendly passenger in our vehicle
					bool bPassenger = false;
					for(int iSeatId=1; iSeatId<=m_pVehicle->GetLastSeatId(); iSeatId++)
					{
						IVehicleSeat *pVehicleSeat = m_pVehicle->GetSeatById(iSeatId);
						if(pVehicleSeat && pVehicleSeat->GetPassenger())
						{
							if(pEntity->GetId() == pVehicleSeat->GetPassenger())
							{
								bPassenger = true;
								break;
							}
						}
					}
					if(bPassenger)
						continue;

					// Check if this is not a friendly entity
					if(!pEntity->GetAI() || pEntity->GetAI()->IsHostile(pOwner->GetEntity()->GetAI(),false))
						continue;

					IPhysicalEntity* pPhysEnt = pEntity->GetPhysics();
					if (!pPhysEnt)
						continue;

					pe_status_nparts snp; 
					pe_status_pos sp;
					for(sp.ipart=pPhysEnt->GetStatus(&snp)-1; sp.ipart>=0; sp.ipart--)
					{
						sp.partid=-1; pPhysEnt->GetStatus(&sp);

						//point in entity = sp.q*(pbbox.Basis.T()*point_in_box+pbbox.center)*sp.scale+sp.pos
						primitives::box pbbox;
						sp.pGeom->GetBBox(&pbbox);
						AABB bbox(-pbbox.size, pbbox.size);
						if (!bbox.IsEmpty())
						{
							Matrix34 geomToPart(pbbox.Basis.T(), pbbox.center);
							Matrix34 partToEnt(Matrix33(sp.q) * Matrix33::CreateScale(Vec3(sp.scale, sp.scale, sp.scale)), sp.pos);
							Matrix34 entToWorld = partToEnt * geomToPart;
							OBB obb(OBB::CreateOBBfromAABB(Matrix33(entToWorld ), bbox));
							if (Overlap::OBB_OBB(aimOBBPos, aimOBB, entToWorld.GetTranslation(), obb))
							{
								success = true;
								LowerWeapon(true);
								StopFire();//Just in case

								SAFE_HUD_FUNC(GetVehicleInterface()->DisplayFriendlyFire(true));
								m_pLookAtEnemy = NULL;
							}
						}
					}
				}
			}

			//Try ray hit
			if(!success)
			{
				ray_hit rayhit;
				IPhysicalEntity* pSkipEnts[10];
				int nSkip = CSingle::GetSkipEntities(this, pSkipEnts, 10);	

				int intersect = gEnv->pPhysicalWorld->RayWorldIntersection(info.weaponPosition, info.aimDirection * 150.0f, ent_all,
					rwi_stop_at_pierceable|rwi_colltype_any, &rayhit, 1, pSkipEnts, nSkip);

				IEntity* pLookAtEntity = NULL;

				if(intersect && rayhit.pCollider)
					pLookAtEntity = m_pEntitySystem->GetEntityFromPhysics(rayhit.pCollider);

				if(pLookAtEntity && pLookAtEntity->GetAI() && pOwner->GetEntity() && pLookAtEntity->GetId()!=GetEntityId())
				{
					bool bFriendlyVehicle = false;
					if( pLookAtEntity && pLookAtEntity->GetId() )
					{
						IVehicle *pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(pLookAtEntity->GetId());
						if ( pVehicle )
						{
							if ( pOwner->GetEntity() && pVehicle->HasFriendlyPassenger( pOwner->GetEntity() ) )
								bFriendlyVehicle = true;
						}				
					}

					if(bFriendlyVehicle||!pLookAtEntity->GetAI()->IsHostile(pOwner->GetEntity()->GetAI(),false))
					{
						LowerWeapon(true);
						StopFire();//Just in case

						SAFE_HUD_FUNC(GetVehicleInterface()->DisplayFriendlyFire(true));
						success = true;
						m_pLookAtEnemy = NULL;
					}	
					else if(g_pGame->GetHUD())
					{
						CHUDVehicleInterface::EVehicleHud hudType = g_pGame->GetHUD()->GetVehicleInterface()->GetHUDType();
						if(hudType == CHUDVehicleInterface::EHUD_TANKA || hudType == CHUDVehicleInterface::EHUD_TANKUS ||
							hudType == CHUDVehicleInterface::EHUD_AAA || hudType == CHUDVehicleInterface::EHUD_APC || hudType == CHUDVehicleInterface::EHUD_APC2)
						{
							if(pLookAtEntity != m_pLookAtEnemy)
							{
								bool isEnemy = false;
								if(gEnv->bMultiplayer)
									isEnemy = true;
								//else if(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pLookAtEntity->GetId()))
								//	isEnemy = true;
								else if(IVehicle *pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(pLookAtEntity->GetId()))
								{
									if(pVehicle->GetDriver())
										isEnemy = true;
								}

								if(isEnemy)
								{
									m_pLookAtEnemy = pLookAtEntity;
									SAFE_HUD_FUNC(PlaySound(ESound_Target_Lock));
									SAFE_HUD_FUNC(IndicateHit(true,m_pLookAtEnemy));
								}
							}
						}
					}
				}
				else if(pLookAtEntity)
				{
					//Special case (Animated objects), check for script table value "bFriendly"
					SmartScriptTable props;
					IScriptTable* pScriptTable = pLookAtEntity->GetScriptTable();
					if(!pScriptTable || !pScriptTable->GetValue("Properties", props))
						return;

					int isFriendly = 0;
					if(props->GetValue("bNoFriendlyFire", isFriendly) && isFriendly!=0)
					{
						LowerWeapon(true);
						StopFire();//Just in case

						SAFE_HUD_FUNC(GetVehicleInterface()->DisplayFriendlyFire(true));
						success = true;
						m_pLookAtEnemy = NULL;
					}
					
				}
				else if(!pLookAtEntity)
					m_pLookAtEnemy = NULL;
			}

		}
	}		
}

//---------------------------------------------------------------------------
void CVehicleWeapon::CheckForFriendlyPlayers(float frameTime)
{
	CActor* pOwner = GetOwnerActor();

	if(pOwner && pOwner->IsPlayer() && gEnv->bMultiplayer)
	{
		m_timeToUpdate-=frameTime;
		if(m_timeToUpdate>0.0f)
			return;

		m_timeToUpdate = 0.15f;

		if(IMovementController* pMC = pOwner->GetMovementController())
		{
			SMovementState info;
			pMC->GetMovementState(info);

			ray_hit rayhit;
			IPhysicalEntity* pSkipEnts[10];
			int nSkip = CSingle::GetSkipEntities(this, pSkipEnts, 10);	

			int intersect = gEnv->pPhysicalWorld->RayWorldIntersection(info.weaponPosition, info.aimDirection * 150.0f, ent_all,
				rwi_stop_at_pierceable|rwi_colltype_any, &rayhit, 1, pSkipEnts, nSkip);

			IEntity* pLookAtEntity = NULL;

			if(intersect && rayhit.pCollider)
				pLookAtEntity = m_pEntitySystem->GetEntityFromPhysics(rayhit.pCollider);

			bool bFriendly = SAFE_HUD_FUNC_RET(GetCrosshair()->IsFriendlyEntity(pLookAtEntity));
			SAFE_HUD_FUNC(GetVehicleInterface()->DisplayFriendlyFire(bFriendly));
		}
	}		
}
