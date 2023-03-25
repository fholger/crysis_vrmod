/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Binocular/Scopes HUD object (refactored from old HUD code)

-------------------------------------------------------------------------
History:
- 17:04:2007  17:30 : Created by Jan Müller

*************************************************************************/

#include "StdAfx.h"
#include "HUDScopes.h"
#include "HUD.h"
#include "HUDRadar.h"
#include "HUDSilhouettes.h"
#include "GameFlashAnimation.h"
#include "../Actor.h"
#include "IWorldQuery.h"
#include "GameRules.h"
#include "GameCVars.h"
#include "Weapon.h"

//-----------------------------------------------------------------------------------------------------

#undef HUD_CALL_LISTENERS
#define HUD_CALL_LISTENERS(func) \
{ \
	if (g_pHUD->m_hudListeners.empty() == false) \
	{ \
	g_pHUD->m_hudTempListeners = g_pHUD->m_hudListeners; \
	for (std::vector<CHUD::IHUDListener*>::iterator tmpIter = g_pHUD->m_hudTempListeners.begin(); tmpIter != g_pHUD->m_hudTempListeners.end(); ++tmpIter) \
	(*tmpIter)->func; \
	} \
}

//-----------------------------------------------------------------------------------------------------

CHUDScopes::CHUDScopes(CHUD *pHUD) : g_pHUD(pHUD)
{
	m_eShowScope = ESCOPE_NONE;
	m_fBinocularDistance = 0.0f;
	m_bShowBinoculars = false;
	m_bShowBinocularsNoHUD = false;
	m_bThirdPerson = false;
	m_bDestroyBinocularsAtNextFrame = false;
	m_iZoomLevel = 0;
	LoadFlashFiles();
}

//-----------------------------------------------------------------------------------------------------

CHUDScopes::~CHUDScopes()
{
	m_animBinoculars.Unload();
	m_animSniperScope.Unload();
}

//-----------------------------------------------------------------------------------------------------

void CHUDScopes::LoadFlashFiles(bool force)
{
	m_animSniperScope.Load("Libs/UI/HUD_ScopeSniper.gfx",eFD_Center, eFAF_ManualRender|eFAF_ThisHandler);
	// Everything which is overlapped by the binoculars / scopes MUST be created before them
	if(force)
	{
		m_animBinoculars.Load("Libs/UI/HUD_Binoculars.gfx",eFD_Center,eFAF_ManualRender|eFAF_ThisHandler);
	}
	else
	{
		m_animBinoculars.Init("Libs/UI/HUD_Binoculars.gfx",eFD_Center,eFAF_ManualRender|eFAF_ThisHandler);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDScopes::SetSilhouette(IActor *pActor,IAIObject *pAIObject)
{
	IUnknownProxy *pUnknownProxy = pAIObject ? pAIObject->GetProxy() : NULL;
	int iAlertnessState = pUnknownProxy ? pUnknownProxy->GetAlertnessState() : 0;
	if(0 == iAlertnessState)
	{
		float r = ((unsigned char) ((g_pGameCVars->hud_colorLine >> 16)	& 0xFF)) / 255.0f;
		float g = ((unsigned char) ((g_pGameCVars->hud_colorLine >> 8)	& 0xFF)) / 255.0f;
		float b = ((unsigned char) ((g_pGameCVars->hud_colorLine >> 0)	& 0xFF)) / 255.0f;
		g_pHUD->m_pHUDSilhouettes->SetSilhouette(pActor,r,g,b,1.0f,-1);
	}
	else if(1 == iAlertnessState)
	{
		g_pHUD->m_pHUDSilhouettes->SetSilhouette(pActor,0.86274f,0.7f,0.4745f,1.0f,-1);
	}
	else
	{
		g_pHUD->m_pHUDSilhouettes->SetSilhouette(pActor,0.5f,0.19215f,0.17647f,1.0f,-1);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDScopes::Update(float fDeltaTime)
{
	if(m_bDestroyBinocularsAtNextFrame)
	{
		m_animBinoculars.Unload();
		m_bDestroyBinocularsAtNextFrame = false;
	}
	if(m_animSniperScope.GetVisible())
	{
		m_animSniperScope.GetFlashPlayer()->Advance(fDeltaTime);
		m_animSniperScope.GetFlashPlayer()->Render();
	}
	if(m_animBinoculars.GetVisible())
	{
		m_animBinoculars.GetFlashPlayer()->Advance(fDeltaTime);
		m_animBinoculars.GetFlashPlayer()->Render();
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDScopes::DisplayBinoculars(CPlayer* pPlayerActor)
{
	if(!pPlayerActor)
		return;

	CCamera &rCamera = gEnv->pSystem->GetViewCamera();

	int iZoomMode = -1;

	// We need to update zoom factor even in cinematics (Scope::Zooom functions are not called)
	IItemSystem *pItemSystem = gEnv->pGame->GetIGameFramework()->GetIItemSystem();
	IInventory *pInventory = pPlayerActor->GetInventory();
	if(pItemSystem && pInventory)
	{
		IEntityClass *pBinocularsClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Binoculars");
		IItem *pBinocularsItem = pBinocularsClass ? pItemSystem->GetItem(pInventory->GetItemByClass(pBinocularsClass)) : NULL;
		IWeapon *pBinocularsWeapon = pBinocularsItem ? pBinocularsItem->GetIWeapon() : NULL;

		if(pBinocularsWeapon)
		{
			IZoomMode *pZoomMode = pBinocularsWeapon->GetZoomMode("Scope");
			CRY_ASSERT(pZoomMode);

			if(pZoomMode)
			{
				int iMaxZoomSteps = pZoomMode->GetMaxZoomSteps();
				CRY_ASSERT(3 == iMaxZoomSteps);

				float fFov0 = pZoomMode->GetZoomFoVScale(0);
				float fFov1 = pZoomMode->GetZoomFoVScale(1);
				float fFov2 = pZoomMode->GetZoomFoVScale(2);
				float fFov3 = pZoomMode->GetZoomFoVScale(3);

				float fFov = rCamera.GetFov();

				if(fFov <= fFov2)
					iZoomMode = 3;
				else if(fFov <= fFov1)
					iZoomMode = 2;
				else if(fFov <= fFov0)
					iZoomMode = 1;
				else
					iZoomMode = 0;
			}
		}
	}

	m_animBinoculars.Invoke("setZoomMode",iZoomMode);

	char szY[32];
	sprintf(szY,"%f",336.0f+rCamera.GetViewdir().z*360.0f);

	m_animBinoculars.CheckedSetVariable("Root.Binoculars.Attitude._y",szY);

	wchar_t szN[32];
	wchar_t szW[32];
	g_pHUD->GetGPSPosition(szN,szW);

	SFlashVarValue args[2] = {szN, szW};
	m_animBinoculars.Invoke("setPosition", args, 2);

	IAIObject *pAIPlayer = pPlayerActor->GetEntity()->GetAI();
	if(!pAIPlayer && !gEnv->bMultiplayer)
		return;

	const std::vector<EntityId> *entitiesInProximity = g_pHUD->m_pHUDRadar->GetNearbyEntities();

	const std::deque<CHUDRadar::RadarEntity> *pEntitiesOnRadar = g_pHUD->GetRadar()->GetEntitiesList();

	if(!entitiesInProximity->empty() || !pEntitiesOnRadar->empty())
	{
		std::map<EntityId, bool> drawnEntities;

		for(std::deque<CHUDRadar::RadarEntity>::const_iterator iter=pEntitiesOnRadar->begin(); iter!=pEntitiesOnRadar->end(); ++iter)
		{
			EntityId uiEntityId = (*iter).m_id;

			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(uiEntityId);
			if(!pEntity)
				continue;

			IActor *pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());
			if(!pActor)
				continue;

			IAIObject *pAIObject = pEntity->GetAI();
			if(!pAIObject)
				continue;

			// Display only enemies
			if(!pAIObject->IsHostile(pAIPlayer,false))
				continue;

			if(!pAIObject->IsEnabled())
				continue;

			SetSilhouette(pActor,pAIObject);

			drawnEntities[uiEntityId] = true;
		}

		if(g_pGameCVars->g_difficultyLevel < 3)
		{
			for(std::vector<EntityId>::const_iterator iter=entitiesInProximity->begin(); iter!=entitiesInProximity->end(); ++iter)
			{
				EntityId uiEntityId = (*iter);

				IEntity *pEntity = gEnv->pEntitySystem->GetEntity(uiEntityId);
				if(!pEntity)
					continue;

				IActor *pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());
				if(!pActor)
					continue;

				if(stl::find_in_map(drawnEntities, uiEntityId, false))
					continue;

				if(!gEnv->bMultiplayer)
				{
					IAIObject *pAIObject = pEntity->GetAI();
					if(!pAIObject)
						continue;

					// Display only enemies
					if(!pAIObject->IsHostile(pAIPlayer,false))
						continue;

					if(!pAIObject->IsEnabled())
						continue;

					SetSilhouette(pActor,pAIObject);
				}
				else
				{
					if(g_pGame->GetGameRules()->GetTeam(pPlayerActor->GetEntityId()) == g_pGame->GetGameRules()->GetTeam(pEntity->GetId()))
						continue;

					// Do not display any silouhette in MP (Eric's request)
//					SetSilhouette(pActor,NULL);
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDScopes::DisplayScope(CPlayer* pPlayerActor)
{
	CGameFlashAnimation *pScope = NULL;
	if(m_eShowScope==ESCOPE_SNIPER)
		pScope = &m_animSniperScope;

	if(pScope)
	{
		SMovementState sMovementState;
		pPlayerActor->GetMovementController()->GetMovementState(sMovementState);

		char szY[32];
		sprintf(szY,"%f",384.0f+sMovementState.eyeDirection.z*360.0f);

		const ray_hit *pRay = pPlayerActor->GetGameObject()->GetWorldQuery()->GetLookAtPoint(500.0f);

		if(pRay)
		{
			char szDistance[32];
			sprintf(szDistance,"%.1f",pRay->dist);

			pScope->Invoke("setDistance",szDistance);
		}
		else
		{
			pScope->Invoke("setDistance","0");
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDScopes::ShowBinoculars(bool bVisible, bool bShowIfNoHUD, bool bNoFadeOutOnHide)
{
	if(bVisible)
	{
		if(!m_bShowBinoculars)
		{
			g_pHUD->PlaySound(ESound_BinocularsSelect, true);
			g_pHUD->PlaySound(ESound_BinocularsAmbience, true);
		}

		m_animBinoculars.Reload();

		SFlashVarValue args[3] = {true, 1, m_bThirdPerson};
		m_animBinoculars.Invoke("setVisible", args, 3);
		m_bShowBinoculars = true;
		m_bShowBinocularsNoHUD = bShowIfNoHUD;
		m_bDestroyBinocularsAtNextFrame = false;
		g_pHUD->SetAirStrikeBinoculars(true);
	}
	else /* if(!bVisible) */
	{
		if(m_bShowBinoculars)
		{
			g_pHUD->PlaySound(ESound_BinocularsDeselect, true);
			g_pHUD->PlaySound(ESound_BinocularsAmbience, false);
		}
		m_bShowBinoculars = false;
		m_bShowBinocularsNoHUD = false;
		g_pHUD->SetAirStrikeBinoculars(false);
		if (bNoFadeOutOnHide)
			m_animBinoculars.Unload();
		else
			m_animBinoculars.Invoke("closeBinoculars");
		m_iZoomLevel = 0;
	}

	g_pHUD->m_pHUDSilhouettes->SetType((IsBinocularsShown() && !m_bThirdPerson) ? 1 : 0);

	HUD_CALL_LISTENERS(OnBinoculars(bVisible));
}

//-----------------------------------------------------------------------------------------------------

void CHUDScopes::SetBinocularsDistance(float fDistance)
{
	SFlashVarValue args[2] = {(int) fDistance, (int) (((fDistance-(int) fDistance))*100.0f)};
	m_animBinoculars.Invoke("setDistance", args, 2);
	m_fBinocularDistance = fDistance;
}

//-----------------------------------------------------------------------------------------------------

void CHUDScopes::SetBinocularsZoomMode(int iZoomMode)
{
//	m_animBinoculars.Invoke("setZoomMode", iZoomMode);
	m_iZoomLevel = iZoomMode;
}

//-----------------------------------------------------------------------------------------------------

void CHUDScopes::ShowScope(int iVisible)
{
	if(iVisible==ESCOPE_NONE)
	{
		m_animSniperScope.Invoke("setVisible", false);
		m_eShowScope = ESCOPE_NONE;
		return;
	}

	SFlashVarValue args[2] = {true, iVisible};
	m_animSniperScope.Invoke("setVisible", args, 2);
	m_eShowScope = (EScopeMode)iVisible;
	m_iZoomLevel = 0;

	IFlashPlayer* pScopePlayer = m_animSniperScope.GetFlashPlayer();
	if(pScopePlayer)
		pScopePlayer->SetCompositingDepth(0.1f);
}

//-----------------------------------------------------------------------------------------------------

void CHUDScopes::SetScopeZoomMode(int iZoomMode, string &scopeType)
{
	int type = 1;
	if(!stricmp(scopeType.c_str(), "scope_assault"))	//assault scope has no sound atm.
		type = 0;
	else if(!stricmp(scopeType.c_str(), "scope_sniper"))
		type = 2;

	if(type && m_iZoomLevel != iZoomMode)
	{
		if(iZoomMode > m_iZoomLevel)
			g_pHUD->PlaySound((type==2)?ESound_SniperZoomIn:ESound_BinocularsZoomIn);
		else
			g_pHUD->PlaySound((type==2)?ESound_SniperZoomOut:ESound_BinocularsZoomOut);
	}

	m_iZoomLevel = iZoomMode;

	m_animSniperScope.Invoke("setZoomMode", iZoomMode);
}

//-----------------------------------------------------------------------------------------------------

void CHUDScopes::OnToggleThirdPerson(bool thirdPerson)
{
	m_bThirdPerson = thirdPerson;

	SFlashVarValue args[3] = {IsBinocularsShown(), 0, m_bThirdPerson};
	m_animBinoculars.Invoke("setVisible", args, 3);

	g_pHUD->m_pHUDSilhouettes->SetType((IsBinocularsShown() && !m_bThirdPerson) ? 1 : 0);

	if(m_eShowScope==ESCOPE_NONE)
	{
		SFlashVarValue args[3] = {false, 0, m_bThirdPerson};
		m_animSniperScope.Invoke("setVisible", args, 3);
	}
	else if(m_eShowScope==ESCOPE_SNIPER)
	{
		SFlashVarValue args[3] = {false, 2, m_bThirdPerson};
		args[0] = true;
		m_animSniperScope.Invoke("setVisible",args, 3);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDScopes::Serialize(TSerialize &ser)
{
	ser.Value("hudBinocular", m_bShowBinoculars);
	ser.Value("hudBinocularZoom", m_iZoomLevel);
	ser.Value("hudBinocularNoHUD", m_bShowBinocularsNoHUD);
	ser.Value("hudBinocularDistance", m_fBinocularDistance);

	ser.EnumValue("scopeMode", m_eShowScope, ESCOPE_NONE, ESCOPE_LAST);

	if(ser.IsReading())
	{
		if(m_eShowScope == ESCOPE_SNIPER)
			ShowScope(0); //will rezoom
			
		SetBinocularsDistance(m_fBinocularDistance);
		if(m_bShowBinoculars)
			ShowBinoculars(true);
		if(m_eShowScope == CHUDScopes::ESCOPE_NONE)
			SetBinocularsZoomMode(m_iZoomLevel);
	}
}

//-----------------------------------------------------------------------------------------------------