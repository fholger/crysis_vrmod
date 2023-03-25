/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 
	Header for HUD object base class
	Defines base class for HUD elements that are drawn using C++ rather than Flash
	Shared by G02 and G04

-------------------------------------------------------------------------
History:
- 07:11:2005: Created by Julien Darre
- 22:02:2006: Refactored for G04 by Matthew Jack

*************************************************************************/
#include "StdAfx.h"
#include "HUDObject.h"
#include "IGame.h"
#include "IGameFramework.h"

//-----------------------------------------------------------------------------------------------------

CHUDObject::CHUDObject()
{
	m_fX = 0.0f;
	m_fY = 0.0f;
}

//-----------------------------------------------------------------------------------------------------

CHUDObject::~CHUDObject()
{
}

//-----------------------------------------------------------------------------------------------------

void CHUDObject::GetHUDObjectMemoryStatistics(ICrySizer * s)
{
}

//-----------------------------------------------------------------------------------------------------