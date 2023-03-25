/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Create Game screen

-------------------------------------------------------------------------
History:
- 03/12/2006: Created by Stas Spivakov

*************************************************************************/
#ifndef __CREATEGAME_H__ 
#define __CREATEGAME_H__

#pragma once

class CMPHub;

struct CMultiPlayerMenu::SCreateGame
{
  SCreateGame(IFlashPlayer* plr, CMPHub* hub, bool lan);
  
  string  m_name;
  string  m_password;
  int     m_maxplayers;
  int     m_spectators;
  bool    m_friendlyFire;
  int     m_friendlyDamageRatio;
  bool    m_enableKTK;
  int     m_numKTKKills;
  bool    m_VoIP;
  bool    m_autoTeamBalance;
  bool    m_controllersOnly;
  bool    m_randomize;
  bool    m_anticheat;
  bool    m_dedicated;
  bool    m_dx10;
  bool    m_TOD;
  float   m_TODStart;
  float   m_TODSpeed;
  bool    m_battleDust;
	bool		m_lan;
	
	CMPHub*	m_hub;
	
  IFlashPlayer* m_player;

  bool HandleFSCommand(EGsUiCommand cmd, const char* pArgs);
  void StartServer();
  void GetLevelRotation();
	void GetGlobalSettings();
	void SetGlobalSettings();
  void UpdateLevels(const char* gamemode);
  void StartDedicated(const char* params);
};

#endif //__CREATEGAME_H__