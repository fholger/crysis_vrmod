/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  Claymore mine implementation
-------------------------------------------------------------------------
History:
- 07:2:2007   12:34 : Created by Steve Humphreys

*************************************************************************/

#include "StdAfx.h"
#include "Claymore.h"
#include "Game.h"
#include "GameCVars.h"
#include "GameRules.h"
#include "HUD/HUD.h"
#include "Item.h"
#include "Player.h"

#include "IEntityProxy.h"
#include "IRenderAuxGeom.h"

//------------------------------------------------------------------------
CClaymore::CClaymore()
: m_triggerDirection(0, 0, 0)
, m_triggerAngle(0.0f)
, m_triggerRadius(3.0f)
, m_timeToArm(0.0f)
, m_armed(false)
, m_teamId(0)
, m_frozen(false)
{
}

//------------------------------------------------------------------------
CClaymore::~CClaymore()
{
	if(gEnv->bMultiplayer && gEnv->bServer)
	{
		IActor *pOwner = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_ownerId);
		if(pOwner && pOwner->IsPlayer())
		{
			((CPlayer*)pOwner)->RecordExplosiveDestroyed(GetEntityId(), 0);
		}
	}

	if(g_pGame->GetHUD())
		g_pGame->GetHUD()->RecordExplosiveDestroyed(GetEntityId());
}

//------------------------------------------------------------------------
bool CClaymore::Init(IGameObject *pGameObject)
{
	bool ok = CProjectile::Init(pGameObject);

	if(g_pGame->GetHUD())
		g_pGame->GetHUD()->RecordExplosivePlaced(GetEntityId());

	// if not already a hit listener, register us.
	//	(removed in ~CProjectile )
	if(!m_hitListener)
	{
		g_pGame->GetGameRules()->AddHitListener(this);
		m_hitListener = true;
	}

	return ok;
}

//------------------------------------------------------------------------

void CClaymore::Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale)
{
	Vec3 newDir = dir;
	m_triggerRadius = GetParam("trigger_radius", m_triggerRadius);
	m_triggerAngle = DEG2RAD(GetParam("trigger_angle", m_triggerAngle));
	m_timeToArm = GetParam("arm_delay", m_timeToArm);	
	newDir.z = 0.0f;
	m_triggerDirection = newDir;
	m_triggerDirection.Normalize();
	m_armed = false;

	CProjectile::Launch(pos, newDir, velocity, speedScale);

	if(gEnv->bMultiplayer && gEnv->bServer)
	{
		CActor* pOwner = GetWeapon()->GetOwnerActor();
		if(pOwner && pOwner->IsPlayer())
		{
			((CPlayer*)pOwner)->RecordExplosivePlaced(GetEntityId(), 0);
		}
	}
}

void CClaymore::HandleEvent(const SGameObjectEvent &event)
{
	CProjectile::HandleEvent(event);

	if (event.event==eCGE_PostFreeze)
		m_frozen=event.param!=0;
}

void CClaymore::ProcessEvent(SEntityEvent &event)
{
	if (m_frozen)
		return;

	switch(event.event)
	{
		case ENTITY_EVENT_ENTERAREA:
		{
			IEntity * pEntity = gEnv->pEntitySystem->GetEntity(event.nParam[0]);
			IVehicle* pVehicle = NULL;
			IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(event.nParam[0]);	// only detonate for actors...
			if(!pActor)
				pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(event.nParam[0]);	// ...or vehicles
			
			// ignore actors in vehicles (we detect the vehicle instead)
			if(pActor && pActor->GetLinkedVehicle())
				pActor = NULL;

			// ignore spectators
			if(pActor && static_cast<CActor*>(pActor)->GetSpectatorMode() != CActor::eASM_None)
				pActor = NULL;

			if(pEntity && (pActor || pVehicle))
			{
				if(g_pGameCVars->g_debugMines != 0)
					CryLog("Claymore detected entity: %d, type %s", pEntity->GetId(), pActor == NULL ? "Vehicle" : "Actor");
					
				m_targetList.push_back(pEntity->GetId());
			}
			break;
		}

		case ENTITY_EVENT_LEAVEAREA:
		{
			IEntity * pEntity = gEnv->pEntitySystem->GetEntity(event.nParam[0]);
			if(pEntity)
			{
				std::list<EntityId>::iterator it = std::find(m_targetList.begin(), m_targetList.end(), pEntity->GetId());
				if(it != m_targetList.end())
					m_targetList.erase(it);
			}
			break;
		}

		default:
			break;
	}

	return CProjectile::ProcessEvent(event);
}

void CClaymore::Update(SEntityUpdateContext &ctx, int updateSlot)
{
	CProjectile::Update(ctx, updateSlot);

	bool debug = (g_pGameCVars->g_debugMines != 0);

	if(gEnv->bServer)
	{
		if(m_armed)
		{
			CGameRules* pGR = g_pGame->GetGameRules();
			if(pGR)
			{
				for(std::list<EntityId>::iterator it = m_targetList.begin(); it != m_targetList.end(); ++it)
				{
					IEntity* pEntity = gEnv->pEntitySystem->GetEntity(*it);
					if(!pEntity) continue;
						
					// if this is a team game, claymores aren't set off by their own team...
					if(pGR->GetTeamCount() > 0 && (m_teamId != 0 && pGR->GetTeam(pEntity->GetId()) == m_teamId))
						continue;

					// otherwise, not set off by the player who dropped them.
					if(pGR->GetTeamCount() == 0 && m_ownerId == pEntity->GetId())
						continue;
					
					IPhysicalEntity *pPhysics = pEntity->GetPhysics();
					if(pPhysics)
					{
						pe_status_dynamics physStatus;
						if(0 != pPhysics->GetStatus(&physStatus) && physStatus.v.GetLengthSquared() > 0.01f)
						{
							// now check angle between this claymore and approaching object
							//	to see if it is within the angular range m_triggerAngle.
							//	If it is, then check distance is less than m_triggerRange,
							//	and also check line-of-sight between the two entities.
							IRenderAuxGeom * pRAG = gEnv->pRenderer->GetIRenderAuxGeom();
							pRAG->SetRenderFlags( e_Mode3D | e_AlphaBlended | e_DrawInFrontOff | e_FillModeSolid | e_CullModeNone );

							AABB entityBBox;	
							pEntity->GetWorldBounds(entityBBox);

							if(debug)
							{
								pRAG->DrawAABB( entityBBox, true, ColorF(1,0,0,0.4f), eBBD_Faceted );
							}

							Vec3 enemyDir = entityBBox.GetCenter() - GetEntity()->GetPos();
							Vec3 checkDir = enemyDir; 
							checkDir.z = 0;
							float distanceSq = enemyDir.GetLengthSquared();

							// for players a simple distance check is fine, but for vehicles use a better intersection check
							//	so any corner of the vehicle going inside the zone sets off the claymore.
							static float playerRadius = 2.5f;
							bool inside = false;
							if(entityBBox.GetRadius() < playerRadius)
							{
								inside = (distanceSq < (m_triggerRadius * m_triggerRadius));
							}
							else
							{							
								static ray_hit hit;
								if(gEnv->pPhysicalWorld->CollideEntityWithBeam(pEntity->GetPhysics(), GetEntity()->GetWorldPos(), enemyDir, m_triggerRadius, &hit))
								{
									inside = true;
									enemyDir = hit.pt - GetEntity()->GetWorldPos();
								}
							}

							if(inside)
							{
								enemyDir.NormalizeSafe();
								checkDir.NormalizeSafe();
								float dotProd = checkDir.Dot(m_triggerDirection);

								if(debug)
								{
									pRAG->DrawLine(GetEntity()->GetPos(), ColorF(1,0,0,1), GetEntity()->GetPos() + Matrix33::CreateRotationZ(m_triggerAngle/2.0f)*m_triggerDirection*m_triggerRadius, ColorF(1,0,0,1), 5.0f);
									pRAG->DrawLine(GetEntity()->GetPos(), ColorF(1,0,0,1), GetEntity()->GetPos() + Matrix33::CreateRotationZ(-m_triggerAngle/2.0f)*m_triggerDirection*m_triggerRadius, ColorF(1,0,0,1), 5.0f);

									ColorF clr;
									clr.a = 0.3f;
									clr.b = 0.4f;
									clr.g = 0.1f;
									clr.r = 1.0f;
									pRAG->DrawLine(GetEntity()->GetPos(), clr, GetEntity()->GetPos() + (enemyDir * m_triggerRadius), clr, 5.0f);
								}

								if(dotProd > cry_cosf(m_triggerAngle/2.0f))
								{
									static const int objTypes = ent_all&(~ent_terrain);   
									static const unsigned int flags = rwi_stop_at_pierceable|rwi_colltype_any;
									ray_hit hit;
									int col = gEnv->pPhysicalWorld->RayWorldIntersection(GetEntity()->GetPos(), (enemyDir * m_triggerRadius * 1.5f), objTypes, flags, &hit, 1, GetEntity()->GetPhysics());

									bool bang = false;
									if (!col)
										bang = true;
									else if (entityBBox.IsContainPoint(hit.pt))
										bang = true;
									else if (hit.pt.GetSquaredDistance(GetEntity()->GetWorldPos()) >= distanceSq)
										bang = true;
									if (bang)
									{
										// pass in the explosion normal, which is -m_triggerDirection
										Explode(true, false, Vec3(0,0,0), -m_triggerDirection);

										if(debug)
										{
											ColorF clr;
											clr.a = 0.3f;
											clr.g = 0.1f;
											clr.r = 1.0f;
											clr.b = 1.0f;
											pRAG->DrawLine(GetEntity()->GetPos(), clr, GetEntity()->GetPos() + (enemyDir * m_triggerRadius), clr, 5.0f);
										}
									}
								}
							}
						}
					}
				}
			}
		}
		else
		{
			m_timeToArm -= gEnv->pTimer->GetFrameTime();
			if(m_timeToArm <= 0.0f)
			{
				m_armed = true;

				IEntityTriggerProxy *pTriggerProxy = (IEntityTriggerProxy*)(GetEntity()->GetProxy(ENTITY_PROXY_TRIGGER));

				if (!pTriggerProxy)
				{
					GetEntity()->CreateProxy(ENTITY_PROXY_TRIGGER);
					pTriggerProxy = (IEntityTriggerProxy*)GetEntity()->GetProxy(ENTITY_PROXY_TRIGGER);
				}

				if(pTriggerProxy)
				{
					// create a trigger volume a couple of metres bigger than we need, to ensure we catch vehicles.
					//	Checks above will still make sure the entity is within the radius before detonating though.
					float radius = m_triggerRadius + 2.0f;
					AABB boundingBox = AABB(Vec3(-radius,-radius,-radius), Vec3(radius,radius,radius));
					pTriggerProxy->SetTriggerBounds(boundingBox);
				}
			}
		}
	}

	if(debug && m_armed)
	{
		IRenderAuxGeom * pRAG = gEnv->pRenderer->GetIRenderAuxGeom();
		ColorF clr;
		clr.a = 0.3f;
		clr.b = 0.4f;
		clr.g = 0.1f;
		clr.r = 1.0f;
		pRAG->SetRenderFlags( e_Mode3D | e_AlphaBlended | e_DrawInFrontOff | e_FillModeSolid | e_CullModeNone );
		pRAG->DrawCylinder(GetEntity()->GetPos(), Vec3(0, 0, 1), m_triggerRadius, m_triggerRadius * 2.0f, clr);
		Vec3 size(m_triggerRadius + 2.0f, m_triggerRadius + 2.0f, m_triggerRadius + 2.0f);
		AABB box(GetEntity()->GetPos() - size, GetEntity()->GetPos() + size);
		pRAG->DrawAABB(box, false, ColorF(0.1f, 0.1f, 0.1f, 0.1f), eBBD_Faceted);
		pRAG->DrawLine(GetEntity()->GetPos(), clr, GetEntity()->GetPos() + m_triggerDirection, clr, 5.0f);
	}
}

void CClaymore::SetParams(EntityId ownerId, EntityId hostId, EntityId weaponId, int fmId, int damage, int hitTypeId)
{
	// if this is a team game, record which team placed this claymore...
	if(gEnv->bServer)
	{
		if(CGameRules* pGameRules = g_pGame->GetGameRules())
		{
			m_teamId = pGameRules->GetTeam(ownerId);
			pGameRules->SetTeam(m_teamId, GetEntityId());
		}
	}

	CProjectile::SetParams(ownerId, hostId, weaponId, fmId, damage, hitTypeId);
}

void CClaymore::Explode(bool destroy, bool impact, const Vec3 &pos, const Vec3 &normal, const Vec3 &vel, EntityId targetId)
{
	// overridden to ensure we always pass the correct blast direction...
	Vec3 newPos = GetEntity()->GetWorldPos();
	newPos.z += 0.2f;
	CProjectile::Explode(destroy, impact, newPos, -m_triggerDirection, vel, targetId);
}