/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a base class for vehicle movements

-------------------------------------------------------------------------
History:
- 09:28:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEMOVEMENTBASE_H__
#define __VEHICLEMOVEMENTBASE_H__

#include <vector>
#include <ParticleParams.h>
#include <IVehicleSystem.h>
#include "Actor.h"
#include "IMaterialEffects.h"
#include "VehicleMovementTweaks.h"

#define WIN32_LEAN_AND_MEAN
#include <platform.h>
#include "CryThread.h"

#define ENGINESOUND_MAX_DIST 150.f
#define ENGINESOUND_IDLE_RATIO 0.2f


struct IGroundEffect;

/** exhaust status structure
*/
struct SExhaustStatus
{
	IVehicleHelper* pHelper;
  int startStopSlot;
  int runSlot;
  int boostSlot;
  int enabled;
    
  SExhaustStatus()
  {    
    startStopSlot = -1;
    runSlot = -1;
    boostSlot = -1;
    enabled = 1;
  }
};

struct TEnvEmitter
{
  TEnvEmitter()
  { 
    layer = -1;      
    slot = -1;
    matId = -1;
    group = -1;
    bContact = false;
    pGroundEffect = 0;    
    active = true;
  }

  QuatT quatT;  // local tm
  int layer; // layer idx the emitter belongs to    
  int slot;  // emitter slot
  int matId; // last surface idx    
  int group; // optional     
  IGroundEffect* pGroundEffect;
  bool bContact; // optional  
  bool active; 
};

struct SEnvParticleStatus
{
  bool initalized;
  
  typedef std::vector<TEnvEmitter> TEnvEmitters;
  TEnvEmitters emitters;  //maps emitter slots to user-defined index
  
  SEnvParticleStatus() : initalized(false)
  {}

  void Serialize(TSerialize ser, unsigned aspects)
  {    
    ser.BeginGroup("EnvParticleStatus");   
    
    int count = emitters.size();
    ser.Value("NumEnvEmitters", count);
    
    if (ser.IsWriting())
    { 
			for(SEnvParticleStatus::TEnvEmitters::iterator it = emitters.begin(); it != emitters.end(); ++it)
			{
				ser.BeginGroup("TEnvEmitterSlot");
        ser.Value("slot", it->slot);
				ser.EndGroup();
			}
    }
    else if (ser.IsReading())
    {
      //assert(count == emitters.size());
      for (int i=0; i<count&&i<emitters.size(); ++i)
			{
				ser.BeginGroup("TEnvEmitterSlot");
        ser.Value("slot", emitters[i].slot);
				ser.EndGroup();
			}
    }

    ser.EndGroup();
  }
};


/** particle status structure
*/
struct SParticleStatus
{  
  std::vector<SExhaustStatus> exhaustStats; // can hold multiple exhausts
  SEnvParticleStatus envStats;

  SParticleStatus()
  { 
  }

  void Serialize(TSerialize ser, unsigned aspects)
  {
    ser.BeginGroup("ExhaustStatus");   
    
    int count = exhaustStats.size();
    ser.Value("NumExhausts", count);
    
    if (ser.IsWriting())
    {
      for(std::vector<SExhaustStatus>::iterator it = exhaustStats.begin(); it != exhaustStats.end(); ++it)      
			{
				ser.BeginGroup("ExhaustRunSlot");
        ser.Value("slot", it->runSlot);
				ser.EndGroup();
			}
    }
    else if (ser.IsReading())
    {
      //assert(count == exhaustStats.size());
      for (int i=0; i<count&&i<exhaustStats.size(); ++i)
			{
				ser.BeginGroup("ExhaustRunSlot");
        ser.Value("slot", exhaustStats[i].runSlot); 
				ser.EndGroup();
			}
    }   
    ser.EndGroup();

    envStats.Serialize(ser, aspects);
  }
};

struct SSurfaceSoundStatus
{
  int matId; 
  float surfaceParam;
  float slipRatio;
  float slipTimer;
  int scratching;

  SSurfaceSoundStatus()
  {
    Reset();
  }

  void Reset()
  {
    matId = 0;
    surfaceParam = 0.f;
    slipRatio = 0.f;
    slipTimer = 0.f;
    scratching = 0;
  }

  void Serialize(TSerialize ser, unsigned aspects)
  {
    ser.BeginGroup("SurfaceSoundStats");
    ser.Value("surfaceParam", surfaceParam);		
    ser.EndGroup();
  }
};

// sounds
enum EVehicleMovementSound
{
  eSID_Start = 0,
  eSID_Run,
  eSID_Stop,
  eSID_Ambience,
  eSID_Bump,
  eSID_Splash,
  eSID_Gear,
  eSID_Slip,
  eSID_Acceleration,
  eSID_Boost,
  eSID_Damage,
  eSID_Max,
};

enum EVehicleMovementAnimation
{
  eVMA_Engine = 0,
  eVMA_Max,
};

struct SMovementSoundStatus
{
  SMovementSoundStatus(){ Reset(); }

  void Reset()
  {    
    for (int i=0; i<eSID_Max; ++i)    
    {      
      sounds[i] = INVALID_SOUNDID;
      lastPlayed[i].SetValue(0);
    }

    inout = 1.f;
  }
  
  tSoundID sounds[eSID_Max];  
  CTimeValue lastPlayed[eSID_Max];
  float inout;
};


namespace 
{
  bool DebugParticles()
  {
    static ICVar* pVar = gEnv->pConsole->GetCVar("v_debugdraw");
    return pVar->GetIVal() == eVDB_Particles;
  }
}

class CVehicleMovementBase : 
  public IVehicleMovement, 
  public IVehicleObject, 
  public ISoundEventListener
{
  IMPLEMENT_VEHICLEOBJECT
public:  
  CVehicleMovementBase();
  virtual ~CVehicleMovementBase();

	virtual bool Init(IVehicle* pVehicle, const SmartScriptTable &table);
	virtual void PostInit();
  virtual void Release();
	virtual void Reset();  
	virtual void Physicalize();
  virtual void PostPhysicalize();

	virtual void ResetInput();

  virtual EVehicleMovementType GetMovementType() { return eVMT_Other; }

	virtual bool StartEngine(EntityId driverId);
	virtual void StopEngine();
	virtual bool IsPowered() { return m_isEnginePowered; }
	virtual void DisableEngine(bool disable);

	virtual float GetDamageRatio() { return m_damage; }

	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value);
	virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params);
  virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);

	virtual void ProcessMovement(const float deltaTime);
	virtual void ProcessActions(const float deltaTime) {}
	virtual void ProcessAI(const float deltaTime) {}
	virtual void Update(const float deltaTime);
  
	virtual void Serialize(TSerialize ser, unsigned aspects);
	virtual void SetChannelId(uint16 id) {};
	virtual void SetAuthority(bool auth){};
  virtual void PostSerialize();

	virtual void OnEngineCompletelyStopped();

	virtual void RequestActions(const SVehicleMovementAction& movementAction);
	virtual bool RequestMovement(CMovementRequest& movementRequest);
	virtual void GetMovementState(SMovementState& movementState);
	virtual bool GetStanceState(EStance stance, float lean, bool defaultPose, SStanceState& state);

	virtual pe_type GetPhysicalizationType() const { return PE_NONE; };
  virtual bool UseDrivingProxy() const { return false; };
  virtual int GetWheelContacts() const { return 0; }
  
	virtual void RegisterActionFilter(IVehicleMovementActionFilter* pActionFilter);
	virtual void UnregisterActionFilter(IVehicleMovementActionFilter* pActionFilter);

  virtual void OnSoundEvent(ESoundCallbackEvent event,ISound *pSound);
  
  virtual void EnableMovementProcessing(bool enable){ m_bMovementProcessingEnabled = enable; }
  virtual bool IsMovementProcessingEnabled(){ return m_bMovementProcessingEnabled; }

	virtual void OnValuesTweaked() {}

  virtual void ProcessEvent(SEntityEvent& event);
  virtual void SetSoundMasterVolume(float vol);
  
protected:

  ILINE IPhysicalEntity* GetPhysics() const { return m_pVehicle->GetEntity()->GetPhysics(); }
  bool IsProfilingMovement();
	
  // sound methods
  ISound* PlaySound(EVehicleMovementSound eSID, float pulse=0.f, const Vec3& offset=Vec3Constants<float>::fVec3_Zero, int soundFlags=0);
  ISound* GetOrPlaySound(EVehicleMovementSound eSID, float pulse=0.f, const Vec3& offset=Vec3Constants<float>::fVec3_Zero, int soundFlags=0);
  void StopSound(EVehicleMovementSound eSID);
  void StopSounds();
  ISound* GetSound(EVehicleMovementSound eSID);
  const string& GetSoundName(EVehicleMovementSound eSID);
  void SetSoundParam(EVehicleMovementSound eSID, const char* param, float value);
  void SetSoundParam(ISound* pSound, const char* param, float value);
  tSoundID GetSoundId(EVehicleMovementSound eSID);
  void DebugDraw(const float deltaTime);

  // animation methiods
  void StartAnimation(EVehicleMovementAnimation eAnim);
  void StopAnimation(EVehicleMovementAnimation eAnim);
  void SetAnimationSpeed(EVehicleMovementAnimation eAnim, float speed);
  
  void SetDamage(float damage, bool fatal);
  virtual void UpdateDamage(const float deltaTime);
  
  virtual void UpdateGameTokens(const float deltaTime);
  virtual void UpdateRunSound(const float deltaTime);
  virtual void UpdateSpeedRatio(const float deltaTime);
  virtual float GetEnginePedal(){ return m_movementAction.power; }
  
  virtual void Boost(bool enable);
  virtual bool Boosting() { return m_boost; }
  virtual void UpdateBoost(const float deltaTime);
  virtual void ResetBoost();

  void ApplyAirDamp(float angleMin, float angVelMin, float deltaTime, int threadSafe);
  void UpdateGravity(float gravity);
  
  // surface particle/sound methods
  virtual void InitExhaust();
  virtual void InitSurfaceEffects();
  virtual void ResetParticles();
  virtual void UpdateSurfaceEffects(const float deltaTime);  
  virtual void RemoveSurfaceEffects();
  virtual void GetParticleScale(const SEnvironmentLayer& layer, float speed, float power, float& countScale, float& sizeScale, float& speedScale);
  virtual void EnableEnvEmitter(TEnvEmitter& emitter, bool enable);
  virtual void UpdateExhaust(const float deltaTime);  
  void FreeEmitterSlot(int& slot);
  void FreeEmitterSlot(const int& slot);
  void StartExhaust(bool ignition=true, bool reload=true);
  void StopExhaust();  
  float GetWaterMod(SExhaustStatus& exStatus);
  float GetSoundDamage();
  
  SMFXResourceListPtr GetEffectNode(int matId);
  const char* GetEffectByIndex(int matId, const char* username);
  float GetSurfaceSoundParam(int matId);
  
  virtual bool GenerateWind() { return true; }
  void InitWind();
  void UpdateWind(const float deltaTime);
  void SetWind(const Vec3& wind);
  Vec3 GetWindPos(Vec3& posRad, Vec3& posLin);
  // ~surface particle/sound methods

	IVehicle* m_pVehicle;
	IEntity* m_pEntity;
	IEntitySoundProxy* m_pEntitySoundsProxy;	
  static IGameTokenSystem* m_pGameTokenSystem;
  static IVehicleSystem* m_pVehicleSystem;
	static IActorSystem* m_pActorSystem;

  SVehicleMovementAction m_movementAction;
  
  bool m_isEngineDisabled;
	bool m_isEngineStarting;
	bool m_isEngineGoingOff;
	float m_engineStartup;
	float m_engineIgnitionTime;

  bool m_bMovementProcessingEnabled;
	bool m_isEnginePowered;
	float m_damage;
  
  string m_soundNames[eSID_Max];
  
  Vec3 m_enginePos;
  float m_runSoundDelay;  
  float m_rpmScale, m_rpmScaleSgn;
  float m_rpmPitchSpeed;
  float m_maxSoundSlipSpeed;  
  float m_soundMasterVolume;
  
  bool m_boost;
	bool m_wasBoosting;
  float m_boostEndurance;
  float m_boostRegen;  
  float m_boostStrength;
  float m_boostCounter;

  IVehicleAnimation* m_animations[eVMA_Max];
      
  SParticleParams* m_pPaParams;
  SParticleStatus m_paStats;
  SSurfaceSoundStatus m_surfaceSoundStats;
  SMovementSoundStatus m_soundStats;

  EntityId m_actorId;
  pe_status_dynamics m_statusDyn; // gets updated once per update
  pe_status_dynamics m_PhysDyn; // gets updated once per phys update
  pe_status_pos	m_PhysPos; // gets updated once per phys update
  float m_maxSpeed; 
  float m_speedRatio;
  float m_speedRatioUnit;

  float m_measureSpeedTimer;
  Vec3 m_lastMeasuredVel;

  // flight stabilization
  Vec3 m_dampAngle;
  Vec3 m_dampAngVel;  

  IPhysicalEntity* m_pWind[2];
  
	typedef std::list<IVehicleMovementActionFilter*> TVehicleMovementActionFilterList;
	TVehicleMovementActionFilterList m_actionFilters;

  typedef std::vector<IVehicleComponent*> TComponents;
  TComponents m_damageComponents;  
  
  struct SSurfaceSoundInfo
  {
    int paramIndex;

    SSurfaceSoundInfo() : paramIndex(0)
    {}
    SSurfaceSoundInfo(int index) : paramIndex(index)
    {}
  };
  typedef std::map<string, SSurfaceSoundInfo> TSurfaceSoundInfo;
  static TSurfaceSoundInfo m_surfaceSoundInfo;

	CVehicleMovementTweaks m_movementTweaks;
	CVehicleMovementTweaks::TTweakGroupId m_aiTweaksId;
	CVehicleMovementTweaks::TTweakGroupId m_playerTweaksId;
  CVehicleMovementTweaks::TTweakGroupId m_playerBoostTweaksId;
	CVehicleMovementTweaks::TTweakGroupId m_multiplayerTweaksId;

  static float m_sprintTime;

	CryFastLock m_lock;
};

struct SPID
{
	SPID();
	void	Reset();
	float	Update( float inputVal, float setPoint, float clampMin, float clampMax );
  void  Serialize(TSerialize ser);
	float	m_kP;
	float	m_kD;
	float	m_kI;
	float	m_prevErr;
	float	m_intErr;
};

#define MOVEMENT_VALUE_OPT(name, var, t) \
	if (t->GetValue(name, var)) \
	{ \
		m_movementTweaks.AddValue(name, &var); \
	}

#define MOVEMENT_VALUE_REQ(name, var, t) \
	if (t->GetValue(name, var)) \
{ \
	m_movementTweaks.AddValue(name, &var); \
} \
	else \
{ \
	CryLog("Movement Init (%s) - failed to init due to missing <%s> parameter", m_pVehicle->GetEntity()->GetClass()->GetName(), name); \
	return false; \
}

#define MOVEMENT_VALUE(name, var) \
	if (table->GetValue(name, var)) \
{ \
	m_movementTweaks.AddValue(name, &var); \
} \
	else \
{ \
	CryLog("Movement Init (%s) - failed to init due to missing <%s> parameter", m_pVehicle->GetEntity()->GetClass()->GetName(), name); \
	return false; \
}


#endif

