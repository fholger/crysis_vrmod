/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  Class for weapons like mines, which can be thrown
-------------------------------------------------------------------------
History:
- 24:1:2007   12:39 : Created by Steve Humphreys

*************************************************************************/

#include "StdAfx.h"
#include "ThrowableWeapon.h"

//------------------------------------------------------------------------
bool CThrowableWeapon::CanSelect() const
{
	return CWeapon::CanSelect() && !OutOfAmmo(false);
}