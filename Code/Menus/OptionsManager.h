/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Handles options setting, getting, saving and loading
between profile, system and flash.

-------------------------------------------------------------------------
History:
- 03/2007: Created by Jan Müller

*************************************************************************/
#ifndef __OPTIONS_MANAGER_H__
#define __OPTIONS_MANAGER_H__

#pragma once

#include "IGameFramework.h"
#include "GameCVars.h"

//-----------------------------------------------------------------------------------------------------

class CGameFlashAnimation;
class CFlashMenuScreen;

//-----------------------------------------------------------------------------------------------------

enum ECrysisProfileColor
{
	CrysisProfileColor_Amber,
	CrysisProfileColor_Blue,
	CrysisProfileColor_Green,
	CrysisProfileColor_Red,
	CrysisProfileColor_White,
	CrysisProfileColor_Default = CrysisProfileColor_Green
};

//-----------------------------------------------------------------------------------------------------

class COptionsManager : public ICVarDumpSink
{

	typedef void (COptionsManager::*OpFuncPtr) (const char*);

public :

	~COptionsManager() 
	{
		sp_optionsManager = 0;
	};

	static COptionsManager* CreateOptionsManager()
	{
		if(!sp_optionsManager)
			sp_optionsManager = new COptionsManager();
		return sp_optionsManager;
	}

	ILINE IPlayerProfileManager *GetProfileManager() { return m_pPlayerProfileManager; }
	ILINE bool IsFirstStart() { return m_firstStart; }

	ECrysisProfileColor GetCrysisProfileColor();

	//ICVarDumpSink
	virtual void OnElementFound(ICVar *pCVar);
	//~ICVarDumpSink

	//flash system
	bool HandleFSCommand(const char *strCommand,const char *strArgs);
	void UpdateFlashOptions();
	void InitProfileOptions(bool switchProfiles = false);
	void UpdateToProfile();
	void ResetDefaults(const char* option);
	//flash system

	//profile system
	void SetProfileManager(IPlayerProfileManager* pProfileManager);
	void SaveCVarToProfile(const char* key, const string& value);
	bool GetProfileValue(const char* key, bool &value);
	bool GetProfileValue(const char* key, int &value);
	bool GetProfileValue(const char* key, float &value);
	bool GetProfileValue(const char* key, string &value);
	void SaveValueToProfile(const char* key, bool value);
	void SaveValueToProfile(const char* key, int value);
	void SaveValueToProfile(const char* key, float value);
	void SaveValueToProfile(const char* key, const string& value);
	void SaveProfile();
	const char* GetProfileName();
	void CVarToProfile();
	void ProfileToCVar();
	bool IgnoreProfile();
	bool WriteGameCfg();
	void SetVideoMode(const char* params);
	void AutoDetectHardware(const char* params);
	void SystemConfigChanged(bool silent = false);
	//~profile system

private:
	struct CCVarSink : public ICVarDumpSink
	{
		CCVarSink(COptionsManager* pMgr, FILE* pFile)
		{
			m_pOptionsManager = pMgr;
			m_pFile = pFile;
		}

		void OnElementFound(ICVar *pCVar);
		COptionsManager* m_pOptionsManager;
		FILE* m_pFile;
	};

	COptionsManager();
	IPlayerProfileManager* m_pPlayerProfileManager;

	struct SOptionEntry
	{
		string name;
		bool   bWriteToConfig;

		SOptionEntry() : bWriteToConfig(false) {}
		SOptionEntry(const string& name, bool bWriteToConfig) : name(name), bWriteToConfig(bWriteToConfig) {}
		SOptionEntry(const char* name, bool bWriteToConfig) : name(name), bWriteToConfig(bWriteToConfig) {}
	};

	std::map<string, SOptionEntry> m_profileOptions;

	ECrysisProfileColor m_eCrysisProfileColor;

	//************ OPTION FUNCTIONS ************
	void SetAntiAliasingMode(const char* params);
	void SetDifficulty(const char* params);
	void PBClient(const char* params);
	//initialize option-functions
	void InitOpFuncMap();
	//option-function mapper
	typedef std::map<string, OpFuncPtr> TOpFuncMap;
	TOpFuncMap	m_opFuncMap;
	typedef TOpFuncMap::iterator TOpFuncMapIt;
	//******************************************

	void SetCrysisProfileColor(const char *szValue);
	CFlashMenuScreen * GetCurrentMenu();

	static COptionsManager* sp_optionsManager;

	const char *m_defaultColorLine;
	const char *m_defaultColorOver;
	const char *m_defaultColorText;
	bool m_pbEnabled;
	bool m_firstStart;

};

//-----------------------------------------------------------------------------------------------------

#endif

//-----------------------------------------------------------------------------------------------------