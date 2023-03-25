/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: CHUDMissionObjectives manages the status of a mission objective.
Also the description of the objective is saved here.
CHUDMissionObjectiveSystem encapsulates all objectives of a level.

-------------------------------------------------------------------------
History:
- 01/02:2006: Created by Jan Müller

*************************************************************************/
#ifndef __HUD_MISSION_OBJECTIVE_SYSTEM_H__
#define __HUD_MISSION_OBJECTIVE_SYSTEM_H__

#pragma once

class CHUDMissionObjectiveSystem;
class CHUDMissionObjective
{
	friend class CHUDMissionObjectiveSystem;

public:
	enum HUDMissionStatus
	{
		FIRST = 0,
		DEACTIVATED = FIRST,
		COMPLETED,
		FAILED,
		ACTIVATED,
		LAST = ACTIVATED
	};

	struct HUDMissionStatusChange
	{
		HUDMissionStatus	m_status;
		EntityId					m_following;

		HUDMissionStatusChange() : m_status(FIRST), m_following(0)
		{}
		HUDMissionStatusChange(HUDMissionStatus status, EntityId following) : m_status(status), m_following(following)
		{}
	};

	CHUDMissionObjective() : m_pMOS(0), m_trackedEntity(0), m_eStatus(DEACTIVATED), m_lastTimeChanged(0), m_secondary(false)
	{
	}

	CHUDMissionObjective(CHUDMissionObjectiveSystem* pMOS, const char* id, const char* shortMsg, const char* msg = 0, const char* mapLabel = 0, bool secondaryObjective = false)
											: m_pMOS(pMOS), m_shortMessage(shortMsg), m_screenMessage(msg), m_id(id), m_silent(false), m_mapLabel(mapLabel)
	{
		m_eStatus = DEACTIVATED;
		m_trackedEntity = 0;
		m_lastTimeChanged = 0;
		m_secondary = secondaryObjective;
	}

	HUDMissionStatus GetStatus() const
	{
		return m_eStatus;
	}

	void SetStatus(HUDMissionStatus);

	int GetColorStatus() const;

	ILINE void SetSilent(bool silent) { m_silent = silent; }
	ILINE bool IsSilent() const { return m_silent; }

	ILINE bool IsSecondary() const { return m_secondary; }

	ILINE void SaveMPChange(int team, HUDMissionStatus status, EntityId attachedTo)
	{
		m_mpChangeMap[team] = HUDMissionStatusChange(status, attachedTo);
	}

	ILINE const std::map<int, HUDMissionStatusChange>* GetMPChangeMap() const
	{
		return &m_mpChangeMap;
	}

	ILINE EntityId GetTrackedEntity() const
	{
		return m_trackedEntity;
	}

	void SetTrackedEntity(EntityId entityID);
	
	ILINE bool GetRadar() const
	{
		return m_trackedEntity != 0;
	}

	ILINE bool IsActivated() const
	{
		return (m_eStatus == ACTIVATED);
	}

	ILINE const char* GetShortDescription() const
	{
		return m_shortMessage.c_str();
	}

	ILINE const char* GetID() const
	{
		return m_id.c_str();
	}

	ILINE const char* GetMessage() const
	{
		return m_screenMessage.c_str();
	}

	ILINE const char* GetMapLabel() const
	{
		if(m_mapLabel.size() == 0)
			return m_shortMessage.c_str();
		return m_mapLabel.c_str();
	}

	void Serialize(TSerialize ser)
	{
		ser.Value("m_id", m_id);
		ser.Value("m_shortDescription", m_shortMessage);
		ser.Value("m_screenMessage", m_screenMessage);
		ser.Value("m_mapLabel", m_mapLabel);
		ser.Value("m_trackedEntity", m_trackedEntity, 'eid');
		ser.Value("m_silent", m_silent);
		ser.Value("m_secondary", m_secondary);
		ser.EnumValue("m_eStatus", m_eStatus, FIRST, LAST);
	}

	void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(m_shortMessage);
		s->Add(m_screenMessage);
		s->Add(m_id);
		s->AddContainer(m_mpChangeMap);
		s->Add(m_mapLabel);
	}

protected:

	ILINE void SetMOS(CHUDMissionObjectiveSystem* pMOS)
	{
		m_pMOS = pMOS;
	}

private:
	CHUDMissionObjectiveSystem* m_pMOS;
	string						m_shortMessage;
	string						m_screenMessage;
	string						m_id;
	string						m_mapLabel;
	EntityId          m_trackedEntity;
	HUDMissionStatus	m_eStatus;
	float							m_lastTimeChanged;
	bool							m_silent;
	bool							m_secondary;

	std::map<int, HUDMissionStatusChange> m_mpChangeMap;
};

class CHUDMissionObjectiveSystem
{
	//this is a list of the current level's mission objectives ...
	std::vector<CHUDMissionObjective> m_currentMissionObjectives;
	bool m_bLoadedObjectives;

public:

	CHUDMissionObjectiveSystem() : m_bLoadedObjectives(false)
	{
	}

	const std::vector<CHUDMissionObjective>& GetObjectives() const
	{
		return m_currentMissionObjectives;
	}


	void GetMemoryStatistics(ICrySizer * s);
	//get the description of the objective
	const char* GetMissionObjectiveLongDescription(const char* objectiveShort);

	//get the name of the objective
	const char* GetMissionObjectiveShortDescription(const char* id);

	//loads the level's mission objectives from a XML file
	void LoadLevelObjectives(bool forceReloading = false);

	//deactivate all loaded objectives
	void DeactivateObjectives(bool leaveOutRecentlyChanged = false);

	//get a pointer to the objective (NULL if not available)
	//TODO: don't return ptr into a vector! If vector changes, ptr is trash!
	CHUDMissionObjective* GetMissionObjective(const char* id);

	void Serialize(TSerialize ser);	//not tested!!

private:
	void LoadLevelObjectives(const char *filename);
};

#endif