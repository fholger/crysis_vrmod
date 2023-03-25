/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 
	Header for G02 CHUD class

-------------------------------------------------------------------------
History:
- 07:11:2005: Created by Julien Darre
- 01:02:2006: Modified by Jan Müller
- 22:02:2006: Refactored for G04 by Matthew Jack
- 2007: Refactored by Jan Müller

*************************************************************************/
#ifndef __HUD_H__
#define __HUD_H__

//-----------------------------------------------------------------------------------------------------

#include "HUDEnums.h"
#include "HUDCommon.h"
#include "IFlashPlayer.h"
#include "IActionMapManager.h"
#include "IInput.h"
#include "IMovementController.h"
#include "IVehicleSystem.h"
#include "Item.h"
#include "NanoSuit.h"
#include "Player.h"
#include "Voting.h"
#include "IViewSystem.h"
#include "ISubtitleManager.h"
#include <CryFixedString.h>

#include "HUDMissionObjectiveSystem.h"

//-----------------------------------------------------------------------------------------------------

struct ILevel;
struct ILevelInfo;
struct IRenderer;
struct IAnimSequence;

class CHUDMissionObjective;
class CHUDObject;
class CHUDRadar;
class CHUDScore;
class CHUDTextChat;
class CHUDObituary;
class CHUDTextArea;
class CHUDTweakMenu;
class CHUDVehicleInterface;
class CHUDPowerStruggle;
class CHUDScopes;
class CHUDCrosshair;
class CHUDSilhouettes;
class CHUDTagNames;
class CLCDWrapper;
class CWeapon;

//-----------------------------------------------------------------------------------------------------

class CHUD :	public CHUDCommon, 
							public IGameFrameworkListener, 
							public IInputEventListener, 
							public IPlayerEventListener, 
							public IItemSystemListener, 
							public IWeaponEventListener, 
							public CNanoSuit::INanoSuitListener,
							public IViewSystemListener,
							public ISubtitleHandler,
							public IEquipmentManager::IListener
{
	friend class CFlashMenuObject;
	friend class CHUDPowerStruggle;
	friend class CHUDVehicleInterface;
	friend class CHUDScopes;
	friend class CHUDCrosshair;

public:

	//current mission objectives
	struct SHudObjective
	{
		string	message;
		string  description;
		int			status;
		SHudObjective() {}
		SHudObjective(const string &msg, const string &desc, const int stat) : message(msg), description(desc), status(stat) {}
		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(*this);
			s->Add(message);
			s->Add(description);
		}
	};
	typedef std::map<string, SHudObjective> THUDObjectiveList;

	CHUD();
	virtual	~	CHUD();

	//HUD initialisation
	bool Init();
	//setting local actor / player id
	void ResetPostSerElements();
	void PlayerIdSet(EntityId playerId);
	void PostSerialize();
  void GameRulesSet(const char* name);
	//handle game events
	void HandleEvent(const SGameObjectEvent &rGameObjectEvent);
	void WeaponAccessoriesInterface(bool visible, bool force = false);
	void SetFireMode(IItem *pItem, IFireMode *pFM, bool forceUpdate = false);

	void AddTrackedProjectile(EntityId id);
	void RemoveTrackedProjectile(EntityId id);

	void AutoAimLocking(EntityId id);
	void AutoAimNoText(EntityId id);
	void AutoAimLocked(EntityId id);
	void AutoAimUnlock(EntityId id);
	void ActorDeath(IActor* pActor);
	void ActorRevive(IActor* pActor);
	void VehicleDestroyed(EntityId id);
	void TextMessage(const char* message);

	void UpdateHUDElements();
	void UpdateCrosshair(IItem *pItem = NULL);
	virtual void UpdateCrosshairVisibility();
	bool ReturnFromLastModalScreen();

	void GetMemoryStatistics( ICrySizer * );

	// IGameFrameworkListener
	virtual void OnPostUpdate(float fDeltaTime);
	virtual void OnSaveGame(ISaveGame* pSaveGame);
	virtual void OnLoadGame(ILoadGame* pLoadGame) {};
	virtual void OnLevelEnd(const char* nextLevel) {};
  virtual void OnActionEvent(const SActionEvent& event);
	// ~IGameFrameworkListener

	// IInputEventListener
	virtual bool OnInputEvent(const SInputEvent &rInputEvent);
	virtual bool OnInputEventUI(const SInputEvent &rInputEvent);
	// ~IInputEventListener

	// IFSCommandHandler
	void HandleFSCommand(const char *strCommand,const char *strArgs);
	// ~IFSCommandHandler

	// FS Command Handlers (as we also call a lot of these externally)
	void OnQuickMenuSpeedPreset();
	void OnQuickMenuStrengthPreset();
	void OnQuickMenuDefensePreset();
	void OnCloak();

	// Overridden for specific functionality
	virtual void UpdateRatio();
	virtual void Serialize(TSerialize ser);

	virtual bool OnAction(const ActionId& action, int activationMode, float value);

	virtual void BattleLogEvent(int type, const char *msg, const char *p0=0, const char *p1=0, const char *p2=0, const char *p3=0);
	virtual const char *GetPlayerRank(EntityId playerId, bool shortName=false);
	
	// IPlayerEventListener
	virtual void OnEnterVehicle(IActor *pActor,const char *strVehicleClassName,const char *strSeatName,bool bThirdPerson);
	virtual void OnExitVehicle(IActor *pActor);
	virtual void OnToggleThirdPerson(IActor *pActor,bool bThirdPerson);
	virtual void OnItemDropped(IActor* pActor, EntityId itemId);
	virtual void OnItemUsed(IActor* pActor, EntityId itemId);
	virtual void OnItemPickedUp(IActor* pActor, EntityId itemId);
	virtual void OnStanceChanged(IActor* pActor, EStance stance) {}
	// ~IPlayerEventListener

	// IItemSystemListener
	virtual void OnSetActorItem(IActor *pActor, IItem *pItem );
	virtual void OnDropActorItem(IActor *pActor, IItem *pItem );
	virtual void OnSetActorAccessory(IActor *pActor, IItem *pItem );
	virtual void OnDropActorAccessory(IActor *pActor, IItem *pItem );
	// ~IItemSystemListener

	// IWeaponEventListener
	virtual void OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType,	const Vec3 &pos, const Vec3 &dir, const Vec3 &vel) {}
	virtual void OnStartFire(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void OnStopFire(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void OnStartReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType) {}
	virtual void OnEndReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType) {}
	virtual void OnOutOfAmmo(IWeapon *pWeapon, IEntityClass* pAmmoType) {}
	virtual void OnReadyToFire(IWeapon *pWeapon) {}
	virtual void OnPickedUp(IWeapon *pWeapon, EntityId actorId, bool destroyed) {}
	virtual void OnDropped(IWeapon *pWeapon, EntityId actorId) {}
	virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId) {}
	virtual void OnStartTargetting(IWeapon *pWeapon);
	virtual void OnStopTargetting(IWeapon *pWeapon);
	virtual void OnSelected(IWeapon *pWeapon, bool select) {}
	// ~IWeaponEventListener

	// INanoSuitListener
	virtual void ModeChanged(ENanoMode mode);     // nanomode
	virtual void EnergyChanged(float energy);     // energy
	// ~INanoSuitListener

	// ISubTitleHandler
	virtual void ShowSubtitle(ISound* pSound, bool bShow);
	virtual void ShowSubtitle(const char* subtitleLabel, bool bShow);
	// ~ISubTitleHandler

	// IEquipmentPackManager::IListener
	virtual void OnBeginGiveEquipmentPack()	{	m_quietMode = true;	}
	virtual void OnEndGiveEquipmentPack()	{ m_quietMode = false;	}
	// ~IEquipmentPackManager::IListener

	// ILevelSystemListener - handled by FlashMenuObject
	virtual void OnLoadingStart(ILevelInfo *pLevel);
	virtual void OnLoadingComplete(ILevel *pLevel);
	// ~ILevelSystemListener

	// IViewSystemListener
	virtual bool OnBeginCutScene(IAnimSequence* pSeq, bool bResetFX);
	virtual bool OnEndCutScene(IAnimSequence* pSeq);
	virtual void OnPlayCutSceneSound(IAnimSequence* pSeq, ISound* pSound);
	virtual bool OnCameraChange(const SCameraParams& cameraParams);
	// ~IViewSystemListener

	// Console Variable Changed callbacks (registered in GameCVars)
	static void OnCrosshairCVarChanged(ICVar *pCVar);
	static void OnSubtitleCVarChanged (ICVar* pCVar);
	static void OnSubtitlePanoramicHeightCVarChanged (ICVar* pCVar);
	// 

	enum HUDSubtitleMode
	{
		eHSM_Off = 0,
		eHSM_All = 1,
		eHSM_CutSceneOnly = 2,
	};

	// enable/disable subtitles
	void SetSubtitleMode(HUDSubtitleMode mode);

	//memory optimization
	void UnloadVehicleHUD(bool bShow);
	void UnloadSimpleHUDElements(bool unload);

  //Game rules specific stuff - load/unload stuff
  void LoadGameRulesHUD(bool load);

	struct SWeaponAccessoriesHelpersOffsets
	{
		int iX;
		int iY;
	};
	typedef std::map<string,SWeaponAccessoriesHelpersOffsets> TMapWeaponAccessoriesHelperOffsets;
	std::map<string,TMapWeaponAccessoriesHelperOffsets> m_mapWeaponAccessoriesHelpersOffsets;
	void LoadWeaponAccessories(XmlNodeRef weaponAccessoriesXmlNode);
	void LoadWeaponsAccessories();
	void AdjustWeaponAccessory(const char *szWeapon,const char *szHelper,Vec3 *pvScreenSpace);
	bool UpdateWeaponAccessoriesScreen();
	void UpdateBuyMenuPages();

  void SetInMenu(bool m);

	//sets the color to all hud elements
	void SetHUDColor();
	//set the color of one hud element
	void SetFlashColor(CGameFlashAnimation* pGameFlashAnimation);
	//render boot-up animation
	virtual void ShowBootSequence();
	//render download animation
	virtual void ShowDownloadSequence();
	//render Death effects
	void ShowDeathFX(int type);
	//enable/disable an animation for minimap not available
	void SetMinimapNotAvailable(bool enable) { m_bNoMiniMap = enable; }

	//shows a warning in the flash hud
	void ShowWarningMessage(EWarningMessages message, const char* optionalText = NULL);
	void HandleWarningAnswer(const char* warning = NULL);

	// Airstrike interface
	virtual bool IsAirStrikeAvailable();
	virtual void SetAirStrikeEnabled(bool enabled);
	void SetAirStrikeBinoculars(bool bEnabled);
	void AddAirstrikeEntity(EntityId);
	void ClearAirstrikeEntities();
	void NotifyAirstrikeSucceeded(bool bSucceeded);
	bool StartAirStrike();
	void UpdateAirStrikeTarget(EntityId target);
	void DrawAirstrikeTargetMarkers();
	// ~Airstrike interface

	bool GetShowAllOnScreenObjectives(){ return m_bShowAllOnScreenObjectives; }
	void SetShowAllOnScreenObjectives(bool show) {m_bShowAllOnScreenObjectives = show; }
	const THUDObjectiveList& GetHUDObjectiveList()	{	return m_hudObjectivesList;	}

	// display a flash message
	// label can be a plain text message or a localization label
	// @pos   position: 1=top,2=middle,3=bottom
	void DisplayFlashMessage(const char* label, int pos = 1, const ColorF &col = Col_White, bool formatWStringWithParams = false, const char* paramLabel1 = 0, const char* paramLabel2 = 0, const char* paramLabel3 = 0, const char* paramLabel4 = 0);
	void DisplayOverlayFlashMessage(const char* label, const ColorF &col = Col_White, bool formatWStringWithParams = false, const char* paramLabel1 = 0, const char* paramLabel2 = 0, const char* paramLabel3 = 0, const char* paramLabel4 = 0);
	void DisplayBigOverlayFlashMessage(const char* label, float duration=3.0f, int posX=400, int posY=300, ColorF col = Col_White);
	void DisplayAmmoPickup(const char* ammoName, int ammoAmount);
	void FadeOutBigOverlayFlashMessage();
	void DisplayTempFlashText(const char* label, float seconds, const ColorF &col = Col_White);
	void SetQuietMode(bool enabled);
	bool GetQuietMode() const	{	return m_quietMode;	}

	//Get and Set
	void LockTarget(EntityId target, ELockingType type, bool showtext = true, bool multiple = false);
	void UnlockTarget(EntityId target);

	//Scoreboard
	void AddToScoreBoard(EntityId player, int kills, int deaths, int ping);
	void ForceScoreBoard(bool force);
	void ResetScoreBoard();
  void SetVotingState(EVotingState state, int timeout, EntityId id, const char* descr);

	//RadioButtons & Chat
	void SetRadioButtons(bool active, int buttonNo = 0);
	void ShowGamepadConnected(bool active);
	void ObituaryMessage(EntityId targetId, EntityId shooterId, const char *weaponClassName, int material, int hit_type);

	//Radar
	void AddToRadar(EntityId entityId) const;
	void ShowSoundOnRadar(const Vec3& pos, float intensity = 1.0f) const;
	void SetRadarScanningEffect(bool show);

	//get sub-hud
	ILINE CHUDRadar* GetRadar() {return m_pHUDRadar;}
	ILINE CHUDVehicleInterface* GetVehicleInterface() { return m_pHUDVehicleInterface; }
	ILINE CHUDPowerStruggle* GetPowerStruggleHUD() { return m_pHUDPowerStruggle; }
	ILINE CHUDTextChat* GetMPChat() {return m_pHUDTextChat;}
	ILINE CHUDScopes* GetScopes() { return m_pHUDScopes; }
	ILINE CHUDCrosshair* GetCrosshair() { return m_pHUDCrosshair; }
	ILINE CHUDTagNames* GetTagNames() { return m_pHUDTagNames; }
	ILINE CHUDSilhouettes* GetSilhouettes() { return m_pHUDSilhouettes; }

	//mission objectives
	void UpdateMissionObjectiveIcon(EntityId objective, int friendly, FlashOnScreenIcon iconType, bool forceNoOffset = false, const Vec3 rotationTarget=Vec3(0, 0, 0));
	void UpdateAllMissionObjectives();
	void UpdateObjective(CHUDMissionObjective* pObjective);
	void SetMainObjective(const char* objectiveKey, bool isGoal);
	const char* GetMainObjective();
	void AddOnScreenMissionObjective(IEntity *pEntity, int friendly);
	void SetOnScreenObjective(EntityId pID);
	EntityId GetOnScreenObjective() {return m_iOnScreenObjective; }
	bool IsUnderAttack(IEntity *pEntity);
	ILINE CHUDMissionObjectiveSystem& GetMissionObjectiveSystem() { return m_missionObjectiveSystem; }
	const wchar_t* LocalizeWithParams(const char* label, bool bAdjustActions=true, const char* param1 = 0, const char* param2 = 0, const char* param3 = 0, const char* param4 = 0);
	//~mission objectives

	//BattleStatus code : consult Marco C.
	//	Increases the battle status
	void	TickBattleStatus(float fValue);
	void	UpdateBattleStatus();
	//	Queries the battle status, 0=no battle, 1=full battle
	float	QueryBattleStatus() { return (m_fBattleStatus); }
	float GetBattleRange();

	// Add/Remove external hud objects, they're called after the normal hud objects
	void RegisterHUDObject(CHUDObject* pObject);
	void DeregisterHUDObject(CHUDObject* pObject);

	void FadeCinematicBars(int targetVal);

	//PowerStruggle
	void OnPlayerVehicleBuilt(EntityId playerId, EntityId vehicleId);
	void BuyViaFlash(int item);
	void ShowTutorialText(const wchar_t* text, int pos);
	void SetTutorialTextPosition(int pos);
	void SetTutorialTextPosition(float posX, float posY);
	void MP_ResetBegin();
	void MP_ResetEnd();

	//script function helper
	int CallScriptFunction(IEntity *pEntity, const char *function);

	//HUDPDA.cpp
	//(de)activate buttons in the quick menu 
	void ActivateQuickMenuButton(EQuickMenuButtons button, bool active = true);
	void SetQuickMenuButtonDefect(EQuickMenuButtons button, bool defect = true);
	ILINE bool IsQuickMenuButtonActive(EQuickMenuButtons button) const { return (m_activeButtons & (1<<button))?true:false;}
	ILINE bool IsQuickMenuButtonDefect(EQuickMenuButtons button) const { return (m_defectButtons & (1<<button))?true:false;}
	void ResetQuickMenu();
	void GetGPSPosition(wchar_t *szN,wchar_t *szW);
	//~HUDPDA.cpp

	// Some special HUD fx
	void BreakHUD(int state = 1);  //1 malfunction, 2 dead
	void RebootHUD();

	//bool ShowPDA(bool bShow, int iTab=-1);
	ILINE bool ShowBuyMenu(bool show) { return ShowPDA(show, true); }
	bool ShowPDA(bool show, bool buyMenu = false);
	void ShowObjectives(bool bShow);
	void ShowReviveCycle(bool show);
	void StartPlayerFallAndPlay();
	bool IsPDAActive() const { return m_animPDA.GetVisible(); };
	bool IsBuyMenuActive() const { return (m_pModalHUD == &m_animBuyMenu); };
	bool IsScoreboardActive() const;
	ILINE bool HasTACWeapon() { return m_hasTACWeapon; };
	void SetTACWeapon(bool hasTACWeapon);
	void SetStealthExposure(float exposure);

	void SpawnPointInvalid();

	//interface effects
	void IndicateDamage(EntityId weaponId, Vec3 direction, bool onVehicle = false);
	void IndicateHit(bool enemyIndicator = false,IEntity *pEntity = NULL, bool explosionFeedback = false);
	void ShowKillAreaWarning(bool active, int timer);
	void ShowTargettingAI(EntityId id);
	void ShowProgress(int progress = -1, bool init = false, int posX = 0, int posY = 0, const char* text = NULL, bool topText = true, bool lockingBar = false);
	void FakeDeath(bool revive = false);
	ILINE bool IsFakeDead() { return (m_fPlayerRespawnTimer)?true:false; }
	void ShowDataUpload(bool active);
	void ShowSpectate(bool active);
	void ShowWeaponsOnGround();
	void FireModeSwitch(bool grenades = false);
	ILINE int GetSelectedFiremode() const { return m_curFireMode; }
	void DrawGroundCircle(Vec3 pos, float radius, float thickness = 1.0f, float anglePerSection = 5.0f, ColorB col = ColorB(255,0,0,255), bool aligned = true, float offset = 0.1f, bool useSecondColor = false, ColorB colB = ColorB(0,255,0,255));

	// called from CHUDRadar
	void OnEntityAddedToRadar(EntityId entityId);

	// IHardwareMouseEventListener
	virtual void OnHardwareMouseEvent(int iX,int iY,EHARDWAREMOUSEEVENT eHardwareMouseEvent);

	//HUDSoundImpl
	void PlaySound(ESound eSound, bool play = true);
	void PlayStatusSound(const char* identifier, bool forceSuitSound = false);
	//~HUDSoundImpl

	ILINE CGameFlashAnimation* GetModalHUD() { return m_pModalHUD; }
	ILINE CGameFlashAnimation* GetMapAnim() { return &m_animPDA; }

	// hud event listeners
	struct IHUDListener
	{
		virtual void HUDDestroyed() {};
		virtual void PDAOpened() {};
		virtual void PDAClosed() {};
		virtual void PDATabChanged(int tab) {};
		virtual void OnShowObjectives(bool open) {};
		virtual void WeaponAccessoryChanged(CWeapon* pWeapon, const char* accessory, bool bAddAccessory) {};
		virtual void OnAirstrike(int mode, EntityId entityId) {}; // mode: 0=stop, 1=start
		virtual void OnNightVision(bool bEnabled) {};
		virtual void OnBinoculars(bool bShown) {};
		virtual void OnEntityAddedToRadar(EntityId entityId) {};

		// SNH: adding these for PowerStruggle tutorial
		virtual void OnBuyMenuOpen(bool open, FlashRadarType buyZoneType) {};
		virtual void OnMapOpen(bool open) {};
		virtual void OnBuyMenuItemHover(const char* itemname) {};
		virtual void OnShowBuyMenuPage(int page) {};
		virtual void OnShowScoreBoard() {};
		virtual void OnBuyItem(const char* itemname) {};
	};
	bool RegisterListener(IHUDListener* pListener);
	bool UnRegisterListener(IHUDListener* pListener);

	//assistance restriction
	bool IsInputAssisted();

	CWeapon *GetCurrentWeapon();

	void RecordExplosivePlaced(EntityId eid);
	void RecordExplosiveDestroyed(EntityId eid);
	void HideInventoryOverview();

	ILINE bool IsInCinematic() { return m_cineHideHUD; }
	ILINE bool IsInitializing() { return m_animInitialize.IsLoaded(); }
	ILINE bool InSpectatorMode() { return m_animSpectate.GetVisible(); }
	void RefreshSpectatorHUDText() { m_prevSpectatorMode = -1; }	// mark it as changed, so values are fetched again from the target actor

	CGameFlashAnimation *GetWeaponMenu() { return &m_animWeaponAccessories; }

	// marcok: I know it's bad to even have this in the HUD, but the way gamerulessystem is currently used I don't want to duplicate this elsewhere
	EHUDGAMERULES GetCurrentGameRules()	{	return m_currentGameRules;	}

private:

	//some Update functions
	void UpdateHealth();
	bool UpdateTimers(float frameTime);
	void UpdateWarningMessages(float frameTime);
	void UpdateTeamActionHUD();

	void RequestRevive();

	void InitPDA();
	void HandleFSCommandPDA(const char *strCommand,const char *strArgs);
	void UpdatePlayerAmmo();

	//HUDInterfaceEffects
	void QuickMenuSnapToMode(ENanoMode mode);
	void AutoSnap();
	void TrackProjectiles(CPlayer* pPlayerActor);
	void UpdateProjectileTracker(CGameFlashAnimation &anim, IEntity *pProjectile, uint8 &status, const Vec3 &player);
	void Targetting(EntityId pTargetEntity, bool bStatic);
	void UpdateVoiceChat();
	//~HUDInterfaceEffects

	int FillUpMOArray(std::vector<double> *doubleArray, double a, double b, double c, double d, double e, double f, double g, double h);

	void UpdateCinematicAnim(float frameTime);
	void UpdateSubtitlesAnim(float frameTime);
	void UpdateSubtitlesManualRender(float frameTime);
	void InternalShowSubtitle(const char* subtitleLabel, ISound* pSound, bool bShow);
	// returns pointer to static buffer! so NOT re-entrant!

	void ShowInventoryOverview(const char* curCategory, const char* curItem, bool grenades = false);

	//modal hud management
	void SwitchToModalHUD(CGameFlashAnimation* pModalHUD,bool bNeedMouse);
	bool IsModalHUDAvailable() { return m_pModalHUD == 0; }

	//helper funcs
	void GetProjectionScale(CGameFlashAnimation *pGameFlashAnimation,float *pfScaleX,float *pfScaleY,float *pfHalfUselessSize);
	bool WeaponHasAttachments();

	bool ShowWeaponAccessories(bool enable);

	//member hud objects (sub huds)
	CHUDRadar							*m_pHUDRadar;
	CHUDScore							*m_pHUDScore;
	CHUDTextChat					*m_pHUDTextChat;
	CHUDObituary					*m_pHUDObituary;
	CHUDTextArea					*m_pHUDTextArea;
	CHUDTweakMenu					*m_pHUDTweakMenu;
	CHUDVehicleInterface	*m_pHUDVehicleInterface;
	CHUDPowerStruggle			*m_pHUDPowerStruggle;
	CHUDScopes						*m_pHUDScopes;
	CHUDCrosshair					*m_pHUDCrosshair;
	CHUDTagNames					*m_pHUDTagNames;
	CHUDSilhouettes 			*m_pHUDSilhouettes;

	bool					m_forceScores;

	//cached pointer to renderer
	IRenderer			*m_pRenderer;
	IUIDraw				*m_pUIDraw;

	IFFont *m_pDefaultFont;

	//sound related
	float	m_fLastSoundPlayedCritical;
	float m_fSpeedTimer, m_fStrengthTimer, m_fDefenseTimer;
	float	m_fBattleStatus,m_fBattleStatusDelay;	

	//this manages the mission objectives
	CHUDMissionObjectiveSystem		m_missionObjectiveSystem;
	string		m_currentGoal, m_currentMainObjective;

	//status messages that are displayed on screen and trigger vocals
	std::map<string, string> m_statusMessagesMap;

	//this controls which buttons are currently active (Speed, Strength, Cloak, Weapon, Armor)
	int						m_activeButtons;
	int						m_defectButtons;
	bool					m_acceptNextWeaponCommand;

	//NanoSuit-pointer for suit interaction
	CNanoSuit    *m_pNanoSuit;
	//NanoSuit
	ENanoSlot m_eNanoSlotMax;
	float		m_fHealth;
	float		m_fSuitEnergy;
	
	//interface logic
	CGameFlashAnimation*	m_pModalHUD;
	CGameFlashAnimation*	m_pSwitchScoreboardHUD;
	bool m_bScoreboardCursor;
	bool m_bFirstFrame;
	bool m_bHideCrosshair;
	bool m_bAutosnap;
	bool m_bIgnoreMiddleClick;
	bool m_bLaunchWS;
	bool m_bDestroyInitializePending;
	bool m_bInMenu;
	bool m_bThirdPerson;
	bool m_bNightVisionActive;
	bool m_bTacLock;
	float m_fNightVisionTimer;
	float m_fNightVisionEnergy;
	float m_fXINightVisionActivateTimer;
	float m_fSuitChangeSoundTimer;
	float m_fLastReboot;
	float m_fRemainingReviveCycleTime;
	int m_iBreakHUD;
	int m_iWeaponAmmo;
	int m_iWeaponInvAmmo;
	int m_iWeaponClipSize;
	int m_iGrenadeAmmo;
	int m_playerAmmo, m_playerRestAmmo;
	int m_playerClipSize;
	string m_sGrenadeType;
	int m_iVoiceMode;
	bool m_bMiniMapZooming;
	EntityId m_iPlayerOwnedVehicle;
	bool m_bExclusiveListener;
	float m_fMiddleTextLineTimeout;
	float m_fOverlayTextLineTimeout;

	float m_fBigOverlayTextLineTimeout; // serialized
	string m_bigOverlayText; // serialized
	ColorF m_bigOverlayTextColor; // serialized
	int m_bigOverlayTextX; // serialized
	int m_bigOverlayTextY; // serialized

	int m_iOpenTextChat;
	bool m_bNoMiniMap;
	int m_iProgressBar;
	int m_iProgressBarX, m_iProgressBarY;
	string m_sProgressBarText;
	bool m_bProgressBarTextPos;
	bool m_bProgressLocking;

	//airstrike
	bool m_bAirStrikeAvailable;
	float m_fAirStrikeStarted;
	EntityId m_iAirStrikeTarget;
	std::vector<EntityId> m_possibleAirStrikeTargets;
	EntityId m_objectiveNearCenter;

	//respawn and restart
	float		m_fPlayerDeathTime;
	float		m_fPlayerRespawnTimer;
	float		m_fLastPlayerRespawnEffect;
	bool		m_bRespawningFromFakeDeath;

	bool		m_changedSpawnGroup;

	EntityId m_uiWeapondID;
	bool m_bShowAllOnScreenObjectives;
	EntityId m_iOnScreenObjective;
	bool m_hasTACWeapon;

  EHUDGAMERULES m_currentGameRules;

	CGameFlashAnimation	m_animFriendlyProjectileTracker;
	CGameFlashAnimation	m_animHostileProjectileTracker;
	CGameFlashAnimation	m_animMissionObjective;
	CGameFlashAnimation	m_animPDA;
	CGameFlashAnimation	m_animQuickMenu;
	CGameFlashAnimation	m_animRadarCompassStealth;
	CGameFlashAnimation m_animTacLock;
	CGameFlashAnimation m_animGamepadConnected;
	CGameFlashAnimation m_animBuyMenu;
	CGameFlashAnimation m_animScoreBoard;
	CGameFlashAnimation m_animWeaponAccessories;
	CGameFlashAnimation m_animWarningMessages;
	CGameFlashAnimation m_animDownloadEntities;
	CGameFlashAnimation	m_animInitialize;
	CGameFlashAnimation m_animBreakHUD;
	CGameFlashAnimation m_animRebootHUD;
	CGameFlashAnimation m_animAirStrike;
	CGameFlashAnimation m_animMessages;
	CGameFlashAnimation m_animChat;
	CGameFlashAnimation m_animVoiceChat;
	CGameFlashAnimation m_animKillAreaWarning;
	CGameFlashAnimation m_animDeathMessage;
	CGameFlashAnimation m_animCinematicBar;
	CGameFlashAnimation m_animObjectivesTab;
	CGameFlashAnimation m_animBattleLog;
	CGameFlashAnimation m_animSubtitles;
	CGameFlashAnimation m_animRadioButtons;
	CGameFlashAnimation m_animPlayerPP;
	CGameFlashAnimation m_animProgress;
	CGameFlashAnimation m_animProgressLocking;
	CGameFlashAnimation m_animTutorial;
	CGameFlashAnimation m_animDataUpload;
	CGameFlashAnimation m_animSpectate;
	CGameFlashAnimation m_animNightVisionBattery;
	CGameFlashAnimation m_animHUDCornerLeft;
	CGameFlashAnimation m_animHUDCornerRight;
	CGameFlashAnimation m_animPlayerStats;
	CGameFlashAnimation m_animHexIcons;
	CGameFlashAnimation m_animKillLog;
	CGameFlashAnimation m_animOverlayMessages;
	CGameFlashAnimation m_animBigOverlayMessages;
	CGameFlashAnimation m_animWeaponSelection;
	CGameFlashAnimation m_animSpawnCycle;
	CGameFlashAnimation m_animAmmoPickup;
	CGameFlashAnimation m_animTeamSelection;
	CGameFlashAnimation m_animNetworkConnection;

	// HUD objects
	typedef std::list<CHUDObject *> THUDObjectsList;
	THUDObjectsList m_hudObjectsList;
	THUDObjectsList m_externalHUDObjectList;

	// HUD listener
	std::vector<IHUDListener*> m_hudListeners;
	std::vector<IHUDListener*> m_hudTempListeners;
	std::vector<double> m_missionObjectiveValues;
	int m_missionObjectiveNumEntries;

	EntityId m_entityTargetAutoaimId;

	//gamepad autosnapping
	float m_fAutosnapCursorRelativeX;
	float m_fAutosnapCursorRelativeY;
	float m_fAutosnapCursorControllerX;
	float m_fAutosnapCursorControllerY;
	bool m_bOnCircle;
	float m_lastSpawnUpdate;

	// sv_restart
	bool m_bNoMouse;
	bool m_bNoMove;
	bool m_bSuitMenuFilter;

	//timers
	float m_fDamageIndicatorTimer;
	float m_fPlayerFallAndPlayTimer;
//	float m_fSetAgressorIcon;
//	EntityId  m_agressorIconID;
	int		m_lastPlayerPPSet;

	//entity classes for faster comparison
	IEntityClass *m_pSCAR, *m_pSCARTut, *m_pFY71, *m_pSMG, *m_pDSG1, *m_pShotgun, *m_pLAW, *m_pGauss, *m_pClaymore, *m_pAVMine;

	THUDObjectiveList m_hudObjectivesList;

	//this is a list of sound ids to be able to stop them
	tSoundID m_soundIDs[ESound_Hud_Last];

	//cinematic bar
	enum HUDCineState
	{
		eHCS_None = 0,
		eHCS_Fading
	};


	struct SBuyMenuKeyLog
	{
		
		enum EBMKL_State
		{
			eBMKL_NoInput = 0,
			eBMKL_Tab,
			eBMKL_Frame,
			eBMKL_Button
		};
		
		EBMKL_State m_state;
		int m_tab;
		int m_frame;
		int m_button;
		bool m_tempDisabled;

		void Clear()
		{
			m_state = eBMKL_NoInput;
			m_tab = 0;
			m_frame = 0;
			m_button = 0;
			m_tempDisabled = false;
		};
	};

	struct SSubtitleEntry
	{
		struct Chunk
		{
			float time; // wrt. timeRemaining
			int start;
			int len;
		};

		float      timeRemaining;
		tSoundID   soundId;
		string     key;     // cannot cache LocalizationInfo, would crash on reload dialog data
		wstring    localized;
		wstring    characterName; // p4
		bool       bPersistant;
		bool       bNameShown;
		int        nChunks;       // p4
		int        nCurChunk;     // p4
		Chunk      chunks[10];    // p4

		bool operator==(const char* otherKey) const
		{
			return key == otherKey;
		}

		SSubtitleEntry()
		{
			timeRemaining = 0.0f;
			soundId = INVALID_SOUNDID;
			bPersistant = false;
			bNameShown = false;
			nChunks = 0;
			nCurChunk = 0;
			memset(chunks, 0, sizeof(chunks));
		}
	};
	typedef std::list<SSubtitleEntry> TSubtitleEntries;
	void SubtitleCreateChunks(SSubtitleEntry& entry, const wstring& localizedString);
	void SubtitleAssignCharacterName(SSubtitleEntry& entry);
	void SubtitleAppendCharacterName(const CHUD::SSubtitleEntry& entry, CryFixedWStringT<1024>& locString);

	TSubtitleEntries m_subtitleEntries;
	HUDSubtitleMode m_hudSubTitleMode; // not serialized, as set by cvar
	bool m_bSubtitlesNeedUpdate; // need update (mainly for flash re-pumping)
	HUDCineState m_cineState;
	bool m_cineHideHUD;
	bool m_bCutscenePlaying;
	bool m_bStopCutsceneNextUpdate;
	bool m_bCutsceneAbortPressed;
	bool m_bCutsceneCanBeAborted;
	float m_fCutsceneSkipTimer;
	float m_fBackPressedTime;

	SBuyMenuKeyLog m_buyMenuKeyLog;

	float m_lastNonAssistedInput;
	bool m_hitAssistanceAvailable;

	bool m_quietMode;
	string m_delayedMessage;

	// list of claymore and mines (placed by all players)
	std::list<EntityId> m_explosiveList;
	//list of firemodes
	std::map<string, int> m_hudFireModes;
	int m_curFireMode;
	//list of ammos
	std::map<string, int> m_hudAmmunition;
	std::vector<EntityId> m_trackedProjectiles;
	
	uint8 m_friendlyTrackerStatus;
	uint8 m_hostileTrackerStatus;

	// to prevent localising strings / invoking flash every frame, just check for these changing...
	int m_prevSpectatorMode;
	int m_prevSpectatorHealth;
	EntityId m_prevSpectatorTarget;
};

//-----------------------------------------------------------------------------------------------------

#endif

//-----------------------------------------------------------------------------------------------------
