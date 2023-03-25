/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a flow nodes for vehicles in Game02

-------------------------------------------------------------------------
History:
- 12:12:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "IVehicleSystem.h"
#include "IFlowSystem.h"
#include "Nodes/G2FlowBaseNode.h"
#include "VehicleActionEntityAttachment.h"
#include "GameCVars.h"

class CVehicleActionEntityAttachment;

//------------------------------------------------------------------------
class CFlowVehicleEntityAttachment
: public CFlowBaseNode
{
public:

	CFlowVehicleEntityAttachment(SActivationInfo* pActivationInfo);
	~CFlowVehicleEntityAttachment() {}

	// CFlowBaseNode
	virtual IFlowNodePtr Clone(SActivationInfo* pActivationInfo);
	virtual void GetConfiguration(SFlowNodeConfig& nodeConfig);
	virtual void ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo);
	virtual void Serialize(SActivationInfo* pActivationInfo, TSerialize ser);

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
	// ~CFlowBaseNode

protected:

	IVehicle* GetVehicle();
	CVehicleActionEntityAttachment* GetVehicleAction();

	IFlowGraph *m_pGraph;
	TFlowNodeId m_nodeID;

	EntityId m_vehicleId;

	enum EInputs
	{
		IN_DROPATTACHMENTTRIGGER,
	};

	enum EOutputs
	{
		OUT_ENTITYID,
		OUT_ISATTACHED,
	};
};

//------------------------------------------------------------------------
CFlowVehicleEntityAttachment::CFlowVehicleEntityAttachment(SActivationInfo* pActivationInfo)
{
	m_nodeID = pActivationInfo->myID;
	m_pGraph = pActivationInfo->pGraph;

	if (IEntity* pEntity = pActivationInfo->pEntity)
	{
		IVehicleSystem* pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();
		assert(pVehicleSystem);

		if (pVehicleSystem->GetVehicle(pEntity->GetId()))
			m_vehicleId = pEntity->GetId();
	}
	else
		m_vehicleId = 0;
}

//------------------------------------------------------------------------
IFlowNodePtr CFlowVehicleEntityAttachment::Clone(SActivationInfo* pActivationInfo)
{
	return new CFlowVehicleEntityAttachment(pActivationInfo);
}

void CFlowVehicleEntityAttachment::Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
{
	// MATHIEU: FIXME!
}

//------------------------------------------------------------------------
void CFlowVehicleEntityAttachment::GetConfiguration(SFlowNodeConfig& nodeConfig)
{
	static const SInputPortConfig pInConfig[] = 
	{
		InputPortConfig_Void("DropAttachmentTrigger", _HELP("Trigger to drop the attachment")),
		{0}
	};

	static const SOutputPortConfig pOutConfig[] = 
	{
		OutputPortConfig<int>("EntityId", _HELP("Entity Id of the attachment")),
		OutputPortConfig<bool>("IsAttached", _HELP("If the attachment is still attached")),
		{0}
	};

	nodeConfig.sDescription = _HELP("Handle the entity attachment used as vehicle action");
	nodeConfig.nFlags |= EFLN_TARGET_ENTITY;
	nodeConfig.pInputPorts = pInConfig;
	nodeConfig.pOutputPorts = pOutConfig;
	nodeConfig.SetCategory(EFLN_WIP);
}

//------------------------------------------------------------------------
void CFlowVehicleEntityAttachment::ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
{
	if (flowEvent == eFE_SetEntityId)
	{
		if (IEntity* pEntity = pActivationInfo->pEntity)
		{
			IVehicleSystem* pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();
			assert(pVehicleSystem);

			if (pEntity->GetId() != m_vehicleId)
				m_vehicleId = 0;

			if (IVehicle* pVehicle = pVehicleSystem->GetVehicle(pEntity->GetId()))
				m_vehicleId = pEntity->GetId();
		}
		else
		{
			m_vehicleId = 0;
		}
	}
	else if (flowEvent == eFE_Activate)
	{
		if (CVehicleActionEntityAttachment* pAction = GetVehicleAction())
		{
			if (IsPortActive(pActivationInfo, IN_DROPATTACHMENTTRIGGER))
			{
				pAction->DetachEntity();

				SFlowAddress addr(m_nodeID, OUT_ISATTACHED, true);
				m_pGraph->ActivatePort(addr, pAction->IsEntityAttached());
			}

			SFlowAddress addr(m_nodeID, OUT_ENTITYID, true);
			m_pGraph->ActivatePort(addr, pAction->GetAttachmentId());
		}
	}
}

//------------------------------------------------------------------------
IVehicle* CFlowVehicleEntityAttachment::GetVehicle()
{
	if (!m_vehicleId)
		return NULL;

	IVehicleSystem* pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();
	assert(pVehicleSystem);

	return pVehicleSystem->GetVehicle(m_vehicleId);
}

//------------------------------------------------------------------------
CVehicleActionEntityAttachment* CFlowVehicleEntityAttachment::GetVehicleAction()
{
	if (!m_vehicleId)
		return NULL;

	IVehicleSystem* pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();
	assert(pVehicleSystem);

	if (IVehicle* pVehicle = pVehicleSystem->GetVehicle(m_vehicleId))
	{
		for (int i = 1; i < pVehicle->GetActionCount(); i++)
		{
			IVehicleAction* pAction = pVehicle->GetAction(i);
			assert(pAction);

			if (CVehicleActionEntityAttachment* pAttachment = 
				CAST_VEHICLEOBJECT(CVehicleActionEntityAttachment, pAction))
			{
				return pAttachment;
			}
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
class CFlowVehicleSetAltitudeLimit
	: public CFlowBaseNode
{
public:

	CFlowVehicleSetAltitudeLimit(SActivationInfo* pActivationInfo) {};
	~CFlowVehicleSetAltitudeLimit() {}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	enum Inputs
	{
		EIP_SetLimit,
		EIP_Limit
	};

	virtual void GetConfiguration(SFlowNodeConfig& nodeConfig)
	{
		static const SInputPortConfig pInConfig[] = 
		{
			InputPortConfig_Void  ("SetLimit", _HELP("Trigger to set limit")),
			InputPortConfig<float>("Limit", _HELP("Altitude limit in meters")),
			{0}
		};

		nodeConfig.sDescription = _HELP("Set Vehicle's Maximum Altitude");
		nodeConfig.pInputPorts = pInConfig;
		nodeConfig.pOutputPorts = 0;
		nodeConfig.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent flowEvent, SActivationInfo* pActivationInfo)
	{
		if (flowEvent == eFE_Activate && IsPortActive(pActivationInfo, EIP_SetLimit))
		{
			const float fVal = GetPortFloat(pActivationInfo, EIP_Limit);
			CryFixedStringT<128> buf;
			buf.FormatFast("%g", fVal);
			g_pGameCVars->pAltitudeLimitCVar->ForceSet(buf.c_str());
		}
	}
};


REGISTER_FLOW_NODE("Vehicle:EntityAttachment", CFlowVehicleEntityAttachment);
REGISTER_FLOW_NODE("Game:SetVehicleAltitudeLimit", CFlowVehicleSetAltitudeLimit);
