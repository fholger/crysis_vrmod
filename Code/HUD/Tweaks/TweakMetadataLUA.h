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


#ifndef __CTWEAKMETADATALUA_H__
#define __CTWEAKMETADATALUA_H__

#pragma once

//-------------------------------------------------------------------------

#include "TweakMetadata.h"
#include "IScriptSystem.h"

//-------------------------------------------------------------------------

class CTweakMetadataLUA : public CTweakMetadata {
public:

	CTweakMetadataLUA(IScriptTable *pTable);

	~CTweakMetadataLUA() {}

	string GetValue(void);

	bool DecreaseValue(void) { return ChangeValue(false); }

	bool IncreaseValue(void) { return ChangeValue(true); }

	void StoreChanges( IScriptTable *pTable );

	void Init(void);

protected:

	// The value found in LUA before the Tweak system changes anything
	ScriptAnyValue m_originalValue;

	// The incrementer function, if it exists
	HSCRIPTFUNCTION m_incrementer;

	// The decrementer function, if it exists
	HSCRIPTFUNCTION m_decrementer;

	// Wraps fetching a Lua function value
	HSCRIPTFUNCTION FetchFunctionValue(IScriptTable *pTable, const char *sKey) const;

	// Wraps fetching a LUA value (possibly actually a function)
	ScriptAnyValue GetLuaValue(void) const;

	// Wraps setting a LUA variable
	void SetLVar(const ScriptAnyValue &value) const;

	// Wraps incrementing/decrementing a LUA variable (or calling the LUA functions)
	bool ChangeValue(bool increment) const;
};

#endif // __CTWEAKMETADATALUA_H__
