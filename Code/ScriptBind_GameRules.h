/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Script Binding for GameRules

-------------------------------------------------------------------------
History:
- 23:2:2006   18:30 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTBIND_GAMERULES_H__
#define __SCRIPTBIND_GAMERULES_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IScriptSystem.h>
#include <ScriptHelpers.h>


class CGameRules;
class CActor;
struct IGameFramework;
struct ISystem;


class CScriptBind_GameRules :
	public CScriptableBase
{
public:
	CScriptBind_GameRules(ISystem *pSystem, IGameFramework *pGameFramework);
	virtual ~CScriptBind_GameRules();

	void AttachTo(CGameRules *pGameRules);

	int IsServer(IFunctionHandler *pH);
	int IsClient(IFunctionHandler *pH);
	int CanCheat(IFunctionHandler *pH);

	int SpawnPlayer(IFunctionHandler *pH, int channelId, const char *name, const char *className, Vec3 pos, Vec3 angles);
	int ChangePlayerClass(IFunctionHandler *pH, int channelId, const char *className);
	int RevivePlayer(IFunctionHandler *pH, ScriptHandle playerId, Vec3 pos, Vec3 angles, int teamId, bool clearInventory);
	int RevivePlayerInVehicle(IFunctionHandler *pH, ScriptHandle playerId, ScriptHandle vehicleId, int seatId, int teamId, bool clearInventory);
	int RenamePlayer(IFunctionHandler *pH, ScriptHandle playerId, const char *name);
	int KillPlayer(IFunctionHandler *pH, ScriptHandle playerId, bool dropItem, bool ragdoll,
		ScriptHandle shooterId, ScriptHandle weaponId, float damage, int material, int hit_type, Vec3 impulse);
	int MovePlayer(IFunctionHandler *pH, ScriptHandle playerId, Vec3 pos, Vec3 angles);
	int GetPlayerByChannelId(IFunctionHandler *pH, int channelId);
	int GetChannelId(IFunctionHandler *pH, ScriptHandle playerId);
	int GetPlayerCount(IFunctionHandler *pH);
	int GetSpectatorCount(IFunctionHandler *pH);
	int GetPlayers(IFunctionHandler *pH);
	int IsPlayerInGame(IFunctionHandler *pH, ScriptHandle playerId);
	int IsProjectile(IFunctionHandler *pH, ScriptHandle entityId);
	int IsSameTeam(IFunctionHandler *pH, ScriptHandle entityId0, ScriptHandle entityId1);
	int IsNeutral(IFunctionHandler *pH, ScriptHandle entityId);

	int AddSpawnLocation(IFunctionHandler *pH, ScriptHandle entityId);
	int RemoveSpawnLocation(IFunctionHandler *pH, ScriptHandle id);
	int GetSpawnLocationCount(IFunctionHandler *pH);
	int GetSpawnLocationByIdx(IFunctionHandler *pH, int idx);
	int GetSpawnLocation(IFunctionHandler *pH, ScriptHandle playerId, bool ignoreTeam, bool includeNeutral);
	int GetSpawnLocations(IFunctionHandler *pH);
	int GetFirstSpawnLocation(IFunctionHandler *pH, int teamId);

	int AddSpawnGroup(IFunctionHandler *pH, ScriptHandle groupId);
	int AddSpawnLocationToSpawnGroup(IFunctionHandler *pH, ScriptHandle groupId, ScriptHandle location);
	int RemoveSpawnLocationFromSpawnGroup(IFunctionHandler *pH, ScriptHandle groupId, ScriptHandle location);
	int RemoveSpawnGroup(IFunctionHandler *pH, ScriptHandle groupId);
	int GetSpawnLocationGroup(IFunctionHandler *pH, ScriptHandle spawnId);
	int GetSpawnGroups(IFunctionHandler *pH);
	int IsSpawnGroup(IFunctionHandler *pH, ScriptHandle entityId);

	int GetTeamDefaultSpawnGroup(IFunctionHandler *pH, int teamId);
	int SetTeamDefaultSpawnGroup(IFunctionHandler *pH, int teamId, ScriptHandle groupId);
	int SetPlayerSpawnGroup(IFunctionHandler *pH, ScriptHandle playerId, ScriptHandle groupId);

	int AddSpectatorLocation(IFunctionHandler *pH, ScriptHandle location);
	int RemoveSpectatorLocation(IFunctionHandler *pH, ScriptHandle id);
	int GetSpectatorLocationCount(IFunctionHandler *pH);
	int GetSpectatorLocation(IFunctionHandler *pH, int idx);
	int GetSpectatorLocations(IFunctionHandler *pH);
	int GetRandomSpectatorLocation(IFunctionHandler *pH);
	int GetInterestingSpectatorLocation(IFunctionHandler *pH);
	// for 3rd person follow-cam mode
	int GetNextSpectatorTarget(IFunctionHandler *pH, ScriptHandle playerId, int change);
	int ChangeSpectatorMode(IFunctionHandler* pH, ScriptHandle playerId, int mode, ScriptHandle targetId);
	int CanChangeSpectatorMode(IFunctionHandler* pH, ScriptHandle playerId);
	
	int AddMinimapEntity(IFunctionHandler *pH, ScriptHandle entityId, int type, float lifetime);
	int RemoveMinimapEntity(IFunctionHandler *pH, ScriptHandle entityId);
	int ResetMinimap(IFunctionHandler *pH);

	int GetPing(IFunctionHandler *pH, int channelId);

	int ResetEntities(IFunctionHandler *pH);
	int ServerExplosion(IFunctionHandler *pH, ScriptHandle shooterId, ScriptHandle weaponId, float dmg,
		Vec3 pos, Vec3 dir, float radius, float angle, float pressure, float holesize);
	int ServerHit(IFunctionHandler *pH, ScriptHandle targetId, ScriptHandle shooterId, ScriptHandle weaponId, float dmg, float radius, int materialId, int partId, int typeId);

	int CreateTeam(IFunctionHandler *pH, const char *name);
	int RemoveTeam(IFunctionHandler *pH, int teamId);
	int GetTeamName(IFunctionHandler *pH, int teamId);
	int GetTeamId(IFunctionHandler *pH, const char *teamName);
	int GetTeamCount(IFunctionHandler *pH);
	int GetTeamPlayerCount(IFunctionHandler *pH, int teamId);
	int GetTeamChannelCount(IFunctionHandler *pH, int teamId);
	int GetTeamPlayers(IFunctionHandler *pH, int teamId);

	int SetTeam(IFunctionHandler *pH, int teamId, ScriptHandle playerId);
	int GetTeam(IFunctionHandler *pH, ScriptHandle playerId);
	int GetChannelTeam(IFunctionHandler *pH, int channelId);

	int AddObjective(IFunctionHandler *pH, int teamId, const char *objective, int status, ScriptHandle entityId);
	int SetObjectiveStatus(IFunctionHandler *pH, int teamId, const char *objective, int status);
	int SetObjectiveEntity(IFunctionHandler *pH, int teamId, const char *objective, ScriptHandle entityId);
	int RemoveObjective(IFunctionHandler *pH, int teamId, const char *objective);
	int ResetObjectives(IFunctionHandler *pH);

	int TextMessage(IFunctionHandler *pH, int type, const char *msg);
	int SendTextMessage(IFunctionHandler *pH, int type, const char *msg);
	int SendChatMessage(IFunctionHandler *pH, int type, ScriptHandle sourceId, ScriptHandle targetId, const char *msg);
	int ForbiddenAreaWarning(IFunctionHandler *pH, bool active, int timer, ScriptHandle targetId);

	int ResetGameTime(IFunctionHandler *pH);
	int GetRemainingGameTime(IFunctionHandler *pH);
	int IsTimeLimited(IFunctionHandler *pH);

	int ResetRoundTime(IFunctionHandler *pH);
	int GetRemainingRoundTime(IFunctionHandler *pH);
	int IsRoundTimeLimited(IFunctionHandler *pH);

	int ResetPreRoundTime(IFunctionHandler *pH);
	int GetRemainingPreRoundTime(IFunctionHandler *pH);

	int ResetReviveCycleTime(IFunctionHandler *pH);
	int GetRemainingReviveCycleTime(IFunctionHandler *pH);

	int ResetGameStartTimer(IFunctionHandler *pH, float time);
	int GetRemainingStartTimer(IFunctionHandler *pH);

	int	EndGame(IFunctionHandler *pH);
	int NextLevel(IFunctionHandler *pH);

	int RegisterHitMaterial(IFunctionHandler *pH, const char *materialName);
	int GetHitMaterialId(IFunctionHandler *pH, const char *materialName);
	int GetHitMaterialName(IFunctionHandler *pH, int materialId);
	int ResetHitMaterials(IFunctionHandler *pH);

	int RegisterHitType(IFunctionHandler *pH, const char *type);
	int GetHitTypeId(IFunctionHandler *pH, const char *type);
	int GetHitType(IFunctionHandler *pH, int id);
	int ResetHitTypes(IFunctionHandler *pH);

	int ForceScoreboard(IFunctionHandler *pH, bool force);
	int FreezeInput(IFunctionHandler *pH, bool freeze);

	int ScheduleEntityRespawn(IFunctionHandler *pH, ScriptHandle entityId, bool unique, float timer);
	int AbortEntityRespawn(IFunctionHandler *pH, ScriptHandle entityId, bool destroyData);

	int ScheduleEntityRemoval(IFunctionHandler *pH, ScriptHandle entityId, float timer, bool visibility);
	int AbortEntityRemoval(IFunctionHandler *pH, ScriptHandle entityId);

	int SetSynchedGlobalValue(IFunctionHandler *pH, int key);
	int GetSynchedGlobalValue(IFunctionHandler *pH, int key);

	int SetSynchedEntityValue(IFunctionHandler *pH, ScriptHandle entityId, int key);
	int GetSynchedEntityValue(IFunctionHandler *pH, ScriptHandle entityId, int key);

	int ResetSynchedStorage(IFunctionHandler *pH);
	int ForceSynchedStorageSynch(IFunctionHandler *pH, int channelId);

	int IsDemoMode(IFunctionHandler *pH);

	// functions which query the console variables relevant to Crysis gamemodes
	int GetTimeLimit(IFunctionHandler *pH);
	int GetRoundTime(IFunctionHandler *pH);
	int GetPreRoundTime(IFunctionHandler *pH);
	int GetRoundLimit(IFunctionHandler *pH);
	int GetFragLimit(IFunctionHandler *pH);
	int GetFragLead(IFunctionHandler *pH);
  int GetFriendlyFireRatio(IFunctionHandler *pH);
  int GetReviveTime(IFunctionHandler *pH);
	int GetMinPlayerLimit(IFunctionHandler *pH);
	int GetMinTeamLimit(IFunctionHandler *pH);
	int GetTeamLock(IFunctionHandler *pH);
	int GetAutoTeamBalance(IFunctionHandler *pH);
	int GetAutoTeamBalanceThreshold(IFunctionHandler *pH);
	
	int IsFrozen(IFunctionHandler *pH, ScriptHandle entityId);
	int FreezeEntity(IFunctionHandler *pH, ScriptHandle entityId, bool freeze, bool vapor);
	int ShatterEntity(IFunctionHandler *pH, ScriptHandle entityId, Vec3 pos, Vec3 impulse);

  int DebugCollisionDamage(IFunctionHandler *pH);
	int DebugHits(IFunctionHandler *pH);

	int SendHitIndicator(IFunctionHandler* pH, ScriptHandle shooterId, bool explosion);
	int SendDamageIndicator(IFunctionHandler* pH, ScriptHandle targetId, ScriptHandle shooterId, ScriptHandle weaponId);

	int IsInvulnerable(IFunctionHandler* pH, ScriptHandle playerId);
	int SetInvulnerability(IFunctionHandler* pH, ScriptHandle playerId, bool invulnerable);

	// for the power struggle tutorial
	int TutorialEvent(IFunctionHandler* pH, int eventType);

	int GameOver(IFunctionHandler* pH, int localWinner);
	int EnteredGame(IFunctionHandler* pH);
	int EndGameNear(IFunctionHandler* pH, ScriptHandle entityId);

	int SPNotifyPlayerKill(IFunctionHandler* pH, ScriptHandle targetId, ScriptHandle weaponId, bool bHeadShot);

	// EMP Grenade
	int ProcessEMPEffect(IFunctionHandler* pH, ScriptHandle targetId, float timeScale);
	int PerformDeadHit(IFunctionHandler* pH);

private:
	void RegisterGlobals();
	void RegisterMethods();

	CGameRules *GetGameRules(IFunctionHandler *pH);
	CActor *GetActor(EntityId id);

	SmartScriptTable	m_players;
	SmartScriptTable	m_teamplayers;
	SmartScriptTable	m_spawnlocations;
	SmartScriptTable	m_spectatorlocations;
	SmartScriptTable	m_spawngroups;

	ISystem						*m_pSystem;
	IScriptSystem			*m_pSS;
	IGameFramework		*m_pGameFW;
};


#endif //__SCRIPTBIND_GAMERULES_H__
