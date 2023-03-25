/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------

Description: Utilities for working with the Script system

-------------------------------------------------------------------------
History:
- 29:03:2006   15:00 : Created by Matthew Jack

Notes:
	Most of this is pretty inefficient at the moment - apply with care

*************************************************************************/


#include "StdAfx.h"
#include "ScriptUtils.h"
#include "ScriptHelpers.h"

//-------------------------------------------------------------------------

bool IsEqual(ScriptAnyValue &a, ScriptAnyValue &b) {
	// Could have a more efficient pre-test
	// Could also compare references for strings....
	string s1 = ToString(a);
	string s2 = ToString(b);
	return (s1==s2);
}

//-------------------------------------------------------------------------

string ToString(ScriptAnyValue &value) {
	string result;
	ScriptAnyType type = value.type;
	switch (type)
	{
	case ANY_ANY:
		result = string("");
		break;
	case ANY_TBOOLEAN:
		result = string(value.b ? "True" : "False");
		break;
	case ANY_TFUNCTION:
		result.Format("Function: %x", value.function);
		break;
	case ANY_THANDLE:
		result.Format("Handle: %x", value.ptr);
		break;
	case ANY_TNIL:
		result = string("Nil");
		break;
	case ANY_TNUMBER:
		result.Format("%f",value.number );
		break;
	case ANY_TSTRING:
		result = string(value.str);
		break;
	case ANY_TTABLE:
		result.Format("Table: %x", value.table);
		break;
	case ANY_TUSERDATA:
		result.Format("Table: %x", value.ud.ptr);
		break;
	case ANY_TVECTOR:
		result.Format("(%f, %f, %f)");
		break;
	default: 
		return "Unrecognised type";
	}

	return result;
}

//-------------------------------------------------------------------------

bool GetLuaVarRecursive(const char *sKey, ScriptAnyValue &result) {
	// Copy the string
	string tokenStream(sKey);
	string token;
	int curPos = 0;

	ScriptAnyValue value;

	// Deal with first token specially
	token = tokenStream.Tokenize(".", curPos);
	if (token.empty()) return false; // Catching, say, an empty string
	if (! gEnv->pScriptSystem->GetGlobalAny(token,value) ) return false;

	// Tokenize remainder
	token = tokenStream.Tokenize(".", curPos);
	while (!token.empty()) {
		// Make sure the last step was a table
		if (value.type != ANY_TTABLE) return false;

		// Must use temporary 
		ScriptAnyValue getter;
		value.table->GetValueAny(token, getter);
		value = getter;	
		token = tokenStream.Tokenize(".", curPos);
	}

	result = value;
	return true;
}

//-------------------------------------------------------------------------

// This could perhaps be done better by preparsing into STL string array, but it's ok as is
bool SetLuaVarRecursive(const char *sKey, const ScriptAnyValue &newValue) {

	string tokenStream(sKey);

	// It might be a global - i.e. only one token
	if (tokenStream.find(".") == string::npos) {
		// Set as a global only
		gEnv->pScriptSystem->SetGlobalAny(sKey, newValue);
		return true;
	}

	// Init for tokenizing
	string token;
	int curPos = 0;
	ScriptAnyValue value;

	// Deal with first token specially
	token = tokenStream.Tokenize(".", curPos);
	gEnv->pScriptSystem->GetGlobalAny(token,value);

	token = tokenStream.Tokenize(".", curPos);
	if (token.empty()) return false; // Must be malformed path, ending with "."

	// Tokenize remainder, always looking ahead for end
	string nextToken = tokenStream.Tokenize(".", curPos);
	while (!nextToken.empty()) {
		// Make sure the last step was a table
		if (value.type != ANY_TTABLE) return false;

		// Must use temporary
		ScriptAnyValue getter;
		value.table->GetValueAny(token, getter);
		value = getter;	

		// Advance to the token ahead
		token = nextToken;
		nextToken = tokenStream.Tokenize(".", curPos);
	}

	// We should be left on the final token
	value.table->SetValueAny(token, newValue);

	// Delete copy and return
	return true;
}

//-------------------------------------------------------------------------

// Dump a Lua table as a string
bool DumpLuaTable( IScriptTable * table, FILE * file, string &result ) {
	
	IScriptSystem *pScript = gEnv->pScriptSystem;
	char *str = NULL; 		
	HSCRIPTFUNCTION f = pScript->GetFunctionPtr("DumpTableAsLuaString");
	//ScriptAnyValue val;
	//gEnv->pScriptSystem->GetGlobalAny("DumpTableAsLuaString",val);
	//SmartScriptFunction fun( pScript, pScript->GetFunctionPtr("DumpTableAsLuaString") );
	bool success = Script::CallReturn( pScript, f, table, "Tweaks.TweaksSave", str );
	result = str;
	return success;
}
