/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 14:08:2006   11:29 : Created by AlexL

*************************************************************************/
#include "StdAfx.h"
#include "ScriptBind_Game.h"
#include "Game.h"
#include "Menus/FlashMenuObject.h"
#include "HUD/HUD.h"
#include "Lam.h"

//------------------------------------------------------------------------
CScriptBind_Game::CScriptBind_Game(ISystem *pSystem, IGameFramework *pGameFramework)
: m_pSystem(pSystem),
	m_pSS(pSystem->GetIScriptSystem()),
	m_pGameFW(pGameFramework)
{
	Init(m_pSS, m_pSystem);
	SetGlobalName("Game");

	RegisterMethods();
	RegisterGlobals();
}

//------------------------------------------------------------------------
CScriptBind_Game::~CScriptBind_Game()
{
}

//------------------------------------------------------------------------
void CScriptBind_Game::RegisterGlobals()
{
}

//------------------------------------------------------------------------
void CScriptBind_Game::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Game::

	SCRIPT_REG_TEMPLFUNC(ShowMainMenu, "");
	SCRIPT_REG_TEMPLFUNC(ShowInGameMenu, "");
	SCRIPT_REG_TEMPLFUNC(PauseGame, "pause");
	SCRIPT_REG_TEMPLFUNC(PlayFlashAnim, "");
	SCRIPT_REG_TEMPLFUNC(PlayVideo, "");
	SCRIPT_REG_TEMPLFUNC(QueryBattleStatus, "");
	SCRIPT_REG_TEMPLFUNC(GetNumLightsActivated,"");

#undef SCRIPT_REG_CLASSNAME
}

//------------------------------------------------------------------------
int CScriptBind_Game::ShowMainMenu(IFunctionHandler *pH)
{
	CFlashMenuObject* pFMO = CFlashMenuObject::GetFlashMenuObject();
	if (pFMO)
	{
		pFMO->ShowMainMenu();
	}
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Game::ShowInGameMenu(IFunctionHandler *pH)
{
	CFlashMenuObject* pFMO = CFlashMenuObject::GetFlashMenuObject();
	if (pFMO)
	{
		pFMO->ShowInGameMenu();
	}
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Game::PauseGame( IFunctionHandler *pH, bool pause )
{
	bool forced = false;

	if (pH->GetParamCount() > 1)
	{
		pH->GetParam(2, forced);
	}
	m_pGameFW->PauseGame(pause, forced);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Game::PlayFlashAnim(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS(1);

	const char* pFlashAnim(0);
	pH->GetParam(1, pFlashAnim);
	CFlashMenuObject* pFMO(CFlashMenuObject::GetFlashMenuObject());
	if (pFlashAnim && pFMO)
		pFMO->PlayFlashAnim(pFlashAnim);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Game::PlayVideo(IFunctionHandler *pH)
{
	SCRIPT_CHECK_PARAMETERS_MIN(1);

	const char* pVideo(0);
	pH->GetParam(1, pVideo);

	int audioCh(0);
	if (pH->GetParamCount() > 1)
		pH->GetParam(2, audioCh);

	int voiceCh(-1);
	if (pH->GetParamCount() > 2)
		pH->GetParam(3, voiceCh);

	bool useSubtitles(false);
	if (pH->GetParamCount() > 3)
		pH->GetParam(4, useSubtitles);

	bool exclusiveVideo(false);
	if (pH->GetParamCount() > 4)
		pH->GetParam(5, exclusiveVideo);

	CFlashMenuObject* pFMO(CFlashMenuObject::GetFlashMenuObject());
	if (pVideo && pFMO)
		pFMO->PlayVideo(pVideo, true, 0, audioCh, voiceCh, useSubtitles, exclusiveVideo);

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Game::QueryBattleStatus(IFunctionHandler *pH)
{		
	float fStatus=0;
	if (g_pGame && g_pGame->GetHUD()) 
	{	
		CHUD *pHUD = g_pGame->GetHUD();
		fStatus=pHUD->QueryBattleStatus();		
	}
	return pH->EndFunction(fStatus);
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Game::GetNumLightsActivated(IFunctionHandler *pH)
{		
	return pH->EndFunction(CLam::GetNumLightsActivated());
}