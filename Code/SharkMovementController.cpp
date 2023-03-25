// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "SharkMovementController.h"
#include "Shark.h"

CSharkMovementController::CSharkMovementController( CShark * pShark ) : m_pShark(pShark), m_atTarget(false)
{
}

void CSharkMovementController::Reset()
{
}

bool CSharkMovementController::Update( float frameTime, SActorFrameMovementParams& params )
{
	UpdateCurMovementState( params );
	return false;
}

void CSharkMovementController::Release()
{
	delete this;
}

bool CSharkMovementController::RequestMovement( CMovementRequest& request )
{
	SMovementState state;
	GetMovementState(state);

	Vec3 currentEyePos = state.eyePosition;
	Vec3 currentPos = m_pShark->GetEntity()->GetWorldPos();
	Vec3 currentForw = m_pShark->GetEntity()->GetWorldRotation() * FORWARD_DIRECTION;

	CShark::SMovementRequestParams os (request);

	if (request.HasMoveTarget())
		os.vMoveDir = (request.GetMoveTarget() - currentPos).GetNormalizedSafe(FORWARD_DIRECTION);


	if (request.HasForcedNavigation())
	{
		os.vMoveDir = request.GetForcedNavigation();
		os.fDesiredSpeed = os.vMoveDir.GetLength();
		os.vMoveDir.NormalizeSafe();
	}



	m_pShark->SetActorMovement(os);

	if (request.HasFireTarget())
		m_currentMovementRequest.SetFireTarget( request.GetFireTarget() );
	else if (request.RemoveFireTarget())
		m_currentMovementRequest.ClearFireTarget();

	if (request.HasAimTarget())
		m_currentMovementRequest.SetAimTarget( request.GetAimTarget() );
	else if (request.RemoveAimTarget())
		m_currentMovementRequest.ClearAimTarget();

	return true;
}

void CSharkMovementController::UpdateCurMovementState(const SActorFrameMovementParams& params)
{
	SMovementState& state(m_currentMovementState);
	CShark::SBodyInfo bodyInfo;
	m_pShark->GetActorInfo( bodyInfo );
	//state.maxSpeed = bodyInfo.maxSpeed;
	//state.minSpeed = bodyInfo.minSpeed;
	//state.normalSpeed = bodyInfo.normalSpeed;
	state.stance = bodyInfo.stance;
	state.m_StanceSize		= bodyInfo.m_stanceSizeAABB;
	state.m_ColliderSize	= bodyInfo.m_colliderSizeAABB;
	state.eyeDirection = bodyInfo.vEyeDir;
	state.animationEyeDirection = bodyInfo.vEyeDirAnim;
	state.eyePosition = bodyInfo.vEyePos;
	state.weaponPosition = bodyInfo.vFirePos;
	state.movementDirection = bodyInfo.vFwdDir;
	state.upDirection = bodyInfo.vUpDir;
	state.atMoveTarget = m_atTarget;

	/*	if (IItem * pItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem( itemEntity ))
	if (const IWeapon * pWeapon = pItem->GetIWeapon())
	state.weaponPosition = pWeapon->GetFiringPos(Vec3(0,0,0));*/

	state.aimDirection = bodyInfo.vFireDir.GetNormalizedSafe();

	state.fireDirection = state.aimDirection;

	state.isAlive = (m_pShark->GetHealth()>0);


	//---------------------------------------------
	state.isAiming = false;

	state.isFiring = false;

}

bool CSharkMovementController::GetStanceState(EStance stance, float lean, bool defaultPose, SStanceState& state)
{
	const SStanceInfo*	pStance = m_pShark->GetStanceInfo(stance);
	if(!pStance)
		return false;

	if(defaultPose)
	{
		state.pos.Set(0,0,0);
		state.bodyDirection = FORWARD_DIRECTION;
		state.upDirection(0,0,1);
		state.weaponPosition = m_pShark->GetWeaponOffsetWithLean(stance, lean, m_pShark->GetEyeOffset());
		state.aimDirection = FORWARD_DIRECTION;
		state.fireDirection = FORWARD_DIRECTION;
		state.eyePosition = pStance->GetViewOffsetWithLean(lean);
		state.eyeDirection = FORWARD_DIRECTION;
		state.m_StanceSize = pStance->GetStanceBounds();
		state.m_ColliderSize = pStance->GetColliderBounds();
	}
	else
	{
		// TODO: the directions are like not to match. Is the AI even using them?
		CShark::SBodyInfo bodyInfo;
		m_pShark->GetActorInfo( bodyInfo );

		Matrix34	tm = m_pShark->GetEntity()->GetWorldTM();

		state.pos = m_pShark->GetEntity()->GetWorldPos();
		state.bodyDirection = bodyInfo.vFwdDir;
		state.upDirection = bodyInfo.vUpDir;
		state.weaponPosition = tm.TransformPoint(m_pShark->GetWeaponOffsetWithLean(stance, lean, m_pShark->GetEyeOffset()));
		state.aimDirection = bodyInfo.vFireDir;
		state.fireDirection = bodyInfo.vFireDir;
		state.eyePosition = tm.TransformPoint(pStance->GetViewOffsetWithLean(lean));
		state.eyeDirection = bodyInfo.vEyeDir;
		state.m_StanceSize = pStance->GetStanceBounds();
		state.m_ColliderSize = pStance->GetColliderBounds();
	}

	return true;
}