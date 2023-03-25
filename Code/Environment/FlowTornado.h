// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#ifndef __FLOWTORNADO_H__
#define __FLOWTORNADO_H__

#include "../Game.h"
#include "Tornado.h"
#include "../Nodes/G2FlowBaseNode.h"

class CFlowTornadoWander : public CFlowBaseNode
{
public:
	CFlowTornadoWander( SActivationInfo * pActInfo )
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowTornadoWander(pActInfo);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Activate", _HELP("Tell the tornado to actually wander to the new [Target]")),
				InputPortConfig<EntityId>("Target", _HELP("Set a new wandering target for the tornado")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("Done", _HELP("Triggered when target has been reached")),
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Tells a tornado entity to wander in the direction of the [Target] entity");
		config.SetCategory(EFLN_WIP);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0) && pActInfo->pEntity)
			{
				CTornado* pTornado = (CTornado*)g_pGame->GetIGameFramework()->QueryGameObjectExtension(pActInfo->pEntity->GetId(), "Tornado");

				if (pTornado)
				{ 
					IEntity* pTarget = gEnv->pEntitySystem->GetEntity( GetPortEntityId(pActInfo, 1) );
					if (pTarget)
					{
						pTornado->SetTarget(pTarget, this);
					}
				}        
			}
			break;
		}
	}

	void Done()
	{
		if (m_actInfo.pGraph)
			ActivateOutput( &m_actInfo, 0, true);
	}
private:
	SActivationInfo m_actInfo;
};

#endif //__FLOWTORNADO_H__
