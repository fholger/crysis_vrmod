/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Binocular/Scopes HUD object (refactored from old HUD code)

-------------------------------------------------------------------------
History:
- 17:04:2007  17:30 : Created by Jan Müller

*************************************************************************/

#ifndef HUD_SCOPES_H
#define HUD_SCOPES_H

# pragma once


#include "HUDObject.h"
#include "IActorSystem.h"
#include "IFlashPlayer.h"
#include "GameFlashAnimation.h"

class CHUD;
class CPlayer;

class CHUDScopes
{
	friend class CHUD;
public:

	enum EScopeMode 
	{
		ESCOPE_NONE,
		ESCOPE_ASSAULT,
		ESCOPE_SNIPER,
		ESCOPE_LAST
	};

	CHUDScopes(CHUD *pHUD);
	~CHUDScopes();

	void Update(float fDeltaTime);
	void LoadFlashFiles(bool force=false);
	void ShowBinoculars(bool bVisible, bool bShowIfNoHUD=false, bool bNoFadeOutOnHide=false);
	void SetBinocularsDistance(float fDistance);
	void SetBinocularsZoomMode(int iZoomMode);
	void ShowScope(int iVisible);
	void SetScopeZoomMode(int iZoomMode, string &scopeType);
	void OnToggleThirdPerson(bool thirdPerson);
	ILINE bool IsBinocularsShown() const { return m_bShowBinoculars; }
	ILINE EScopeMode GetCurrentScope() const { return m_eShowScope; }
	void Serialize(TSerialize &ser);
	void DestroyBinocularsAtNextFrame() { m_bDestroyBinocularsAtNextFrame = true; }

private:

	void DisplayBinoculars(CPlayer* pPlayerActor);
	void DisplayScope(CPlayer* pPlayerActor);
	void SetSilhouette(IActor *pActor,IAIObject *pAIObject);

	//the main HUD
	CHUD			*g_pHUD;
	//the scope flash movies
	CGameFlashAnimation m_animBinoculars;
	CGameFlashAnimation m_animSniperScope;
	//binoculars visible
	bool m_bShowBinoculars;
	//zoom level
	int m_iZoomLevel;
	//scope visible
	EScopeMode m_eShowScope;
	// show binoculars without HUD being visible, e.g. cutscenes
	bool m_bShowBinocularsNoHUD; 
	// distance of binocs view
	float m_fBinocularDistance;
	// currently in third person ?
	bool m_bThirdPerson;
	bool m_bDestroyBinocularsAtNextFrame;
};

#endif
