/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a standard helicopter movements

-------------------------------------------------------------------------
History:
- 04:04:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"

#include "IMovementController.h"
#include "IVehicleSystem.h"
#include "VehicleMovementHelicopter.h"
#include "VehicleActionLandingGears.h"
#include "ICryAnimation.h"
#include "GameUtils.h"

#include "IRenderAuxGeom.h"

//------------------------------------------------------------------------
CVehicleMovementHelicopter::CVehicleMovementHelicopter()
: m_pRotorHelper(NULL),
	m_damageActual(0.0f),
	m_steeringDamage(0.0f),
	m_mass(0.0f),
	m_desiredRoll(0.0f),
	m_engineForce(0.0f),
	m_playerAcceleration(0.0f),
	m_noHoveringTimer(0.0f),
	m_xyHelp(0.0f),
	m_vehicleVolume(1.0f),
	m_turbulence(0.0f),
	m_pLandingGears(NULL),
	m_liftPitchAngle(0.0f),
	m_relaxTimer(0.0f),
	m_relaxForce(0.0f),
	m_maxPitchAngleMov(0.0f),
	m_turbulenceMultMax(0.0f),
	m_velDamp(0.0f),
	m_isTouchingGround(false),
	m_timeOnTheGround(50.0f),
	m_desiredPitch(0.0f)
{
	m_currentFwdDir.zero();
	m_currentLeftDir.zero();
	m_currentUpDir.zero();

	m_workingUpDir.zero();
	m_engineUpDir.zero();

	m_netActionSync.PublishActions( CNetworkMovementHelicopter(this) );

	m_playerControls.Init(this);
}

//------------------------------------------------------------------------
bool CVehicleMovementHelicopter::Init(IVehicle* pVehicle, const SmartScriptTable &table)
{
	if (!CVehicleMovementBase::Init(pVehicle, table))
		assert(0);

	MOVEMENT_VALUE("engineWarmupDelay", m_engineWarmupDelay);

  // heli abilities
  MOVEMENT_VALUE("altitudeMax", m_altitudeMax);
  MOVEMENT_VALUE("rotorDiskTiltScale", m_rotorDiskTiltScale);
  MOVEMENT_VALUE("pitchResponsiveness", m_pitchResponsiveness);
  MOVEMENT_VALUE("rollResponsiveness", m_rollResponsiveness);
	MOVEMENT_VALUE("yawResponsiveness", m_yawResponsiveness);
	MOVEMENT_VALUE("enginePowerMax", m_enginePowerMax);
	MOVEMENT_VALUE("rotationDamping", m_rotationDamping);
	MOVEMENT_VALUE("yawPerRoll", m_yawPerRoll);

  // high-level controller abilities
  MOVEMENT_VALUE("maxYawRate", m_maxYawRate);
  MOVEMENT_VALUE("maxFwdSpeed", m_maxFwdSpeed);
  MOVEMENT_VALUE("maxLeftSpeed", m_maxLeftSpeed);
  MOVEMENT_VALUE("maxUpSpeed", m_maxUpSpeed);
  MOVEMENT_VALUE("basicSpeedFraction", m_basicSpeedFraction);
  MOVEMENT_VALUE("yawDecreaseWithSpeed", m_yawDecreaseWithSpeed);
  MOVEMENT_VALUE("tiltPerVelDifference", m_tiltPerVelDifference);
  MOVEMENT_VALUE("maxTiltAngle", m_maxTiltAngle);
  MOVEMENT_VALUE("extraRollForTurn", m_extraRollForTurn);
	MOVEMENT_VALUE("rollForTurnForce", m_rollForTurnForce);
  MOVEMENT_VALUE("yawPerRoll", m_yawPerRoll);
	MOVEMENT_VALUE("pitchActionPerTilt", m_pitchActionPerTilt);
	MOVEMENT_VALUE("pitchInputConst", m_pitchInputConst);
  MOVEMENT_VALUE("powerInputConst", m_powerInputConst);
  MOVEMENT_VALUE("powerInputDamping", m_powerInputDamping);
	MOVEMENT_VALUE("relaxForce", m_relaxForce);
  MOVEMENT_VALUE("yawInputConst", m_yawInputConst);
  MOVEMENT_VALUE("yawInputDamping", m_yawInputDamping);
	MOVEMENT_VALUE("maxRollAngle", m_maxRollAngle);

	m_movementTweaks.Init(table);

	// Initialise the power PID.
	m_powerPID.Reset();
	m_powerPID.m_kP = 0.2f;
	m_powerPID.m_kD = 0.01f;
	m_powerPID.m_kI = 0.0001f;

  m_maxSpeed = 40.f; // empirically determined

	m_pRotorAnim = NULL;

	//m_yaw_controller(-0.03f, 0.0f, 0.00f),
	//m_power_controller(0.4f, 0.0f, -0.1f)

	m_liftPID.Reset();
	m_yawPID.Reset();

	m_liftPID.m_kP = 0.66f;
	m_liftPID.m_kD = 0.2f;
	m_liftPID.m_kI = 0.0f;

	m_yawPID.m_kP = -0.03f;
	m_yawPID.m_kI = 0.0f;
	m_yawPID.m_kD = 0.0f;

  // high-level controller
	Ang3 angles = m_pEntity->GetWorldAngles();
  m_desiredDir = angles.z;

  m_desiredHeight = m_pEntity->GetWorldPos().z;
  m_lastDir = m_desiredDir;
  m_enginePower = 0.0f;

	m_isTouchingGround = false;
	m_timeOnTheGround = 50.0f;

	char* pRotorPartName = NULL;
	if (table->GetValue("rotorPartName", pRotorPartName))
		m_pRotorPart = m_pVehicle->GetPart(pRotorPartName);
	else
		m_pRotorPart = NULL;

	m_desiredPitch = 0.0f;

	ResetActions();

	m_playerDampingBase = 0.1f;
	m_playerDampingRotation = 0.15f;
	m_playerDimLowInput = 0.25f;

	m_playerRotationMult.x = 60.0f;
	m_playerRotationMult.y = 60.0f;
	m_playerRotationMult.z = 10.0f;

	m_strafeForce = 1.0f;

	m_engineUpDir.Set(0.0f, 0.0f, 1.0f);

	m_boostMult = 2.00f;

	ICVar* pSensitivVar = gEnv->pConsole->GetCVar("cl_sensitivity");

	m_playerControls.RegisterValue(&m_liftAction, false, 0.0f, "lift");
	m_playerControls.RegisterAction(eVAI_MoveUp, CHelicopterPlayerControls::eVM_Positive, &m_liftAction);
	m_playerControls.RegisterAction(eVAI_MoveDown, CHelicopterPlayerControls::eVM_Negative, &m_liftAction);

	m_playerControls.RegisterValue(&m_turnAction, false, 0.0f, "roll");
	m_playerControls.RegisterAction(eVAI_TurnLeft, CHelicopterPlayerControls::eVM_Negative, &m_turnAction);
	m_playerControls.RegisterAction(eVAI_TurnRight, CHelicopterPlayerControls::eVM_Positive, &m_turnAction);
	m_playerControls.RegisterAction(eVAI_RotateYaw, CHelicopterPlayerControls::eVM_Positive, &m_turnAction, pSensitivVar);
	m_playerControls.SetActionMult(eVAI_RotateYaw, m_maxYawRate * 0.65f);

	m_playerControls.RegisterValue(&m_desiredRoll, false, gf_PI * 4.0f, "turn");
	m_playerControls.RegisterAction(eVAI_RollLeft, CHelicopterPlayerControls::eVM_Negative, &m_desiredRoll);
	m_playerControls.RegisterAction(eVAI_RollRight, CHelicopterPlayerControls::eVM_Positive, &m_desiredRoll);
	m_playerControls.RegisterAction(eVAI_RotateRoll, CHelicopterPlayerControls::eVM_Positive, &m_desiredRoll, pSensitivVar);

	m_playerControls.RegisterValue(&m_desiredPitch, false, 0.0f, "pitch");
	m_playerControls.RegisterAction(eVAI_RotatePitch, CHelicopterPlayerControls::eVM_Negative, &m_desiredPitch, pSensitivVar);
	m_playerControls.SetActionMult(eVAI_RotatePitch, m_maxYawRate * 0.65f);

	m_pInvertPitchVar = gEnv->pConsole->GetCVar("v_invertPitchControl");
	m_pAltitudeLimitVar = gEnv->pConsole->GetCVar("v_altitudeLimit");
	m_pAltitudeLimitLowerOffsetVar = gEnv->pConsole->GetCVar("v_altitudeLimitLowerOffset");
	m_pStabilizeVTOL = gEnv->pConsole->GetCVar("v_stabilizeVTOL");

	m_turbulenceMultMax = 0.25f;

	return true;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::PostInit()
{
	CVehicleMovementBase::PostInit();
	m_pRotorAnim = m_pVehicle->GetAnimation("rotor");
	m_pRotorHelper = m_pVehicle->GetHelper("rotorSmokeOut");

	for (int i = 0; i < m_pVehicle->GetActionCount(); i++)
	{
		IVehicleAction* pAction = m_pVehicle->GetAction(i);

		m_pLandingGears = CAST_VEHICLEOBJECT(CVehicleActionLandingGears, pAction);
		if (m_pLandingGears)
			break;
	}
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::Release()
{
	CVehicleMovementBase::Release();
	delete this;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::Physicalize()
{
	CVehicleMovementBase::Physicalize();

	pe_simulation_params simParams;
	simParams.dampingFreefall = 0.01f;
	GetPhysics()->SetParams(&simParams);
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::Reset()
{
	CVehicleMovementBase::Reset();

	m_playerControls.Reset();

	m_isEnginePowered = false;

	m_steeringDamage = 0.0f;
	m_damageActual = 0.0f;
	m_turbulence = 0.0f;

	ResetActions();

	m_powerPID.Reset();
	m_liftPID.Reset();
	m_yawPID.Reset();

  // high-level controller
	Ang3 angles = m_pEntity->GetWorldAngles();
  m_desiredDir = angles.z;

  m_desiredHeight = m_pEntity->GetWorldPos().z;
  m_lastDir = m_desiredDir;
  m_enginePower = 0.0f;

	m_isTouchingGround = false;
	m_timeOnTheGround = 50.0f;

	m_desiredPitch = 0.0f;
	m_desiredRoll = 0.0f;

	m_engineForce = 0.0f;

	m_noHoveringTimer = 0.0f;

	m_xyHelp = 0.0f;
	m_liftPitchAngle = 0.0f;
	m_relaxTimer = 0.0f;

	m_velDamp = 0.0f;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::ResetActions()
{
	m_liftAction = 0.0f;
	m_forwardAction = 0.0f;
	m_turnAction = 0.0f;
	m_strafeAction = 0.0f;
	m_rotateAction.zero();
	m_rotateTarget.zero();
	m_actionRoll = 0.0f;
	m_desiredRoll = 0.0f;
	m_playerAcceleration = 0.0f;
}

//------------------------------------------------------------------------
bool CVehicleMovementHelicopter::StartEngine(EntityId driverId)
{
	if (!CVehicleMovementBase::StartEngine(driverId))
	{
		return false;
	}

  m_powerPID.Reset();

	if (m_pRotorAnim)
		m_pRotorAnim->StartAnimation();

	m_playerControls.Reset();

	return true;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::StopEngine()
{
	CVehicleMovementBase::StopEngine();
	ResetActions();

	m_playerControls.Reset();

	m_engineStartup += m_engineWarmupDelay;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params)
{
	CVehicleMovementBase::OnEvent(event, params);

	if (event == eVME_DamageSteering)
	{
		float newSteeringDamage = (((float(cry_rand()) / float(RAND_MAX)) * 2.5f ) + 0.5f);
		m_steeringDamage = max(m_steeringDamage, newSteeringDamage);
	}
  else if (event == eVME_Repair)
  {
    m_steeringDamage = min(m_steeringDamage, params.fValue);

		// bit workaround - we never get a repair message for the last bit (as the damage level hasn't changed).
		//	However, the helicopter only ever sends 1.0 or 0.0 to here...
		if(params.fValue < 0.25)
		{
			m_damageActual = 0.0f;
			m_damage = 0.0f;
		}
  }
	else if (event == eVME_GroundCollision)
	{
		const float stopOver = 1.0f;

		m_isTouchingGround = true;
		m_noHoveringTimer = max(stopOver, m_noHoveringTimer);
	}
	else if (event == eVME_Damage)
	{
		const float stopOver = 1.0f;

		m_noHoveringTimer = max(stopOver, m_noHoveringTimer);
		m_damage = params.fValue;
	}
	else if (event == eVME_WarmUpEngine)
	{
		m_enginePower = m_enginePowerMax;
	}
	else if (event == eVME_PlayerSwitchView)
	{
		if (params.fValue == 1.0f)
		{
			m_desiredRoll = 0.0f;
			m_rotateTarget.y = 0.0f;
		}
	}
	else if (event == eVME_Turbulence)
	{
 		m_turbulence = max(m_turbulence, params.fValue);
	}
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	CryAutoLock<CryFastLock> lk(m_lock);

	CVehicleMovementBase::OnAction(actionId, activationMode, value);

	m_playerControls.OnAction(actionId, activationMode, value);
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::ProcessActions(const float deltaTime)
{
FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );

	UpdateDamages(deltaTime);
	UpdateEngine(deltaTime);

	m_velDamp = 0.125f;

	m_playerControls.ProcessActions(deltaTime);

	Limit(m_forwardAction, -1.0f, 1.0f);
	Limit(m_strafeAction, -1.0f, 1.0f);

	m_actionYaw = 0.0f;

	Matrix33 tm( m_PhysPos.q);
	Ang3 angles = Ang3::GetAnglesXYZ(tm);
	Vec3 worldPos =  m_PhysPos.pos;

	// +ve pitch means nose up
	const float& currentPitch = angles.x;
	// +ve roll means to the left
	const float& currentRoll = angles.y;
	// +ve direction mean rotation anti-clockwise about the z axis - 0 means along y
	float currentDir = angles.z;

	float pitchDeg = RAD2DEG(currentPitch);
	if (m_maxPitchAngleMov != 0.0 && pitchDeg >= (m_maxPitchAngleMov * 0.5f))
	{
		float mult = pitchDeg / (m_maxPitchAngleMov);
		
		if (mult > 1.0f && m_desiredPitch < 0.0f)
		{
			m_desiredPitch *= 0.0f;
			m_actionPitch *= 0.0f;
			m_desiredPitch += 0.5f * mult;
		}
		else if (m_desiredPitch < 0.0f)
		{
			m_desiredPitch *= (1.0f - mult);
			m_desiredPitch += 0.05f;
		}
	}
	else if (m_maxPitchAngleMov != 0.0 && pitchDeg <= (-m_maxPitchAngleMov * 0.5f))
	{
		float mult = abs(pitchDeg) / (m_maxPitchAngleMov);

		if (mult > 1.0f && m_desiredPitch > 0.0f)
		{
			m_desiredPitch *= 0.0f;
			m_actionPitch *= 0.0f;
			m_desiredPitch += 0.5 * mult;
		}
		else if (m_desiredPitch > 0.0f)
		{
			m_desiredPitch *= (1.0f - mult);
			m_desiredPitch -= 0.05f;
		}
	}

  Vec3 currentVel = m_PhysDyn.v;
  Vec3 currentVel2D = currentVel;
  currentVel2D.z = 0.0f;
	if (currentRoll >= DEG2RAD(m_maxRollAngle * 0.5f) && m_desiredRoll > 0.001f)
	{
		float r = currentRoll / DEG2RAD(m_maxRollAngle);
		r = min(1.0f, r * 1.0f);
		r = 1.0f - r;
		m_desiredRoll *= r;
		m_desiredRoll = min(1.0f, m_desiredRoll);
	}
	else if (currentRoll <= DEG2RAD(-m_maxRollAngle * 0.5f) && m_desiredRoll < 0.001f)
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

	if (!iszero(m_desiredPitch))
	{
		m_actionPitch -= m_desiredPitch * m_pitchInputConst;
		Limit(m_actionPitch, -m_maxYawRate, m_maxYawRate);
	}

	m_actionRoll += m_pitchActionPerTilt * m_desiredRoll * (m_playerAcceleration + 1.0f);
	Limit(m_actionRoll, -10.0f, 10.0f);
	Limit(m_actionPitch, -10.0f, 10.0f);

	float relaxRollTolerance = 0.0f;

	if (!iszero(m_turnAction) || abs(m_PhysDyn.w.z) > DEG2RAD(10.0f))
	{
		m_actionYaw += -m_turnAction * m_yawInputConst * GetDamageMult();

		float side = 0.0f;
		if (abs(m_turnAction) > 0.01f)
			side = min(1.0f, max(-1.0f, m_turnAction));

		float roll = DEG2RAD(m_extraRollForTurn * side) - (currentRoll);
		m_actionRoll += max(0.0f, abs(roll)) * side * m_rollForTurnForce;
		
		float pitchComp = abs(currentPitch) / DEG2RAD(2.50f);

		if (pitchComp > 1.0f)
			roll *= pitchComp;

		roll *= max(1.0f, abs(m_PhysDyn.w.z));

		m_actionRoll += roll;

		Limit(m_actionYaw, -m_maxYawRate, m_maxYawRate);
	}

	m_desiredDir = currentDir;
	m_lastDir = currentDir;

	float boost = Boosting() ? m_boostMult : 1.0f;

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

	ProcessActionsLift(deltaTime);

	if (m_pStabilizeVTOL)
	{
		float stabilizeTime = m_pStabilizeVTOL->GetFVal();

		if (stabilizeTime > 0.0f)
		{
			if (m_relaxTimer < 6.0f)
				m_relaxTimer += deltaTime;
			else
			{
				float r = currentRoll - relaxRollTolerance;
				r = min(1.0f, max(-1.0f, r));

				m_actionRoll += -r * m_relaxForce * (m_relaxTimer / 6.0f);
			}
		}
	}

	if (m_netActionSync.PublishActions( CNetworkMovementHelicopter(this) ))
		m_pVehicle->GetGameObject()->ChangedNetworkState(eEA_GameClientDynamic);
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::ProcessActionsLift(float deltaTime)
{
	Ang3 angles = m_pEntity->GetWorldAngles();
	const float& currentPitch = angles.x;
	const float& currentRoll = angles.y;

	float boost = Boosting() ? m_boostMult : 1.0f;

	float workingUpDirZ = max(0.90f, min(1.0f, m_workingUpDir.z));
	float pitchRatio = (workingUpDirZ - 0.95f) / 0.05f;
	float liftAction = 0.0f;
	if (pitchRatio > -0.0001f)
		liftAction = m_liftAction * sin(pitchRatio * gf_PI * 0.5f);

	float fwdAction = 0.0f;
	if (pitchRatio < -0.0001f)
		fwdAction = m_liftAction * cos(pitchRatio * gf_PI * 0.5f);

	if (m_liftAction < 0.0f && pitchRatio < 0.0f)
	{
		liftAction = m_liftAction * (-max(-1.0f, pitchRatio) * 0.5f);
		fwdAction = 0.0f;
	}

	m_noHoveringTimer = 0.0f;

	float gravity;
	pe_simulation_params paramsSim;
	if (GetPhysics()->GetParams(&paramsSim))
		gravity = abs(paramsSim.gravity.z);
	else
		gravity = 9.8f;

	// hovering force
	
	m_control.impulse += Vec3(m_workingUpDir.x, m_workingUpDir.y, min(1.0f, m_workingUpDir.z)) * gravity * (boost);
	m_control.impulse += m_workingUpDir * m_enginePower * gravity * liftAction * boost;

	// forward force

	m_control.impulse += Vec3(m_workingUpDir.x, m_workingUpDir.y, 0.0f) * gravity * m_enginePower * fwdAction;

	if (abs(m_desiredRoll) > 0.0001)
	{
		float side = 1.0f;
		if (currentRoll < 0.0f)
			side = -1.0f;
		else if (currentRoll > 0.0f)
			side = 1.0f;
		else
			side = 0.0f;

		m_control.impulse += m_currentLeftDir * m_enginePower * (m_liftAction + (m_desiredRoll * side)) * boost;
	}

	// force damping

	float horizDamp = m_velDamp;
	float vertDamp = 1.0f;

	if (Boosting())
		horizDamp *= 0.25f;

	float turbulenceMult = 1.0f - min(m_turbulenceMultMax, m_turbulence);
	m_control.impulse.x -= m_PhysDyn.v.x * horizDamp;
	m_control.impulse.y -= m_PhysDyn.v.y * horizDamp;
	m_control.impulse.z -= m_PhysDyn.v.z * vertDamp;

	IActor* pActor = m_pActorSystem->GetActor(m_actorId);

	int profile = g_pGameCVars->v_profileMovement;
	if ((profile == 1 && pActor && pActor->IsClient()) || profile == 2)
	{
		IRenderer* pRenderer = gEnv->pRenderer;
		float color[4] = {1,1,1,1};

		Ang3 localAngles = m_pEntity->GetWorldAngles();

		pRenderer->Draw2dLabel(5.0f, 350.0f, 1.5f, color, false, "pitchRatio: %f,  liftAction: %f,  fwdAction: %f", pitchRatio, liftAction, fwdAction);
	}
}

//===================================================================
// ProcessAI
// This treats the helicopter as able to move in any horizontal direction
// by tilting in any direction. Yaw control is thus secondary. Throttle
// control is also secondary since it is adjusted to maintain or change
// the height, and the amount needed depends on the tilt.
//===================================================================
//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementHelicopter::ProcessAI(const float deltaTime)
{
	FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );
	
	// it's useless to progress further if the engine has yet to be turned on
	if (!m_isEnginePowered)
		return;

	m_movementAction.Clear();
	ResetActions();

	// Our current state
	const Vec3 worldPos =  m_PhysPos.pos;
	const Matrix33 worldMat( m_PhysPos.q);
	const Ang3 worldAngles = Ang3::GetAnglesXYZ(worldMat);

	const Vec3 currentVel = m_PhysDyn.v;
	const Vec3 currentVel2D(currentVel.x, currentVel.y, 0.0f);
	
	m_velDamp = 0.15f;
	
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

	float desiredTiltAngle = m_tiltPerVelDifference * desiredVelChange2D.GetLength();

	float powerfactor = desiredSpeed/m_maxSpeed;
	if ( powerfactor < 1.0f )
	powerfactor = 1.0f;
	Limit(desiredTiltAngle, -(m_maxTiltAngle*powerfactor), (m_maxTiltAngle*powerfactor));

	if (desiredTiltAngle > 0.0001f)
	{
		const Vec3 desiredWorldTiltAxis = Vec3(-desiredVelChange2D.y, desiredVelChange2D.x, 0.0f).GetNormalizedSafe();
		const Vec3 desiredLocalTiltAxis = worldMat.GetTransposed() * desiredWorldTiltAxis;

		float desiredPitch = desiredTiltAngle * desiredLocalTiltAxis.x;
		float desiredRoll = desiredTiltAngle * desiredLocalTiltAxis.y;

		m_actionPitch += m_pitchActionPerTilt * (desiredPitch - worldAngles.x);
		m_actionRoll += m_pitchActionPerTilt * (desiredRoll - worldAngles.y);
	}

	float desiredDir = atan2(-desiredLookDir2D.x, desiredLookDir2D.y);
	while (currentDir < desiredDir - gf_PI)
		currentDir += 2.0f * gf_PI;
	while (currentDir > desiredDir + gf_PI)
		currentDir -= 2.0f * gf_PI;

	m_actionYaw = (desiredDir - currentDir) * m_yawInputConst;
	m_actionYaw += m_yawInputDamping * (currentDir - m_lastDir) / deltaTime;
	m_lastDir = currentDir;

	Limit(m_actionPitch, -1.0f, 1.0f);
	Limit(m_actionRoll, -1.0f, 1.0f);
	Limit(m_actionYaw, -1.0f, 1.0f);
	Limit(m_hoveringPower, -1.0f, 1.0f);

	float currentHeight = worldPos.z;
	if ( m_aiRequest.HasMoveTarget() )
	{
		m_hoveringPower = m_powerPID.Update(currentVel.z, desiredVel.z, -1.0f, 1.0f);
		m_liftAction = 0.0f;
		m_desiredHeight = currentHeight;
	}
	else
	{
		// to keep hovering at the same place
		m_hoveringPower = m_powerPID.Update(currentVel.z, m_desiredHeight - currentHeight, -1.0f, 1.0f);
		m_liftAction = 0.0f;
	}

}

void CVehicleMovementHelicopter::SetSoundMasterVolume(float vol)
{
	m_vehicleVolume = vol;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::PreProcessMovement(const float deltaTime)
{
	IPhysicalEntity* pPhysics = GetPhysics();
	assert(pPhysics);

	float gravity;

	pe_simulation_params paramsSim;
	if (pPhysics->GetParams(&paramsSim))
		gravity = abs(paramsSim.gravity.z);
	else
		gravity = 9.8f;

	m_engineForce = gravity * m_enginePower;

	const Matrix33 tm( m_PhysPos.q);
	const Ang3 angles = Ang3::GetAnglesXYZ(tm);

	m_workingUpDir = m_engineUpDir;
	//m_workingUpDir += (m_rotorDiskTiltScale * Vec3(m_actionRoll, -m_actionPitch, 0.0f));
	m_workingUpDir += (m_rotorDiskTiltScale * Vec3(angles.y, -angles.x, 0.0f));
	m_workingUpDir = tm * m_workingUpDir;
	m_workingUpDir.NormalizeSafe();

	if (m_noHoveringTimer > 0.0f)
		m_noHoveringTimer -= deltaTime;

}

//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementHelicopter::ProcessMovement(const float deltaTime)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );

	IPhysicalEntity* pPhysics = GetPhysics();
	assert(pPhysics);

	m_netActionSync.UpdateObject(this);

	CryAutoLock<CryFastLock> lk(m_lock);

	Vec3& impulse = m_control.impulse;
	Vec3& angImpulse = m_control.angImpulse;

	impulse.zero();
	angImpulse.zero();

	m_actionPitch = m_actionRoll = m_actionYaw = m_hoveringPower = 0.0f;

	if (!m_isEnginePowered)
		return;

  // This results in either ProcessActions or ProcessAI getting called
	CVehicleMovementBase::ProcessMovement(deltaTime);
  
	const Matrix33 tm( m_PhysPos.q);
	Ang3 angles = Ang3::GetAnglesXYZ(tm);

	m_currentFwdDir		= tm * Vec3(0.0f, 1.0f , 0.0f);
	m_currentLeftDir	= tm * Vec3(-1.0f, 0.0f, 0.0f);
	m_currentUpDir		= tm * Vec3(0.0f, 0.0f , 1.0f);

	m_mass				= m_PhysDyn.mass;
	Vec3& velocity		= m_PhysDyn.v;
	Vec3& angVelocity	= m_PhysDyn.w;

	float gravity;

	pe_simulation_params paramsSim;
	if (pPhysics->GetParams(&paramsSim))
		gravity = abs(paramsSim.gravity.z);
	else
		gravity = 9.8f;
	
	UpdateDamages(deltaTime);
	UpdateEngine(deltaTime);
	PreProcessMovement(deltaTime);

	m_control.iSource = 3;
	m_control.iApplyTime = 0;

	if (!m_controlDamages.impulse.IsZero() || !m_controlDamages.angImpulse.IsZero())
	{
		m_controlDamages.iSource = 3;
		m_controlDamages.iApplyTime = 0;

		pPhysics->Action(&m_controlDamages,1);
	}

	if (abs(angles.x) < 0.01f)
		angles.x = 0.0f;
	if (abs(angles.y) < 0.01f)
		angles.y = 0.0f;

	float turbulenceMult = 1.0f - min(m_turbulenceMultMax, m_turbulence);

	Vec3 engineImpulse;
	engineImpulse  = m_workingUpDir * m_engineForce * m_hoveringPower;

	impulse += (engineImpulse - (velocity * m_velDamp * turbulenceMult));
	impulse *= deltaTime * m_mass;

	angImpulse += -m_currentLeftDir * m_actionPitch * m_pitchResponsiveness;
	angImpulse += m_currentFwdDir * m_actionRoll * m_rollResponsiveness;
	angImpulse += m_currentUpDir * m_actionYaw * m_yawResponsiveness;
	angImpulse += m_currentUpDir * m_steeringDamage * m_yawResponsiveness * 0.5f;
	angImpulse *= m_mass * deltaTime;

	angImpulse -= (angVelocity - m_controlDamages.angImpulse) * m_mass * m_rotationDamping * deltaTime;

	float powerScale = GetEnginePower();
	impulse *= powerScale;
	angImpulse *= powerScale;

	// apply the action
	pPhysics->Action(&m_control, 1);

	m_turbulence -= m_turbulence * deltaTime;
	if (m_turbulence < 0.01)
		m_turbulence = 0.0f;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::UpdateDamages(float deltaTime)
{
	Vec3& impulse = m_controlDamages.impulse;
	Vec3& angImpulse = m_controlDamages.angImpulse;

	impulse.zero();
	angImpulse.zero();
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::UpdateEngine(float deltaTime)
{
	// will update the engine power up to the maximum according to the ignition time

	if (m_isEnginePowered && !m_isEngineGoingOff)
	{
		if (m_enginePower < m_enginePowerMax)
		{
			m_enginePower += deltaTime * (m_enginePowerMax / m_engineWarmupDelay);
			m_enginePower = min(m_enginePower, m_enginePowerMax);
		}
	}
	else
	{
		if (m_enginePower >= 0.0f)
		{
			m_enginePower -= deltaTime * (m_enginePowerMax / m_engineWarmupDelay);
			if (m_damageActual > 0.0f)
				m_enginePower *= 1.0f - min(1.0f, m_damageActual);
		}
	}
}

// Lookup value from 3 point linear spline.
// (tn, vn) pairs define the spline values at certain points in time the inbetween values are linearly interpolated.
//------------------------------------------------------------------------
inline float LookupSpline( float t, float t0, float v0, float t1, float v1, float t2, float v2 )
{
	if( t < t0 )
		return v0;
	if( t > t2 )
		return t2;

	float	ts, dt, dv;

	if( t < t1 )
	{
		ts = t0;
		dt = t1 - t0;
		dv = v1 - v0;
	}
	else
	{
		ts = t1;
		dt = t2 - t1;
		dv = v2 - v1;
	}

	return v0 + (t - t0) / dt * dv;
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::Update(const float deltaTime)
{
  FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );

	CVehicleMovementBase::Update(deltaTime);

	CryAutoLock<CryFastLock> lk(m_lock);

	if (m_isTouchingGround)
	{
		m_timeOnTheGround += deltaTime;
		m_isTouchingGround = false;
	}
	else
	{
		m_timeOnTheGround = 0.0f;
	}

	// ai specific sound matter

	if ( m_soundMasterVolume != m_vehicleVolume )
	{
		float vol = m_soundMasterVolume;
		if ( m_vehicleVolume == 0 )
			CVehicleMovementBase::SetSoundMasterVolume(m_vehicleVolume);
		else if ( vol < m_vehicleVolume )
		{
			vol +=deltaTime;
			if ( vol > m_vehicleVolume )
				vol = m_vehicleVolume;
			CVehicleMovementBase::SetSoundMasterVolume(vol);
		}
		else if ( vol > m_vehicleVolume )
		{
			vol -=deltaTime;
			if ( vol < m_vehicleVolume )
				vol = m_vehicleVolume;
			CVehicleMovementBase::SetSoundMasterVolume(vol);
		}
	}

	// update animation
	if (m_pRotorAnim)
		m_pRotorAnim->SetSpeed(m_enginePower / m_enginePowerMax);

	IActor* pActor = m_pActorSystem->GetActor(m_actorId);

	int profile = g_pGameCVars->v_profileMovement;
	if ((profile == 1 && pActor && pActor->IsClient()) || profile == 2)
	{
		IRenderer* pRenderer = gEnv->pRenderer;
		float color[4] = {1,1,1,1};

		Ang3 localAngles = m_pEntity->GetWorldAngles();

		m_mass = m_statusDyn.mass;
		Vec3& velocity = m_statusDyn.v;
		Vec3& angVelocity = m_statusDyn.w;

		pRenderer->Draw2dLabel(5.0f,   0.0f, 2.0f, color, false, "Helicopter movement"); Vec3 i; i = m_control.impulse.GetNormalizedSafe();
		pRenderer->Draw2dLabel(5.0f,  85.0f, 1.5f, color, false, "impulse: %f, %f, %f (%f, %f, %f)", m_control.impulse.x, m_control.impulse.y, m_control.impulse.z, i.x, i.y, i.z);
		pRenderer->Draw2dLabel(5.0f, 100.0f, 1.5f, color, false, "angImpulse: %f, %f, %f", m_control.angImpulse.x, m_control.angImpulse.y, m_control.angImpulse.z); i = velocity.GetNormalizedSafe();
		pRenderer->Draw2dLabel(5.0f, 115.0f, 1.5f, color, false, "velocity: %f, %f, %f (%f) (%f, %f, %f)", velocity.x, velocity.y, velocity.z, velocity.GetLength(), i.x, i.y, i.z);
		pRenderer->Draw2dLabel(5.0f, 130.0f, 1.5f, color, false, "angular velocity: %f, %f, %f", RAD2DEG(angVelocity.x), RAD2DEG(angVelocity.y), RAD2DEG(angVelocity.z));
		pRenderer->Draw2dLabel(5.0f, 160.0f, 1.5f, color, false, "angles: %f, %f, %f (%f, %f, %f)", RAD2DEG(localAngles.x), localAngles.y, localAngles.z, RAD2DEG(localAngles.x), RAD2DEG(localAngles.y), RAD2DEG(localAngles.z));
		pRenderer->Draw2dLabel(5.0f, 175.0f, 1.5f, color, false, "m_rpmScale: %f, damage: %f, damageActual: %f, turbulence: %f", m_rpmScale, m_damage, m_damageActual, m_turbulence);
		pRenderer->Draw2dLabel(5.0f, 190.0f, 1.5f, color, false, "m_turnAction: %f, actionYaw: %f, targetRotation: %f, %f, %f", m_turnAction, m_actionYaw, RAD2DEG(m_rotateTarget.x), RAD2DEG(m_rotateTarget.y), RAD2DEG(m_rotateTarget.z));
		pRenderer->Draw2dLabel(5.0f, 220.0f, 1.5f, color, false, "lift: %f, engineForce: %f, hoveringPower: %f, desiredHeight: %f, boost: %d, fwdAction: %f", m_liftAction, m_engineForce, m_hoveringPower, m_desiredHeight, Boosting(), m_forwardAction);
		pRenderer->Draw2dLabel(5.0f, 235.0f, 1.5f, color, false, "pitchAction:  %f, rollAction:  %f", m_actionPitch, m_actionRoll);
		pRenderer->Draw2dLabel(5.0f, 250.0f, 1.5f, color, false, "desiredPitch: %f, desiredRoll: %f", m_desiredPitch, m_desiredRoll);

		Vec3 direction = m_pEntity->GetWorldTM().GetColumn(1);
		pRenderer->Draw2dLabel(5.0f, 270.0f, 1.5f, color, false, "fwd direction: %.2f, %.2f, %.2f", direction.x, direction.y, direction.z);
		pRenderer->Draw2dLabel(5.0f, 285.0f, 1.5f, color, false, "workingUpDir:  %.2f, %.2f, %.2f", m_workingUpDir.x, m_workingUpDir.y, m_workingUpDir.z);
		pRenderer->Draw2dLabel(5.0f, 300.0f, 1.5f, color, false, "accel:  %f", m_playerAcceleration);
	}
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::OnEngineCompletelyStopped()
{
  CVehicleMovementBase::OnEngineCompletelyStopped();

	RemoveSurfaceEffects();

	if (m_pRotorAnim)
		m_pRotorAnim->StopAnimation();
}

//------------------------------------------------------------------------
float CVehicleMovementHelicopter::GetEnginePedal()
{
	return max(max(abs(m_liftAction), abs(m_forwardAction)), abs(m_strafeAction));
}

//------------------------------------------------------------------------
float CVehicleMovementHelicopter::GetEnginePower()
{
	float enginePower = (m_enginePower / m_enginePowerMax) * (1.0f - min(1.0f, m_damageActual));

	if (m_noHoveringTimer > 0.0f)
		return enginePower *= m_noHoveringTimer;

	return enginePower;
}

//------------------------------------------------------------------------
bool CVehicleMovementHelicopter::RequestMovement(CMovementRequest& movementRequest)
{
	FUNCTION_PROFILER( gEnv->pSystem, PROFILE_GAME );
 
	m_movementAction.isAI = true;
	if (!m_isEnginePowered)
		return false;

	CryAutoLock<CryFastLock> lk(m_lock);

	if (movementRequest.HasLookTarget())
		m_aiRequest.SetLookTarget(movementRequest.GetLookTarget());
	else
		m_aiRequest.ClearLookTarget();

	if (movementRequest.HasMoveTarget())
	{
		Vec3 entityPos = m_pEntity->GetWorldPos();
		Vec3 start(entityPos);
		Vec3 end( movementRequest.GetMoveTarget() );
		Vec3 pos = ( end - start ) * 100.0f;
		pos +=start;
		m_aiRequest.SetMoveTarget( pos );
	}
	else
		m_aiRequest.ClearMoveTarget();

	if (movementRequest.HasDesiredSpeed())
		m_aiRequest.SetDesiredSpeed(movementRequest.GetDesiredSpeed());
	else
		m_aiRequest.ClearDesiredSpeed();

	if (movementRequest.HasForcedNavigation())
	{
		Vec3 entityPos = m_pEntity->GetWorldPos();
		m_aiRequest.SetForcedNavigation(movementRequest.GetForcedNavigation());
		m_aiRequest.SetDesiredSpeed(movementRequest.GetForcedNavigation().GetLength());
		m_aiRequest.SetMoveTarget(entityPos+movementRequest.GetForcedNavigation().GetNormalizedSafe()*100.0f);
	}
	else
		m_aiRequest.ClearForcedNavigation();

	return true;
		
}

//------------------------------------------------------------------------
void CVehicleMovementHelicopter::Serialize(TSerialize ser, unsigned aspects)
{
	CVehicleMovementBase::Serialize(ser, aspects);

	if ((ser.GetSerializationTarget() == eST_Network) &&(aspects & eEA_GameClientDynamic))
	{
		m_netActionSync.Serialize(ser, aspects);
	}
	else if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.Value("enginePower", m_enginePower);
		ser.Value("timeOnTheGround", m_timeOnTheGround);
		ser.Value("lastDir", m_lastDir);
		ser.Value("desiredHeight", m_desiredHeight);
		ser.Value("vehicleVolume",m_vehicleVolume);
		ser.Value("turbulence", m_turbulence);

		ser.Value("steeringDamage",m_steeringDamage);
		ser.Value("noHoveringTimer",m_noHoveringTimer);
		ser.Value("relaxTimer",m_relaxTimer);
		ser.Value("desiredRoll",m_desiredRoll);
		ser.Value("desiredPitch",m_desiredPitch);

		if (ser.IsReading())
			m_isTouchingGround = m_timeOnTheGround > 0.0f;
	}
};

void CVehicleMovementHelicopter::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	m_playerControls.GetMemoryStatistics(s);
}

//------------------------------------------------------------------------
void CHelicopterPlayerControls::Init(CVehicleMovementHelicopter* pHelicopterMovement)
{
	m_pHelicopterMovement = pHelicopterMovement; 
	Reset();
}

//------------------------------------------------------------------------
void CHelicopterPlayerControls::RegisterValue(float* pValue, bool isAccumulator, float perUpdateMult, const string& name)
{
	if (!pValue)
		return;

	m_values.resize(m_values.size() + 1);
	SMovementValue& movementValue = m_values.back();
	movementValue.pValue = pValue;
	movementValue.isAccumulator = isAccumulator;
	movementValue.perUpdateMult = perUpdateMult;
	movementValue.name = name;
	movementValue.serializedValue = 0.0f;

	*pValue = 0.0f;
}

//------------------------------------------------------------------------
void CHelicopterPlayerControls::RegisterAction(TVehicleActionId actionId, EValueModif valueModif, float* pValue, ICVar* pSensivityCVar)
{
	int valueId = 0;
	TMovementValueVector::iterator valueIte = m_values.begin();
	TMovementValueVector::iterator valueEnd = m_values.end();

	for (; valueIte != valueEnd; ++valueIte)
	{
		SMovementValue& movementValue = *valueIte;
		if (movementValue.pValue == pValue)
			break;

		valueId++;
	}

	if (valueIte == valueEnd)
		return;

	m_actions.resize(m_actions.size() + 1);
	SActionInfo& actionInfo = m_actions.back();

	actionInfo.actionId = actionId;
	actionInfo.activationMode = eAAM_OnRelease;
	actionInfo.movementValueId = valueId;
	actionInfo.value = 0.0f;
	actionInfo.valueModif = valueModif;
	actionInfo.pSensivityCVar = pSensivityCVar;
	actionInfo.mult = 1.0f;
}

//------------------------------------------------------------------------
void CHelicopterPlayerControls::SetValueUpdateMult(float* pValue, float perUpdateMult)
{
	TMovementValueVector::iterator valueIte = m_values.begin();
	TMovementValueVector::iterator valueEnd = m_values.end();

	for (; valueIte != valueEnd; ++valueIte)
	{
		SMovementValue& movementValue = *valueIte;
		if (movementValue.pValue == pValue)
		{
			movementValue.perUpdateMult = perUpdateMult;
			return;
		}
	}
}

//------------------------------------------------------------------------
void CHelicopterPlayerControls::SetActionMult(TVehicleActionId actionId, float mult)
{
	TActionInfoVector::iterator actionIte = m_actions.begin();
	TActionInfoVector::iterator actionEnd = m_actions.end();

	for (; actionIte != actionEnd; ++actionIte)
	{
		SActionInfo& actionInfo = *actionIte;

		if (actionInfo.actionId == actionId)
		{
			actionInfo.mult = mult;
		}
	}
}

//------------------------------------------------------------------------
void CHelicopterPlayerControls::Reset()
{
	TMovementValueVector::iterator valueIte = m_values.begin();
	TMovementValueVector::iterator valueEnd = m_values.end();

	for (; valueIte != valueEnd; ++valueIte)
	{
		SMovementValue& movementValue = *valueIte;
		*movementValue.pValue = 0.0f;
	}

	TActionInfoVector::iterator actionIte = m_actions.begin();
	TActionInfoVector::iterator actionEnd = m_actions.end();

	for (; actionIte != actionEnd; ++actionIte)
	{
		SActionInfo& actionInfo = *actionIte;
		actionInfo.activationMode = eAAM_OnRelease;
		actionInfo.value = 0.0f;
	}
}

//------------------------------------------------------------------------
void CHelicopterPlayerControls::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{ 
	TActionInfoVector::iterator actionIte = m_actions.begin();
	TActionInfoVector::iterator actionEnd = m_actions.end();

	for (; actionIte != actionEnd; ++actionIte)
	{
		SActionInfo& actionInfo = *actionIte;

		if (actionInfo.actionId == actionId)
		{
			if (activationMode == eAAM_OnPress)
				actionInfo.value = value * actionInfo.mult;
			else if (activationMode == eAAM_OnHold)
				actionInfo.value = value;
			else if (activationMode == eAAM_OnRelease)
				actionInfo.value = 0.0f;
			else if (activationMode == eAAM_Always)
			{
				if (actionInfo.pSensivityCVar)
				{
					float mouseSensitivity = 0.01f * actionInfo.pSensivityCVar->GetFVal();
					actionInfo.value += value * mouseSensitivity;
				}
				else
					actionInfo.value += value;
			}
			else
				break;

			actionInfo.activationMode = activationMode;

			break;
		}
	}
}

//------------------------------------------------------------------------
void CHelicopterPlayerControls::ProcessActions(const float deltaTime)
{
	TMovementValueVector::iterator valueIte = m_values.begin();
	TMovementValueVector::iterator valueEnd = m_values.end();

	for (; valueIte != valueEnd; ++valueIte)
	{
		SMovementValue& movementValue = *valueIte;
		if (movementValue.isAccumulator)
			*movementValue.pValue -= *movementValue.pValue * movementValue.perUpdateMult * deltaTime;
		else
			*movementValue.pValue = 0.0f;
	}

	TActionInfoVector::iterator actionIte = m_actions.begin();
	TActionInfoVector::iterator actionEnd = m_actions.end();

	for (; actionIte != actionEnd; ++actionIte)
	{
		SActionInfo& actionInfo = *actionIte;
		float& v = *m_values[actionInfo.movementValueId].pValue;

		if (actionInfo.activationMode != 0)
		{
			if (actionInfo.activationMode == eAAM_OnPress || actionInfo.activationMode == eAAM_OnHold 
				|| actionInfo.activationMode == eAAM_OnRelease)
			{
				if (actionInfo.valueModif == eVM_Positive)
					v += actionInfo.value;
				else if (actionInfo.valueModif == eVM_Negative)
					v -= actionInfo.value;
			}	
			else if (actionInfo.activationMode == eAAM_Always)
			{
				v += actionInfo.value;
				actionInfo.value = 0.0f;
			}
		}
	}
}

//------------------------------------------------------------------------
void CHelicopterPlayerControls::Serialize(TSerialize ser, unsigned aspects)
{
	if (ser.GetSerializationTarget() == eST_Network && aspects & CONTROLLED_ASPECT)
	{
		TMovementValueVector::iterator valueIte = m_values.begin();
		TMovementValueVector::iterator valueEnd = m_values.end();

		for (; valueIte != valueEnd; ++valueIte)
		{
			SMovementValue& value = *valueIte;
			ser.Value(value.name.c_str(), *value.pValue, 'vPow');
		}
	}
}

//------------------------------------------------------------------------
void CHelicopterPlayerControls::ClearAllActions()
{
	for (size_t i=0; i<m_actions.size(); ++i)
		m_actions[i].activationMode = 0;
}

//------------------------------------------------------------------------
void CHelicopterPlayerControls::GetMemoryStatistics(ICrySizer * s)
{
	s->AddContainer(m_actions);
	s->AddContainer(m_values);
	for (size_t i=0; i<m_values.size(); i++)
		s->Add(m_values[i].name);
}

//------------------------------------------------------------------------
CNetworkMovementHelicopter::CNetworkMovementHelicopter()
: m_hoveringPower(0.0f),
	m_forwardAction(0.0f),
	m_actionPitch(0.0f),
	m_actionYaw(0.0f),
	m_actionRoll(0.0f),
	m_actionStrafe(0.0f),
  m_boost(false)
{
}

//------------------------------------------------------------------------
float CVehicleMovementHelicopter::GetDamageMult()
{
	float damageMult = (1.0f - min(1.0f, m_damage)) * 0.75f;
	damageMult += 0.25f;

	return damageMult;
}

//------------------------------------------------------------------------
CNetworkMovementHelicopter::CNetworkMovementHelicopter(CVehicleMovementHelicopter *pMovement)
{
	m_hoveringPower = pMovement->m_hoveringPower;
	m_forwardAction = pMovement->m_forwardAction;
	m_actionPitch = pMovement->m_actionPitch;
	m_actionYaw = pMovement->m_actionYaw;
	m_actionRoll = pMovement->m_actionRoll;
	m_actionStrafe = pMovement->m_strafeAction;
	m_desiredDir = pMovement->m_desiredDir;
	m_desiredHeight = pMovement->m_desiredHeight;
  m_boost = pMovement->m_boost;
}

//------------------------------------------------------------------------
void CNetworkMovementHelicopter::UpdateObject(CVehicleMovementHelicopter *pMovement)
{
	pMovement->m_playerControls.ClearAllActions();
	pMovement->m_hoveringPower = m_hoveringPower;
	pMovement->m_forwardAction = m_forwardAction;
	pMovement->m_actionPitch = m_actionPitch;
	pMovement->m_actionYaw = m_actionYaw;
	pMovement->m_actionRoll = m_actionRoll;
	pMovement->m_strafeAction = m_actionStrafe;
	pMovement->m_desiredHeight = m_desiredHeight;
	pMovement->m_desiredDir = m_desiredDir;
  pMovement->m_boost = m_boost;
}

//------------------------------------------------------------------------
void CNetworkMovementHelicopter::Serialize(TSerialize ser, unsigned aspects)
{
	if (ser.GetSerializationTarget()==eST_Network && aspects&CONTROLLED_ASPECT)
	{
		ser.Value("power", m_hoveringPower, 'vPow');
		ser.Value("fws", m_forwardAction, 'vPow');
		ser.Value("strafe", m_actionStrafe, 'vPow');
		ser.Value("pitch", m_actionPitch, 'vAng');
		ser.Value("yaw", m_actionYaw, 'vAng');
		ser.Value("roll", m_actionRoll, 'vAng');
		ser.Value("height", m_desiredHeight, 'iii');
		ser.Value("dir", m_desiredDir, 'iii');
    ser.Value("boost", m_boost, 'bool');
	}
}
