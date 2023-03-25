/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 28:10:2005   16:00 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Scope.h"
#include "Player.h"

#include "Game.h"
#include "HUD/HUD.h"
#include "HUD/HUDScopes.h"

//---------------------------------------------
CScope::CScope():
m_showTimer(-1.0f),
m_hideTimer(-1.0f)
{

}

//------------------------------------------------------------------------
void CScope::Update(float frameTime, uint frameId)
{
	CIronSight::Update(frameTime, frameId);

	if (m_showTimer>0.0f)
	{
		m_showTimer-=frameTime;
		if (m_showTimer<=0.0f)
		{
			m_showTimer=-1.0f;
			//m_pWeapon->Hide(false);
			m_pWeapon->SendMusicLogicEvent(eMUSICLOGICEVENT_SNIPERMODE_LEAVE);
			m_pWeapon->OnZoomOut();
		}

		m_pWeapon->RequireUpdate(eIUS_Zooming);
	}

	if (m_hideTimer>0.0f)
	{
		m_hideTimer-=frameTime;
		if (m_hideTimer<=0.0f)
		{
			if(m_pWeapon->GetOwnerActor() && m_pWeapon->GetOwnerActor()->IsClient())
			{
				m_hideTimer=-1.0f;
				//m_pWeapon->Hide(true);
				m_pWeapon->OnZoomIn();
				int iZoom = 0;
				if(!strcmp(m_scopeparams.scope.c_str(),"scope_assault"))
					iZoom = 1;
				if(!strcmp(m_scopeparams.scope.c_str(),"scope_sniper"))
					iZoom = 2;

				if(iZoom != 0)
				{
					SAFE_HUD_FUNC(GetScopes()->ShowScope(iZoom));
					m_pWeapon->SendMusicLogicEvent(eMUSICLOGICEVENT_SNIPERMODE_ENTER);
				}
			}

		}

		m_pWeapon->RequireUpdate(eIUS_Zooming);
	}
}

//------------------------------------------------------------------------
void CScope::ResetParams(const struct IItemParamsNode *params)
{
	CIronSight::ResetParams(params);

	const IItemParamsNode *scope = params?params->GetChild("scope"):0;
	m_scopeparams.Reset(scope);
}

//------------------------------------------------------------------------
void CScope::PatchParams(const struct IItemParamsNode *patch)
{
	CIronSight::PatchParams(patch);

	const IItemParamsNode *scope = patch->GetChild("scope");
	m_scopeparams.Reset(scope, false);
}

//------------------------------------------------------------------------
void CScope::Activate(bool activate)
{
	if (!activate)
	{
		if (m_zoomed || m_zoomTimer>0.0f)
		{
			if(	!strcmp(m_scopeparams.scope.c_str(),"scope_default") ||
					!strcmp(m_scopeparams.scope.c_str(),"scope_assault") ||
					!strcmp(m_scopeparams.scope.c_str(),"scope_sniper"))
			{
				SAFE_HUD_FUNC(GetScopes()->ShowScope(0));
			}
		}
	}

	CIronSight::Activate(activate);
}

//------------------------------------------------------------------------
void CScope::OnEnterZoom()
{
	CIronSight::OnEnterZoom();
	m_hideTimer = 0.15f;
	m_showTimer = -1.0f;
}

//------------------------------------------------------------------------
void CScope::OnLeaveZoom()
{
	CIronSight::OnLeaveZoom();
	m_showTimer = 0.025f;
	m_hideTimer = -1.0f;

	if(	!strcmp(m_scopeparams.scope.c_str(),"scope_default") ||
			!strcmp(m_scopeparams.scope.c_str(),"scope_assault") ||	
			!strcmp(m_scopeparams.scope.c_str(),"scope_sniper"))
	{
		SAFE_HUD_FUNC(GetScopes()->ShowScope(0));
	}
}

//-------------------------------------------------------------
void CScope::OnZoomStep(bool zoomingIn, float t)
{
	CIronSight::OnZoomStep(zoomingIn, t);

	// only call the HUD when we have finished(!) zooming (t == 1.0f)
	if (t > 0.999f)
	{
		if(	!strcmp(m_scopeparams.scope.c_str(),"scope_default") ||
			!strcmp(m_scopeparams.scope.c_str(),"scope_assault") ||
			!strcmp(m_scopeparams.scope.c_str(),"scope_sniper"))
		{
			SAFE_HUD_FUNC(GetScopes()->SetScopeZoomMode(m_currentStep, m_scopeparams.scope));
		}
		else if(!strcmp(m_scopeparams.scope.c_str(),"scope_binoculars"))
		{
			SAFE_HUD_FUNC(GetScopes()->SetBinocularsZoomMode(m_currentStep));
		}
	}
}

//-------------------------------------------------------------
void CScope::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	m_scopeparams.GetMemoryStatistics(s);
	CIronSight::GetMemoryStatistics(s);
}
