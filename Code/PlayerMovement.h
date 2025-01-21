// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
// takes input rotations and movement requests, and produces physical impulses/turns based upon current player state

#ifndef __PLAYERMOVEMENT_H__
#define __PLAYERMOVEMENT_H__

#pragma once

#include "Player.h"

class CPlayerMovement
{
public:
	// NOTE: Removed const from player, so that PlayerMovement ZeroG can play sounds through it.
	CPlayerMovement(CPlayer& player, const SActorFrameMovementParams& movement, float frameTime );
	void Process( CPlayer& player );
	void Commit( CPlayer& player );

private:
	const float m_frameTime;
	const SPlayerParams& m_params;
	const SPlayerStats& m_stats;
	SActorFrameMovementParams m_movement;
	CPlayer& m_player;
	const Quat& m_viewQuat;
	const Quat& m_baseQuat;
	const Vec3& m_upVector;
	Vec3 m_worldPos;
	const int m_actions;

	void ProcessFlyMode();
	void ProcessMovementOnLadder(CPlayer& player);
	void ProcessFlyingZeroG();
	void ProcessSwimming();
	void ProcessOnGroundOrJumping( CPlayer& player );
	void ProcessTurning();
	void ProcessParachute();

	void AdjustMovementForEnvironment( Vec3& movement, bool sprinting );
	void AdjustPlayerPositionOnLadder(CPlayer &player);

	SCharacterMoveRequest m_request; // our primary output... how to move!
	bool m_detachLadder; // do we want to detach from a ladder?
	Vec3 m_velocity; // from CPlayer... gets updated here and committed
	float m_onGroundWBoots;
	bool m_jumped;
	Vec3 m_gBootsSpotNormal;
	float m_thrusters;
	float m_zgDashTimer;
	Vec3 m_zgDashWorldDir;
	Quat m_turnTarget;
	bool m_hasJumped;
	float m_waveTimer, m_waveRandomMult;
	float m_stickySurfaceTimer;
	bool m_swimJumping;
};


#endif
