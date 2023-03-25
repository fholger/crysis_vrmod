/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a vehicle action to deploy a rope

-------------------------------------------------------------------------
History:
- 30:11:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "IVehicleSystem.h"
#include "VehicleActionDeployRope.h"
#include "Game.h"

float g_ropeLenght = 12.0f;

//------------------------------------------------------------------------
CVehicleActionDeployRope::CVehicleActionDeployRope()
: m_pVehicle(NULL),
	m_seatId(InvalidVehicleSeatId),
	m_pRopeRenderNode(NULL),
	m_pRopeHelper(NULL),
	m_pDeployAnim(NULL),
	m_deployAnimOpenedId(InvalidVehicleAnimStateId),
	m_deployAnimClosedId(InvalidVehicleAnimStateId)
{
}

//------------------------------------------------------------------------
CVehicleActionDeployRope::~CVehicleActionDeployRope()
{
	if (m_ropeUpperId)
	{
		gEnv->pEntitySystem->RemoveEntity(m_ropeUpperId);
		m_ropeUpperId = 0;
	}

	if (m_ropeLowerId)
	{
		gEnv->pEntitySystem->RemoveEntity(m_ropeLowerId);
		m_ropeLowerId = 0;
	}
}

//------------------------------------------------------------------------
bool CVehicleActionDeployRope::Init(IVehicle* pVehicle, TVehicleSeatId seatId, const SmartScriptTable &table)
{
	m_pVehicle = pVehicle;
	m_seatId = seatId;

	SmartScriptTable deployRopeTable;
	if (!table->GetValue("DeployRope", deployRopeTable))
		return false;

	char* pHelperName;
	if (deployRopeTable->GetValue("helper", pHelperName))
		m_pRopeHelper = m_pVehicle->GetHelper(pHelperName);

	char* pAnimName;
	if (deployRopeTable->GetValue("animation", pAnimName))
	{
		m_pDeployAnim = m_pVehicle->GetAnimation(pAnimName);

		if (m_pDeployAnim)
		{
			m_deployAnimOpenedId = m_pDeployAnim->GetStateId("opened");
			m_deployAnimClosedId = m_pDeployAnim->GetStateId("closed");

			if (m_deployAnimOpenedId == InvalidVehicleAnimStateId 
				|| m_deployAnimClosedId == InvalidVehicleAnimStateId)
			{
				m_pDeployAnim = NULL;
			}
		}
	}

	return m_pRopeHelper != NULL;
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::Reset()
{
	if (m_ropeUpperId)
	{
		gEnv->pEntitySystem->RemoveEntity(m_ropeUpperId);
		m_ropeUpperId = 0;
	}

	if (m_ropeLowerId)
	{
		gEnv->pEntitySystem->RemoveEntity(m_ropeLowerId);
		m_ropeLowerId = 0;
	}

	m_actorId = 0;
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
 	if (actionId == eVAI_Attack1 && activationMode == eAAM_OnPress)
	{
		if (m_pDeployAnim)
			m_pDeployAnim->ChangeState(m_deployAnimOpenedId);
		
		DeployRope();
	}
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	if (event == eVE_PassengerExit && params.iParam == m_seatId)
	{
		IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
		assert(pActorSystem);

		IActor* pActor = pActorSystem->GetActor(params.entityId);
		if (!pActor)
		{
			assert(pActor);
			return;
		}

		m_actorId = pActor->GetEntityId();

		DeployRope();
		AttachOnRope(pActor->GetEntity());
	} 
	else if (event == eVE_Destroyed)
	{
		if (m_ropeUpperId)
		{
			gEnv->pEntitySystem->RemoveEntity(m_ropeUpperId);
			m_ropeUpperId = 0;
		}

		if (m_ropeLowerId)
		{
			gEnv->pEntitySystem->RemoveEntity(m_ropeLowerId);
			m_ropeLowerId = 0;
		}
	}
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::Update(const float deltaTime)
{
	if (!m_ropeUpperId && !m_ropeLowerId && !m_actorId)
		return;

	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	assert(pActorSystem);

	IActor* pActor = pActorSystem->GetActor(m_actorId);
	if (!pActor)
		return;

	Vec3 worldPos = pActor->GetEntity()->GetWorldTM().GetTranslation();

	if (IRopeRenderNode* pRopeUpper = GetRopeRenderNode(m_ropeUpperId))
	{
		Vec3 points[2];
		points[0] = m_pRopeHelper->GetWorldTM().GetTranslation();
		points[1] = worldPos;

		pRopeUpper->SetPoints(points, 2);

		float lenghtLeft = max(0.0f, g_ropeLenght - (points[0].z - points[1].z));

		if (IRopeRenderNode* pRopeLower = GetRopeRenderNode(m_ropeLowerId))
		{
			Vec3 points[2];
			points[0] = worldPos;
			points[1] = Vec3(worldPos.x, worldPos.y, worldPos.z - lenghtLeft);

			pRopeLower->SetPoints(points, 2);
		}
	}
}

//------------------------------------------------------------------------
bool CVehicleActionDeployRope::DeployRope()
{
	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	assert(pActorSystem);

	IActor* pActor = pActorSystem->GetActor(m_actorId);
	if (!pActor)
		return false;

	Vec3 upperPos = m_pRopeHelper->GetWorldTM().GetTranslation();
	Vec3 lowerPos(upperPos.x, upperPos.y, upperPos.z - g_ropeLenght);

	m_ropeUpperId = CreateRope(m_pVehicle->GetEntity()->GetPhysics(), upperPos, upperPos);
	m_ropeLowerId = CreateRope(pActor->GetEntity()->GetPhysics(), upperPos, lowerPos);
	
	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

	return true;
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::AttachOnRope(IEntity* pEntity)
{
	assert(pEntity);
	if (!pEntity)
		return;

	IRopeRenderNode* pRopeUpper = GetRopeRenderNode(m_ropeUpperId);
	if (!pRopeUpper)
		return;

	assert(pRopeUpper->GetPointsCount() >= 2);

	IPhysicalEntity* pRopePhys = pRopeUpper->GetPhysics();
	assert(pRopePhys);

	typedef std::vector <Vec3> TVec3Vector;
	TVec3Vector points;

	int pointCount;

	pe_status_rope ropeStatus;
	if (pRopePhys->GetStatus(&ropeStatus))
		pointCount = ropeStatus.nSegments + 1;
	else
		pointCount = 0;

	if (pointCount < 2)
		return;

	points.resize(pointCount);
	ropeStatus.pPoints = &points[0];

	if (pRopePhys->GetStatus(&ropeStatus))
	{
		Matrix34 worldTM;
		worldTM.SetIdentity();
		worldTM = Matrix33(m_pVehicle->GetEntity()->GetWorldTM());
		worldTM.SetTranslation(ropeStatus.pPoints[1]);
		pEntity->SetWorldTM(worldTM);
	}

	pRopeUpper->LinkEndEntities(m_pVehicle->GetEntity()->GetPhysics(), pEntity->GetPhysics());
}

//------------------------------------------------------------------------
EntityId CVehicleActionDeployRope::CreateRope(IPhysicalEntity* pLinkedEntity, const Vec3& highPos, const Vec3& lowPos)
{
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
	assert(pEntitySystem);

	char pRopeName[256];
	_snprintf(pRopeName, 256, "%s_rope_%d", m_pVehicle->GetEntity()->GetName(), m_seatId);
	pRopeName[sizeof(pRopeName)-1] = '\0';

	SEntitySpawnParams params;
	params.sName = pRopeName;
	params.nFlags = ENTITY_FLAG_CLIENT_ONLY;
	params.pClass = pEntitySystem->GetClassRegistry()->FindClass("RopeEntity");

	IEntity* pRopeEntity = pEntitySystem->SpawnEntity(params, true);
	if (!pRopeEntity)
		return 0;

	pRopeEntity->SetFlags(pRopeEntity->GetFlags() | ENTITY_FLAG_CASTSHADOW );

	pRopeEntity->CreateProxy(ENTITY_PROXY_ROPE);

	IEntityRopeProxy* pEntityRopeProxy = (IEntityRopeProxy*) pRopeEntity->GetProxy(ENTITY_PROXY_ROPE);
	if (!pEntityRopeProxy)
	{
		pEntitySystem->RemoveEntity(pRopeEntity->GetId());
		return 0;
	}

	IRopeRenderNode* pRopeNode = pEntityRopeProxy->GetRopeRendeNode();
	assert(pRopeNode);

	Vec3 ropePoints[2];
	ropePoints[0] = highPos;
	ropePoints[1] = lowPos;

	IRopeRenderNode::SRopeParams m_ropeParams;

	m_ropeParams.nFlags = IRopeRenderNode::eRope_CheckCollisinos | IRopeRenderNode::eRope_Smooth;
	m_ropeParams.fThickness = 0.05f;
	m_ropeParams.fAnchorRadius = 0.1f;
	m_ropeParams.nNumSegments = 8;
	m_ropeParams.nNumSides = 4;
	m_ropeParams.nMaxSubVtx = 3;
	m_ropeParams.nPhysSegments = 8;
	m_ropeParams.mass = 1.0f;
	m_ropeParams.friction = 2;
	m_ropeParams.frictionPull = 2;
	m_ropeParams.wind.Set(0,0,0);
	m_ropeParams.windVariance = 0;
	m_ropeParams.waterResistance = 0;
	m_ropeParams.jointLimit = 0;
	m_ropeParams.maxForce = 0;
	m_ropeParams.airResistance = 0;
	m_ropeParams.fTextureTileU = 1.0f;
	m_ropeParams.fTextureTileV = 10.0f;

	pRopeNode->SetParams(m_ropeParams);
	pRopeNode->SetPoints(ropePoints, 2);
	pRopeNode->SetEntityOwner(m_pVehicle->GetEntity()->GetId());

	pRopeNode->LinkEndEntities(m_pVehicle->GetEntity()->GetPhysics(), NULL);

	return pRopeEntity->GetId();
}

//------------------------------------------------------------------------
IRopeRenderNode* CVehicleActionDeployRope::GetRopeRenderNode(EntityId ropeId)
{
	if (!ropeId)
		return NULL;

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(ropeId);
	if (!pEntity)
		return NULL;

	IEntityRopeProxy* pEntityProxy = (IEntityRopeProxy*) pEntity->GetProxy(ENTITY_PROXY_ROPE);
	if (!pEntityProxy)
		return NULL;

	return pEntityProxy->GetRopeRendeNode();
}

//------------------------------------------------------------------------
void CVehicleActionDeployRope::GetMemoryStatistics(ICrySizer* pSizer)
{
	pSizer->Add(*this);
}

DEFINE_VEHICLEOBJECT(CVehicleActionDeployRope);
