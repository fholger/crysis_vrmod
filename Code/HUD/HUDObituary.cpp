// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "HUDObituary.h"
#include "IUIDraw.h"
#include "Game.h"

CHUDObituary::CHUDObituary()
:	m_deathHead(0), m_empty(true)
{
	m_pDefaultFont = GetISystem()->GetICryFont()->GetFont("default");
	CRY_ASSERT(m_pDefaultFont);
}

CHUDObituary::~CHUDObituary()
{
}

void CHUDObituary::Update(float fDeltaTime)
{
	if(!m_empty)
	{
		CTimeValue now = gEnv->pTimer->GetCurrTime();

		m_empty=true;
		int msg=m_deathHead;
		int y=0;
		for (int i=0; i<OBITUARY_SIZE; i++)
		{
			float age=(now-m_deathTimes[msg]).GetMilliSeconds();
			if(age<8000.0f)
			{
				float alpha=1.0f;
				if (age>3500.0f)
					alpha=1.0f-((age-3500.0f)/4500.0f);
				gEnv->pGame->GetIGameFramework()->GetIUIDraw()->DrawTextW(m_pDefaultFont, 10, 8.0f+y*20.0f, 18.0f, 18.0f, m_deaths[msg].c_str(), alpha, 0.76f, 0.97f, 0.74f, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP);
				m_empty=false;
				y++;
			}
			msg++;
			if(msg>OBITUARY_SIZE-1)
				msg=0;
		}
	}
}

void CHUDObituary::AddMessage(const wchar_t *msg)
{
	m_deaths[m_deathHead] = msg;
	m_deathTimes[m_deathHead] = gEnv->pTimer->GetCurrTime();
	m_deathHead++;
	
	if(m_deathHead > OBITUARY_SIZE-1)
		m_deathHead = 0;

	m_empty = false;
}

void CHUDObituary::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	for (int i=0; i<OBITUARY_SIZE; i++)
		s->Add(m_deaths[i]);
}