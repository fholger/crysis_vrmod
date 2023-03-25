/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2006.
-------------------------------------------------------------------------

Description: 
	Base class for Tweak menu components

-------------------------------------------------------------------------
History:
- 28:02:2006  : Created by Matthew Jack

Notes:	m_bIsInit is a bad idea - only need one in HUDTweak

*************************************************************************/

#include "StdAfx.h"
#include "TweakCommon.h"
#include "TweakMenu.h"
#include "TweakMetadata.h"
#include "IScriptSystem.h"

//-------------------------------------------------------------------------

string CTweakCommon::FetchStringValue(IScriptTable *pTable, const char *sKey) 
{
	string result;
	const char * sString;
	if (pTable->GetValue(sKey,sString)) {
		result = sString;
	}
	return result;
}

//-------------------------------------------------------------------------

CTweakCommon * CTweakCommon::GetNewTweak( IScriptTable *pTable ) {
	// Identify what kind of Tweak this is
	CTweakCommon *p_tweak = NULL;

	// A menu?
	if (pTable->HaveValue("MENU")) 
		p_tweak = new CTweakMenu(pTable);

	// A Tweak metadata item?
	else if (pTable->HaveValue("NAME"))
		p_tweak = CTweakMetadata::GetNewMetadata(pTable);

	if (p_tweak) return p_tweak;

	// Otherwise an unrecognised table
	return new CTweakCommon("*Unrecognised table*");
}
