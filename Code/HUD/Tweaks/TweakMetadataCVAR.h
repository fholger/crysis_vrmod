/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2006.
-------------------------------------------------------------------------

Description: 
	Header for CTweakMetadata

-------------------------------------------------------------------------
History:
- 28:02:2006  : Created by Matthew Jack

*************************************************************************/


#ifndef __CTWEAKMETADATACVAR_H__
#define __CTWEAKMETADATACVAR_H__

#pragma once

//-------------------------------------------------------------------------

#include "TweakMetadata.h"

//-------------------------------------------------------------------------
// Forward declarations

struct IScriptTable;

//-------------------------------------------------------------------------

class CTweakMetadataCVAR : public CTweakMetadata {
public:
	
	CTweakMetadataCVAR(IScriptTable *pTable);
	
	~CTweakMetadataCVAR() {};

	string GetValue(void);

	bool DecreaseValue(void) { return ChangeValue(false); }

	bool IncreaseValue(void) { return ChangeValue(true); }

	void UpdateChanges( IScriptTable *pTable )  { /* CVARs save themselves */ }

protected:

	// Increment/decrement the value
	bool ChangeValue(bool bIncrement);

	// Wraps fetching the CVAR
	ICVar * GetCVar(void) const;

	// Type of the CVAR
	int m_CVarType;
};

#endif // __CTWEAKMETADATACVAR_H__