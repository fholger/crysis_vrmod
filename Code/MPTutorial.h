/*************************************************************************
Crytek Source File.
Copyright (C), *Crytek Studios, *2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: MP Tutorial for PowerStruggle

-------------------------------------------------------------------------
History:
- 12:03:2007: Created by Steve Humphreys

*************************************************************************/

#ifndef __MPTUTORIAL_H__
#define __MPTUTORIAL_H__

#pragma once

#include "HUD/HUD.h"

class CPlayer;

// things we show a tutorial message for
enum ETutorialEvent
{
	eTE_NullEvent = -1,

	// initial briefing. Events between eTE_START_BRIEFING and eTE_END_BRIEFING play in order,
	//	all others in whatever order they happen
	eTE_FIRST_BRIEFING = 0,
	eTE_StartGame = eTE_FIRST_BRIEFING,
	eTE_ContinueTutorial,
	eTE_Barracks,
	eTE_BarracksTwo,
	eTE_CloseBarracksBuyMenu,
	eTE_OpenMap,
	eTE_CloseMap,
	eTE_Swingometer,
	eTE_LAST_BRIEFING = eTE_Swingometer,

	// shown when tutorial is disabled - 'you can reenable it in the menus'
	eTE_TutorialDisabled,

	// capturing a factory
	eTE_NeutralFactory,
	eTE_CaptureFactory,
	eTE_VehicleBuyMenu,
	eTE_WarBuyMenu,
	eTE_PrototypeBuyMenu,
	eTE_PrototypeTab,
	eTE_NavalBuyMenu,
	eTE_AirBuyMenu,
	eTE_CloseBuyMenu,
	eTE_BuyAmmo,
	eTE_SpawnBunker,
	eTE_CaptureAlienSite,
	eTE_AlienNoPrototype,
	eTE_AllAliensNoPrototype,

	// prototype factory
	eTE_EnterPrototypeFactory,
	eTE_Reactor50,
	eTE_Reactor100,

	eTE_ScoreBoard,
	eTE_Promotion,

	// in game action
	eTE_ClaymoreOrMineBought,
	eTE_EnemySpotted,
	eTE_BoardVehicle,
	eTE_EnterHostileFactory,
	eTE_Wounded,
	eTE_Killed,
	eTE_TACTankStarted,
	eTE_TACTankCompleted,
	eTE_TACTankBase,
	eTE_TACLauncherCompleted,
	eTE_SingularityStarted,
	eTE_SingularityCompleted,
	eTE_SingularityBase,
	eTE_EnemyNearBase,
	eTE_TurretUnderAttack,
	eTE_HqUnderAttack,
	eTE_HqCritical,
	eTE_ApproachEnemyBase,
	eTE_ApproachEnemyHq,
	eTE_ApproachEnemySub,
	eTE_ApproachEnemyCarrier,
	eTE_GameOverWin,
	eTE_GameOverLose,

	eTE_NumEvents,
};

// when the currently displayed message will be removed. All except Time - audio must finish before other condition is tested.
enum EMessageRemovalCondition
{
	eMRC_None = 0,								// default state
	eMRC_Time,										// removed after timer runs out
	eMRC_SoundFinished,						// removed when audio finishes
	eMRC_CloseBuyMenu,						// removed when buy menu is closed
	eMRC_OpenBuyMenu,							// removed when buy menu is opened (or leaving factory)
	eMRC_OpenMap,									// removed when map is opened
	eMRC_CloseBuyMenuOrChangeTab,	// removed when buy menu is closed or tab is changed
};

// status of the message
enum EMessageStatus
{
	eMS_Checking = 0,							// keep checking for message display condition
	eMS_Waiting,									// condition has been triggered, waiting to be displayed
	eMS_Displaying,								// message is currently onscreen
	eMS_DontCheck,								// message is now disabled (already shown)
};

// one of these exists for all the above events
struct STutorialEvent
{
	string m_name;				// eg 'StartGame' - basically the ETutorialEvent without the eTE_ prefix. Used to build localised text string.
	string m_soundName;		// eg 'mp_american_bridge_officer_1_brief01' - filenames from the ui_dialog_recording_list.xml
												// NB if sound is not found, text is fetched from the localised text file, not from the subtitle system
	string m_action;			// some messages need a 'press x key' inserted. m_action is the actionmap name, which is replaced by the currently bound key.

	EMessageRemovalCondition m_removal;	// when this message should be removed. Defaults to eMRC_SoundFinished.
	EMessageStatus m_status;	// current status of this message
};

// chunk of an event (audio is one event, text is sometimes split if too long)
struct STutorialTextChunk
{
	wstring m_text;				// text to show
	float m_startPercent;	// when during the audio to show this text

	int m_startPos;				// necessary to work out changeover time
	int m_length;					// " " 
};

// details of the currently playing event
struct SCurrentlyPlayingEvent
{
	enum { eMAX_TEXT_CHUNKS = 10 };

	EMessageRemovalCondition m_msgRemovalCondition;
	_smart_ptr<ISound> m_pCurrentSound;
	float m_msgDisplayTime;						// how long the current message will be shown for

	float m_soundLength;
	float m_soundStartTime;

	STutorialTextChunk m_chunks[eMAX_TEXT_CHUNKS];
	int m_numChunks;
	int m_currentChunk;

	void Reset()
	{
		m_msgRemovalCondition = eMRC_None;
		m_msgDisplayTime = 0.0f;
		m_pCurrentSound = NULL;
		m_soundLength = 0.0f;
		m_soundStartTime = 0.0f;
		m_currentChunk = 0;
		for(int i=0; i<eMAX_TEXT_CHUNKS; ++i)
		{
			m_chunks[i].m_text = L"";
			m_chunks[i].m_length = 0;
			m_chunks[i].m_startPercent = 0.0f;
			m_chunks[i].m_startPos = 0;
		}
		m_numChunks = 0;
	}
};

class CMPTutorial : public CHUD::IHUDListener, public ISoundEventListener
{
public:
	CMPTutorial();
	~CMPTutorial();

	// IHUDListener
	virtual void OnBuyMenuOpen(bool open, FlashRadarType buyZoneType);
	virtual void OnMapOpen(bool open);
	virtual void OnEntityAddedToRadar(EntityId id);
	virtual void OnShowBuyMenuPage(int page);
	virtual void OnShowScoreBoard();
	virtual void OnBuyItem(const char* itemname);
	// ~IHUDListener

	// ISoundEventListener
	virtual void OnSoundEvent( ESoundCallbackEvent event,ISound *pSound );
	// ~ISoundEventListener

	bool TriggerEvent(ETutorialEvent event);
	void EnableTutorialMode(bool enable);
	bool IsEnabled() const			{ return m_enabled; }

	void Update();

private:
	void InitEvents();
	void InitEntityClasses();

	bool CheckBriefingEvents(const CPlayer* pPlayer);
	bool CheckNearbyEntities(const CPlayer* pPlayer);
	bool CheckVehicles(const CPlayer* pPlayer);
	bool CheckBases(const CPlayer* pPlayer);

	void ShowMessage(STutorialEvent& event);
	void HideMessage();

	void CreateTextChunks(const wstring& localizedString);

	static void ForceTriggerEvent(IConsoleCmdArgs* args);
	static void ResetAllEvents(IConsoleCmdArgs* args);
	const char* GetKeyName(const char* action);

	bool m_enabled;
	bool m_initialised;
	bool m_wasInVehicle;

	STutorialEvent m_events[eTE_NumEvents];
	ETutorialEvent m_currentBriefingEvent;
	float m_entityCheckTimer;					// only check nearby entities every so often
	float m_baseCheckTimer;						// only check bases every so often

	SCurrentlyPlayingEvent m_currentEvent;

	// stored entity classes to prevent looking them up repeatedly
	IEntityClass* m_pHQClass;
	IEntityClass* m_pFactoryClass;
	IEntityClass* m_pAlienEnergyPointClass;
	IEntityClass* m_pPlayerClass;
	IEntityClass* m_pTankClass;
	IEntityClass* m_pTechChargerClass;
	IEntityClass* m_pSpawnGroupClass;
	IEntityClass* m_pSUVClass;

	// list of bases (HQ) for quicker checking later
	std::list<EntityId> m_baseList;
	std::list<EntityId> m_spawnGroupList;
	std::list<EntityId> m_alienEnergyPointList;
	std::list<EntityId> m_factoryList;
};


#endif // __MPTUTORIAL_H__