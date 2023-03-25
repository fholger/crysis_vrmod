/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements amphibious movement type

-------------------------------------------------------------------------
History:
- 13:06:2007: Created by MichaelR

*************************************************************************/
#ifndef __VEHICLEMOVEMENTAmphibious_H__
#define __VEHICLEMOVEMENTAmphibious_H__

#include "VehicleMovementStdWheeled.h"
#include "VehicleMovementStdBoat.h"


class CVehicleMovementAmphibious 
  : public CVehicleMovementStdWheeled
{
public:

  CVehicleMovementAmphibious();
  virtual ~CVehicleMovementAmphibious();

  virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table);
  virtual void PostInit();
  virtual void Reset();  
  virtual void PostPhysicalize();

  virtual EVehicleMovementType GetMovementType() { return eVMT_Amphibious; }
  
  virtual bool StartEngine(EntityId driverId);
  virtual void StopEngine();
  virtual void DisableEngine(bool disable);

  virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value);
  virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params);
  virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);
  
  virtual void Update(const float deltaTime);  
  virtual void ProcessMovement(const float deltaTime);
  
  virtual void Serialize(TSerialize ser, unsigned aspects);
  virtual void PostSerialize();
  
  virtual void ProcessEvent(SEntityEvent& event);

  virtual void GetMemoryStatistics(ICrySizer * s);

protected:

  ILINE bool Submerged() { return m_statusDyn.submergedFraction > 0.01f; }
  
  virtual void UpdateRunSound(const float deltaTime);
  
  virtual void Boost(bool enable);
  
  //virtual bool Boosting() { return m_boost; }  
  //virtual void UpdateSurfaceEffects(const float deltaTime);  
 
  CVehicleMovementStdBoat m_boat;
};

#endif
