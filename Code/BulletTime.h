// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#ifndef __BULLET_TIME_H__
#define __BULLET_TIME_H__

#pragma once

struct ISound;

class CBulletTime
{
public:
	CBulletTime();

	void Update();
	void Activate(bool activate);
	bool IsActive() const	{	return m_active;	}

private:
	void TimeScaleTarget(float target);

	_smart_ptr<ISound> m_pSoundBegin;
	_smart_ptr<ISound> m_pSoundEnd;

	bool	m_active;
	float m_timeScaleTarget;
	float m_timeScaleCurrent;
	float m_energy;
};
#endif //__BULLET_TIME_H__

