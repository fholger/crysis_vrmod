/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 02:27:2007: Created by Marco Koegler

*************************************************************************/
#include "StdAfx.h"
#include "FlashAnimation.h"
#include "IFlashPlayer.h"
#include "HUD/FlashPlayerNULL.h"

IFlashPlayer*	CFlashAnimation::s_pFlashPlayerNull = 0;

CFlashAnimation::CFlashAnimation()
{
	m_pFlashPlayer = 0;
	m_dock = eFD_Stretch;
}

CFlashAnimation::~CFlashAnimation()
{
	SAFE_RELEASE(m_pFlashPlayer);
}

IFlashPlayer*	CFlashAnimation::GetFlashPlayer() const
{
	if (m_pFlashPlayer)
		return m_pFlashPlayer;
	else
	{
		// create shared null player
		if (!s_pFlashPlayerNull)
		{
			s_pFlashPlayerNull = new CFlashPlayerNULL();
		}
		return s_pFlashPlayerNull;
	}
}

void CFlashAnimation::SetDock(uint32 eFDock)
{
	m_dock = eFDock;
}

bool CFlashAnimation::LoadAnimation(const char* name)
{
	SAFE_RELEASE(m_pFlashPlayer);

	m_pFlashPlayer = GetISystem()->CreateFlashPlayerInstance();

	if(m_pFlashPlayer && m_pFlashPlayer->Load(name))
		return true;

	SAFE_RELEASE(m_pFlashPlayer);
	return false;
}

void CFlashAnimation::Unload()
{
	SAFE_RELEASE(m_pFlashPlayer);
}

bool CFlashAnimation::IsLoaded() const
{
	return (m_pFlashPlayer != 0);
}

void CFlashAnimation::RepositionFlashAnimation()
{
	if(m_pFlashPlayer)
	{
		IRenderer *pRenderer = gEnv->pRenderer;

		float fMovieRatio		=	((float)m_pFlashPlayer->GetWidth()) / ((float)m_pFlashPlayer->GetHeight());
		float fRenderRatio	=	((float)pRenderer->GetWidth()) / ((float)pRenderer->GetHeight());

		float fWidth				=	pRenderer->GetWidth();
		float fHeight				=	pRenderer->GetHeight();
		float fXPos = 0.0f;
		float fYPos = 0.0f;

		float fXOffset			= (fWidth - (fMovieRatio * fHeight));

		if(fRenderRatio != fMovieRatio && !(m_dock & eFD_Stretch))
		{
			fWidth = fWidth-fXOffset;

			if (m_dock & eFD_Left)
				fXPos = 0;
			else if (m_dock & eFD_Right)
				fXPos = fXOffset;
			else if (m_dock & eFD_Center)
				fXPos = fXOffset * 0.5;
		}

		m_pFlashPlayer->SetViewport(int(fXPos),0,int(fWidth),int(fHeight));
	}
}

void CFlashAnimation::SetVisible(bool visible)
{
	if (m_pFlashPlayer)
		m_pFlashPlayer->SetVisible(visible);
}

bool CFlashAnimation::GetVisible() const
{
	if (m_pFlashPlayer)
		return m_pFlashPlayer->GetVisible();

	return false;
}

bool CFlashAnimation::IsAvailable(const char* pPathToVar) const
{
	if (m_pFlashPlayer)
		return m_pFlashPlayer->IsAvailable(pPathToVar);

	return true;
}

bool CFlashAnimation::SetVariable(const char* pPathToVar, const SFlashVarValue& value)
{
	if (m_pFlashPlayer)
		return m_pFlashPlayer->SetVariable(pPathToVar, value);

	return true;
}

bool CFlashAnimation::CheckedSetVariable(const char* pPathToVar, const SFlashVarValue& value)
{
	if (m_pFlashPlayer && m_pFlashPlayer->IsAvailable(pPathToVar))
		return m_pFlashPlayer->SetVariable(pPathToVar, value);

	return true;
}

bool CFlashAnimation::Invoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult)
{
	if (m_pFlashPlayer)
		return m_pFlashPlayer->Invoke(pMethodName, pArgs, numArgs, pResult);

	return true;
}

bool CFlashAnimation::CheckedInvoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult)
{
	if (m_pFlashPlayer && m_pFlashPlayer->IsAvailable(pMethodName))
		return m_pFlashPlayer->Invoke(pMethodName, pArgs, numArgs, pResult);

	return true;
}
