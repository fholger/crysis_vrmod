/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------

Description: Utilities for working with the Script system

-------------------------------------------------------------------------
History:
-	29:03:2006  : Created by Matthew Jack

*************************************************************************/

#ifndef __SCRIPTUTILS_H__
#define __SCRIPTUTILS_H__

#pragma once

#include "IScriptSystem.h"

//-------------------------------------------------------------------------

// Test equality independantly of type, trying to mirror LUA equality
bool IsEqual(ScriptAnyValue &a, ScriptAnyValue &b);

template <class T> bool IsEqual(ScriptAnyValue &a, T &b) {
	ScriptAnyValue tmp(b);	
	return IsEqual(a,tmp);
}

template <class T> bool IsEqual(T &a, ScriptAnyValue &b){
	ScriptAnyValue tmp(a);
	return IsEqual(tmp,b);
}

// Convert to a string representation
string ToString(ScriptAnyValue &value);

// Badly named?
// Get a LUA value by a full path
bool GetLuaVarRecursive(const char *sKey, ScriptAnyValue &result);
bool GetLuaVarRecursive(const char *sKey, ScriptAnyValue &result,const ScriptAnyValue &initVal);

template <class T> bool GetLuaVarRecursive(const char *sKey, T &result) { 
	ScriptAnyValue value;
	bool success = GetLuaVarRecursive(sKey, value);
	value.CopyTo(result);
	return success;
}

template <class T> bool GetLuaVarRecursive(const char *sKey, T &result, const T &initVal) { 
	ScriptAnyValue value,val(initVal);
	bool success = GetLuaVarRecursive(sKey, value, val);
	value.CopyTo(result);
	return success;
}

// Set a LUA value by a full path
bool SetLuaVarRecursive(const char *sKey, const ScriptAnyValue &newValue);

template <class T> bool SetLuaVarRecursive(const char *sKey, const T &newValue) { 
	ScriptAnyValue value(newValue);
	return SetLuaVarRecursive(sKey, value);
}

inline bool GetLuaVarRecursive(const char *sKey, ScriptAnyValue &result,const ScriptAnyValue &initVal)
{
	bool bRet=true;
	if(!GetLuaVarRecursive(sKey, result))
	{
		if(SetLuaVarRecursive(sKey,initVal))
			;//result=initVal;
		else
			bRet=false;
		result=initVal;
	}
	return bRet;
}

// Dump a table out to a file stream as LUA code
bool DumpLuaTable( IScriptTable * table, FILE * file, string &str);


#endif __SCRIPTUTILS_H__