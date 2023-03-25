/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements the shark.

-------------------------------------------------------------------------
History:
- 12:08:2007: Created by Luciano Morpurgo

*************************************************************************/
#ifndef __SHARK_H__
#define __SHARK_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Actor.h"


//this might change
struct SSharkStats : public SActorStats
{
	float turnRadius;
//	void Serialize( TSerialize ser );
	SSharkStats()
	{
		memset(this,0,sizeof(SSharkStats));
	}
};

struct SSharkParams : public SActorParams
{
/*	string trailEffect;
	float trailEffectMinSpeed;
	float trailEffectMaxSpeedSize;
	float trailEffectMaxSpeedCount;
	Vec3	trailEffectDir;
*/
	float speedInertia;
	float rollAmount;
	float rollSpeed;

	float minTurnRadius;

	float sprintMultiplier;
	float sprintDuration;
	float accel;
	float decel;
	float rotSpeed_min;
	float rotSpeed_max;

	float speed_min;

	float minDistanceCircle;
	float maxDistanceCircle;
	int		numCircles;
	float minDistForUpdatingMoveTarget;
	string meleeAnimation;
	float meleeDistance;
	float circlingTime;
	float attackRollTime;
	float attackRollAngle;
	int		escapeAnchorType;
	bool	bSpawned;

	string headBoneName;
	string spineBoneName1;
	string spineBoneName2;
	SSharkParams()
	{
		accel = decel = 1.0f;
		meleeAnimation = "";
		spineBoneName2 = "";
		spineBoneName1 = "";
		headBoneName = "";
/*		trailEffectMinSpeed = 0;
		trailEffectMaxSpeedSize = 0;
		trailEffectMaxSpeedCount = 0;
		trailEffectDir.zero();
		trailEffect = "";
*/
		speedInertia = 0;
		rollAmount = 0;
		rollSpeed = 0;

		minTurnRadius = 0;

		sprintMultiplier = 0;
		sprintDuration = 0;
		rotSpeed_min = 0;
		rotSpeed_max = 1;

		speed_min = 0.1f;

		minDistanceCircle = 0;
		maxDistanceCircle = 0;
		numCircles = 0;
		minDistForUpdatingMoveTarget = 0;
		
		meleeDistance = 0;
		circlingTime = 0;
		attackRollTime = 0;
		attackRollAngle = 0;
		escapeAnchorType = 0;
		bSpawned = false;
	}

	void Serialize( TSerialize ser );
};


struct SSharkInput
{
	Vec3 deltaMovement;//desired movement change: X = side movement, Y = forward, Z = up
	Vec3 deltaRotation;//desired rotation change, X = pitch, Z = yaw, Y will probably be the lean

	int actions;

	//misc
	Vec3 movementVector;//direct movement vector, it will be capped between 0-1 length, used only by AI for now
	Vec3 viewVector;//if len != 0 use as view direction (no roll atm)

	Vec3 viewDir; // requested view direction
	float pathLength; // remaining path length
/*
	Vec3 posTarget;
	Vec3 dirTarget;
	Vec3 upTarget;
	float	speedTarget;
*/
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

	SSharkInput()
	{
		memset(this,0,sizeof(SSharkInput));
		//speedTarget = 1.0f;
	}

	void Serialize( TSerialize ser );
};


class CShark :
	public CActor
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

	typedef enum 
	{
		S_Sleeping,
		S_Reaching,
		S_Circling,
		S_Attack,
		S_FinalAttackAlign,
		S_FinalAttack,
		S_PrepareEscape,
		S_Escaping,
		S_Spawning,
		S_Last = S_Escaping
	} eSharkStatus;

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


	CShark();
	virtual ~CShark();


	virtual bool Init( IGameObject * pGameObject );
	virtual void Update(SEntityUpdateContext&, int updateSlot);
	virtual void PrePhysicsUpdate();
	virtual void Kill();
	virtual void Revive(bool fromInit = false);
	virtual void RagDollize( bool fallAndPlay );
	virtual void BindInputs( IAnimationGraphState * pAGState );
	virtual void Reset(bool toGame);
	virtual void ProcessEvent(SEntityEvent& event);
	virtual void OnAction(const ActionId& actionId, int activationMode, float value);
	virtual void FullSerialize( TSerialize ser );
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
	virtual void PostPhysicalize();
	virtual void SetMovementTarget(const Vec3 &position,const Vec3 &looktarget,const Vec3 &up,float speed) {};//{m_input.posTarget = position; m_input.dirTarget = looktarget; m_input.upTarget=up;m_input.speedTarget=speed;}
	virtual void SetAngles(const Ang3 &angles);
	virtual Ang3 GetAngles();
	//virtual void StanceChanged(EStance last);
	// ~CActor

	//virtual void ProcessRotation(float frameTime);
	virtual void ProcessMovement(float frameTime);

	virtual void ProcessAnimation(ICharacterInstance *pCharacter,float frameTime);
//	virtual void ProcessBonesRotation(ICharacterInstance *pCharacter,float frameTime);
	virtual void ProcessRotation(float frameTime);

	//
	virtual void SetDesiredSpeed(const Vec3 &desiredSpeed);
	virtual void SetDesiredDirection(const Vec3 &desiredDir);

	virtual void ResetAnimations();

	//stances
	//virtual void	SetActorStance(SMovementRequestParams &control, int& actions);
	//

	//misc
	virtual void UpdateStats(float frameTime);
	//virtual void UpdateFiringDir(float frameTime){m_stats.fireDir = m_stats.fireDirGoal;}

	virtual void Draw(bool draw);

	//virtual bool IsFlying(){return true;}

	//void SetTentacles(ICharacterInstance *pCharacter,float animStiffness,float mass = 0,float damping = 0,bool bRagdolize = false);
	//void PushCharacterTentacles(ICharacterInstance *pCharacter);

	//void DetachTentacle(ICharacterInstance *pCharacter,const char *tentacle);

	//virtual void SetFiring(bool fire);

	virtual IActorMovementController * CreateMovementController();

	virtual void UpdateFootSteps(float frameTime) {};

	static  const char* GetActorClassType() { return "CShark"; }
	virtual const char* GetActorClass() const { return CShark::GetActorClassType(); }

	ILINE const Vec3& GetWeaponOffset() const { return m_weaponOffset; }
	ILINE const Vec3& GetEyeOffset() const { return m_eyeOffset; }

	//void SetSearchBeamGoal(const Vec3& dir);
	//Quat GetSearchBeamQuat() const;
	//void SetSearchBeamQuat(const Quat& rot);
	//void UpdateSearchBeam(float frameTime);

	void GetMemoryStatistics(ICrySizer * s) 
	{ 
		s->Add(*this);
		//GetAlienMemoryStatistics(s); 
	}

	//virtual void SetAnimTentacleParams(pe_params_rope& rope, float animBlend);

	//Player can't grab sharks
	virtual int	 GetActorSpecies() { return eGCT_UNKNOWN; }

	//virtual bool IsCloaked() const { return m_stats.cloaked; }
	virtual void	SetStance(EStance stance);

protected:
	
	void GetMovementVector(Vec3& move, float& speed, float& maxSpeed);
	void SetActorMovementCommon(SMovementRequestParams& control);
	virtual void UpdateAnimGraph( IAnimationGraphState * pState );
	void SetMoveTarget(Vec3 moveTarget, bool bForced = false, float minDistForUpdate = 0);
	void AdjustMoveTarget(float maxHeight, const IEntity* pTargetEntity);

	void UpdateCollider(float frameTime, const IEntity* pTarget);
	void UpdateStatus(float frameTime, const IEntity* pTarget);
	void Attack(bool fast=false);
	void GoAway();
	void SetStartPos(const Vec3& targetPos);
	void MeleeAnimation();
	void FindEscapePoints();
	void SetReaching(const Vec3& targetPos);
	float GetDistHeadTarget(const Vec3& targetPos, const Vec3& targetDirN,float& dotMouth);
	void ResetValues();

	typedef std::vector<Vec3> TPointList;

	Quat		m_modelQuat;//the model rotation
	Quat		m_modelAddQuat;
	Vec3		m_modelOffset;
	//Vec3		m_modelOffsetAdd;
	Vec3		m_weaponOffset;
	Vec3		m_eyeOffset;
	Vec3		m_circleDisplacement;
	bool		m_bCircularTrajectory;
	float		m_turnRadius;
	float		m_rollTime;
	float		m_roll;
	float		m_rollMaxAngle;
	Vec3		m_chosenEscapeDir;
	Vec3		m_escapeDir;
	int			m_tryCount;
	Vec3		m_lastCheckedPos;
	float		m_AABBMaxZCache;
	TPointList m_EscapePoints;

	EntityId m_targetId;

	Matrix33	m_eyeMtx;//view matrix

	Matrix33	m_viewMtx;//view matrix
	Matrix33	m_baseMtx;//base rotation matrix, rotating on the Z axis

	Vec3		m_velocity;

	Vec3		m_debugcenter;
	float		m_debugradius;

	float		m_curSpeed;
	float		m_turnSpeed;
	float		m_turnSpeedGoal;
	Ang3		m_angularVel;
	Vec3		m_lastPos;
	Vec3		m_lastRot;
	Vec3		m_startPos;

	float		m_headOffset;//optimized usage of viewoffset (to get the nose position)

	Matrix34	m_charLocalMtx;

	//const char* m_idleAnimationName;
	float m_turnAnimBlend;
	float m_circleRadius;

	SCharacterMoveRequest m_moveRequest;

//	IAttachment* m_pTrailAttachment;
//	float m_trailSpeedScale;

	SSharkStats	m_stats;
	SSharkParams m_params;
	SSharkInput m_input;

	
	eSharkStatus m_state;
	Vec3 m_moveTarget;
	int		m_numHalfCircles;
	float m_remainingCirclingTime;
	float m_remainingAttackTime;
	float m_animationSpeed;
	float m_animationSpeedAttack;
	Vec3 m_lastSpawnPoint;

	IAnimationGraph::InputID m_inputSpeed;
	IAnimationGraph::InputID m_idSignalInput;

};


#endif //__ALIEN_H__
