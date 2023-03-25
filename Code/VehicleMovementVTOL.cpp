/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a vtol vehicle movement

-------------------------------------------------------------------------
History:
- 13:03:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"

#include "ICryAnimation.h"
#include "IMovementController.h"

#include "IVehicleSystem.h"
#include "VehicleMovementVTOL.h"
#include "VehicleActionLandingGears.h"

#include "IRenderAuxGeom.h"

#include "GameUtils.h"

//------------------------------------------------------------------------
CVehicleMovementVTOL::CVehicleMovementVTOL()
: m_horizontal(0.0f),
	m_isVTOLMovement(true),
	m_forwardInverseMult(0.2f),
	m_wingsAnimTime(1.0f),
	m_wingsAnimTimeInterp(1.0f),
	m_strafeAction(0.0f),
	m_strafeForce(1.0f),
	m_relaxTimer(0.0f),
	m_soundParamTurn(0.0f),
	m_liftPitchAngle(0.0f),
	m_wingsTimer(0.0f),
	m_relaxPitchTime(0.0f),
	m_relaxRollTime(0.0f)
{
}

//------------------------------------------------------------------------
bool CVehicleMovementVTOL::Init(IVehicle* pVehicle, const SmartScriptTable &table)
{
	if (!CVehicleMovementHelicopter::Init(pVehicle, table))
		return false;

	MOVEMENT_VALUE("horizFwdForce", m_horizFwdForce);
	MOVEMENT_VALUE("horizLeftForce", m_horizLeftForce);
	MOVEMENT_VALUE("boostForce", m_boostForce);
	MOVEMENT_VALUE("strafeForce", m_strafeForce);
	MOVEMENT_VALUE("angleLift", m_angleLift);

	m_playerDampingBase = 0.15f;
	m_playerDampingRotation = 0.3f;
	m_playerDimLowInput = 0.01f;

	m_playerRotationMult.x = 55.0f;
	m_playerRotationMult.y = 40.0f;
	m_playerRotationMult.z = 0.0f;

	m_maxFwdSpeedHorizMode = m_maxFwdSpeed;
	m_maxUpSpeedHorizMode = m_maxUpSpeed;

	m_pWingsAnimation = NULL;
	m_wingHorizontalStateId = InvalidVehicleAnimStateId;
	m_wingVerticalStateId = InvalidVehicleAnimStateId;

	if (!table->GetValue("timeUntilWingsRotate", m_timeUntilWingsRotate))
		m_timeUntilWingsRotate = 0.65f;

	m_engineUpDir.Set(0.0f, 0.0f, 1.0f);

	if (!table->GetValue("wingsSpeed", m_wingsSpeed))
		m_wingsSpeed = 1.0f;

	m_playerDampingBase *= 3.0f;

	m_fwdPID.Reset();
	m_fwdPID.m_kP = 0.66f;
	m_fwdPID.m_kD = 0.2f;
	m_fwdPID.m_kI = 0.0f;

	m_relaxForce = 0.50f;
	m_relaxPitchTime = 0.0f;
	m_relaxRollTime = 0.0f;

	m_playerControls.RegisterValue(&m_forwardAction, false, 0.0f, "forward");
	m_playerControls.RegisterAction(eVAI_MoveForward, CHelicopterPlayerControls::eVM_Positive, &m_forwardAction);
	m_playerControls.RegisterAction(eVAI_MoveBack, CHelicopterPlayerControls::eVM_Negative, &m_forwardAction);

	m_playerControls.RegisterValue(&m_strafeAction, false, 0.0f, "strafe");
	m_playerControls.RegisterAction(eVAI_StrafeLeft, CHelicopterPlayerControls::eVM_Negative, &m_strafeAction);
	m_playerControls.RegisterAction(eVAI_StrafeRight, CHelicopterPlayerControls::eVM_Positive, &m_strafeAction);

	m_playerControls.SetActionMult(eVAI_RotateYaw, m_maxYawRate * 0.4f);
	m_playerControls.SetActionMult(eVAI_RotatePitch, m_maxYawRate * 0.4f);

	m_pStabilizeVTOL = gEnv->pConsole->GetCVar("v_stabilizeVTOL");

	m_maxSpeed = 75.0f;

	return true;
}

//------------------------------------------------------------------------
void CVehicleMovementVTOL::PostInit()
{
	CVehicleMovementHelicopter::PostInit();

	m_pWingsAnimation = m_pVehicle->GetAnimation("wings");

	if (m_pWingsAnimation)
	{
		m_wingHorizontalStateId = m_pWingsAnimation->GetStateId("tohorizontal");
		m_wingVerticalStateId = m_pWingsAnimation->GetStateId("tovertical");

		m_pWingsAnimation->ToggleManualUpdate(true);
	}
}

//------------------------------------------------------------------------
void CVehicleMovementVTOL::Reset()
{
	CVehicleMovementHelicopter::Reset();

	m_horizontal = 0.0f;
	m_engineUpDir.Set(0.0f, 0.0f, 1.0f);

	m_isVTOLMovement = true;

	m_wingsAnimTime = 1.0f;
	m_wingsAnimTimeInterp = 1.0f;

	m_relaxPitchTime = 0.0f;
	m_relaxRollTime = 0.0f;

	m_strafeAction = 0.0f;

	m_relaxTimer = 0.0f;

	m_soundParamTurn = 0.0f;

	m_liftPitchAngle = 0.0f;
}

//------------------------------------------------------------------------
void CVehicleMovementVTOL::SetHorizontalMode(float horizontal)
{
	if (horizontal > 0.0f)
	{
		m_horizontal = horizontal;

		m_maxFwdSpeed = m_maxFwdSpeedHorizMode * 0.25f;
		m_maxFwdSpeed += m_maxFwdSpeedHorizMode * 0.75f * m_horizontal;

		m_maxUpSpeed = m_maxUpSpeedHorizMode * 0.50f;
		m_maxUpSpeed += m_maxUpSpeedHorizMode * 0.5f * (1.0f - m_horizontal);

		m_engineUpDir.Set(0.0f, 1.0f, 0.0f);
	}
	else
	{
		m_horizontal = 0.0f;

		m_maxFwdSpeed = m_maxFwdSpeedHorizMode * 0.25f;
		m_maxUpSpeed = m_maxUpSpeedHorizMode;

		m_engineUpDir.Set(0.0f, 0.0f, 1.0f);
	}
}

//------------------------------------------------------------------------
void CVehicleMovementVTOL::PreProcessMovement(const float deltaTime)
{
	CVehicleMovementHelicopter::PreProcessMovement(deltaTime);

	if (abs(m_forwardAction) > 0.0f && m_timeOnTheGround <= 0.01f)
		SetHorizontalMode(1.0f);
	else
		SetHorizontalMode(0.0f);

	if (m_forwardAction > 0.0f && m_timeOnTheGround <= 0.01f)
	{
		m_wingsTimer += deltaTime;
		m_wingsTimer = min(m_wingsTimer, m_timeUntilWingsRotate);
	}
	else
	{
		m_wingsTimer -= deltaTime * 0.65f;
		m_wingsTimer = max(m_wingsTimer, 0.0f);
	}

	Interpolate(m_wingsAnimTime, 1.0f - (m_wingsTimer / m_timeUntilWingsRotate), m_wingsSpeed, deltaTime);

	if (!m_isVTOLMovement)
		return;
	
	IPhysicalEntity* pPhysics = GetPhysics();
	assert(pPhysics);

	float gravity;
	pe_simulation_params paramsSim;
	if (pPhysics->GetParams(&paramsSim))
		gravity = abs(paramsSim.gravity.z);
	else
		gravity = 9.2f;

	float vertical = 1.0f - m_horizontal;

	//m_engineForce = max(1.0f, gravity * vertical) * m_enginePower * max(0.25f, vertical);
	m_engineForce = 0.0f;
	m_engineForce += gravity * vertical * m_enginePower;
	m_engineForce += m_horizontal * m_enginePower;

	Matrix33 tm( m_PhysPos.q);
	Ang3 angles = Ang3::GetAnglesXYZ(tm);

 	m_workingUpDir = m_engineUpDir; //Vec3(0.0f, 0.0f, 1.0f);
	
	m_workingUpDir += (vertical * m_rotorDiskTiltScale * Vec3(angles.y, -angles.x, 0.0f));
	m_workingUpDir += (m_horizontal * m_rotorDiskTiltScale * Vec3(0.0f, 0.0f, angles.z));

	m_workingUpDir = tm * m_workingUpDir;
	m_workingUpDir.z += 0.25f;
	m_workingUpDir.NormalizeSafe();

	float strafe = m_strafeAction * m_strafeForce;

	if (m_noHoveringTimer <= 0.0f)
	{
		Vec3 forwardImpulse;

		float turbulenceMult = 1.0f - min(m_turbulenceMultMax, m_turbulence);

		forwardImpulse = m_currentFwdDir * m_enginePower * m_horizFwdForce * m_horizontal
			* (m_forwardAction + (Boosting() * m_boostForce)) * GetDamageMult() * turbulenceMult;

		if (m_forwardAction < 0.0f)
			forwardImpulse *= m_forwardInverseMult;

		forwardImpulse += m_currentUpDir * m_liftAction * m_enginePower * gravity;
		Vec3 fakeLeftDir = tm * Vec3(-1.0f, 0.0f, 0.0f);
		fakeLeftDir.z = 0.0f;
		forwardImpulse += fakeLeftDir * -strafe * m_enginePower * m_horizLeftForce * turbulenceMult;

 		float horizDamp = 0.25f;
		static float vertDamp = 0.0f;

		if ( m_movementAction.isAI )
			horizDamp *= abs(m_turnAction * 4.0f) + 1.0f;
		else
			horizDamp = m_velDamp;

 		m_control.impulse += forwardImpulse;
		m_control.impulse.x -= m_PhysDyn.v.x * horizDamp * turbulenceMult;
		m_control.impulse.y -= m_PhysDyn.v.y * horizDamp * turbulenceMult;
		m_control.impulse.z -= m_PhysDyn.v.z * vertDamp * turbulenceMult;
	}

	m_workingUpDir.z += 0.45f * m_liftAction;
	m_workingUpDir.NormalizeSafe();


	return;
}

//------------------------------------------------------------------------
void CVehicleMovementVTOL::ResetActions()
{
	CVehicleMovementHelicopter::ResetActions();

	m_actionPitch = 0.0f;
}

//------------------------------------------------------------------------
bool CVehicleMovementVTOL::StartEngine(EntityId driverId)
{
	if (!CVehicleMovementHelicopter::StartEngine(driverId))
	{
		return false;
	}

	if (IActor* pActor = m_pActorSystem->GetActor(m_actorId))
	{
		if ( pActor->IsPlayer() )
		{
			m_isVTOLMovement = true;
		}
		else
		{
			m_isVTOLMovement = false;
		}
	}

	if (m_pWingsAnimation)
	{
		m_pWingsAnimation->StartAnimation();
		m_pWingsAnimation->ToggleManualUpdate(true);
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleMovementVTOL::StopEngine()
{
	CVehicleMovementHelicopter::StopEngine();

	if (m_pWingsAnimation)
	{
		m_pWingsAnimation->StopAnimation();
	}
}

//------------------------------------------------------------------------
void CVehicleMovementVTOL::ProcessActions(const float deltaTime)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );

	UpdateDamages(deltaTime);
	UpdateEngine(deltaTime);

	m_velDamp = 0.25f;

	m_playerControls.ProcessActions(deltaTime);

	Limit(m_forwardAction, -1.0f, 1.0f);
	Limit(m_strafeAction, -1.0f, 1.0f);

	m_actionYaw = 0.0f;

	Vec3 worldPos = m_pEntity->GetWorldPos();

	IPhysicalEntity* pPhysics = GetPhysics();

	// get the current state

	// roll pitch + yaw

	Matrix34 worldTM;
	if (m_pRotorPart)
		worldTM = m_pRotorPart->GetWorldTM();
	else
		worldTM = m_pEntity->GetWorldTM();

	Vec3 specialPos = worldTM.GetTranslation();
	Ang3 angles = Ang3::GetAnglesXYZ(Matrix33(worldTM));

	Matrix33 tm;
	tm.SetRotationXYZ((angles));

	// +ve pitch means nose up
	const float& currentPitch = angles.x;
	// +ve roll means to the left
	const float& currentRoll = angles.y;
	// +ve direction mean rotation anti-clockwise about the z axis - 0 means along y
	float currentDir = angles.z;

	const float maxPitchAngle = 60.0f;
	
	float pitchDeg = RAD2DEG(currentPitch);
	if (pitchDeg >= (maxPitchAngle * 0.75f))
	{
		float mult = pitchDeg / (maxPitchAngle);
		
		if (mult > 1.0f && m_desiredPitch < 0.0f)
		{
			m_desiredPitch *= 0.0f;
			m_actionPitch *= 0.0f;
			m_desiredPitch += 0.2f * mult;
		}
		else if (m_desiredPitch < 0.0f)
		{
			m_desiredPitch *= (1.0f - mult);
			m_desiredPitch += 0.05f;
		}
	}
	else if (pitchDeg <= (-maxPitchAngle * 0.75f))
	{
		float mult = abs(pitchDeg) / (maxPitchAngle);

		if (mult > 1.0f && m_desiredPitch > 0.0f)
		{
			m_desiredPitch *= 0.0f;
			m_actionPitch *= 0.0f;
			m_desiredPitch += 0.2 * mult;
		}
		else if (m_desiredPitch > 0.0f)
		{
			m_desiredPitch *= (1.0f - mult);
			m_desiredPitch -= 0.05f;
		}
	}

	if (currentRoll >= DEG2RAD(m_maxRollAngle * 0.7f) && m_desiredRoll > 0.001f)
	{
		float r = currentRoll / DEG2RAD(m_maxRollAngle);
		r = min(1.0f, r * 1.0f);
		r = 1.0f - r;
		m_desiredRoll *= r;
		m_desiredRoll = min(1.0f, m_desiredRoll);
	}
	else if (currentRoll <= DEG2RAD(-m_maxRollAngle * 0.7f) && m_desiredRoll < 0.001f)
	{
		float r = abs(currentRoll) / DEG2RAD(m_maxRollAngle);
		r = min(1.0f, r * 1.0f);
		r = 1.0f - r;
		m_desiredRoll *= r;
		m_desiredRoll = max(-1.0f, m_desiredRoll);
	}

	Vec3 currentFwdDir2D = m_currentFwdDir;
	currentFwdDir2D.z = 0.0f;
	currentFwdDir2D.NormalizeSafe();

	Vec3 currentLeftDir2D(-currentFwdDir2D.y, currentFwdDir2D.x, 0.0f);

	Vec3 currentVel = m_PhysDyn.v;
	Vec3 currentVel2D = currentVel;
	currentVel2D.z = 0.0f;

	float currentHeight = worldPos.z;
	float currentFwdSpeed = currentVel.Dot(currentFwdDir2D);

	ProcessActions_AdjustActions(deltaTime);

	float inputMult = m_basicSpeedFraction;

	// desired things
	float turnDecreaseScale = m_yawDecreaseWithSpeed / (m_yawDecreaseWithSpeed + fabs(currentFwdSpeed));

	Vec3 desired_vel2D = 
		currentFwdDir2D * m_forwardAction * m_maxFwdSpeed * inputMult + 
		currentLeftDir2D * m_strafeAction * m_maxLeftSpeed * inputMult;

	// calculate the angle changes

	Vec3 desiredVelChange2D = desired_vel2D - currentVel2D;

	float desiredTiltAngle = m_tiltPerVelDifference * desiredVelChange2D.GetLength();
	Limit(desiredTiltAngle, -m_maxTiltAngle, m_maxTiltAngle);

	float goal = abs(m_desiredPitch) + abs(m_desiredRoll);
	goal *= 1.5f;
	Interpolate(m_playerAcceleration, goal, 0.25f, deltaTime);
	Limit(m_playerAcceleration, 0.0f, 5.0f);

	//static float g_angleLift = 4.0f;

	if (abs(m_liftAction) > 0.001f && abs(m_forwardAction) < 0.001)
	{
		float pitch = RAD2DEG(currentPitch);

		if (m_liftPitchAngle < 0.0f && m_liftAction > 0.0f)
			m_liftPitchAngle = 0.0f;
		else if (m_liftPitchAngle > 0.0f && m_liftAction < 0.0f)
			m_liftPitchAngle = 0.0f;

		Interpolate(m_liftPitchAngle, 1.25f * m_liftAction, 0.75f, deltaTime);

		if (m_liftPitchAngle < 1.0f && m_liftPitchAngle > -1.0f)
			m_desiredPitch += 0.05f * m_liftAction;
	}
	else if (m_liftAction < 0.001f && abs(m_liftPitchAngle) > 0.001)
	{
		Interpolate(m_liftPitchAngle, 0.0f, 1.0f, deltaTime);
		m_desiredPitch += 0.05f * -m_liftPitchAngle;
	}

	/* todo
	else if (m_liftAction < -0.001f)
	{
		m_desiredPitch += min(0.0f, (DEG2RAD(-5.0f) - currentPitch)) * 0.5f * m_liftAction;
	}*/

	if (!iszero(m_desiredPitch))
	{
		m_actionPitch -= m_desiredPitch * m_pitchInputConst;
		Limit(m_actionPitch, -m_maxYawRate, m_maxYawRate);
	}

	float rollAccel = 1.0f;
	if (abs(currentRoll + m_desiredRoll) < abs(currentRoll))
		rollAccel *= 1.25f;

	m_actionRoll += m_pitchActionPerTilt * m_desiredRoll * rollAccel * (m_playerAcceleration + 1.0f);
	Limit(m_actionRoll, -10.0f, 10.0f);
	Limit(m_actionPitch, -10.0f, 10.0f);

	// roll as we turn
	if (!m_strafeAction)
	{
		m_actionYaw += m_yawPerRoll * currentRoll;
	}

	if (abs(m_strafeAction) > 0.001f)
	{
		float side = 0.0f;
		side = min(1.0f, max(-1.0f, m_strafeAction));

		float roll = DEG2RAD(m_extraRollForTurn * 0.25f * side) - (currentRoll);
		m_actionRoll += max(0.0f, abs(roll)) * side * 1.0f;
	}

	float relaxRollTolerance = 0.0f;

	if (abs(m_turnAction) > 0.01f || abs(m_PhysDyn.w.z) > DEG2RAD(3.0f))
	{
		m_actionYaw += -m_turnAction * m_yawInputConst * GetDamageMult();

		float side = 0.0f;
		if (abs(m_turnAction) > 0.01f)
			side = min(1.0f, max(-1.0f, m_turnAction));

		float roll = DEG2RAD(m_extraRollForTurn * side) - (currentRoll);
		m_actionRoll += max(0.0f, abs(roll)) * side * m_rollForTurnForce;

		roll *= max(1.0f, abs(m_PhysDyn.w.z));

		m_actionRoll += roll;

		Limit(m_actionYaw, -m_maxYawRate, m_maxYawRate);
	}

	m_desiredDir = currentDir;
	m_lastDir = currentDir;

	float boost = Boosting() ? m_boostMult : 1.0f;
	float liftActionMax = 1.0f;

	if (m_pAltitudeLimitVar)
	{
		float altitudeLimit = m_pAltitudeLimitVar->GetFVal();

		if (!iszero(altitudeLimit))
		{
			float altitudeLowerOffset;

			if (m_pAltitudeLimitLowerOffsetVar)
			{
				float r = 1.0f - min(1.0f, max(0.0f, m_pAltitudeLimitLowerOffsetVar->GetFVal()));
				altitudeLowerOffset = r * altitudeLimit;
			}
			else
				altitudeLowerOffset = altitudeLimit;

			float mult = 1.0f;

			if (currentHeight >= altitudeLimit)
			{
				mult = 0.0f;
			}
			else if (currentHeight >= altitudeLowerOffset)
			{
				float zone = altitudeLimit - altitudeLowerOffset;
				mult = (altitudeLimit - currentHeight) / (zone);
			}

			m_liftAction *= mult;

			if (currentPitch > DEG2RAD(0.0f))
			{
				if (m_forwardAction > 0.0f)
					m_forwardAction *= mult;

				if (m_actionPitch > 0.0f)
				{
					m_actionPitch *= mult;
					m_actionPitch += -currentPitch;
				}
			}

			m_desiredHeight = min(altitudeLowerOffset, currentHeight);
		}
	}
	else
	{
		m_desiredHeight = currentHeight;
	}

	if (abs(m_liftAction) > 0.001f)
	{
		m_liftAction = min(liftActionMax, max(-0.2f, m_liftAction));

		m_hoveringPower = (m_powerInputConst * m_liftAction) * boost;
		m_noHoveringTimer = 0.0f;
	}
	else if (!m_isTouchingGround)
	{
		if (m_noHoveringTimer <= 0.0f)
		{
			float gravity;

			pe_simulation_params paramsSim;
			if (pPhysics->GetParams(&paramsSim))
				gravity = abs(paramsSim.gravity.z);
			else
				gravity = 9.2f;

			float upDirZ = m_workingUpDir.z;

			if (abs(m_forwardAction) > 0.01 && upDirZ > 0.0f)
				upDirZ = 1.0f;
			else if (upDirZ > 0.8f)
				upDirZ = 1.0f;

			float upPower = upDirZ;
			upPower -= min(1.0f, abs(m_forwardAction) * abs(angles.x));

			float turbulenceMult = 1.0f - min(m_turbulenceMultMax, m_turbulence);
			Vec3& impulse = m_control.impulse;
			impulse += Vec3(0.0f, 0.0f, upPower) * gravity * turbulenceMult * GetDamageMult();
			impulse.z -= m_PhysDyn.v.z * turbulenceMult;
		}
		else
		{
			m_noHoveringTimer -= deltaTime;
		}
	}

	if (m_pStabilizeVTOL)
	{
		float stabilizeTime = m_pStabilizeVTOL->GetFVal();

		if (stabilizeTime > 0.0f)
		{
			if (m_relaxTimer < 6.0f)
				m_relaxTimer += deltaTime;
			else
			{
				float pitchTarget;

				if (m_horizontal >= 1.0f)
					pitchTarget = DEG2RAD(0.0f);
				else
					pitchTarget = 0.0f;

				const float g_relaxPitchAnglesWithin = 0.0f;

				float r = currentRoll - relaxRollTolerance;
				r = min(1.0f, max(-1.0f, r));

				m_actionRoll += -r * m_relaxForce * (m_relaxTimer / 6.0f);
			}

		}
	}

	if (m_netActionSync.PublishActions( CNetworkMovementHelicopter(this) ))
		m_pVehicle->GetGameObject()->ChangedNetworkState(eEA_GameClientDynamic);
}

//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementVTOL::ProcessAI(const float deltaTime)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );
	
	if (!m_isVTOLMovement)
	{
		CVehicleMovementHelicopter::ProcessAI(deltaTime);
		return;
	}

	m_velDamp = 0.15f;

	const float maxDirChange = 15.0f;

	// it's useless to progress further if the engine has yet to be turned on
	if (!m_isEnginePowered)
		return;

	m_movementAction.Clear();
	m_movementAction.isAI = true;
	ResetActions();

	// Our current state
	const Vec3 worldPos =  m_PhysPos.pos;
	const Matrix33 worldMat( m_PhysPos.q);
	Ang3 worldAngles = Ang3::GetAnglesXYZ(worldMat);

	const Vec3 currentVel = m_PhysDyn.v;
	const Vec3 currentVel2D(currentVel.x, currentVel.y, 0.0f);
	// +ve direction mean rotation anti-clocwise about the z axis - 0 means along y
	float currentDir = worldAngles.z;

	// to avoid singularity
	const Vec3 vWorldDir = worldMat * FORWARD_DIRECTION;
	const Vec3 vWorldDir2D =  Vec3( vWorldDir.x,  vWorldDir.y, 0.0f ).GetNormalizedSafe();

	// Our inputs
	const float desiredSpeed = m_aiRequest.HasDesiredSpeed() ? m_aiRequest.GetDesiredSpeed() : 0.0f;
	const Vec3 desiredMoveDir = m_aiRequest.HasMoveTarget() ? (m_aiRequest.GetMoveTarget() - worldPos).GetNormalizedSafe() : vWorldDir;
	const Vec3 desiredMoveDir2D = Vec3(desiredMoveDir.x, desiredMoveDir.y, 0.0f).GetNormalizedSafe(vWorldDir2D);
	const Vec3 desiredVel = desiredMoveDir * desiredSpeed; 
	const Vec3 desiredVel2D(desiredVel.x, desiredVel.y, 0.0f);
	const Vec3 desiredLookDir = m_aiRequest.HasLookTarget() ? (m_aiRequest.GetLookTarget() - worldPos).GetNormalizedSafe() : desiredMoveDir;
	const Vec3 desiredLookDir2D = Vec3(desiredLookDir.x, desiredLookDir.y, 0.0f).GetNormalizedSafe(vWorldDir2D);

	// Calculate the desired 2D velocity change
	Vec3 desiredVelChange2D = desiredVel2D - currentVel2D;
	float velChangeLength = desiredVelChange2D.GetLength2D();

	bool isLandingMode = false;
	if (m_pLandingGears && m_pLandingGears->AreLandingGearsOpen())
		isLandingMode = true;

	bool isHorizontal = (desiredSpeed >= 5.0f) && (desiredMoveDir.GetLength2D() > desiredMoveDir.z);
	float desiredPitch = 0.0f;
	float desiredRoll = 0.0f;

	float desiredDir = atan2(-desiredLookDir2D.x, desiredLookDir2D.y);

	while (currentDir < desiredDir - gf_PI)
		currentDir += 2.0f * gf_PI;
	while (currentDir > desiredDir + gf_PI)
		currentDir -= 2.0f * gf_PI;

	float diffDir = (desiredDir - currentDir);
	m_actionYaw = diffDir * m_yawInputConst;
	m_actionYaw += m_yawInputDamping * (currentDir - m_lastDir) / deltaTime;
	m_lastDir = currentDir;

	if (isHorizontal && !isLandingMode)
	{
		float desiredFwdSpeed = desiredVelChange2D.GetLength();

		desiredFwdSpeed *= min(1.0f, diffDir / DEG2RAD(maxDirChange));

		if (!iszero(desiredFwdSpeed))
		{
			const Vec3 desiredWorldTiltAxis = Vec3(-desiredVelChange2D.y, desiredVelChange2D.x, 0.0f);
			const Vec3 desiredLocalTiltAxis = worldMat.GetTransposed() * desiredWorldTiltAxis;

			m_forwardAction = m_fwdPID.Update(currentVel.y, desiredLocalTiltAxis.GetLength(), -1.0f, 1.0f);

		float desiredTiltAngle = m_tiltPerVelDifference * desiredVelChange2D.GetLength();
		Limit(desiredTiltAngle, -m_maxTiltAngle, m_maxTiltAngle);

		if (desiredTiltAngle > 0.0001f)
		{
			const Vec3 desiredWorldTiltAxis = Vec3(-desiredVelChange2D.y, desiredVelChange2D.x, 0.0f).GetNormalizedSafe();
			const Vec3 desiredLocalTiltAxis = worldMat.GetTransposed() * desiredWorldTiltAxis;

			Vec3 vVelLocal = worldMat.GetTransposed() * desiredVel;
			vVelLocal.NormalizeSafe();

			float dotup = vVelLocal.Dot(Vec3( 0.0f,0.0f,1.0f ) );
			float currentSpeed = currentVel.GetLength();

			desiredPitch = dotup *currentSpeed / 100.0f;
			desiredRoll = desiredTiltAngle * desiredLocalTiltAxis.y *currentSpeed/30.0f;
		}

		}
	}
	else
	{
		float desiredTiltAngle = m_tiltPerVelDifference * desiredVelChange2D.GetLength();
		Limit(desiredTiltAngle, -m_maxTiltAngle, m_maxTiltAngle);

		if (desiredTiltAngle > 0.0001f)
		{
			const Vec3 desiredWorldTiltAxis = Vec3(-desiredVelChange2D.y, desiredVelChange2D.x, 0.0f).GetNormalizedSafe();
			const Vec3 desiredLocalTiltAxis = worldMat.GetTransposed() * desiredWorldTiltAxis;

			desiredPitch = desiredTiltAngle * desiredLocalTiltAxis.x;
			desiredRoll = desiredTiltAngle * desiredLocalTiltAxis.y;
		}
	}

	float currentHeight = m_PhysPos.pos.z;
	if ( m_aiRequest.HasMoveTarget() )
	{
		m_hoveringPower = m_powerPID.Update(currentVel.z, desiredVel.z, -1.0f, 4.0f);
		
		//m_hoveringPower = (m_desiredHeight - currentHeight) * m_powerInputConst;
		//m_hoveringPower += m_powerInputDamping * (currentHeight - m_lastHeight) / deltaTime;

		if (isHorizontal)
		{
			if (desiredMoveDir.z > 0.6f || desiredMoveDir.z < -0.85f)
			{
				desiredPitch = max(-0.5f, min(0.5f, desiredMoveDir.z)) * DEG2RAD(35.0f);
				m_forwardAction += abs(desiredMoveDir.z);
			}

			m_liftAction = min(2.0f, max(m_liftAction, m_hoveringPower * 2.0f));
		}
		else
		{
			m_liftAction = 0.0f;
		}
	}
	else
	{
		// to keep hovering at the same place
		m_hoveringPower = m_powerPID.Update(currentVel.z, m_desiredHeight - currentHeight, -1.0f, 1.0f);
		m_liftAction = 0.0f;

		if (m_pVehicle->GetAltitude() > 10.0f)	//TODO: this line is not MTSafe
			m_liftAction = m_forwardAction;
	}

	m_actionPitch += m_pitchActionPerTilt * (desiredPitch - worldAngles.x);
	m_actionRoll += m_pitchActionPerTilt * (desiredRoll - worldAngles.y);

	Limit(m_actionPitch, -1.0f, 1.0f);
	Limit(m_actionRoll, -1.0f, 1.0f);
	Limit(m_actionYaw, -1.0f, 1.0f);

	if (m_horizontal > 0.0001f)
		m_desiredHeight = m_PhysPos.pos.z;
	
	Limit(m_forwardAction, -1.0f, 1.0f);
}

//------------------------------------------------------------------------
void CVehicleMovementVTOL::OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params)
{
	CVehicleMovementHelicopter::OnEvent(event, params);

	if (event == eVME_SetMode)
	{

		bool requestedMovement = (params.iValue == 1);
		if ( requestedMovement != m_isVTOLMovement)
		{
			m_isVTOLMovement = requestedMovement;
			if ( m_isVTOLMovement == true )
				m_pWingsAnimation->ChangeState(m_wingHorizontalStateId);
			else
				m_pWingsAnimation->ChangeState(m_wingVerticalStateId);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleMovementVTOL::Update(const float deltaTime)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );

	CVehicleMovementHelicopter::Update(deltaTime);
	CryAutoLock<CryFastLock> lk(m_lock);

	IActorSystem* pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();
	assert(pActorSystem);
	IActor* pActor = pActorSystem->GetActor(m_actorId);

	if (m_pWingsAnimation)
	{
		if(pActor && pActor->IsClient())
			m_wingsAnimTimeInterp = m_wingsAnimTime;
		else
			Interpolate(m_wingsAnimTimeInterp, m_wingsAnimTime, 2.0f, deltaTime);

		m_pWingsAnimation->SetTime(m_wingsAnimTimeInterp);
	}

	if (pActor && pActor->IsClient())
	{
		float turbulence = m_turbulence;

		if (m_pAltitudeLimitVar)
		{
			float altitudeLimit = m_pAltitudeLimitVar->GetFVal();
			float currentHeight = m_pEntity->GetWorldPos().z;

			if (!iszero(altitudeLimit))
			{
				float altitudeLowerOffset;

				if (m_pAltitudeLimitLowerOffsetVar)
				{
					float r = 1.0f - min(1.0f, max(0.0f, m_pAltitudeLimitLowerOffsetVar->GetFVal()));
					altitudeLowerOffset = r * altitudeLimit;

					if (currentHeight >= altitudeLowerOffset)
					{
						if (currentHeight > altitudeLowerOffset)
						{
							float zone = altitudeLimit - altitudeLowerOffset;
							turbulence += (currentHeight - altitudeLowerOffset) / (zone);
						}
					}
				}
			}
		}

		if (turbulence > 0.0f)
		{
			static_cast<CActor*>(pActor)->CameraShake(0.50f * turbulence, 0.0f, 0.05f, 0.04f, Vec3(0.0f, 0.0f, 0.0f), 10, "VTOL_Update_Turbulence");
		}

		float enginePowerRatio = m_enginePower / m_enginePowerMax;

		if (enginePowerRatio > 0.0f)
		{
			float rpmScaleDesired = 0.2f;
			rpmScaleDesired += abs(m_forwardAction) * 0.8f;
			rpmScaleDesired += abs(m_strafeAction) * 0.4f;
			rpmScaleDesired += abs(m_turnAction) * 0.25f;
			rpmScaleDesired = min(1.0f, rpmScaleDesired);
			Interpolate(m_rpmScale, rpmScaleDesired, 1.0f, deltaTime);
		}

		float turnParamGoal = min(1.0f, abs(m_turnAction)) * 0.6f;
		turnParamGoal *= (min(1.0f, max(0.0f, m_speedRatio)) + 1.0f) * 0.50f;
		turnParamGoal += turnParamGoal * m_boost * 0.25f;
		Interpolate(m_soundParamTurn, turnParamGoal, 0.5f, deltaTime);
		SetSoundParam(eSID_Run, "turn", m_soundParamTurn);

		float damage = GetSoundDamage();
		if (damage > 0.1f)
		{ 
			//if (ISound* pSound = GetOrPlaySound(eSID_Damage, 5.f, m_enginePos))
				//SetSoundParam(pSound, "damage", damage);        
		}
	}
}

//------------------------------------------------------------------------
void CVehicleMovementVTOL::UpdateEngine(float deltaTime)
{
	// will update the engine power up to the maximum according to the ignition time

	float damageMult = GetDamageMult();

	float enginePowerMax = m_enginePowerMax * damageMult; 

	if (m_isEnginePowered && !m_isEngineGoingOff)
	{
		if (m_enginePower < enginePowerMax)
		{
			m_enginePower += deltaTime * (enginePowerMax / m_engineIgnitionTime);
			m_enginePower = min(m_enginePower, enginePowerMax);
		}
		else
		{
			m_enginePower = max(enginePowerMax, m_enginePower);
		}
	}
	else
	{
		if (m_enginePower >= 0.0f)
		{
			float powerReduction = enginePowerMax / m_engineIgnitionTime;
			if (m_damage)
				powerReduction *= 2.0f;

			m_enginePower -= deltaTime * powerReduction;
			m_enginePower = max(m_enginePower, 0.0f);
		}
	}
}

//------------------------------------------------------------------------
float CVehicleMovementVTOL::GetDamageMult()
{
	float damageMult = (1.0f - min(1.0f, m_damage)) * 0.75f;
	damageMult += 0.25f;

	return damageMult;
}

//------------------------------------------------------------------------
void CVehicleMovementVTOL::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	CVehicleMovementHelicopter::GetMemoryStatistics(s);
}

//------------------------------------------------------------------------
void CVehicleMovementVTOL::Serialize(TSerialize ser, unsigned aspects)
{
	CVehicleMovementHelicopter::Serialize(ser, aspects);

	if ((ser.GetSerializationTarget() == eST_Network) &&(aspects & eEA_GameClientDynamic))
	{
		ser.Value("wings", m_wingsAnimTime, 'unit');
	}
	else
	{
		ser.Value("horizontal", m_horizontal);
		ser.Value("wingsTime", m_wingsAnimTime);
		ser.Value("forwardAction", m_forwardAction);
		ser.Value("isVTOLMovement", m_isVTOLMovement); // for ai

		if (ser.IsReading())
			SetHorizontalMode(m_horizontal);
	}
}
