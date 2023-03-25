// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "Game.h"
#include "Player.h"
#include "NanoSuit.h"
#include "Nodes/G2FlowBaseNode.h"

#if defined(WHOLE_PROJECT)
	#define GetPlayer GetPlayerNodes
#endif

namespace
{
	CPlayer* GetPlayer(EntityId entityId)
	{
		static IActorSystem* pActorSys = 0;
		if (pActorSys == 0)
			pActorSys = g_pGame->GetIGameFramework()->GetIActorSystem();
		
		if (pActorSys != 0)
		{
			IActor* pIActor = pActorSys->GetActor(entityId);
			if (pIActor)
			{
				CActor* pActor = (CActor*) pIActor;
				if (pActor && CPlayer::GetActorClassType() == pActor->GetActorClass())
					return static_cast<CPlayer*> (pActor);
			}
		}
		return 0;
	}
};	

class CFlowNanoSuitNode : public CFlowBaseNode, public CNanoSuit::INanoSuitListener
{
public:
	CFlowNanoSuitNode( SActivationInfo * pActInfo ) : m_entityId (0)
	{
	}

	~CFlowNanoSuitNode()
	{
		RemoveAsNanoSuitListener();
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowNanoSuitNode(pActInfo);
	}

	enum EInputPorts
	{
		EIP_Cloak = 0,
		EIP_Strength,
		EIP_Defense,
		EIP_Speed,
		EIP_Energy,
		EIP_CloakLevel,
	};

	enum EOutputPorts
	{
		EOP_Cloak = 0,
		EOP_Strength,
		EOP_Defense,
		EOP_Speed,
		EOP_Energy,
		EOP_CloakLevel,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void  ("Cloak", _HELP("Trigger to select Cloak Mode")),
			InputPortConfig_Void  ("Strength", _HELP("Trigger to select Strength Mode")),
			InputPortConfig_Void  ("Defense", _HELP("Trigger to select Defense Mode")),
			InputPortConfig_Void  ("Speed", _HELP("Trigger to select Speed Mode")),
			InputPortConfig<float>("Energy", 0.0f, _HELP("Set Energy")),
			InputPortConfig<int>  ("CloakLevel", 1, _HELP("Set cloak level [1-3]")),
			InputPortConfig_Void  ("BreakHUD", _HELP("Trigger to break the HUD, causing it to disappear")),
			InputPortConfig_Void  ("RebootHUD", _HELP("Trigger to reboot the HUD, causing it to appear")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void  ("Cloak", _HELP("Triggered on Default Mode")),
			OutputPortConfig_Void  ("Strength", _HELP("Triggered on Strength Mode")),
			OutputPortConfig_Void  ("Defense", _HELP("Triggered on Defense Mode")),
			OutputPortConfig_Void  ("Speed", _HELP("Triggered on Speed Mode")),
			OutputPortConfig<float>("Energy", _HELP("Current Energy")),
			OutputPortConfig<int>  ("CloakLevel", _HELP("Current cloak level [set when Cloak mode is active]")),
			// OutputPortConfig_Void  ("BreakHUD", "Triggered on breaking the HUD"),
			// OutputPortConfig_Void  ("RebootHUD", "Triggered on rebooting the HUD"),
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("NanoSuit Node");
		config.SetCategory(EFLN_WIP);
	}

	void RemoveAsNanoSuitListener()
	{
		if (m_entityId != 0)
		{
			CPlayer* pPlayer = GetPlayer(m_entityId);
			if (pPlayer != 0)
			{
				CNanoSuit* pSuit = pPlayer->GetNanoSuit();
				if(pSuit)
					pSuit->RemoveListener(this);
			}
			m_entityId = 0;
		}
	}

	void AddAsNanoSuitListener()
	{
		if (m_entityId != 0)
		{
			CPlayer* pPlayer = GetPlayer(m_entityId);
			if (pPlayer != 0)
			{
				CNanoSuit* pSuit = pPlayer->GetNanoSuit();
				if(pSuit)
					pSuit->AddListener(this);
			}
		}
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				CPlayer* pPlayer = GetPlayer(m_entityId);
				if (pPlayer)
				{
					CNanoSuit* pSuit = pPlayer->GetNanoSuit();
					if(pSuit)
					{
						ModeChanged(pSuit->GetMode());
						EnergyChanged(pSuit->GetSuitEnergy());
					}
				}
			}
			break;
		case eFE_SetEntityId:
			{
				RemoveAsNanoSuitListener();
				m_entityId = pActInfo->pEntity ? pActInfo->pEntity->GetId() : 0;
				AddAsNanoSuitListener();
			}
			break;

		case eFE_Activate:
			{
				CPlayer* pPlayer = GetPlayer(m_entityId);
				if (pPlayer == 0)
					return;
				CNanoSuit* pSuit = pPlayer->GetNanoSuit();
				if(!pSuit)
					return;

				if (IsPortActive(pActInfo, EIP_Cloak))
					pSuit->SetMode(NANOMODE_CLOAK);
				else if (IsPortActive(pActInfo, EIP_Speed))
					pSuit->SetMode(NANOMODE_SPEED);
				else if (IsPortActive(pActInfo, EIP_Strength))
					pSuit->SetMode(NANOMODE_STRENGTH);
				else if (IsPortActive(pActInfo, EIP_Defense))
					pSuit->SetMode(NANOMODE_DEFENSE);
	
				if (IsPortActive(pActInfo, EIP_Energy))
					pSuit->SetSuitEnergy(GetPortFloat(pActInfo, EIP_Energy));
				if (IsPortActive(pActInfo, EIP_CloakLevel))
					pSuit->SetCloakLevel( (ENanoCloakMode) GetPortInt(pActInfo, EIP_CloakLevel));
			}
			break;
		}
	}

	// INanoSuitListener
	void ModeChanged(ENanoMode mode)
	{
		if (mode == NANOMODE_CLOAK)
			ActivateOutput(&m_actInfo, EOP_Cloak, true);
		else if (mode == NANOMODE_SPEED)
			ActivateOutput(&m_actInfo, EOP_Speed, true);
		else if (mode == NANOMODE_STRENGTH)
			ActivateOutput(&m_actInfo, EOP_Strength, true);
		else if (mode == NANOMODE_DEFENSE)
			ActivateOutput(&m_actInfo, EOP_Defense, true);

		CPlayer* pPlayer = GetPlayer(m_entityId);
		if (pPlayer)
		{
			if(pPlayer->GetNanoSuit())
				ActivateOutput(&m_actInfo, EOP_CloakLevel, (int) pPlayer->GetNanoSuit()->GetCloak()->GetState());
		}
	}

	void EnergyChanged(float energy)
	{
		ActivateOutput(&m_actInfo, EOP_Energy, energy);
	}


	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	SActivationInfo m_actInfo;
	EntityId        m_entityId;
	//~INanoSuitListener
};


class CFlowNanoSuitGetNode : public CFlowBaseNode
{
public:
	CFlowNanoSuitGetNode( SActivationInfo * pActInfo )
	{
	}

	~CFlowNanoSuitGetNode()
	{
	}

	/*
	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return this; // new CFlowNanoSuitNode(pActInfo);
	}
	*/

	enum EInputPorts
	{
		EIP_Trigger = 0,
	};

	enum EOutputPorts
	{
		EOP_Cloak = 0,
		EOP_Strength,
		EOP_Defense,
		EOP_Speed,
		EOP_Energy,
		EOP_CloakLevel,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void  ("Trigger", _HELP("Trigger to get current NanoSuit values")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void  ("Cloak", _HELP("Triggered on Cloak Mode")),
			OutputPortConfig_Void  ("Strength", _HELP("Triggered on Strength Mode")),
			OutputPortConfig_Void  ("Defense", _HELP("Triggered on Defense Mode")),
			OutputPortConfig_Void  ("Speed", _HELP("Triggered on Speed Mode")),
			OutputPortConfig<float>("Energy", _HELP("Current Energy")),
			OutputPortConfig<int>  ("CloakLevel", _HELP("Current cloak level [1-3]")),
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("NanoSuitGet Node");
		config.SetCategory(EFLN_WIP);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			break;

		case eFE_Activate:
			{
				if (!IsPortActive(pActInfo, EIP_Trigger))
					return;

				if (pActInfo->pEntity == 0)
					return;
				CPlayer* pPlayer = GetPlayer(pActInfo->pEntity->GetId());
				if (pPlayer == 0)
					return;
				CNanoSuit* pSuit = pPlayer->GetNanoSuit();
				if(!pSuit)
					return;
				const int mode = pSuit->GetMode();
				if (mode == NANOMODE_CLOAK)
					ActivateOutput(pActInfo, EOP_Cloak, true);
				else if (mode == NANOMODE_SPEED)
					ActivateOutput(pActInfo, EOP_Speed, true);
				else if (mode == NANOMODE_STRENGTH)
					ActivateOutput(pActInfo, EOP_Strength, true);
				else if (mode == NANOMODE_DEFENSE)
					ActivateOutput(pActInfo, EOP_Defense, true);

				ActivateOutput(pActInfo, EOP_Energy, pSuit->GetSuitEnergy());
				ActivateOutput(pActInfo, EOP_CloakLevel, (int) pSuit->GetCloak()->GetState());
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


class CFlowNanoSuitControlNode : public CFlowBaseNode
{
public:

	CFlowNanoSuitControlNode( SActivationInfo * pActInfo )
	{
	}

	~CFlowNanoSuitControlNode()
	{
	}

	/*
	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return this;
	}
	*/

	enum EInputPorts
	{
		EIP_Add = 0,
		EIP_Remove,
		EIP_Defect,
		EIP_Repair,
		EIP_Mode
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void  ("Add", _HELP("Trigger to add mode")),
			InputPortConfig_Void  ("Remove", _HELP("Trigger to remove mode")),
			InputPortConfig_Void  ("SetDefect", _HELP("Trigger to set mode defect")),
			InputPortConfig_Void  ("Repair", _HELP("Trigger to repair mode")),
			InputPortConfig<int>  ("Mode", 0, _HELP("Mode to add/remove"), 0, _UICONFIG("enum_int:Speed=0,Armor=1,Strength=2,Cloak=3")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("NanoSuit ModeControl");
		config.SetCategory(EFLN_WIP);
	}


	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			break;

		case eFE_Activate:
			{
				if (pActInfo->pEntity == 0)
					return;
				const bool bAdd = IsPortActive(pActInfo, EIP_Add);
				const bool bRemove = IsPortActive(pActInfo, EIP_Remove);
				const bool bDefect = IsPortActive(pActInfo, EIP_Defect);
				const bool bRepair = IsPortActive(pActInfo, EIP_Repair);
				if (bAdd || bRemove || bDefect || bRepair)
				{
					static const int PortInt2ModeMapping[]= { NANOMODE_SPEED, NANOMODE_DEFENSE, NANOMODE_STRENGTH, NANOMODE_CLOAK };

					CPlayer* pPlayer = GetPlayer(pActInfo->pEntity->GetId());
					if (pPlayer == 0)
						return;
					CNanoSuit *pSuit = pPlayer->GetNanoSuit();
					if(!pSuit)
						return;
					int mode = GetPortInt(pActInfo, EIP_Mode);
					if (mode < 0 || mode >= sizeof(PortInt2ModeMapping)/sizeof(PortInt2ModeMapping[0]))
					{
						GameWarning("[flow] CFlowNanoSuitControlNode: Illegal mode %d", mode);
						return;
					}
					if (bAdd)
						pSuit->ActivateMode((ENanoMode)PortInt2ModeMapping[mode], true);
					else if (bRemove)
						pSuit->ActivateMode((ENanoMode)PortInt2ModeMapping[mode], false);
					if (bDefect)
						pSuit->SetModeDefect((ENanoMode)PortInt2ModeMapping[mode], true);
					else if (bRepair)
						pSuit->SetModeDefect((ENanoMode)PortInt2ModeMapping[mode], false);
				}
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowParachuteControlNode : public CFlowBaseNode
{
public:

	CFlowParachuteControlNode( SActivationInfo * pActInfo )
	{
	}

	~CFlowParachuteControlNode()
	{
	}

	/*
	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return this;
	}
	*/

	enum EInputPorts
	{
		EIP_Open = 0,
		EIP_Close,
		EIP_Remove,
		EIP_FreeFall,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void  ("Open", _HELP("Trigger to open")),
			InputPortConfig_Void  ("Close", _HELP("Trigger to close")),
			InputPortConfig_Void  ("Remove", _HELP("Trigger to remove")),
			InputPortConfig_Void  ("FreeFall", _HELP("Trigger to force Player into FreeFall state")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Parachute ModeControl");
		config.SetCategory(EFLN_WIP);
	}


	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (pActInfo->pEntity == 0)
					return;
				CPlayer* pPlayer = GetPlayer(pActInfo->pEntity->GetId());
				if (pPlayer == 0)
					return;
				int8 newState = -1;
				if(IsPortActive(pActInfo, EIP_Open))
					newState = 2;
				else if(IsPortActive(pActInfo, EIP_Close))
					newState = 1;
				else if(IsPortActive(pActInfo, EIP_Remove))
				{
					newState = 0;
					pPlayer->EnableParachute(false);
				}
	
				if (newState != -1)
					pPlayer->ChangeParachuteState(newState);

				if (IsPortActive(pActInfo, EIP_FreeFall))
					pPlayer->ForceFreeFall();

			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowNanoSuitFakeMaterial : public CFlowBaseNode
{
public:
	CFlowNanoSuitFakeMaterial( SActivationInfo * pActInfo )
	{
	}

	~CFlowNanoSuitFakeMaterial()
	{
	}

	enum EInputPorts
	{
		EIP_Asian = 0,
		EIP_Cloak,
		EIP_Strength,
		EIP_Defense,
		EIP_Speed,
	};

	enum EOutputPorts
	{
		EOP_Done = 0,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<bool> ("Asian", false, _HELP("If true, use Asian material, otherwise US")),
			InputPortConfig_Void  ("Cloak", _HELP("Trigger to select Cloak Mode")),
			InputPortConfig_Void  ("Strength", _HELP("Trigger to select Strength Mode")),
			InputPortConfig_Void  ("Defense", _HELP("Trigger to select Defense Mode")),
			InputPortConfig_Void  ("Speed", _HELP("Trigger to select Speed Mode")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("Done", _HELP("Triggered when Done.")),
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Fake Materials on Characters (non Player/AI) NanoSuit for Cinematics");
		config.SetCategory(EFLN_WIP);
	}


	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			{
				IEntity* pEntity = pActInfo->pEntity;
				if (pEntity != 0)
				{
					ENanoMode nanoMode = NANOMODE_LAST;
					if (IsPortActive(pActInfo, EIP_Cloak))
						nanoMode = NANOMODE_CLOAK;
					else if (IsPortActive(pActInfo, EIP_Speed))
						nanoMode = NANOMODE_SPEED;
					else if (IsPortActive(pActInfo, EIP_Strength))
						nanoMode = NANOMODE_STRENGTH;
					else if (IsPortActive(pActInfo, EIP_Defense))
						nanoMode = NANOMODE_DEFENSE;
					if (nanoMode != NANOMODE_LAST)
					{
						const bool bAsian = GetPortBool(pActInfo, EIP_Asian);
						CNanoSuit::SNanoMaterial* pNanoMat = CNanoSuit::GetNanoMaterial(nanoMode, bAsian);
						if (pNanoMat)
						{
							CNanoSuit::AssignNanoMaterialToEntity(pEntity, pNanoMat);
							ActivateOutput(pActInfo, EOP_Done, true);
						}
					}
				}
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


REGISTER_FLOW_NODE("NanoSuit:NanoSuit", CFlowNanoSuitNode);
REGISTER_FLOW_NODE_SINGLETON("NanoSuit:NanoSuitGet", CFlowNanoSuitGetNode);
REGISTER_FLOW_NODE_SINGLETON("NanoSuit:ModeControl", CFlowNanoSuitControlNode);
REGISTER_FLOW_NODE_SINGLETON("NanoSuit:FakeMaterial", CFlowNanoSuitFakeMaterial);

REGISTER_FLOW_NODE_SINGLETON("Inventory:ParachuteControl", CFlowParachuteControlNode);

#if defined(WHOLE_PROJECT)
	#undef GetPlayer
#endif

