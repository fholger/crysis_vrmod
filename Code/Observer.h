/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Implements the Observer alien.
  
 -------------------------------------------------------------------------
  History:
  - 9:6:2005: Created by Filippo De Luca
  - 15:8:2005: Renamed CDrone to CObserver by Mikko Mononen

*************************************************************************/
#ifndef __OBSERVER_H__
#define __OBSERVER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Hunter.h"

class CObserver :
	public CHunter
{
public:

	virtual void	SetActorStance(SMovementRequestParams &control, int& actions)
	{
		// Empty
	}

protected:
};


#endif //__OBSERVER_H__
