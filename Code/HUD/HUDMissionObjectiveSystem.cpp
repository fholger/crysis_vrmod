// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "ISerialize.h"
#include "Game.h"
#include "HUD.h"
#include "HUDRadar.h"

void CHUDMissionObjective::SetStatus(HUDMissionStatus status)
{
	if (status == m_eStatus)
		return;

	m_eStatus = status;

	assert (m_pMOS != 0);

	SAFE_HUD_FUNC(UpdateObjective(this));

	m_lastTimeChanged = gEnv->pTimer->GetFrameStartTime().GetSeconds();
}

int CHUDMissionObjective::GetColorStatus() const
{
	int colorStatus = 0;
	switch (m_eStatus)
	{
	case ACTIVATED:
		colorStatus = 1; //green
		if(IsSecondary())
			colorStatus = 2; //yellow
		break;
	case CHUDMissionObjective::DEACTIVATED:
		break;
	case CHUDMissionObjective::COMPLETED:
		colorStatus = 4; //grey
		break;
	case CHUDMissionObjective::FAILED:
		colorStatus = 3; //red
		break;
	}

	return colorStatus;
}

const char* CHUDMissionObjectiveSystem::GetMissionObjectiveLongDescription(const char* id)
{
	const char* message = 0;
	CHUDMissionObjective* pObj = GetMissionObjective(id);
	if (pObj)
		message = pObj->GetMessage();
	return message;
}

const char* CHUDMissionObjectiveSystem::GetMissionObjectiveShortDescription(const char* id)
{
	const char* message = 0;
	CHUDMissionObjective* pObj = GetMissionObjective(id);
	if (pObj)
		message = pObj->GetShortDescription();
	return message;
}

void CHUDMissionObjectiveSystem::LoadLevelObjectives(bool forceReloading)
{
	if(!m_bLoadedObjectives || forceReloading)
	{
		m_currentMissionObjectives.clear();
		CryFixedStringT<32> filename;

		filename = "Libs/UI/Objectives_new.xml";
		if(gEnv->bMultiplayer)
			filename = "Libs/UI/MP_Objectives.xml";
		LoadLevelObjectives(filename.c_str());

		//additional objectives
		if(gEnv->bEditor)
		{
			char *levelName;
			char *levelPath;
			g_pGame->GetIGameFramework()->GetEditorLevel(&levelName, &levelPath);
			filename = levelPath;
		}
		else
		{
			ILevel *pLevel = g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel();
			if(!pLevel)
				return;
			filename = pLevel->GetLevelInfo()->GetPath();
		}
		filename.append("/objectives.xml");
		LoadLevelObjectives(filename.c_str());
	}
}

void CHUDMissionObjectiveSystem::LoadLevelObjectives(const char *filename)
{
	XmlNodeRef missionObjectives = GetISystem()->LoadXmlFile(filename);
	if (missionObjectives == 0)
		return;

	for(int tag = 0; tag < missionObjectives->getChildCount(); ++tag)
	{
		XmlNodeRef mission = missionObjectives->getChild(tag);
		const char* attrib;
		const char* objective;
		const char* text;
		const char* optional;
		for(int obj = 0; obj < mission->getChildCount(); ++obj)
		{
			XmlNodeRef objectiveNode = mission->getChild(obj);
			string id(mission->getTag());
			id.append(".");
			id.append(objectiveNode->getTag());
			if(objectiveNode->getAttributeByIndex(0, &attrib, &objective) && objectiveNode->getAttributeByIndex(1, &attrib, &text))
			{
				bool secondaryObjective = false;
				const char* mapLabel = NULL;
				int attribs = objectiveNode->getNumAttributes();
				for(int attribIndex = 2; attribIndex < attribs; ++attribIndex)
				{
					if(objectiveNode->getAttributeByIndex(attribIndex, &attrib, &optional))
					{
						if(attrib)
						{
							if(!stricmp(attrib, "Secondary"))
							{
								if(!stricmp(optional, "true"))
									secondaryObjective = true;
							}
							else if(!stricmp(attrib, "MapLabel"))
								mapLabel = optional;
						}
					}
				}
				m_currentMissionObjectives.push_back(CHUDMissionObjective(this, id.c_str(), objective, text, mapLabel, secondaryObjective));
			}
			else
				GameWarning("Error reading mission objectives.");
		}
	}
}

void CHUDMissionObjectiveSystem::DeactivateObjectives(bool leaveOutRecentlyChanged)
{
	for(int i = 0; i < m_currentMissionObjectives.size(); ++i)
	{
		if(!leaveOutRecentlyChanged ||
			(m_currentMissionObjectives[i].m_lastTimeChanged - gEnv->pTimer->GetFrameStartTime().GetSeconds() > 0.2f))
		{
			bool isSilent = m_currentMissionObjectives[i].IsSilent();
			m_currentMissionObjectives[i].SetSilent(true);
			m_currentMissionObjectives[i].SetStatus(CHUDMissionObjective::DEACTIVATED);
			m_currentMissionObjectives[i].SetSilent(isSilent);
		}
	}
}

CHUDMissionObjective* CHUDMissionObjectiveSystem::GetMissionObjective(const char* id)
{
	std::vector<CHUDMissionObjective>::iterator it;
	for(it = m_currentMissionObjectives.begin(); it != m_currentMissionObjectives.end(); ++it)
	{
		if(!strcmp( (*it).GetID(), id))
			return &(*it);
	}

	return NULL;
}

void CHUDMissionObjectiveSystem::Serialize(TSerialize ser)	//not tested!!
{
	if (ser.IsReading())
		LoadLevelObjectives(true);

	float now = gEnv->pTimer->GetFrameStartTime().GetSeconds();

	for(std::vector<CHUDMissionObjective>::iterator it = m_currentMissionObjectives.begin(); it != m_currentMissionObjectives.end(); ++it)
	{
		ser.BeginGroup("HUD_Objective");
		ser.EnumValue("ObjectiveStatus", (*it).m_eStatus, CHUDMissionObjective::FIRST, CHUDMissionObjective::LAST);
		ser.Value("trackedEntity", (*it).m_trackedEntity);
		ser.EndGroup();

		if(ser.IsReading() && (*it).m_eStatus != CHUDMissionObjective::DEACTIVATED)
		{				
			CHUDMissionObjective obj = *it;
			obj.m_lastTimeChanged = now;
			bool isSilent = obj.IsSilent();
			obj.SetSilent(true);
			SAFE_HUD_FUNC(UpdateObjective(&obj));
			obj.SetSilent(isSilent);
		}
	}
}

void CHUDMissionObjectiveSystem::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->AddContainer(m_currentMissionObjectives);
	for (size_t i=0; i<m_currentMissionObjectives.size(); i++)
		m_currentMissionObjectives[i].GetMemoryStatistics(s);
}

void CHUDMissionObjective::SetTrackedEntity(EntityId entityID)
{	
	if(m_trackedEntity)
		SAFE_HUD_FUNC(GetRadar()->UpdateMissionObjective(m_trackedEntity, false, 0, false));

	m_trackedEntity = entityID;
}
