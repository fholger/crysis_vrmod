/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Flash menu screen "Profiles"

-------------------------------------------------------------------------
History:
- 09:21:2006: Created by Jan Neugebauer

*************************************************************************/
#include "StdAfx.h"

#include "FlashMenuObject.h"
#include "FlashMenuScreen.h"
#include "IGameFramework.h"
#include "IPlayerProfiles.h"
#include "IUIDraw.h"
#include "IMusicSystem.h"
#include "ISound.h"
#include "Game.h"
#include "OptionsManager.h"
#include "HUD/GameFlashLogic.h"
#include "MPHub.h"

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::UpdateProfiles()
{
	IPlayerProfileManager *pProfileMan = g_pGame->GetOptions()->GetProfileManager();
	if(!pProfileMan)
		return;

	m_pPlayerProfileManager = pProfileMan;

	IPlayerProfileManager::EProfileOperationResult result;
	m_pPlayerProfileManager->SaveProfile(m_pPlayerProfileManager->GetCurrentUser(), result);

	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART])
	{
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("Root.MainMenu.Profile.resetProfiles");

		const char *userName = m_pPlayerProfileManager->GetCurrentUser();

		for(int i = 0; i < m_pPlayerProfileManager->GetProfileCount(userName); ++i )
		{
			IPlayerProfileManager::SProfileDescription profDesc;
			pProfileMan->GetProfileInfo(userName, i, profDesc);
			const IPlayerProfile *pProfile = m_pPlayerProfileManager->PreviewProfile(userName, profDesc.name);
			string buffer;
			if(pProfile && pProfile->GetAttribute("Singleplayer.LastSavedGame", buffer))
			{
				int pos = buffer.rfind('/');
				if(pos)
					buffer = buffer.substr(pos+1, buffer.length());
			}
			SFlashVarValue args[3] = {profDesc.name, buffer.c_str(), GetMappedProfileName(profDesc.name) };
			m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("Root.MainMenu.Profile.addProfileToList", args, 3);
		}
		m_pPlayerProfileManager->PreviewProfile(userName, NULL);

		IPlayerProfile *pProfile = pProfileMan->GetCurrentProfile(userName);
		if(pProfile)
			m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("setActiveProfile", GetMappedProfileName(pProfile->GetName()));
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::AddProfile(const char *profileName)
{
	if(m_pPlayerProfileManager)
	{
		CryFixedStringT<128> sName(profileName);
		sName = sName.Trim();

		// check for invalid chars
		static const char* invalidChars = "\\/:*?\"<>~|";
		if (sName.find_first_of(invalidChars) != CryFixedStringT<128>::npos)
		{
			if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART])
			{
				ShowMenuMessage("@ui_menu_PROFILEERROR");
			}			
			return;
		}

		const char *userName = m_pPlayerProfileManager->GetCurrentUser();
	
		IPlayerProfileManager::EProfileOperationResult result;
		bool bDone = m_pPlayerProfileManager->CreateProfile(userName,sName.c_str(), false, result);
		if(bDone)
		{
			SelectProfile(sName.c_str(), false, true);
			RestoreDefaults();

			IPlayerProfile *pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
			if(!pProfile)
				return;

			//reset to default (it's a copy of the current one)
			g_pGame->GetOptions()->SaveValueToProfile("Singleplayer.LastSavedGame", "");

			UpdateProfiles();
			if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART])
			{
				m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("Root.MainMenu.Profile.gotoProfileMenu");
				ShowMenuMessage("@ui_menu_PROFILECREATED");
			}			
		}
		else
		{
			if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART])
			{
				ShowMenuMessage("@ui_menu_PROFILEERROR");
			}			
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::SelectProfile(const char *profileName, bool silent, bool keepOldSettings)
{
	if(m_pPlayerProfileManager)
	{
		const char *userName = m_pPlayerProfileManager->GetCurrentUser();
		IPlayerProfile *oldProfile = m_pPlayerProfileManager->GetCurrentProfile(userName);
		if(oldProfile)
			SwitchProfiles(oldProfile->GetName(), profileName);
		else
			SwitchProfiles(NULL, profileName);
    g_pGame->GetIGameFramework()->GetILevelSystem()->LoadRotation();
		UpdateProfiles();
		if(keepOldSettings)
			g_pGame->GetOptions()->UpdateToProfile();
		g_pGame->GetOptions()->InitProfileOptions(true);
		g_pGame->GetOptions()->UpdateFlashOptions();
		g_pGame->GetOptions()->WriteGameCfg();
		UpdateMenuColor();
		if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART])
		{
			if(!silent)
			{
				m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("Root.MainMenu.Profile.gotoProfileMenu");
				ShowMenuMessage("@ui_menu_PROFILELOADED");
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::SwitchProfiles(const char *oldProfile, const char *newProfile)
{
	const char *userName = m_pPlayerProfileManager->GetCurrentUser();
	if(oldProfile)
	{
		m_pPlayerProfileManager->ActivateProfile(userName,oldProfile);
		
		if(m_multiplayerMenu)
			m_multiplayerMenu->DoLogoff();
		
		g_pGame->GetOptions()->SaveValueToProfile("Activated", 0);
		g_pGame->GetOptions()->SaveProfile();
		
	}
	if(newProfile)
	{
		m_pPlayerProfileManager->ActivateProfile(userName,newProfile);
		g_pGame->GetOptions()->SaveValueToProfile("Activated", 1);
		g_pGame->GetOptions()->SaveProfile();
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::SelectActiveProfile()
{

	IPlayerProfileManager *pMan = g_pGame->GetOptions()->GetProfileManager();
	if(!pMan) return;

	const char *userName = pMan->GetCurrentUser();

	for(int i = 0; i < pMan->GetProfileCount(userName); ++i )
	{
		IPlayerProfileManager::SProfileDescription profDesc;
		pMan->GetProfileInfo(userName, i, profDesc);
		const IPlayerProfile *preview = pMan->PreviewProfile(userName, profDesc.name);
		int iActive = 0;
		if(preview)
		{
			preview->GetAttribute("Activated",iActive);
		}
		if(iActive>0)
		{
			pMan->ActivateProfile(userName,profDesc.name);
			break;
		}
	}
	pMan->PreviewProfile(userName,NULL);
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::DeleteProfile(const char *profileName)
{
	if(!m_pPlayerProfileManager)
		return;
	const char *userName = m_pPlayerProfileManager->GetCurrentUser();
	IPlayerProfileManager::EProfileOperationResult result;
	m_pPlayerProfileManager->DeleteProfile(userName, profileName, result);
	
	IPlayerProfile *pCurProfile = m_pPlayerProfileManager->GetCurrentProfile(userName);
	if(!pCurProfile)
	{
		IPlayerProfileManager::SProfileDescription profDesc;
		m_pPlayerProfileManager->GetProfileInfo(userName, 0, profDesc);
		SelectProfile(profDesc.name, true);
		ShowMenuMessage("@ui_menu_PROFILEDELETED");
	}

	g_pGame->GetOptions()->InitProfileOptions(true);
	g_pGame->GetOptions()->UpdateFlashOptions();
	UpdateProfiles();
	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART])
	{
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("Root.MainMenu.Profile.updateProfileList");
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::SetProfile()
{
	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated()) return;

	SelectActiveProfile();
	g_pGame->GetOptions()->InitProfileOptions();
	g_pGame->GetOptions()->UpdateFlashOptions();
	UpdateProfiles();
	UpdateMenuColor();
}

//-----------------------------------------------------------------------------------------------------

const wchar_t* CFlashMenuObject::GetMappedProfileName(const char* profileName)
{
	if (stricmp(profileName, "default") == 0)
		gEnv->pSystem->GetLocalizationManager()->LocalizeLabel("@ui_DefaultProfileName", m_tempMappedProfileName);
	else
		gEnv->pSystem->GetLocalizationManager()->LocalizeString(profileName, m_tempMappedProfileName);
	return m_tempMappedProfileName.c_str();
}

