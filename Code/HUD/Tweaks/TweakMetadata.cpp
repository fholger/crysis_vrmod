/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2006.
-------------------------------------------------------------------------

Description: 
	Abstract class unifying metadata classes

-------------------------------------------------------------------------
History:
- 28:02:2006  : Created by Matthew Jack

*************************************************************************/

#include "StdAfx.h"
#include "TweakMetadata.h"
#include "TweakMetadataCVAR.h"
#include "TweakMetadataLUA.h"
#include "IScriptSystem.h"

//-------------------------------------------------------------------------


CTweakMetadata::CTweakMetadata(IScriptTable *pTable) {
	// Default delta value
	m_fDelta = 1.0f;

	// Get the common elements of Tweaks
	m_sName = FetchStringValue(pTable, "NAME");

	// Fetch any delta hint
	string sIncHint = FetchStringValue(pTable, "DELTA");
	if (!sIncHint.empty()) {
		double fReadDelta = atof(sIncHint.c_str());
		// Check the read value
		if (fReadDelta == 0.0) {
			m_sError = "Invalid delta value";
		} else {
			m_fDelta = fReadDelta;
		}
	}	
}

//-------------------------------------------------------------------------

// Identify the type metadata a table represents, return an instance
CTweakCommon *CTweakMetadata::GetNewMetadata( IScriptTable *pTable )  {

	bool bLua = pTable->HaveValue("LUA");
	bool bCVar = pTable->HaveValue("CVAR");

	if (! (bLua ^ bCVar) ) {
		// One, and only one, should be present
		return new CTweakCommon("*Broken metadata*");
	}

	if (bLua) return new CTweakMetadataLUA(pTable);
	else return new CTweakMetadataCVAR(pTable);
}

//-------------------------------------------------------------------------