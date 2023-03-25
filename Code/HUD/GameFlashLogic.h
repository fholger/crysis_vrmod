/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 18:08:2005: Created by Julien Darre

*************************************************************************/
#ifndef __GAMEFLASHLOGIC_H__
#define __GAMEFLASHLOGIC_H__

//-----------------------------------------------------------------------------------------------------

#include "GameFlashAnimation.h"
#include "IGameTokens.h"

//-----------------------------------------------------------------------------------------------------

#define HUD_MAX_STRING_SIZE 256

//-----------------------------------------------------------------------------------------------------

class CGameFlashLogic : public IGameTokenEventListener
{
public:
	// IGameTokenEventListener
	virtual void OnGameTokenEvent( EGameTokenEvent event,IGameToken *pGameToken );
	// ~IGameTokenEventListener

	void Init(const char *strASControl,const char *strVariable,const char *strToken,float fScale,float fOffset);
	void ReInit();

		CGameFlashLogic(CGameFlashAnimation *pGameFlashAnimation);
	~	CGameFlashLogic();

	void GetMemoryStatistics(ICrySizer * s);
	const string& GetToken() const {	return m_token;	}
private:

	CGameFlashAnimation*	m_pGameFlashAnimation;

	string m_token;
	string m_invoke;

	float m_fScale;
	float m_fOffset;
};

//-----------------------------------------------------------------------------------------------------

inline void ConvertWString(const wstring& s, string& d)
{
  d.resize(wstring::_strlen(s));
  char* dst = d.begin();
  const wchar_t* src = s;

  while (char c=(char)(*src++ & 0xFF))
  {
    *dst++ = c;
  }
}

//-----------------------------------------------------------------------------------------------------

#endif

//-----------------------------------------------------------------------------------------------------