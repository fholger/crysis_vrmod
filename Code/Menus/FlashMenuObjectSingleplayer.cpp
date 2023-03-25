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
#include "IRenderer.h"
#include "Game.h"
#include "Menus/OptionsManager.h"
#include <time.h>

enum EDifficulty
{
	EDifficulty_EASY,
	EDifficulty_NORMAL,
	EDifficulty_REALISTIC,
	EDifficulty_DELTA,
	EDifficulty_END
};


//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::UpdateSingleplayerDifficulties()
{
	if(!m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART])
		return;

	if(!m_pPlayerProfileManager)
		return;

	IPlayerProfile *pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
	if(!pProfile)
		return;

	string sGeneralPath = "Singleplayer.Difficulty";
	int iDifficulties = 8;
/*	for(int i=0; i<EDifficulty_END; ++i)
	{
		string sPath = sGeneralPath;
		char c[5];
		itoa(i, c, 10);
		sPath.append(c);
		sPath.append(".available");

		TFlowInputData data;
		pProfile->GetAttribute(sPath, data, false);
		bool bDone = false;
		data.GetValueWithConversion(bDone);
		if(bDone)
		{
			iDifficulties += i*2;
		}
	}
*/
	int iDifficultiesDone = 0;
	for(int i=0; i<EDifficulty_END; ++i)
	{
		string sPath = sGeneralPath;
		char c[5];
		itoa(i, c, 10);
		sPath.append(c);
		sPath.append(".done");

		TFlowInputData data;
		pProfile->GetAttribute(sPath, data, false);
		bool bDone = false;
		data.GetValueWithConversion(bDone);
		if(bDone)
		{
			iDifficultiesDone += std::max(i*2,1);
		}
	}

	TFlowInputData data;
	pProfile->GetAttribute("Singleplayer.LastSelectedDifficulty", data, false);
	int iDiff = 2;
	data.GetValueWithConversion(iDiff);

	if(iDiff<=0)
	{
		iDiff = 2;
	}

	m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("Root.MainMenu.SinglePlayer.enableDifficulties", iDifficulties);
	m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("Root.MainMenu.SinglePlayer.enableDifficultiesStats", iDifficultiesDone);
	m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("Root.MainMenu.SinglePlayer.selectDifficulty", iDiff);
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::StartSingleplayerGame(const char *strDifficulty)
{
	int iDifficulty = 0;
	if(!strcmp(strDifficulty,"Easy"))
	{
		iDifficulty = 1;
	}
	else if(!strcmp(strDifficulty,"Normal"))
	{
		iDifficulty = 2;
	}
	else if(!strcmp(strDifficulty,"Realistic"))
	{
		iDifficulty = 3;
	}
	else if(!strcmp(strDifficulty,"Delta"))
	{
		iDifficulty = 4;
	}

	// load configuration from disk
	if (iDifficulty != 0)
		LoadDifficultyConfig(iDifficulty);

	if(m_pPlayerProfileManager)
	{
		IPlayerProfile *pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
		if(pProfile)
		{
			pProfile->SetAttribute("Singleplayer.LastSelectedDifficulty",(TFlowInputData)iDifficulty);
			IPlayerProfileManager::EProfileOperationResult result;
			m_pPlayerProfileManager->SaveProfile(m_pPlayerProfileManager->GetCurrentUser(), result);
		}
	}
	StopVideo();
	m_bDestroyStartMenuPending = true;
	m_stateEntryMovies = eEMS_GameStart;
	if(m_pMusicSystem)
		m_pMusicSystem->EndTheme(EThemeFade_StopAtOnce, 0, true);
	PlaySound(ESound_MenuAmbience,false);
}

ILINE void expandSeconds(int secs, int& days, int& hours, int& minutes, int& seconds)
{
	days  = secs / 86400;
	secs -= days * 86400;
	hours = secs / 3600;
	secs -= hours * 3600;
	minutes = secs / 60;
	seconds = secs - minutes * 60;
}

void secondsToString(int secs, wstring& outString)
{
	int d,h,m,s;
	expandSeconds(secs, d, h, m, s);
	string str;
	if (d > 1)
		str.Format("%d @ui_days %02d:%02d:%02d", d, h, m, s);
	else if (d > 0)
		str.Format("%d @ui_day %02d:%02d:%02d", d, h, m, s);
	else if (h > 0)
		str.Format("%02d:%02d:%02d", h, m, s);
	else
		str.Format("%02d:%02d", m, s);
	gEnv->pSystem->GetLocalizationManager()->LocalizeString(str, outString);
}

void CFlashMenuObject::UpdateSaveGames()
{
	CFlashMenuScreen* pScreen = m_pCurrentFlashMenuScreen;
	if(!pScreen)
		return;

	//*************************************************************************

	std::vector<SaveGameMetaData> saveGameData;
	
	//*************************************************************************

	//get current mod info
	SModInfo info;
	string modName, modVersion;
	if(g_pGame->GetIGameFramework()->GetModInfo(&info))
	{
		modName = info.m_name;
		modVersion = info.m_version;
	}

	pScreen->CheckedInvoke("resetSPGames");

	// TODO: find a better place for this as it needs to be set only once -- CW
	gEnv->pSystem->SetFlashLoadMovieHandler(this);

	if(!m_pPlayerProfileManager)
		return;

	IPlayerProfile *pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
	if(!pProfile)
		return;
 
	ILocalizationManager* pLocMgr = gEnv->pSystem->GetLocalizationManager();
	ISaveGameEnumeratorPtr pSGE = pProfile->CreateSaveGameEnumerator();
	ISaveGameEnumerator::SGameDescription desc;	

	//get the meta data into the struct
	for (int i=0; i<pSGE->GetCount(); ++i)
	{
		pSGE->GetDescription(i, desc);

		//check mod version
		const char *tempModName = desc.metaData.xmlMetaDataNode->getAttr("ModName");
		const char *tempModVersion = desc.metaData.xmlMetaDataNode->getAttr("ModVersion");
		if(tempModName && tempModVersion )
		{
			if(strcmp(modName.c_str(),tempModName) || strcmp(modVersion.c_str(),tempModVersion))
				continue;
		}

		int kills = 0;
		float levelPlayTimeSec = 0.0f;
		float gamePlayTimeSec = 0.0f;
		int difficulty = g_pGameCVars->g_difficultyLevel;
		desc.metaData.xmlMetaDataNode->getAttr("sp_kills", kills);
		desc.metaData.xmlMetaDataNode->getAttr("sp_levelPlayTime", levelPlayTimeSec);
		desc.metaData.xmlMetaDataNode->getAttr("sp_gamePlayTime", gamePlayTimeSec);
		desc.metaData.xmlMetaDataNode->getAttr("sp_difficulty", difficulty);

		SaveGameMetaData data;
		data.name = desc.name;
		data.buildVersion = desc.metaData.buildVersion;
		data.description = desc.description;
		data.fileVersion = desc.metaData.fileVersion;
		data.gamePlayTimeSec = gamePlayTimeSec;
		data.gameRules = desc.metaData.gameRules;
		data.humanName = desc.humanName;
		data.levelName = g_pGame->GetMappedLevelName(desc.metaData.levelName);
		data.levelPlayTimeSec = levelPlayTimeSec;
		data.saveTime = desc.metaData.saveTime;
		data.kills = kills;
		data.difficulty = difficulty;
		saveGameData.push_back(data);
	}

	if(saveGameData.size())
	{
		//sort by the set sorting rules
		std::sort(saveGameData.begin(), saveGameData.end(), SaveGameDataCompare(m_eSaveGameCompareMode));

		//send sorted data to flash
		int start = (m_bSaveGameSortUp)?0:saveGameData.size()-1;
		int end = (m_bSaveGameSortUp)?saveGameData.size():-1;
		int inc = (m_bSaveGameSortUp)?1:-1;
		for(int i = start; i != end; i+=inc)
		{
			SaveGameMetaData data = saveGameData[i];

			wstring levelPlayTimeString;
			pLocMgr->LocalizeDuration((int)data.levelPlayTimeSec, levelPlayTimeString);

			wstring gamePlayTimeSecString;
			pLocMgr->LocalizeDuration((int)data.gamePlayTimeSec, gamePlayTimeSecString);

			wstring dateString;
			pLocMgr->LocalizeDate(data.saveTime, true, true, true, dateString);

			wstring timeString;
			pLocMgr->LocalizeTime(data.saveTime, true, false, timeString);

			dateString+=L" ";
			dateString+=timeString;

			bool levelStart = (ValidateName(data.name))?true:false;

			SFlashVarValue args[12] =
			{
				data.name, 
				data.description, 
				data.humanName, 
				data.levelName, 
				data.gameRules, 
				data.fileVersion, 
				data.buildVersion, 
				levelPlayTimeString.c_str(),
				dateString.c_str(),
				levelStart,
				data.kills,
				data.difficulty
			};		
			pScreen->CheckedInvoke("addGameToList", args, sizeof(args) / sizeof(args[0]));
		}
	}

	pScreen->CheckedInvoke("updateGameList");
}

void CFlashMenuObject::LoadGame(const char *fileName)
{
	//overwrite the last savegame with the to be loaded one
	m_sLastSaveGame = fileName;
	gEnv->pGame->GetIGameFramework()->LoadGame(fileName, false, true);
}

void CFlashMenuObject::DeleteSaveGame(const char *fileName)
{

	const char *reason = ValidateName(fileName);
	if(reason)
	{
		ShowMenuMessage(reason);
		return;
	}
	else
	{
		if(!m_pPlayerProfileManager)
			return;

		IPlayerProfile *pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
		if(!pProfile)
			return;
		pProfile->DeleteSaveGame(fileName);
		UpdateSaveGames();
	}
}

bool CFlashMenuObject::SaveGame(const char *fileName)
{
	string sSaveFileName = fileName;
	const char *reason = ValidateName(fileName);
	if(reason)
	{
		ShowMenuMessage(reason);
		return false;
	}
	else
	{
		IActor *pActor = g_pGame->GetIGameFramework()->GetClientActor();
		if(pActor && pActor->GetHealth() <= 0)
		{
			ShowMenuMessage("@ui_dead_no_save");
			return false;
		}

		sSaveFileName.append(".CRYSISJMSF");
		const bool bSuccess =	gEnv->pGame->GetIGameFramework()->SaveGame(sSaveFileName,true, true, eSGR_QuickSave, true);
		if (!bSuccess)
			return false;

		UpdateSaveGames();
	}
	return true;
}

const char* CFlashMenuObject::ValidateName(const char *fileName)
{
	string sFileName(fileName);
	int index = sFileName.rfind('.');
	if(index>=0)
	{
		sFileName = sFileName.substr(0,index);
	}
	index = sFileName.rfind('_');
	if(index>=0)
	{
		string check(sFileName.substr(index+1,sFileName.length()-(index+1)));
		//if(!stricmp(check, "levelstart")) //because of the french law we can't do this ...
		if(!stricmp(check, "crysis"))
			return "@ui_error_levelstart";
	}
	return NULL;
}

void CFlashMenuObject::UpdateMods()
{
	string dir("Mods\\");
	string search = dir;
	search += "*.*";

	SModInfo info;
	string currentMod;
	bool currentModExists = g_pGame->GetIGameFramework()->GetModInfo(&info);
	if(currentModExists)
		currentMod = info.m_name;

	m_pCurrentFlashMenuScreen->Invoke("Root.MainMenu.Mods.resetMods");

	ICryPak *pPak = gEnv->pSystem->GetIPak();
	_finddata_t fd;
	intptr_t handle = pPak->FindFirst(search.c_str(), &fd, ICryPak::FLAGS_NO_MASTER_FOLDER_MAPPING);
	if (handle > -1)
	{
		do
		{
			if(fd.attrib & _A_SUBDIR)
			{
				if(!stricmp("..",fd.name)) continue;
				string subDir(dir);
				subDir.append(fd.name);
				subDir += "\\";
				if(g_pGame->GetIGameFramework()->GetModInfo(&info, subDir.c_str()))
				{
					string screenshot = subDir;
					screenshot += "\\";
					screenshot += info.m_screenshot;
					bool isCurrent = false;
					if(currentModExists)
						isCurrent = (!strcmp(info.m_name,currentMod.c_str()))?true:false;
					SFlashVarValue args[8] = {info.m_name, info.m_name, info.m_name, info.m_version, info.m_url, info.m_description, screenshot.c_str(), isCurrent};
					m_pCurrentFlashMenuScreen->Invoke("Root.MainMenu.Mods.addModToList",args, 8);
				}
			}
		}
		while (pPak->FindNext(handle, &fd) >= 0);
	}
}
