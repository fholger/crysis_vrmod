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
#include "IronSight.h"
#include "Player.h"
#include "GameCVars.h"
#include "Single.h"
#include "BulletTime.h"

#define PHYS_FOREIGN_ID_DOF_QUERY PHYS_FOREIGN_ID_USER+3

//------------------------------------------------------------------------
CIronSight::CIronSight()
: m_pWeapon(0), 
	m_savedFoVScale(0.0f),
	m_zoomed(false),
	m_zoomingIn(false),
	m_zoomTimer(0.0f),
	m_zoomTime(0.0f),
	m_focus(1.0f),
	m_startFoV(0.0f),
	m_endFoV(0.0f),
	m_smooth(0.0f),
	m_enabled(true),
	m_currentStep(0),
	m_initialNearFov(60.0f),
	m_maxDoF(100.0f),
	m_minDoF(0.0f),
	m_averageDoF(50.0f),
	m_swayTime(0.0f),
	m_lastRecoil(0.0f)
{
}

//------------------------------------------------------------------------
CIronSight::~CIronSight()
{
}

//------------------------------------------------------------------------
void CIronSight::Init(IWeapon *pWeapon, const struct IItemParamsNode *params)
{
	m_pWeapon = static_cast<CWeapon *>(pWeapon);

	ResetParams(params);
}

//------------------------------------------------------------------------
void CIronSight::Update(float frameTime, uint frameId)
{
	bool keepUpdating=false;
	CActor* pActor = m_pWeapon->GetOwnerActor();
	bool isClient = (pActor && pActor->IsClient());

	float doft = 1.0f;
	if (!m_zoomed)
		doft = 0.0f;

	if (m_zoomTime > 0.0f)	// zoomTime is set to 0.0 when zooming ends
	{
		keepUpdating=true;
		float t = CLAMP(1.0f-m_zoomTimer/m_zoomTime, 0.0f, 1.0f);
		float fovScale;

		if (m_smooth)
		{
			if (m_startFoV > m_endFoV)
				doft = t;
			else
				doft = 1.0f-t;

			fovScale = m_startFoV+t*(m_endFoV-m_startFoV);
		}
		else
		{
			fovScale = m_startFoV;
			if (t>=1.0f)
				fovScale = m_endFoV;
		}

		OnZoomStep(m_startFoV>m_endFoV, t);

		SetActorFoVScale(fovScale, true, true, true);

		if(isClient && m_zoomparams.scope_mode && !UseAlternativeIronSight())
		{
			AdjustScopePosition(t*1.25f,m_startFoV>m_endFoV);
			AdjustNearFov(t*1.25f,m_startFoV>m_endFoV);
		}

		// marcok: please don't touch
		if (isClient && g_pGameCVars->goc_enable)
		{
			g_pGameCVars->goc_targety = LERP((-2.5f), (-1.5f), doft*doft);
		}

		if (t>=1.0f)
		{
			if (m_zoomingIn)
			{
				m_zoomed = true;
				m_pWeapon->RequestZoom(fovScale);
			}
			else
			{
				m_zoomed = false;
				m_pWeapon->RequestZoom(1.0f);
			}

			m_zoomTime = 0.0f;
		}
	}

	if (isClient && g_pGameCVars->g_dof_ironsight != 0 && g_pGameCVars->goc_enable==0)
		UpdateDepthOfField(pActor, frameTime, doft);

	bool wasZooming = m_zoomTimer>0.0f;
	if (wasZooming || m_zoomed)
	{
		m_zoomTimer -= frameTime;
		if (m_zoomTimer<0.0f)
		{
			m_zoomTimer=0.0f;
			if (wasZooming)
			{
				if (m_zoomingIn)
				{
					OnZoomedIn();
				}
				else
				{
					OnZoomedOut();
				}
			}
		}

		if (m_focus < 1.0f)
		{
			m_focus += frameTime*1.5f;
		}
		else if (m_focus > 1.0f)
		{
			m_focus = 1.0f;
		}

		if (isClient)
		{
			//float t=m_zoomTimer/m_zoomTime;
			if (m_zoomTime > 0.0f)
			{
				//t=1.0f-max(t, 0.0f);
				gEnv->p3DEngine->SetPostEffectParam("Dof_BlurAmount", 1.0f);
			}

			// try to convert user-defined settings to IronSight system (used for Core)
			float userActive;
			gEnv->p3DEngine->GetPostEffectParam("Dof_User_Active", userActive);
			if (userActive > 0.0f)
			{
				float focusRange;
				float focusDistance;
				gEnv->p3DEngine->GetPostEffectParam("Dof_User_FocusRange", focusRange);
				gEnv->p3DEngine->GetPostEffectParam("Dof_User_FocusDistance", focusDistance);
				gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMin", focusDistance);
				gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMax", focusDistance);
				gEnv->p3DEngine->SetPostEffectParam("Dof_FocusLimit", focusDistance+focusRange*0.5f);
			}
			else
			{
				gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMin", g_pGameCVars->g_dofset_minScale * m_minDoF);
				gEnv->p3DEngine->SetPostEffectParam("Dof_FocusMax", g_pGameCVars->g_dofset_maxScale * m_averageDoF);
				gEnv->p3DEngine->SetPostEffectParam("Dof_FocusLimit", g_pGameCVars->g_dofset_limitScale * m_averageDoF);
			}
		}

		keepUpdating=true;
	}

	if (keepUpdating)
		m_pWeapon->RequireUpdate(eIUS_Zooming);
}

//------------------------------------------------------------------------
void CIronSight::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CIronSight::ResetParams(const struct IItemParamsNode *params)
{
	const IItemParamsNode *zoom = params?params->GetChild("zoom"):0;
	const IItemParamsNode *actions = params?params->GetChild("actions"):0;
	const IItemParamsNode *spreadMod = params?params->GetChild("spreadMod"):0;
	const IItemParamsNode *recoilMod = params?params->GetChild("recoilMod"):0; 
	const IItemParamsNode *zoomSway = params?params->GetChild("zoomSway"):0;

	m_zoomparams.Reset(zoom);
	m_actions.Reset(actions);
	m_spreadModParams.Reset(spreadMod);
	m_recoilModParams.Reset(recoilMod);
	m_zoomsway.Reset(zoomSway);
}

//------------------------------------------------------------------------
void CIronSight::PatchParams(const struct IItemParamsNode *patch)
{
	const IItemParamsNode *zoom = patch->GetChild("zoom");
	const IItemParamsNode *actions = patch->GetChild("actions");
	const IItemParamsNode *spreadMod = patch->GetChild("spreadMod");
	const IItemParamsNode *recoilMod = patch->GetChild("recoilMod");
	const IItemParamsNode *zoomSway = patch->GetChild("zoomSway");

	m_zoomparams.Reset(zoom, false);
	m_actions.Reset(actions, false);
	m_spreadModParams.Reset(spreadMod, false);
	m_recoilModParams.Reset(recoilMod, false);
	m_zoomsway.Reset(zoomSway,false);
}

//------------------------------------------------------------------------
void CIronSight::Activate(bool activate)
{
	if (!activate && m_zoomed && m_zoomingIn && m_zoomparams.dof)
		ClearDoF();

	if (!activate && !m_zoomparams.suffix.empty())
		m_pWeapon->SetActionSuffix("");

	m_zoomed = false;
	m_zoomingIn = false;

	m_currentStep = 0;
	m_lastRecoil = 0.0f;

	SetRecoilScale(1.0f);
	SetActorFoVScale(1.0f, true, true, true);
	SetActorSpeedScale(1.0f);

	ResetTurnOff();
	if (!activate)
	{
		ClearBlur();
	}

	if(!activate && m_zoomparams.scope_mode)
	{
		ResetFovAndPosition();
	}
}

//------------------------------------------------------------------------
bool CIronSight::CanZoom() const
{
	return true;
}

//------------------------------------------------------------------------
bool CIronSight::StartZoom(bool stayZoomed, bool fullZoomout, int zoomStep)
{
	if (m_pWeapon->IsBusy() || (IsToggle() && IsZooming()))
		return false;
	CActor *pActor = m_pWeapon->GetOwnerActor();
	if (pActor && pActor->GetScreenEffects() != 0)
	{
		pActor->GetScreenEffects()->EnableBlends(false, pActor->m_autoZoomInID);
		pActor->GetScreenEffects()->EnableBlends(false, pActor->m_autoZoomOutID);
		pActor->GetScreenEffects()->EnableBlends(false, pActor->m_hitReactionID);

		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
	}

	if (!m_zoomed || stayZoomed)
	{
		EnterZoom(m_zoomparams.zoom_in_time, m_zoomparams.layer.c_str(), true, zoomStep);
		m_currentStep = zoomStep;

		m_pWeapon->AssistAiming(m_zoomparams.stages[m_currentStep-1], true);
	}
	else
	{
		int currentStep = m_currentStep;
		int nextStep = currentStep+1;

		if (nextStep > m_zoomparams.stages.size())
		{
			if (!stayZoomed)
			{
				if (fullZoomout)
				{
					StopZoom();
				}
				else
				{
					float oFoV = GetZoomFoVScale(currentStep);
					m_currentStep = 0;
					float tFoV = GetZoomFoVScale(m_currentStep);
					ZoomIn(m_zoomparams.stage_time, oFoV, tFoV, true);
					return true;
				}
			}
		}
		else
		{
			float oFoV = GetZoomFoVScale(currentStep);
			float tFoV = GetZoomFoVScale(nextStep);

			ZoomIn(m_zoomparams.stage_time, oFoV, tFoV, true);

			m_currentStep = nextStep;

			m_pWeapon->AssistAiming(m_zoomparams.stages[m_currentStep-1], true);
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------
void CIronSight::StopZoom()
{
	LeaveZoom(m_zoomparams.zoom_out_time, true);
	m_currentStep = 0;
}

//------------------------------------------------------------------------
void CIronSight::ExitZoom()
{
	if (m_zoomed || m_zoomTime>0.0f)
	{
		LeaveZoom(m_zoomparams.zoom_out_time, true);
		m_currentStep = 0;
	}
}

//------------------------------------------------------------------------
void CIronSight::ZoomIn()
{
	if (m_pWeapon->IsBusy())
		return;

	if (!m_zoomed)
	{
		EnterZoom(m_zoomparams.zoom_in_time, m_zoomparams.layer.c_str(), true);
		m_currentStep = 1;
	}
	else
	{
		int currentStep = m_currentStep;
		int nextStep = currentStep+1;

		if (nextStep > m_zoomparams.stages.size())
			return;
		else
		{
			float oFoV = GetZoomFoVScale(currentStep);
			float tFoV = GetZoomFoVScale(nextStep);

			ZoomIn(m_zoomparams.stage_time, oFoV, tFoV, true);

			m_currentStep = nextStep;
		}
	}
}

//------------------------------------------------------------------------
bool CIronSight::ZoomOut()
{
	if (m_pWeapon->IsBusy())
		return false;

	if (!m_zoomed)
	{
		EnterZoom(m_zoomparams.zoom_in_time, m_zoomparams.layer.c_str(), true);
		m_currentStep = 1;
	}
	else
	{
		int currentStep = m_currentStep;
		int nextStep = currentStep-1;

		if (nextStep < 1)
			return false;
		else
		{
			float oFoV = GetZoomFoVScale(currentStep);
			float tFoV = GetZoomFoVScale(nextStep);

			ZoomIn(m_zoomparams.stage_time, oFoV, tFoV, true);

			m_currentStep = nextStep;
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------
bool CIronSight::IsZoomed() const
{
	return m_zoomed;
}

//------------------------------------------------------------------------
bool CIronSight::IsZooming() const
{
	return m_zoomTimer>0.0f;
}

//------------------------------------------------------------------------
void CIronSight::Enable(bool enable)
{
	m_enabled = enable;
}

//------------------------------------------------------------------------
bool CIronSight::IsEnabled() const
{
	return m_enabled;
}

//------------------------------------------------------------------------
void CIronSight::EnterZoom(float time, const char *zoom_layer, bool smooth, int zoomStep)
{
	if (IsZooming() && !m_zoomingIn)
	{
		OnZoomedOut();
	}
	ResetTurnOff();
	OnEnterZoom();
	SetActorSpeedScale(0.35f);

	// marcok: please leave goc alone
	if(!UseAlternativeIronSight() && !g_pGameCVars->goc_tpcrosshair)
		m_pWeapon->FadeCrosshair(1.0f, 0.0f, WEAPON_FADECROSSHAIR_ZOOM);

	float oFoV = GetZoomFoVScale(0);
	float tFoV = GetZoomFoVScale(zoomStep);

	ZoomIn(time, oFoV, tFoV, smooth);

	if(UseAlternativeIronSight())
		m_pWeapon->SetActionSuffix(m_zoomparams.suffix_FC.c_str());
	else
		m_pWeapon->SetActionSuffix(m_zoomparams.suffix.c_str());
	m_pWeapon->PlayAction(m_actions.zoom_in, 0, false, CItem::eIPAF_Default);
}

void CIronSight::LeaveZoom(float time, bool smooth)
{
	if (IsZooming() && m_zoomingIn)
	{
		OnZoomedIn();
	}
	ResetTurnOff();
	OnLeaveZoom();
	SetActorSpeedScale(1.0f);

	// marcok: please leave goc alone
	if(!UseAlternativeIronSight() && !g_pGameCVars->goc_tpcrosshair)
		m_pWeapon->FadeCrosshair(0.0f, 1.0f, WEAPON_FADECROSSHAIR_ZOOM);
	else if(UseAlternativeIronSight())
		m_pWeapon->FadeCrosshair(1.0f, 1.0f, 0.1f);

	float oFoV = GetZoomFoVScale(0);
	float tFoV = GetZoomFoVScale(1);

	ZoomOut(time, tFoV, oFoV, smooth);

	bool playAnim = true;
	if(UseAlternativeIronSight() && m_currentStep==0)
		playAnim = false;

	if(playAnim)
	{
		m_pWeapon->StopLayer(m_zoomparams.layer, CItem::eIPAF_Default|CItem::eIPAF_NoBlend);
		m_pWeapon->PlayAction(m_actions.zoom_out, 0, false, CItem::eIPAF_Default);
	}
	
	m_pWeapon->SetActionSuffix("");
	m_pWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);
	m_currentStep = 0;
}

//------------------------------------------------------------------------
void CIronSight::ResetTurnOff()
{
	static ItemString idle = "idle";
	m_savedFoVScale = 0.0f;
	m_pWeapon->StopLayer(m_zoomparams.layer, CItem::eIPAF_Default|CItem::eIPAF_NoBlend);
	m_pWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, idle);
}

//------------------------------------------------------------------------
struct CIronSight::DisableTurnOffAction
{
	DisableTurnOffAction(CIronSight *_ironSight): ironSight(_ironSight) {};
	CIronSight *ironSight;

	void execute(CItem *pWeapon)
	{
		if(ironSight->UseAlternativeIronSight())
			pWeapon->SetActionSuffix(ironSight->m_zoomparams.suffix_FC.c_str());
		else
			pWeapon->SetActionSuffix(ironSight->m_zoomparams.suffix.c_str());
		
		ironSight->OnZoomedIn();
	}
};

struct CIronSight::EnableTurnOffAction
{
	EnableTurnOffAction(CIronSight *_ironSight): ironSight(_ironSight) {};
	CIronSight *ironSight;

	void execute(CItem *pWeapon)
	{
		ironSight->OnZoomedOut();
	}
};

void CIronSight::TurnOff(bool enable, bool smooth, bool anim)
{
	if (!enable && (m_savedFoVScale > 0.0f))
	{
		OnEnterZoom();
	
		float oFoV = GetZoomFoVScale(0);
		float tFoV = m_savedFoVScale;

		SetActorSpeedScale(0.35f);

		ZoomIn(m_zoomparams.zoom_out_time, oFoV, tFoV, smooth);

		if (anim)
		{
			if(UseAlternativeIronSight())
				m_pWeapon->SetActionSuffix(m_zoomparams.suffix_FC.c_str());
			else
				m_pWeapon->SetActionSuffix(m_zoomparams.suffix.c_str());
			m_pWeapon->PlayAction(m_actions.zoom_in);
		}

		m_pWeapon->GetScheduler()->TimerAction((uint)(m_zoomparams.zoom_out_time*1000), CSchedulerAction<DisableTurnOffAction>::Create(this), false);
		m_savedFoVScale = 0.0f;
	}
	else if (m_zoomed && enable)
	{
		m_pWeapon->SetBusy(true);
		m_savedFoVScale = GetActorFoVScale();

		OnLeaveZoom();

		float oFoV = GetZoomFoVScale(0);
		float tFoV = m_savedFoVScale;

		SetActorSpeedScale(1);
		ZoomOut(m_zoomparams.zoom_out_time, tFoV, oFoV, smooth);

		m_pWeapon->StopLayer(m_zoomparams.layer, CItem::eIPAF_Default|CItem::eIPAF_NoBlend);
		m_pWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, g_pItemStrings->idle);

		if (anim)
		{
			m_pWeapon->PlayAction(m_actions.zoom_out);
			m_pWeapon->SetActionSuffix("");
		}

		m_pWeapon->GetScheduler()->TimerAction((uint)(m_zoomparams.zoom_out_time*1000), CSchedulerAction<EnableTurnOffAction>::Create(this), false);
	}
}

//------------------------------------------------------------------------
void CIronSight::ZoomIn(float time, float from, float to, bool smooth)
{
	m_zoomTime = time;
	m_zoomTimer = m_zoomTime;
	m_startFoV = from;
	m_endFoV = to;
	m_smooth = smooth;

	float totalFoV = abs(m_endFoV-m_startFoV);
	float ownerFoV = GetActorFoVScale();

	m_startFoV = ownerFoV;
	
	float deltaFoV = abs(m_endFoV-m_startFoV)/totalFoV;
	
	if (deltaFoV < totalFoV)
	{
		m_zoomTime = (deltaFoV/totalFoV)*time;
		m_zoomTimer = m_zoomTime;
	}

	m_zoomingIn = true;

	if(!m_zoomed)
		m_initialNearFov = *(float*)gEnv->pRenderer->EF_Query(EFQ_DrawNearFov);

	m_pWeapon->RequireUpdate(eIUS_Zooming);
}

//------------------------------------------------------------------------
void CIronSight::ZoomOut(float time, float from, float to, bool smooth)
{
	m_zoomTimer = time;
	m_zoomTime = m_zoomTimer;

	m_startFoV = from;
	m_endFoV = to;
	m_smooth = smooth;


	float totalFoV = abs(m_endFoV-m_startFoV);
	float ownerFoV = GetActorFoVScale();

	m_startFoV = ownerFoV;

	float deltaFoV = abs(m_endFoV-m_startFoV);

	if (deltaFoV < totalFoV)
	{
		m_zoomTime = (deltaFoV/totalFoV)*time;
		m_zoomTimer = m_zoomTime;
	}

	m_zoomingIn = false;

	m_pWeapon->RequireUpdate(eIUS_Zooming);
}

//------------------------------------------------------------------------
void CIronSight::OnEnterZoom()
{
	CActor* pActor = m_pWeapon->GetOwnerActor();
	if (pActor && pActor->IsClient())
	{
		if (g_pGameCVars->g_dof_ironsight != 0)
		{
			if (m_zoomparams.dof)
			{
				gEnv->p3DEngine->SetPostEffectParam("Dof_UseMask", 1);
				gEnv->p3DEngine->SetPostEffectParamString("Dof_MaskTexName", UseAlternativeIronSight()? m_zoomparams.alternate_dof_mask : m_zoomparams.dof_mask);
			}
			else
			{
				gEnv->p3DEngine->SetPostEffectParam("Dof_UseMask", 0);
			}
			gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 1);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusRange", -1);
		}

		if (m_zoomparams.blur_amount > 0.0f)
		{
			gEnv->p3DEngine->SetPostEffectParam("FilterMaskedBlurring_Amount", m_zoomparams.blur_amount);
			gEnv->p3DEngine->SetPostEffectParamString("FilterMaskedBlurring_MaskTexName", m_zoomparams.blur_mask);
		}

		if (pActor->GetActorClass() == CPlayer::GetActorClassType())
		{
			CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
			if (g_pGameCVars->bt_ironsight && (!g_pGameCVars->bt_speed || (pPlayer->GetNanoSuit() && (pPlayer->GetNanoSuit()->GetMode() == NANOMODE_SPEED))))
			{
				g_pGame->GetBulletTime()->Activate(true);
			}
		}
	}
	m_swayTime = 0.0f;
}

//------------------------------------------------------------------------
void CIronSight::OnZoomedIn()
{
	m_pWeapon->PlayLayer(m_zoomparams.layer, CItem::eIPAF_Default|CItem::eIPAF_NoBlend);
	m_pWeapon->SetDefaultIdleAnimation(CItem::eIGS_FirstPerson, m_actions.idle);

	m_zoomed = true;

	CActor* pActor = m_pWeapon->GetOwnerActor();
	if (pActor && pActor->IsClient())
	{
		if(m_zoomparams.dof)
		{
			m_focus = 1.0f;

			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusRange", -1.0f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_BlurAmount", 1.0f);
			gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 1);
		}
	}

	ApplyZoomMod(m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode()));
	m_swayCycle = 0.0f;
	m_lastRecoil = 0.0f;

	m_pWeapon->SetFPWeapon(0.45f,true);
}

//------------------------------------------------------------------------
void CIronSight::OnLeaveZoom()
{
	m_pWeapon->SetFPWeapon(0.1f,true);
	ClearBlur();
	ClearDoF();
}

//------------------------------------------------------------------------
void CIronSight::OnZoomedOut()
{
	m_zoomed = false;
	ClearDoF();
	m_pWeapon->ExitViewmodes();
	if (CActor *pActor = m_pWeapon->GetOwnerActor())
	{
		if(CScreenEffects *pSFX = pActor->GetScreenEffects())
		{
			pSFX->ClearBlendGroup(pActor->m_autoZoomOutID);
			pSFX->ClearBlendGroup(pActor->m_autoZoomInID);
			pSFX->ClearBlendGroup(pActor->m_hitReactionID);
			pSFX->EnableBlends(true, pActor->m_autoZoomInID);
			pSFX->EnableBlends(true, pActor->m_autoZoomOutID);
			pSFX->EnableBlends(true, pActor->m_hitReactionID);
		}
		if(pActor->IsClient())
		{
			if(SPlayerStats* pStats = static_cast<SPlayerStats*>(pActor->GetActorStats()))
				pStats->FPWeaponSwayOn = false;
		}
	}


	//Reset spread and recoil modifications
	IFireMode* pFireMode = m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode());

	if(pFireMode)
	{
		pFireMode->ResetSpreadMod();
		pFireMode->ResetRecoilMod();
	}

	if(m_zoomparams.scope_mode)
		ResetFovAndPosition();

}

//------------------------------------------------------------------------
void CIronSight::OnZoomStep(bool zoomingIn, float t)
{
	m_focus = 0.0f;
}

//------------------------------------------------------------------------
void CIronSight::UpdateDepthOfField(CActor* pActor, float frameTime, float t)
{
	if (pActor)
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
		if (IMovementController *pMV = pActor->GetMovementController())
		{
			SMovementState ms;
			pMV->GetMovementState(ms);
			Vec3 start = ms.eyePosition;
			Vec3 dir = ms.eyeDirection;
			static ray_hit hit;	

			IPhysicalEntity* pSkipEntities[10];
			int nSkip = CSingle::GetSkipEntities(m_pWeapon, pSkipEntities, 10);
			// jitter the direction (non-uniform disk sampling ... we want to bias the center in this case)
			f32 cosTheta, sinTheta;
			f32 theta = Random() * gf_PI2;
			f32 spreadAngle = DEG2RAD(g_pGameCVars->g_dof_sampleAngle)/2.0f;
			f32 scale = tan_tpl(spreadAngle);
			f32 radiusSqrt = scale * Random();
			sincos_tpl(theta, &cosTheta, &sinTheta);
			f32 x = radiusSqrt * cosTheta;
			f32 y = radiusSqrt * sinTheta;
			
			Matrix33 viewRotation(pPlayer->GetViewQuatFinal());

			Vec3 xOff = x * viewRotation.GetColumn0();
			Vec3 yOff = y * viewRotation.GetColumn2();
			
			// jitter
			if (true)
			{
				dir += xOff + yOff;
				dir.Normalize();
			}
			const static float minRelaxSpeed = 10.0f;
			const static float maxRelaxSpeed = 1.0f;

			f32 delta;

			if (gEnv->pPhysicalWorld->RayWorldIntersection(start, 1000.0f*dir, ent_all,
				rwi_pierceability(10)|rwi_ignore_back_faces, &hit, 1, pSkipEntities, nSkip))
			{
				delta = g_pGameCVars->g_dof_minHitScale*hit.dist - m_minDoF;
				Limit(delta, -g_pGameCVars->g_dof_minAdjustSpeed, g_pGameCVars->g_dof_minAdjustSpeed);
				//delta *= fabs(delta/minAdjustSpeed);
				m_minDoF += delta * frameTime;
				
				delta = g_pGameCVars->g_dof_maxHitScale*hit.dist - m_maxDoF;
				Limit(delta, -g_pGameCVars->g_dof_maxAdjustSpeed, g_pGameCVars->g_dof_maxAdjustSpeed);
				//delta *= fabs(delta/maxAdjustSpeed);
				m_maxDoF += delta * frameTime;
			}
			if (m_maxDoF - g_pGameCVars->g_dof_distAppart < m_minDoF)
			{
				m_maxDoF = m_minDoF + g_pGameCVars->g_dof_distAppart;
			}
			else
			{
				// relax max to min
				delta = m_minDoF - m_maxDoF;
				Limit(delta, -maxRelaxSpeed, maxRelaxSpeed);
				//delta *= fabs(delta/maxRelaxSpeed);
				m_maxDoF += delta * frameTime;
			}
			// the average is relaxed to the center between min and max
			m_averageDoF = (m_maxDoF - m_minDoF)/2.0f;
			Limit(delta, -g_pGameCVars->g_dof_averageAdjustSpeed, g_pGameCVars->g_dof_averageAdjustSpeed);
			//delta *= fabs(delta/averageAdjustSpeed);
			m_averageDoF += delta * frameTime;
		}
	}
}
//------------------------------------------------------------------------
void CIronSight::Serialize(TSerialize ser)
{
}

//------------------------------------------------------------------------
void CIronSight::SetActorFoVScale(float fovScale, bool sens,bool recoil, bool hbob)
{
	if (!m_pWeapon->GetOwnerActor())
		return;

	SActorParams *pActorParams = m_pWeapon->GetOwnerActor()->GetActorParams();
	if (!pActorParams)
		return;

	pActorParams->viewFoVScale = fovScale;

	if (sens)
		pActorParams->viewSensitivity = GetSensitivityFromFoVScale(fovScale);

	if (hbob)
	{
		float mult = GetHBobFromFoVScale(fovScale);
		pActorParams->weaponInertiaMultiplier = mult;
		pActorParams->weaponBobbingMultiplier = mult;
	}
	

}

//------------------------------------------------------------------------
float CIronSight::GetActorFoVScale() const
{
	if (!m_pWeapon->GetOwnerActor())
		return 1.0f;

	SActorParams *pActorParams = m_pWeapon->GetOwnerActor()->GetActorParams();
	if (!pActorParams)
		return 1.0f;

	return pActorParams->viewFoVScale;
}


//------------------------------------------------------------------------
void CIronSight::SetActorSpeedScale(float scale)
{
	if (!m_pWeapon->GetOwnerActor())
		return;

	if (CActor *pActor=m_pWeapon->GetOwnerActor())
		pActor->SetZoomSpeedMultiplier(scale);
}

//------------------------------------------------------------------------
float CIronSight::GetActorSpeedScale() const
{
	if (CActor *pActor=m_pWeapon->GetOwnerActor())
		return pActor->GetZoomSpeedMultiplier();

	return 1.0f;
}

//------------------------------------------------------------------------
float CIronSight::GetSensitivityFromFoVScale(float scale) const
{
	float mag = GetMagFromFoVScale(scale);
	if (mag<=0.99f)
		return 1.0f;

	return 1.0f/(mag*m_zoomparams.sensitivity_ratio);
}

//------------------------------------------------------------------------
float CIronSight::GetHBobFromFoVScale(float scale) const
{
	float mag = GetMagFromFoVScale(scale);
	if (mag<=1.001f)
		return 1.0f;

	return 1.0f/(mag*m_zoomparams.hbob_ratio);
}

//------------------------------------------------------------------------
float CIronSight::GetRecoilFromFoVScale(float scale) const
{
	float mag = GetMagFromFoVScale(scale);
	if (mag<=1.001f)
		return 1.0f;

	return 1.0f/(mag*m_zoomparams.recoil_ratio);
}

//------------------------------------------------------------------------
float CIronSight::GetMagFromFoVScale(float scale) const
{
	assert(scale>0.0f);
	return 1.0f/scale;
}

//------------------------------------------------------------------------
float CIronSight::GetFoVScaleFromMag(float mag) const
{
	assert(mag>0.0f);
	if (mag >= 1.0f)
		return 1.0f/mag;
	else
		return 1.0f;
}

//------------------------------------------------------------------------
float CIronSight::GetZoomFoVScale(int step) const
{
	if (!step)
		return 1.0f;

	return 1.0f/m_zoomparams.stages[step-1];
}

//------------------------------------------------------------------------
void CIronSight::ClearDoF()
{
	CActor* pActor = m_pWeapon->GetOwnerActor();
	if (pActor && pActor->IsClient())
	{
		gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 0.0f);

		if (pActor->GetActorClass() == CPlayer::GetActorClassType())
		{
			CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
			if (g_pGameCVars->bt_ironsight && (!g_pGameCVars->bt_speed || (pPlayer->GetNanoSuit() && (pPlayer->GetNanoSuit()->GetMode() == NANOMODE_SPEED))))
			{
				g_pGame->GetBulletTime()->Activate(false);
			}
		}
	}
}

//------------------------------------------------------------------------
void CIronSight::ClearBlur()
{
	CActor* pActor = m_pWeapon->GetOwnerActor();
	if (pActor && pActor->IsClient())
	{
		gEnv->p3DEngine->SetPostEffectParam("FilterMaskedBlurring_Amount", 0.0f);
	}
}

//-------------------------------------------------------------------------
bool CIronSight::UseAlternativeIronSight() const
{
	bool weaponSupport = m_zoomparams.support_FC_IronSight;
	bool diffEnabled = g_pGameCVars->g_difficultyLevel<4 || gEnv->bMultiplayer;
	bool flagEnabled = (g_pGameCVars->g_enableAlternateIronSight)?true:false;
	bool noScope = !IsScope();
	return (diffEnabled && flagEnabled && weaponSupport);
}

//-------------------------------------------------------------------------
void CIronSight::AdjustScopePosition(float time, bool zoomIn)
{
	Vec3 offset(g_pGameCVars->i_offset_right,g_pGameCVars->i_offset_front,g_pGameCVars->i_offset_up);

	if(zoomIn && (m_currentStep==1))
	{
		if(time>1.0f)
			time = 1.0f;

		offset = m_zoomparams.scope_offset * time;
	}
	else if(!m_currentStep)
	{
		offset = m_zoomparams.scope_offset - m_zoomparams.scope_offset*time;
		if(time>=1.0f)
			offset.zero();
	}

	g_pGameCVars->i_offset_right = offset.x;
	g_pGameCVars->i_offset_front = offset.y;
	g_pGameCVars->i_offset_up = offset.z;
}

//-------------------------------------------------------------------------
void CIronSight::AdjustNearFov(float time, bool zoomIn)
{
	float newFov = -1.0f;
	if(zoomIn && (m_currentStep==1))
	{
		if(time>1.0f)
			time = 1.0f;
		newFov = (m_initialNearFov*(1.0f-time))+(m_zoomparams.scope_nearFov*time);
	}
	else if(!m_currentStep)
	{
		newFov = (m_initialNearFov*time)+(m_zoomparams.scope_nearFov*(1.0f-time));
		if(time>1.0f)
			newFov = m_initialNearFov;
	}
	if(newFov>0.0f)
		gEnv->pRenderer->EF_Query(EFQ_DrawNearFov,(INT_PTR)&newFov);
}

//------------------------------------------------------------------------
void CIronSight::ResetFovAndPosition()
{
	if(m_pWeapon->GetOwnerActor() && m_pWeapon->GetOwnerActor()->IsClient())
	{
		AdjustScopePosition(1.1f,false);
		AdjustNearFov(1.1f,false);
	}
}

//-------------------------------------------------------------------------
void CIronSight::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	m_zoomparams.GetMemoryStatistics(s);
	m_actions.GetMemoryStatistics(s);
	m_spreadModParams.GetMemoryStatistics(s);
	m_recoilModParams.GetMemoryStatistics(s);
}

//--------------------------------------------------------------------------
void CIronSight::ApplyZoomMod(IFireMode* pFM)
{
	if(pFM)
	{
		pFM->PatchSpreadMod(m_spreadModParams);
		pFM->PatchRecoilMod(m_recoilModParams);
	}
}

//--------------------------------------------------------------------------
bool CIronSight::IsToggle()
{
	return !UseAlternativeIronSight();
}

//-----------------------------------------------------------------------
void CIronSight::FilterView(SViewParams &viewparams)
{

	if((m_zoomsway.maxX <=0.0f) && (m_zoomsway.maxY<=0.0f))
		return;

	float x,y;
	
	ZoomSway(gEnv->pTimer->GetFrameTime(),x,y);

	Ang3 viewAngles(viewparams.rotation);
	viewAngles.x += x;
	viewAngles.z += y;

	Quat rotation(viewAngles);
	
	viewparams.rotation = rotation;

}

//--------------------------------------------------------------------------
void CIronSight::ZoomSway(float time, float &x, float&y)
{
	static bool  firing = false;

	bool wasFiring = firing;

	//Update while not firing...
	if(IFireMode* pFM = m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode()))
	{
		if(pFM->IsFiring())
			firing = true;
		else
			firing = false;
	}

	//Reset cycle after firing
	if(wasFiring && !firing)
		m_swayTime = m_zoomsway.stabilizeTime*(1.0f-m_zoomsway.scaleAfterFiring);

	m_swayCycle+=(0.3f*time);

	if(m_swayCycle>1.0f)
		m_swayCycle-=1.0f;

	//Just a simple sin/cos function
	float dtX = cry_sinf(m_swayCycle*g_PI*4.0f);
	float dtY = -cry_cosf(m_swayCycle*g_PI*2.0f);

	m_swayTime += time;

	//Strength scale
	float strengthScale = 1.0f;
	float stanceScale = 1.0f;
	if(CPlayer* pPlayer = static_cast<CPlayer*>(m_pWeapon->GetOwnerActor()))
	{
		if(SPlayerStats* pStats = static_cast<SPlayerStats*>(pPlayer->GetActorStats()))
			pStats->FPWeaponSwayOn = true;

		if(pPlayer->GetNanoSuit() && pPlayer->GetNanoSuit()->GetMode()==NANOMODE_STRENGTH)
			strengthScale = m_zoomsway.strengthScale;

		//Stance mods
		if(pPlayer->GetStance()==STANCE_CROUCH)
			stanceScale = m_zoomsway.crouchScale;
		else if(pPlayer->GetStance()==STANCE_PRONE)
			stanceScale = m_zoomsway.proneScale;
	}

	//Time factor
	float factor = m_zoomsway.minScale;
	float settleTime = m_zoomsway.stabilizeTime*m_zoomsway.strengthScaleTime;
	if(m_swayTime<settleTime)
	{
		factor = (settleTime-m_swayTime)/settleTime;
		if(factor<m_zoomsway.minScale)
			factor = m_zoomsway.minScale;
	}

	//Final displacement
	x = dtX*m_zoomsway.maxX*factor*strengthScale*stanceScale;
	y = dtY*m_zoomsway.maxY*factor*strengthScale*stanceScale;
}

//======================================================
void CIronSight::PostFilterView(SViewParams & viewparams)
{
	if(m_zoomparams.scope_mode)
	{
		if(IFireMode* pFM = m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode()))
		{
			Vec3 front = viewparams.rotation.GetColumn1();
			if(pFM->IsFiring())
			{
				float strengthScale = 1.0f;
				CPlayer* pPlayer = static_cast<CPlayer*>(m_pWeapon->GetOwnerActor());
				if(pPlayer)
				{
					if(pPlayer->GetNanoSuit() && pPlayer->GetNanoSuit()->GetMode()==NANOMODE_STRENGTH)
						strengthScale = 2.0;
				}

				float currentRecoil = pFM->GetRecoil();
				if(currentRecoil>1.5f)
					currentRecoil = 1.5f + Random(-1.25f,0.65f);
				else if(currentRecoil>0.6f)
					currentRecoil = currentRecoil + Random(-0.4f,0.3f);
			
				float scale = 0.01f * currentRecoil * strengthScale;
				front *=scale;
				viewparams.position += front;

				m_lastRecoil = currentRecoil;
			}
			else
			{
				const float decay = 75.0f;
				float currentRecoil = m_lastRecoil - (decay*gEnv->pTimer->GetFrameTime());
				float scale = 0.005f * currentRecoil;
				scale = CLAMP(scale,0.0f,1.0f);
				front *=scale;
				viewparams.position += front;

				m_lastRecoil = max(0.0f,currentRecoil);
			}
		}
	}
}