// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#ifndef __PLAYERMOVEMENTCONTROLLER_H__
#define __PLAYERMOVEMENTCONTROLLER_H__

#pragma once

#include "Player.h"
#include "TimeValue.h"
#include "GameUtils.h"

class CPlayerMovementController : public IActorMovementController
{
public:
	CPlayerMovementController( CPlayer * pPlayer );
	inline virtual ~CPlayerMovementController() {}

	virtual void Reset();
	void IdleUpdate(float frameTime);
	virtual bool Update( float frameTime, SActorFrameMovementParams& params );
	ILINE virtual void PostUpdate( float frameTime )
	{
		// GetMovementState is called 4-5 times per entity per frame, and calculating this is expensive
		// => it's better to cache it and just copy when we need it... it's constant after the update
		UpdateMovementState( m_currentMovementState );
		m_idleChecker.Update(frameTime);
	}

	virtual void Release();

	virtual bool RequestMovement( CMovementRequest& request );
	ILINE virtual void GetMovementState( SMovementState& state )
	{
		state = m_currentMovementState;
	};

	virtual bool GetStanceState( EStance stance, float lean, bool defaultPose, SStanceState& state );

	virtual void BindInputs( IAnimationGraphState * pAGState );
	virtual bool GetStats(SStats& stats);

	virtual void Serialize(TSerialize &ser);

	ILINE void StrengthJump(bool strengthJump) { m_strengthJump=strengthJump; };
	ILINE bool ShouldStrengthJump() const { return m_strengthJump; };
	ILINE void ClearStrengthJump() { m_strengthJump=false; };

protected:
	bool UpdateNormal( float frameTime, SActorFrameMovementParams& params );
	virtual void UpdateMovementState( SMovementState& state );

	Vec3 ProcessAimTarget(const Vec3 &newTarget,float frameTime);

protected:
	CPlayer * m_pPlayer;

	typedef bool (CPlayerMovementController::*UpdateFunc)( float frameTime, SActorFrameMovementParams& params );
	UpdateFunc m_updateFunc;

	CMovementRequest m_state;
	float m_desiredSpeed;
	bool m_atTarget;
	bool m_usingLookIK;
	bool m_usingAimIK;
	bool m_aimClamped;
	Vec3 m_lookTarget;
	Vec3 m_aimTarget;
	float m_animTargetSpeed;
	int m_animTargetSpeedCounter;
	Vec3 m_fireTarget;
	EStance m_targetStance;
	bool m_strengthJump;

	SMovementState m_currentMovementState;

	std::vector<Vec3> m_aimTargets;
	int m_aimTargetsCount;
	int m_aimTargetsIterator;
	float m_aimNextTarget;
	
protected:
	class CIdleChecker
	{
	public:
		CIdleChecker();
		void Reset(CPlayerMovementController* pMC);
		void Update(float frameTime);
		bool Process(SMovementState& movementState, CMovementRequest& currentReq, CMovementRequest& newReq);
		ILINE bool IsIdle() const { return m_bInIdle; }
		void Serialize(TSerialize &ser);
	protected:
		CPlayerMovementController* m_pMC;
		float m_timeToIdle;
		float m_movementTimeOut;
		int m_frameID;
		bool m_bHaveInteresting;
		bool m_bInIdle;
		bool m_bHaveValidMove;
	};

	class CTargetInterpolator
	{
	public:
		CTargetInterpolator() { Reset(); }
		void Reset() 
		{ 
			m_lastValue = 0.0f; 
			m_painDelta = 0.0f; 
			m_pain = 0.0f; 
			m_rotate = false; 
			m_target=Vec3(ZERO);
		}

		bool HasTarget( CTimeValue now, CTimeValue timeout ) const 
		{ 
			return m_lastValue > 0.0f && now - m_lastValue < timeout; 
		}
		Vec3 GetTarget() const { return m_target; }
		bool GetTarget( 
			Vec3& target, 
			Vec3& rotateTarget, 
			const Vec3& playerPos, 
			const Vec3& moveDirection, 
			const Vec3& bodyDirection, 
			const Vec3& entityDirection,
			float maxAngle, 
			float distToEnd, 
			float viewFollowMovement, 
			ColorB * clr,
			const char ** bodyTargetType );

		void Update(float frameTime);

		void TargetValue( 
			const Vec3& value, 
			CTimeValue now, 
			CTimeValue timeout, 
			float frameTime, 
			bool updateLastValue, 
			const Vec3& playerPos, 
			float maxAngularVelocity );

		void Serialize(TSerialize &ser)
		{
			if(ser.GetSerializationTarget() != eST_Network)
			{
				ser.Value("lastTime", m_lastValue);
				ser.Value("target", m_target);
				ser.Value("m_painDelta", m_painDelta);
				ser.Value("m_pain", m_pain);
				ser.Value("m_rotate", m_rotate);
			}
		}

	private:
		CTimeValue m_lastValue;
		Vec3 m_target;
		float m_painDelta;
		float m_pain;
		bool	m_rotate;
	};
	CTargetInterpolator m_aimInterpolator;
	CTargetInterpolator m_lookInterpolator;

	IAnimationGraph::InputID m_inputDesiredSpeed;
	IAnimationGraph::InputID m_inputDesiredTurnAngleZ;
	IAnimationGraph::InputID m_inputStance;
	IAnimationGraph::InputID m_inputPseudoSpeed;

	CIdleChecker m_idleChecker;
};

#endif
