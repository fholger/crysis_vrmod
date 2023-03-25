/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Flash menu screen base class

-------------------------------------------------------------------------
History:
- 07:18:2006: Created by Julien Darre

*************************************************************************/
#include "StdAfx.h"
#include "FlashMenuScreen.h"
#include "IFlashPlayer.h"

//-----------------------------------------------------------------------------------------------------

bool CFlashMenuScreen::Load(const char *strFile)
{
	if (LoadAnimation(strFile))
	{
		IRenderer *pRenderer = gEnv->pRenderer;
		UpdateRatio();
		//m_pFlashPlayer->SetViewport(0,0,pRenderer->GetWidth(),pRenderer->GetHeight());
		GetFlashPlayer()->SetBackgroundAlpha(0.0f);
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuScreen::UpdateRatio()
{
	if(IsLoaded())
	{
		IFlashPlayer* pFlashPlayer = GetFlashPlayer();
		IRenderer* pRenderer = gEnv->pRenderer;

		// Native width/height/ratio
		float fMovieWidth			= (float) pFlashPlayer->GetWidth();
		float fMovieHeight		= (float) pFlashPlayer->GetHeight();
		float fMovieRatio			=	fMovieWidth / fMovieHeight;

		// Current renderer width/height/ratio
		float fRendererWidth	=	(float) pRenderer->GetWidth();
		float fRendererHeight	=	(float) pRenderer->GetHeight();
		float fRendererRatio	=	fRendererWidth / fRendererHeight;

		// Compute viewport so that it fits
		float fViewportX			= 0.0f;
		float fViewportY			= 0.0f;
		float fViewportWidth	= 0.0f;
		float fViewportHeight	= 0.0f;

		/*
			All the Flash files have been designed either in 4/3 or 16/9 aspect ratios in resolutions
			such as 1024x768 or 1366x768. Problem is that minimum aspect ratio is NOT 4/3 but 5/4,
			mainly because of LCD monitors. We need to rescale accordingly in screen resolutions like
			1280x1024	so that we use the file in its 4/3 mode. Hence, there is wasted place in both
			vertical and horizontal areas but this is the only way to fits right.
		*/

		if(fRendererRatio >= 4.0f/3.0f)
		{
			fViewportHeight	= fRendererHeight;

			fViewportX = (fRendererWidth - (fRendererHeight * fMovieRatio)) * 0.5f;
			fViewportWidth = fRendererHeight * fMovieRatio;
		}
		else
		{
			fViewportX = (fRendererWidth - ((fRendererWidth * 3.0f / 4.0f) * fMovieRatio)) * 0.5f;
			fViewportY = (fRendererHeight - (fRendererWidth * 3.0f / 4.0f)) * 0.5f;

			fViewportWidth = (fRendererWidth * 3.0f / 4.0f) * fMovieRatio;
			fViewportHeight = fRendererWidth * 3.0f / 4.0f;
		}

		pFlashPlayer->SetViewport((int)fViewportX,(int)fViewportY,(int)fViewportWidth,(int)fViewportHeight);
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuScreen::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
}

//-----------------------------------------------------------------------------------------------------
