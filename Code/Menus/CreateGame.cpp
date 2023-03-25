/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Create Game Screen

-------------------------------------------------------------------------
History:
- 03/12/2006: Created by Stas Spivakov

*************************************************************************/
#include "StdAfx.h"
#include "MPHub.h"
#include "MPLobbyUI.h"
#include "MultiplayerMenu.h"
#include "CreateGame.h"

#include "IGame.h"
#include "IGameFramework.h"
#include "ILevelSystem.h"
#include "IPlayerProfiles.h"

enum ECreateGameOptions
{
  eCGO_none,
  eCGO_name,
  eCGO_password,
  eCGO_maxplayers,
  eCGO_spectators,
  eCGO_friendlyFire,
  eCGO_friendlyDamageRatio,
  eCGO_enableKTK,//KTK = kick team killers
  eCGO_numKTKKills,
  eCGO_VoIP,
  eCGO_autoTeamBalance,
  eCGO_controllersOnly,
  eCGO_randomize,
  eCGO_anticheat,
  eCGO_dedicated,
  eCGO_dx10,
  eCGO_TOD,//TOD = time of day
  eCGO_TODStart,
  eCGO_TODSpeed,
  eCGO_battleDust
};

static TKeyValuePair<ECreateGameOptions,const char*>
gCreateGameOptions[] = {
  {eCGO_none,""},
  {eCGO_name,"ServerName"},
  {eCGO_password,"ServerPassword"},
  {eCGO_maxplayers,"MaximumPlayers"},
  {eCGO_spectators,"AllowSpectators"},
  {eCGO_friendlyFire,"ch_FriendlyFire"},
  {eCGO_friendlyDamageRatio,"FriendlyFireDamageRatio"},
  {eCGO_enableKTK,"ch_KickTeamKiller"},
  {eCGO_numKTKKills,"KickTeamKillerKills"},
  {eCGO_VoIP,"ch_VoiceCommunication"},
  {eCGO_autoTeamBalance,"ch_AutoTeamBalance"},
  {eCGO_controllersOnly,"ch_ControllersOnly"},
  {eCGO_randomize,"ch_Randomize"},
  {eCGO_anticheat,"ch_AntiCheat"},
  {eCGO_dedicated,"ch_DedicatedServer"},
  {eCGO_dx10,"ch_EnableDX10"},
  {eCGO_TOD,"ch_EnableTOD"},
  {eCGO_TODStart,"TODStart"},
  {eCGO_TODSpeed,"TODSpeed"},
  {eCGO_battleDust,"ch_EnableBattleDust"}
};

enum EMapOptions
{
  eMO_none,
  eMO_timelimit,
  eMO_timelimitMinutes,
  eMO_killlimit,
  eMO_killlimitKills,
  eMO_respawnTime,
  eMO_fastEconomy,
	eMO_roundtime,
	eMO_roundlimit,
	eMO_preroundtime,
	eMO_suddendeath_time
};

static TKeyValuePair<EMapOptions,const char*>
gMapOptions[] = {
  {eMO_none,""},
  {eMO_timelimit,"ch_TimeLimit"},
  {eMO_timelimitMinutes,"TimeLimit"},
  {eMO_killlimit,"ch_KillLimit"},
  {eMO_killlimitKills,"KillLimit"},
  {eMO_respawnTime,"RespawnTime"},
  {eMO_fastEconomy,"ch_FastEconomy"},
	{eMO_roundtime,"RoundTime"},
	{eMO_roundlimit,"RoundsWin"},
	{eMO_preroundtime,"DefreezeTime"},
	{eMO_suddendeath_time,"UnhideTime"}
};

static const float  TOD_SPEED_MIN = 0.5f;
static const float  TOD_SPEED_MAX = 6.0f;


CMultiPlayerMenu::SCreateGame::SCreateGame(IFlashPlayer* plr, CMPHub* hub, bool lan):
m_maxplayers(0),
m_spectators(0),
m_friendlyFire(false),
m_friendlyDamageRatio(0),
m_enableKTK(false),
m_numKTKKills(0),
m_VoIP(false),
m_autoTeamBalance(false),
m_controllersOnly(false),
m_randomize(false),
m_anticheat(false),
m_dedicated(false),
m_dx10(false),
m_TOD(true),
m_TODStart(0),
m_TODSpeed(0),
m_battleDust(false),
m_player(plr),
m_hub(hub),
m_lan(lan)
{
}

bool CMultiPlayerMenu::SCreateGame::HandleFSCommand(EGsUiCommand cmd, const char* pArgs)
{ 
  switch(cmd)
  {

	case eGUC_createServerUpdateLevels:
		UpdateLevels(pArgs);
		break;
	case eGUC_createServerOpened:
		SetGlobalSettings();
		break;
  case eGUC_createServerParams:
    GetGlobalSettings();
    break;
  case eGUC_createServerStart:
    StartServer();
    break;
  default:
    return false;
  }
  return true;
}

void CMultiPlayerMenu::SCreateGame::StartServer()
{
  GetLevelRotation();
	
	ILevelSystem *pLevelSystem = gEnv->pGame->GetIGameFramework()->GetILevelSystem();
  if(!pLevelSystem)
    return;

  ILevelRotation *rotation = pLevelSystem->GetLevelRotation();
  if(!rotation)
    return;

  if(m_randomize)
    rotation->SetRandom(true);
  IConsole *pConsole = gEnv->pConsole;

  if(ICVar* var = pConsole->GetCVar("sv_servername"))
    var->Set(m_name);

  if(ICVar* var = pConsole->GetCVar("sv_password"))
    var->Set(m_password);

  if(ICVar* var = pConsole->GetCVar("sv_maxplayers"))
    var->Set(m_maxplayers);

  if(ICVar* var = pConsole->GetCVar("sv_maxspectators"))
    var->Set(m_spectators);

  if(ICVar* var = pConsole->GetCVar("g_friendlyfireratio"))
    var->Set(m_friendlyFire?float(m_friendlyDamageRatio/100.0f):0.0f);

	if(ICVar* var = pConsole->GetCVar("sv_timeofdayenable"))
		var->Set(m_TOD?1:0);

  if(ICVar* var = pConsole->GetCVar("sv_timeofdaylength"))
    var->Set(float(m_TODSpeed));

  if(ICVar* var = pConsole->GetCVar("sv_timeofdaystart"))
    var->Set(float(m_TODStart));

	if(ICVar* var = pConsole->GetCVar("g_tk_punish"))
		var->Set(m_enableKTK?1:0);

  if(ICVar* var = pConsole->GetCVar("g_tk_punish_limit"))
    var->Set(m_numKTKKills);

  if(ICVar* var = pConsole->GetCVar("sv_requireinputdevice"))
    var->Set(m_controllersOnly?"gamepad":"dontcare");

  if(ICVar* var = pConsole->GetCVar("net_enable_voice_chat"))
    var->Set(m_VoIP?1:0);

  if(ICVar* var = pConsole->GetCVar("g_autoteambalance"))
    var->Set(m_autoTeamBalance?1:0);

  if(ICVar* var = pConsole->GetCVar("g_battledust_enable"))
    var->Set(m_battleDust?1:0);

  if(ICVar* var = pConsole->GetCVar("sv_lanonly"))
		var->Set(m_lan?1:0);

  if(m_anticheat)
		pConsole->ExecuteString("net_pb_sv_enable true");
  else
    pConsole->ExecuteString("net_pb_sv_enable false");

  string command("sv_gamerules ");
  command.append(rotation->GetNextGameRules());
  gEnv->pConsole->ExecuteString(command.c_str()); 

  command = "g_nextlevel"; //"map ";
  //command.append(rotation->GetNextLevel());
  command.append(" s");
  if(m_dx10)
    command.append(" x");

  ICVar* rot = gEnv->pConsole->GetCVar("sv_levelrotation");
  if(rot)
  {
    ILevelRotationFile* file = 0;
    IPlayerProfileManager *pProfileMan = gEnv->pGame->GetIGameFramework()->GetIPlayerProfileManager();
    if(pProfileMan)
    {
      const char *userName = pProfileMan->GetCurrentUser();
      IPlayerProfile *pProfile = pProfileMan->GetCurrentProfile(userName);
      if(pProfile)
      {
        file = pProfile->GetLevelRotationFile(rot->GetString());
      }
    }
    if(file)
    {
      rotation->Save(file);
      file->Complete();
    }
  }

  if(m_dedicated)
  {
    //command = "g_nextlevel +" + command;
    if(m_anticheat)
      command = "net_pb_sv_enable true +" + command;
    StartDedicated(command);
  } 
  else
  {
    pConsole->ExecuteString("disconnect");
    //pConsole->ExecuteString("g_nextlevel");
    gEnv->pGame->GetIGameFramework()->ExecuteCommandNextFrame(command.c_str());
  }
}

void CMultiPlayerMenu::SCreateGame::GetLevelRotation()
{
	SFlashVarValue value(int(0));
	m_player->GetVariable("m_backServersCount",&value);
	int listSize = (int)value.GetDouble();
	if(listSize<=0) return;

	ILevelSystem *pLevelSystem = gEnv->pGame->GetIGameFramework()->GetILevelSystem();
	if(!pLevelSystem)
    return;
	ILevelRotation *rotation = pLevelSystem->GetLevelRotation();
	if(!rotation)
	  return;

	rotation->Reset();

	for(int i=0; i<listSize; ++i)
	{

		m_player->Invoke1("setDataForMap",i);

		SFlashVarValue levelName("");
		m_player->GetVariable("m_backServerName",&levelName);

		SFlashVarValue gameMode("");
		m_player->GetVariable("m_backServerGameMode",&gameMode);

		int levelidx = rotation->AddLevel(levelName.GetConstStrPtr(), gameMode.GetConstStrPtr());

		int size = m_player->GetVariableArraySize("m_backKeys");
		std::vector<const char*> keyArray;
		keyArray.resize(size);
		m_player->GetVariableArray(FVAT_ConstStrPtr,"m_backKeys", 0, &keyArray[0], size);

		size = m_player->GetVariableArraySize("m_backValues");
		std::vector<const char*> valueArray;
		valueArray.resize(size);
		m_player->GetVariableArray(FVAT_ConstStrPtr,"m_backValues", 0, &valueArray[0], size);

		if(keyArray.size() != valueArray.size())
		{
			CryLog("UI|CreateGame: Rotation Key/Value Arrays not matching!");
			continue;
		}
    
    bool fraglimit = false;
    bool timelimit = false;
    int  kills = 0;
    int  time = 0;
    int  respawn = 0;
    bool fasteconomy = false;
		int  roundtime = 0;
		int  preroundtime = 0;
		int  roundlimit = 0;
		int  suddendeath_time = 0;
		for(int i=0; i<keyArray.size(); ++i)
		{
			//CryLog("rotation: %s = %s",keyArray[i], valueArray[i]);
      EMapOptions opt = KEY_BY_VALUE(string(keyArray[i]),gMapOptions);
      switch(opt)
      {
      case eMO_timelimit:
        timelimit = IsTrue(valueArray[i]);
        break;
      case eMO_timelimitMinutes:
        time = atoi(valueArray[i]);
        break;
      case eMO_killlimit:
        fraglimit = IsTrue(valueArray[i]);
        break;
      case eMO_killlimitKills:
        kills = atoi(valueArray[i]);
        break;
      case eMO_respawnTime:
        respawn = atoi(valueArray[i]);
        break;
			case eMO_fastEconomy:
				fasteconomy = IsTrue(valueArray[i]);
				break;
			case eMO_roundtime:
				roundtime = atoi(valueArray[i]);
				break;
			case eMO_roundlimit:
				roundlimit = atoi(valueArray[i]);
				break;
			case eMO_preroundtime:
				preroundtime = atoi(valueArray[i]);
				break;
			case eMO_suddendeath_time:
				suddendeath_time = atoi(valueArray[i]);
				break;
      }
		}
    rotation->AddSetting(levelidx,string().Format("g_timelimit %d",timelimit?time:0));
    rotation->AddSetting(levelidx,string().Format("g_fraglimit %d",fraglimit?kills:0));
		rotation->AddSetting(levelidx,string().Format("g_pp_scale_income %.2f",fasteconomy?1.4f:1.0f));
		rotation->AddSetting(levelidx,string().Format("g_revivetime %d",respawn));
		rotation->AddSetting(levelidx,string().Format("g_roundtime %d",roundtime));
		rotation->AddSetting(levelidx,string().Format("g_roundlimit %d",roundlimit));
		rotation->AddSetting(levelidx,string().Format("g_preroundtime %d",preroundtime));
		rotation->AddSetting(levelidx,string().Format("g_suddendeath_time %d",suddendeath_time));
	}
}

void CMultiPlayerMenu::SCreateGame::GetGlobalSettings()
{
	int size = m_player->GetVariableArraySize("m_backKeys");
	std::vector<const char*> keyArray;
	keyArray.resize(size);
	m_player->GetVariableArray(FVAT_ConstStrPtr,"m_backKeys", 0, &keyArray[0], size);

	size = m_player->GetVariableArraySize("m_backValues");
	std::vector<const char*> valueArray;
	valueArray.resize(size);
	m_player->GetVariableArray(FVAT_ConstStrPtr,"m_backValues", 0, &valueArray[0], size);

	if(keyArray.size() != valueArray.size())
	{
		CryLog("UI|CreateGame: Gobal Settings Key/Value Arrays not matching!");
		return;
	}
	for(int i=0; i<keyArray.size(); ++i)
	{
    ECreateGameOptions opt = KEY_BY_VALUE(string(keyArray[i]),gCreateGameOptions);
    switch(opt)
    {
    case eCGO_name:
      m_name = valueArray[i];
      break;
    case eCGO_password:
      m_password = valueArray[i];
      break;
    case eCGO_maxplayers:
      m_maxplayers = atoi(valueArray[i]);
      break;
    case eCGO_spectators:
      m_spectators = atoi(valueArray[i]);
      break;
    case eCGO_friendlyFire:
      m_friendlyFire = IsTrue(valueArray[i]);
      break;
    case eCGO_friendlyDamageRatio:
      m_friendlyDamageRatio = atoi(valueArray[i]);
      break;
    case eCGO_enableKTK:
      m_enableKTK = IsTrue(valueArray[i]);
      break;
    case eCGO_numKTKKills:
      m_numKTKKills = atoi(valueArray[i]);
      break;
    case eCGO_VoIP:
      m_VoIP = IsTrue(valueArray[i]);
      break;
    case eCGO_autoTeamBalance:
      m_autoTeamBalance = IsTrue(valueArray[i]);
      break;
    case eCGO_controllersOnly:
      m_controllersOnly = IsTrue(valueArray[i]);
      break;
    case eCGO_randomize:
      m_randomize = IsTrue(valueArray[i]);
      break;
    case eCGO_anticheat:
      m_anticheat = IsTrue(valueArray[i]);
      break;
    case eCGO_dedicated:
      m_dedicated = IsTrue(valueArray[i]);
      break;
    case eCGO_dx10:
      m_dx10 = IsTrue(valueArray[i]);
      break;
    case eCGO_TOD:
      m_TOD = IsTrue(valueArray[i]);
      break;
    case eCGO_TODStart:
      {
        m_TODStart = atof(valueArray[i]);
      }
      break;
    case eCGO_TODSpeed:
      m_TODSpeed = TOD_SPEED_MIN + atoi(valueArray[i])*0.01f*(TOD_SPEED_MAX-TOD_SPEED_MIN);
      break;
    case eCGO_battleDust:
      m_battleDust = IsTrue(valueArray[i]);
      break;
    default:
      //WTF!
      {
        int a=3;
      }
    }
	}
}

void CMultiPlayerMenu::SCreateGame::SetGlobalSettings()
{
  static const int CACHE_SIZE = 30;
  std::vector<string>      cache;
  cache.reserve(CACHE_SIZE);//WARNING!!!! should not be reallocated during the function execution!!!
	std::vector<string> keyArray;
	keyArray.reserve(10);
	std::vector<const char*> valueArray;
	valueArray.reserve(10);

	if(ICVar* v = gEnv->pConsole->GetCVar("sv_servername"))
  {
    m_name = v->GetString();
	  keyArray.push_back(VALUE_BY_KEY(eCGO_name,gCreateGameOptions));
		valueArray.push_back(m_name.empty()?gEnv->pNetwork->GetHostName():m_name.c_str());
  }
  if(ICVar* v = gEnv->pConsole->GetCVar("sv_password"))
  {
    m_password = v->GetString();
    keyArray.push_back(VALUE_BY_KEY(eCGO_password,gCreateGameOptions));
    valueArray.push_back(v->GetString());
  }
  if(ICVar* v = gEnv->pConsole->GetCVar("sv_maxplayers"))
  {
    m_maxplayers = v->GetIVal();
    keyArray.push_back(VALUE_BY_KEY(eCGO_maxplayers,gCreateGameOptions));
    valueArray.push_back(v->GetString());
  }
  if(ICVar* v = gEnv->pConsole->GetCVar("sv_maxspectators"))
  {
    m_spectators = v->GetIVal();
    keyArray.push_back(VALUE_BY_KEY(eCGO_spectators,gCreateGameOptions));
		char s[32]      = "\0";
		itoa(m_spectators, s, 10);
    valueArray.push_back(s);
  }
  if(ICVar* v = gEnv->pConsole->GetCVar("g_friendlyfireratio"))
  {
    m_friendlyDamageRatio = int(v->GetFVal()*100.0f+0.5f);
    m_friendlyFire = m_friendlyDamageRatio!=0;
    keyArray.push_back(VALUE_BY_KEY(eCGO_friendlyFire,gCreateGameOptions));
    valueArray.push_back(m_friendlyFire?"1":"0");
    cache.push_back(string().Format("%d",m_friendlyDamageRatio).c_str());
    keyArray.push_back(VALUE_BY_KEY(eCGO_friendlyDamageRatio,gCreateGameOptions));
    valueArray.push_back(cache.back());
  }

	if(ICVar* v = gEnv->pConsole->GetCVar("g_tk_punish"))
	{
		m_enableKTK = v->GetIVal()!=0;
		keyArray.push_back(VALUE_BY_KEY(eCGO_enableKTK,gCreateGameOptions));
		valueArray.push_back(m_enableKTK?"1":"0");
	}

  if(ICVar* v = gEnv->pConsole->GetCVar("g_tk_punish_limit"))
  {
		m_numKTKKills = v->GetIVal();
		keyArray.push_back(VALUE_BY_KEY(eCGO_numKTKKills,gCreateGameOptions));
    valueArray.push_back(v->GetString());
  }

  if(ICVar* v = gEnv->pConsole->GetCVar("net_enable_voice_chat"))
  {
    m_VoIP = v->GetIVal()!=0;
    keyArray.push_back(VALUE_BY_KEY(eCGO_VoIP,gCreateGameOptions));
    valueArray.push_back(m_VoIP?"1":"0");
  }
  if(ICVar* v = gEnv->pConsole->GetCVar("g_autoteambalance"))
  {
    m_autoTeamBalance = v->GetIVal()!=0;
    keyArray.push_back(VALUE_BY_KEY(eCGO_autoTeamBalance,gCreateGameOptions));
    valueArray.push_back(m_autoTeamBalance?"1":"0");
  }
  if(ILevelRotation *rot = gEnv->pGame->GetIGameFramework()->GetILevelSystem()->GetLevelRotation())
  {
    m_randomize = rot->IsRandom();
    keyArray.push_back(VALUE_BY_KEY(eCGO_randomize,gCreateGameOptions));
    valueArray.push_back(m_randomize?"1":"0");
  }

  {
    m_dx10 = gEnv->pRenderer->GetRenderType() == eRT_DX10;
    keyArray.push_back(VALUE_BY_KEY(eCGO_dx10,gCreateGameOptions));
    valueArray.push_back(m_dx10?"1":"0");
  }

  if(ICVar* v = gEnv->pConsole->GetCVar("g_battledust_enable"))
  {
    m_battleDust = v->GetIVal()!=0;
    keyArray.push_back(VALUE_BY_KEY(eCGO_battleDust,gCreateGameOptions));
    valueArray.push_back(m_battleDust?"1":"0");
  }

  if(ICVar* v = gEnv->pConsole->GetCVar("sv_timeofdayenable"))
  {
    m_TOD = v->GetIVal()!=0;
    keyArray.push_back(VALUE_BY_KEY(eCGO_TOD,gCreateGameOptions));
    valueArray.push_back(m_TOD?"1":"0");
  }

  if(ICVar* v = gEnv->pConsole->GetCVar("sv_timeofdaylength"))
  {
    m_TODSpeed = min(TOD_SPEED_MAX,max(v->GetFVal(),TOD_SPEED_MIN));
    cache.push_back(string().Format("%.0f",100.0f*(m_TODSpeed-TOD_SPEED_MIN)/(TOD_SPEED_MAX-TOD_SPEED_MIN)).c_str());
    keyArray.push_back(VALUE_BY_KEY(eCGO_TODSpeed,gCreateGameOptions));
    valueArray.push_back(cache.back());
  }

  if(ICVar* v = gEnv->pConsole->GetCVar("sv_timeofdaystart"))
  {
    m_TODStart = v->GetFVal();
    cache.push_back(string().Format("%.5f",m_TODStart).c_str());
    keyArray.push_back(VALUE_BY_KEY(eCGO_TODStart,gCreateGameOptions));
    valueArray.push_back(cache.back());
  }

	m_player->SetVariableArray(FVAT_ConstStrPtr, "m_backKeys", 0, &keyArray[0], keyArray.size());
	m_player->SetVariableArray(FVAT_ConstStrPtr, "m_backValues", 0, &valueArray[0], keyArray.size());
	m_player->Invoke0("_root.Root.MainMenu.MultiPlayer.CreateGame_M.CreateGame.CreateGame_Controls.retrieveGlobalSettings");

	//reset
	m_player->Invoke0("resetRotationLevels");
	ILevelSystem *pLevelSystem = gEnv->pGame->GetIGameFramework()->GetILevelSystem();
	if(pLevelSystem)
	{
		ILevelRotation* rot = pLevelSystem->GetLevelRotation();
		if(rot && rot->GetLength()>0)
		{
			if(m_randomize)
				rot->SetRandom(false);

			rot->First();
			do 
			{
				valueArray.resize(0);
				keyArray.resize(0);

				string level = rot->GetNextLevel();
				string rules = rot->GetNextGameRules();
				string desc = level;

				ILevelInfo *pLevelInfo = pLevelSystem->GetLevelInfo(level);
				if(pLevelInfo && pLevelInfo->SupportsGameType(rules))
				{
					desc = pLevelInfo->GetDisplayName();
				}

				for(int i=0;i<rot->GetNextSettingsNum();++i)
				{
					string setting = rot->GetNextSetting(i);
					int sp = setting.find(' ');
					if(sp==-1)
						continue;
					string var = setting.substr(0,sp);
					string val = &setting[sp+1];
					
					if(!var.compareNoCase("g_timelimit"))
					{
						int t = atoi(val);
						keyArray.push_back(VALUE_BY_KEY(eMO_timelimit,gMapOptions));
						valueArray.push_back(t==0?"0":"1");
						
						cache.push_back(val.c_str());
						keyArray.push_back(VALUE_BY_KEY(eMO_timelimitMinutes,gMapOptions));
						valueArray.push_back(cache.back());
					}
					if(!var.compareNoCase("g_fraglimit"))
					{
						int f = atoi(val);
						keyArray.push_back(VALUE_BY_KEY(eMO_killlimit,gMapOptions));
						valueArray.push_back(f==0?"0":"1");

						cache.push_back(val.c_str());
						keyArray.push_back(VALUE_BY_KEY(eMO_killlimitKills,gMapOptions));
						valueArray.push_back(cache.back());
					}
					if(!var.compareNoCase("g_revivetime"))
					{
						int t = atoi(val);
						cache.push_back(val.c_str());
						keyArray.push_back(VALUE_BY_KEY(eMO_respawnTime,gMapOptions));
						valueArray.push_back(cache.back());
					}
					if(!var.compareNoCase("g_pp_scale_income"))
					{
						float f = atof(val);
						keyArray.push_back(VALUE_BY_KEY(eMO_fastEconomy,gMapOptions));
						valueArray.push_back(f==1.0f?"0":"1");
					}
				}
				m_player->SetVariable("m_backServerName", SFlashVarValue(level));
				m_player->SetVariable("m_backServerGameMode", SFlashVarValue(rules));
				m_player->SetVariable("m_backServerDescription", SFlashVarValue(desc));

				m_player->SetVariableArray(FVAT_ConstStrPtr, "m_backKeys", 0, &keyArray[0], keyArray.size());
				m_player->SetVariableArray(FVAT_ConstStrPtr, "m_backValues", 0, &valueArray[0], keyArray.size());
				m_player->Invoke0("addLevelToRotation");
				
				cache.resize(0);

			}while(rot->Advance());
			
			if(m_randomize)
				rot->SetRandom(true);
		}
	 }
  //assert(cache.size()<=CACHE_SIZE);

}

void CMultiPlayerMenu::SCreateGame::UpdateLevels(const char* gamemode)
{
	m_player->Invoke0("resetMultiplayerLevel");
  ILevelSystem *pLevelSystem = gEnv->pGame->GetIGameFramework()->GetILevelSystem();
  if(pLevelSystem)
  {
    for(int l = 0; l < pLevelSystem->GetLevelCount(); ++l)
    {
      ILevelInfo *pLevelInfo = pLevelSystem->GetLevelInfo(l);
      if(pLevelInfo && pLevelInfo->SupportsGameType(gamemode))
			{
        string disp(pLevelInfo->GetDisplayName());
        SFlashVarValue args[2] = {pLevelInfo->GetName(),disp.empty()?pLevelInfo->GetName():disp.c_str()};
				m_player->Invoke("addMultiplayerLevel", args, 2);
			}

    }
  }
}

void CMultiPlayerMenu::SCreateGame::StartDedicated(const char* params)
{
  if(gEnv->pGame->GetIGameFramework()->SaveServerConfig("%USER%/config/server.cfg"))
	{
		string cmd = "Bin32/CrysisDedicatedServer.exe";
		if(gEnv->pSystem->IsDevMode())
		{
			cmd.append(" -devmode");
		}
		SModInfo info;
		if(gEnv->pGame->GetIGameFramework()->GetModInfo(&info))
		{
			cmd.append(" -mod ");
			cmd.append(info.m_name);
		}
		cmd.append(" +exec %USER%/config/server.cfg +");
		cmd.append(params);
		if(gEnv->pGame->GetIGameFramework()->StartProcess(cmd))
		{
			gEnv->pSystem->Quit();
			return;
		}
	}
	m_hub->ShowError("Dedicated server launch failed.");
}
