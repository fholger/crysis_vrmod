/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements movement type for tracked vehicles

-------------------------------------------------------------------------
History:n
- 13:06:2005: Created by MichaelR

*************************************************************************/
#ifndef __VEHICLEMOVEMENTTANK_H__
#define __VEHICLEMOVEMENTTANK_H__

#include "VehicleMovementStdWheeled.h"

class CVehicleMovementTank
  :   public CVehicleMovementStdWheeled
{
public:

  CVehicleMovementTank();
  virtual ~CVehicleMovementTank();

  // overrides from StdWheeled
  virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table);  
  virtual void PostInit();
  virtual void Reset();
  	
	virtual void ProcessAI(const float deltaTime);
	virtual void ProcessMovement(const float deltaTime);
	virtual bool RequestMovement(CMovementRequest& movementRequest);

	virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params);
	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value);
	
  virtual void Update(const float deltaTime);
  virtual void StopEngine();
  virtual void UpdateSounds(const float deltaTime);

	virtual void GetMemoryStatistics(ICrySizer * s);
  // ~StdWheeled

protected:
  
  virtual void UpdateSpeedRatio(const float deltaTime);
  virtual float GetEnginePedal(){ return m_currPedal; }
  virtual bool DoGearSound() { return false; }
  virtual float GetMinRPMSoundRatio() { return 0.6f; }  
  virtual void DebugDrawMovement(const float deltaTime);
  virtual float GetWheelCondition() const;
  void SetLatFriction(float latFric);

  float m_pedalSpeed;
  float m_pedalThreshold;
  float m_steerSpeed;
  float m_steerSpeedRelax;
  float m_steerLimit;
  float m_minFriction;
  float m_maxFriction;
  float m_latFricMin, m_latFricMinSteer, m_latFricMax, m_currentFricMin;
  float m_latSlipMin, m_latSlipMax, m_currentSlipMin;
  
  float m_currPedal;
  float m_currSteer;
  
  IVehiclePart* m_drivingWheels[2];
  float m_steeringImpulseMin;
  float m_steeringImpulseMax;
  float m_steeringImpulseRelaxMin;
  float m_steeringImpulseRelaxMax;

	typedef std::vector<IVehiclePart*> TTreadParts;
	TTreadParts m_treadParts;    
};

#endif
