/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Flash menu screen base class

-------------------------------------------------------------------------
History:
- 07:18:2006: Created by Julien Darre

*************************************************************************/
#ifndef __FLASHMENUSCREEN_H__
#define __FLASHMENUSCREEN_H__

#include "FlashAnimation.h"

//-----------------------------------------------------------------------------------------------------

class CFlashMenuScreen : public CFlashAnimation
{
public:
	void GetMemoryStatistics(ICrySizer * s);

	bool Load(const char *strFile);
	void UpdateRatio();
};

//-----------------------------------------------------------------------------------------------------

#endif

//-----------------------------------------------------------------------------------------------------