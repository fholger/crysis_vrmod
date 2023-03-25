/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: C++ Item Implementation

-------------------------------------------------------------------------
History:
- 11:9:2004   15:00 : Created by Márcio Martins

*************************************************************************/
#ifndef __AUTOMATIC_H__
#define __AUTOMATIC_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Single.h"


class CAutomatic : public CSingle
{
protected:
	typedef struct SAutomaticActions
	{
		SAutomaticActions() { Reset(); };
		void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
		{
			CItemParamReader reader(params);
			ResetValue(automatic_fire,"automatic_fire");
		}

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(automatic_fire);
		}

		ItemString automatic_fire;

	} SAutomaticActions;

public:
	CAutomatic();
	virtual ~CAutomatic();

	// CSingle

	virtual void ResetParams(const struct IItemParamsNode *params);
	virtual void PatchParams(const struct IItemParamsNode *patch);

	virtual void GetMemoryStatistics(ICrySizer * s);

	virtual void Update(float frameTime, uint frameId);
	virtual void StartFire();
	virtual void StopFire();
	virtual const char *GetType() const;
	// no new members... don't override GetMemoryStatistics

	// ~CSingle

protected:

	SAutomaticActions m_automaticactions;
	uint							m_soundId;
};

#endif //__AUTOMATIC_H__