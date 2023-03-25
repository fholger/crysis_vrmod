////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2007.
// -------------------------------------------------------------------------
//  File name:   SPAnalyst.cpp
//  Version:     v1.00
//  Created:     07/07/2006 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: SPAnalyst to track/serialize some SP stats
// -------------------------------------------------------------------------
//  History:
//	 - 09:11:2007        : Modified - Jan Müller: Added SP gameplay to xml recording
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SPAnalyst.h"

#include <ISerialize.h>
#include <Game.h>
#include <ISaveGame.h>
#include <ILoadGame.h>

#include "GameCVars.h"
#include "Player.h"
#include "HUD/HUD.h"
#include "HUD/HUDRadar.h"

CSPAnalyst::CSPAnalyst() : m_bEnabled(false), m_bChainLoad(false), m_fUpdateTimer(0.0f), m_iTimesFired(0)
{
	IGameFramework* pGF = g_pGame->GetIGameFramework();
	pGF->GetILevelSystem()->AddListener(this);
	pGF->RegisterListener(this, "CSPAnalyst", FRAMEWORKLISTENERPRIORITY_GAME);
}

CSPAnalyst::~CSPAnalyst()
{
	WriteXML();
	IGameFramework* pGF = g_pGame->GetIGameFramework();
	if (m_bEnabled)
		pGF->GetIGameplayRecorder()->UnregisterListener(this);
	pGF->GetILevelSystem()->RemoveListener(this);
	pGF->UnregisterListener(this);
}

void CSPAnalyst::Enable(bool bEnable)
{
	if (m_bEnabled != bEnable)
	{
		if (bEnable)
		{
			Reset();
			g_pGame->GetIGameFramework()->GetIGameplayRecorder()->RegisterListener(this);
		}
		else
			g_pGame->GetIGameFramework()->GetIGameplayRecorder()->UnregisterListener(this);
		m_bEnabled = bEnable;
	}
}

void CSPAnalyst::OnPostUpdate(float fDeltaTime)
{
	if(!m_bEnabled)
		return;

	if(g_pGameCVars->g_spRecordGameplay)
	{
		if(!m_recordingData)
			StartRecording();

		m_fUpdateTimer += fDeltaTime;
		if(m_fUpdateTimer >= g_pGameCVars->g_spGameplayRecorderUpdateRate)
		{
			RecordGameplayFrame();
			m_fUpdateTimer = 0.0f;
		}
	}
}

bool CSPAnalyst::IsPlayer(EntityId entityId) const
{
	// fast path, if already known
	if (m_gameAnalysis.player.entityId == entityId)
		return true;

	// slow path, only first time
	if (IActor *pActor=g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId))
		return pActor->IsPlayer();
	return false;
}

void CSPAnalyst::NewPlayer(IEntity* pEntity)
{
	m_gameAnalysis.player = PlayerAnalysis(pEntity->GetId());
	m_gameAnalysis.player.name = pEntity->GetName();
}

CSPAnalyst::PlayerAnalysis* CSPAnalyst::GetPlayer(EntityId entityId)
{
	if (m_gameAnalysis.player.entityId == entityId)
		return &m_gameAnalysis.player;
	return 0;
}

void CSPAnalyst::OnGameplayEvent(IEntity *pEntity, const GameplayEvent &event)
{
	if(!gEnv->bServer)
		return;

	EntityId entityId = pEntity ? pEntity->GetId() : 0;

	if (entityId && IsPlayer(entityId))
		ProcessPlayerEvent(pEntity, event);

	switch (event.event)
	{
	case eGE_GameStarted:
		{
			const float t = gEnv->pTimer->GetCurrTime();
			// CryLogAlways("[CSPAnalyst]: Game Started at %f", t);
			// if the levelStartTime was serialized, don't touch it
			if (m_gameAnalysis.levelStartTime == 0.0f)
				m_gameAnalysis.levelStartTime = gEnv->pTimer->GetFrameStartTime();
			// if the gameStartTime was serialized, don't touch it
			if (m_gameAnalysis.gameStartTime == 0.0f)
				m_gameAnalysis.gameStartTime = m_gameAnalysis.levelStartTime;
		}
		// called in SP as well
		break;
	case eGE_ItemSelected:
	case eGE_ItemPickedUp:
	case eGE_ItemDropped:
		{
			if(event.extra)
			{
				EntityId entityId = EntityId((int)(event.extra));
				if(IItem* pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(entityId))
				{
					if(event.event == eGE_ItemSelected)
						WriteValue("event.itemSelected", string(pItem->GetEntity()->GetClass()->GetName()));
					else if(event.event == eGE_ItemPickedUp)
						WriteValue("event.itemPickedUp", string(pItem->GetEntity()->GetClass()->GetName()));
					else if(event.event == eGE_ItemDropped)
						WriteValue("event.itemDropped", string(pItem->GetEntity()->GetClass()->GetName()));
				}
			}
		}
		break;
	case eGE_EnteredVehicle:
		{
			if(event.extra)
			{
				EntityId entityId = EntityId((int)(event.extra));
				if(IVehicle *pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(entityId))
					WriteValue("EnteredVehicle", pVehicle->GetEntity()->GetClass()->GetName());
				else
					WriteValue("EnteredVehicle", int(entityId));
			}
		}
		break;
	case eGE_LeftVehicle:
		{
			if(event.extra)
			{
				EntityId entityId = EntityId((int)(event.extra));
				if(IVehicle *pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(entityId))
					WriteValue("EnteredVehicle", pVehicle->GetEntity()->GetClass()->GetName());
				else
					WriteValue("EnteredVehicle", int(entityId));
			}
		}
		break;
	case eGE_WeaponShot:
		{
			if(g_pGameCVars->g_spRecordGameplay && m_bEnabled)
				m_iTimesFired++;
		}
		break;
	case eGE_WeaponReload:
		{
			WriteValue("WeaponReload", int(eGE_WeaponReload));
		}
		break;
	case eGE_Death:
		{
			WriteValue("PlayerDeath", int(eGE_Death));
		}
	case eGE_Disconnected:
		{
			WriteValue("GameDisconnected", int(eGE_Disconnected));
		}
		break;
	case eGE_GameEnd:
		{
			int a = 0;
		}
		// not called in SP yet
		break;
	default:
		break;
	}
}

void CSPAnalyst::ProcessPlayerEvent(IEntity* pEntity, const GameplayEvent& event)
{
	assert (pEntity != 0);
	const EntityId entityId = pEntity->GetId();

	switch (event.event)
	{
	case eGE_Connected:
		{
			NewPlayer(pEntity);
			const float t = gEnv->pTimer->GetCurrTime();
			// CryLogAlways("[CSPAnalyst]: Connected at %f", t);
		}
		break;
	case eGE_Kill:
		if (PlayerAnalysis* pA = GetPlayer(entityId))
			++pA->kills;
		break;
	case eGE_Death:
		if (PlayerAnalysis* pA = GetPlayer(entityId))
			++pA->deaths;
	default:
		break;
	}
}

void CSPAnalyst::PlayerAnalysis::Serialize(TSerialize ser)
{
	ser.BeginGroup("PA");
	ser.Value("kills", kills);
	ser.Value("deaths", deaths);
	ser.EndGroup();
}

void CSPAnalyst::Serialize(TSerialize ser)
{
	if (ser.BeginOptionalGroup("SPAnalyst", true))
	{
		ser.Value("levelStartTime", m_gameAnalysis.levelStartTime);
		ser.Value("gameStartTime", m_gameAnalysis.gameStartTime);
		m_gameAnalysis.player.Serialize(ser);
		ser.EndGroup();
	}
}

void CSPAnalyst::Reset()
{
	m_gameAnalysis = GameAnalysis();
}

void CSPAnalyst::OnLoadingStart(ILevelInfo *pLevelInfo)
{
	if (pLevelInfo == 0)
		return;

	WriteXML(); //close old record

	// when we load 'Island' the Game starts
	if (stricmp(pLevelInfo->GetName(), "island") == 0 || !m_bChainLoad)
	{
		Reset(); // -> so gameStartTime will be assigned in eGE_GameStarted event
	}
	// in any case, reset the level start time, so it will either be restored from SaveGame or 
	// set in eGE_GameStarted event handling
	m_gameAnalysis.levelStartTime = CTimeValue(0.0f);
	m_bChainLoad = false;
}

void CSPAnalyst::OnLoadingComplete(ILevel *pLevel)
{
	if(g_pGameCVars->g_spRecordGameplay)
		StartRecording();
}

void CSPAnalyst::OnSaveGame(ISaveGame* pSaveGame)
{
	if (m_bEnabled == false || pSaveGame == 0)
		return;

	CTimeValue now = gEnv->pTimer->GetFrameStartTime();

	pSaveGame->AddMetadata("sp_kills", m_gameAnalysis.player.kills);
	pSaveGame->AddMetadata("sp_deaths", m_gameAnalysis.player.deaths);
	pSaveGame->AddMetadata("sp_levelPlayTime", (int)((now-m_gameAnalysis.levelStartTime).GetSeconds()));
	pSaveGame->AddMetadata("sp_gamePlayTime", (int)((now-m_gameAnalysis.gameStartTime).GetSeconds()));

	WriteValue("event.gameSaved", pSaveGame->GetFileName());
}

void CSPAnalyst::OnLoadGame(ILoadGame* pLoadGame)
{
	WriteValue("event.gameLoaded", pLoadGame->GetFileName());
}

void CSPAnalyst::OnLevelEnd(const char *nextLevel)
{
	m_bChainLoad = true;
}

void CSPAnalyst::StartRecording()
{
	if(g_pGame->GetIGameFramework()->IsGameStarted())
	{
		m_recordingFileName = string("GameplayRecord_");
		m_recordingFileName.append(g_pGame->CreateSaveGameName());
		m_recordingData = GetISystem()->CreateXmlNode(m_recordingFileName.c_str());
		m_frameCount = 0;
	}
}

void CSPAnalyst::RecordGameplayFrame()
{
	if(!m_recordingData)
		return;

	m_currentRecordingSection = GetISystem()->CreateXmlNode("frame");
	m_frameCount++;
	if(!m_currentRecordingSection)
		m_recordingData = m_currentRecordingSection;
	else
		m_recordingData->addChild(m_currentRecordingSection);

	WriteValue("nr", m_frameCount);
	WriteValue("time", (gEnv->pTimer->GetFrameStartTime()-m_gameAnalysis.levelStartTime).GetSeconds());

	if(CPlayer *pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor()))
	{
		WriteValue("stats.posX", pPlayer->GetEntity()->GetWorldPos().x);
		WriteValue("stats.posY", pPlayer->GetEntity()->GetWorldPos().y);
		WriteValue("stats.posZ", pPlayer->GetEntity()->GetWorldPos().z);

		WriteValue("stats.health", pPlayer->GetHealth());
		WriteValue("stats.kills", m_gameAnalysis.player.kills);
		WriteValue("stats.weapon", pPlayer->GetCurrentItem()?pPlayer->GetCurrentItem()->GetEntity()->GetClass()->GetName():"none");

		if(CNanoSuit *pSuit = pPlayer->GetNanoSuit())
		{
			WriteValue("suit.energy", pSuit->GetSuitEnergy());
			WriteValue("suit.mode", int(pSuit->GetMode()));

			WriteValue("player.nightvision", pSuit->IsNightVisionEnabled()?"on":"off");
		}

		WriteValue("player.timesFired", m_iTimesFired);
		m_iTimesFired = 0;

		if(CHUDRadar *pRadar = g_pGame->GetHUD()->GetRadar())
		{
			//WriteValue("player.scopeBinocs", g_pGame->GetHUD()->GetScopes()->IsBinocularsShown()?"on":"off");

			const std::vector<EntityId>* pEntities = pRadar->GetNearbyEntities();
			int enemyAI, enemyAIAlerted, enemyAIAttacking, enemyAIInVehicle;
			enemyAI = enemyAIAlerted = enemyAIAttacking = enemyAIInVehicle = 0;
			int allAI = 0;
			for(int i = 0; i < pEntities->size(); ++i)
			{
				EntityId id = (*pEntities)[i];
				if(IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(id))
				{
					allAI++;
					IAIObject *pAI = pActor->GetEntity()->GetAI();
					if(pAI && pAI->IsHostile(pPlayer->GetEntity()->GetAI(), false))
					{
						enemyAI++;
						IUnknownProxy *pAIProxy = pAI->GetProxy();
						if(pAIProxy)
						{
							int alertness = pAIProxy->GetAlertnessState();
							if(alertness == 1)
								enemyAIAlerted++;
							else if(alertness > 1)
								enemyAIAttacking++;

							if(pAIProxy->GetLinkedVehicleEntityId())
								enemyAIInVehicle++;
						}
					}
				}
			}

			WriteValue("ai.all", allAI);
			WriteValue("ai.alertness", pRadar->GetStealthValue());
			WriteValue("ai.proximity", enemyAI);
			WriteValue("ai.proximityAlerted", enemyAIAlerted);
			WriteValue("ai.proximityAttacking", enemyAIAttacking);
			WriteValue("ai.proximityVehicle", enemyAIInVehicle);
		}

		if(IVehicle *pVehicle = pPlayer->GetLinkedVehicle())
		{
			WriteValue("vehicle.used", pVehicle->GetEntity()->GetClass()->GetName());
			WriteValue("vehicle.damage", pVehicle->GetDamageRatio(true));
		}
		else
		{
			WriteValue("vehicle.used", 0);
			WriteValue("vehicle.damage", 0);
		}
	}
}

void CSPAnalyst::WriteXML()
{
	if(m_recordingData)
	{
		const string xmlHeader("<?xml version=\"1.0\"?>\n<?mso-application progid=\"Excel.Sheet\"?>\n");

		char path[256];
		CryGetCurrentDirectory(256, path);
		string filename(path);
		filename.append("\\");
		filename.append(m_recordingFileName);
		filename.append(".xml");

		FILE *pFile = gEnv->pCryPak->FOpen(filename,"wb");
		if (pFile)
		{
			_smart_ptr<IXmlStringData> pXmlStrData = m_recordingData->getXMLData( 6000000 );
			gEnv->pCryPak->FWrite((void*)xmlHeader.c_str(), xmlHeader.size(), 1, pFile);
			gEnv->pCryPak->FWrite((void*)pXmlStrData->GetString(), pXmlStrData->GetStringLength(), 1, pFile);
			gEnv->pCryPak->FClose(pFile);
		}
		m_recordingData = NULL;
		m_currentRecordingSection = NULL;
	}
}

void CSPAnalyst::WriteToSection(const char* name, int value)
{
	if(m_currentRecordingSection)
	{
		char buffer[32];
		itoa(value, buffer, 10);
		m_currentRecordingSection->setAttr(name, string(buffer));
	}
}

void CSPAnalyst::WriteToSection(const char* name, const char* value)
{
	WriteToSection(name, string(value));
}

void CSPAnalyst::WriteToSection(const char* name, string value)
{
	if(m_currentRecordingSection)
		m_currentRecordingSection->setAttr(name, value);
}

void CSPAnalyst::WriteToSection(const char* name, float value)
{
	if(m_currentRecordingSection)
	{
		int high = (int)value;
		int low = int((value-(float)high)*1000.0f);
		char highbuffer[16];
		itoa(high, highbuffer, 10);
		char lowbuffer[16];
		itoa(low, lowbuffer, 10);
		string floatAsString(highbuffer);
		floatAsString.append(".");
		floatAsString.append(lowbuffer);
		m_currentRecordingSection->setAttr(name, floatAsString);
	}
}