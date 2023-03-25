// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "Game.h"

#include "HUD/HUD.h"
#include "Nodes/G2FlowBaseNode.h"
#include "Player.h"
#include "GameCVars.h"

#include <IVehicleSystem.h>
#include <StringUtils.h>

#if defined(WHOLE_PROJECT)
	#define GetPlayer GetPlayerSensor
#endif

CPlayer* GetPlayer(EntityId id)
{
	if (id == 0)
		return 0;
	CActor* pActor = static_cast<CActor*> (g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(id));
	if (pActor != 0 && pActor->GetActorClass() == CPlayer::GetActorClassType())
		return static_cast<CPlayer*> (pActor);
	return 0;
}

IVehicle* GetVehicle(EntityId id)
{
	if (id == 0)
		return 0;
	return g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(id);
}

class CFlowNode_ActorSensor : public CFlowBaseNode, public IPlayerEventListener, public IVehicleEventListener
{
public:
	CFlowNode_ActorSensor( SActivationInfo * pActInfo ) : m_entityId(0), m_vehicleId(0)
	{
	}

	~CFlowNode_ActorSensor()
	{
		UnRegisterActor();
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_ActorSensor(pActInfo);
	}

	void Serialize(SActivationInfo* pActivationInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			UnRegisterActor();
		}
		ser.Value("m_entityId", m_entityId);
		ser.Value("m_vehicleId", m_vehicleId);
		if (ser.IsReading())
		{
			if (m_entityId != 0)
			{
				RegisterActor();
			}
			if (m_vehicleId != 0)
			{
				IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(m_vehicleId);
				if (pVehicle)
				{
					pVehicle->RegisterVehicleEventListener(this, "CFlowNode_ActorSensor");
				}
				else
					m_vehicleId = 0;
			}
		}
	}

	enum OUTS
	{
		EOP_ENTER = 0,
		EOP_EXIT,
		EOP_SEAT,
		EOP_ITEMPICKEDUP,
		EOP_ITEMDROPPED,
		EOP_ITEMUSED,
		EOP_NPCGRABBED,
		EOP_NPCTHROWN,
		EOP_OBJECTGRABBED,
		EOP_OBJECTTHROWN,
		EOP_STANCECHANGED,
		EOP_SPECIALMOVE,
		EOP_ONDEATH,
	};

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Enable", _HELP("Trigger to enable")),
			InputPortConfig_Void( "Disable", _HELP("Trigger to enable")),
			{0}
		};
		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig<EntityId> ( "EnterVehicle", _HELP("Triggered when entering a vehicle")),
			OutputPortConfig<EntityId> ( "ExitVehicle", _HELP("Triggered when exiting a vehicle")),
			OutputPortConfig<int>      ( "SeatChange", _HELP("Triggered when seat has changed")),
			OutputPortConfig<EntityId> ( "ItemPickedUp", _HELP("Triggered when an item is picked up")),
			OutputPortConfig<EntityId> ( "ItemDropped", _HELP("Triggered when an item is dropped")),
			OutputPortConfig<EntityId> ( "ItemUsed", _HELP("Triggered when an item is used")),
			OutputPortConfig<EntityId> ( "NPCGrabbed", _HELP("Triggered when an NPC is grabbed")),
			OutputPortConfig<EntityId> ( "NPCThrown", _HELP("Triggered when an NPC is thrown")),
			OutputPortConfig<EntityId> ( "ObjectGrabbed", _HELP("Triggered when an object is grabbed")),
			OutputPortConfig<EntityId> ( "ObjectThrown", _HELP("Triggered when an object is thrown")),
			OutputPortConfig<int>      ( "StanceChanged", _HELP("Triggered when Stance changed. 0=Stand,1=Crouch,2=Prone,3=Relaxed,4=Stealth,5=Swim,6=ZeroG")),
			OutputPortConfig<int>      ( "SpecialMove", _HELP("Triggered On SpecialMove. 0=Jump,1=SpeedSprint,2=StrengthJump")),
			OutputPortConfig<int>      ( "OnDeath", _HELP("Triggered when Actor dies. Outputs 0 if not god. 1 if god.")),
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Tracks the attached Entity and its Vehicle-related actions");
		config.SetCategory(EFLN_WIP);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;
			break;
		case eFE_SetEntityId:
			break;
		case eFE_Activate:
			if (IsPortActive(pActInfo, 1))
				UnRegisterActor();

			if (IsPortActive(pActInfo, 0))
			{
				UnRegisterActor();
				m_entityId = pActInfo->pEntity != 0 ? pActInfo->pEntity->GetId() : 0;
				RegisterActor();
			}
			break;
		}
	}

	
	void RegisterActor()
	{
		if (m_entityId == 0)
			return;
		CPlayer* pPlayer = GetPlayer(m_entityId);
		if (pPlayer != 0)
		{
			pPlayer->RegisterPlayerEventListener(this);
			return;
		}
		m_entityId = 0;
	}

	void UnRegisterActor()
	{
		if (m_entityId == 0)
			return;

		CPlayer* pPlayer = GetPlayer(m_entityId);
		if (pPlayer != 0)
			pPlayer->UnregisterPlayerEventListener(this);
		m_entityId = 0;

		IVehicle* pVehicle = GetVehicle(m_vehicleId);
		if (pVehicle != 0)
			pVehicle->UnregisterVehicleEventListener(this);
		m_vehicleId = 0;
	}

	// IPlayerEventListener
	virtual void OnEnterVehicle(IActor *pActor,const char *strVehicleClassName,const char *strSeatName,bool bThirdPerson)
	{
		if (pActor->GetEntityId() != m_entityId)
			return;
		CPlayer* pPlayer = static_cast<CPlayer*> (pActor);
		if(m_vehicleId)
		{
			if(IVehicle *pVehicle = GetVehicle(m_vehicleId))
				pVehicle->UnregisterVehicleEventListener(this);
		}
		IVehicle* pVehicle = pPlayer->GetLinkedVehicle();
		if (pVehicle == 0)
			return;
		pVehicle->RegisterVehicleEventListener(this, "CFlowNode_ActorSensor");
		m_vehicleId = pVehicle->GetEntityId();
		ActivateOutput(&m_actInfo, EOP_ENTER, m_vehicleId);
		IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger(m_entityId);
		if (pSeat)
			ActivateOutput(&m_actInfo, EOP_SEAT, pSeat->GetSeatId());
	}

	virtual void OnExitVehicle(IActor *pActor)
	{
		if (pActor->GetEntityId() != m_entityId)
			return;
		CPlayer* pPlayer = static_cast<CPlayer*> (pActor);

		IVehicle* pVehicle = GetVehicle(m_vehicleId);
		if (pVehicle == 0)
		{
			m_vehicleId = 0;
			return;
		}
		ActivateOutput(&m_actInfo, EOP_EXIT, m_vehicleId);
		pVehicle->UnregisterVehicleEventListener(this);
		m_vehicleId = 0;
	}

	virtual void OnToggleThirdPerson(IActor *pActor,bool bThirdPerson)
	{

	}

	virtual void OnItemDropped(IActor* pActor, EntityId itemId)
	{
		ActivateOutput(&m_actInfo, EOP_ITEMDROPPED, itemId);
	}
	virtual void OnItemUsed(IActor* pActor, EntityId itemId)
	{
		ActivateOutput(&m_actInfo, EOP_ITEMUSED, itemId);
	}
	virtual void OnItemPickedUp(IActor* pActor, EntityId itemId)
	{
		ActivateOutput(&m_actInfo, EOP_ITEMPICKEDUP, itemId);
	}
	virtual void OnStanceChanged(IActor* pActor, EStance stance)
	{
		ActivateOutput(&m_actInfo, EOP_STANCECHANGED, static_cast<int> (stance));
	}
	virtual void OnSpecialMove(IActor* pActor, ESpecialMove move)
	{
		ActivateOutput(&m_actInfo, EOP_SPECIALMOVE, static_cast<int> (move));
	}
	virtual void OnDeath(IActor* pActor, bool bIsGod)
	{
		ActivateOutput(&m_actInfo, EOP_ONDEATH, bIsGod ? 1 : 0);
	}
	virtual void OnObjectGrabbed(IActor* pActor, bool bIsGrab, EntityId objectId, bool bIsNPC, bool bIsTwoHanded)
	{
		if (bIsGrab)
			ActivateOutput(&m_actInfo, bIsNPC ? EOP_NPCGRABBED : EOP_OBJECTGRABBED, objectId);
		else
			ActivateOutput(&m_actInfo, bIsNPC ? EOP_NPCTHROWN : EOP_OBJECTTHROWN, objectId);
	}
	// ~IPlayerEventListener

	// IVehicleEventListener
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
	{
		if (event == eVE_VehicleDeleted)
		{
			IVehicle* pVehicle = GetVehicle(m_vehicleId);
			if (pVehicle == 0)
			{
				m_vehicleId = 0;
				return;
			}
			pVehicle->UnregisterVehicleEventListener(this);
			m_vehicleId = 0;
		}
		else if (event == eVE_PassengerChangeSeat)
		{
			if (params.entityId == m_entityId)
			{
				ActivateOutput(&m_actInfo, EOP_SEAT, params.iParam); // seat id
			}
		}
	}
	// ~IVehicleEventListener

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

protected:
	EntityId m_entityId;
	EntityId m_vehicleId;
	SActivationInfo m_actInfo;
};

class CFlowNode_DifficultyLevel : public CFlowBaseNode 
{
	enum INPUTS
	{
		EIP_Trigger = 0,
	};

	enum OUTPUTS
	{
		EOP_Easy = 0,
		EOP_Medium,
		EOP_Hard,
		EOP_Delta
	};

public:
	CFlowNode_DifficultyLevel( SActivationInfo * pActInfo ) { }

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void  ( "Trigger", _HELP("Trigger to get difficulty level." )),
			{0}
		};
		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void  ( "Easy", _HELP("Easy") ),
			OutputPortConfig_Void  ( "Normal", _HELP("Normal") ),
			OutputPortConfig_Void  ( "Hard", _HELP("Hard") ),
			OutputPortConfig_Void  ( "Delta", _HELP("Delta") ),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Get difficulty level.");
		config.SetCategory(EFLN_WIP);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if(eFE_Activate == event && IsPortActive(pActInfo, EIP_Trigger))
		{
			const int level = g_pGameCVars->g_difficultyLevel; // [0] or 1-4
			if (level > EOP_Easy && level <= EOP_Delta)
			{
				ActivateOutput(pActInfo, level-1, true);
			}
		}
	}
};

// THIS FLOWNODE IS A SINGLETON, although it has member variables!
// THIS IS INTENDED!!!!
class CFlowNode_OverrideFOV : public CFlowBaseNode 
{
	enum INPUTS
	{
		EIP_SetFOV = 0,
		EIP_GetFOV,
		EIP_ResetFOV
	};

	enum OUTPUTS
	{
		EOP_CurFOV = 0,
		EOP_ResetDone,
	};

public:
	CFlowNode_OverrideFOV( SActivationInfo * pActInfo ) { m_storedFOV = 0.0f; }

	~CFlowNode_OverrideFOV()
	{
		if (m_storedFOV > 0.0f)
		{
			ICVar* pCVar = (gEnv && gEnv->pConsole) ? gEnv->pConsole->GetCVar("cl_fov") : 0;
			if (pCVar)
				pCVar->Set(m_storedFOV);
		}
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig<float> ( "SetFOV", _HELP("Trigger to override Camera's FieldOfView." )),
			InputPortConfig_Void   ( "GetFOV", _HELP("Trigger to get current Camera's FieldOfView." )),
			InputPortConfig_Void   ( "ResetFOV", _HELP("Trigger to reset FieldOfView to default value." )),
			{0}
		};
		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig<float>  ( "CurFOV", _HELP("Current FieldOfView") ),
			OutputPortConfig_Void    ( "ResetDone", _HELP("Triggered after Reset") ),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Override Camera's FieldOfView. Cutscene ONLY!");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ICVar* pCVar = gEnv->pConsole->GetCVar("cl_fov");

		if (ser.IsWriting())
		{
			float curFOV = 0.0f;

			// in case we're currently active, store current value as well
			if (m_storedFOV > 0.0f && pCVar)
				curFOV = pCVar->GetFVal();

			ser.Value("m_storedFOV", m_storedFOV);
			ser.Value("curFOV", curFOV);
		}
		else
		{
			float storedFOV = 0.0f;
			float curFOV = 0.0f;
			ser.Value("m_storedFOV", storedFOV);
			ser.Value("curFOV", curFOV);

			// if we're currently active, restore first
			if(m_storedFOV > 0.0f)
			{
				if (pCVar)
					pCVar->Set(m_storedFOV);
				m_storedFOV = 0.0f;
			}

			m_storedFOV = storedFOV;
			if (m_storedFOV > 0.0f && curFOV > 0.0f && pCVar)
				pCVar->Set(curFOV);
		}
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		ICVar* pCVar = gEnv->pConsole->GetCVar("cl_fov");
		if (pCVar == 0)
			return;

		switch (event)
		{
		case eFE_Initialize:
			if (m_storedFOV > 0.0f)
			{
				pCVar->Set(m_storedFOV);
				m_storedFOV = 0.0f;
			}
			break;

		case eFE_Activate:
			// get fov
			if (IsPortActive(pActInfo, EIP_GetFOV))
				ActivateOutput(pActInfo, EOP_CurFOV, pCVar->GetFVal());

			// set fov (store backup if not already done)
			if (IsPortActive(pActInfo, EIP_SetFOV))
			{
				if (m_storedFOV <= 0.0f)
					m_storedFOV = pCVar->GetFVal();
				pCVar->Set(GetPortFloat(pActInfo, EIP_SetFOV));
			}
			if (IsPortActive(pActInfo, EIP_ResetFOV))
			{
				if (m_storedFOV > 0.0f)
				{
					pCVar->Set(m_storedFOV);
					m_storedFOV = 0.0f;
				}
			}
			break;
		}
	}

	float m_storedFOV;
};

REGISTER_FLOW_NODE("Game:ActorSensor",	CFlowNode_ActorSensor);
REGISTER_FLOW_NODE_SINGLETON("Game:DifficultyLevel",	CFlowNode_DifficultyLevel);
REGISTER_FLOW_NODE_SINGLETON("Camera:OverrideFOV",	CFlowNode_OverrideFOV); // Intended: Singleton although contains members!

#if defined(WHOLE_PROJECT)
	#undef GetPlayer
#endif

