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

#ifndef __THROWABLEWEAPON_H__
#define __THROWABLEWEAPON_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Weapon.h"

class CThrowableWeapon : public CWeapon
{
public:
	CThrowableWeapon() {};
	virtual ~CThrowableWeapon() {};

	virtual bool CanSelect() const;
};


#endif // __THROWABLEWEAPON_H__