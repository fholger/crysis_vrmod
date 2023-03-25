/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: HUD obituary - receives player death messages in multiplayer.

-------------------------------------------------------------------------
History:
- 09:06:2006: Created by Márcio Martins

*************************************************************************/
#ifndef __HUDOBITUARY_H__
#define __HUDOBITUARY_H__


#include "HUDObject.h"
#include <IInput.h>


class CHUDObituary : public CHUDObject
{
	static const int OBITUARY_SIZE = 8;
public:
	CHUDObituary();
	~CHUDObituary();

	virtual void Update(float deltaTime);
	virtual void AddMessage(const wchar_t *msg);

	void GetMemoryStatistics(ICrySizer * s);

private:
	IFFont				*m_pDefaultFont;
	wstring				m_deaths[OBITUARY_SIZE];
	CTimeValue		m_deathTimes[OBITUARY_SIZE];
	int						m_deathHead;
	bool					m_empty;
};

#endif //__HUDOBITUARY_H__