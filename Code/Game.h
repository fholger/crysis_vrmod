/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 3:8:2004   11:23 : Created by Márcio Martins

*************************************************************************/
#ifndef __GAME_H__
#define __GAME_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <IGame.h>
#include <IGameFramework.h>
#include <IGameObjectSystem.h>
#include <IGameObject.h>
#include <IActorSystem.h>
#include <StlUtils.h>
#include "ClientSynchedStorage.h"
#include "ServerSynchedStorage.h"
#include "Cry_Camera.h"

#define GAME_NAME				"Crysis"
#define GAME_LONGNAME		"Crysis"


struct ISystem;
struct IConsole;
struct ILCD;

class	CScriptBind_Actor;
class CScriptBind_Item;
class CScriptBind_Weapon;
class CScriptBind_GameRules;
class CScriptBind_Game;
class CScriptBind_HUD;
class CWeaponSystem;
class CFlashMenuObject;
class COptionsManager;

struct IActionMap;
struct IActionFilter;
class  CGameActions;
class CGameRules;
class CBulletTime;
class CHUD;
class CSynchedStorage;
class CClientSynchedStorage;
class CServerSynchedStorage;
struct SCVars;
struct SItemStrings;
class CItemSharedParamsList;
class CSPAnalyst;
class CSoundMoods;
class CLaptopUtil;
class CLCDWrapper;

// when you add stuff here, also update in CGame::RegisterGameObjectEvents
enum ECryGameEvent
{
	eCGE_PreFreeze = eGFE_PreFreeze,	// this is really bad and must be fixed
	eCGE_PreShatter = eGFE_PreShatter,
	eCGE_PostFreeze = 256,
	eCGE_PostShatter,
	eCGE_OnShoot,
	eCGE_Recoil, 
	eCGE_BeginReloadLoop,
	eCGE_EndReloadLoop,
	eCGE_ActorRevive,
	eCGE_VehicleDestroyed,
	eCGE_TurnRagdoll,
	eCGE_EnableFallAndPlay,
	eCGE_DisableFallAndPlay,
	eCGE_VehicleTransitionEnter,
	eCGE_VehicleTransitionExit,
	eCGE_HUD_PDAMessage,
	eCGE_HUD_TextMessage,
	eCGE_TextArea,
	eCGE_HUD_Break,
	eCGE_HUD_Reboot,
	eCGE_InitiateAutoDestruction,
	eCGE_Event_Collapsing,
	eCGE_Event_Collapsed,
	eCGE_MultiplayerChatMessage,
	eCGE_ResetMovementController,
	eCGE_AnimateHands,
	eCGE_Ragdoll,
	eCGE_EnablePhysicalCollider,
	eCGE_DisablePhysicalCollider,
	eCGE_RebindAnimGraphInputs,
	eCGE_OpenParachute,
  eCGE_Turret_LockedTarget,
  eCGE_Turret_LostTarget,
};

static const int GLOBAL_SERVER_IP_KEY						=	1000;
static const int GLOBAL_SERVER_PUBLIC_PORT_KEY	= 1001;
static const int GLOBAL_SERVER_NAME_KEY					=	1002;

class CGame :
  public IGame, public IGameFrameworkListener
{
public:
  typedef bool (*BlockingConditionFunction)();
public:
	CGame();
	virtual ~CGame();

	// IGame
	virtual bool  Init(IGameFramework *pFramework);
	virtual bool  CompleteInit();
	virtual void  Shutdown();
	virtual int   Update(bool haveFocus, unsigned int updateFlags);
	virtual void  ConfigureGameChannel(bool isServer, IProtocolBuilder *pBuilder);
	virtual void  EditorResetGame(bool bStart);
	virtual void  PlayerIdSet(EntityId playerId);
	virtual string  InitMapReloading();
	virtual bool IsReloading() { return m_bReload; }
	virtual IGameFramework *GetIGameFramework() { return m_pFramework; }

	virtual const char *GetLongName();
	virtual const char *GetName();

	virtual void GetMemoryStatistics(ICrySizer * s);

	virtual void OnClearPlayerIds();
	//auto-generated save game file name
	virtual const char* CreateSaveGameName();
	//level names were renamed without changing the file/directory
	virtual const char* GetMappedLevelName(const char *levelName) const;
	// ~IGame

  // IGameFrameworkListener
  virtual void OnPostUpdate(float fDeltaTime);
  virtual void OnSaveGame(ISaveGame* pSaveGame);
  virtual void OnLoadGame(ILoadGame* pLoadGame);
	virtual void OnLevelEnd(const char* nextLevel) {};
  virtual void OnActionEvent(const SActionEvent& event);
  // ~IGameFrameworkListener

  void BlockingProcess(BlockingConditionFunction f);
  void GameChannelDestroyed(bool isServer);
  void DestroyHUD();

	virtual CScriptBind_Actor *GetActorScriptBind() { return m_pScriptBindActor; }
	virtual CScriptBind_Item *GetItemScriptBind() { return m_pScriptBindItem; }
	virtual CScriptBind_Weapon *GetWeaponScriptBind() { return m_pScriptBindWeapon; }
	virtual CScriptBind_GameRules *GetGameRulesScriptBind() { return m_pScriptBindGameRules; }
	virtual CScriptBind_HUD *GetHUDScriptBind() { return m_pScriptBindHUD; }
	virtual CWeaponSystem *GetWeaponSystem() { return m_pWeaponSystem; };
	virtual CItemSharedParamsList *GetItemSharedParamsList() { return m_pItemSharedParamsList; };

	CGameActions&	Actions() const {	return *m_pGameActions;	};

	CGameRules *GetGameRules() const;
	CBulletTime *GetBulletTime() const;
	CSoundMoods *GetSoundMoods() const;
	CLaptopUtil *GetLaptopUtil() const;
	CHUD *GetHUD() const;
	CFlashMenuObject *GetMenu() const;
	COptionsManager *GetOptions() const;

	ILINE CSynchedStorage *GetSynchedStorage() const
	{
		if (m_pServerSynchedStorage && gEnv->bServer)
			return m_pServerSynchedStorage;

		return m_pClientSynchedStorage;
	}

	ILINE CServerSynchedStorage *GetServerSynchedStorage() const
	{
		return m_pServerSynchedStorage;
	}

	CSPAnalyst* GetSPAnalyst() const { return m_pSPAnalyst; }

	const string& GetLastSaveGame(string &levelName);
	const string& GetLastSaveGame() { string tmp; return GetLastSaveGame(tmp); }

  ILINE SCVars *GetCVars() {return m_pCVars;}
	static void DumpMemInfo(const char* format, ...) PRINTF_PARAMS(1, 2);

protected:
	virtual void LoadActionMaps(const char* filename = "libs/config/defaultProfile.xml");

	virtual void ReleaseActionMaps();

	virtual void InitScriptBinds();
	virtual void ReleaseScriptBinds();

	virtual void CheckReloadLevel();

	// These funcs live in GameCVars.cpp
	virtual void RegisterConsoleVars();
	virtual void RegisterConsoleCommands();
	virtual void UnregisterConsoleCommands();

	virtual void RegisterGameObjectEvents();

	// marcok: this is bad and evil ... should be removed soon
	static void CmdRestartGame(IConsoleCmdArgs *pArgs);

	static void CmdDumpSS(IConsoleCmdArgs *pArgs);

	static void CmdLastInv(IConsoleCmdArgs *pArgs);
	static void CmdName(IConsoleCmdArgs *pArgs);
	static void CmdTeam(IConsoleCmdArgs *pArgs);
	static void CmdLoadLastSave(IConsoleCmdArgs *pArgs);
	static void CmdSpectator(IConsoleCmdArgs *pArgs);
	static void CmdJoinGame(IConsoleCmdArgs *pArgs);
	static void CmdKill(IConsoleCmdArgs *pArgs);
  static void CmdVehicleKill(IConsoleCmdArgs *pArgs);
	static void CmdRestart(IConsoleCmdArgs *pArgs);
	static void CmdSay(IConsoleCmdArgs *pArgs);
	static void CmdReloadItems(IConsoleCmdArgs *pArgs);
	static void CmdLoadActionmap(IConsoleCmdArgs *pArgs);
  static void CmdReloadGameRules(IConsoleCmdArgs *pArgs);
  static void CmdNextLevel(IConsoleCmdArgs* pArgs);
  static void CmdStartKickVoting(IConsoleCmdArgs* pArgs);
  static void CmdStartNextMapVoting(IConsoleCmdArgs* pArgs);
  static void CmdVote(IConsoleCmdArgs* pArgs);
	static void CmdListPlayers(IConsoleCmdArgs* pArgs);

  static void CmdQuickGame(IConsoleCmdArgs* pArgs);
  static void CmdQuickGameStop(IConsoleCmdArgs* pArgs);
  static void CmdBattleDustReload(IConsoleCmdArgs* pArgs);
  static void CmdLogin(IConsoleCmdArgs* pArgs);
	static void CmdLoginProfile(IConsoleCmdArgs* pArgs);
	static void CmdRegisterNick(IConsoleCmdArgs* pArgs);
  static void CmdCryNetConnect(IConsoleCmdArgs* pArgs);

	IGameFramework			*m_pFramework;
	IConsole						*m_pConsole;

	CWeaponSystem				*m_pWeaponSystem;

	bool								m_bReload;

	// script binds
	CScriptBind_Actor		*m_pScriptBindActor;
	CScriptBind_Item		*m_pScriptBindItem;
	CScriptBind_Weapon	*m_pScriptBindWeapon;
	CScriptBind_GameRules*m_pScriptBindGameRules;
	CScriptBind_Game    *m_pScriptBindGame;
	CScriptBind_HUD     *m_pScriptBindHUD;

	//menus
	CFlashMenuObject		*m_pFlashMenuObject;
	COptionsManager			*m_pOptionsManager;

	IActionMap					*m_pDebugAM;
	IActionMap					*m_pDefaultAM;
	IActionMap					*m_pMultiplayerAM;
	CGameActions				*m_pGameActions;	
	IPlayerProfileManager* m_pPlayerProfileManager;
	CHUD								*m_pHUD;

	CServerSynchedStorage	*m_pServerSynchedStorage;
	CClientSynchedStorage	*m_pClientSynchedStorage;
	CSPAnalyst          *m_pSPAnalyst;
	bool								m_inDevMode;

	EntityId m_uiPlayerID;

	SCVars*	m_pCVars;
	SItemStrings					*m_pItemStrings;
	CItemSharedParamsList *m_pItemSharedParamsList;
	string                 m_lastSaveGame;
	string								 m_newSaveGame;

	CBulletTime						*m_pBulletTime;
	CSoundMoods						*m_pSoundMoods;
	CLaptopUtil						*m_pLaptopUtil;
	ILCD									*m_pLCD;

	typedef std::map<string, string, stl::less_stricmp<string> > TLevelMapMap;
	TLevelMapMap m_mapNames;
};

extern CGame *g_pGame;

#define SAFE_HARDWARE_MOUSE_FUNC(func)\
	if(gEnv->pHardwareMouse)\
		gEnv->pHardwareMouse->func

#define SAFE_MENU_FUNC(func)\
	{	if(g_pGame && g_pGame->GetMenu()) g_pGame->GetMenu()->func; }

#define SAFE_MENU_FUNC_RET(func)\
	((g_pGame && g_pGame->GetMenu()) ? g_pGame->GetMenu()->func : NULL)

#define SAFE_HUD_FUNC(func)\
	{	if(g_pGame && g_pGame->GetHUD()) g_pGame->GetHUD()->func; }

#define SAFE_HUD_FUNC_RET(func)\
	((g_pGame && g_pGame->GetHUD()) ? g_pGame->GetHUD()->func : NULL)

#define SAFE_LAPTOPUTIL_FUNC(func)\
	{	if(g_pGame && g_pGame->GetLaptopUtil()) g_pGame->GetLaptopUtil()->func; }

#define SAFE_LAPTOPUTIL_FUNC_RET(func)\
	((g_pGame && g_pGame->GetLaptopUtil()) ? g_pGame->GetLaptopUtil()->func : NULL)

#define SAFE_SOUNDMOODS_FUNC(func)\
	{	if(g_pGame && g_pGame->GetSoundMoods()) g_pGame->GetSoundMoods()->func; }

#endif //__GAME_H__