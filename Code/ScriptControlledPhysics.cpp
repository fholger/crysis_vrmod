/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 26:7:2007   17:01 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ScriptControlledPhysics.h"


//------------------------------------------------------------------------
CScriptControlledPhysics::CScriptControlledPhysics()
: m_moving(false)
, m_speed(0.0f)
, m_maxSpeed(0.0f)
, m_acceleration(0.0f)
, m_stopTime(1.0f)
,	m_rotating(false)
, m_rotationTarget(IDENTITY)
, m_rotationSpeed(0.0f)
, m_rotationMaxSpeed(0.0f)
, m_rotationAcceleration(0.0f)
, m_rotationStopTime(0.0f)
{
}


//------------------------------------------------------------------------
CScriptControlledPhysics::~CScriptControlledPhysics()
{
}

//------------------------------------------------------------------------
void CScriptControlledPhysics::RegisterGlobals()
{
}

//------------------------------------------------------------------------
void CScriptControlledPhysics::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptControlledPhysics::

	SCRIPT_REG_TEMPLFUNC(GetSpeed, "");
	SCRIPT_REG_TEMPLFUNC(GetAcceleration, "");
	
	SCRIPT_REG_TEMPLFUNC(GetAngularSpeed, "");
	SCRIPT_REG_TEMPLFUNC(GetAngularAcceleration, "");

	SCRIPT_REG_TEMPLFUNC(MoveTo, "point, initialSpeed, speed, acceleration, stopTime");
	SCRIPT_REG_TEMPLFUNC(RotateTo, "dir, roll, initialSpeed, speed, acceleration, stopTime");
	SCRIPT_REG_TEMPLFUNC(RotateToAngles, "angles, initialSpeed, speed, acceleration, stopTime");
}

//------------------------------------------------------------------------
bool CScriptControlledPhysics::Init(IGameObject * pGameObject )
{
	CScriptableBase::Init(gEnv->pScriptSystem, gEnv->pSystem, 1);

	SetGameObject(pGameObject);
	pGameObject->EnablePhysicsEvent(true, eEPE_OnPostStepLogged);

	RegisterGlobals();
	RegisterMethods();

	SmartScriptTable thisTable(gEnv->pScriptSystem);

	thisTable->SetValue("__this", ScriptHandle(GetEntityId()));
	thisTable->Delegate(GetMethodsTable());
	GetEntity()->GetScriptTable()->SetValue("scp", thisTable);

	return true;
}


//------------------------------------------------------------------------
void CScriptControlledPhysics::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CScriptControlledPhysics::PostInit( IGameObject * pGameObject )
{
	if (IPhysicalEntity *pPE=GetEntity()->GetPhysics())
	{
		pe_params_flags fp;
		fp.flagsOR=pef_log_poststep;
		pPE->SetParams(&fp);
	}
}

//------------------------------------------------------------------------
void CScriptControlledPhysics::HandleEvent(const SGameObjectEvent& event)
{
	switch(event.event)
	{
	case eGFE_OnPostStep:
		OnPostStep((EventPhysPostStep *)event.ptr);
		break;
	}
}

//------------------------------------------------------------------------
void CScriptControlledPhysics::OnPostStep(EventPhysPostStep *pPostStep)
{
	pe_action_set_velocity av;

	bool moving=m_moving;
	bool rotating=m_rotating;

	float dt=pPostStep->dt;

	if (m_moving)
	{
		Vec3 current=pPostStep->pos;
		Vec3 target=m_moveTarget;
		Vec3 delta=target-current;
		float distance=delta.len();
		Vec3 dir=delta;
		if(distance>0.01f)
			dir *= (1.0f/distance);

		if (distance<0.01f)
		{
			m_speed=0.0f;
			m_moving=false;

			pPostStep->pos=target;

			av.v=ZERO;
		}
		else
		{
			float a=m_speed/m_stopTime;
			float d=m_speed*m_stopTime-0.5f*a*m_stopTime*m_stopTime;
			
			if (distance<=(d+0.01f))
				m_acceleration=(distance-m_speed*m_stopTime)/(m_stopTime*m_stopTime);

			m_speed=m_speed+m_acceleration*dt;
			if (m_speed>=m_maxSpeed)
			{
				m_speed=m_maxSpeed;
				m_acceleration=0.0f;
			}
			else if (m_speed*dt>distance)
				m_speed=distance/dt;
			else if (m_speed<0.05f)
				m_speed=0.05f;

			av.v=dir*m_speed;
		}
	}

	if (m_rotating)
	{
		Quat current=pPostStep->q;
		Quat target=m_rotationTarget;

		Quat rotation=target*!current;
		float angle=cry_acosf(CLAMP(rotation.w, -1.0f, 1.0f))*2.0f;
		float original=angle;
		if (angle>gf_PI)
			angle=angle-gf_PI2;
		else if (angle<-gf_PI)
			angle=angle+gf_PI2;

		if (cry_fabsf(angle)<0.001f)
		{
			m_rotationSpeed=0.0f;
			m_rotating=false;
			pPostStep->q=m_rotationTarget;
			av.w=ZERO;
		}
		else
		{
			float a=m_rotationSpeed/m_rotationStopTime;
			float d=m_rotationSpeed*m_stopTime-0.5f*a*m_rotationStopTime*m_rotationStopTime;

			if (cry_fabsf(angle)<d+0.001f)
				m_rotationAcceleration=(angle-m_rotationSpeed*m_rotationStopTime)/(m_rotationStopTime*m_rotationStopTime);

			m_rotationSpeed=m_rotationSpeed+sgn(angle)*m_rotationAcceleration*dt;
			if (cry_fabsf(m_rotationSpeed*dt)>cry_fabsf(angle))
				m_rotationSpeed=angle/dt;
			else if (cry_fabsf(m_rotationSpeed)<0.001f)
				m_rotationSpeed=sgn(m_rotationSpeed)*0.001f;
			else if (cry_fabsf(m_rotationSpeed)>=m_rotationMaxSpeed)
			{
				m_rotationSpeed=sgn(m_rotationSpeed)*m_rotationMaxSpeed;
				m_rotationAcceleration=0.0f;
			}

		}

		if(cry_fabsf(angle)>=0.001f)
			av.w=(rotation.v/cry_sinf(original*0.5f)).normalized();
		av.w*=m_rotationSpeed;
	}

	if (moving || rotating)
	{
		if (IPhysicalEntity *pPE=GetEntity()->GetPhysics())
			pPE->Action(&av);
	}

	if ((moving && !m_moving) || (rotating && !m_rotating))
		GetEntity()->SetWorldTM(Matrix34::Create(GetEntity()->GetScale(), pPostStep->q, pPostStep->pos));
}


//------------------------------------------------------------------------
int CScriptControlledPhysics::GetSpeed(IFunctionHandler *pH)
{
	return pH->EndFunction(m_speed);
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::GetAcceleration(IFunctionHandler *pH)
{
	return pH->EndFunction(m_acceleration);
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::GetAngularSpeed(IFunctionHandler *pH)
{
	return pH->EndFunction(m_rotationSpeed);
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::GetAngularAcceleration(IFunctionHandler *pH)
{
	return pH->EndFunction(m_rotationAcceleration);
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::MoveTo(IFunctionHandler *pH, Vec3 point, float initialSpeed, float speed, float acceleration, float stopTime)
{
	m_moveTarget=point;
	m_moving=true;
	m_speed=initialSpeed;
	m_maxSpeed=speed;
	m_acceleration=acceleration;
	m_stopTime=stopTime;

	pe_action_awake aa;
	aa.bAwake=1;

	if (IPhysicalEntity *pPE=GetEntity()->GetPhysics())
		pPE->Action(&aa);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::RotateTo(IFunctionHandler *pH, Vec3 dir, float roll, float initialSpeed, float speed, float acceleration, float stopTime)
{
	m_rotationTarget=Quat::CreateRotationVDir(dir, roll);
	m_rotating=true;
	m_rotationSpeed=DEG2RAD(initialSpeed);
	m_rotationMaxSpeed=DEG2RAD(speed);
	m_rotationAcceleration=DEG2RAD(acceleration);
	m_rotationStopTime=stopTime;

	pe_action_awake aa;
	aa.bAwake=1;

	if (IPhysicalEntity *pPE=GetEntity()->GetPhysics())
		pPE->Action(&aa);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptControlledPhysics::RotateToAngles(IFunctionHandler *pH, Vec3 angles, float initialSpeed, float speed, float acceleration, float stopTime)
{
	m_rotationTarget=Quat::CreateRotationXYZ(Ang3(angles));
	m_rotating=true;
	m_rotationSpeed=DEG2RAD(initialSpeed);
	m_rotationMaxSpeed=DEG2RAD(speed);
	m_rotationAcceleration=DEG2RAD(acceleration);
	m_rotationStopTime=stopTime;

	pe_action_awake aa;
	aa.bAwake=1;

	if (IPhysicalEntity *pPE=GetEntity()->GetPhysics())
		pPE->Action(&aa);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
void CScriptControlledPhysics::GetMemoryStatistics(ICrySizer * s)
{
	int nSize = sizeof(*this);
	s->AddObject(this,nSize);
}

