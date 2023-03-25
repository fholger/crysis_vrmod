// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include <StdAfx.h>
#include "HUDTextArea.h"
#include "IUIDraw.h"
#include "Game.h"


CHUDTextArea::CHUDTextArea() : m_pos(0, 0), m_fadetime (4.0f)
{
	m_pDefaultFont = GetISystem()->GetICryFont()->GetFont("default");
	CRY_ASSERT(m_pDefaultFont);

	m_pTimer = gEnv->pTimer;
}

CHUDTextArea::~CHUDTextArea()
{

}

void CHUDTextArea::Update(float deltaTime)
{
	float now = m_pTimer->GetAsyncTime().GetMilliSeconds();
	float fadetime = m_fadetime * 1000.0f;

	if(!m_entries.empty())
	{
		int y = 0;
		for (THUDTextEntries::iterator i = m_entries.begin(); i != m_entries.end();)
		{
			float age = now - (*i).time;
			if (age < fadetime)
			{
				float alpha = 1.0f;
				if (age > fadetime/2.0f)
					alpha = 1.0f - ((age - fadetime/2.0f) / (fadetime/2.0f));

				gEnv->pGame->GetIGameFramework()->GetIUIDraw()->DrawText(m_pDefaultFont, 10 + m_pos.x, 8.0f+ m_pos.y +y*20.0f, 18.0f, 18.0f, (*i).message.c_str(), alpha, 0.76f, 0.97f, 0.74f, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP, UIDRAWHORIZONTAL_LEFT, UIDRAWVERTICAL_TOP);
				++i;
				++y;
			}
			else
			{
				THUDTextEntries::iterator next = i;
				++next;
				m_entries.erase(i);
				i = next;
			}
		}
	}	
}

void CHUDTextArea::AddMessage(const char* msg)
{
	SHUDTextEntry entry;

	entry.message = msg;
	entry.time = m_pTimer->GetAsyncTime().GetMilliSeconds();

	m_entries.push_back(entry);
}

void CHUDTextArea::SetPos(const Vec2& pos)
{
	m_pos = pos;
}

void CHUDTextArea::SetFadeTime(float fadetime)
{
	m_fadetime = fadetime;
}

void CHUDTextArea::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->AddContainer(m_entries);
	for (THUDTextEntries::iterator iter = m_entries.begin(); iter != m_entries.end(); ++iter)
		s->Add(iter->message);
}