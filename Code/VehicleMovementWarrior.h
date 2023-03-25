
/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements Warrior movement

-------------------------------------------------------------------------
History:
- 22:06:2006: Created by Michael Rauh

*************************************************************************/
#ifndef __VEHICLEMOVEMENTWARRIOR_H__
#define __VEHICLEMOVEMENTWARRIOR_H__

#include "VehicleMovementHovercraft.h"


class CVehicleMovementWarrior  
  : public CVehicleMovementHovercraft
{
public:
  
  struct SThrusterW : public SThruster
  {      
    SThrusterW( const Vec3& vpos, const Vec3& vdir ) : SThruster(vpos, vdir)
    { 
    }    
  };

  enum EWarriorMovement
  {
    eWM_None = 0,
    eWM_Hovering,
    eWM_Leveling,
    eWM_Neutralizing,
    eWM_Last
  };

  CVehicleMovementWarrior();
  virtual ~CVehicleMovementWarrior();

  virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table);  
  virtual void Reset();
	virtual void Physicalize();

  virtual EVehicleMovementType GetMovementType() { return eVMT_Land; }
  
	virtual bool StartEngine(EntityId driverId);	
  virtual void StopEngine();
	virtual void ProcessMovement(const float deltaTime);
  virtual bool RequestMovement(CMovementRequest& movementRequest);

	virtual void Serialize(TSerialize ser, unsigned aspects);
  virtual void Update(const float deltaTime);
  virtual bool UpdateAlways();
  virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params);

	virtual void GetMemoryStatistics(ICrySizer * s);

protected:
  
  virtual bool GenerateWind() { return false; }

private:

  void ResetThrusters();
  void EnableThruster(SThruster* pThruster, bool enable);
  float RotatePart(IVehiclePart* pPart, float angleGoal, int axis, float speed, float deltaTime, float maxDelta = 0.f);
  
  bool IsCollapsing();
  bool IsCollapsed() { return m_collapsed; }
  
  void Collapse();
  void Collapsed(bool collapsed);

  int m_maxThrustersDamaged;
  int m_thrustersDamaged;
  float m_collapsedFeetAngle;
  float m_collapsedLegAngle;
  float m_recoverTime;
  
  float m_collapseTimer;  
  bool m_collapsed;
  bool m_platformDown;

  TThrusters m_thrustersInit;

  IVehiclePart* m_pTurret;
  IVehiclePart* m_pWing;
  IVehiclePart* m_pCannon;

	IVehicleHelper* m_pPlatformPos;
  
};
#endif
