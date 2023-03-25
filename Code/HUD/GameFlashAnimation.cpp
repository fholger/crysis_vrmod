/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:

-------------------------------------------------------------------------
History:
- 07:11:2005: Created by Julien Darre

*************************************************************************/
#include "StdAfx.h"
#include "GameFlashAnimation.h"
#include "GameFlashLogic.h"
#include "IFlashPlayer.h"
#include "HUD.h"

//-----------------------------------------------------------------------------------------------------

CGameFlashAnimation::CGameFlashAnimation()
{
	m_flags = 0;
}

//-----------------------------------------------------------------------------------------------------

CGameFlashAnimation::~CGameFlashAnimation()
{
	// deregister us from the HUD Updates and Rendering
	SAFE_HUD_FUNC(Remove(this));

	for(TGameFlashLogicsList::iterator iter=m_gameFlashLogicsList.begin(); iter!=m_gameFlashLogicsList.end(); ++iter)
	{
		SAFE_DELETE(*iter);
	}
}

//-----------------------------------------------------------------------------------------------------
uint32 CGameFlashAnimation::GetFlags() const
{
	return m_flags;
}
//-----------------------------------------------------------------------------------------------------

void CGameFlashAnimation::Init(const char *strFileName, EFlashDock docking, uint32 flags)
{
	Unload();
	m_fileName = strFileName;
	SetDock(docking);
	m_flags = flags;
}

//-----------------------------------------------------------------------------------------------------
bool CGameFlashAnimation::Reload(bool forceUnload)
{
	if (forceUnload)
		Unload();

	if (!m_fileName.empty() && !IsLoaded())
	{
		if (LoadAnimation(m_fileName.c_str()))
		{
			IRenderer *pRenderer = gEnv->pRenderer;
			GetFlashPlayer()->SetViewport(0,0,pRenderer->GetWidth(),pRenderer->GetHeight());
			GetFlashPlayer()->SetBackgroundAlpha(0.0f);

			SAFE_HUD_FUNC(Register(this));

			if (m_flags & eFAF_ThisHandler)
				GetFlashPlayer()->SetFSCommandHandler(g_pGame?g_pGame->GetHUD():NULL);

			SAFE_HUD_FUNC(RepositionFlashAnimation(this));
			SAFE_HUD_FUNC(SetFlashColor(this));

			if (!(m_flags & eFAF_Visible))
				SetVisible(false);

			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------

bool CGameFlashAnimation::Load(const char *strFileName, EFlashDock docking, uint32 flags)
{
	Init(strFileName, docking, flags);
	return Reload();
}

void CGameFlashAnimation::Unload()
{
	// early out
	if (!IsLoaded())
		return;

	CFlashAnimation::Unload();

	// remove us from the HUD updates and rendering
	SAFE_HUD_FUNC(Remove(this));
}

//-----------------------------------------------------------------------------------------------------

void CGameFlashAnimation::AddVariable(const char *strControl,const char *strVariable,const char *strToken,float fScale,float fOffset)
{
	string token(strToken);
	// make sure we don't already have this variable
	TGameFlashLogicsList::const_iterator endIt = m_gameFlashLogicsList.end();
	for (TGameFlashLogicsList::const_iterator i = m_gameFlashLogicsList.begin(); i != endIt; ++i)
	{
		if ((*i)->GetToken() == token)
			return;
	}

	// it's unique ... so we create and add it
	CGameFlashLogic *pGFVariable = new CGameFlashLogic(this);
	pGFVariable->Init(strControl, strVariable, strToken, fScale, fOffset);
	m_gameFlashLogicsList.push_back(pGFVariable);
}

//-----------------------------------------------------------------------------------------------------

void CGameFlashAnimation::ReInitVariables()
{
	TGameFlashLogicsList::const_iterator endIt = m_gameFlashLogicsList.end();
	for (TGameFlashLogicsList::const_iterator i = m_gameFlashLogicsList.begin(); i != endIt; ++i)
	{
		(*i)->ReInit();
	}
}

//-----------------------------------------------------------------------------------------------------

void CGameFlashAnimation::GetMemoryStatistics(ICrySizer * s)
{
	s->AddContainer(m_gameFlashLogicsList);
	for (TGameFlashLogicsList::iterator iter = m_gameFlashLogicsList.begin(); iter != m_gameFlashLogicsList.end(); ++iter)
	{
		(*iter)->GetMemoryStatistics(s);
	}
}
