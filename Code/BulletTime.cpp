// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "BulletTime.h"
#include "GameCVars.h"
#include <ISound.h>
#include <GameUtils.h>

CBulletTime::CBulletTime()
: m_active(false)
, m_timeScaleTarget(1.0f)
, m_timeScaleCurrent(1.0f)
{
	m_energy = g_pGameCVars->bt_energy_max;
}

void CBulletTime::Update()
{
	if (!(g_pGameCVars->bt_speed || g_pGameCVars->bt_ironsight) || gEnv->bMultiplayer)
		return;

	// normalized frametime
	float frameTime = gEnv->pTimer->GetFrameTime();
	float normFrameTime = frameTime;
	float timeScale = gEnv->pTimer->GetTimeScale();
	if (timeScale < 1.0f)
	{
		timeScale = max(0.0001f, timeScale);
		normFrameTime = frameTime/timeScale;
	}

	if (m_active)
	{
		Interpolate(m_energy, 0.0f, g_pGameCVars->bt_energy_decay, normFrameTime);
		// deactivate when we run out of energy
		if (m_energy < 0.001f)
		{
			Activate(false);
		}
	}
	else
	{
		Interpolate(m_energy, g_pGameCVars->bt_energy_max, g_pGameCVars->bt_energy_regen, normFrameTime);
	}

	Interpolate(m_timeScaleCurrent, m_timeScaleTarget, 2.0f, normFrameTime);
	gEnv->pTimer->SetTimeScale(m_timeScaleCurrent);
}

void CBulletTime::Activate(bool activate)
{
	bool changed = (activate != m_active);

	if (!changed || gEnv->bMultiplayer)
		return;

	if (!m_pSoundBegin)
		m_pSoundBegin = gEnv->pSoundSystem->CreateSound("Sounds/interface/hud/slowmotion_activate_01.mp3", FLAG_SOUND_2D|FLAG_SOUND_RELATIVE|FLAG_SOUND_16BITS|FLAG_SOUND_LOAD_SYNCHRONOUSLY);

	if (!m_pSoundEnd)
		m_pSoundEnd= gEnv->pSoundSystem->CreateSound("Sounds/interface/hud/slowmotion_deactivate_01.mp3", FLAG_SOUND_2D|FLAG_SOUND_RELATIVE|FLAG_SOUND_16BITS|FLAG_SOUND_LOAD_SYNCHRONOUSLY);

	if (activate)
	{
		TimeScaleTarget(g_pGameCVars->bt_time_scale);
		gEnv->pSoundSystem->SetSoundActiveState(m_pSoundBegin, eSoundState_None);
		if (m_pSoundEnd)
			m_pSoundEnd->Stop();
		if (m_pSoundBegin)
			m_pSoundBegin->Play();
		gEnv->pSoundSystem->SetMasterPitch(g_pGameCVars->bt_pitch);
	}
	else
	{
		TimeScaleTarget(1.0f);
		gEnv->pSoundSystem->SetSoundActiveState(m_pSoundEnd, eSoundState_None);
		if (m_pSoundEnd)
			m_pSoundEnd->Play();
		if (m_pSoundBegin)
			m_pSoundBegin->Stop();
		gEnv->pSoundSystem->SetMasterPitch(0.0f);
	}

	m_active = activate;
}

void CBulletTime::TimeScaleTarget(float target)
{
	m_timeScaleTarget = target;
}
