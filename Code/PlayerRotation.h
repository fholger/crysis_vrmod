// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
// takes input rotations and movement requests, and produces physical impulses/turns based upon current player state

#ifndef __PLAYERROTATION_H__
#define __PLAYERROTATION_H__

#pragma once

#include "Player.h"

class CPlayerRotation
{
public:
	CPlayerRotation( const CPlayer& player, const SActorFrameMovementParams& movement, float frameTime );
	void Process();
	void Commit( CPlayer& player );

private:
	const float m_frameTime;
	const SPlayerParams& m_params;
	const SPlayerStats& m_stats;
	const CPlayer& m_player;
	const int m_actions;

	Ang3 m_deltaAngles;
	
	void GetStanceAngleLimits(float & minAngle,float & maxAngle);
	ILINE float GetLocalPitch()
	{
		return asin((m_baseQuat.GetInverted() * m_viewQuat.GetColumn1()).z);
	}
	
	void ProcessFlyingZeroG();
	void ProcessNormalRoll();
	void ProcessAngularImpulses();
	void ProcessNormal();
	void ProcessLean();
	void ProcessFreeFall();
	void ProcessParachute();

	void ClampAngles();
  
	Quat m_viewQuat;
	Quat m_viewQuatFinal;
	Quat m_baseQuat;

	Quat m_baseQuatLinked;
	Quat m_viewQuatLinked;

	float m_viewRoll;
	Vec3 m_upVector;
	Ang3 m_viewAnglesOffset;
	float m_leanAmount;	  
	float m_absRoll;
	float	m_desiredLeanAmount;

	// angular impulses - from stats
	float m_angularImpulseTime;
	Ang3 m_angularImpulse;
	Ang3 m_angularVel;
	// internal working vars
	Ang3 m_angularImpulseDelta;
};

/*
CPlayerRotation movementProcessing(*this);
movementProcessing.Process();
movementProcessing.Commit(*this);
*/

#endif
