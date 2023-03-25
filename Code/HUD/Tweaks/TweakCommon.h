/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2006.
-------------------------------------------------------------------------

Description: 
	Header for CTweakCommon, a base class for Tweak menu components

-------------------------------------------------------------------------
History:
- 28:02:2006  : Created by Matthew Jack

*************************************************************************/


#ifndef __CTWEAKCOMMON_H__
#define __CTWEAKCOMMON_H__

#pragma once

//-------------------------------------------------------------------------
// Forward declarations

struct IScriptTable;

//-------------------------------------------------------------------------


class CTweakCommon {
public:

	enum ETweakType {
		eTT_Metadata,		// A metadata item
		eTT_Menu,				// A menu
		eTT_Broken			// Any item that failed in construction
	};

	// Common tweak constructor
	CTweakCommon() : m_bIsInit(false) {}

	// Common tweak destructor
	~CTweakCommon() {};

	// Tweak constructor used for components that could not be constructed
	CTweakCommon(string sErrorMessage) : m_bIsInit(false) { m_sName = sErrorMessage; }

	// Get the type of this tweak
	virtual ETweakType GetType() { return eTT_Broken; } 

	// Get the name of this tweak component
	const string &GetName(void) const { return m_sName; }

	// Identify the type of tweak a table represents, return an instance
	static CTweakCommon *GetNewTweak( IScriptTable *pTable );

	// Cause a recursive save of changed values - for now, only LUA values
	virtual void StoreChanges( IScriptTable *pTable ) {} ;

	virtual void Init(void) {}; 

	virtual bool IsInitialized(void) { return m_bIsInit; }

protected:

	// Wraps fetching a Lua table entry
	string FetchStringValue(IScriptTable *pTable, const char *sKey);

	// The name of this tweak component
	string m_sName;

	// If there are non-fatal errors, they can be reported here
	string m_sError;

	// Simple check for initialization
	bool m_bIsInit;
};

#endif // __CTWEAKCOMMON_H__