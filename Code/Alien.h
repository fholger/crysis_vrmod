/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Implements the naked alien.
  
 -------------------------------------------------------------------------
  History:
  - 6:12:2004: Created by Filippo De Luca

*************************************************************************/
#ifndef __ALIEN_H__
#define __ALIEN_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Actor.h"
#include <IActionMapManager.h>

struct SViewParams;
struct IItemSystem;
struct IGroundEffect;

struct IDebugHistory;
struct IDebugHistoryManager;

//this might change
struct SAlienInput
{
	Vec3 deltaMovement;//desired movement change: X = side movement, Y = forward, Z = up
	Vec3 deltaRotation;//desired rotation change, X = pitch, Z = yaw, Y will probably be the lean

	int actions;

	//misc
	Vec3 movementVector;//direct movement vector, it will be capped between 0-1 length, used only by AI for now
	Vec3 viewVector;//if len != 0 use as view direction (no roll atm)

	Vec3 viewDir; // requested view direction
	float pathLength; // remaining path length

	Vec3 posTarget;
	Vec3 dirTarget;
	Vec3 upTarget;
	float	speedTarget;

	static const int STICKY_ACTIONS = 
		ACTION_JUMP | 
		ACTION_CROUCH |
		ACTION_LEANLEFT | 
		ACTION_LEANRIGHT |
		ACTION_SPRINT;

	void ResetDeltas()
	{
		deltaMovement.Set(0,0,0);
		deltaRotation.Set(0,0,0);
		movementVector.Set(0,0,0);
		viewVector.Set(0,0,0);

		//REMINDER:be careful setting the actions to 0, some actions may not need to be resetted each frame.
		actions &= ~STICKY_ACTIONS;
	};

	SAlienInput()
	{
		memset(this,0,sizeof(SAlienInput));
		speedTarget = 1.0f;
	}

	void Serialize( TSerialize ser );
};

//this might change
struct SAlienStats : public SActorStats
{
	float	bobCycle;

	float sprintLeft;
	float sprintTreshold;
	float sprintMaxSpeed;

	bool isThirdPerson;
	
	bool isFiring;
	bool isFloating;
  
	Vec3 eyePos;
	Ang3 eyeAngles;

	//LM
	float desiredSpeed;
	//

	//AI
	Vec3 lookTargetSmooth;
	Vec3 fireDir;
	Vec3 fireDirGoal;

	//misc
	float physicsAnimationRatio;
	Vec3 animationSpeedVec;
	Vec3 lastRootPos;

  //Vec3 angVelocity;

  bool cloaked;
	//Vec3 dynModelOffset;

	SAlienStats()
	{
		memset(this,0,sizeof(SAlienStats));

		fireDir.Set(0,1,0);
		fireDirGoal.Set(0,1,0);

		zeroGUp.Set(0,0,1);
	}
	void Serialize( TSerialize ser );

};

struct SAlienParams : public SActorParams
{
	float speedInertia;
	float rollAmount;
	float rollSpeed;
		
	float sprintMultiplier;
	float sprintDuration;

	float rotSpeed_min;
	float rotSpeed_max;
		
	float speed_min;

	float forceView;
	
	float movingBend;

	float idealSpeed;
	float blendingRatio;
	float	approachLookat;

	char fullAnimTentacles[256];
	float fullAnimationTentaclesBlendMult;
	int8 tentaclesCollide;

	char tentaclesMaterial[64];
	float tentaclesRadius;
	float tentaclesJointLimit;
	float tentacleStiffnessDecay;
	float tentacleDampAnim;


	float cameraShakeRange;
	float cameraShakeMultiplier;

	char groundEffect[128];
	float groundEffectHeight;
  float groundEffectHeightScale;
  float groundEffectBaseScale;
  float groundEffectMaxSpeed;

  char  trailEffect[128];
  float trailEffectMinSpeed;
  float trailEffectMaxSpeedSize;
  float trailEffectMaxSpeedCount;
	Vec3	trailEffectDir;
    
	char  healthTrailEffect[128];
	float healthTrailEffectMaxSize;
	float healthTrailEffectMaxCount;
	Vec3	healthTrailEffectDir;

  float turnSoundMaxVel;
  int16 turnSoundBoneId;
    
	SAlienParams()
	{
		memset(this,0,sizeof(SAlienParams));

		speedInertia = 3.0f;
		rollAmount = 1.0f;
		rollSpeed = 1.0f;
		
		sprintMultiplier = 1.5f;
		sprintDuration = 0.5f;
		
		idealSpeed = -1.0f;
		blendingRatio = 10.0f; 
		approachLookat = 0;

		rotSpeed_min = 0.0f;
		rotSpeed_max = 0.6f;
		
		speed_min = 0.0f;

		forceView = 1.0f;

		fullAnimationTentaclesBlendMult = 10.0f;

		tentaclesCollide = 0;

		strcpy(tentaclesMaterial,"mat_alien_flesh");
		tentaclesRadius = 0.1f;
		tentaclesJointLimit = 10.0f;

		cameraShakeRange = 90.0f;
		cameraShakeMultiplier = 1.0f;

    groundEffectHeightScale = 1.f;
    groundEffectBaseScale = 1.f;
    groundEffectMaxSpeed = 0.f;

    trailEffectMinSpeed = 0.f;
    trailEffectMaxSpeedSize = 0.f;
    trailEffectMaxSpeedCount = 0.f;
		trailEffectDir.Set(0,1,0);

		healthTrailEffectMaxSize = 0.f;
		healthTrailEffectMaxCount = 0.f;
		healthTrailEffectDir.Set(0,1,0);

    turnSoundMaxVel = 0.f;
    turnSoundBoneId = -1;
	}

	void Serialize( TSerialize ser );
};

struct SSearchBeamStats
{
  SSearchBeamStats()  
  {
    pAttachment = NULL;
    itemId = 0;
    goalQuat.SetIdentity();
    active = false;
  }
  
  IAttachment* pAttachment;
  EntityId itemId;
  Quat goalQuat;
  bool active;
};

class CAlien;

class CAlienBeam
{
public:

	CAlienBeam(CAlien *pAlien) : m_pAlien(pAlien), m_active(false), m_effectSlot(0), m_beamTargetId(0), m_lCenter(0,0,0), m_followBoneID(-1)
	{}
	
	//
	void Start(const char *effect,EntityId targetId,Ang3 rotOffset = Ang3(0,0,gf_PI),const char *attachToBone = NULL);
	void Stop();
	void Update(float frameTime);
	ILINE bool IsActive(){return m_active;}

	void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

protected:

	CAlien *m_pAlien;

	bool m_active;

	int m_effectSlot;

	int m_followBoneID;

	EntityId m_beamTargetId;

	Vec3 m_lCenter;
};


/**
 * Prepares solid (non-particle based) debris pieces for spawning (basically
 * loads their geometries from disk) and spawns them in the OnKillEvent() call.
 */
class CDebrisSpawner 
{
public:

	CDebrisSpawner();
	~CDebrisSpawner();

	bool Init(CAlien* /*, const SmartScriptTable & */);
	void Reset();
	void Release() { delete this; }
	void Serialize(TSerialize ser);
	void Update(const float deltaTime);
	/// To be called once, when the Alien dies.
	void OnKillEvent();

	void GetMemoryStatistics(ICrySizer * s);

private:
	CAlien * m_pAlien;
	std::vector <EntityId> m_debrisParts;
};


class CAlien :
	public CActor,
	public IActionListener
{
public:
	struct SMovementRequestParams
	{
		bool	aimLook;
		int		bodystate;
		Vec3	vShootTargetPos;
		Vec3	vAimTargetPos;
		Vec3	vLookTargetPos;
		Vec3	vMoveDir;
		float	fDesiredSpeed;
		EActorTargetPhase	eActorTargetPhase;
		bool	bExactPositioning;
		PATHPOINTVECTOR	remainingPath;
		float	fDistanceToPathEnd;

		/// Initializes SMovementRequestParams from CMovementRequest.
		explicit SMovementRequestParams (CMovementRequest& );
	};

	/// SAIBodyInfo was previously used in place of this struct.
	struct SBodyInfo {
		Vec3		vEyePos;		
		Vec3		vEyeDir;
		Vec3		vEyeDirAnim;
		Vec3		vFwdDir;
		Vec3		vUpDir;
		Vec3		vFireDir;
		Vec3		vFirePos;
		float		maxSpeed;
		float		normalSpeed;
		float		minSpeed;
		EStance		stance;
		AABB		m_stanceSizeAABB;	// approximate local bounds of the stance.
		AABB		m_colliderSizeAABB;	// approximate local bounds of the stance collider only.

		SBodyInfo() : vEyePos(ZERO), vEyeDir(ZERO), vEyeDirAnim(ZERO),
			vFwdDir(ZERO), vUpDir(ZERO), vFireDir(ZERO),
			maxSpeed(0), normalSpeed(0), minSpeed(0),
			stance(STANCE_NULL)
		{
			m_stanceSizeAABB.min	=Vec3(ZERO);
			m_stanceSizeAABB.max	=Vec3(ZERO);
			m_colliderSizeAABB.min=Vec3(ZERO);
			m_colliderSizeAABB.max=Vec3(ZERO);
		}
	};


	CAlien();
	virtual ~CAlien();

	// CActor
	virtual void ProcessEvent(SEntityEvent& event);
	virtual bool CreateCodeEvent(SmartScriptTable &rTable);

	virtual bool Init( IGameObject * pGameObject );
	virtual void Update(SEntityUpdateContext&, int updateSlot);
	virtual void PrePhysicsUpdate();
	virtual void UpdateView(SViewParams &viewParams);

	virtual void Kill();
	virtual void Revive(bool fromInit = false);
	virtual void RagDollize( bool fallAndPlay );
	virtual void BindInputs( IAnimationGraphState * pAGState );
  virtual void Reset(bool toGame);

	virtual void OnAction(const ActionId& actionId, int activationMode, float value);
	virtual void FullSerialize( TSerialize ser );
	virtual void PostSerialize();
	virtual void SerializeXML( XmlNodeRef& node, bool bLoading );
	virtual void SetAuthority( bool auth );
	//AI specific
	virtual void SetActorMovement(SMovementRequestParams &control);
	virtual void GetActorInfo( SBodyInfo& bodyInfo );
	//retrieve actor status
	virtual SActorStats *GetActorStats() { return &m_stats; };
	virtual const SActorStats *GetActorStats() const { return &m_stats; };
	virtual SActorParams *GetActorParams() { return &m_params; };
	virtual void SetStats(SmartScriptTable &rTable);
	virtual void UpdateScriptStats(SmartScriptTable &rTable);
	//set actor params
	virtual void SetParams(SmartScriptTable &rTable,bool resetFirst);
	virtual void Physicalize(EStance stance=STANCE_STAND);
	virtual void PostPhysicalize();
	virtual void SetMovementTarget(const Vec3 &position,const Vec3 &looktarget,const Vec3 &up,float speed) {m_input.posTarget = position; m_input.dirTarget = looktarget; m_input.upTarget=up;m_input.speedTarget=speed;}
	virtual void SetAngles(const Ang3 &angles);
	virtual Ang3 GetAngles();
	virtual void StanceChanged(EStance last);
//	virtual void SetRotation(const Quat &rot,int flags=0);
	// ~CActor

	virtual void ProcessRotation(float frameTime);
	virtual void ProcessMovement(float frameTime);
	virtual void ProcessMovement2(float frameTime);
	virtual void ProcessSwimming(float frameTime);
	
	virtual void ProcessAnimation(ICharacterInstance *pCharacter,float frameTime);
	virtual void ProcessBonesRotation(ICharacterInstance *pCharacter,float frameTime);

	//
	virtual void SetDesiredSpeed(const Vec3 &desiredSpeed);
	virtual void SetDesiredDirection(const Vec3 &desiredDir);

	virtual void ResetAnimations();

	//stances
	virtual void	SetActorStance(SMovementRequestParams &control, int& actions);
	//

	//misc
	virtual void UpdateStats(float frameTime);
	virtual void UpdateFiringDir(float frameTime){m_stats.fireDir = m_stats.fireDirGoal;}

	virtual void Draw(bool draw);
	
	virtual bool IsFlying(){return true;}

	void SetTentacles(ICharacterInstance *pCharacter,float animStiffness,float mass = 0,float damping = 0,bool bRagdolize = false);
	void PushCharacterTentacles(ICharacterInstance *pCharacter);

	void DetachTentacle(ICharacterInstance *pCharacter,const char *tentacle);

	virtual void SetFiring(bool fire);

	virtual IActorMovementController * CreateMovementController();

	virtual void UpdateFootSteps(float frameTime) {};

	virtual bool IsAlien() {return true;}
	
	static  const char* GetActorClassType() { return "CAlien"; }
	virtual const char* GetActorClass() const { return CAlien::GetActorClassType(); }

	ILINE const Vec3& GetWeaponOffset() const { return m_weaponOffset; }
	ILINE const Vec3& GetEyeOffset() const { return m_eyeOffset; }

  void SetSearchBeamGoal(const Vec3& dir);
  Quat GetSearchBeamQuat() const;
  void SetSearchBeamQuat(const Quat& rot);
  void UpdateSearchBeam(float frameTime);

	void GetMemoryStatistics(ICrySizer * s) 
	{ 
		s->Add(*this);
		GetAlienMemoryStatistics(s); 
	}

	virtual void SetAnimTentacleParams(pe_params_rope& rope, float animBlend);

	//Player can grab naked aliens
	virtual int	 GetActorSpecies() { return eGCT_ALIEN; }

protected:
  virtual void OnCloaked(bool cloaked) { m_stats.cloaked = cloaked; }

	void GetAlienMemoryStatistics(ICrySizer * s);
	void GetMovementVector(Vec3& move, float& speed, float& maxSpeed);
  void SetActorMovementCommon(SMovementRequestParams& control);
	virtual void UpdateAnimGraph( IAnimationGraphState * pState );
    
	IItemSystem	*m_pItemSystem;
	
	Quat		m_modelQuat;//the model rotation
	Vec3		m_modelOffset;
	Vec3		m_modelOffsetAdd;
	Vec3		m_weaponOffset;
	Vec3		m_eyeOffset;

	Matrix33	m_eyeMtx;//view matrix

	Matrix33	m_viewMtx;//view matrix
	Matrix33	m_baseMtx;//base rotation matrix, rotating on the Z axis

	Matrix34	m_charLocalMtx;

	Vec3		m_velocity;
	Vec3		m_desiredVelocity;
	Quat		m_desiredVeloctyQuat;

	float		m_curSpeed;
	float		m_turnSpeed;
	float		m_turnSpeedGoal;

	int			m_requestedStance;

	SCharacterMoveRequest m_moveRequest;

	//zeroG specific	
	Ang3		m_angularVel;
	//

	SAlienInput m_input;
	SAlienStats	m_stats;
	SAlienParams m_params;

	//animation Specific
	float	m_tentacleBlendRatio;

	bool	m_forceOrient;

	bool m_isFiring;//very bad, to be removed after M4

	float	m_endOfThePathTime;

	std::vector<IPhysicalEntity *> m_tentaclesProxy;
	std::vector<IPhysicalEntity *> m_tentaclesProxyFullAnimation;

	IAnimationGraph::InputID m_inputSpeed;
	IAnimationGraph::InputID m_inputDesiredSpeed;
	IAnimationGraph::InputID m_inputAiming;

	//misc
	float m_followEyesTime;
	float	m_roll;

	//effects and such
	IGroundEffect *m_pGroundEffect;  
  IAttachment* m_pTrailAttachment;
	IAttachment* m_pHealthTrailAttachment;
  float m_trailSpeedScale;
	float m_healthTrailScale;
  SSearchBeamStats m_searchbeam;
  _smart_ptr<ISound> m_pTurnSound;
  
	CAlienBeam *m_pBeamEffect;
	Vec3				m_oldGravity;

	void UpdateDebugGraphs();
	IDebugHistoryManager* m_pDebugHistoryManager;
public:
	void DebugGraph_AddValue(const char* id, float value) const;

};


#endif //__ALIEN_H__
