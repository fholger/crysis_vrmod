// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#ifndef __SHARKMOVEMENTCONTROLLER_H__
#define __SHARKMOVEMENTCONTROLLER_H__

#pragma once

#include "IMovementController.h"
#include "Actor.h"

class CShark;

class CSharkMovementController : public IActorMovementController
{
public:
	CSharkMovementController( CShark * pShark );

	virtual void Reset();
	virtual bool Update( float frameTime, SActorFrameMovementParams& params );
	virtual void PostUpdate( float frameTime ){}
	virtual void Release();

	virtual bool RequestMovement( CMovementRequest& request );
	ILINE virtual void GetMovementState( SMovementState& state )
	{
		state = m_currentMovementState;
	};

	virtual bool GetStanceState( EStance stance, float lean, bool defaultPose, SStanceState& state );

	virtual bool GetStats(SStats& stats)
	{
		return false;
	}

	virtual void Serialize(TSerialize &ser) {}

private:

	void	UpdateCurMovementState(const SActorFrameMovementParams& params);

	CShark * m_pShark;
	bool m_atTarget;
	bool m_exact;

	CMovementRequest m_currentMovementRequest;
	SMovementState m_currentMovementState;
};

#endif
