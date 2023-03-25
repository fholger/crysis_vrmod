/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 	
	Sound moods (from game side)

-------------------------------------------------------------------------
History:
- 31:07:2007: Created by Julien Darre

*************************************************************************/
#include "StdAfx.h"
#include "SoundMoods.h"

//-----------------------------------------------------------------------------------------------------

CSoundMoods::CSoundMoods()
{
	m_pSoundMoodManager = NULL;

	// Note: there is no sound system in a dedicated server
	if(gEnv->pSoundSystem)
	{
		m_pSoundMoodManager = gEnv->pSoundSystem->GetIMoodManager();
		CRY_ASSERT(m_pSoundMoodManager);
	}

	m_vecSoundMoods.resize(32);
}

//-----------------------------------------------------------------------------------------------------

CSoundMoods::~CSoundMoods()
{
}

//-----------------------------------------------------------------------------------------------------

void CSoundMoods::AddSoundMood(const char *szSoundMood,uint32 uiFadeIn,float fDuration,uint32 uiFadeOut,float fFade)
{
	if(!m_pSoundMoodManager)
		return;

	SSoundMood *pSoundMood = NULL;

	// First pass: we don't want to play the same sound mood twice
	for(TVectorSoundMoods::iterator iter=m_vecSoundMoods.begin(); iter!=m_vecSoundMoods.end(); ++iter)
	{
		SSoundMood *pIterSoundMood = &(*iter);
		if(!strcmp(pIterSoundMood->strSoundMood,szSoundMood))
		{
			pSoundMood = pIterSoundMood;
			break;
		}
	}

	if(!pSoundMood)
	{
		// Second pass: create it if not already in our vector
		for(TVectorSoundMoods::iterator iter=m_vecSoundMoods.begin(); iter!=m_vecSoundMoods.end(); ++iter)
		{
			SSoundMood *pIterSoundMood = &(*iter);
			if(!pIterSoundMood->bValid)
			{
				pSoundMood = pIterSoundMood;
				break;
			}
		}
	}

	CRY_ASSERT_MESSAGE(pSoundMood,"Vector size should be increased !");

	if(pSoundMood)
	{
		if(pSoundMood->bValid)
		{
			m_pSoundMoodManager->UpdateSoundMood(szSoundMood,0.0f,0);
		}

		pSoundMood->strSoundMood	= szSoundMood;
		pSoundMood->uiFadeOutTime	= (uint32)(gEnv->pTimer->GetCurrTime() + (uiFadeIn + fDuration) / 1000.0f);
		pSoundMood->uiFadeOut			= uiFadeOut;
		pSoundMood->bValid				=	true;
		pSoundMood->bUnlimited		= fDuration == -1.0f ? true : false;

		m_pSoundMoodManager->RegisterSoundMood(szSoundMood);
		m_pSoundMoodManager->UpdateSoundMood(szSoundMood,fFade,uiFadeIn);
	}
}

//-----------------------------------------------------------------------------------------------------

void CSoundMoods::AddSoundMood(ESOUNDMOOD eSoundMood,float fPercent)
{
	if(!m_pSoundMoodManager)
		return;

	switch(eSoundMood)
	{
	case SOUNDMOOD_ENTER_BINOCULARS:
		AddSoundMood("cryvision",500,-1.0f,0,1.0f);
		break;
	case SOUNDMOOD_LEAVE_BINOCULARS:
		RemoveSoundMood("cryvision",0.0f,500);
		break;
	case SOUNDMOOD_ENTER_CLOAK:
		AddSoundMood("cloak",1000,-1.0f,0,1.0f);
		break;
	case SOUNDMOOD_LEAVE_CLOAK:
		RemoveSoundMood("cloak",0.0f,1000);
		break;
	case SOUNDMOOD_ENTER_CONCENTRATION:
		AddSoundMood("concentration",1000,-1.0f,0,1.0f);
		break;
	case SOUNDMOOD_LEAVE_CONCENTRATION:
		RemoveSoundMood("concentration",0.0f,500);
		break;
	case SOUNDMOOD_EXPLOSION:
		AddSoundMood("explosion",1000,0.0f,3000,fPercent/100.0f);
		break;
	case SOUNDMOOD_ENTER_FREEZE:
		AddSoundMood("frozen",1500,-1.0f,0,1.0f);
		break;
	case SOUNDMOOD_LEAVE_FREEZE:
		RemoveSoundMood("frozen",0.0f,2000);
		break;
	case SOUNDMOOD_LOWHEALTH:
		AddSoundMood("low_health",0,3000.0f,1000,fPercent/100.0f);
		break;
	default:
		CRY_ASSERT(0);
		break;
	}
}

//-----------------------------------------------------------------------------------------------------

void CSoundMoods::RemoveSoundMood(const char *szSoundMood,float fFade,uint32 uiFadeOut)
{
	if(!m_pSoundMoodManager)
		return;

	for(TVectorSoundMoods::iterator iter=m_vecSoundMoods.begin(); iter!=m_vecSoundMoods.end(); ++iter)
	{
		SSoundMood *pSoundMood = &(*iter);
		if(pSoundMood->bValid && !strcmp(pSoundMood->strSoundMood,szSoundMood))
		{
			m_pSoundMoodManager->UpdateSoundMood(szSoundMood,fFade,uiFadeOut);
			pSoundMood->bValid = false;
			return;
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CSoundMoods::Serialize(TSerialize ser)
{
	if(!m_pSoundMoodManager)
		return;

	uint uiSoundMood = 0;
	for(TVectorSoundMoods::iterator iter=m_vecSoundMoods.begin(); iter!=m_vecSoundMoods.end(); ++iter,++uiSoundMood)
	{
		SSoundMood *pSoundMood = &(*iter);

		char szTemp[256];
		sprintf(szTemp,"strSoundMood_%d",	uiSoundMood);	ser.Value(szTemp,pSoundMood->strSoundMood);
		sprintf(szTemp,"uiFadeOutTime_%d",uiSoundMood);	ser.Value(szTemp,pSoundMood->uiFadeOutTime);
		sprintf(szTemp,"uiFadeOut_%d",		uiSoundMood);	ser.Value(szTemp,pSoundMood->uiFadeOut);
		sprintf(szTemp,"bValid_%d",				uiSoundMood);	ser.Value(szTemp,pSoundMood->bValid);
		sprintf(szTemp,"bUnlimited_%d",		uiSoundMood);	ser.Value(szTemp,pSoundMood->bUnlimited);
	}
}

//-----------------------------------------------------------------------------------------------------

void CSoundMoods::Update()
{
	if(!m_pSoundMoodManager)
		return;

	float fTime = gEnv->pTimer->GetCurrTime();

	for(TVectorSoundMoods::iterator iter=m_vecSoundMoods.begin(); iter!=m_vecSoundMoods.end(); ++iter)
	{
		SSoundMood *pSoundMood = &(*iter);
		if(pSoundMood->bValid && fTime >= pSoundMood->uiFadeOutTime && !pSoundMood->bUnlimited)
		{
			// Sound mood will be automatically unregistered after fade out
			m_pSoundMoodManager->UpdateSoundMood(pSoundMood->strSoundMood,0.0f,pSoundMood->uiFadeOut);
			pSoundMood->bValid = false;
		}
	}
}

//-----------------------------------------------------------------------------------------------------