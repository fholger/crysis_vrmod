/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Iron Sight

-------------------------------------------------------------------------
History:
- 28:10:2005   16:00 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCOPE_H__
#define __SCOPE_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "IronSight.h"


class CScope : public CIronSight
{
	typedef struct SScopeParams
	{
		SScopeParams() { Reset(); };
		void Reset(const IItemParamsNode *params=0, bool defaultInit=true)
		{
			CItemParamReader reader(params);

			ResetValue(scope, "scope_default");
			ResetValue(dark_out_time, 0.15f);
			ResetValue(dark_in_time, 0.15f);
		};

		void GetMemoryStatistics(ICrySizer * s)
		{
			s->Add(scope);
		}

		string	scope;
		float		dark_out_time;
		float		dark_in_time;
	} SScopeParams;

public:

	CScope();

	// IZoomMode
	virtual void Update(float frameTime, uint frameId);

	virtual void ResetParams(const struct IItemParamsNode *params);
	virtual void PatchParams(const struct IItemParamsNode *patch);

	virtual void Activate(bool activate);

	virtual void GetMemoryStatistics(ICrySizer * s);

	virtual void UpdateFPView(float frameTime) {};
	// ~IZoomMode


	// CIronSight
	virtual void OnEnterZoom();
	virtual void OnLeaveZoom();
	virtual void OnZoomStep(bool zoomingIn, float t);
	virtual bool IsScope() const { return true; }
	// ~CIronSight

protected:
	float					m_showTimer;
	float					m_hideTimer;
	SScopeParams	m_scopeparams;
};


#endif // __SCOPE_H__