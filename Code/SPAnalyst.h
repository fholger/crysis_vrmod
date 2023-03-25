////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2007.
// -------------------------------------------------------------------------
//  File name:   SPAnalyst.h
//  Version:     v1.00
//  Created:     07/07/2006 by AlexL
//  Compilers:   Visual Studio.NET
//  Description: SPAnalyst to track/serialize some SP stats
// -------------------------------------------------------------------------
//  History:
//	 - 09:11:2007        : Modified - Jan Müller: Added SP gameplay to file recording
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SPANALYST_H__
#define __SPANALYST_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <IGameFramework.h>
#include <IGameplayRecorder.h>
#include <ILevelSystem.h>
#include <SerializeFwd.h>

struct ISaveGame;

class CSPAnalyst : public IGameplayListener, public ILevelSystemListener, public IGameFrameworkListener
{
public:
	CSPAnalyst();
	virtual ~CSPAnalyst();

	// serialize to/from savegame
	void Serialize(TSerialize ser);

	void Enable(bool bEnable);

	void Reset();

	// IGameplayListener
	virtual void OnGameplayEvent(IEntity *pEntity, const GameplayEvent &event);
	// ~IGameplayListener

	// ILevelSystemListener
	virtual void OnLevelNotFound(const char *levelName) {};
	virtual void OnLoadingStart(ILevelInfo *pLevel);
	virtual void OnLoadingComplete(ILevel *pLevel);
	virtual void OnLoadingError(ILevelInfo *pLevel, const char *error) {};
	virtual void OnLoadingProgress(ILevelInfo *pLevel, int progressAmount) {};
	// ~ILevelSystemListener

	// IGameFrameworkListener
	virtual void OnPostUpdate(float fDeltaTime);
	virtual void OnSaveGame(ISaveGame* pSaveGame);
	virtual void OnLoadGame(ILoadGame* pLoadGame);
	virtual void OnLevelEnd(const char* nextLevel);
	virtual void OnActionEvent(const SActionEvent& event) {};
	// ~IGameFrameworkListener

	ILINE void StopRecording() {WriteXML();}
	ILINE int GetTimePlayed() { return (int)((gEnv->pTimer->GetFrameStartTime()-m_gameAnalysis.levelStartTime).GetSeconds()); }

protected:
	struct PlayerAnalysis
	{
		EntityId entityId;
		string   name;
		int      kills;
		int      deaths;

		PlayerAnalysis(EntityId entityId=0)
		: entityId(entityId),
			kills(0),
			deaths(0)
		{
		}
		
		void Serialize(TSerialize ser);
	};

	struct GameAnalysis
	{
		PlayerAnalysis player;
		CTimeValue levelStartTime;  // time when this level has started, restored from SG
		                            // will be set whenever a new level is loaded
		CTimeValue gameStartTime;   // will be set when 'Island' is loaded (also restored from SG)
		                            // will never be touched again
	};

protected:
	bool IsPlayer(EntityId entityId) const;
	void ProcessPlayerEvent(IEntity* pEntity, const GameplayEvent& event);
	void NewPlayer(IEntity* pEntity);
	PlayerAnalysis* GetPlayer(EntityId entityId);

private:

	//************** XML recording
	void StartRecording();
	void WriteXML();
	void RecordGameplayFrame();
	template<class T> void WriteValue(const char* name, T value)
	{
		if(m_recordingData)
			WriteToSection(name, value);
	}
	void WriteToSection(const char* name, int value);
	void WriteToSection(const char* name, float value);
	void WriteToSection(const char* name, const char* value);
	void WriteToSection(const char* name, string value);
	//***************

protected:
	bool m_bEnabled;
	bool m_bChainLoad;
	GameAnalysis m_gameAnalysis;

	//XML recording helpers
	XmlNodeRef	m_recordingData, m_currentRecordingSection;
	string			m_recordingFileName;
	float				m_fUpdateTimer;
	int					m_iTimesFired;
	int					m_frameCount;
};

#endif // #define __SPANALYST_H__