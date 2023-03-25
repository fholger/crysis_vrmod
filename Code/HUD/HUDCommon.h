/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 
	Header for CHUDCommon base class
	Shared by G02 and G04


-------------------------------------------------------------------------
History:
- 22:02:2006: Created by Matthew Jack from original HUD class

*************************************************************************/
#ifndef __HUDCOMMON_H__
#define __HUDCOMMON_H__

//-----------------------------------------------------------------------------------------------------

#include <list>
#include "IGameObject.h"
#include "IFlashPlayer.h"
#include "GameFlashAnimation.h"
#include "IHardwareMouse.h"
#include "IMaterialEffects.h"

//-----------------------------------------------------------------------------------------------------

// Forward declarations
class CWeapon;

//-----------------------------------------------------------------------------------------------------

class CHUDCommon : public IFSCommandHandler, public IHardwareMouseEventListener
{
	friend class CHUDVehicleInterface;

public:

	// IFSCommandHandler
	virtual void HandleFSCommand(const char *strCommand,const char *strArgs) = 0;
	// ~IFSCommandHandler

	// IHardwareMouseEventListener
	virtual void OnHardwareMouseEvent(int iX,int iY,EHARDWAREMOUSEEVENT eHardwareMouseEvent) = 0;
	// ~IHardwareMouseEventListener

	// IConsole callback
	static void HUD					(ICVar *pVar);
	static void ShowGODMode	(IConsoleCmdArgs *pConsoleCmdArgs);
	// ~IConsole callback

						CHUDCommon();
	virtual	~	CHUDCommon();


	virtual void UpdateRatio();
	virtual void Serialize(TSerialize ser);

	// Set the GOD mode
	void SetGODMode(uint8 ucGodMode, bool forceUpdate = false);

	// Show or hide the Flash HUD
	void Show(bool bShow);

	// Cursor handling
	void CursorIncrementCounter();
	void CursorDecrementCounter();

	bool IsVisible() { return m_bShow; }

	// Positioning and scaling animations
	void	RepositionFlashAnimation(CGameFlashAnimation *pAnimation) const;

	// Start Interference Effect
	void StartInterference(float distortion, float displacement, float alpha, float decay);

	// Add a flash animation to the update/render processing (the anims will do 
	// this automatically when they are loaded, except if manual update is specified)
	void Register(CGameFlashAnimation* pAnim);

	// Remove a flash animation from the update/render processing (the anims will call
	// this when Unload() is called
	void Remove(CGameFlashAnimation* pAnim);

protected:
	//DEBUG : used for balancing (player deaths in god mode)
	int						m_iDeaths;

	// Current GOD mode
	bool					m_bShowGODMode;
	uint8					m_godMode;
	TMFXEffectId	m_deathFxId;
	float					m_fLastGodModeUpdate;
	char					m_strGODMode[32];

	// Cursor state
	int m_iCursorVisibilityCounter;

	// Cached sizes
	int						m_width;
	int						m_height;

	// Interference
	bool					m_bForceInterferenceUpdate;
	float					m_distortionStrength;
	float					m_displacementStrength;
	float					m_alphaStrength;
	float					m_interferenceDecay;

	int						m_distortionX;
	int						m_distortionY;
	int						m_displacementX;
	int						m_displacementY;
	int						m_alpha;

	// Flash HUD visibility
	bool m_bShow;

	void CreateInterference();

	virtual void UpdateCrosshairVisibility() = 0;

	// List of Flash animations loaded
	typedef std::list<CGameFlashAnimation *> TGameFlashAnimationsList;
	TGameFlashAnimationsList m_gameFlashAnimationsList;
};

//-----------------------------------------------------------------------------------------------------

#endif

//-----------------------------------------------------------------------------------------------------
