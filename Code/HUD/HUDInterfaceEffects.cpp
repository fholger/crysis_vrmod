// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "HUD.h"
#include "HUDRadar.h"
#include "HUDSilhouettes.h"
#include "GameFlashAnimation.h"
#include "IRenderer.h"
#include "../Actor.h"
#include "WeaponSystem.h"
#include "Weapon.h"
#include "Claymore.h"
#include "../GameRules.h"
#include <IMaterialEffects.h>
#include "Game.h"
#include "GameCVars.h"
#include "IMusicSystem.h"
#include "INetwork.h"
#include "HUDVehicleInterface.h"
#include "Player.h"
#include "PlayerInput.h"
#include "IMovieSystem.h"
#include "HUDScopes.h"
#include "HUDCrosshair.h"
#include "OffHand.h"
#include "GameActions.h"

void CHUD::QuickMenuSnapToMode(ENanoMode mode)
{
	switch(mode)
	{
	case NANOMODE_SPEED:
		m_fAutosnapCursorRelativeX = 0.0f;
		m_fAutosnapCursorRelativeY = -30.0f;
		break;
	case NANOMODE_STRENGTH:
		m_fAutosnapCursorRelativeX = 30.0f;
		m_fAutosnapCursorRelativeY = 0.0f;
		break;
	case NANOMODE_DEFENSE:
		m_fAutosnapCursorRelativeX = -30.0f;
		m_fAutosnapCursorRelativeY = 0.0f;
		break;
	case NANOMODE_CLOAK:
		m_fAutosnapCursorRelativeX = 20.0f;
		m_fAutosnapCursorRelativeY = 30.0f;
		break;
	default:
		break;
	}
}


void CHUD::AutoSnap()
{
	const float fRadius = 25.0f;
	static Vec2 s_vCursor = Vec2(0,0);
	if(fabsf(m_fAutosnapCursorControllerX)>0.1 || fabsf(m_fAutosnapCursorControllerY)>0.1)
	{
		s_vCursor.x = m_fAutosnapCursorControllerX * 30.0f;
		s_vCursor.y = m_fAutosnapCursorControllerY * 30.0f;
	}
	else
	{
		s_vCursor.x = m_fAutosnapCursorRelativeX;
		s_vCursor.y = m_fAutosnapCursorRelativeY;
	}
	if(m_bOnCircle && s_vCursor.GetLength() < fRadius*0.5f)
	{
		m_fAutosnapCursorRelativeX = 0;
		m_fAutosnapCursorRelativeY = 0;
		m_bOnCircle = false;
	}
	if(s_vCursor.GetLength() > fRadius)
	{
		s_vCursor.NormalizeSafe();
		m_fAutosnapCursorRelativeX = s_vCursor.x*fRadius;
		m_fAutosnapCursorRelativeY = s_vCursor.y*fRadius;
		m_bOnCircle = true;
	}


	const char* autosnapItem = "Center";

	if(m_bOnCircle)
	{
		Vec2 vCursor = s_vCursor;
		vCursor.NormalizeSafe();

		float fAngle;
		if(vCursor.y < 0)
		{
			fAngle = RAD2DEG(acos_tpl(vCursor.x));
		}
		else
		{
			fAngle = RAD2DEG(gf_PI2-acos_tpl(vCursor.x));
		}

		char szAngle[32];
		sprintf(szAngle,"%f",-fAngle+90.0f);

/*
		ColorB col(255,255,255,255);
		int iW=m_pRenderer->GetWidth();
		int iH=m_pRenderer->GetHeight();
		m_pRenderer->Set2DMode(true,iW,iH);
		m_pRenderer->GetIRenderAuxGeom()->DrawLine(Vec3(iW/2,iH/2,0),col,Vec3(iW/2+vCursor.x*100,iH/2+vCursor.y*100,0),col,5);
		m_pRenderer->Set2DMode(false,0,0);
*/

		m_animQuickMenu.CheckedSetVariable("Root.QuickMenu.Circle.Indicator._rotation",szAngle);

		if(fAngle >= 342 || fAngle < 52)
		{
			autosnapItem = "Strength";
		}
		else if(fAngle >= 52 && fAngle < 128)
		{
			autosnapItem = "Speed";
		}
		else if(fAngle >= 128 && fAngle < 205)
		{
			autosnapItem = "Defense";
		}
		else if(fAngle >= 205 && fAngle < 260)
		{
			autosnapItem = "Weapon";
		}
		else if(fAngle >= 260 && fAngle < 342)
		{
			autosnapItem = "Cloak";
		}
	}

	m_animQuickMenu.CheckedInvoke("Root.QuickMenu.setAutosnapItem", autosnapItem);
}

void CHUD::UpdateMissionObjectiveIcon(EntityId objective, int friendly, FlashOnScreenIcon iconType, bool forceNoOffset, Vec3 rotationTarget)
{
	IEntity *pObjectiveEntity = GetISystem()->GetIEntitySystem()->GetEntity(objective);
	if(!pObjectiveEntity) return;

	AABB box;
	pObjectiveEntity->GetWorldBounds(box);
	Vec3 vWorldPos = Vec3(0,0,0);

	SEntitySlotInfo info;
	int slotCount = pObjectiveEntity->GetSlotCount();
	for(int i=0; i<slotCount; ++i)
	{
		if (pObjectiveEntity->GetSlotInfo(i, info))
		{
			if (info.pCharacter)
			{
				int16 id = info.pCharacter->GetISkeletonPose()->GetJointIDByName("objectiveicon");
				if (id >= 0)
				{
					//vPos = pCharacter->GetISkeleton()->GetHelperPos(helper);
					vWorldPos = info.pCharacter->GetISkeletonPose()->GetAbsJointByID(id).t;
					if (!vWorldPos.IsZero())
					{
						vWorldPos = pObjectiveEntity->GetSlotWorldTM(i).TransformPoint(vWorldPos);
						break;
					}
				}
			}
		}
	}
	if(vWorldPos == Vec3(0,0,0))
		vWorldPos = pObjectiveEntity->GetWorldPos();

	if(!forceNoOffset)
		vWorldPos.z += 2.0f;

	Vec3 vEntityScreenSpace;
	m_pRenderer->ProjectToScreen(	vWorldPos.x, vWorldPos.y,	vWorldPos.z, &vEntityScreenSpace.x, &vEntityScreenSpace.y, &vEntityScreenSpace.z);
	Vec3 vEntityTargetSpace;
	bool useTarget = false;
	if(!rotationTarget.IsZero())
	{
		m_pRenderer->ProjectToScreen(	rotationTarget.x, rotationTarget.y,	rotationTarget.z, &vEntityTargetSpace.x, &vEntityTargetSpace.y, &vEntityTargetSpace.z);
		useTarget = true;
	}

	CActor *pActor = (CActor*)(gEnv->pGame->GetIGameFramework()->GetClientActor());
	bool bBack = false;
	if (IMovementController *pMV = pActor->GetMovementController())
	{
		SMovementState state;
		pMV->GetMovementState(state);
		Vec3 vLook = state.eyeDirection;
		Vec3 vDir = vWorldPos - pActor->GetEntity()->GetWorldPos();
		float fDot = vLook.Dot(vDir);
		if(fDot<0.0f)
			bBack = true;
	}

	bool bLeft(false), bRight(false), bTop(false), bBottom(false);

	if(vEntityScreenSpace.z > 1.0f && bBack)
	{
		Vec2 vCenter(50.0f, 50.0f);
		Vec2 vTarget(-vEntityScreenSpace.x, -vEntityScreenSpace.y);
		Vec2 vScreenDir = vTarget - vCenter;
		vScreenDir = vCenter + (100.0f * vScreenDir.NormalizeSafe());
		vEntityScreenSpace.y = vScreenDir.y;
		vEntityScreenSpace.x = vScreenDir.x;
		useTarget = false;
	}
	if(vEntityScreenSpace.x < 2.0f)
	{
		bLeft = true;
		vEntityScreenSpace.x = 2.0f;
		useTarget = false;
	}
	if(vEntityScreenSpace.x > 98.0f)
	{
		bRight = true;
		vEntityScreenSpace.x = 98.0f;
		useTarget = false;
	}
	if(vEntityScreenSpace.y < 2.01f)
	{
		bTop = true;
		vEntityScreenSpace.y = 2.0f;
		useTarget = false;
	}
	if(vEntityScreenSpace.y > 97.99f)
	{
		bBottom = true;
		vEntityScreenSpace.y = 98.0f;
		useTarget = false;
	}

	float fScaleX = 0.0f;
	float fScaleY = 0.0f;
	float fHalfUselessSize = 0.0f;
	GetProjectionScale(&m_animMissionObjective,&fScaleX,&fScaleY,&fHalfUselessSize);

	if(bLeft && bTop)
	{
		iconType = eOS_TopLeft;
	}
	else if(bLeft && bBottom)
	{
		iconType = eOS_BottomLeft;
	}
	else if(bRight && bTop)
	{
		iconType = eOS_TopRight;
	}
	else if(bRight && bBottom)
	{
		iconType = eOS_BottomRight;
	}
	else if(bLeft)
	{
		iconType = eOS_Left;
	}
	else if(bRight)
	{
		iconType = eOS_Right;
	}
	else if(bTop)
	{
		iconType = eOS_Top;
	}
	else if(bBottom)
	{
		iconType = eOS_Bottom;
	}

	float rotation = 0.0f;
	if(useTarget)
	{
		float diffX = (vEntityScreenSpace.x*fScaleX+fHalfUselessSize+16.0f) - (vEntityTargetSpace.x*fScaleX+fHalfUselessSize+16.0f);
		float diffY = (vEntityScreenSpace.y*fScaleY) - (vEntityTargetSpace.y*fScaleY);
		Vec2 dir(diffX, diffY);
		dir.NormalizeSafe();

		float fAngle;
		if(dir.y < 0)
		{
			fAngle = RAD2DEG(acos_tpl(dir.x));
		}
		else
		{
			fAngle = RAD2DEG(gf_PI2-acos_tpl(dir.x));
		}
		rotation = fAngle - 90.0f;
	}

	int		iMinDist = g_pGameCVars->hud_onScreenNearDistance;
	int		iMaxDist = g_pGameCVars->hud_onScreenFarDistance;
	float	fMinSize = g_pGameCVars->hud_onScreenNearSize;
	float	fMaxSize = g_pGameCVars->hud_onScreenFarSize;

	CActor *pPlayerActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor());

	float	fDist = (vWorldPos-pPlayerActor->GetEntity()->GetWorldPos()).len();
	float fSize = 1.0;
	if(fDist<=iMinDist)
	{
		fSize = fMinSize;
	}
	else if(fDist>=iMaxDist)
	{
		fSize = fMaxSize;
	}
	else if(iMaxDist>iMinDist)
	{
		float fA = ((float)iMaxDist - fDist);
		float fB = (float)(iMaxDist - iMinDist);
		float fC = (fMinSize - fMaxSize);
		fSize = ((fA / fB) * fC) + fMaxSize;
	}

	float centerX = 50.0;
	float centerY = 50.0;

	m_missionObjectiveNumEntries += FillUpMOArray(&m_missionObjectiveValues, objective, vEntityScreenSpace.x*fScaleX+fHalfUselessSize+16.0f, vEntityScreenSpace.y*fScaleY, iconType, friendly, (int)fDist, fSize*fSize, -rotation);
	bool nearCenter = (pow(centerX-vEntityScreenSpace.x+1.5f,2.0f)+pow(centerY-vEntityScreenSpace.y+2.5f,2.0f))<16.0f;
	if(nearCenter && (gEnv->bMultiplayer || m_pHUDScopes->IsBinocularsShown()))
	{
		m_objectiveNearCenter = objective;
	}
}

void CHUD::UpdateAllMissionObjectives()
{
	if(m_missionObjectiveValues.size() && (gEnv->bMultiplayer || m_pHUDScopes->IsBinocularsShown()))
	{
		m_animMissionObjective.SetVisible(true);
		m_animMissionObjective.GetFlashPlayer()->SetVariableArray(FVAT_Double, "m_allValues", 0, &m_missionObjectiveValues[0], m_missionObjectiveNumEntries);
		m_animMissionObjective.Invoke("updateMissionObjectives");
		const char* description = "";
		if(m_pHUDRadar)
			description = m_pHUDRadar->GetObjectiveDescription(m_objectiveNearCenter);
		SFlashVarValue args[2] = {(int)m_objectiveNearCenter, description};
		m_animMissionObjective.Invoke("setNearCenter",args, 2);
		m_objectiveNearCenter = 0;
	}
	else if(m_animMissionObjective.GetVisible())
	{
		m_animMissionObjective.SetVisible(false);
	}

	m_missionObjectiveNumEntries = 0;
	m_missionObjectiveValues.clear();
}


int CHUD::FillUpMOArray(std::vector<double> *doubleArray, double a, double b, double c, double d, double e, double f, double g, double h)
{
	doubleArray->push_back(a);
	doubleArray->push_back(b);
	doubleArray->push_back(c);
	doubleArray->push_back(d);
	doubleArray->push_back(e);
	doubleArray->push_back(f);
	doubleArray->push_back(g);
	doubleArray->push_back(h);
	return 8;
}

void CHUD::TrackProjectiles(CPlayer* pPlayerActor)
{
	if(m_trackedProjectiles.empty())
	{
		if (m_friendlyTrackerStatus)
			UpdateProjectileTracker(m_animFriendlyProjectileTracker, 0, m_friendlyTrackerStatus, ZERO);
		if (m_hostileTrackerStatus)
			UpdateProjectileTracker(m_animHostileProjectileTracker, 0, m_hostileTrackerStatus, ZERO);

		return;
	}

	if ((g_pGameCVars->g_difficultyLevel > 3) && !gEnv->bMultiplayer)
		return;

	if(!m_animFriendlyProjectileTracker.IsLoaded())
	{
		m_animFriendlyProjectileTracker.Reload();
		if(!m_animFriendlyProjectileTracker.IsLoaded()) //asset missing so far ..
			m_animFriendlyProjectileTracker.Load("Libs/UI/HUD_GrenadeDetect.gfx", eFD_Center, eFAF_Visible);
	}
	if(!m_animHostileProjectileTracker.IsLoaded())
		m_animHostileProjectileTracker.Reload();

	IEntity *pClosestFriendly=0;
	IEntity *pClosestHostile=0;

	float closestFriendly=999999.0;
	float closestHostile=999999.0f;

	int teamId=g_pGame->GetGameRules()->GetTeam(pPlayerActor->GetEntityId());

	std::vector<EntityId>::iterator end = m_trackedProjectiles.end();
	std::vector<EntityId>::iterator it = m_trackedProjectiles.begin();

	CWeaponSystem *pWeaponSystem=g_pGame->GetWeaponSystem();

	Vec3 player=pPlayerActor->GetEntity()->GetWorldPos();
	Vec3 screen;

	for (;it!=end;++it)
	{
		if (CProjectile *pProjectile=pWeaponSystem->GetProjectile(*it))
		{
			Vec3 proj=pProjectile->GetEntity()->GetWorldPos();
			m_pRenderer->ProjectToScreen(	proj.x, proj.y,	proj.z, &screen.x,	&screen.y, &screen.z );

			if (screen.z > 1.0f)
				continue; // ignore grenades behind the camera

			float distSq=(player-proj).len2();
			if (distSq>20.0f*20.0f)
				continue;

			int projTeamId=g_pGame->GetGameRules()->GetTeam(pProjectile->GetOwnerId());
			bool hostile=(!teamId || teamId!=projTeamId) && (pProjectile->GetOwnerId()!=pPlayerActor->GetEntityId());

			if (distSq<closestHostile && hostile)
			{
				pClosestHostile=pProjectile->GetEntity();
				closestHostile=distSq;
			}
			else if (distSq<closestFriendly && !hostile)
			{
				pClosestFriendly=pProjectile->GetEntity();
				closestFriendly=distSq;
			}
		}
	}

	UpdateProjectileTracker(m_animFriendlyProjectileTracker, pClosestFriendly, m_friendlyTrackerStatus, player);
	UpdateProjectileTracker(m_animHostileProjectileTracker, pClosestHostile, m_hostileTrackerStatus, player);
}

void CHUD::UpdateProjectileTracker(CGameFlashAnimation &anim, IEntity *pProjectile, uint8 &status, const Vec3 &player)
{
	if (pProjectile)
	{
		Vec3 screen;
		Vec3 world=pProjectile->GetWorldPos();
		m_pRenderer->ProjectToScreen(	world.x, world.y,	world.z, &screen.x,	&screen.y, &screen.z);

		if (!status)
		{
			anim.Invoke("showGrenadeDetector");
			status=1;
		}
		else
		{
			if(screen.x<3.0f)
			{
				screen.x=3.0f;
				anim.Invoke("morphLeft");
				status=2;
			}
			else if(screen.x>97.0f)
			{
				screen.x=97.0f;
				anim.Invoke("morphRight");
				status=2;
			}
			else if(status>1)
			{
				anim.Invoke("morphNone");
				status=1;
			}
		}

		float sx=0.0f;
		float sy=0.0f;
		float useless=0.0f;

		GetProjectionScale(&anim, &sx, &sy, &useless);

		float mh	= (float) anim.GetFlashPlayer()->GetHeight();
		float rh	= (float) m_pRenderer->GetHeight();

		// Note: 18 is the size of the box (coming from Flash)
		float boxX = 18.0f*mh/rh;
		float boxY = 18.0f*mh/rh;

		char strX[32];
		char strY[32];
		sprintf(strX,"%f", screen.x*sx-boxX+useless);
		sprintf(strY,"%f", screen.y*sy-boxY);

		anim.SetVariable("Root.GrenadeDetect._x", strX);
		anim.SetVariable("Root.GrenadeDetect._y", strY);

		char strDistance[32];
		sprintf(strDistance, "%.2fM",(world-player).len());
		anim.Invoke("setDistance", strDistance);

		string grenadeName("@");
		grenadeName.append(pProjectile->GetClass()->GetName());
		anim.Invoke("setGrenadeType", grenadeName.c_str());
	}
	else if (status)
	{
		anim.Invoke("hideGrenadeDetector");
		status=0;
	}
}

void CHUD::IndicateDamage(EntityId weaponId, Vec3 direction, bool onVehicle)
{
	Vec3 vlookingDirection = FORWARD_DIRECTION;
	CGameFlashAnimation* pAnim = NULL;

	CActor *pPlayerActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if(!pPlayerActor)
		return;

	if(IEntity *pEntity = gEnv->pEntitySystem->GetEntity(weaponId))
	{
		if(pEntity->GetClass() == CItem::sGaussRifleClass)
		{
			m_animRadarCompassStealth.Invoke("GaussHit");
			m_animPlayerStats.Invoke("GaussHit");
		}
	}

	IMovementController *pMovementController = NULL;

	float fFront = 0.0f;
	float fRight = 0.0f;

	if(!onVehicle)
	{
		if(!g_pGameCVars->hud_chDamageIndicator)
			return;
		pMovementController = pPlayerActor->GetMovementController();
		pAnim = m_pHUDCrosshair->GetFlashAnim();
	}
	else if(IVehicle *pVehicle = pPlayerActor->GetLinkedVehicle())
	{
		pMovementController = pVehicle->GetMovementController();
		pAnim = &(m_pHUDVehicleInterface->m_animStats);
	}

	if(pMovementController && pAnim)
	{
		SMovementState sMovementState;
		pMovementController->GetMovementState(sMovementState);
		vlookingDirection = sMovementState.eyeDirection;
	}
	else
		return;

	if(!onVehicle)
	{
		//we use a static/world damage indicator and add the view rotation to the indicator animation now
		fFront = -(Vec3(0,-1,0).Dot(direction));
		fRight = -(Vec3(-1, 0, 0).Dot(direction));
	}
	else
	{
		Vec3 vRightDir = vlookingDirection.Cross(Vec3(0,0,1));
		fFront = -vlookingDirection.Dot(direction);
		fRight = -vRightDir.Dot(direction);
	}

	if(fabsf(fFront) > 0.35f)
	{
		if(fFront > 0)
			pAnim->Invoke("setDamageDirection", 1);
		else
			pAnim->Invoke("setDamageDirection", 3);
	}
	if(fabsf(fRight) > 0.35f)
	{
		if(fRight > 0)
			pAnim->Invoke("setDamageDirection", 2);
		else
			pAnim->Invoke("setDamageDirection", 4);
	}
	if(fFront == 0.0f && fRight == 0.0f)
	{
		pAnim->Invoke("setDamageDirection", 1);
		pAnim->Invoke("setDamageDirection", 2);
		pAnim->Invoke("setDamageDirection", 3);
		pAnim->Invoke("setDamageDirection", 4);
	}

	m_fDamageIndicatorTimer = gEnv->pTimer->GetFrameStartTime().GetSeconds();
}

void CHUD::IndicateHit(bool enemyIndicator,IEntity *pEntity, bool explosionFeedback)
{
	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if(!pPlayer)
		return;

	if(explosionFeedback)
		PlaySound(ESound_SpecialHitFeedback);

	if(!pPlayer->GetLinkedVehicle())
		m_pHUDCrosshair->GetFlashAnim()->Invoke("indicateHit");
	else
	{
		IVehicleSeat *pSeat = pPlayer->GetLinkedVehicle()->GetSeatForPassenger(pPlayer->GetEntityId());
		if(pSeat && !pSeat->IsDriver())
			m_pHUDCrosshair->GetFlashAnim()->Invoke("indicateHit");
		else
		{
			m_pHUDVehicleInterface->m_animMainWindow.Invoke("indicateHit", enemyIndicator);

			if(pEntity && !gEnv->bMultiplayer)
			{
				float r = ((unsigned char) ((g_pGameCVars->hud_colorLine >> 16)	& 0xFF)) / 255.0f;
				float g = ((unsigned char) ((g_pGameCVars->hud_colorLine >> 8)	& 0xFF)) / 255.0f;
				float b = ((unsigned char) ((g_pGameCVars->hud_colorLine >> 0)	& 0xFF)) / 255.0f;

				// It should be useless to test if pEntity is an enemy (it's already done by caller func)
				IActor *pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());
				if(pActor)
					m_pHUDSilhouettes->SetSilhouette(pActor,r,g,b,1.0f,5.0f,true);
				else if(IVehicle *pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(pEntity->GetId()))
					m_pHUDSilhouettes->SetSilhouette(pVehicle,r,g,b,1.0f,5.0f);
			}
		}
	}
}

void CHUD::Targetting(EntityId pTargetEntity, bool bStatic)
{
	if(IsAirStrikeAvailable() && GetScopes()->IsBinocularsShown())
	{
		float fCos = fabsf(cosf(gEnv->pTimer->GetAsyncCurTime()));
		std::vector<EntityId>::const_iterator it = m_possibleAirStrikeTargets.begin();
		for(; it != m_possibleAirStrikeTargets.end(); ++it)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(*it);
			if(pEntity)
			{
				m_pHUDSilhouettes->SetSilhouette(pEntity, 1.0f-0.6f*fCos, 1.0f-0.4f*fCos, 1.0f-0.20f*fCos, 0.5f, -1.0f);
			}
		}
	}
	else
	{
		IEntity *pEntityTargetAutoaim = gEnv->pEntitySystem->GetEntity(m_entityTargetAutoaimId);
		if(NULL == pEntityTargetAutoaim)
		{
			m_entityTargetAutoaimId = 0;
		}
		else
		{
			m_pHUDSilhouettes->SetSilhouette(pEntityTargetAutoaim, 0.8f, 0.8f, 1.0f, 0.5f, -1.0f);
		}
	}

	// Tac lock
	if(GetCurrentWeapon() && m_bTacLock)
	{
		Vec3 vAimPos		= (static_cast<CWeapon *>(GetCurrentWeapon()))->GetAimLocation();
		Vec3 vTargetPos	= (static_cast<CWeapon *>(GetCurrentWeapon()))->GetTargetLocation();

		Vec3 vAimScreenSpace;		
		m_pRenderer->ProjectToScreen(vAimPos.x,vAimPos.y,vAimPos.z,&vAimScreenSpace.x,&vAimScreenSpace.y,&vAimScreenSpace.z);

		Vec3 vTargetScreenSpace;
		m_pRenderer->ProjectToScreen(vTargetPos.x,vTargetPos.y,vTargetPos.z,&vTargetScreenSpace.x,&vTargetScreenSpace.y,&vTargetScreenSpace.z);

		float fScaleX = 0.0f;
		float fScaleY = 0.0f;
		float fHalfUselessSize = 0.0f;
		GetProjectionScale(&m_animTacLock,&fScaleX,&fScaleY,&fHalfUselessSize);

		m_animTacLock.SetVariable("AimSpot._x",SFlashVarValue(vAimScreenSpace.x*fScaleX+fHalfUselessSize));
		m_animTacLock.SetVariable("AimSpot._y",SFlashVarValue(vAimScreenSpace.y*fScaleY));

		m_animTacLock.SetVariable("TargetSpot._x",SFlashVarValue(vTargetScreenSpace.x*fScaleX+fHalfUselessSize));
		m_animTacLock.SetVariable("TargetSpot._y",SFlashVarValue(vTargetScreenSpace.y*fScaleY));
	}

	//OnScreenMissionObjective
	float fX(0.0f), fY(0.0f);
	CActor *pActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	int team = 0;
	CGameRules *pGameRules = (CGameRules*)(gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules());
	if(pActor && pGameRules)
		team = pGameRules->GetTeam(pActor->GetEntityId());

	if(m_iPlayerOwnedVehicle)
	{
		IEntity *pEntity = GetISystem()->GetIEntitySystem()->GetEntity(m_iPlayerOwnedVehicle);
		if(!pEntity)
		{
			m_iPlayerOwnedVehicle = 0;
		}
		else
		{
			IVehicle *pCurrentVehicle = pActor->GetLinkedVehicle();
			if(!(pCurrentVehicle && pCurrentVehicle->GetEntityId() == m_iPlayerOwnedVehicle))
				UpdateMissionObjectiveIcon(m_iPlayerOwnedVehicle,1,eOS_Purchase);
		}
	}

	if(!gEnv->bMultiplayer && GetScopes()->IsBinocularsShown() && g_pGameCVars->g_difficultyLevel < 3)
	{
		//draw single player mission objectives
		std::map<EntityId, CHUDRadar::RadarObjective>::const_iterator it = m_pHUDRadar->m_missionObjectives.begin();
		std::map<EntityId, CHUDRadar::RadarObjective>::const_iterator end = m_pHUDRadar->m_missionObjectives.end();
		for(; it != end; ++it)
		{
			UpdateMissionObjectiveIcon(it->first, 0, eOS_SPObjective);
		}
	}

/*	if(m_pHUDRadar->m_selectedTeamMates.size())
	{
		std::vector<EntityId>::iterator it = m_pHUDRadar->m_selectedTeamMates.begin();
		for(; it != m_pHUDRadar->m_selectedTeamMates.end(); ++it)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(*it);
			if(pEntity)
				UpdateMissionObjectiveIcon(*it,1,eOS_TeamMate);
		}
	}*/

	if(pActor && pGameRules)
	{
		SetTACWeapon(false);

		const std::vector<CGameRules::SMinimapEntity> synchEntities = pGameRules->GetMinimapEntities();
		for(int m = 0; m < synchEntities.size(); ++m)
		{
			bool vehicle = false;
			CGameRules::SMinimapEntity mEntity = synchEntities[m];
			FlashRadarType type = m_pHUDRadar->GetSynchedEntityType(mEntity.type);
			IEntity *pEntity = NULL;
			if(type == ENuclearWeapon)
			{
				if(IItem *pWeapon = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(mEntity.entityId))
				{
					pEntity = gEnv->pEntitySystem->GetEntity(mEntity.entityId);
					if(EntityId ownerId=pWeapon->GetOwnerId())
					{
						pEntity = gEnv->pEntitySystem->GetEntity(ownerId);

						if (ownerId==g_pGame->GetIGameFramework()->GetClientActorId())
						{
							SetTACWeapon(true);
							pEntity=0;
						}
					}
				}
				else
				{
					pEntity = gEnv->pEntitySystem->GetEntity(mEntity.entityId);
					if (IVehicle *pVehicle=g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(mEntity.entityId))
					{
						vehicle=true;

						if (pVehicle->GetDriver()==g_pGame->GetIGameFramework()->GetClientActor())
						{
							SetTACWeapon(true);
							pEntity=0;
						}
					}
				}
			}

			if(!pEntity) continue;

			float fX(0.0f), fY(0.0f);

			int friendly = m_pHUDRadar->FriendOrFoe(gEnv->bMultiplayer,  team, pEntity, pGameRules);
			if(vehicle)
				UpdateMissionObjectiveIcon(pEntity->GetId(),friendly,eOS_TACTank);
			else
			{
				CPlayer *pShooter=static_cast<CPlayer *>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()));
				if (pShooter && (!pShooter->IsCloaked() || friendly==EFriend || pShooter->GetLinkedVehicle()))
					UpdateMissionObjectiveIcon(pEntity->GetId(),friendly,eOS_TACWeapon);
			}

			bool yetdrawn = false;
			if(vehicle)
			{
				IVehicle *pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(pEntity->GetId());
				if(pVehicle)
				{
					IActor *pDriverActor = pVehicle->GetDriver();
					if(pDriverActor)
					{
						IEntity *pDriver = pDriverActor->GetEntity();
						if(pDriver)
						{
							SFlashVarValue arg[2] = {(int)pEntity->GetId(),pDriver->GetName()};
							m_animMissionObjective.Invoke("setPlayerName", arg, 2);
							yetdrawn = true;
						}
					}
					else
					{
						SFlashVarValue arg[2] = {(int)pEntity->GetId(),"@ui_osmo_NODRIVER"};
						m_animMissionObjective.Invoke("setPlayerName", arg, 2);
						yetdrawn = true;
					}
				}
			}
			if(!yetdrawn)
			{
				SFlashVarValue arg[2] = {(int)pEntity->GetId(),pEntity->GetName()};
				m_animMissionObjective.Invoke("setPlayerName", arg, 2);
			}
		}
	}

	if(pActor)
	{
		std::vector<EntityId>::iterator it = m_pHUDRadar->GetObjectives()->begin();
		for(; it != m_pHUDRadar->GetObjectives()->end(); ++it)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(*it);
			if(pEntity)
			{
				int friendly = m_pHUDRadar->FriendOrFoe(gEnv->bMultiplayer, team, pEntity, pGameRules);
				FlashRadarType type = m_pHUDRadar->ChooseType(pEntity);
				if(friendly==1 && IsUnderAttack(pEntity))
				{
					friendly = 3;
					AddOnScreenMissionObjective(pEntity, friendly);
				}
				else if(HasTACWeapon() && (type == EHeadquarter || type == EHeadquarter2) && friendly == 2)
				{
					// Show TAC Target icon
					AddOnScreenMissionObjective(pEntity, friendly);
				}
				else if(m_bShowAllOnScreenObjectives || m_iOnScreenObjective==(*it))
				{
					AddOnScreenMissionObjective(pEntity, friendly);
				}
			}
		}
	}

	// icons for friendly claymores and mines
	if(gEnv->bMultiplayer && pActor && pGameRules)
	{
		int playerTeam = pGameRules->GetTeam(pActor->GetEntityId());
		std::list<EntityId>::iterator next;
		std::list<EntityId>::iterator it = m_explosiveList.begin();
		for(; it != m_explosiveList.end(); it=next)
		{
			next = it; ++next;

			int mineTeam = pGameRules->GetTeam(*it);
			if((mineTeam != 0 && mineTeam == playerTeam) || (mineTeam == 0 && pGameRules->GetTeamCount() < 2))
			{
				// quick check for proximity
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(*it);
				if(pEntity)
				{
					Vec3 dir = pEntity->GetWorldPos() - pActor->GetEntity()->GetWorldPos();
					if(dir.GetLengthSquared() <= 100.0f)
					{
						Vec3 targetPoint(0,0,0);
						FlashOnScreenIcon icon = eOS_Bottom;
						CProjectile *pProjectile = g_pGame->GetWeaponSystem()->GetProjectile(*it);
						if(pProjectile && pProjectile->GetEntity())
						{
							// in IA, only display mines/claymores placed by this player
							if(pGameRules->GetTeamCount() < 2 && pProjectile->GetOwnerId() != pActor->GetEntityId())
								continue;

							IEntityClass* pClass = pProjectile->GetEntity()->GetClass();
							if(pClass == m_pClaymore)
							{
								icon = eOS_Claymore;
								CClaymore *pClaymore = static_cast<CClaymore *>(pProjectile);
								if(pClaymore)
								{
									targetPoint = pEntity->GetWorldPos() + pClaymore->GetTriggerDirection();
								}
							}
							else if(pClass == m_pAVMine)
							{
								icon = eOS_Mine;
							}
							UpdateMissionObjectiveIcon(*it, 1, icon, true, targetPoint);
						}
					}
				}
			}
		}
	}

	UpdateAllMissionObjectives();
}

bool CHUD::IsUnderAttack(IEntity *pEntity)
{
	if(!pEntity)
		return false;

	IScriptTable *pScriptTable = g_pGame->GetGameRules()->GetEntity()->GetScriptTable();
	if (!pScriptTable)
		return false;

	bool underAttack=false;
	HSCRIPTFUNCTION scriptFuncHelper = NULL;
	if(pScriptTable->GetValue("IsUncapturing", scriptFuncHelper) && scriptFuncHelper)
	{
		Script::CallReturn(gEnv->pScriptSystem, scriptFuncHelper, pScriptTable, ScriptHandle(pEntity->GetId()), underAttack);
		gEnv->pScriptSystem->ReleaseFunc(scriptFuncHelper);
		scriptFuncHelper = NULL;
	}

	return underAttack;
}

void CHUD::AddOnScreenMissionObjective(IEntity *pEntity, int friendly)
{
	FlashRadarType type = m_pHUDRadar->ChooseType(pEntity);
	if(type == EHeadquarter)
		if(HasTACWeapon() && friendly==2)
			UpdateMissionObjectiveIcon(pEntity->GetId(),friendly,eOS_HQTarget);
		else
			UpdateMissionObjectiveIcon(pEntity->GetId(),friendly,eOS_HQKorean);
	else if(type == EHeadquarter2)
		if(HasTACWeapon() && friendly==2)
			UpdateMissionObjectiveIcon(pEntity->GetId(),friendly,eOS_HQTarget);
		else
			UpdateMissionObjectiveIcon(pEntity->GetId(),friendly,eOS_HQUS);
	else if(type == EFactoryTank)
		UpdateMissionObjectiveIcon(pEntity->GetId(),friendly,eOS_FactoryTank);
	else if(type == EFactoryAir)
		UpdateMissionObjectiveIcon(pEntity->GetId(),friendly,eOS_FactoryAir);
	else if(type == EFactorySea)
		UpdateMissionObjectiveIcon(pEntity->GetId(),friendly,eOS_FactoryNaval);
	else if(type == EFactoryVehicle)
		UpdateMissionObjectiveIcon(pEntity->GetId(),friendly,eOS_FactoryVehicle);
	else if(type == EFactoryPrototype)
		UpdateMissionObjectiveIcon(pEntity->GetId(),friendly,eOS_FactoryPrototypes);
	else if(type == EAlienEnergySource)
		UpdateMissionObjectiveIcon(pEntity->GetId(),friendly,eOS_AlienEnergyPoint);

	else
	{
		//now spawn points
		std::vector<EntityId> locations;
		CGameRules *pGameRules = (CGameRules*)(gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules());
		pGameRules->GetSpawnGroups(locations);
		for(int i = 0; i < locations.size(); ++i)
		{
			IEntity *pSpawnEntity = gEnv->pEntitySystem->GetEntity(locations[i]);
			if(!pSpawnEntity) continue;
			else if(pSpawnEntity == pEntity)
			{
				UpdateMissionObjectiveIcon(pEntity->GetId(),friendly,eOS_Spawnpoint);
				break;
			}
		}
	}
}

void CHUD::ShowKillAreaWarning(bool active, int timer)
{
	if(active && timer && !m_animKillAreaWarning.IsLoaded())
	{
		m_animDeathMessage.Unload();
		m_animKillAreaWarning.Reload();
		m_animKillAreaWarning.Invoke("showWarning");
	}
	else if(m_animKillAreaWarning.IsLoaded() && (!active || !timer))
	{
		m_animKillAreaWarning.Unload();
		return;
	}

	//set timer
	if (m_animKillAreaWarning.IsLoaded())
	{
		m_animKillAreaWarning.Invoke("setCountdown", timer);
	}
	else if(timer <= 0 && active)
	{
		if(IActor *pActor = gEnv->pGame->GetIGameFramework()->GetClientActor())
		{
			IMaterialEffects* pMaterialEffects = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects();
			SMFXRunTimeEffectParams params;
			params.pos = pActor->GetEntity()->GetWorldPos();
			params.soundSemantic = eSoundSemantic_HUD;
			TMFXEffectId id = pMaterialEffects->GetEffectIdByName("player_fx", "player_boundry_damage");
			pMaterialEffects->ExecuteEffect(id, params);
		}

		m_animDeathMessage.Reload();
		m_animDeathMessage.Invoke("showKillEvent");
	}
}


void CHUD::ShowDeathFX(int type)
{
	if(m_godMode || !g_pGame->GetIGameFramework()->IsGameStarted())
		return;

	IMaterialEffects* pMaterialEffects = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects();
	if (m_deathFxId != InvalidEffectId)
	{
		pMaterialEffects->StopEffect(m_deathFxId);
		m_deathFxId = InvalidEffectId;
	}

	if (type<=0)
		return;

	if (gEnv->bMultiplayer)
	{
		if (g_pGameCVars->g_deathEffects==0)
			return;
	}

	IActor *pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
	if (!pActor)
		return;

	//if(pActor->GetHealth() <= 0)
	//	return;
	//find a fix for retriggering this when actor died already ...

	SMFXRunTimeEffectParams params;
	params.pos = pActor->GetEntity()->GetWorldPos();
	params.soundSemantic = eSoundSemantic_HUD;

	static const char* deathType[] =
	{
		"playerdeath_generic",
		"playerdeath_headshot",
		"playerdeath_melee",
		"playerdeath_freeze"
	};
	
	static const int maxTypes = sizeof(deathType) / sizeof(deathType[0]);
	type = type < 1 ? 1 : ( type <= maxTypes ? type : maxTypes ); // sets values outside [0, maxTypes) to 0  

	m_deathFxId = pMaterialEffects->GetEffectIdByName("player_fx", deathType[type-1]);
	if (m_deathFxId != InvalidEffectId)
		pMaterialEffects->ExecuteEffect(m_deathFxId, params);

	if(IMusicSystem *pMusic = gEnv->pSystem->GetIMusicSystem())
		pMusic->SetMood("low_health");
}

void CHUD::UpdateVoiceChat()
{
	if(!gEnv->bMultiplayer)
		return;

	IVoiceContext *pVoiceContext = gEnv->pGame->GetIGameFramework()->GetNetContext()->GetVoiceContext();

	if(!pVoiceContext || !pVoiceContext->IsEnabled())
		return;

	IActor *pLocalActor = g_pGame->GetIGameFramework()->GetClientActor();
	if(!pLocalActor)
		return;

	INetChannel *pNetChannel = gEnv->pGame->GetIGameFramework()->GetClientChannel();
	if(!pNetChannel)
		return;

	bool someoneTalking = false;
	EntityId localPlayerId = pLocalActor->GetEntityId();
	//CGameRules::TPlayers players;
	//g_pGame->GetGameRules()->GetPlayers(players); - GameRules has 0 players on client
	//for(CGameRules::TPlayers::iterator it = players.begin(), itEnd = players.end(); it != itEnd; ++it)
	IActorIteratorPtr it = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
	while (IActor* pActor = it->Next())
	{
		if (!pActor->IsPlayer())
			continue;

		CGameRules* pGR = g_pGame->GetGameRules();

		//IEntity *pEntity = gEnv->pEntitySystem->GetEntity(*it);
		IEntity *pEntity = pActor->GetEntity();
		if(pEntity)
		{
			if(pEntity->GetId() == localPlayerId)
			{			
				if(g_pGame->GetIGameFramework()->IsVoiceRecordingEnabled())
				{
					SUIWideString voice(pEntity->GetName());
					m_animVoiceChat.Invoke("addVoice", voice.c_str());
					someoneTalking = true;
				}
			}
			else if(pGR && (pGR->GetTeamCount() == 1 || (pGR->GetTeam(pEntity->GetId()) == pGR->GetTeam(localPlayerId))))
			{
				if(pNetChannel->TimeSinceVoiceReceipt(pEntity->GetId()).GetSeconds() < 0.2f)
				{
					SUIWideString voice(pEntity->GetName());
					m_animVoiceChat.Invoke("addVoice", voice.c_str());
					someoneTalking = true;
				}
			}
		}
	}

	if(someoneTalking)
	{
		m_animVoiceChat.SetVisible(true);
		m_animVoiceChat.Invoke("updateVoiceChat");
	}
	else
	{
		m_animVoiceChat.SetVisible(false);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UpdateCrosshairVisibility()
{
	// marcok: don't touch this, please
	if (g_pGameCVars->goc_enable)
	{
		m_pHUDCrosshair->GetFlashAnim()->Invoke("setVisible", 1);
		return;
	}

	bool wasVisible = false;

	if(!m_pHUDCrosshair->GetFlashAnim()->IsLoaded())
		return;
	if(m_pHUDCrosshair->GetFlashAnim()->GetFlashPlayer()->GetVisible())
	{
		wasVisible = true;
		m_pHUDCrosshair->GetFlashAnim()->GetFlashPlayer()->SetVisible(false);
	}

  bool forceVehicleCrosshair = m_pHUDVehicleInterface->GetVehicle() && m_pHUDVehicleInterface->ForceCrosshair();
	
	if(!m_iCursorVisibilityCounter && !m_bAutosnap && !m_bHideCrosshair && (!m_bThirdPerson || forceVehicleCrosshair) && !m_pHUDScopes->IsBinocularsShown())
	{
		// Do not show crosshair while in vehicle
		if((!m_pHUDVehicleInterface->GetVehicle() && !m_pHUDVehicleInterface->IsParachute()) || forceVehicleCrosshair)
		{
			m_pHUDCrosshair->GetFlashAnim()->GetFlashPlayer()->SetVisible(true);
		}
	}

	if(wasVisible != m_pHUDCrosshair->GetFlashAnim()->GetFlashPlayer()->GetVisible())
	{
		//turn off damage circle
		m_pHUDCrosshair->GetFlashAnim()->Invoke("clearDamageDirection");
		m_pHUDCrosshair->GetFlashAnim()->GetFlashPlayer()->Advance(0.1f);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UpdateCrosshair(IItem *pItem) 
{ 
	m_pHUDCrosshair->SelectCrosshair(pItem); 
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnToggleThirdPerson(IActor *pActor,bool bThirdPerson)
{
	if (!pActor->IsClient())
		return;

	m_bThirdPerson = bThirdPerson;

	if(m_pHUDVehicleInterface && m_pHUDVehicleInterface->GetHUDType()!=CHUDVehicleInterface::EHUD_NONE)
		m_pHUDVehicleInterface->UpdateVehicleHUDDisplay();

	m_pHUDScopes->OnToggleThirdPerson(bThirdPerson);

	UpdateCrosshairVisibility();
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ShowTargettingAI(EntityId id)
{
	if(gEnv->bMultiplayer)
		return;

	if(!g_pGameCVars->hud_chDamageIndicator)
		return;

	if(g_pGameCVars->g_difficultyLevel > 2)
		return;

	EntityId actorID = id;
	if(IVehicle *pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(id))
	{
		if(pVehicle->GetDriver())
			actorID = pVehicle->GetDriver()->GetEntityId();
	}

	if(IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(actorID))
	{
		if(pActor != g_pGame->GetIGameFramework()->GetClientActor())
		{
			m_pHUDSilhouettes->SetSilhouette(pActor,0.89411f,0.10588f,0.10588f,1,2);
			m_pHUDRadar->AddEntityTemporarily(actorID);
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::FadeCinematicBars(int targetVal)
{
	m_animCinematicBar.Reload();

	m_animCinematicBar.SetVisible(true);
	m_animCinematicBar.Invoke("setBarPos", targetVal<<1); // *2, because in flash its percentage of half size!
	m_cineState = eHCS_Fading;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UpdateCinematicAnim(float frameTime)
{
	if (m_cineState == eHCS_None)
		return;

	if(m_animCinematicBar.IsLoaded())
	{
		IFlashPlayer* pFP = m_animCinematicBar.GetFlashPlayer();
		pFP->Advance(frameTime);
		pFP->Render();
		if (pFP->GetVisible() == false)
		{
			m_cineState = eHCS_None;
			m_animCinematicBar.Unload();
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::UpdateSubtitlesAnim(float frameTime)
{
	UpdateSubtitlesManualRender(frameTime);
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::OnBeginCutScene(IAnimSequence* pSeq, bool bResetFX)
{
	if (pSeq == 0)
		return false;

	if(m_pModalHUD == &m_animPDA)
	{
		ShowPDA(false);
	}
	else if(m_pModalHUD == &m_animWeaponAccessories)
	{
		ShowWeaponAccessories(false);
	}

	if(m_bNightVisionActive)
		OnAction(g_pGame->Actions().hud_night_vision, 1, 1.0f);	//turn off

	int flags = pSeq->GetFlags();
	if (IAnimSequence::IS_16TO9 & flags)
	{
		FadeCinematicBars(g_pGameCVars->hud_panoramicHeight);
	}

	if (IAnimSequence::NO_PLAYER & flags)
		g_pGameActions->FilterCutsceneNoPlayer()->Enable(true);
	else
		g_pGameActions->FilterCutscene()->Enable(true);

	g_pGameActions->FilterInVehicleSuitMenu()->Enable(true);
	g_pGameActions->FilterNoGrenades()->Enable(true);

	if(IAnimSequence::NO_HUD & flags)
	{
		m_cineHideHUD = true;
	}

	CActor *pPlayerActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor());

	if (pPlayerActor)
	{
		if (CPlayer* pPlayer = static_cast<CPlayer*> (pPlayerActor))
		{
			if(m_pHUDScopes->m_animBinoculars.IsLoaded())
			{
				if(m_pHUDScopes->IsBinocularsShown())
					pPlayer->SelectLastItem(false);
				m_pHUDScopes->ShowBinoculars(false,false,true);
			}

			pPlayer->StopLoopingSounds();

			if(IAnimSequence::NO_PLAYER & flags)
			{
				if (SPlayerStats* pActorStats = static_cast<SPlayerStats*> (pPlayer->GetActorStats()))
					pActorStats->spectatorMode = CActor::eASM_Cutscene;	// moved up to avoid conflict with the MP spectator modes
				pPlayer->Draw(false);
				if (pPlayer->GetPlayerInput())
					pPlayer->GetPlayerInput()->Reset();
				if(IItem* pItem = pPlayer->GetCurrentItem())
				{
					if(IWeapon *pWeapon = pItem->GetIWeapon())
					{
						if(pWeapon->IsZoomed() || pWeapon->IsZooming())
							pWeapon->ExitZoom();
					}
				}
				if(COffHand* pOffHand = static_cast<COffHand*>(pPlayer->GetItemByClass(CItem::sOffHandClass)))
					pOffHand->OnBeginCutScene();
				
				pPlayer->HolsterItem(true);
			}
		}
	}

	m_fCutsceneSkipTimer = g_pGameCVars->g_cutsceneSkipDelay;
	m_bCutscenePlaying = true;
	m_bCutsceneAbortPressed = false;

#ifdef USER_alexl
	CryLogAlways("[CX]: BEGIN Frame=%d 0x%p Name=%s Cutscene=%d NoPlayer=%d NoHUD=%d NoAbort=%d",
		gEnv->pRenderer->GetFrameID(false), pSeq, pSeq->GetName(), flags & IAnimSequence::CUT_SCENE, flags & IAnimSequence::NO_PLAYER, flags & IAnimSequence::NO_HUD, flags & IAnimSequence::NO_ABORT);
#endif

	return true;
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::OnEndCutScene(IAnimSequence* pSeq)
{
	if (pSeq == 0)
		return false;

	m_pHUDCrosshair->Reset();

	m_bCutscenePlaying = false;
	if (m_bStopCutsceneNextUpdate)
	{
		m_bStopCutsceneNextUpdate = false; // just in case, the cutscene wasn't stopped in CHUD::OnPostUpdate, but due to some other stuff (like Serialize/Flowgraph)
	}
	g_pGameActions->FilterCutscene()->Enable(false);
	g_pGameActions->FilterCutsceneNoPlayer()->Enable(false);
	g_pGameActions->FilterInVehicleSuitMenu()->Enable(false);
	g_pGameActions->FilterNoGrenades()->Enable(false);

	int flags = pSeq->GetFlags();
	if (IAnimSequence::IS_16TO9 & flags)
	{
		FadeCinematicBars(0);
	}

	if(IAnimSequence::NO_HUD & flags)
		m_cineHideHUD = false;

	if(IAnimSequence::NO_PLAYER & flags)
	{
		if (CActor *pPlayerActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor()))
		{
			if (CPlayer* pPlayer = static_cast<CPlayer*> (pPlayerActor))
			{
				if (SPlayerStats* pActorStats = static_cast<SPlayerStats*> (pPlayer->GetActorStats()))
					pActorStats->spectatorMode = CActor::eASM_None;
				pPlayer->Draw(true);
				if(COffHand* pOffHand = static_cast<COffHand*>(pPlayer->GetItemByClass(CItem::sOffHandClass)))
					pOffHand->OnEndCutScene();

				// restore health and nanosuit, because time has passed during cutscene
				// and player was not-enabled
				// -> simulate health-regen
				pPlayer->SetHealth(pPlayer->GetMaxHealth());
				if (pPlayer->GetNanoSuit())
				{
					pPlayer->GetNanoSuit()->ResetEnergy();
				}
				pPlayer->HolsterItem(false);
			}
		}
	}

#ifdef USER_alexl
	CryLogAlways("[CX]: END Frame=%d 0x%p Name=%s Cutscene=%d NoPlayer=%d NoHUD=%d NoAbort=%d",
		gEnv->pRenderer->GetFrameID(false), pSeq, pSeq->GetName(), flags & IAnimSequence::CUT_SCENE, flags & IAnimSequence::NO_PLAYER, flags & IAnimSequence::NO_HUD, flags & IAnimSequence::NO_ABORT);
#endif

	return true;
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::OnCameraChange(const SCameraParams& cameraParams)
{
	return true;
}

//-----------------------------------------------------------------------------------------------------

void CHUD::OnPlayCutSceneSound(IAnimSequence* pSeq, ISound* pSound)
{
	if (m_hudSubTitleMode == eHSM_CutSceneOnly)
	{
		if (pSound && pSound->GetFlags() & FLAG_SOUND_VOICE)
			ShowSubtitle(pSound, true);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetSubtitleMode(HUDSubtitleMode mode)
{
	m_hudSubTitleMode = mode;
	ISubtitleManager* pSubtitleManager = g_pGame->GetIGameFramework()->GetISubtitleManager();
	if (pSubtitleManager == 0)
		return;
	if (m_hudSubTitleMode == eHSM_Off || m_hudSubTitleMode == eHSM_CutSceneOnly)
	{
		pSubtitleManager->SetEnabled(false);
		pSubtitleManager->SetHandler(0);
		if (m_hudSubTitleMode == eHSM_Off)
		{
			m_subtitleEntries.clear();
			m_bSubtitlesNeedUpdate = true;
		}
	}
	else // mode == eHSM_All
	{
		pSubtitleManager->SetEnabled(true);
		pSubtitleManager->SetHandler(this);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ShowProgress(int progress, bool init /* = false */, int posX /* = 0 */, int posY /* = 0 */, const char *text, bool topText, bool lockingBar)
{
	CGameFlashAnimation *pAnim = &m_animProgress;
	if(m_bProgressLocking)
		pAnim = &m_animProgressLocking;

	if(init)
	{
		if(lockingBar)
		{
			if(m_animProgress.IsLoaded())
				m_animProgress.Unload();
		}
		else
		{
			if(m_animProgressLocking.IsLoaded())
				m_animProgressLocking.Unload();
		}

		m_bProgressLocking = lockingBar;

		if(!pAnim->IsLoaded())
		{
			if(lockingBar)
				m_animProgressLocking.Load("Libs/UI/HUD_TAC_Locking.gfx", eFD_Center, eFAF_Visible);
			else
				m_animProgress.Load("Libs/UI/HUD_ProgressBar.gfx", eFD_Center, eFAF_Default);
			m_iProgressBar = 0;
		}

		pAnim->Invoke("showProgressBar", true);
		const wchar_t* localizedText = (text)?LocalizeWithParams(text, true):L"";

		SFlashVarValue args[2] = {localizedText, topText ? 1 : 2};
		pAnim->Invoke("setText", args, 2);
		SFlashVarValue pos[2] = {posX*1024/800, posY*768/512};
		pAnim->Invoke("setPosition", pos, 2);

		m_iProgressBarX = posX;
		m_iProgressBarY = posY;
		m_sProgressBarText = string(text);
		m_bProgressBarTextPos = topText;
	}
	else if(progress < 0 && pAnim->IsLoaded())
	{
		pAnim->Invoke("showProgressBar", false); // for sound callback
		pAnim->Unload();
		m_iProgressBar = 0;
	}

	if(pAnim->IsLoaded() && (m_iProgressBar != progress || init))
	{
		pAnim->Invoke("setProgressBar", progress);
		m_iProgressBar = progress;
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::FakeDeath(bool revive)
{
	return; //feature disabled

	CPlayer *pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
	if(pPlayer->IsGod() || gEnv->bMultiplayer)
		return;

	if(pPlayer->GetLinkedEntity())		//not safe
	{
		m_fPlayerRespawnTimer = 0.0f;
		pPlayer->SetHealth(0);
		pPlayer->CreateScriptEvent("kill",0);
		return;
	}

	if(revive)
	{
		float now = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		float diff = now - m_fPlayerRespawnTimer;

		if(pPlayer->GetHealth() <= 0)
		{
			m_fPlayerRespawnTimer = 0.0f;
			return;
		}
		
		if(diff > -3.0f && (now - m_fLastPlayerRespawnEffect > 0.5f))
		{
			IMaterialEffects* pMaterialEffects = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects();
			SMFXRunTimeEffectParams params;
			params.pos = pPlayer->GetEntity()->GetWorldPos();
			params.soundSemantic = eSoundSemantic_HUD;
			TMFXEffectId id = pMaterialEffects->GetEffectIdByName("player_fx", "player_damage_armormode");
			pMaterialEffects->ExecuteEffect(id, params);
			m_fLastPlayerRespawnEffect = now;
		}
		else if(diff > 0.0)
		{
			if(m_bRespawningFromFakeDeath)
			{
				pPlayer->StandUp();
				pPlayer->Revive(false);
				RebootHUD();
				pPlayer->HolsterItem(false);
				if (g_pGameCVars->g_godMode == 3)
				{
					g_pGame->GetGameRules()->PlayerPosForRespawn(pPlayer, false);
				}
				m_fPlayerRespawnTimer = 0.0f;
				//if (IUnknownProxy * pProxy = pPlayer->GetEntity()->GetAI()->GetProxy())
				//	pProxy->EnableUpdate(true); //seems not to work
				pPlayer->CreateScriptEvent("cloaking", 0);
				if (pPlayer->GetEntity()->GetAI())
					gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER,1, "OnNanoSuitUnCloak",pPlayer->GetEntity()->GetAI());
				m_bRespawningFromFakeDeath = false;
				DisplayOverlayFlashMessage(" ");
			}
			else
			{
				m_fPlayerRespawnTimer = 0.0f;
				pPlayer->SetHealth(0);
				pPlayer->CreateScriptEvent("kill",0);
			}
		}
		else
		{
			char text[256];
			char seconds[10];
			sprintf(seconds,"%i",(int)(fabsf(diff)));
			if(m_bRespawningFromFakeDeath)
				sprintf(text,"@ui_respawn_counter");
			else
				sprintf(text,"@ui_revive_counter");
			DisplayFlashMessage(text, 2, ColorF(1.0f,0.0f,0.0f), true, seconds);
		}
	}
	else if(!m_fPlayerRespawnTimer)
	{
		if(pPlayer && (/*g_pGameCVars->g_playerRespawns > 0*/ g_pGameCVars->g_difficultyLevel < 2 || g_pGameCVars->g_godMode == 3))
		{
			//g_pGameCVars->g_playerRespawns--;  //unlimited for now
			//if (g_pGameCVars->g_playerRespawns < 0)
			//	g_pGameCVars->g_playerRespawns = 0;

			pPlayer->HolsterItem(true);
			pPlayer->Fall(Vec3(0,0,0), true);
			pPlayer->SetDeathTimer();
			//if (IUnknownProxy * pProxy = pPlayer->GetEntity()->GetAI()->GetProxy())
			//	pProxy->EnableUpdate(false); //seems not to work - cloak instead
			pPlayer->CreateScriptEvent("cloaking", 1);
			if (pPlayer->GetEntity()->GetAI())
				gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER,1, "OnNanoSuitCloak",pPlayer->GetEntity()->GetAI());

			ShowDeathFX(0);
			GetRadar()->Reset();
			BreakHUD(2);
			m_fPlayerRespawnTimer = gEnv->pTimer->GetFrameStartTime().GetSeconds() + 10.0f;
			m_fLastPlayerRespawnEffect = 0.0f;
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ShowDataUpload(bool active)
{
	if(active)
	{
		if(!m_animDataUpload.IsLoaded())
		{
			m_animDataUpload.Load("Libs/UI/HUD_Recording.gfx", eFD_Right);
			m_animDataUpload.Invoke("showUplink", true);
		}
	}
	else
	{
		if(m_animDataUpload.IsLoaded())
			m_animDataUpload.Unload();
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ShowWeaponsOnGround()
{
	if(!m_pHUDScopes->IsBinocularsShown())
		return;

	if(gEnv->bMultiplayer)
		return;

	if(g_pGameCVars->g_difficultyLevel > 2)
		return;

	IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
	if(!pClientActor)
		return;

	Vec3 clientPos = pClientActor->GetEntity()->GetWorldPos();
	CCamera camera=GetISystem()->GetViewCamera();
	IPhysicalEntity *pSkipEnt=pClientActor?pClientActor->GetEntity()->GetPhysics():0;

	//go through all weapons without owner in your proximity and highlight them
	const std::vector<EntityId> *pItems = m_pHUDRadar->GetNearbyItems();
	if(pItems && pItems->size() > 0)
	{
		std::vector<EntityId>::const_iterator it = pItems->begin();
		std::vector<EntityId>::const_iterator end = pItems->end();
		for(; it != end; ++it)
		{
			EntityId id = *it;
			IItem *pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(id);
			if(pItem && pItem->GetIWeapon() /*&& !pItem->GetOwnerId()*/ && !pItem->GetEntity()->GetParent() && !pItem->GetEntity()->IsHidden())
			{
				float distance = (pItem->GetEntity()->GetWorldPos() - clientPos).len();
				if(distance < 10.0f)
				{
					IPhysicalEntity *pPE = pItem->GetEntity()->GetPhysics();
					if(pPE)
					{
						pe_status_dynamics dyn;
						pPE->GetStatus(&dyn);
						Vec3 dir=(dyn.centerOfMass-camera.GetPosition())*1.15f;
						//raycast object
						ray_hit hit;
						if (gEnv->pPhysicalWorld->RayWorldIntersection(camera.GetPosition(), dir, ent_all, (13&rwi_pierceability_mask), &hit, 1, &pSkipEnt, pSkipEnt?1:0))
						{
							if (!hit.bTerrain && hit.pCollider==pPE)
							{
								//display
//								UpdateMissionObjectiveIcon(id, 1, eOS_Bottom, true);

								m_pHUDSilhouettes->SetSilhouette(pItem,0,0,1,1,-1);
							}
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::FireModeSwitch(bool grenades /* = false */)
{
	if(m_quietMode)
		return;

	if(!grenades)
		m_animPlayerStats.Invoke("switchFireMode");
	else
		m_animPlayerStats.Invoke("switchGrenades");
}

//-----------------------------------------------------------------------------------------------------

void CHUD::DrawGroundCircle(Vec3 pos, float radius, float thickness /* = 1.0f */, float anglePerSection /* = 5.0f */, ColorB col, bool aligned /* = true */, float offset /* = 0.1f */, bool useSecondColor, ColorB colB)
{
	Vec3 p0,p1;
	p0.x = pos.x + radius*sin(0.0f);
	p0.y = pos.y + radius*cos(0.0f);
	p0.z = pos.z;

	if(aligned)
	{
		float terrainHeight = gEnv->p3DEngine->GetTerrainZ((int)p0.x, (int)p0.y);
		p0.z = terrainHeight + 0.25f;
	}

	float step = anglePerSection/180*gf_PI;

	bool switchColor = true;

	for (float angle = 0; angle < 360.0f/180*gf_PI+step; angle += step)
	{
		p1.x = pos.x + radius*sin(angle);
		p1.y = pos.y + radius*cos(angle);
		if(aligned)
		{
			float terrainHeight = gEnv->p3DEngine->GetTerrainZ((int)p1.x, (int)p1.y);
			p1.z = terrainHeight + offset;
			if(p1.z - p0.z > 0.15f)
				p1.z = (3.0f* p0.z + p1.z) * 0.25f;
		}
		else
			p1.z = pos.z + offset;

		if(angle == 0)
			p0 = p1;

		if(useSecondColor)
		{
			switchColor = !switchColor;
			if(switchColor)
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(p0,colB,p1,col,thickness);
			else
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(p0,col,p1,colB,thickness);
		}
		else
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(p0,col,p1,col,thickness);

		p0 = p1;
	}   
}
