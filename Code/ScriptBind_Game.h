/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Script Binding for Game

-------------------------------------------------------------------------
History:
- 14:08:2006   11:30 : Created by AlexL
*************************************************************************/
#ifndef __SCRIPTBIND_GAME_H__
#define __SCRIPTBIND_GAME_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <IScriptSystem.h>
#include <ScriptHelpers.h>

struct IGameFramework;
struct ISystem;

class CScriptBind_Game :
	public CScriptableBase
{
public:
	CScriptBind_Game(ISystem *pSystem, IGameFramework *pGameFramework);
	virtual ~CScriptBind_Game();

protected:
	int ShowMainMenu(IFunctionHandler *pH);
	int ShowInGameMenu(IFunctionHandler *pH);
	int PauseGame(IFunctionHandler *pH, bool pause);
	int PlayFlashAnim(IFunctionHandler *pH);
	int PlayVideo(IFunctionHandler *pH);
	//!	Queries battle status, range from 0 (quiet) to 1 (full combat)
	int	QueryBattleStatus(IFunctionHandler *pH);
	int GetNumLightsActivated(IFunctionHandler *pH);

private:
	void RegisterGlobals();
	void RegisterMethods();

	ISystem						*m_pSystem;
	IScriptSystem			*m_pSS;
	IGameFramework		*m_pGameFW;
};

#endif //__SCRIPTBIND_GAME_H__