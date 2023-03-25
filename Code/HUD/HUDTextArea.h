/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Some scrolling fading area of text messages

-------------------------------------------------------------------------
History:
- 13:06:2006: Created by Marco Koegler

*************************************************************************/
#ifndef __HUDTEXTAREA_H__
#define __HUDTEXTAREA_H__


#include "HUDObject.h"
#include <list>

struct ITimer;

class CHUDTextArea : public CHUDObject
{
	struct SHUDTextEntry
	{
		string	message;
		float		time;
	};

	typedef std::list<SHUDTextEntry>	THUDTextEntries;

public:
	CHUDTextArea();
	virtual ~CHUDTextArea();

	virtual void Update(float deltaTime);
	virtual void AddMessage(const char* msg);

	void GetMemoryStatistics(ICrySizer * s);

	// Description:
	//  Set position of top-left corner of text area
	void SetPos(const Vec2& pos);
	
	// Description:
	//	Set fadeout time (in seconds ... default is 4 secs).
	void SetFadeTime(float fadetime);

private:
	IFFont*						m_pDefaultFont;
	THUDTextEntries		m_entries;	// the entries which are currently being displayed
	ITimer*						m_pTimer;
	Vec2							m_pos;			// position of top-left corner
	float							m_fadetime;	// time in seconds for one entry to fade away
};

#endif //__HUDTEXTAREA_H__