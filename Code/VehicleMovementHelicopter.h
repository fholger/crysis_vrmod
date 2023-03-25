/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a standard helicopter movements

-------------------------------------------------------------------------
History:
- 09:05:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEMOVEMENTHELICOPTER_H__
#define __VEHICLEMOVEMENTHELICOPTER_H__

#include "VehicleMovementBase.h"
#include "Network/NetActionSync.h"

class CHelicopterPlayerControls;
class CVehicleMovementHelicopter;
class CVehicleActionLandingGears;

class CNetworkMovementHelicopter
{
public:
	CNetworkMovementHelicopter();
	CNetworkMovementHelicopter(CVehicleMovementHelicopter *pMovement);

	
	typedef CVehicleMovementHelicopter * UpdateObjectSink;

	
	bool operator == (const CNetworkMovementHelicopter &rhs)
	{
		return false;
	};

	bool operator != (const CNetworkMovementHelicopter &rhs)
	{
		return !this->operator==(rhs);
	};

	void UpdateObject( CVehicleMovementHelicopter *pMovement );
	void Serialize(TSerialize ser, unsigned aspects);

	static const uint8 CONTROLLED_ASPECT = eEA_GameClientDynamic;

private:
	float m_hoveringPower;
	float m_forwardAction;
	float m_actionPitch;
	float m_actionYaw;
	float m_actionRoll;
	float m_actionStrafe;
	float m_desiredHeight;
	float m_desiredDir;
  bool  m_boost;
};

class CHelicopterPlayerControls
{
public:

	enum EValueModif
	{
		eVM_Positive = 1,
		eVM_Negative,
	};

	void Init(CVehicleMovementHelicopter* pHelicopterMovement);

	void RegisterValue(float* pValue, bool isAccumulator, float perUpdateMult, const string& name);
	void RegisterAction(TVehicleActionId actionId, EValueModif valueModif, float* pValue, ICVar* pSensivityCVar = NULL);

	void SetValueUpdateMult(float* pValue, float perUpdateMult);
	void SetActionMult(TVehicleActionId actionId, float mult);

	void Reset();
	void OnAction(const TVehicleActionId actionId, int activationMode, float value);
	void ProcessActions(const float deltaTime);
	
	void Serialize(TSerialize ser, unsigned aspects);

	void GetMemoryStatistics(ICrySizer * s);

	void ClearAllActions();

protected:

	struct SMovementValue
	{
		string name;
		float* pValue;
		float serializedValue;
		float perUpdateMult;
		bool isAccumulator;
	};

	struct SActionInfo
	{
		TVehicleActionId actionId;
		int activationMode;
		float value;

		int movementValueId;
		EValueModif valueModif;
		
		float mult;
		ICVar* pSensivityCVar;
	};

	CVehicleMovementHelicopter* m_pHelicopterMovement;

	typedef std::vector<SActionInfo> TActionInfoVector;
	TActionInfoVector m_actions;

	typedef std::vector<SMovementValue> TMovementValueVector;
	TMovementValueVector m_values;

	static const uint8 CONTROLLED_ASPECT = eEA_GameClientDynamic;
};


class CVehicleMovementHelicopter
	: public CVehicleMovementBase
{
public:

	CVehicleMovementHelicopter();
	virtual ~CVehicleMovementHelicopter() {}

	// IVehicleMovement
	virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table);
	virtual void PostInit();
	virtual void Reset();
	virtual void Release();
	virtual void Physicalize();

  virtual EVehicleMovementType GetMovementType() { return eVMT_Air; }
	virtual float GetDamageRatio() { return min(1.0f, max(m_damage, m_steeringDamage/3.0f ) ); }

	virtual bool StartEngine(EntityId driverId);
	virtual void StopEngine();

	virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params);
	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value);

	virtual void ProcessMovement(const float deltaTime);
	virtual void ProcessActions(const float deltaTime);
	virtual void ProcessAI(const float deltaTime);
	
	virtual void Update(const float deltaTime);

	virtual bool RequestMovement(CMovementRequest& movementRequest);

	virtual void Serialize(TSerialize ser, unsigned aspects);
	virtual void SetAuthority( bool auth )
	{
		m_netActionSync.CancelReceived();
	};
	virtual void SetChannelId(uint16 id) {};
	// ~IVehicleMovement

	// CVehicleMovementBase
	virtual void OnEngineCompletelyStopped();
	virtual float GetEnginePedal();
	// ~CVehicleMovementBase

	void SetRotorPart(IVehiclePart* pPart) { m_pRotorPart = pPart; }

	virtual void PreProcessMovement(const float deltaTime);
	virtual void ResetActions();
	virtual void UpdateDamages(float deltaTime);
	virtual void UpdateEngine(float deltaTime);
	virtual void ProcessActions_AdjustActions(float deltaTime) {}
	virtual void SetSoundMasterVolume(float vol);

	virtual void GetMemoryStatistics(ICrySizer * s);

	virtual void ProcessActionsLift(float deltaTime);

protected:

	float GetEnginePower();
	float GetDamageMult();

	pe_action_impulse m_control;
	pe_action_impulse m_controlDamages;

	// animation stuff
	IVehicleAnimation* m_pRotorAnim;

	IVehiclePart* m_pRotorPart;
	IVehicleHelper* m_pRotorHelper;

	CVehicleActionLandingGears* m_pLandingGears;

	// movement settings (shouldn't be changed outside Init())
	float m_engineWarmupDelay;

  // heli abilities
	float m_altitudeMax;
  float m_rotationDamping;
	float m_strafeForce;

  /// Adjustment to the rotor up direction with control input. 1 means direction changes by 45degrees
  /// at full control input
  float m_rotorDiskTiltScale; 
  float m_pitchResponsiveness;
	float m_rollResponsiveness;
  float m_yawResponsiveness;

  // high-level controller abilities
  float m_maxYawRate;
  float m_maxFwdSpeed;
  float m_maxLeftSpeed;
  float m_maxUpSpeed;
  float m_basicSpeedFraction;
  float m_yawDecreaseWithSpeed; ///< units of m/s to half the yawing during turns
  float m_tiltPerVelDifference;
  float m_maxTiltAngle;
  float m_extraRollForTurn;
	float m_rollForTurnForce;
  float m_yawPerRoll;
  float m_pitchActionPerTilt;
  // power control
  float m_powerInputConst;
  float m_powerInputDamping;
  // yaw control
  float m_yawInputConst;
  float m_yawInputDamping;
  float m_lastDir;
	float m_maxRollAngle;

	float m_turbulenceMultMax;
	
	float m_pitchInputConst;

	float m_playerDampingBase;
	float m_playerDampingRotation;
	float m_playerDimLowInput;
	Vec3 m_playerRotationMult;

	float m_relaxForce;

	float m_maxPitchAngleMov;

  // ramps from 0 to m_enginePowerMax during idle-up
	float m_enginePower;
	float m_enginePowerMax;

	// movement states (shouldn't be changed outside ProcessMovement())

	float m_damageActual;
	float m_steeringDamage;
	float m_turbulence;

	// movement actions (to be modified by AI and player actions) and correspond 
  // to low-level "stick" inputs in the range -1 to 1
	float m_hoveringPower;
	float m_actionPitch;
	float m_actionYaw;
	float m_actionRoll;

  // these get set by player and converted into the m_action*** values
  float m_liftAction;
	float m_forwardAction;
	float m_turnAction;
  float m_strafeAction;
	
	Vec3 m_rotateAction;
	Vec3 m_rotateTarget;

	SPID m_liftPID;
	SPID m_yawPID;

  // for the high-level controller
  float m_desiredDir;
  float m_desiredHeight;
	
	bool m_isUsingDesiredPitch;
	float m_desiredPitch;
	float m_desiredRoll;

	// player
	CHelicopterPlayerControls m_playerControls;
	float m_playerAcceleration;
	float m_noHoveringTimer;
	float m_xyHelp;
	float m_liftPitchAngle;
	float m_relaxTimer;

	// ai
	CMovementRequest m_aiRequest;

	// Network related
	CNetActionSync<CNetworkMovementHelicopter> m_netActionSync;

	SPID m_powerPID;
	float	m_LastControl;

	bool m_isTouchingGround;
	float m_timeOnTheGround;

	Vec3 m_currentFwdDir;
	Vec3 m_currentLeftDir;
	Vec3 m_currentUpDir;

	Vec3 m_engineUpDir;
	Vec3 m_workingUpDir;

	float m_engineForce;
	float m_mass;

	float m_velDamp;

	float m_boostMult;
	float m_vehicleVolume;

	ICVar* m_pInvertPitchVar;
	ICVar* m_pAltitudeLimitVar;
	ICVar* m_pAltitudeLimitLowerOffsetVar;
	ICVar* m_pAirControlSensivity;
	ICVar* m_pStabilizeVTOL;

	friend class CNetworkMovementHelicopter;

};

#endif
