/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2006.
-------------------------------------------------------------------------

Description: 
	Wraps a unit of Tweak metadata

-------------------------------------------------------------------------
History:
- 28:02:2006  : Created by Matthew Jack

Notes:
	In general, variables and functions should be found indirectly, through
	strings, as they may not be created when the menu is loaded. This hasn't
	yet been properly incorporated.
	Also, state table awaits implementation, for more powerful LUA incrementers.

*************************************************************************/

#include "StdAfx.h"
#include "TweakMetadataLUA.h"
#include "IScriptSystem.h"
#include "ScriptUtils.h"


//-------------------------------------------------------------------------

CTweakMetadataLUA::CTweakMetadataLUA(IScriptTable *pTable) : CTweakMetadata(pTable)
{
	// Already fetched by base classes: DELTA, NAME

	// Look for the essential elements of a Tweak
	m_bValid = true;

	// Fetch the variable name (we know this exists)
	m_sVariable = FetchStringValue(pTable, "LUA");

	// Look for an incrementer function
	m_incrementer = FetchFunctionValue(pTable, "INCREMENTER");

	// Look for an decrementer function
	m_decrementer = FetchFunctionValue(pTable, "DECREMENTER");


	// There's probably other checks that should be done, including looking for unrecognised elements

}

//-------------------------------------------------------------------------

string CTweakMetadataLUA::GetValue(void) {
	string result = "Not found";
	char buffer[256];

	// What type is the variable?
	ScriptAnyValue value = GetLuaValue();
	ScriptAnyType type = value.type;
	switch (type) {
		case ANY_TSTRING:
			result = value.str; 
			break;
		case ANY_TNUMBER:
			//result = itoa(value.number, buffer, 10);
			_snprintf(buffer, sizeof buffer, "%g", value.number);
			buffer[sizeof buffer - 1] = 0;
			result = buffer;
			break;
		case ANY_TBOOLEAN:
			result = (value.b ? "True" : "False");
			break;
		case ANY_TFUNCTION:
			result = "A function";
			break;
		default:
			result = "Type not handled";
	}

	return result;
}

//-------------------------------------------------------------------------


ScriptAnyValue CTweakMetadataLUA::GetLuaValue(void) const {
	IScriptSystem *scripts = gEnv->pScriptSystem;
	//ScriptAnyValue failed("Value unrecognised");
	ScriptAnyValue result;

	// Fetch as a variable
	if (GetLuaVarRecursive(m_sVariable.c_str(), result)) {
		
		// Is this actually a function? If so call it
		if (result.type == ANY_TFUNCTION) {
				scripts->BeginCall(result.function);
				scripts->EndCallAny(result);
		} 
	}
	return result;
}



//-------------------------------------------------------------------------

bool CTweakMetadataLUA::ChangeValue(bool bIncrement) const {
	IScriptSystem *scripts = gEnv->pScriptSystem;

	if (!bIncrement && m_decrementer) {
		// A decrement function has been provided 

		scripts->BeginCall(m_decrementer);
		//m_pSystem->GetIScriptSystem()->PushFuncParam(maxAlertness); // Give it state
		scripts->EndCall();

	} else if (bIncrement && m_incrementer) {
		// An increment function has been provided 

		scripts->BeginCall(m_incrementer);
		//m_pSystem->GetIScriptSystem()->PushFuncParam(maxAlertness); // Give it state
		scripts->EndCall();

	}	else {

		// Simple variable - get, (in|de)crement and set

		// Decide delta
		double fDelta = m_fDelta;
		if (!bIncrement) fDelta *= -1.0;

		// Change variable based on type
		ScriptAnyValue value = GetLuaValue();
		ScriptAnyType type = value.type;
		switch (type) {
			case ANY_TNUMBER:
				value.number += fDelta;
				break;
			case ANY_TBOOLEAN:
				value.b ^= true;
				break;
			default:
				// Type not handled
				return false;
		}

		// Set the variable
		SetLuaVarRecursive(m_sVariable.c_str(), value);
	}

	return true;
}

//-------------------------------------------------------------------------

HSCRIPTFUNCTION CTweakMetadataLUA::FetchFunctionValue(IScriptTable *pTable, const char *sKey) const {

	// Can't this be done using the templating?
	
	ScriptAnyValue fun;
	if (pTable->GetValueAny(sKey, fun)) {

		if (fun.type == ANY_TFUNCTION) {
			return fun.function;
		}
	} 
	
	return NULL;
}

//-------------------------------------------------------------------------

void CTweakMetadataLUA::StoreChanges( IScriptTable *pTable )  {
	ScriptAnyValue value = GetLuaValue();
	if (! IsEqual(m_originalValue, value) ) {
		pTable->SetValue( m_sVariable, value);
	}
}


//-------------------------------------------------------------------------

void CTweakMetadataLUA::Init(void) {
	m_originalValue = GetLuaValue();
}

//-------------------------------------------------------------------------


/*
// Now check what actually provides the value
if (result.type == ANY_TSTRING) {

// If this looks like a path to a function, try to convert 
if (strpbrk(result.str,".")) {

// Divide into path and possible contents of brackets
char * tokenStream = strdup(result.str);
char * token = strtok(tokenStream,"()");
ScriptAnyValue fun;

// Try to fetch the first part
if (GetLuaVarRecursive(token, fun) && fun.type == ANY_TFUNCTION) {

// Try to recognise the second part
token = strtok(NULL,"()");
if (token && strcmp(token,"self")) 

}
free(tokenStream);
}
}
*/
