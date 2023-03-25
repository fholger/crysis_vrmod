/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2006.
-------------------------------------------------------------------------

Description: 
	Header for CHUDTweakMenu

-------------------------------------------------------------------------
History:
- 28:02:2006  : Created by Matthew Jack

*************************************************************************/

#ifndef __HUDTWEAKMENU_H__
#define __HUDTWEAKMENU_H__

#pragma once

#include "../HUDObject.h"
#include "TweakMenu.h"
#include "TweakTraverser.h"

//-----------------------------------------------------------------------------------------------------
// Forward declarations
//class CHUDDraw;

//-----------------------------------------------------------------------------------------------------



class CHUDTweakMenu :	public CHUDObject
{
public:
	CHUDTweakMenu(IScriptSystem *pScriptSystem);
	
	virtual ~CHUDTweakMenu(void);

	void Init( IScriptSystem *pScriptSystem );

	virtual void Update(float fDeltaTime);

	void OnActionTweak(const char *actionName, int activationMode, float value);

	// Save all changed state in the Tweak menu
	void WriteChanges( void );

	// Load any state that has previously been saved
	void LoadChanges( void );

	void GetMemoryStatistics(ICrySizer * s);

protected:
	// Colors to use 
	enum ETextColor {
		eTC_Red,
		eTC_Green,
		eTC_Blue,
		eTC_Yellow,
		eTC_White
	};

	bool m_bActive;

	// Draw the menu
	void DrawMenu(void);

	// Reset the menu pane
	void ResetMenuPane(void);

	// Print line to menu pane
	void PrintToMenuPane( const char * line,  ETextColor colour );

	// Return a string representing the current path through the menu
	string GetMenuPath(void) const;

	// Fetch the table of saved changes
	SmartScriptTable FetchSaveTable( void );

	// The root Tweak menu
	CTweakMenu *m_menu;

	// Vector of strings type
	typedef std::vector<string> TStringVec;

	// Traverser to keep place in the menu
	CTweakTraverser m_traverser;	

	// Dimensions of menu pane
	float m_fWidth, m_fHeight;

	// State of console
	int m_nLineCount;

	// Original state of the no_aiupdate CVAR
	int m_nOriginalStateAICVAR;

	// black texture
	uint	m_nBlackTexID;

	IFFont *m_pDefaultFont;
};


#endif  // __HUDTWEAKMENU_H__