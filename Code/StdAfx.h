/*************************************************************************
	Crytek Source File.
	Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description:	include file for standard system include files,	or project
								specific include files that are used frequently, but are
								changed infrequently

 -------------------------------------------------------------------------
	History:
	- 20:7:2004   10:51 : Created by Marco Koegler
	- 3:8:2004		15:00 : Taken-over by Márcio Martins

*************************************************************************/
#if !defined(AFX_STDAFX_H__B36C365D_F0EA_4545_B3BC_1E0EAB3B5E43__INCLUDED_)
#define AFX_STDAFX_H__B36C365D_F0EA_4545_B3BC_1E0EAB3B5E43__INCLUDED_


//#define _CRTDBG_MAP_ALLOC

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <CryModuleDefs.h>
#define eCryModule eCryM_Game
#define RWI_NAME_TAG "RayWorldIntersection(Game)"
#define PWI_NAME_TAG "PrimitiveWorldIntersection(Game)"

// Insert your headers here
#include <platform.h>
#include <algorithm>
#include <vector>
#include <memory>

#include <smartptr.h>

#include <Cry_Math.h>
#include <ISystem.h>
#include <I3DEngine.h>
#include <IInput.h>
#include <IConsole.h>
#include <ITimer.h>
#include <ILog.h>
#include <IGameplayRecorder.h>

#ifndef GAMEDLL_EXPORTS
#define GAMEDLL_EXPORTS
#endif

#ifdef GAMEDLL_EXPORTS
#define GAME_API DLL_EXPORT
#else
#define GAME_API
#endif

//////////////////////////////////////////////////////////////////////////
//! Reports a Game Warning to validator with WARNING severity.
inline void GameWarning( const char *format,... ) PRINTF_PARAMS(1, 2);
inline void GameWarning( const char *format,... )
{
	if (!format)
		return;
	char buffer[MAX_WARNING_LENGTH];
	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);
	CryWarning( VALIDATOR_MODULE_GAME,VALIDATOR_WARNING,buffer );
}

extern struct SCVars *g_pGameCVars;

#if defined(WIN32) && !defined(XENON)
//#define USE_G15_LCD
extern void* g_hInst;
#endif

//---------------------------------------------------------------------
inline float LinePointDistanceSqr(const Line& line, const Vec3& point, float zScale = 1.0f)
{
	Vec3 x0=point;
	Vec3 x1=line.pointonline;
	Vec3 x2=line.pointonline+line.direction;

	x0.z*=zScale;
	x1.z*=zScale;
	x2.z*=zScale;

	return ((x2-x1).Cross(x1-x0)).GetLengthSquared()/(x2-x1).GetLengthSquared();
}


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__B36C365D_F0EA_4545_B3BC_1E0EAB3B5E43__INCLUDED_)
