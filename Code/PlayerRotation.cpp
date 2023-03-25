// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "PlayerRotation.h"
#include "GameUtils.h"
#include "GameCVars.h"
#include "Item.h"

#define ENABLE_NAN_CHECK

#ifdef ENABLE_NAN_CHECK
#define CHECKQNAN_FLT(x) \
	assert(((*(unsigned*)&(x))&0xff000000) != 0xff000000u && (*(unsigned*)&(x) != 0x7fc00000))
#else
#define CHECKQNAN_FLT(x) (void*)0
#endif

#define CHECKQNAN_VEC(v) \
	CHECKQNAN_FLT(v.x); CHECKQNAN_FLT(v.y); CHECKQNAN_FLT(v.z)

#define CHECKQNAN_MAT33(v) \
	CHECKQNAN_VEC(v.GetColumn(0)); \
	CHECKQNAN_VEC(v.GetColumn(1)); \
	CHECKQNAN_VEC(v.GetColumn(2))

#define CHECKQNAN_QUAT(q) \
	CHECKQNAN_VEC(q.v); \
	CHECKQNAN_FLT(q.w)

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
CPlayerRotation::CPlayerRotation( const CPlayer& player, const SActorFrameMovementParams& movement, float m_frameTime ) : 
	m_frameTime(m_frameTime),
	m_params(player.m_params),
	m_stats(player.m_stats),
	m_viewQuat(player.m_viewQuat),
	m_baseQuat(player.m_baseQuat),
	m_viewQuatFinal(player.m_viewQuatFinal),
	m_baseQuatLinked(player.m_linkStats.baseQuatLinked),
	m_viewQuatLinked(player.m_linkStats.viewQuatLinked),
	m_player(player),
	m_viewRoll(player.m_viewRoll),
	m_upVector(player.m_upVector),
	m_viewAnglesOffset(player.m_viewAnglesOffset),
	m_leanAmount(player.m_stats.leanAmount),
	m_actions(player.m_actions),	  
	m_absRoll(0.0f),
	m_angularImpulseTime(m_stats.angularImpulseTime),
	m_angularImpulse(m_stats.angularImpulse),
	m_angularImpulseDelta(m_stats.angularImpulse),
	m_angularVel(m_stats.angularVel),
	m_desiredLeanAmount(movement.desiredLean)
{
	m_deltaAngles = movement.deltaAngles;
	CHECKQNAN_FLT(m_deltaAngles);
}

void CPlayerRotation::Process()
{
	//FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	//
	float forceLookLen2(m_player.m_stats.forceLookVector.len2());
	if (forceLookLen2>0.001f)
	{
		float forceLookLen(cry_sqrtf(forceLookLen2));
		Vec3 forceLook(m_player.m_stats.forceLookVector);
		forceLook *= 1.0f/forceLookLen;
		forceLook = m_player.m_viewQuatFinal.GetInverted() * forceLook;

		float smoothSpeed(6.6f * forceLookLen);
		m_deltaAngles.x += asin(forceLook.z) * min(1.0f,m_frameTime*smoothSpeed);
		m_deltaAngles.z += cry_atan2f(-forceLook.x,forceLook.y) * min(1.0f,m_frameTime*smoothSpeed);

		CHECKQNAN_VEC(m_deltaAngles);
	}

	ProcessAngularImpulses();

	ProcessLean();
	
	if (m_stats.inAir && m_stats.inZeroG)
		ProcessFlyingZeroG();
	else if (m_stats.inFreefall.Value()==1)
		ProcessFreeFall();
	else if (m_stats.inFreefall.Value()==2)
		ProcessParachute();
	else
	{
		ProcessNormalRoll();
		ClampAngles();
		ProcessNormal();
	}

	//CHECKQNAN_MAT33(m_viewMtx);

	//update freelook when linked to an entity
	SLinkStats *pLinkStats = &m_player.m_linkStats;
	if (pLinkStats->linkID)
	{
		IEntity *pLinked = pLinkStats->GetLinked();
		if (pLinked)
		{
			//at this point m_baseQuat and m_viewQuat contain the previous frame rotation, I'm using them to calculate the delta rotation.
			m_baseQuatLinked *= m_player.m_baseQuat.GetInverted() * m_baseQuat;
			m_viewQuatLinked *= m_player.m_viewQuat.GetInverted() * m_viewQuat;

			m_baseQuat = pLinked->GetRotation() * m_baseQuatLinked;
			m_viewQuat = pLinked->GetRotation() * m_viewQuatLinked;
		}
	}
	//

	m_viewQuatFinal = m_viewQuat * Quat::CreateRotationXYZ(m_player.m_viewAnglesOffset);
}

void CPlayerRotation::Commit( CPlayer& player )
{
	player.m_baseQuat = m_baseQuat.GetNormalized();
	player.m_viewQuat = m_viewQuat.GetNormalized();

	CHECKQNAN_QUAT(m_baseQuat);
	CHECKQNAN_QUAT(m_viewQuat);
	if (!player.IsTimeDemo())
	{
		//CHECKQNAN_MAT33(m_baseMtx);
		//CHECKQNAN_MAT33(m_viewMtx);
		player.m_viewQuatFinal = m_viewQuatFinal.GetNormalized();
	}

	player.m_linkStats.baseQuatLinked = m_baseQuatLinked.GetNormalized();
	player.m_linkStats.viewQuatLinked = m_viewQuatLinked.GetNormalized();
	
	player.m_viewRoll = m_viewRoll;
	player.m_upVector = m_upVector;
	player.m_viewAnglesOffset = m_viewAnglesOffset;
	player.m_stats.leanAmount = m_leanAmount;	
	player.m_stats.angularImpulseTime = m_angularImpulseTime;
	player.m_stats.angularImpulse = m_angularImpulse;
	player.m_stats.angularVel = m_angularVel;

	player.m_stats.forceLookVector.zero();

	if (m_absRoll > 0.01f)
		player.CreateScriptEvent("thrusters",m_absRoll);
}

void CPlayerRotation::GetStanceAngleLimits(float & minAngle,float & maxAngle)
{
	EStance stance = m_player.GetStance();

	switch(stance)
	{
	default:
	case STANCE_CROUCH:
	case STANCE_STAND:
		minAngle = -80.0f;
		maxAngle = 80.0f;
		break;
	case STANCE_PRONE:
		minAngle = -35.0f;
		maxAngle = 45.0f;
		break;
	}

	//Limit camera rotation on ladders(to prevent clipping)
	if(m_player.m_stats.isOnLadder)
	{
		minAngle = -40.0f;
		maxAngle = 80.0f;
	}
	if(m_stats.grabbedHeavyEntity!=0)
	{
		minAngle = -35.0f;  //Limit angle to prevent clipping, throw objects at feet, etc...
	}

	// SNH: additional restriction based on weapon type if prone.
	if(m_player.GetStance() == STANCE_PRONE && g_pGameCVars->g_proneAimAngleRestrict_Enable != 0)
	{
		float dist = 0.0f;
		CItem* pItem = (CItem*)(m_player.GetCurrentItem());
		if(pItem)
			dist = pItem->GetParams().raise_distance;

		SMovementState movestate;
		m_player.m_pMovementController->GetMovementState(movestate);
	
		// try a cylinder intersection test
		IPhysicalEntity* pIgnore[2];
		pIgnore[0] = m_player.GetEntity()->GetPhysics();
		pIgnore[1] = pItem ? pItem->GetEntity()->GetPhysics() : NULL;

		primitives::cylinder cyl;
		cyl.r = 0.05f;
		cyl.axis = movestate.aimDirection;
		cyl.hh = dist;
		cyl.center = movestate.weaponPosition + movestate.aimDirection*cyl.hh;

		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawCylinder(cyl.center, cyl.axis, cyl.r, cyl.hh, ColorF(0.4f,1.0f,0.6f, 0.2f));

		float n = 0.0f;
		geom_contact *contacts;
		intersection_params params;
		params.bStopAtFirstTri = false;
		params.bSweepTest = false;
		params.bNoBorder = true;
		params.bNoAreaContacts = true;
		n = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(primitives::cylinder::type, &cyl, Vec3(0,0,2), 
			ent_static|ent_terrain, &contacts, 0,
			geom_colltype_player, &params, 0, 0, pIgnore, pIgnore[1]?2:1);
		int ret = (int)n;
		
		geom_contact *currentc = contacts;
		for (int i=0; i<ret; i++)
		{
			geom_contact *contact = currentc;
			if (contact && (fabs_tpl(contact->n.z)>0.2f))
			{
				Vec3 dir = contact->pt - movestate.weaponPosition;
				dir.NormalizeSafe();
				Vec3 horiz = dir;
				horiz.z = 0.0f;
				horiz.NormalizeSafe();
				float cosangle = dir.Dot(horiz);
				Limit(cosangle, -1.0f, 1.0f);
				float newMin = acos_tpl(cosangle);
				newMin = -newMin * 180.0f / gf_PI;
				//float col[] = {1,1,1,1};
				//gEnv->pRenderer->Draw2dLabel(100,100, 1.0f, col, false, "minangle: %.2f", newMin);
				//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(contact->pt, 0.03f, ColorF(1,0,0,1));
				minAngle = MAX(newMin, minAngle);
			}
			++currentc;
		}
	
		if (ret)
		{
			WriteLockCond lockColl(*params.plock, 0);
			lockColl.SetActive(1);
		}

	}

	minAngle *= gf_PI/180.0f;
	maxAngle *= gf_PI/180.0f;
}

void CPlayerRotation::ProcessFlyingZeroG()
{
	bool bEnableGyroVerticalFade = (g_pGameCVars->pl_zeroGEnableGyroFade > 0);
	bool bEnableGyroSpeedFade = (g_pGameCVars->pl_zeroGEnableGyroFade < 2);

	//thats necessary when passing from groundG to normalG
	m_baseQuat = m_viewQuat;
	//m_baseQuat = Quat::CreateSlerp(m_viewQuat,m_player.GetEntity()->GetRotation() * Quat::CreateRotationZ(gf_PI),0.5f);

	Ang3 desiredAngVel(m_deltaAngles.x,m_deltaAngles.y * 0.3f,m_deltaAngles.z);

	//view recoil in zeroG cause the player to rotate
	desiredAngVel.x += m_viewAnglesOffset.x * 0.1f;
	desiredAngVel.z += m_viewAnglesOffset.z * 0.1f;

	//so once used reset it.
	m_viewAnglesOffset.Set(0,0,0);

	//gyroscope: the gyroscope just apply the right roll speed to compensate the rotation, that way effects like
	//propulsion particles and such can be done easily just by using the angularVel
	float rotInertia(g_pGameCVars->pl_zeroGAimResponsiveness);

	if (m_player.GravityBootsOn() && m_stats.gBootsSpotNormal.len2()>0.01f)
	{
		Vec3 vRef(m_baseQuat.GetInverted() * m_stats.gBootsSpotNormal);
		Ang3 alignAngle(0,0,0);
		alignAngle.y = cry_atan2f(vRef.x, vRef.z);
		alignAngle.x = cry_atan2f(vRef.y, vRef.z);

		desiredAngVel.y += alignAngle.y * 0.05f;
		desiredAngVel.x -= alignAngle.x * 0.05f;
	}

	if (m_actions & ACTION_GYROSCOPE && desiredAngVel.y==0)
	{
		// we want to fade out the gyroscopic effect 
		Vec3 vRef(m_baseQuat.GetInverted() * m_stats.zeroGUp);
		Ang3 alignAngle(0,0,0);
		alignAngle.y = cry_atan2f(vRef.x,vRef.z);

		float gyroFade = 1.0f;
		if (bEnableGyroVerticalFade)
		{
			float gyroFadeAngleInner = g_pGameCVars->pl_zeroGGyroFadeAngleInner;
			float gyroFadeAngleOuter = g_pGameCVars->pl_zeroGGyroFadeAngleOuter;
			float gyroFadeAngleSpan = gyroFadeAngleOuter - gyroFadeAngleInner;
			float gyroFadeAngleSpanInv = 1.0f / gyroFadeAngleSpan;
			float viewVerticalAlignment = abs(m_viewQuat.GetFwdZ());
			float viewVerticalAngle = RAD2DEG(cry_asinf(viewVerticalAlignment));
			gyroFade = 1.0f - CLAMP((viewVerticalAngle - gyroFadeAngleInner) * gyroFadeAngleSpanInv, 0.0, 1.0f);
			gyroFade = cry_powf(gyroFade, g_pGameCVars->pl_zeroGGyroFadeExp);
		}

		float speedFade = 1.0f;
		if (bEnableGyroSpeedFade)
		{
			float speed = m_player.GetLastRequestedVelocity().GetLength();
			speedFade = 1.0f - std::min(1.0f, speed / 5.0f);
		}

		desiredAngVel.y += alignAngle.y * speedFade * gyroFade * m_frameTime * g_pGameCVars->pl_zeroGGyroStrength;

		//rotInertia = 3.0f;
	}

	m_absRoll = fabs(desiredAngVel.y);

	Interpolate(m_angularVel,desiredAngVel,rotInertia,m_frameTime);
	Ang3 finalAngle(m_angularVel + m_angularImpulseDelta);

	m_baseQuat *= Quat::CreateRotationZ(finalAngle.z) * Quat::CreateRotationX(finalAngle.x) * Quat::CreateRotationY(finalAngle.y);
	m_baseQuat.Normalize();

	/*IEntity *pEnt = m_player.GetEntity();
	Vec3 offsetToCenter(Vec3(0,0,m_player.GetStanceInfo(m_player.GetStance())->heightCollider));
	Vec3 finalPos(pEnt->GetWorldTM() * offsetToCenter);
	Quat newBaseQuat(m_baseQuat * Quat::CreateRotationZ(finalAngle.z) * Quat::CreateRotationX(finalAngle.x) * Quat::CreateRotationY(finalAngle.y));
	Vec3 newPos(pEnt->GetWorldPos() + m_baseQuat * offsetToCenter);
	pEnt->SetPos(pEnt->GetWorldPos() + (finalPos - newPos),ENTITY_XFORM_USER);*/
	
	//CHECKQNAN_MAT33(m_baseMtx);

	m_viewQuat = m_baseQuat;
	m_viewRoll = 0;
	m_upVector = m_baseQuat.GetColumn2();
}

void CPlayerRotation::ProcessFreeFall()
{
	//thats necessary when passing from groundG to normalG
	//m_baseQuat = m_viewQuat;
	
	Ang3 desiredAngVel(m_deltaAngles.x,m_deltaAngles.y*0.5f/*+m_deltaAngles.z*0.5f*/,m_deltaAngles.z);

	float rotInertia(5.5f);

	//align to gravity vector
	Vec3 vRef(m_viewQuat.GetInverted() * Vec3(0,0,1));
	float alignAngleY = cry_atan2f(vRef.x,vRef.z);

	desiredAngVel.y += alignAngleY * 0.05f;

	Interpolate(m_angularVel,desiredAngVel,rotInertia,m_frameTime);
	Ang3 finalAngle(m_angularVel + m_angularImpulseDelta);

	//limit Z angle
	float viewPitch(GetLocalPitch());
	float zLimit(-1.3f);
	if (viewPitch + finalAngle.x < zLimit)
		finalAngle.x = zLimit - viewPitch;

	m_viewQuat *= Quat::CreateRotationZ(finalAngle.z) * Quat::CreateRotationX(finalAngle.x) * Quat::CreateRotationY(finalAngle.y);
	m_viewQuat.Normalize();

	Vec3 up(Vec3(0,0,1));
	Vec3 right(m_viewQuat.GetColumn0());
	Vec3 forward((up % right).GetNormalized());
	m_baseQuat = Quat(Matrix33::CreateFromVectors(forward % up,forward,up));

	//m_viewQuat = m_baseQuat;
	m_viewRoll = 0;
	m_upVector = m_baseQuat.GetColumn2();
}

void CPlayerRotation::ProcessParachute()
{
	//thats necessary when passing from groundG to normalG
	//m_baseQuat = m_viewQuat;
	
	Ang3 desiredAngVel(m_deltaAngles.x,m_deltaAngles.y*0.5f-m_deltaAngles.z*1.0f,m_deltaAngles.z);

	float rotInertia(7.7f);

	//align to gravity vector
	Vec3 vRef(m_viewQuat.GetInverted() * Vec3(0,0,1));
	float alignAngleY = cry_atan2f(vRef.x,vRef.z);

	desiredAngVel.y += alignAngleY * 0.05f;

	Interpolate(m_angularVel,desiredAngVel,rotInertia,m_frameTime);
	Ang3 finalAngle(m_angularVel + m_angularImpulseDelta);

	//limit Z angle
	float viewPitch(GetLocalPitch());
	float zLimit(-1.3f);
	if (viewPitch + finalAngle.x < zLimit)
		finalAngle.x = zLimit - viewPitch;

	m_viewQuat *= Quat::CreateRotationZ(finalAngle.z) * Quat::CreateRotationX(finalAngle.x) * Quat::CreateRotationY(finalAngle.y);
	m_viewQuat.Normalize();

	Vec3 up(Vec3(0,0,1));
	Vec3 right(m_viewQuat.GetColumn0());
	Vec3 forward((up % right).GetNormalized());
	m_baseQuat = Quat(Matrix33::CreateFromVectors(forward % up,forward,up));

	//m_viewQuat = m_baseQuat;
	m_viewRoll = 0;
	m_upVector = m_baseQuat.GetColumn2();
}

void CPlayerRotation::ProcessNormalRoll()
{
	//apply lean/roll
	float rollAngleGoal(0);
	float speed2(m_stats.velocity.len2());

	//add some leaning when strafing
	if (speed2 > 0.01f && m_stats.inAir)
	{
		Vec3 velVec(m_stats.velocity);
		float maxSpeed = m_player.GetStanceMaxSpeed(STANCE_STAND);
		if (maxSpeed > 0.01f)
			velVec /= maxSpeed;

		float dotSide(m_viewQuat.GetColumn0() * velVec);

		rollAngleGoal -= dotSide * 1.5f * gf_PI/180.0f;
	}

	float tempLean = m_leanAmount;
	if(m_player.IsPlayer() && !m_player.IsThirdPerson())
	{	
		m_leanAmount *= 0.3f;
	}

	// Scale down player leaning by 50% when moving in 3rd person.
	if (m_player.IsPlayer() && m_player.IsThirdPerson())
	{
		float movementFraction = min(1.0f, speed2 / sqr(2.0f));
		m_leanAmount *= 1.0f - movementFraction * 0.5f;
	}
		
	rollAngleGoal += m_leanAmount * m_params.leanAngle * gf_PI/180.0f;
	Interpolate(m_viewRoll,rollAngleGoal,9.9f,m_frameTime);

	Interpolate(m_angularVel,Ang3(0,0,0),6.6f,m_frameTime);
	m_deltaAngles += m_stats.angularVel + m_angularImpulseDelta;

	//Restore leanAmount if reseted
	m_leanAmount = tempLean;
}

void CPlayerRotation::ProcessAngularImpulses()
{
	//update angular impulse
	if (m_angularImpulseTime>0.001f)
	{
		m_angularImpulse *= min(m_angularImpulseTime / m_stats.angularImpulseTimeMax,1.0f);
		m_angularImpulseTime -= m_frameTime;
	}
	else if (m_stats.angularImpulseDeceleration>0.001f)
	{
		Interpolate(m_angularImpulse,ZERO,m_stats.angularImpulseDeceleration, m_frameTime);
	}
	m_angularImpulseDelta -= m_angularImpulse;
}

void CPlayerRotation::ClampAngles()
{
	{
		//cap up/down looking
		float minAngle,maxAngle;
		GetStanceAngleLimits(minAngle,maxAngle);

		float currentViewPitch=GetLocalPitch();
		float newPitch = currentViewPitch + m_deltaAngles.x;
		if (newPitch < minAngle)
			newPitch = minAngle;
		else if (newPitch > maxAngle)
			newPitch = maxAngle;
		m_deltaAngles.x = newPitch - currentViewPitch;

	}

	{
		//further limit the view if necessary
		float limitV = m_params.vLimitRangeV;
		float limitH = m_params.vLimitRangeH;
		Vec3  limitDir = m_params.vLimitDir;
		float limitVUp = m_params.vLimitRangeVUp;
		float limitVDown = m_params.vLimitRangeVDown;

		if (m_player.m_stats.isFrozen.Value())
		{ 
      float clampMin = g_pGameCVars->cl_frozenAngleMin;
      float clampMax = g_pGameCVars->cl_frozenAngleMax;
      float frozenLimit = DEG2RAD(clampMin + (clampMax-clampMin)*(1.f-m_player.GetFrozenAmount(true)));    

			if (limitV == 0 || limitV>frozenLimit)
				limitV = frozenLimit;
			if (limitH == 0 || limitH>frozenLimit)
				limitH = frozenLimit;

      if (g_pGameCVars->cl_debugFreezeShake)
      {
        static float color[] = {1,1,1,1};    
        gEnv->pRenderer->Draw2dLabel(100,200,1.5,color,false,"limit: %f", RAD2DEG(frozenLimit));
      }
		}

		if(m_player.m_stats.isOnLadder)
		{
			limitDir = -m_player.m_stats.ladderOrientation;
			limitH = DEG2RAD(40.0f);
		}

		if ((limitH+limitV+limitVUp+limitVDown) && limitDir.len2()>0.1f)
		{
			//A matrix is built around the view limit, and then the player view angles are checked with it.
			//Later, if necessary the upVector could be made customizable.
			Vec3 forward(limitDir);
			Vec3 up(m_baseQuat.GetColumn2());
			Vec3 right(-(up % forward));
			right.Normalize();

			Matrix33 limitMtx;
			limitMtx.SetFromVectors(right,forward,right%forward);
			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(m_player.GetEntity()->GetWorldPos(), ColorB(0,0,255,255), m_player.GetEntity()->GetWorldPos() + limitMtx.GetColumn(0), ColorB(0,0,255,255));
			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(m_player.GetEntity()->GetWorldPos(), ColorB(0,255,0,255), m_player.GetEntity()->GetWorldPos() + limitMtx.GetColumn(1), ColorB(0,255,0,255));
			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(m_player.GetEntity()->GetWorldPos(), ColorB(255,0,0,255), m_player.GetEntity()->GetWorldPos() + limitMtx.GetColumn(2), ColorB(255,0,0,255));
			limitMtx.Invert();

			Vec3 localDir(limitMtx * m_viewQuat.GetColumn1());
//			Vec3 localDir(limitMtx * m_player.GetEntity()->GetWorldRotation().GetColumn1());

			Ang3 limit;

			if (limitV)
			{
				limit.x = asin(localDir.z) + m_deltaAngles.x;

				float deltaX(limitV - fabs(limit.x));
				if (deltaX < 0.0f)
					m_deltaAngles.x += deltaX*(limit.x>0.0f?1.0f:-1.0f);
			}

			if (limitVUp || limitVDown)
			{
				limit.x = asin(localDir.z) + m_deltaAngles.x;

				if(limit.x>=limitVUp && limitVUp!=0)
				{
					float deltaXUp(limitVUp - limit.x);
					m_deltaAngles.x += deltaXUp;
				}
				if(limit.x<=limitVDown && limitVDown!=0)
				{
					float deltaXDown(limitVDown - limit.x);
					m_deltaAngles.x += deltaXDown;
				}
			}

			if (limitH)
			{
				limit.z = cry_atan2f(-localDir.x,localDir.y) + m_deltaAngles.z;

				float deltaZ(limitH - fabs(limit.z));
				if (deltaZ < 0.0f)
					m_deltaAngles.z += deltaZ*(limit.z>0.0f?1.0f:-1.0f);		
			}
		}
	}
}

void CPlayerRotation::ProcessNormal()
{
	m_upVector = Vec3::CreateSlerp(m_upVector,m_stats.upVector,min(5.0f*m_frameTime, 1.0f));

	//create a matrix perpendicular to the ground
	Vec3 up(m_upVector);
	//..or perpendicular to the linked object Z
	SLinkStats *pLinkStats = &m_player.m_linkStats;
	if (pLinkStats->linkID && pLinkStats->flags & LINKED_FREELOOK)
	{
		IEntity *pLinked = pLinkStats->GetLinked();
		if (pLinked)
			up = pLinked->GetRotation().GetColumn2();
	}
	
	Vec3 right(m_baseQuat.GetColumn0());
	Vec3 forward((up % right).GetNormalized());

	CHECKQNAN_VEC(up);
	CHECKQNAN_VEC(right);
	CHECKQNAN_VEC(forward);

	m_baseQuat = Quat(Matrix33::CreateFromVectors(forward % up,forward,up));
	//CHECKQNAN_MAT33(m_baseMtx);
	m_baseQuat *= Quat::CreateRotationZ(m_deltaAngles.z);
	//m_baseQuat.Normalize();

	m_viewQuat = m_baseQuat * 
		Quat::CreateRotationX(GetLocalPitch() + m_deltaAngles.x) * 
		Quat::CreateRotationY(m_viewRoll);
	//m_viewQuat.Normalize();

	//CHECKQNAN_MAT33(m_viewMtx);
}

void CPlayerRotation::ProcessLean()
{
	float leanAmt(0.0f);

	if(m_stats.isOnLadder)
		return;

	if (!m_stats.inZeroG || m_stats.inAir<0.1f)
	{
		if(float sLean = m_player.GetSpeedLean())
			leanAmt = std::min(1.0f, sLean * 0.05f);
		else if((m_actions & (ACTION_LEANLEFT|ACTION_LEANRIGHT)) != 0)
			leanAmt = ((m_actions & ACTION_LEANLEFT)?-1.0f:0.0f) + ((m_actions & ACTION_LEANRIGHT)?1.0f:0.0f);
		else if(fabsf(m_desiredLeanAmount) > 0.01f)
			leanAmt = m_desiredLeanAmount;
	}

	EStance stance = m_player.GetStance();
	if (stance == STANCE_PRONE)
		leanAmt *= 0.65f;

	m_leanAmount = leanAmt;
	
	//check if its possible
	if (m_leanAmount*m_leanAmount > 0.01f)
	{
		float noLean(0.0f);
		Vec3 headPos(m_player.GetEntity()->GetWorldPos() + m_baseQuat * m_player.GetStanceViewOffset(stance,&noLean));
		Vec3 newPos(m_player.GetEntity()->GetWorldPos() + m_baseQuat * m_player.GetStanceViewOffset(stance,&m_leanAmount));

		/*gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(headPos, 0.05f, ColorB(0,0,255,255) );
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(newPos, 0.05f, ColorB(0,0,255,255) );
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(headPos, ColorB(0,0,255,255), newPos, ColorB(0,0,255,255));*/

		ray_hit hit;
		int rayFlags(rwi_stop_at_pierceable|rwi_colltype_any);//COLLISION_RAY_PIERCABILITY & rwi_pierceability_mask);
		IPhysicalEntity *pSkip(m_player.GetEntity()->GetPhysics());

		float distMult(3.0f);

		if (gEnv->pPhysicalWorld->RayWorldIntersection(headPos + m_viewQuat.GetColumn1() * 0.25f, (newPos - headPos)*distMult, ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid, rayFlags, &hit, 1, &pSkip, 1))
		{
			float dist((headPos - newPos).len2() * distMult);
			m_leanAmount *= ((hit.pt - headPos).len2() / dist) / distMult;

			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(hit.pt, 0.05f, ColorB(0,255,0,255) );
		}
	}
}
