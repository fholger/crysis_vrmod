/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2006.
-------------------------------------------------------------------------

Description: 
	Wraps a unit of Tweak metadata

-------------------------------------------------------------------------
History:
- 28:02:2006  : Created by Matthew Jack

*************************************************************************/

#include "StdAfx.h"
#include "TweakMetadataCVAR.h"
#include "IScriptSystem.h"

//-------------------------------------------------------------------------

CTweakMetadataCVAR::CTweakMetadataCVAR(IScriptTable *pTable) : CTweakMetadata(pTable)
{
	// Already fetched by base classes: DELTA, NAME

	// Look for the essential elements of a Tweak
	m_bValid = true;

	// Fetch the variable name (we know this exists)
	m_sVariable = FetchStringValue(pTable, "CVAR");
		
	// There's probably other checks that should be done, including looking for unrecognised elements
	
	// What type is the CVAR?
	m_CVarType = 0;
	if (GetCVar()) m_CVarType  = GetCVar()->GetType();
}

//-------------------------------------------------------------------------

string CTweakMetadataCVAR::GetValue(void) {
	string result = "Not found";
	ICVar *cVar = GetCVar();
	if (cVar) result = cVar->GetString();
	return result;
}
		
//-------------------------------------------------------------------------

bool CTweakMetadataCVAR::ChangeValue(bool bIncrement) {
	// Get delta
	double fDelta = m_fDelta;
	if (!bIncrement) fDelta *= -1.0;

	// Get and check CVAR
	ICVar *cVar = GetCVar();
	if (!cVar) return false;
	
	// Deal with appropriate type
	switch (cVar->GetType()) {
		case CVAR_INT:
			cVar->Set( (int) (cVar->GetIVal() + fDelta)	);
			break;
		case CVAR_FLOAT:
			cVar->Set( (float) (cVar->GetFVal() + fDelta) );
			break;
		default:;
			// Strings are non-obvious
			// Might also be a non-exisitent variable			
	}
	return true;
}

//-------------------------------------------------------------------------

// Wraps fetching the console variable
ICVar * CTweakMetadataCVAR::GetCVar(void) const 
{
	return gEnv->pConsole->GetCVar(m_sVariable.c_str());
}

//-------------------------------------------------------------------------

