/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Flash menu screen "Options"

-------------------------------------------------------------------------
History:
- 10:09:2006: Created by Jan Neugebauer

*************************************************************************/
#include "StdAfx.h"

#include "FlashMenuObject.h"
#include "FlashMenuScreen.h"
#include "IGameFramework.h"
#include "IPlayerProfiles.h"
#include "IUIDraw.h"
#include "IMusicSystem.h"
#include "ISound.h"
#include "IActionMapManager.h"
#include "Game.h"
#include "Menus/OptionsManager.h"

static const char* uiControlCodePrefix = "@cc_"; // "@cc_"; // AlexL 03/04/2007: enable this when keys/controls are fully localized
static const size_t uiControlCodePrefixLen = strlen(uiControlCodePrefix);

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::SaveActionToMap(const char* actionmap, const char* action, const char *uiKey)
{
	if(!m_pPlayerProfileManager)
		return;

	IPlayerProfile *pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
	if(!pProfile)
		return;
	IActionMap* pMap = pProfile->GetActionMap(actionmap);
	if(pMap)
	{
		const char* key = uiKey;
		if (strstr(key, uiControlCodePrefix) == key)
			key+=uiControlCodePrefixLen;
		// else
		// 	assert(false);
		pMap->BindAction(action, key, 0);

//START workarounds
		if(strcmp("hud_show_multiplayer_scoreboard", action) == 0)
		{
			pMap->BindAction("hud_hide_multiplayer_scoreboard", key, 0);
		}
		if(strcmp("landvehicle", actionmap) == 0)
		{
			IActionMap* pSeaMap = pProfile->GetActionMap("seavehicle");
			if(pSeaMap)
			{
				pSeaMap->BindAction(action, key, 0);
			}
		}
//END workarounds

		IPlayerProfileManager::EProfileOperationResult result;
		m_pPlayerProfileManager->SaveProfile(m_pPlayerProfileManager->GetCurrentUser(), result);
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::SetCVar(const char *command, const string& value)
{
	string sCommand = command;

	//execute the console variable
	sCommand.append(" ");
	sCommand.append(value);
	gEnv->pConsole->ExecuteString(sCommand.c_str());

	//save it to profile
	g_pGame->GetOptions()->SaveCVarToProfile(sCommand, value);
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::UpdateKeyMenu()
{
	if(!m_pCurrentFlashMenuScreen)
		return;

	if(!m_pPlayerProfileManager)
		return;

	IPlayerProfile *pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
	if(!pProfile)
		return;

	IActionMapManager* pAM = gEnv->pGame->GetIGameFramework()->GetIActionMapManager();
	IActionMapIteratorPtr iter = pAM->CreateActionMapIterator();
	while (IActionMap* pMap = iter->Next())
	{
		const char* actionMapName = pMap->GetName();
		IActionMapBindInfoIteratorPtr pIter = pMap->CreateBindInfoIterator();
		while (const SActionMapBindInfo* pInfo = pIter->Next())
		{
			for (int i=0; i<pInfo->nKeys; ++i)
			{
				const char* sKey = pInfo->keys[i];
				if (*sKey)
				{
					if(strcmp(sKey,"<unknown>") != 0)
					{
						if(strncmp(sKey, "xi_", 3) != 0)
						{
							CryFixedStringT<64> ui_key (uiControlCodePrefix, uiControlCodePrefixLen);
							ui_key+=sKey;
							SFlashVarValue args[3] = {actionMapName, pInfo->action, ui_key.c_str()};
							m_pCurrentFlashMenuScreen->Invoke("Root.MainMenu.Options.setActionmapKey", args, 3);
						}
					}
				}
			}
		}
	}	
	m_pCurrentFlashMenuScreen->Invoke("Root.MainMenu.Options.updateContent");
	string sCrosshair;
	g_pGame->GetOptions()->GetProfileValue("Crosshair", sCrosshair);
	m_pCurrentFlashMenuScreen->Invoke("Root.MainMenu.Options.updateCrosshair", sCrosshair.c_str());
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::UpdateCVar(const char *command)
{
	string sCommand = command;

	//execute the console variable
	ICVar* state = gEnv->pConsole->GetCVar(sCommand);
	string sValue = state->GetString();

	if(m_pCurrentFlashMenuScreen)
	{
		SFlashVarValue args[2] = {(const char *)sCommand, (const char *)sValue};
		m_pCurrentFlashMenuScreen->Invoke("Root.MainMenu.Options.updateCVar", args, 2);
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::RestoreDefaults()
{
	IActionMapManager* pAmMgr = g_pGame->GetIGameFramework()->GetIActionMapManager();
	if (pAmMgr == 0) return;
	XmlNodeRef root = GetISystem()->LoadXmlFile("libs/config/defaultProfile.xml");
	pAmMgr->LoadFromXML(root);

	if(m_pCurrentFlashMenuScreen)
	{
		m_pCurrentFlashMenuScreen->Invoke("Root.MainMenu.Options.clearKeyChanges");
	}

	UpdateKeyMenu();

	IPlayerProfileManager::EProfileOperationResult result;
	m_pPlayerProfileManager->SaveProfile(m_pPlayerProfileManager->GetCurrentUser(), result);

}

//-----------------------------------------------------------------------------------------------------

