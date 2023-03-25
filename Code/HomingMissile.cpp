/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 08:12:2005   14:14 : Created by MichaelR (port from Marcios HomingMissile.lua)

*************************************************************************/
#include "StdAfx.h"
#include "HomingMissile.h"
#include "Actor.h"
#include "Game.h"
#include "GameCVars.h"
#include "WeaponSystem.h"
#include "Single.h"
#include "NetInputChainDebug.h"
#include <IVehicleSystem.h>
#include <IViewSystem.h>

#define HM_TIME_TO_UPDATE 0.0f

//------------------------------------------------------------------------
CHomingMissile::CHomingMissile()
{
  m_isCruising = false;
  m_isDescending = false;
  m_controlled = false;
	m_autoControlled = false;
	m_cruise = true;
  m_destination.zero();
  m_targetId = 0;
	m_maxTargetDistance = 200.0f;		//Default
	m_lazyness = 0.35f;
	m_controlledTimer=0.15f;
	m_lockedTimer=0.2f;
	m_detonationRadius = 0.0f;
}

//------------------------------------------------------------------------
CHomingMissile::~CHomingMissile()
{

}

//------------------------------------------------------------------------
bool CHomingMissile::Init(IGameObject *pGameObject)
{
	if (CRocket::Init(pGameObject))
	{
		m_cruiseAltitude = GetParam("cruise_altitude", m_cruiseAltitude);

		m_accel = GetParam("accel", m_accel);
		m_turnSpeed = GetParam("turn_speed", m_turnSpeed);
		m_maxSpeed = GetParam("max_speed", m_maxSpeed);
		m_alignAltitude = GetParam("align_altitude", m_alignAltitude);
		m_descendDistance = GetParam("descend_distance", m_descendDistance);
		m_maxTargetDistance = GetParam("max_target_distance", m_maxTargetDistance);
		m_cruise = GetParam("cruise", m_cruise);
		m_controlled = GetParam("controlled", m_controlled);
		m_autoControlled = GetParam("autoControlled",m_autoControlled);
		m_lazyness = GetParam("lazyness",m_lazyness);
		m_lockedTimer = GetParam("initial_delay",m_lockedTimer);
		m_detonationRadius = GetParam("detonation_radius",m_detonationRadius);

		return true;
	}

	return false;
}

//------------------------------------------------------------------------
void CHomingMissile::Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale)
{
  CRocket::Launch(pos, dir, velocity, speedScale);

	if (m_controlled)
	{
		Vec3 dest(pos+dir*1000.0f);

		SetDestination(dest);
	}
}

//------------------------------------------------------------------------
void CHomingMissile::Update(SEntityUpdateContext &ctx, int updateSlot)
{

	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

  CRocket::Update(ctx, updateSlot);

  // update destination if required
  if (!m_cruise)
		UpdateControlledMissile(ctx.fFrameTime);
  else 
		UpdateCruiseMissile(ctx.fFrameTime);
}

//-----------------------------------------------------------------------------
void CHomingMissile::UpdateControlledMissile(float frameTime)
{
	bool isServer = gEnv->bServer;
	bool isClient = gEnv->bClient;

	CActor *pClientActor=0;
	if (gEnv->bClient)
		pClientActor=static_cast<CActor *>(g_pGame->GetIGameFramework()->GetClientActor());
	bool isOwner = ((!m_ownerId && isServer) || (isClient && pClientActor && (pClientActor->GetEntityId() == m_ownerId) && pClientActor->IsPlayer()));

	IRenderer* pRenderer = gEnv->pRenderer;
	IRenderAuxGeom* pGeom = pRenderer->GetIRenderAuxGeom();
	float color[4] = {1,1,1,1};
	const static float step = 15.f;  
	float y = 20.f;    

	bool bDebug = g_pGameCVars->i_debug_projectiles > 0;

	if (isOwner || isServer)
	{
		//If there's a target, follow the target
		if(isServer)
		{
			if (m_targetId)
			{
				if (m_lockedTimer>0.0f)
					m_lockedTimer=m_lockedTimer-frameTime;
				else
				{
					// If we are here, there's a target
					IEntity* pTarget = gEnv->pEntitySystem->GetEntity(m_targetId);
					if (pTarget)
					{
						AABB box;
						pTarget->GetWorldBounds(box);
						Vec3 finalDes = box.GetCenter();
						SetDestination(finalDes);
						//SetDestination( box.GetCenter() );

						if (bDebug)
							pRenderer->Draw2dLabel(5.0f, y+=step, 1.5f, color, false, "Target Entity: %s", pTarget->GetName());
					}

					m_lockedTimer+=0.05f;
				}
			}
			else if(m_autoControlled)
				return;
		} 

		if (m_controlled && !m_autoControlled && isOwner && !m_targetId)
		{
			//Check if the weapon is still selected
			CWeapon *pWeapon = GetWeapon();

			if(!pWeapon || !pWeapon->IsSelected())
				return;

			if (m_controlledTimer>0.0f)
				m_controlledTimer=m_controlledTimer-frameTime;
			else if (pClientActor && pClientActor->IsPlayer()) 	//Follow the crosshair
			{
				if (IMovementController *pMC=pClientActor->GetMovementController())
				{
					Vec3 eyePos(ZERO);
					Vec3 eyeDir(ZERO);

					IVehicle* pVehicle = pClientActor->GetLinkedVehicle();
					if(!pVehicle)
					{
						SMovementState state;
						pMC->GetMovementState(state);

						eyePos = state.eyePosition;
						eyeDir = state.eyeDirection;
					}
					else
					{	
						SViewParams viewParams;
						pVehicle->UpdateView(viewParams, pClientActor->GetEntityId());

						eyePos = viewParams.position;
						eyeDir = viewParams.rotation * Vec3(0,1,0);
						//eyeDir = (viewParams.targetPos - viewParams.position).GetNormalizedSafe();
					}

					int pierceability=7;

					if (IPhysicalEntity *pPE=GetEntity()->GetPhysics())
					{
						if (pPE->GetType()==PE_PARTICLE)
						{
							pe_params_particle pp;

							if (pPE->GetParams(&pp))
								pierceability=pp.iPierceability;
						}
					}

					static const int objTypes = ent_all;
					static const int flags = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (pierceability & rwi_pierceability_mask) | (geom_colltype14 << rwi_colltype_bit);

					IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;
					static IPhysicalEntity* pSkipEnts[10];
					int numSkip = CSingle::GetSkipEntities(pWeapon, pSkipEnts, 10);

					ray_hit hit;
					int hits = 0;

					float range=m_maxTargetDistance;
					hits = pWorld->RayWorldIntersection(eyePos + 1.5f*eyeDir, eyeDir*range, objTypes, flags, &hit, 1, pSkipEnts, numSkip);
					
					while (hits)
					{
						if (gEnv->p3DEngine->RefineRayHit(&hit, eyeDir*range))
							break;

						eyePos = hit.pt+eyeDir*0.003f;
						range -= hit.dist+0.003f;

						hits = pWorld->RayWorldIntersection(eyePos, eyeDir*range, objTypes, flags, &hit, 1, pSkipEnts, numSkip);
					}

					DestinationParams params;

					if(hits)
						params.pt=hit.pt;
					else
						params.pt=(eyePos+m_maxTargetDistance*eyeDir);	//Some point in the sky...

					GetGameObject()->InvokeRMI(SvRequestDestination(), params, eRMI_ToServer);

					if (bDebug)
					{
						pRenderer->Draw2dLabel(5.0f, y+=step, 1.5f, color, false, "PlayerView eye direction: %.3f %.3f %.3f", eyeDir.x, eyeDir.y, eyeDir.z);
						pRenderer->Draw2dLabel(5.0f, y+=step, 1.5f, color, false, "PlayerView Target: %.3f %.3f %.3f", hit.pt.x, hit.pt.y, hit.pt.z);
						pRenderer->GetIRenderAuxGeom()->DrawCone(m_destination, Vec3(0,0,-1), 2.5f, 7.f, ColorB(255,0,0,255));
					}
				}

				m_controlledTimer+=0.0f;
			}
		}
	}

	//This code is shared by both modes above (auto and controlled)
	if(!m_destination.IsZero())
	{
		pe_status_dynamics status;
		if (!GetEntity()->GetPhysics()->GetStatus(&status))
		{
			CryLogAlways("couldn't get physics status!");
			return;
		}

		pe_status_pos pos;
		if (!GetEntity()->GetPhysics()->GetStatus(&pos))
		{
			CryLogAlways("couldn't get physics pos!");
			return;
		}

		float currentSpeed = status.v.len();

		if (currentSpeed>0.001f)
		{
			Vec3 currentVel = status.v;
			Vec3 currentPos = pos.pos;
			Vec3 goalDir(ZERO);

			assert(!_isnan(currentSpeed));
			assert(!_isnan(currentVel.x) && !_isnan(currentVel.y) && !_isnan(currentVel.z));

			//Just a security check
			if((currentPos-m_destination).len2()<(m_detonationRadius*m_detonationRadius))
			{
				Explode(true, true, m_destination, -currentVel.normalized(), currentVel, m_targetId);

				return;
			}

			goalDir = m_destination - currentPos;
			goalDir.Normalize();

			//Turn more slowly...
			currentVel.Normalize();

			if(bDebug)
			{

				pRenderer->Draw2dLabel(50,55,2.0f,color,false, "  Destination: %.3f, %.3f, %.3f",m_destination.x,m_destination.y,m_destination.z);
				pRenderer->Draw2dLabel(50,80,2.0f,color,false, "  Current Dir: %.3f, %.3f, %.3f",currentVel.x,currentVel.y,currentVel.z);
				pRenderer->Draw2dLabel(50,105,2.0f,color,false,"  Goal    Dir: %.3f, %.3f, %.3f",goalDir.x,goalDir.y,goalDir.z);
			}

			float cosine = currentVel.Dot(goalDir);
			cosine = CLAMP(cosine,-1.0f,1.0f);
			float totalAngle = RAD2DEG(cry_acosf(cosine));

			assert(totalAngle>=0);

			if (cosine<0.99)
			{
				float maxAngle = m_turnSpeed*frameTime;
				if (maxAngle>totalAngle)
					maxAngle=totalAngle;
				float t=(maxAngle/totalAngle)*m_lazyness;

				assert(t>=0.0 && t<=1.0);

				goalDir = Vec3::CreateSlerp(currentVel, goalDir, t);
				goalDir.Normalize();
			}

			if(bDebug)
				pRenderer->Draw2dLabel(50,180,2.0f,color,false,"Corrected Dir: %.3f, %.3f, %.3f",goalDir.x,goalDir.y,goalDir.z);

			pe_action_set_velocity action;
			action.v = goalDir * currentSpeed;
			GetEntity()->GetPhysics()->Action(&action);
		}
	}
}

//----------------------------------------------------------------------------
void CHomingMissile::UpdateCruiseMissile(float frameTime)
{

	IRenderer* pRenderer = gEnv->pRenderer;
	IRenderAuxGeom* pGeom = pRenderer->GetIRenderAuxGeom();
	float color[4] = {1,1,1,1};
	const static float step = 15.f;  
	float y = 20.f;    

	bool bDebug = g_pGameCVars->i_debug_projectiles > 0;

	if (m_targetId)
	{
		IEntity* pTarget = gEnv->pEntitySystem->GetEntity(m_targetId);
		if (pTarget)
		{
			AABB box;
			pTarget->GetWorldBounds(box);
			SetDestination( box.GetCenter() );

			//if (bDebug)
				//pRenderer->Draw2dLabel(5.0f, y+=step, 1.5f, color, false, "Target Entity: %s", pTarget->GetName());
		}    
	}
	else 
	{
		// update destination pos from weapon
		static IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
		IItem* pItem = pItemSystem->GetItem(m_weaponId);
		if (pItem && pItem->GetIWeapon())
		{
			const Vec3& dest = pItem->GetIWeapon()->GetDestination();
			SetDestination( dest );

			//if (bDebug)
				//pRenderer->Draw2dLabel(5.0f, y+=step, 1.5f, color, false, "Weapon Destination: (%.1f %.1f %.1f)", dest.x, dest.y, dest.z);
		}
	}

	pe_status_dynamics status;
	if (!GetEntity()->GetPhysics()->GetStatus(&status))
		return;

	float currentSpeed = status.v.len();
	Vec3 currentPos = GetEntity()->GetWorldPos();
	Vec3 goalDir(ZERO);

	if (!m_destination.IsZero())
	{

		if((currentPos-m_destination).len2()<(m_detonationRadius*m_detonationRadius))
		{
			Explode(true, true, m_destination, -status.v.normalized(), status.v, m_targetId);
			return;
		}

		if (bDebug)
			pGeom->DrawCone(m_destination, Vec3(0,0,-1), 2.5f, 7.f, ColorB(255,0,0,255));

		float heightDiff = (m_cruiseAltitude-m_alignAltitude) - currentPos.z;

		if (!m_isCruising && heightDiff * sgn(status.v.z) > 0.f)
		{
			// if heading towards align altitude (but not yet reached) accelerate to max speed    
			if (bDebug)
				pRenderer->Draw2dLabel(5.0f,  y+=step,   1.5f, color, false, "[HomingMissile] accelerating (%.1f / %.1f)", currentSpeed, m_maxSpeed);    
		}
		else if (!m_isCruising && heightDiff * sgnnz(status.v.z) < 0.f && (status.v.z<0 || status.v.z>0.25f))
		{
			// align to cruise
			if (currentSpeed != 0)
			{
				goalDir = status.v;
				goalDir.z = 0;
				goalDir.normalize();
			}    

			if (bDebug)
				pRenderer->Draw2dLabel(5.0f,  y+=step, 1.5f, color, false, "[HomingMissile] aligning"); 
		}
		else
		{
			if (bDebug)
				pRenderer->Draw2dLabel(5.0f,  y+=step, 1.5f, color, false, "[HomingMissile] cruising..."); 

			// cruise
			m_isCruising = true;

			if (!m_destination.IsZero())
			{
				float groundDistSq = m_destination.GetSquaredDistance2D(currentPos);
				float distSq = m_destination.GetSquaredDistance(currentPos);
				float descendDistSq = sqr(m_descendDistance);

				if (m_isDescending || groundDistSq <= descendDistSq)
				{
					if (bDebug)
						pRenderer->Draw2dLabel(5.0f,  y+=step, 1.5f, color, false, "[HomingMissile] descending!"); 

					if (distSq != 0)
						goalDir = (m_destination - currentPos).normalized();
					else 
						goalDir.zero();

					m_isDescending = true;
				}              
				else
				{
					Vec3 airPos = m_destination;
					airPos.z = currentPos.z;          
					goalDir = airPos - currentPos;
					if (goalDir.len2() != 0)
						goalDir.Normalize();
				}    
			}
		}
	}  

	float desiredSpeed = currentSpeed;
	if (currentSpeed < m_maxSpeed-0.1f)
	{
		desiredSpeed = min(m_maxSpeed, desiredSpeed + m_accel*frameTime);
	}

	Vec3 currentDir = status.v.GetNormalizedSafe(FORWARD_DIRECTION);
	Vec3 dir = currentDir;

	if (!goalDir.IsZero())
	{ 
		float cosine = max(min(currentDir.Dot(goalDir), 0.999f), -0.999f);
		float goalAngle = RAD2DEG(acos_tpl(cosine));
		float maxAngle = m_turnSpeed * frameTime;

		if (bDebug)
		{ 
			pGeom->DrawCone( currentPos, goalDir, 0.4f, 12.f, ColorB(255,0,0,255) );
			pRenderer->Draw2dLabel(5.0f,  y+=step, 1.5f, color, false, "[HomingMissile] goalAngle: %.2f", goalAngle); 

		}

		if (goalAngle > maxAngle+0.05f)    
			dir = (Vec3::CreateSlerp(currentDir, goalDir, maxAngle/goalAngle)).normalize();
		else //if (goalAngle < 0.005f)
			dir = goalDir;
	}

	pe_action_set_velocity action;
	action.v = dir * desiredSpeed;
	GetEntity()->GetPhysics()->Action(&action);

	if (bDebug)
	{
		pGeom->DrawCone( currentPos, dir, 0.4f, 12.f, ColorB(128,128,0,255) );  
		pRenderer->Draw2dLabel(5.0f,  y+=step, 1.5f, color, false, "[HomingMissile] currentSpeed: %.1f (max: %.1f)", currentSpeed, m_maxSpeed); 
	}
}
//-------------------------------------------------------------------------------
void CHomingMissile::FullSerialize(TSerialize ser)
{
	CRocket::FullSerialize(ser);
	SerializeDestination(ser);
}

void CHomingMissile::SerializeDestination( TSerialize ser )
{
	bool gotdestination=!m_destination.IsZero();
	if (ser.BeginOptionalGroup("gotdestination", gotdestination))
	{
		ser.Value("destination", m_destination, 'wrl3');
		ser.EndGroup();
	}
}

bool CHomingMissile::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if (aspect == eEA_GameServerDynamic)
		SerializeDestination(ser);
	return CRocket::NetSerialize(ser, aspect, profile, flags);
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CHomingMissile, SvRequestDestination)
{
	SetDestination(params.pt);

	return true;
}
