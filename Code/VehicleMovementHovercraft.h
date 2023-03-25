
/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements Hovercraft movement

-------------------------------------------------------------------------
History:
- 27:04:2005: Created by Michael Rauh

*************************************************************************/
#ifndef __VEHICLEMOVEMENTHOVERCRAFT_H__
#define __VEHICLEMOVEMENTHOVERCRAFT_H__

#include "VehicleMovementBase.h"
#include "Network/NetActionSync.h"


class CVehicleMovementHovercraft;
class CNetworkMovementHovercraft
{
public:
  CNetworkMovementHovercraft();
  CNetworkMovementHovercraft(CVehicleMovementHovercraft *pMovement);

  typedef CVehicleMovementHovercraft * UpdateObjectSink;

  bool operator == (const CNetworkMovementHovercraft &rhs)
  { 
    return false;
  };

  bool operator != (const CNetworkMovementHovercraft &rhs)
  {
    return !this->operator==(rhs);
  };

  void UpdateObject( CVehicleMovementHovercraft *pMovement );
  void Serialize(TSerialize ser, unsigned aspects);

  static const uint8 CONTROLLED_ASPECT = eEA_GameClientDynamic;

private:
  float m_steer;
  float m_pedal;  
  bool  m_boost;
};

class CVehicleMovementHovercraft  
  : public CVehicleMovementBase
{
public:

  // thruster for hovering
  struct SThruster 
  {      
    SThruster( const Vec3& vpos, const Vec3& vdir )
    {
      pos = vpos;
      dir = vdir;    
      prevHit.zero();
      maxForce = 0;      
      tiltAngle = 0; 
      hoverHeight = -1.f; // if -1, inherit from Hovercraft table
      hoverVariance = -1.f; 
      heightAdaption = -1.f; 
      pPart = 0;
      pParentPart = 0;
      heightInitial = 0;      
      prevDist = 0.f;
      levelOffsetInitial = 0.f;
      levelOffset = 0.f;
      cylinderRadius = 0.f;
      pushing = true;
      groundContact = false;
      hit = false;
      enabled = true;
    }

    Vec3 pos;   // in vehicle frame
    Vec3 dir;  
    Vec3 prevHit;
    float tiltAngle;
    float maxForce;                
    float hoverHeight;
    float hoverVariance;
    float heightAdaption;
    float levelOffsetInitial; // intial offset in vehicle frame for leveling out
    float levelOffset; // additional desired offset (for tilting)
    IVehicleHelper* pHelper; // optionally
    IVehiclePart* pPart; // helper part
    IVehiclePart* pParentPart; // optionally
    float heightInitial; // local initial height, used for height adaption
    float prevDist; // previous hit dist
    float cylinderRadius; // if > 0, use cylinder instead of ray geom
    bool hit;
    bool pushing;
    bool groundContact;    
    bool enabled;
  };

  CVehicleMovementHovercraft();
  virtual ~CVehicleMovementHovercraft();

  virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table);
  virtual void Release();
  virtual void Reset();
	virtual void Physicalize();
  virtual void PostPhysicalize();

  virtual EVehicleMovementType GetMovementType() { return eVMT_Amphibious; }
  
	virtual bool SetParams(const SmartScriptTable &table);
	virtual bool SetSounds(const SmartScriptTable &table) { return true; }

	virtual bool StartEngine(EntityId driverId);	
  virtual void StopEngine();
  virtual bool IsPowered() { return m_isEnginePowered || m_bEngineAlwaysOn; }
	virtual void ProcessMovement(const float deltaTime);
  virtual bool RequestMovement(CMovementRequest& movementRequest);
  virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value);

	virtual void Serialize(TSerialize ser, unsigned aspects);
  virtual void SetAuthority( bool auth ) { m_netActionSync.CancelReceived(); }

  virtual bool UseDrivingProxy() const { return true; };

  virtual void Update(const float deltaTime);
  virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params);

	virtual void GetMemoryStatistics(ICrySizer * s);

  friend class CNetworkMovementHovercraft;

protected:

  bool InitThrusters(SmartScriptTable table);

  // thrusters
  typedef std::vector<SThruster*> TThrusters;
  TThrusters m_vecThrusters;

  float m_liftForce;  
  int   m_numThrusters;
  float m_hoverHeight;
  float m_hoverVariance;
  float m_hoverFrequency;
  float m_hoverTimer;
  float m_thrusterMaxHeightCoeff;  
  float m_stiffness;
  float m_damping;
  float m_thrusterTilt;
  float m_dampLimitCoeff;
  bool  m_bSampleByHelpers;
  float m_thrusterHeightAdaption;
  float m_thrusterUpdate;
  float m_thrusterTimer;
  int   m_contacts;
      
  // driving
  float m_velMax;
  float m_velMaxReverse;
  float m_accel;
  float m_accelCoeff;
  Vec3  m_pushOffset;
  float m_pushTilt;
  float m_linearDamping;

  // steering
  float m_turnRateMax;
  float m_turnRateReverse;
  float m_turnAccel;
  float m_turnAccelCoeff;  
  float m_cornerForceCoeff;
  Vec3  m_cornerOffset;
  float m_cornerTilt;
  float m_turnDamping;

  Vec3 m_Inertia;
  Vec3 m_gravity;
  Vec3 m_massOffset;
  bool m_bRetainGravity;

  bool m_bEngineAlwaysOn;
  float m_startComplete;
  
  SVehicleMovementAction m_prevAction;
  CNetActionSync<CNetworkMovementHovercraft> m_netActionSync;
	
  // AI related
  // PID controller for speed control.  
  float	m_direction;
  SPID	m_dirPID;
  float	m_steering;
  float m_prevAngle;
};
#endif
