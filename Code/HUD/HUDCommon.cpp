/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 
	Common functionality for all HUDs using Flash player
	Code which is not game-specific should go here
	Shared by G02 and G04

-------------------------------------------------------------------------
History:
- 22:02:2006: Created by Matthew Jack from original HUD class

*************************************************************************/
#include "StdAfx.h"
#include "HUDCommon.h"
#include "HUD.h"

#include "GameFlashAnimation.h"
#include "GameFlashLogic.h"
#include "Menus/FlashMenuObject.h"

#include "Player.h"
#include "Weapon.h"
#include "Game.h"
#include "GameCVars.h"
#include "GameActions.h"

//-----------------------------------------------------------------------------------------------------
//-- IConsoleArgs
//-----------------------------------------------------------------------------------------------------

void CHUDCommon::HUD(ICVar *pVar)
{
	SAFE_HUD_FUNC(Show(pVar->GetIVal()!=0));
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::ShowGODMode(IConsoleCmdArgs *pConsoleCmdArgs)
{
	CHUD* pHUD = g_pGame->GetHUD();

	if(pHUD && 2 == pConsoleCmdArgs->GetArgCount())
	{
		if(gEnv->pSystem->IsDevMode())
			pHUD->m_bShowGODMode = false;
		else
		{
			if(0 == strcmp(pConsoleCmdArgs->GetArg(1),"0"))
			{
				pHUD->m_bShowGODMode = false;
			}
			else
			{
				pHUD->m_bShowGODMode = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------
//-- ~ IConsoleArgs
//-----------------------------------------------------------------------------------------------------

void CHUDCommon::UpdateRatio()
{
	// try to resize based on any width and height
	for(TGameFlashAnimationsList::iterator i=m_gameFlashAnimationsList.begin(); i!=m_gameFlashAnimationsList.end(); ++i)
	{
		CGameFlashAnimation *pAnim = (*i);
		RepositionFlashAnimation(pAnim);
	}

	m_width		= gEnv->pRenderer->GetWidth();
	m_height	= gEnv->pRenderer->GetHeight();
}

//-----------------------------------------------------------------------------------------------------

CHUDCommon::CHUDCommon() 
{
	m_bShowGODMode = true;
	m_godMode = 0;
	m_iDeaths = 0;

	strcpy(m_strGODMode,"");
	
	m_width					= 0;
	m_height				= 0;

	m_bForceInterferenceUpdate = false;
	m_distortionStrength = 0;
	m_displacementStrength = 0;
	m_alphaStrength = 0;
	m_interferenceDecay = 0;

	m_displacementX = 0;
	m_displacementY = 0;
	m_distortionX = 0;
	m_distortionY = 0;
	m_alpha	= 100;

	m_iCursorVisibilityCounter = 0;

	m_bShow = true;

	gEnv->pConsole->AddCommand("ShowGODMode",ShowGODMode);

	if(gEnv->pHardwareMouse)
	{
		gEnv->pHardwareMouse->AddListener(this);
	}
}

//-----------------------------------------------------------------------------------------------------

CHUDCommon::~CHUDCommon()
{
	if(m_iCursorVisibilityCounter)
	{
		if(gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->DecrementCounter();
		}
		if(g_pGameActions && g_pGameActions->FilterNoMouse())
		{
			g_pGameActions->FilterNoMouse()->Enable(false);
		}
	}

	if(gEnv->pHardwareMouse)
	{
		gEnv->pHardwareMouse->RemoveListener(this);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::Show(bool bShow)
{
	m_bShow = bShow;
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::SetGODMode(uint8 ucGodMode, bool forceUpdate)
{
	if (forceUpdate || m_godMode != ucGodMode)
	{
		m_godMode = ucGodMode;
		m_fLastGodModeUpdate = gEnv->pTimer->GetAsyncTime().GetSeconds();

		if(gEnv->pSystem->IsDevMode())
		{
			if(0 == ucGodMode)
			{
				strcpy(m_strGODMode,"GOD MODE OFF");
				m_iDeaths = 0;
			}
			else if(1 == ucGodMode)
			{
				strcpy(m_strGODMode,"GOD");
			}
			else if(2 == ucGodMode)
			{
				strcpy(m_strGODMode,"Team GOD");
			}
			else if(3 == ucGodMode)
			{
				strcpy(m_strGODMode,"DEMI GOD");
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------
//-- Cursor handling
//-----------------------------------------------------------------------------------------------------

void CHUDCommon::CursorIncrementCounter()
{
	m_iCursorVisibilityCounter++;
	assert(m_iCursorVisibilityCounter >= 0);

	if(1 == m_iCursorVisibilityCounter)
	{
		if(gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->IncrementCounter();
		}
		if(g_pGameActions && g_pGameActions->FilterNoMouse())
		{
			g_pGameActions->FilterNoMouse()->Enable(true);
		}
		UpdateCrosshairVisibility();
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::CursorDecrementCounter()
{
	m_iCursorVisibilityCounter--;
	assert(m_iCursorVisibilityCounter >= 0);

	if(0 == m_iCursorVisibilityCounter)
	{
		if(gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->DecrementCounter();
		}
		if(g_pGameActions && g_pGameActions->FilterNoMouse())
		{
			g_pGameActions->FilterNoMouse()->Enable(false);
		}
		UpdateCrosshairVisibility();
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::Register(CGameFlashAnimation* pAnim)
{
	TGameFlashAnimationsList::iterator it = std::find(m_gameFlashAnimationsList.begin(), m_gameFlashAnimationsList.end(), pAnim);

	if (it == m_gameFlashAnimationsList.end())
		m_gameFlashAnimationsList.push_back(pAnim);
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::Remove(CGameFlashAnimation* pAnim)
{
	TGameFlashAnimationsList::iterator it = std::find(m_gameFlashAnimationsList.begin(), m_gameFlashAnimationsList.end(), pAnim);

	if (it != m_gameFlashAnimationsList.end())
		m_gameFlashAnimationsList.erase(it);
}

//-----------------------------------------------------------------------------------------------------
//-- Starting new interference effect 
//-----------------------------------------------------------------------------------------------------

void CHUDCommon::StartInterference(float distortion, float displacement, float alpha, float decay)
{
	m_distortionStrength = distortion;
	m_displacementStrength = displacement;
	m_alphaStrength = alpha;
	m_interferenceDecay = decay;
	m_bForceInterferenceUpdate = true;
}

//-----------------------------------------------------------------------------------------------------
//-- Creating random distortion and displacements
//-----------------------------------------------------------------------------------------------------

void CHUDCommon::CreateInterference()
{
	if(m_distortionStrength || m_displacementStrength || m_alphaStrength || m_bForceInterferenceUpdate)
	{
		float fDistortionStrengthOverTwo = m_distortionStrength * 0.5f;

		m_distortionX		= (int)((Random()*m_distortionStrength)		-fDistortionStrengthOverTwo);
		m_distortionY		= (int)((Random()*m_distortionStrength)		-fDistortionStrengthOverTwo);
		m_displacementX	= (int)((Random()*m_displacementStrength)	-fDistortionStrengthOverTwo);
		m_displacementY	= (int)((Random()*m_displacementStrength)	-fDistortionStrengthOverTwo);

		m_alpha = 100 - (int)(Random()*m_alphaStrength);
		
		float fMultiplier = m_interferenceDecay * gEnv->pTimer->GetFrameTime();

		m_distortionStrength		-= m_distortionStrength		* fMultiplier;
		m_displacementStrength	-= m_displacementStrength	* fMultiplier;
		m_alphaStrength					-= m_alphaStrength				* fMultiplier;

		if(m_distortionStrength<0.5f)
			m_distortionStrength = 0.0f;
		if(m_displacementStrength<0.5f)
			m_displacementStrength = 0.0f;
		if(m_alphaStrength<1.0f)
			m_alphaStrength = 0.0f;

		if(!m_distortionStrength && !m_displacementStrength && !m_alphaStrength)
		{
			m_displacementX = 0;
			m_displacementY = 0;
			m_distortionX = 0;
			m_distortionY = 0;
			m_alpha	= 100;
		}

		for(TGameFlashAnimationsList::iterator iter=m_gameFlashAnimationsList.begin(); iter!=m_gameFlashAnimationsList.end(); ++iter)
		{
			RepositionFlashAnimation(*iter);
		}

		m_bForceInterferenceUpdate = false;
	}
}

//-----------------------------------------------------------------------------------------------------
//-- Positioning and scaling animations
//-----------------------------------------------------------------------------------------------------

void CHUDCommon::RepositionFlashAnimation(CGameFlashAnimation *pAnimation) const
{
	if(!pAnimation)
		return;

	pAnimation->RepositionFlashAnimation();

	IFlashPlayer *player = pAnimation->GetFlashPlayer();
	if(player)
	{
		int iX0,iY0,iWidth,iHeight;
		float fAspectRatio;
		player->GetViewport(iX0,iY0,iWidth,iHeight,fAspectRatio);
		player->SetViewport((int)(iX0+m_displacementX-m_distortionX*0.5),(int)(m_displacementY-m_distortionY*0.5),iWidth+m_distortionX,iHeight+m_distortionY);
		player->SetVariable("_alpha",SFlashVarValue(m_alpha));
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDCommon::Serialize(TSerialize ser)
{
	ser.Value("distortionStrength",m_distortionStrength);
	ser.Value("displacementStrength",m_displacementStrength);
	ser.Value("alphaStrength",m_alphaStrength);
	ser.Value("interferenceDecay",m_interferenceDecay);

	m_bForceInterferenceUpdate = true;
}

//-----------------------------------------------------------------------------------------------------