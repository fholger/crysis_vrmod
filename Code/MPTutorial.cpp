/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: MP Tutorial for PowerStruggle

-------------------------------------------------------------------------
History:
- 12:03:2007: Created by Steve Humphreys

*************************************************************************/

#include "StdAfx.h"

#include "MPTutorial.h"

#include "Game.h"
#include "GameActions.h"
#include "GameCVars.h"
#include "GameRules.h"
#include "HUD/HUDPowerStruggle.h"
#include "HUD/HUDRadar.h"
#include "Player.h"

const float MESSAGE_DISPLAY_TIME = 4.0f;		// default time for msg display (if no audio)
const float MESSAGE_GAP_TIME = 2.0f;				// gap between msgs
const float ENTITY_CHECK_TIME = 0.5f;

// macro to set event names to correspond to the enum value (minus the eTE_ prefix),
//	turn on all event checks
#define SET_TUTORIAL_EVENT_BLANK(eventName, soundName){ \
	m_events[eTE_##eventName].m_name = #eventName; \
	m_events[eTE_##eventName].m_soundName = #soundName; \
	m_events[eTE_##eventName].m_action = ""; \
	m_events[eTE_##eventName].m_removal = eMRC_SoundFinished; \
	m_events[eTE_##eventName].m_status = eMS_Checking; \
	}

#define SET_TUTORIAL_EVENT_BLANK_KEY(eventName, soundName, keyName){ \
	m_events[eTE_##eventName].m_name = #eventName; \
	m_events[eTE_##eventName].m_soundName = #soundName; \
	m_events[eTE_##eventName].m_action = #keyName; \
	m_events[eTE_##eventName].m_removal = eMRC_SoundFinished; \
	m_events[eTE_##eventName].m_status = eMS_Checking; \
}

// useful function in HUDTextEvents.cpp
extern const char* GetSoundKey(const char* soundName);

CMPTutorial::CMPTutorial()
: m_initialised(false)
, m_currentBriefingEvent(eTE_StartGame)
, m_entityCheckTimer(0.0f)
, m_baseCheckTimer(ENTITY_CHECK_TIME / 2.0f)
, m_wasInVehicle(false)
{
	// initialise everything even if disabled - we might be enabled later?
	int enabled = g_pGameCVars->g_PSTutorial_Enabled;
	m_enabled = (enabled != 0);

	m_currentEvent.Reset();

	// blank out all the events initially, and assign their names
	InitEvents();

	// get entity classes we'll need later
	InitEntityClasses();

	// add console command
	gEnv->pConsole->AddCommand("g_psTutorial_TriggerEvent", ForceTriggerEvent, VF_CHEAT, "Trigger an MP tutorial event by name");
	gEnv->pConsole->AddCommand("g_psTutorial_Reset", ResetAllEvents, VF_CHEAT, "Reset powerstruggle tutorial");
}

CMPTutorial::~CMPTutorial()
{
	if(m_initialised && g_pGame->GetHUD())
		g_pGame->GetHUD()->UnRegisterListener(this);
}

void CMPTutorial::InitEvents()
{
	SET_TUTORIAL_EVENT_BLANK_KEY(StartGame, mp_american_bridge_officer_1_brief01,hud_mptutorial_disable);
	SET_TUTORIAL_EVENT_BLANK(ContinueTutorial, mp_american_bridge_officer_1_brief02);
	SET_TUTORIAL_EVENT_BLANK_KEY(Barracks, mp_american_bridge_officer_1_brief03, hud_buy_weapons);
	SET_TUTORIAL_EVENT_BLANK_KEY(BarracksTwo, mp_american_bridge_officer_1_brief03_split, hud_mouseclick);
	SET_TUTORIAL_EVENT_BLANK_KEY(CloseBarracksBuyMenu, mp_american_bridge_officer_1_brief04, hud_show_pda_map);
	SET_TUTORIAL_EVENT_BLANK(OpenMap, mp_american_bridge_officer_1_brief05);
	SET_TUTORIAL_EVENT_BLANK(CloseMap, mp_american_bridge_officer_1_brief06);
	SET_TUTORIAL_EVENT_BLANK(Swingometer, mp_american_bridge_officer_1_brief07);

	SET_TUTORIAL_EVENT_BLANK(TutorialDisabled,None);

	// capturing a factory
	SET_TUTORIAL_EVENT_BLANK(NeutralFactory, mp_american_bridge_officer_1_factory01);
	SET_TUTORIAL_EVENT_BLANK_KEY(CaptureFactory, mp_american_bridge_officer_1_factory01_split, hud_buy_weapons);
	SET_TUTORIAL_EVENT_BLANK_KEY(VehicleBuyMenu, mp_american_bridge_officer_1_factory02, hud_mouseclick);
	SET_TUTORIAL_EVENT_BLANK_KEY(WarBuyMenu, mp_american_bridge_officer_1_factory03, hud_mouseclick);
	SET_TUTORIAL_EVENT_BLANK_KEY(PrototypeBuyMenu, mp_american_bridge_officer_1_factory04, hud_mouseclick);
	SET_TUTORIAL_EVENT_BLANK(PrototypeTab, mp_american_bridge_officer_1_factory04a);
	SET_TUTORIAL_EVENT_BLANK_KEY(NavalBuyMenu, mp_american_bridge_officer_1_factory05, hud_mouseclick);
	SET_TUTORIAL_EVENT_BLANK_KEY(AirBuyMenu, mp_american_bridge_officer_1_factory06, hud_mouseclick);
	SET_TUTORIAL_EVENT_BLANK(CloseBuyMenu, mp_american_bridge_officer_1_factory07);
	SET_TUTORIAL_EVENT_BLANK_KEY(BuyAmmo, mp_american_bridge_officer_1_factory08, buyammo);
	SET_TUTORIAL_EVENT_BLANK(SpawnBunker, mp_american_bridge_officer_1_factory09);
	SET_TUTORIAL_EVENT_BLANK(CaptureAlienSite, mp_american_bridge_officer_1_factory10);
	SET_TUTORIAL_EVENT_BLANK(AlienNoPrototype, mp_american_bridge_officer_1_factory11);
	SET_TUTORIAL_EVENT_BLANK(AllAliensNoPrototype, mp_american_bridge_officer_1_factory12);

	// prototype factory
	SET_TUTORIAL_EVENT_BLANK(EnterPrototypeFactory, mp_american_bridge_officer_1_prototype01);
	SET_TUTORIAL_EVENT_BLANK(Reactor50, mp_american_bridge_officer_1_prototype06);
	SET_TUTORIAL_EVENT_BLANK(Reactor100, mp_american_bridge_officer_1_prototype07);

	SET_TUTORIAL_EVENT_BLANK(ScoreBoard, mp_american_bridge_officer_1_brief08);
	SET_TUTORIAL_EVENT_BLANK(Promotion, mp_american_bridge_officer_1_brief09);

	// in game action
	SET_TUTORIAL_EVENT_BLANK(ClaymoreOrMineBought, mp_american_bridge_officer_1_brief10);
	SET_TUTORIAL_EVENT_BLANK(EnemySpotted, mp_american_bridge_officer_1_ingame01);
	SET_TUTORIAL_EVENT_BLANK(BoardVehicle, mp_american_bridge_officer_1_ingame02);
	SET_TUTORIAL_EVENT_BLANK(EnterHostileFactory, mp_american_bridge_officer_1_ingame03);
	SET_TUTORIAL_EVENT_BLANK(Wounded, mp_american_bridge_officer_1_ingame04);
	SET_TUTORIAL_EVENT_BLANK(Killed, mp_american_bridge_officer_1_ingame05);
	SET_TUTORIAL_EVENT_BLANK(TACTankStarted, mp_american_bridge_officer_1_ingame06);
	SET_TUTORIAL_EVENT_BLANK(TACTankCompleted, mp_american_bridge_officer_1_ingame07);
	SET_TUTORIAL_EVENT_BLANK(TACTankBase, mp_american_bridge_officer_1_ingame08);
	SET_TUTORIAL_EVENT_BLANK(TACLauncherCompleted, mp_american_bridge_officer_1_ingame10a);	
	SET_TUTORIAL_EVENT_BLANK(SingularityStarted, mp_american_bridge_officer_1_ingame09);
	SET_TUTORIAL_EVENT_BLANK(SingularityCompleted, mp_american_bridge_officer_1_ingame10);
	SET_TUTORIAL_EVENT_BLANK(SingularityBase, mp_american_bridge_officer_1_ingame11);
	SET_TUTORIAL_EVENT_BLANK(EnemyNearBase, mp_american_bridge_officer_1_ingame12);
	SET_TUTORIAL_EVENT_BLANK(TurretUnderAttack, mp_american_bridge_officer_1_ingame14);	// nb no 13
	SET_TUTORIAL_EVENT_BLANK(HqUnderAttack, mp_american_bridge_officer_1_ingame16);
	SET_TUTORIAL_EVENT_BLANK(HqCritical, mp_american_bridge_officer_1_ingame17);
	SET_TUTORIAL_EVENT_BLANK(ApproachEnemyBase, mp_american_bridge_officer_1_ingame18);
	SET_TUTORIAL_EVENT_BLANK(ApproachEnemyHq, mp_american_bridge_officer_1_ingame19);
	SET_TUTORIAL_EVENT_BLANK(ApproachEnemySub, mp_american_bridge_officer_1_ingame19_sub);	
	SET_TUTORIAL_EVENT_BLANK(ApproachEnemyCarrier, mp_american_bridge_officer_1_ingame19_carrier);
	SET_TUTORIAL_EVENT_BLANK(GameOverWin, mp_american_bridge_officer_1_win01);
	SET_TUTORIAL_EVENT_BLANK(GameOverLose, mp_american_bridge_officer_1_lose01);

	// now fill out the removal conditions for special events:
	m_events[eTE_TutorialDisabled].m_removal = eMRC_Time;
	m_events[eTE_Barracks].m_removal = eMRC_OpenBuyMenu;
	//m_events[eTE_BarracksTwo].m_removal = eMRC_CloseBuyMenu;
	m_events[eTE_CloseBarracksBuyMenu].m_removal = eMRC_OpenMap;
	m_events[eTE_CaptureFactory].m_removal = eMRC_OpenBuyMenu;
//	m_events[eTE_VehicleBuyMenu].m_removal = eMRC_CloseBuyMenu;
//	m_events[eTE_WarBuyMenu].m_removal = eMRC_CloseBuyMenu;
//	m_events[eTE_PrototypeBuyMenu].m_removal = eMRC_CloseBuyMenu;
//	m_events[eTE_PrototypeTab].m_removal = eMRC_CloseBuyMenu;
//	m_events[eTE_NavalBuyMenu].m_removal = eMRC_CloseBuyMenu;
//	m_events[eTE_AirBuyMenu].m_removal = eMRC_CloseBuyMenu;
//	m_events[eTE_BuyAmmo].m_removal = eMRC_CloseBuyMenu;
}

void CMPTutorial::InitEntityClasses()
{
	IEntityClassRegistry* classReg = gEnv->pEntitySystem->GetClassRegistry();
	m_pHQClass = classReg->FindClass("HQ");
	m_pFactoryClass = classReg->FindClass("Factory");
	m_pAlienEnergyPointClass = classReg->FindClass("AlienEnergyPoint");
	m_pPlayerClass = classReg->FindClass("Player");
	m_pTankClass = classReg->FindClass("US_tank");
	m_pTechChargerClass = classReg->FindClass("TechCharger");
	m_pSpawnGroupClass = classReg->FindClass("SpawnGroup");
	m_pSUVClass = classReg->FindClass("Civ_car1");
}

void CMPTutorial::OnBuyMenuOpen(bool open, FlashRadarType buyZoneType)
{
	if(!m_enabled)
		return;

	// hide any previous messages which are waiting for this event.
	if(open && m_currentEvent.m_msgRemovalCondition == eMRC_OpenBuyMenu)
	{
		if(!m_currentEvent.m_pCurrentSound.get())
			HideMessage();
		else 
			m_currentEvent.m_msgRemovalCondition = eMRC_SoundFinished;
	}
	else if(!open && m_currentEvent.m_msgRemovalCondition == eMRC_CloseBuyMenu)
	{
		if(!m_currentEvent.m_pCurrentSound.get())
			HideMessage();	
		else
			m_currentEvent.m_msgRemovalCondition = eMRC_SoundFinished;
	}

	if(open)
	{
		// depends on type of factory
		bool showPrompt = false;
		switch(buyZoneType)
		{
			case ELTV:				// this seems to be the default value, and occurs when players are at spawn points.
			case EHeadquarter:
			case EHeadquarter2:
			case EBarracks:
				TriggerEvent(eTE_BarracksTwo);
				break;

			case EFactoryVehicle:
				showPrompt = TriggerEvent(eTE_VehicleBuyMenu);
				break;

			case EFactoryAir:
				showPrompt = TriggerEvent(eTE_AirBuyMenu);
				break;

			case EFactoryPrototype:
				showPrompt = TriggerEvent(eTE_PrototypeBuyMenu);
				break;

			case EFactorySea:
				showPrompt = TriggerEvent(eTE_NavalBuyMenu);
				break;

			case EFactoryTank:
				showPrompt = TriggerEvent(eTE_WarBuyMenu);
				break;

			default:
				// other types don't show help
				break;
		}

		/*if(!showPrompt)
		{
			if(ammoPage)
				TriggerEvent(eTE_BuyAmmo);
		}
		*/
	}
	else
	{
		if(buyZoneType == EBarracks
			|| buyZoneType == EHeadquarter
			|| buyZoneType == EHeadquarter2
			|| buyZoneType == ELTV)
			TriggerEvent(eTE_CloseBarracksBuyMenu);
		else
			TriggerEvent(eTE_CloseBuyMenu);
	}
}

void CMPTutorial::OnMapOpen(bool open)
{
	if(!m_enabled)
		return;

	// hide any previous messages which are waiting for this event.
	if(open && m_currentEvent.m_msgRemovalCondition == eMRC_OpenMap)
	{
		if(!m_currentEvent.m_pCurrentSound.get())
			HideMessage();
		else
			m_currentEvent.m_msgRemovalCondition = eMRC_SoundFinished;
	}

	if(open)
	{
		TriggerEvent(eTE_OpenMap);
	}
	else
	{
		TriggerEvent(eTE_CloseMap);
	}
}

void CMPTutorial::OnEntityAddedToRadar(EntityId id)
{
	// if the entity is an enemy player, show the prompt.
	if(m_events[eTE_EnemySpotted].m_status != eMS_Checking)
		return;

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
	if(pEntity && pEntity->GetClass() == m_pPlayerClass)
	{
		if(g_pGame->GetGameRules()->GetTeam(id) != g_pGame->GetGameRules()->GetTeam(g_pGame->GetIGameFramework()->GetClientActor()->GetEntityId()))
		{
			TriggerEvent(eTE_EnemySpotted);
		}
	}
}

void CMPTutorial::OnShowBuyMenuPage(int page)
{
	if(page == CHUDPowerStruggle::E_AMMO)
		TriggerEvent(eTE_BuyAmmo);
	else if(page == CHUDPowerStruggle::E_PROTOTYPES)
		TriggerEvent(eTE_PrototypeBuyMenu);
}

void CMPTutorial::OnShowScoreBoard()
{
	TriggerEvent(eTE_ScoreBoard);
}

void CMPTutorial::OnBuyItem(const char* itemname)
{
	if(m_events[eTE_ClaymoreOrMineBought].m_status == eMS_Checking)
	{
		if(!strcmp(itemname, "claymore") || !strcmp(itemname, "avmine"))
		{
			TriggerEvent(eTE_ClaymoreOrMineBought);
		}
	}
}

void CMPTutorial::OnSoundEvent( ESoundCallbackEvent event,ISound *pSound )
{
	if(event == SOUND_EVENT_ON_PLAYBACK_STOPPED)
	{
		assert(m_currentEvent.m_pCurrentSound == pSound);
		if(pSound)
			pSound->RemoveEventListener(this);

		m_currentEvent.m_pCurrentSound = NULL;

		// only remove text if there's no additional condition on it.
		if(m_currentEvent.m_msgRemovalCondition == eMRC_SoundFinished)
		{
			m_currentEvent.m_msgRemovalCondition = eMRC_Time;
			m_currentEvent.m_msgDisplayTime = 0.0f;		
		}
	}
}

void CMPTutorial::Update()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if(!m_enabled && g_pGameCVars->g_PSTutorial_Enabled)
		EnableTutorialMode(true);
	else if(m_enabled && !g_pGameCVars->g_PSTutorial_Enabled)
		EnableTutorialMode(false);

	m_currentEvent.m_msgDisplayTime -= gEnv->pTimer->GetFrameTime();
	if(!m_enabled)
	{	
		if(m_currentEvent.m_msgDisplayTime < 0.0f && m_currentEvent.m_msgRemovalCondition != eMRC_None)
		{
			m_currentEvent.m_msgRemovalCondition = eMRC_None;
			SAFE_HUD_FUNC(ShowTutorialText(NULL,1));
		}
	}

	// update the text... must be done even if not enabled, to ensure the 'you may reenable...' 
	//	message is shown correctly.
	if(m_currentEvent.m_numChunks > 0)
	{
		// calculate how far through the current sound we are
		CTimeValue now = gEnv->pTimer->GetFrameStartTime();
		float soundTimer = now.GetMilliSeconds() - m_currentEvent.m_soundStartTime;
		assert(soundTimer >= 0);
		float soundPercent = 1.0f;
		if(m_currentEvent.m_soundLength == 0.0f && m_currentEvent.m_pCurrentSound.get())
		{
			m_currentEvent.m_soundLength = m_currentEvent.m_pCurrentSound->GetLengthMs();
		}
		if(m_currentEvent.m_soundLength > 0.0f)
		{
			soundPercent = soundTimer / m_currentEvent.m_soundLength;
		}
		for(int i=m_currentEvent.m_numChunks-1; i > m_currentEvent.m_currentChunk; --i)
		{
			if(m_currentEvent.m_chunks[i].m_startPercent <= soundPercent)
			{
				m_currentEvent.m_currentChunk = i;

				int pos = 2; // 2=bottom, 1=middle
				IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();	
				if(pClientActor && pClientActor->GetLinkedVehicle())
				{
					pos = 1;
				}

				SAFE_HUD_FUNC(ShowTutorialText(m_currentEvent.m_chunks[i].m_text, pos));
				break;
			}
		}
	}

	if(!m_enabled)
		return;

	CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
	if(!pPlayer)
		return;

	// don't start until game begins
	if(pPlayer->GetSpectatorMode() != 0 || g_pGame->GetGameRules()->GetCurrentStateId() != 3)
		return;

	if(!m_initialised)
	{
		m_initialised = true;

		if(g_pGame->GetHUD())
		{
			// register as a HUD listener
			g_pGame->GetHUD()->RegisterListener(this);
		}

		// go through entity list and pull out the factories.
		IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();
		while (!pIt->IsEnd())
		{
			if (IEntity * pEnt = pIt->Next())
			{
				if(pEnt->GetClass() == m_pHQClass)
				{
					m_baseList.push_back(pEnt->GetId());
					//CryLog("Adding HQ %d to list: %d", pEnt->GetId(), m_baseList.size());
				}
				else if(pEnt->GetClass() == m_pAlienEnergyPointClass)
				{
					m_alienEnergyPointList.push_back(pEnt->GetId());
					//CryLog("Adding AEP %d to list: %d", pEnt->GetId(), m_alienEnergyPointList.size());
				}
				else if(pEnt->GetClass() == m_pSpawnGroupClass)
				{
					m_spawnGroupList.push_back(pEnt->GetId());
					//CryLog("Adding spawngroup %d to list: %d", pEnt->GetId(), m_spawnGroupList.size());
				}
				else if(pEnt->GetClass() == m_pFactoryClass)
				{
					m_factoryList.push_back(pEnt->GetId());
					//CryLog("Adding Factory %d to list: %d", pEnt->GetId(), m_factoryList.size());
				}
			}
		}
	}

	// first the briefing events. These are shown in order.
	bool showPrompt = CheckBriefingEvents(pPlayer);

	// player has been killed
	if(pPlayer->GetHealth() <= 0)
	{
		showPrompt = TriggerEvent(eTE_Killed);
	}
	else if(!showPrompt)
	{
		// check each event type here. Which might take a while.

		// entering a neutral factory
		// enter prototype factory
		// enter hostile factory
		// find alien crash
		m_entityCheckTimer -= gEnv->pTimer->GetFrameTime();
		if(m_entityCheckTimer < 0.0f)
		{
			CheckNearbyEntities(pPlayer);
			m_entityCheckTimer = ENTITY_CHECK_TIME;
		}

		// board vehicle and vehicle tutorials
		CheckVehicles(pPlayer);

		// player has been wounded
		if(pPlayer->GetHealth() < pPlayer->GetMaxHealth())
			TriggerEvent(eTE_Wounded);

		// bases
		m_baseCheckTimer -= gEnv->pTimer->GetFrameTime();
		if(m_baseCheckTimer < 0.0f)
		{
			CheckBases(pPlayer);
			m_baseCheckTimer = ENTITY_CHECK_TIME;
		}
	}

	bool promptShown = false;
	for(int i=0; i<eTE_NumEvents; ++i)
	{
		if(m_events[i].m_status == eMS_Waiting)
		{
			if(m_currentEvent.m_msgDisplayTime < -MESSAGE_GAP_TIME)
			{
				ShowMessage(m_events[i]);
			}
			promptShown = true;
			break;
		}
	}

	if(!promptShown	&& (m_currentEvent.m_msgRemovalCondition == eMRC_Time) && (m_currentEvent.m_msgDisplayTime < 0.0f))
	{
		HideMessage();
	}
}

void CMPTutorial::ShowMessage(STutorialEvent& event)
{
	assert(event.m_status == eMS_Waiting);

	// cancel previous sound if necessary
	if(m_currentEvent.m_pCurrentSound.get())
	{
		m_currentEvent.m_pCurrentSound->Stop(ESoundStopMode_AtOnce);
		m_currentEvent.m_pCurrentSound = NULL;
	}

	// also play the sound here
	if(gEnv->pSoundSystem && event.m_soundName.compare("None"))
	{
		string soundName = "mp_american/" + event.m_soundName;
		m_currentEvent.m_pCurrentSound = gEnv->pSoundSystem->CreateSound(soundName.c_str(),FLAG_SOUND_VOICE);
		if (m_currentEvent.m_pCurrentSound.get())
		{
			m_currentEvent.m_pCurrentSound->AddEventListener(this, "mptutorial");
			m_currentEvent.m_msgDisplayTime = 1000.0f;	// will be removed by sound finish event
			m_currentEvent.m_pCurrentSound->Play();
			m_currentEvent.m_soundLength = m_currentEvent.m_pCurrentSound->GetLengthMs();		// NB this almost certainly returns 0 as the sound isn't buffered yet :(
			CTimeValue time = gEnv->pTimer->GetFrameStartTime();
			m_currentEvent.m_soundStartTime = time.GetMilliSeconds();
		}
	}
	else
	{
		m_currentEvent.m_msgDisplayTime = MESSAGE_DISPLAY_TIME;

	}

	// create the localised text string from enum
	string msg;
	wstring localizedString;
	wstring finalString;
	ILocalizationManager *pLocalizationMan = gEnv->pSystem->GetLocalizationManager();
	if(!pLocalizationMan) 
		return;

	if(m_currentEvent.m_pCurrentSound)
	{
		msg = GetSoundKey(m_currentEvent.m_pCurrentSound->GetName());
		pLocalizationMan->GetSubtitle(msg.c_str(), localizedString, true);
	}
	else
	{
		msg = "@mp_Tut" + event.m_name;
		pLocalizationMan->LocalizeString(msg, localizedString);
	}

	if(localizedString.empty())
		return;

	// fill out keynames if necessary
	if(!strcmp(event.m_name, "BoardVehicle"))
	{
		// special case for this event - has 5 actions associated...
		string action1 = "@cc_";
		string action2 = "@cc_";
		string action3 = "@cc_";
		string action4 = "@cc_";
		string action5 = "@cc_";

		action1 += GetKeyName("v_changeseat1");
		action2 += GetKeyName("v_changeseat2");
		action3 += GetKeyName("v_changeseat3");
		action4 += GetKeyName("v_changeseat4");
		action5 += GetKeyName("v_changeseat5");

		wstring wActions[5];
		pLocalizationMan->LocalizeString(action1, wActions[0]);
		pLocalizationMan->LocalizeString(action2, wActions[1]);
		pLocalizationMan->LocalizeString(action3, wActions[2]);
		pLocalizationMan->LocalizeString(action4, wActions[3]);
		pLocalizationMan->LocalizeString(action5, wActions[4]);

		const wchar_t* params[] = {wActions[0].c_str(), wActions[1].c_str(), wActions[2].c_str(), wActions[3].c_str(), wActions[4].c_str()};
		pLocalizationMan->FormatStringMessage(finalString, localizedString, params, 5);
	}
	else if(!event.m_action.empty())
	{
		// first get the key name (eg 'mouse1') and make it into a format the localization system understands

		string action = "@cc_";
		action += GetKeyName(event.m_action.c_str());
		wstring wActionString;
		pLocalizationMan->LocalizeString(action, wActionString);

		// now place this in the right place in the string.
		pLocalizationMan->FormatStringMessage(finalString, localizedString, wActionString.c_str());
	}
	else
	{
		finalString = localizedString;
	}

	// split the text into chunks (to allow super-long strings to fit within the window)
	CreateTextChunks(finalString);

	// set status
	event.m_status = eMS_Displaying;

	// set message removal condition
	m_currentEvent.m_msgRemovalCondition = event.m_removal;
	if(!event.m_soundName.compare("None") && m_currentEvent.m_msgRemovalCondition == eMRC_SoundFinished)
		m_currentEvent.m_msgRemovalCondition = eMRC_Time;	// since for now there're some that don't have audio.
}

void CMPTutorial::CreateTextChunks(const wstring& localizedString)
{
	// look for tokens
	const wchar_t token[] = { L"##" };
	const size_t tokenLen = (sizeof(token) / sizeof(token[0])) - 1;

	size_t len = localizedString.length();
	size_t startPos = 0;
	size_t pos = localizedString.find(token, 0);
	m_currentEvent.m_numChunks = 0;
	size_t MAX_CHUNKS = SCurrentlyPlayingEvent::eMAX_TEXT_CHUNKS;

	while (pos != wstring::npos)
	{
		STutorialTextChunk& chunk = m_currentEvent.m_chunks[m_currentEvent.m_numChunks];
		chunk.m_text.assign(localizedString.c_str() + startPos, pos-startPos);		// for simplicity just copy the text to event buffer
		chunk.m_startPos = startPos;
		chunk.m_length = pos-startPos;
		++m_currentEvent.m_numChunks;

		if (m_currentEvent.m_numChunks == MAX_CHUNKS-1)
		{
			GameWarning("CMPTutorial::CreateTextChunks: tutorial event '%S' exceeds max. number of chunks [%d]", localizedString.c_str(), MAX_CHUNKS);
			break;
		}
		startPos = pos+tokenLen;
		pos = localizedString.find(token, startPos);
	}

	// care about the last one, but only if we found at least one
	// otherwise there is no splitter at all, and it's only one chunk
	if (m_currentEvent.m_numChunks > 0)
	{
		STutorialTextChunk& chunk = m_currentEvent.m_chunks[m_currentEvent.m_numChunks];
		chunk.m_startPos = startPos;
		chunk.m_length = len-startPos;
		chunk.m_text.assign(localizedString.c_str() + startPos, len-startPos);		// for simplicity just copy the text to event buffer
 		++m_currentEvent.m_numChunks;

		// now we have the total number of chunks, calc the string length without tokens
		size_t realCharLength = len - (m_currentEvent.m_numChunks-1) * tokenLen;
		for (size_t i=0; i<m_currentEvent.m_numChunks; ++i)
		{
			STutorialTextChunk& chunk = m_currentEvent.m_chunks[i];

			size_t realPos = chunk.m_startPos - i * tokenLen; // calculated with respect to realCharLength
			float pos = (float) realPos / (float) realCharLength; // pos = [0,1]
			chunk.m_startPercent = pos;
		}
	}
	else
	{
		// just one chunk.
		m_currentEvent.m_chunks[0].m_startPercent = 0.0f;
		m_currentEvent.m_chunks[0].m_startPos = 0;
		m_currentEvent.m_chunks[0].m_text = localizedString;
		m_currentEvent.m_chunks[0].m_length = localizedString.length();
		m_currentEvent.m_numChunks = 1;
	}

	// set to invalid so we trigger text next update.
	m_currentEvent.m_currentChunk = -1;
}

void CMPTutorial::HideMessage()
{
	if(g_pGame && g_pGame->GetHUD())
		g_pGame->GetHUD()->ShowTutorialText(NULL,1);

	if(m_currentEvent.m_pCurrentSound.get())
	{
		m_currentEvent.m_pCurrentSound->Stop(ESoundStopMode_AtOnce);
		m_currentEvent.m_pCurrentSound = NULL;
	}

	m_currentEvent.Reset();
}

const char* CMPTutorial::GetKeyName(const char* action)
{
	IActionMapManager* pAM = gEnv->pGame->GetIGameFramework()->GetIActionMapManager();
	IActionMapIteratorPtr iter = pAM->CreateActionMapIterator();
	while (IActionMap* pMap = iter->Next())
	{
		const char* actionMapName = pMap->GetName();
		IActionMapBindInfoIteratorPtr pIter = pMap->CreateBindInfoIterator();
		while (const SActionMapBindInfo* pInfo = pIter->Next())
		{
			if(!strcmp(pInfo->action, action))
				return pInfo->keys[0];
		}
	}

	return "";
}

bool CMPTutorial::TriggerEvent(ETutorialEvent event)
{
	if(m_events[event].m_status != eMS_Checking)
		return false;

	m_events[event].m_status = eMS_Waiting;

	return true;
}

void CMPTutorial::ForceTriggerEvent(IConsoleCmdArgs* pArgs)
{
	CMPTutorial* pTutorial = g_pGame->GetGameRules()->GetMPTutorial();
	if(!pTutorial)
		return;

	if(pArgs && pArgs->GetArgCount() > 1)
	{
		// parse arg1 to find which event to trigger
		const char* testname = pArgs->GetArg(1);
		for(int i=0; i<eTE_NumEvents; ++i)
		{
			if(!strcmp(testname, pTutorial->m_events[i].m_name.c_str()))
			{
				pTutorial->m_events[i].m_status = eMS_Waiting;
				return;
			}
		}
	}
}

void CMPTutorial::ResetAllEvents(IConsoleCmdArgs* args)
{
	CMPTutorial* pTutorial = g_pGame->GetGameRules()->GetMPTutorial();
	if(pTutorial)
	{
		for(int i=0; i<eTE_NumEvents; ++i)
		{
			pTutorial->m_events[i].m_status = eMS_Checking;
		}
		pTutorial->m_currentBriefingEvent = eTE_FIRST_BRIEFING;
	}
}

void CMPTutorial::EnableTutorialMode(bool enable)
{
	if(enable == false && m_enabled)
	{
		// don't trigger this as normal, just fire off the message
		m_events[eTE_TutorialDisabled].m_status = eMS_Waiting;
		ShowMessage(m_events[eTE_TutorialDisabled]);
		m_currentEvent.m_msgDisplayTime = MESSAGE_DISPLAY_TIME;
	}

	m_enabled = enable;
}

bool CMPTutorial::CheckBriefingEvents(const CPlayer* pPlayer)
{
	bool showPrompt = false;

	if(m_currentBriefingEvent < eTE_FIRST_BRIEFING 
		|| m_currentBriefingEvent > eTE_LAST_BRIEFING
		|| m_events[m_currentBriefingEvent].m_status != eMS_Checking)
		return showPrompt;

	switch(m_currentBriefingEvent)
	{
	case eTE_StartGame:
		// trigger as soon as player spawns for the first time. Must be a better way than the state id?
		if(pPlayer->GetSpectatorMode() == 0 && g_pGame->GetGameRules()->GetCurrentStateId() == 3)
		{
			showPrompt = TriggerEvent(m_currentBriefingEvent);
			m_currentBriefingEvent = eTE_ContinueTutorial;
		}
		break;

	case eTE_ContinueTutorial:
		// trigger right after previous, so long as 'end tutorial' hasn't been pressed. (if it has, tutorial will be disabled)
		if(m_events[eTE_StartGame].m_status != eMS_Checking)
		{
			showPrompt = TriggerEvent(m_currentBriefingEvent);
			m_currentBriefingEvent = eTE_Barracks;
		}
		break;

	case eTE_Barracks:
		// trigger 3s after previous ones dismissed
		if(m_events[eTE_ContinueTutorial].m_status != eMS_Checking)
		{
			showPrompt = TriggerEvent(m_currentBriefingEvent);
			m_currentBriefingEvent = eTE_CloseBarracksBuyMenu;
		}
		break;

	case eTE_CloseBarracksBuyMenu:
		// trigger when player closes buy menu in barracks. This is triggered by the HUD so doesn't need checking here.
		m_currentBriefingEvent = eTE_OpenMap;
		break;

	case eTE_OpenMap:
		// trigger when player opens map for first time. This is triggered by the HUD so doesn't need checking here.
		m_currentBriefingEvent = eTE_CloseMap;
		break;

	case eTE_CloseMap:
		// trigger when player closes map. This is triggered by the HUD so doesn't need checking here.
		m_currentBriefingEvent = eTE_Swingometer;
		break;

	case eTE_Swingometer:
		// triggered after close map message
		if(m_events[eTE_CloseMap].m_status != eMS_Checking)
		{
			showPrompt = TriggerEvent(m_currentBriefingEvent);
			m_currentBriefingEvent = eTE_NullEvent;
		}
		break;
	};

	return showPrompt;
}

bool CMPTutorial::CheckNearbyEntities(const CPlayer *pPlayer)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	// checks for:
	//	eTE_NeutralFactory
	//	eTE_CaptureFactory
	//	eTE_EnterHostileFactory
	//	eTE_EnterPrototypeFactory
	//	eTE_ApproachEnemyBase
	//	eTE_ApproachEnemyHQ
	//	eTE_DestroyEnemyHQ
	//	eTE_CaptureAlienSite
	//	If none of these need checking, don't bother.
	if(! ( m_events[eTE_NeutralFactory].m_status == eMS_Checking
			|| m_events[eTE_CaptureFactory].m_status == eMS_Checking
		  || m_events[eTE_EnterHostileFactory].m_status == eMS_Checking
			|| m_events[eTE_EnterPrototypeFactory].m_status == eMS_Checking
			|| m_events[eTE_ApproachEnemyBase].m_status == eMS_Checking
			|| m_events[eTE_ApproachEnemyHq].m_status == eMS_Checking
			|| m_events[eTE_CaptureAlienSite].m_status == eMS_Checking
			|| m_events[eTE_AllAliensNoPrototype].m_status == eMS_Checking
			|| m_events[eTE_AlienNoPrototype].m_status == eMS_Checking ))
	{
		return false;
	}

	bool showPrompt = false;
	if(!pPlayer)
		return showPrompt;

	Vec3 playerPos = pPlayer->GetEntity()->GetWorldPos();
	int playerTeam = g_pGame->GetGameRules()->GetTeam(pPlayer->GetEntityId());

	// rewritten to avoid iterating through the entity list: cached lists of HQ / energy point / spawn groups
	bool allCrashSites = true;	// does the player's team own all crash sites
	bool PTFactory = false;			// does the player's team own the PT factory
	bool nearCrashSite = false;	// is the player near a crash site
	for(std::list<EntityId>::iterator it = m_alienEnergyPointList.begin(); it != m_alienEnergyPointList.end(); ++it)
	{
		EntityId eid = *it;

		// check team 
		if(playerTeam != g_pGame->GetGameRules()->GetTeam(eid))
			allCrashSites = false;

		IEntity* pEnt = gEnv->pEntitySystem->GetEntity(eid);
		if(pEnt)
		{
			Vec3 vec = pEnt->GetWorldPos() - playerPos;
			float distanceSq = vec.GetLengthSquared();
			if(distanceSq < 500.0f && g_pGame->GetGameRules()->GetTeam(pEnt->GetId()) == playerTeam)
			{
				showPrompt = TriggerEvent(eTE_CaptureAlienSite);
				nearCrashSite = true;
			}
		}
	}

	if(m_events[eTE_AllAliensNoPrototype].m_status == eMS_Checking
		|| m_events[eTE_AlienNoPrototype].m_status == eMS_Checking)
	{
		// if player's team doesn't own the PT factory, they get an additional message...
		for(std::list<EntityId>::iterator factIt = m_factoryList.begin(); factIt != m_factoryList.end(); ++factIt)
		{
			EntityId factoryId = *factIt;
			if(g_pGame->GetHUD())
			{
				if(g_pGame->GetHUD()->GetPowerStruggleHUD()->IsFactoryType(factoryId, CHUDPowerStruggle::E_PROTOTYPES))
				{
					if(g_pGame->GetGameRules()->GetTeam(factoryId) == playerTeam)
					{
						PTFactory = true;
					}
				}
			}
		}

		if(!PTFactory)
		{
			if(allCrashSites)
			{
				// player's team owns all aliens but not the factory
				TriggerEvent(eTE_AllAliensNoPrototype);
			}
			else if(nearCrashSite)
			{
				// player has captured an alien but not yet the prototype factory.
				TriggerEvent(eTE_AlienNoPrototype);
			}
		}
	}

	if(!showPrompt && m_events[eTE_ApproachEnemyBase].m_status == eMS_Checking)
	{
		std::list<EntityId>::iterator it = m_baseList.begin();
		for(; it != m_baseList.end(); ++it)
		{
			IEntity* pEnt = gEnv->pEntitySystem->GetEntity(*it);
			if(pEnt)
			{
				Vec3 vec = pEnt->GetWorldPos() - playerPos;
				float distanceSq = vec.GetLengthSquared();
				int team = g_pGame->GetGameRules()->GetTeam(pEnt->GetId());
				if(team != playerTeam)
				{
					if(distanceSq < 48400.0f)	// 220m distance
						showPrompt = TriggerEvent(eTE_ApproachEnemyBase);
				}
			}
		}
	}

	if(!showPrompt && m_events[eTE_SpawnBunker].m_status == eMS_Checking)
	{
		std::list<EntityId>::iterator it = m_spawnGroupList.begin();
		for(; it != m_spawnGroupList.end(); ++it)
		{
			IEntity* pEnt = gEnv->pEntitySystem->GetEntity(*it);
			if(pEnt)
			{
				Vec3 vec = pEnt->GetWorldPos() - playerPos;
				float distanceSq = vec.GetLengthSquared();
				if(distanceSq <= 80.0f)
				{
					if(g_pGame->GetGameRules()->GetTeam(pEnt->GetId()) == 0)
						showPrompt = TriggerEvent(eTE_SpawnBunker);
				}
			}
		}
	}

	if(!showPrompt)
	{
		std::list<EntityId>::iterator it = m_factoryList.begin();
		for(; it != m_factoryList.end(); ++it)
		{
			IEntity* pEnt = gEnv->pEntitySystem->GetEntity(*it);
			if(pEnt)
			{
				Vec3 vec = pEnt->GetWorldPos() - playerPos;
				float distanceSq = vec.GetLengthSquared();
				if(distanceSq < 500.0f)
				{
					// prompt depends on team and factory type
					bool inPrototypeFactory = g_pGame->GetHUD()->GetPowerStruggleHUD()->IsFactoryType(pEnt->GetId(), CHUDPowerStruggle::E_PROTOTYPES);
					int team = g_pGame->GetGameRules()->GetTeam(pEnt->GetId());
					if(team == 0)
					{
						showPrompt = TriggerEvent(eTE_NeutralFactory);
					}
					else if(team != playerTeam)
					{
						showPrompt = TriggerEvent(eTE_EnterHostileFactory);
					}
					else // team == playerTeam
					{
						// don't show the capture event until neutral message has been shown -
						//	prevents displaying it for factories you already own
						if(m_events[eTE_NeutralFactory].m_status != eMS_Checking)
							showPrompt = TriggerEvent(eTE_CaptureFactory);

						if(inPrototypeFactory)
						{
							showPrompt |= TriggerEvent(eTE_EnterPrototypeFactory);
						}
					}
				}
			}
		}
	}

	return showPrompt;
}

bool CMPTutorial::CheckVehicles(const CPlayer* pPlayer)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	bool showPrompt = false;
	if(!pPlayer)
		return showPrompt;

	IVehicle* pVehicle = pPlayer->GetLinkedVehicle();

	if(m_wasInVehicle && !pVehicle)
	{
		// got out of vehicle. Move HUD box back to normal place.
		SAFE_HUD_FUNC(SetTutorialTextPosition(2));
		m_wasInVehicle = false;
		return showPrompt;
	}
 
 	if(pVehicle && !m_wasInVehicle)
 	{
		// just got in. Move HUD box up so it doesn't overlap vehicle hud.
		SAFE_HUD_FUNC(SetTutorialTextPosition(1));

		m_wasInVehicle = true;

		// generic 'boarding a vehicle' message
		TriggerEvent(eTE_BoardVehicle);
	}

	return showPrompt;
}

bool CMPTutorial::CheckBases(const CPlayer *pPlayer)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	// checks for:
	//	eTE_HqCritical
	//	eTE_HqUnderAttack
	//	eTE_TACTankBase
	//	eTE_SingularityBase
	//	If none need checking, don't bother
	if(! ( m_events[eTE_HqCritical].m_status == eMS_Checking
			|| m_events[eTE_HqUnderAttack].m_status == eMS_Checking
			|| m_events[eTE_TACTankBase].m_status == eMS_Checking
			|| m_events[eTE_SingularityBase].m_status == eMS_Checking ))
	{
		return false;
	}

	// go through the base list and check how close the player is to each.
	bool showPrompt = false;

	int localPlayerTeam = g_pGame->GetGameRules()->GetTeam(pPlayer->GetEntityId());

	std::list<EntityId>::iterator it = m_baseList.begin();
	for(; it != m_baseList.end(); ++it)
	{
		EntityId id = *it;

		IEntity* pBaseEntity = gEnv->pEntitySystem->GetEntity(id);
		if(!pBaseEntity)
			continue;

		int baseTeam = g_pGame->GetGameRules()->GetTeam(pBaseEntity->GetId());

		// first check if the player's HQ is under attack
		if(baseTeam == localPlayerTeam)
		{
			if(IScriptTable* pScriptTable = pBaseEntity->GetScriptTable())
			{
				HSCRIPTFUNCTION getHealth=0;
				if (!pScriptTable->GetValue("GetHealth", getHealth))
					continue;

				float health = 0;
				Script::CallReturn(gEnv->pScriptSystem, getHealth, pScriptTable, health);
				gEnv->pScriptSystem->ReleaseFunc(getHealth);

				// hmm, magic number. Can't see any way of finding the initial health....?
				if(health < 300)
				{
					TriggerEvent(eTE_HqCritical);
				}
				else if(health < 800)	
				{
					TriggerEvent(eTE_HqUnderAttack);
				}
			}

			SEntityProximityQuery query;
			Vec3	pos = pBaseEntity->GetWorldPos();
			query.nEntityFlags = -1;
			query.pEntityClass = m_pTankClass;
			float dim = 50.0f;
			query.box = AABB(Vec3(pos.x-dim,pos.y-dim,pos.z-dim), Vec3(pos.x+dim,pos.y+dim,pos.z+dim));
			gEnv->pEntitySystem->QueryProximity(query);
			for(int i=0; i<query.nCount; ++i)
			{
				IEntity* pEntity = query.pEntities[i];
				if(pEntity)
				{
					int tankTeam = g_pGame->GetGameRules()->GetTeam(pEntity->GetId());

					if(tankTeam != localPlayerTeam && tankTeam != baseTeam)
					{
						// cmp names here.
						if(!strncmp(pEntity->GetName(), "ustactank", 9))
						{
							TriggerEvent(eTE_TACTankBase);
						}
						else if(!strncmp(pEntity->GetName(), "ussingtank", 10))
						{
							TriggerEvent(eTE_SingularityBase);
						}
					}
				}
			}
		}
	}

	return showPrompt;
}
