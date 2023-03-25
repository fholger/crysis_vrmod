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


#ifndef __CTWEAKMETADATA_H__
#define __CTWEAKMETADATA_H__

#pragma once

//-------------------------------------------------------------------------

#include "TweakCommon.h"

//-------------------------------------------------------------------------
// Forward declarations

struct IScriptTable;

//-------------------------------------------------------------------------

class CTweakMetadata : public CTweakCommon {
public:
	
	CTweakMetadata(IScriptTable *pTable);
	
	~CTweakMetadata() {};

	virtual string GetValue(void) = 0;

	virtual bool DecreaseValue(void) = 0;

	virtual bool IncreaseValue(void) = 0;

	ETweakType GetType(void) { return eTT_Metadata; }

	static CTweakCommon * GetNewMetadata(IScriptTable *pTable);

	void Init(void) {};

protected:

	// Was the metadata constructed successfully?
	bool m_bValid;

	// The variable this Tweak would tweak
	string m_sVariable; // The name of the CVAR/LUA variable

	// The delta hint amount
	double m_fDelta;
};

#endif // __CTWEAKMETADATA_H__