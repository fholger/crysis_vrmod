/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: HUD score board for multiplayer

-------------------------------------------------------------------------
History:
- 27:03:2006: Created by Jan Müller

*************************************************************************/

#include "StdAfx.h"
#include "HUDObject.h"
#include "HUDScore.h"
#include "HUD.h"
#include "HUDRadar.h"
#include "IGameFramework.h"
#include "IUIDraw.h"
#include "Game.h"
#include "GameRules.h"
#include "GameFlashAnimation.h"
#include "HUDCommon.h"
#include "Actor.h"

CHUDScore::ScoreEntry::ScoreEntry(EntityId id, int kills, int deaths, int ping): m_entityId(id), m_team(-1)
{
	m_kills = kills;
	m_deaths = deaths;
	m_ping = ping;
	m_currentRank = 0;

	if(int team = g_pGame->GetGameRules()->GetTeam(id))
		m_team = team;
}

void CHUDScore::ScoreEntry::UpdateLiveStats()
{
	if (CActor *pActor=static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_entityId)))
	{
		m_alive=pActor->GetHealth()>0;
		if(pActor->GetSpectatorMode() && !g_pGame->GetGameRules()->IsPlayerActivelyPlaying(pActor->GetEntityId()))
			m_spectating = true;
		else
			m_spectating = false;

		//update rank
		m_currentRank = 0;
		CGameRules *pGameRules=g_pGame->GetGameRules();
		if(!m_spectating && pGameRules)
		{
			IScriptTable *pGameRulesScript=pGameRules->GetEntity()->GetScriptTable();
			if(pGameRulesScript)
			{
				HSCRIPTFUNCTION pfnFuHelper = 0;
				if (pGameRulesScript->GetValue("GetPlayerRank", pfnFuHelper) && pfnFuHelper)
				{
					Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, ScriptHandle(pActor->GetEntityId()), m_currentRank);
					gEnv->pScriptSystem->ReleaseFunc(pfnFuHelper);
				}
			}
		}
	}
}

bool CHUDScore::ScoreEntry::operator<(const ScoreEntry& entry) const
{
	if(m_spectating)
		return false;
	else if(entry.m_spectating)
		return true;
	else if(m_team >= 0 && m_team != entry.m_team)
	{
		if(m_team > entry.m_team)
			return false;
		return true;
	}
	else
	{
		if(entry.m_currentRank > m_currentRank)
			return false;
		else if(m_currentRank > entry.m_currentRank)
			return true;
		else if(entry.m_kills > m_kills)
			return false;
		else if(m_kills > entry.m_kills)
			return true;
		else
		{
			if(m_deaths < entry.m_deaths)
				return true;
			else if (m_deaths > entry.m_deaths)
				return false;
			else
			{
				IEntity *pEntity0=gEnv->pEntitySystem->GetEntity(m_entityId);
				IEntity *pEntity1=gEnv->pEntitySystem->GetEntity(entry.m_entityId);

				const char *name0=pEntity0?pEntity0->GetName():"";
				const char *name1=pEntity1?pEntity1->GetName():"";

				if (strcmp(name0, name1)<0)
					return true;
				else
					return false;
			}
		}
		return true;
	}
}

void CHUDScore::SRankStats::Update(IScriptTable* pGameRulesScript, IActor* pActor, CGameFlashAnimation* pFlashBoard)
{
	HSCRIPTFUNCTION pfnFuHelper = 0;
	if (pGameRulesScript->GetValue("GetPlayerRank", pfnFuHelper) && pfnFuHelper)
	{
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, ScriptHandle(pActor->GetEntityId()), currentRank);
		gEnv->pScriptSystem->ReleaseFunc(pfnFuHelper);
	}
	if(!currentRank)
		return;

	if (pGameRulesScript->GetValue("GetRankName", pfnFuHelper) && pfnFuHelper)
	{
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, currentRank, true, false, currentRankName);
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, currentRank+1, true, false, nextRankName);
		gEnv->pScriptSystem->ReleaseFunc(pfnFuHelper);
	}

	if(!nextRankName)
		return;

	if (pGameRulesScript->GetValue("GetPlayerCP", pfnFuHelper) && pfnFuHelper)
	{
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, ScriptHandle(pActor->GetEntityId()), playerCP);
		gEnv->pScriptSystem->ReleaseFunc(pfnFuHelper);
	}

	if(pGameRulesScript->GetValue("GetRankCP", pfnFuHelper) && pfnFuHelper)
	{
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, currentRank, currentRankCP);
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, currentRank+1, nextRankCP);
		gEnv->pScriptSystem->ReleaseFunc(pfnFuHelper);

		//set values to flash
		if (pFlashBoard)
		{
			SFlashVarValue args[4] = {nextRankName, 100.0f * ((playerCP - currentRankCP) / float(nextRankCP - currentRankCP)), currentRank, currentRankName};
			pFlashBoard->Invoke("setClientRankProgress", args, 4);
		}
	}

	if(pGameRulesScript->GetValue("GetRankPP", pfnFuHelper) && pfnFuHelper)
	{
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, currentRank, currentRankPP);
		Script::CallReturn(gEnv->pScriptSystem, pfnFuHelper, pGameRulesScript, currentRank+1, nextRankPP);
		gEnv->pScriptSystem->ReleaseFunc(pfnFuHelper);

		//set values to flash
		if (pFlashBoard)
		{
			SFlashVarValue args[2] = {currentRankPP, nextRankPP};
			pFlashBoard->Invoke("setMinimumPrestige", args, 2);
		}
	}
}

CHUDScore::CHUDScore()
{
	m_bShow = false;
	m_lastUpdate = 0;
	m_lastShowSwitch = 0;
	m_pFlashBoard = NULL;
	m_currentClientTeam = -1;
}

void CHUDScore::Update(float fDeltaTime)
{
	if(m_bShow && gEnv->bMultiplayer)
		Render();
	else
		m_currentClientTeam = -1;
}

void CHUDScore::Reset()
{
	m_scoreBoard.clear();
}

void CHUDScore::AddEntry(EntityId player, int kills, int deaths, int ping)
{
	//doesn't check for existing entries atm. (scoreboard deleted every frame)
	m_scoreBoard.push_back(ScoreEntry(player, kills, deaths, ping));
}

void CHUDScore::Render()
{
	if(!m_pFlashBoard)
		return;

	int lastTeam = -1;
	int clientTeamPoints = 0;
	int enemyTeamPoints = 0;

	CGameRules *pGameRules=g_pGame->GetGameRules();
	if(!pGameRules)
		return;

	IScriptTable *pGameRulesScript=pGameRules->GetEntity()->GetScriptTable();
	int ppKey = 0;
	if(pGameRulesScript)
		pGameRulesScript->GetValue("PP_AMOUNT_KEY", ppKey);

	for(int i = 0; i < m_scoreBoard.size(); ++i)
		m_scoreBoard[i].UpdateLiveStats();
	std::sort(m_scoreBoard.begin(), m_scoreBoard.end());

	//clear the teams stats in flash
	//m_pFlashBoard->Invoke("clearTable");

	std::vector<ScoreEntry>::iterator it;
	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if(!pClientActor)
		return;

	static string svr_name;
	static string svr_ip;

	pGameRules->GetSynchedGlobalValue(GLOBAL_SERVER_IP_KEY, svr_ip);
	pGameRules->GetSynchedGlobalValue(GLOBAL_SERVER_NAME_KEY, svr_name);

	if(svr_ip != m_currentServerIp || svr_name != m_currentServer)
	{
		m_currentServer = svr_name;
		m_currentServerIp = svr_ip;

		SFlashVarValue args[2] = {svr_name.c_str(), svr_ip.c_str()};
		m_pFlashBoard->CheckedInvoke("setServerInfo",args,2);
	}

	bool drawTeamScores=true;

	int clientTeam = pGameRules->GetTeam(pClientActor->GetEntityId());
	bool notTeamed=(clientTeam != 1 && clientTeam != 2);
	if (notTeamed)		//Spectate mode
		clientTeam = 2;

	int teamCount = pGameRules->GetTeamCount();
	if(teamCount > 1)
	{
		if(clientTeam != m_currentClientTeam)
		{
			m_pFlashBoard->CheckedInvoke("setClientTeam", (clientTeam == 1)? "korean" : "us");
			m_currentClientTeam = clientTeam;
		}

		//get the player rank, next rank, CP etc. from gamerules script
		SRankStats rankStats;
		rankStats.Update(pGameRulesScript, pClientActor, m_pFlashBoard);
		drawTeamScores=(rankStats.currentRank!=0); // if we got here it means we don't want to show the number of kills
	}

	m_pFlashBoard->Invoke("clearEntries");
	std::vector<EntityId>::const_iterator start;
	std::vector<EntityId>::const_iterator end;
	std::vector<EntityId> *alreadySelected = NULL;
	if(g_pGame->GetHUD() && g_pGame->GetHUD()->GetRadar())
	{
		alreadySelected = g_pGame->GetHUD()->GetRadar()->GetSelectedTeamMates();
		start = alreadySelected->begin();
		end = alreadySelected->end();
	}

	for(it = m_scoreBoard.begin(); it != m_scoreBoard.end(); ++it)
	{
		ScoreEntry player = *it;

		IEntity *pPlayer=gEnv->pEntitySystem->GetEntity(player.m_entityId);
		//assert(pPlayer);
		if (!pPlayer)
			continue;

		if((teamCount > 1) && (player.m_team != 1 && player.m_team != 2))
			continue;

		if (drawTeamScores)
		{
			if(lastTeam != player.m_team)	//get next team
			{
				lastTeam = player.m_team;

				if (pGameRulesScript)
				{
					int key0;
					if (pGameRulesScript->GetValue("TEAMSCORE_TEAM0_KEY", key0))
					{
						int teamScore=0;
						pGameRules->GetSynchedGlobalValue(key0+player.m_team, teamScore);

						if(lastTeam == clientTeam)
							clientTeamPoints = teamScore;
						else
							enemyTeamPoints = teamScore;
					}
				}
			}
		}

		const char* rank = 0;
		HSCRIPTFUNCTION pfnGetPlayerRank = 0;
		if (pGameRulesScript->GetValue("GetPlayerRankName", pfnGetPlayerRank) && pfnGetPlayerRank)
		{
			Script::CallReturn(gEnv->pScriptSystem, pfnGetPlayerRank, pGameRulesScript, ScriptHandle(player.m_entityId), rank);
			gEnv->pScriptSystem->ReleaseFunc(pfnGetPlayerRank);
		}

		SUIWideString name(pPlayer->GetName());
		if(player.m_spectating)
		{
			name.m_string.append(L" (");
			name.m_string.append(g_pGame->GetHUD()->LocalizeWithParams("@ui_SPECTATE"));
			name.m_string.append(L")");
		}

		SFlashVarValue pp(0);

		if (!notTeamed)
		{
			int playerPP=0;
			pGameRules->GetSynchedEntityValue(player.m_entityId, TSynchedKey(ppKey), playerPP);
			pp=SFlashVarValue(playerPP);
		}
		else
			pp=SFlashVarValue("---");

		bool selected = false;
		if(alreadySelected)
		{
			if(std::find(start, end, player.m_entityId)!=end)
				selected = true;
		}

		IVoiceContext* pVoiceContext = gEnv->pGame->GetIGameFramework()->GetNetContext()->GetVoiceContext();
		bool muted = pVoiceContext->IsMuted(pClientActor->GetEntityId(), pPlayer->GetId());

		int team = (player.m_team == clientTeam)?1:2;
		if(player.m_spectating)
			team = 3;

		SFlashVarValue args[12] = { name.c_str(), team, (player.m_entityId==pClientActor->GetEntityId())?true:false, pp, player.m_kills, player.m_deaths, player.m_ping, player.m_entityId, rank?rank:" ", player.m_alive?0:1,selected, muted};
		m_pFlashBoard->Invoke("addEntry", args, 12);
	}

	if (drawTeamScores)
	//set the teams scores in flash
	{
		SFlashVarValue argsA[2] = {(clientTeam==1)?1:2, enemyTeamPoints};
		m_pFlashBoard->CheckedInvoke("setTeamPoints", argsA, 2);
		SFlashVarValue argsB[2] = {(clientTeam==1)?2:1, clientTeamPoints};
		m_pFlashBoard->CheckedInvoke("setTeamPoints", argsB, 2);
	}
	else
	{
		SFlashVarValue argsA[2] = {1, ""};
		m_pFlashBoard->CheckedInvoke("setTeamPoints", argsA, 2);
		SFlashVarValue argsB[2] = {2, ""};
		m_pFlashBoard->CheckedInvoke("setTeamPoints", argsB, 2);
	}

	EntityId playerBeingKicked = 0;
	m_pFlashBoard->Invoke("drawAllEntries", playerBeingKicked);
}

void CHUDScore::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	if(m_pFlashBoard)
		m_pFlashBoard->GetMemoryStatistics(s);
	s->AddContainer(m_scoreBoard);
}
