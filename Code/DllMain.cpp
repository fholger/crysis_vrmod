/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
Description: 	Dll Entry point to capture dll instance handle (needed for loading of embedded resources)

-------------------------------------------------------------------------
History:
- 01:11:2007: Created by Marco Koegler
*************************************************************************/
#include "StdAfx.h"
#if defined(WIN32) && !defined(XENON)
#include <windows.h>

void* g_hInst = 0;

BOOL APIENTRY DllMain ( HINSTANCE hInst, DWORD reason, LPVOID reserved )
{
	if ( reason == DLL_PROCESS_ATTACH )
		g_hInst = hInst;
	return TRUE;
}
#endif