/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2006.
-------------------------------------------------------------------------

Description: 
	A HUD object that displays a menu for Tweaking gameplay variables

-------------------------------------------------------------------------
History:
- 28:02:2006  : Created by Matthew Jack

*************************************************************************/


#include "StdAfx.h"
#include "HUDTweakMenu.h"
//#include "..\HUDDraw.h"
#include "IUIDraw.h"
#include "IScriptSystem.h"
#include "ScriptUtils.h"
#include "ICryPak.h"
#include "IGame.h"
#include "IGameFramework.h"
//-----------------------------------------------------------------------------------------------------

CHUDTweakMenu::CHUDTweakMenu(IScriptSystem *pScriptSystem) //, m_menu(pScriptSystem)
{
	m_fX = 100.0f;
	m_fY = 100.0f;

	m_fWidth = 200;
	m_fHeight = 200;
	
	m_bActive = false;

	m_menu = new CTweakMenu(pScriptSystem);
	m_traverser = m_menu->GetTraverser();
	m_traverser.First();
  
	m_nOriginalStateAICVAR = 0; // Redundant value
  
	m_nBlackTexID = gEnv->pRenderer->EF_LoadTexture("Textures/defaults/black.dds",0,eTT_2D)->GetTextureID();

	m_pDefaultFont = GetISystem()->GetICryFont()->GetFont("default");
	CRY_ASSERT(m_pDefaultFont);
}

//-----------------------------------------------------------------------------------------------------

CHUDTweakMenu::~CHUDTweakMenu(void)
{
	// Check if we need to save on exit
	ScriptAnyValue bSave = false;
	GetLuaVarRecursive("Tweaks.SAVEONEXIT", bSave);

	if (bSave.b) 
		WriteChanges();

	SAFE_DELETE(m_menu);

	gEnv->pRenderer->RemoveTexture(m_nBlackTexID);
}

//-----------------------------------------------------------------------------------------------------

void CHUDTweakMenu::Init( IScriptSystem *pScriptSystem ) 
{
	// Initialise menu items, for instance noting their default state
	m_menu->Init();

	// Load any saved LUA tweaks
	LoadChanges();
}

//-----------------------------------------------------------------------------------------------------

void CHUDTweakMenu::Update(float fDeltaTime) 
{
	if (!m_bActive)
		return;

	IScriptSystem *pScript = gEnv->pScriptSystem;
	SmartScriptTable tweakTable;
	if (!pScript->GetGlobalValue("Tweaks", tweakTable)) 
		return;

	// Check it the menu needs reloading
	bool bReload = false;
	tweakTable->GetValue("RELOAD",bReload);
	if (bReload) {
		SAFE_DELETE (m_menu);
		m_menu = new CTweakMenu(pScript);
		m_traverser = m_menu->GetTraverser();
		m_traverser.First();
		bReload = false;
		tweakTable->SetValue("RELOAD",bReload);
	}

	// Check if we should save the LUA Tweak changes
	bool bSaveChanges = false;
	tweakTable->GetValue("SAVECHANGES",bSaveChanges);
	if (bSaveChanges) {
		bSaveChanges = false;
		tweakTable->SetValue("SAVECHANGES",bSaveChanges);
		WriteChanges();
	}

	// Check if it has been initialised
	if (! m_menu->IsInitialized()) {
		Init(pScript);
	}

	// Draw the menu
	if (m_bActive) DrawMenu();
}

//-----------------------------------------------------------------------------------------------------

void CHUDTweakMenu::DrawMenu() 
{

	ICryFont *pCryFont = GetISystem()->GetICryFont();
	if (!pCryFont)
		return;

	IFFont *m_pDefaultFont = pCryFont->GetFont("default");

	if (!m_pDefaultFont)
		return;
	
	vector2f oldSize = m_pDefaultFont->GetSize();
	m_pDefaultFont->SetSize( vector2f(24.0f, 24.0f) );

	ResetMenuPane();

	ScriptAnyValue displayBackground;
	displayBackground.b=true;
	GetLuaVarRecursive("Tweaks.BLACKBACKGROUND", displayBackground);
	
	if (displayBackground.b)
	{
		// draw a semi-transparent black box background to make
		// sure the menu text is visible on any background					
		gEnv->pRenderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST);			
		//gEnv->pRenderer->SetViewport(0,0,gEnv->pRenderer->GetWidth(),gEnv->pRenderer->GetHeight());
		gEnv->pRenderer->Draw2dImage(50,50,800-100,600-100,m_nBlackTexID,0,0,1,1,0,1,1,1,0.5f);
	}

	// Set up renderer state
	gEnv->pRenderer->SetState(GS_BLSRC_SRCALPHA|GS_BLDST_ONE|GS_NODEPTHTEST);

	PrintToMenuPane("Tweak Menu", eTC_White );

	// Display the menu path
	PrintToMenuPane( GetMenuPath().c_str(), eTC_Blue );
	
	// Get a Traverser pointing at the top of this menu
	CTweakTraverser itemIter = m_traverser;
	itemIter.Top();

	while (itemIter.Next()) {
		CTweakCommon * item = itemIter.GetItem();
		string text = item->GetName();

		// Accessorize by type
		ETextColor colour = eTC_White;
		switch (item->GetType()) {
			case CTweakCommon::eTT_Menu:
				colour = eTC_Yellow; 
				break;
			case CTweakCommon::eTT_Metadata:
				colour = eTC_Green; 
				text += " " + ((CTweakMetadata*)(item))->GetValue();
				break;
		}
		 
		// Is this the currently selected item?
		if (itemIter == m_traverser) colour = eTC_Red;

		// Display it
		PrintToMenuPane( text.c_str(), colour );
	}

	m_pDefaultFont->SetSize( oldSize );

	// Set back renderer state
	gEnv->pRenderer->SetState(GS_BLSRC_SRCALPHA|GS_BLDST_ONEMINUSSRCALPHA|GS_NODEPTHTEST);
}

//-----------------------------------------------------------------------------------------------------

void CHUDTweakMenu::ResetMenuPane(void) 
{
	m_nLineCount = 0;
}

//-----------------------------------------------------------------------------------------------------

void CHUDTweakMenu::PrintToMenuPane( const char * line,  ETextColor color ) 
{
	IUIDraw *pUIDraw = gEnv->pGame->GetIGameFramework()->GetIUIDraw();

	float fVertSpace = 30;
	float fR = 0.0f, fG =  0.0f , fB = 0.0f;
	float fI = 0.8f;
	switch (color) {
		case eTC_Red:				
			fR = fI; break;		// Selection
		case eTC_Green:
			fG = fI; break;		// Tweaks
		case eTC_Blue:
			fB = fI; break;		// Path
		case eTC_Yellow:
			fR = fG = fI; break;// Submenus
		case eTC_White:
			fR = fG = fB = fI; break; // Default
	}

	pUIDraw->DrawText(m_pDefaultFont, m_fX, m_fY + fVertSpace * m_nLineCount,0,0, line, 0.8f, fR, fG, fB, UIDRAWHORIZONTAL_LEFT,UIDRAWVERTICAL_TOP,UIDRAWHORIZONTAL_LEFT,UIDRAWVERTICAL_TOP);
	m_nLineCount++;
}


//-----------------------------------------------------------------------------------------------------

void CHUDTweakMenu::OnActionTweak(const char *actionName, int activationMode, float value) 
{	
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);
	// Save the state of the CVAR in case it is actually being used
	const char *sAICVAR = "ai_noupdate";

	if (0 == strcmp(actionName,"tweak_enable"))
	{
		if (!m_bActive)
		{
			m_bActive = true;
			if (ICVar *pAiUpdate = gEnv->pConsole->GetCVar( sAICVAR ) )
			{
				m_nOriginalStateAICVAR = pAiUpdate->GetIVal();
				pAiUpdate->Set(1);
			}
		}
		else
		{
			m_bActive = false;
			if (ICVar *pAiUpdate = gEnv->pConsole->GetCVar( sAICVAR ) )
			{
				pAiUpdate->Set(m_nOriginalStateAICVAR);
			}
		}
	}
	else if (0 == strcmp(actionName,"tweak_left"))
	{
		m_traverser.Back();
	}
	else if (0 == strcmp(actionName,"tweak_right"))
	{
		m_traverser.Forward();
	}
	else if (0 == strcmp(actionName,"tweak_up"))
	{
		if (!m_traverser.Previous())
			m_traverser.First();
	}
	else if (0 == strcmp(actionName,"tweak_down"))
	{
		if (!m_traverser.Next())
			m_traverser.Last();
	}
	else if (0 == strcmp(actionName,"tweak_inc"))
	{
		CTweakCommon *item = m_traverser.GetItem();
		if (item && item->GetType() == CTweakCommon::eTT_Metadata) 
			((CTweakMetadata*)(item))->IncreaseValue();	
	}
	else if (0 == strcmp(actionName,"tweak_dec"))
	{
		CTweakCommon *item = m_traverser.GetItem();
		if (item && item->GetType() == CTweakCommon::eTT_Metadata) 
			((CTweakMetadata*)(item))->DecreaseValue();
	}
}


//-----------------------------------------------------------------------------------------------------

string CHUDTweakMenu::GetMenuPath(void) const 
{
	// Check validity
	if (!m_traverser.IsRegistered()) return "No valid menu";

	// Create a copy of the traverser we can use
	CTweakTraverser backTracker = m_traverser;

	// Form string to display of menu position
	string sPathText;
	do {
		sPathText = backTracker.GetMenu()->GetName() + "->" + sPathText;
	}	while (backTracker.Back());
	return sPathText;
}

//-----------------------------------------------------------------------------------------------------

SmartScriptTable CHUDTweakMenu::FetchSaveTable( void ) {
	// Could do this much more neatly with GetLuaVarRecursive

	IScriptSystem * pScripts = gEnv->pScriptSystem;
	SmartScriptTable tweakTable;
	if (! pScripts->GetGlobalValue("Tweaks", tweakTable)) {
		gEnv->pLog->Log("Can't find Tweak table");
	}

	SmartScriptTable saveTable;
	if (!tweakTable->GetValue("TweaksSave", saveTable)) {
		gEnv->pLog->Log("Can't find Tweak save tables");
	}

	return saveTable;
} 


//-----------------------------------------------------------------------------------------------------


void CHUDTweakMenu::WriteChanges( void ) {
	
	SmartScriptTable saveTable = FetchSaveTable();
	if (! saveTable.GetPtr()) return;

	m_menu->StoreChanges( saveTable.GetPtr() );


	//FILE *file=gEnv->pCryPak->GetAlias("Game\\Scripts",true);		
	const char *szFolder=gEnv->pCryPak->GetAlias("Scripts",true);		
	char szScriptFolder[512];	
	_snprintf(szScriptFolder,512,"Game\\%s\\TweaksSave.lua",szFolder);
	FILE *file=fxopen(szScriptFolder,"wt");
	if (file) {
		string luaText;
		DumpLuaTable(saveTable.GetPtr(), file, luaText);
		fprintf(file, "%s", luaText.c_str());
		fclose(file);
	} else {
		gEnv->pLog->Log("No save - failed to open Tweak save file");
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDTweakMenu::LoadChanges( void ) {

	// We should probably reload the script first

	SmartScriptTable saveTable = FetchSaveTable();
	if (! saveTable.GetPtr()) return;

	// Go through and apply these LUA values
	// Identify and recurse on each element of the table
	IScriptTable::Iterator iter = saveTable->BeginIteration();
	while (saveTable->MoveNext(iter)) {

		SetLuaVarRecursive(iter.sKey, iter.value);
	}
	saveTable->EndIteration(iter);
}

void CHUDTweakMenu::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
}