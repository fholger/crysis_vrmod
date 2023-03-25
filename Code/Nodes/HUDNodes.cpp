// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include "GameActions.h"

#include "HUD/HUD.h"
#include "HUD/HUDRadar.h"
#include "HUD/HUDScopes.h"
#include "HUD/HUDSilhouettes.h"
#include "Nodes/G2FlowBaseNode.h"
#include "Weapon.h"

#include <StringUtils.h>

namespace
{
	// to be replaced. hud is no longer a game object 
	void SendHUDEvent( SGameObjectEvent& evt )
	{
		SAFE_HUD_FUNC(HandleEvent(evt));
	}
}

class CFlowNode_PDAMessage : public CFlowBaseNode
{
public:
	CFlowNode_PDAMessage( SActivationInfo * pActInfo )
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "display", _HELP("Connect event here to display the objective" )),
			InputPortConfig<string>( "objective", _HELP("This is the key, text or mission objective identifier." )),
			InputPortConfig<string>( "MessageType", _HELP("Here the message type can be selected: <[empty]> for mission objective, <demo> for demo-mode message, <text> for plain text message")),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_LEGACY);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if(eFE_Activate == event && IsPortActive(pActInfo,0))
		{
			//SGameObjectEvent evt(GetPortString(pActInfo,1).c_str(),eGOEF_ToAll);

			const string& type = GetPortString(pActInfo,2);

			if(type.empty())
			{
				SGameObjectEvent evt(eCGE_HUD_PDAMessage, eGOEF_ToAll, IGameObjectSystem::InvalidExtensionID, (void*) GetPortString(pActInfo,1).c_str());
				SendHUDEvent(evt);
			}
			else if(!strcmp(type.c_str(), "text"))
			{
				SGameObjectEvent evt(eCGE_HUD_TextMessage,eGOEF_ToAll, IGameObjectSystem::InvalidExtensionID, (void*) (GetPortString(pActInfo, 1).c_str()));
				SendHUDEvent(evt);
			}
		}
	}
};

class CFlowNode_HUDEvent : public CFlowBaseNode 
{
public:
	CFlowNode_HUDEvent( SActivationInfo * pActInfo ) { }

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Trigger", _HELP("Trigger this port to send the event to the HUD" )),
			InputPortConfig<string>( "EventName", _HELP("Name of event to send to the HUD" )),
			InputPortConfig<string>( "EventParam", _HELP("Parameter of the event [event-specific]" )),			
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.sDescription = _HELP("Sends an event to the HUD system. EventParam is an event-specific string");
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if(eFE_Activate == event && IsPortActive(pActInfo,0))
		{
			string hudEvent;
			const string& eventName = GetPortString(pActInfo, 1);
			if (CryStringUtils::stristr(eventName.c_str(), "HUD_") == 0) // prefix with HUD_ if not done already
			{
				hudEvent.assign("HUD_");
			}
			hudEvent+=eventName;

			uint32 eventId = g_pGame->GetIGameFramework()->GetIGameObjectSystem()->GetEventID(hudEvent.c_str());
			if (eventId != IGameObjectSystem::InvalidEventID)
			{
				SGameObjectEvent evt(eventId,eGOEF_ToAll, IGameObjectSystem::InvalidExtensionID, (void*) (GetPortString(pActInfo, 2).c_str()));
				SendHUDEvent(evt);
			}
		}
	}
};

class CFlowNode_PDAControl : public CFlowBaseNode, public CHUD::IHUDListener
{
public:
	CFlowNode_PDAControl( SActivationInfo * pActInfo )
	{
	}

	~CFlowNode_PDAControl()
	{
		SAFE_HUD_FUNC(UnRegisterListener(this));
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_PDAControl(pActInfo);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	enum INPUTS
	{
		EIP_Open = 0,
		EIP_Close,
		EIP_Tab,
		EIP_ShowObj,
		EIP_HideObj,
	};

	enum OUTPUTS
	{
		EOP_Opened = 0,
		EOP_Closed,
		EOP_TabChanged,
		EOP_ObjOpened,
		EOP_ObjClosed,
	};

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Open",  _HELP("Open Map"), _HELP("OpenMap")),
			InputPortConfig_Void( "Close", _HELP("Close Map"), _HELP("CloseMap") ),
			InputPortConfig<int>( "Tab", -1, _HELP("Tab to open."), 0, _UICONFIG("enum_int:Current=-1,Map=1")),
			InputPortConfig_Void( "ShowObjectives", _HELP("Show SP Objectives") ),
			InputPortConfig_Void( "HideObjectives", _HELP("Hide SP Objecvies") ),
			{0}
		};
		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void( "Opened", _HELP("Triggered when PDA was opened"), _HELP("MapOpened")),
			OutputPortConfig_Void( "Closed", _HELP("Triggered when PDA was closed"), _HELP("MapClosed")),
			OutputPortConfig<int> ("TabChanged", _HELP("Triggered when PDA Tab changed. Map=1")),
			OutputPortConfig_Void ("ObjectivesOpened", _HELP("Triggered when Objectives are shown")),
			OutputPortConfig_Void ("ObjectivesClosed", _HELP("Triggered when Objectives are hidden again")),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Control the PDA/Map");
		config.SetCategory(EFLN_WIP);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				SAFE_HUD_FUNC(RegisterListener(this));
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Open))
				{
					SAFE_HUD_FUNC(ShowPDA(true, (GetPortInt(pActInfo, 2)==1)?false:true));
				}
				if (IsPortActive(pActInfo, EIP_Close))
				{
					SAFE_HUD_FUNC(ShowPDA(false));
				}
				if (IsPortActive(pActInfo, EIP_ShowObj))
				{
					SAFE_HUD_FUNC(ShowObjectives(true)); // calls us back
				}
				if (IsPortActive(pActInfo, EIP_HideObj))
				{
					SAFE_HUD_FUNC(ShowObjectives(false)); // calls us back
				}
			}
			break;
		}
	}

protected:
	virtual void HUDDestroyed() {}
	virtual void PDAOpened()
	{
		ActivateOutput(&m_actInfo, EOP_Opened, true);
	}

	virtual void PDAClosed()
	{
		ActivateOutput(&m_actInfo, EOP_Closed, true);
	}

	virtual void PDATabChanged(int tab)
	{
		ActivateOutput(&m_actInfo, EOP_TabChanged, tab);
	}

	virtual void OnShowObjectives(bool bOpen)
	{
		const int port = bOpen ? EOP_ObjOpened : EOP_ObjClosed;
		ActivateOutput(&m_actInfo, port, true);
	}
	SActivationInfo m_actInfo;
};

class CFlowNode_ShowHUDMessage : public CFlowBaseNode 
{
	enum INPUTS
	{
		EIP_Trigger = 0,
		EIP_Message,
		EIP_Pos,
		EIP_Color,
		EIP_Time
	};

public:
	CFlowNode_ShowHUDMessage( SActivationInfo * pActInfo ) { }

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void   ( "Trigger", _HELP("Trigger this port to show the message") ),
			InputPortConfig<string>( "text_Message", _HELP("Localized label of message to display") ),
			InputPortConfig<int>   ( "Pos", 1, _HELP("Position where to show" ), 0, _UICONFIG("enum_int:Top=1,Middle=2,Bottom=3") ),			
			InputPortConfig<Vec3>  ( "clr_Color", Vec3(1.0f,1.0f,1.0f), _HELP("Color") ),
			InputPortConfig<float> ( "Time", 3.0f, _HELP("If Pos is Middle: Time to show the message. 0.0 -> forever until cleared") ),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.sDescription = _HELP("Displays a localized message on the Flash-HUD.");
		config.SetCategory(EFLN_WIP);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if(eFE_Activate == event && IsPortActive(pActInfo,EIP_Trigger))
		{
			const string& msg = GetPortString(pActInfo, EIP_Message);
			const Vec3& colorVec = GetPortVec3(pActInfo, EIP_Color);
			int pos = GetPortInt(pActInfo, EIP_Pos);
			if (pos < 1 || pos > 3)
				pos = 1;
			if (pos == 2)
			{
				const float time = GetPortFloat(pActInfo, EIP_Time);
				SAFE_HUD_FUNC(DisplayTempFlashText(msg, time <= 0.0f ? -1.0f : time, ColorF(colorVec, 1.0f)));
			}
			else
				SAFE_HUD_FUNC(DisplayFlashMessage(msg, pos, ColorF(colorVec, 1.0f)));
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowNode_HUDControl : public CFlowBaseNode 
{
	enum INPUTS
	{
		EIP_Show = 0,
		EIP_Hide,
		EIP_Boot,
		EIP_Break,
		EIP_Reboot,
		EIP_AlienInterference,
		EIP_AlienInterferenceStrength,
		EIP_MapNotAvailable
	};

public:
	CFlowNode_HUDControl( SActivationInfo * pActInfo ) { }

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void   ( "Show",  _HELP("Trigger this port to show the HUD") ),
			InputPortConfig_Void   ( "Hide",  _HELP("Trigger this port to hide the HID") ),
			InputPortConfig_Void   ( "BootSeq",  _HELP("Trigger this port to show the Boot Sequence") ),
			InputPortConfig_Void   ( "BreakSeq", _HELP("Trigger this port to show the Break Sequence") ),
			InputPortConfig_Void   ( "RebootSeq", _HELP("Trigger this port to show the Reboot Sequence") ),
			InputPortConfig<bool>  ( "AlienInterference", false, _HELP("Trigger with boolean to enable/disable Alien interference effect") ),
			InputPortConfig<float> ( "AlienInterferenceStrength", 1.0f, _HELP("Strength of Alien interference effect") ),
			InputPortConfig<bool>  ( "MapNotAvailable", false, _HELP("Trigger with boolean to enable/disable an animation when Minimap does not exist") ),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.sDescription = _HELP("Trigger some HUD-specific visual effects.");
		config.SetCategory(EFLN_WIP);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if(eFE_Activate == event)
		{
			if (IsPortActive(pActInfo, EIP_Show))
				SAFE_HUD_FUNC(Show(true));
			if (IsPortActive(pActInfo, EIP_Hide))
				SAFE_HUD_FUNC(Show(false));
			if (IsPortActive(pActInfo, EIP_Boot))
				SAFE_HUD_FUNC(ShowBootSequence());
			if (IsPortActive(pActInfo, EIP_Break))
				SAFE_HUD_FUNC(BreakHUD());
			if (IsPortActive(pActInfo, EIP_Reboot))
				SAFE_HUD_FUNC(RebootHUD());
			if (IsPortActive(pActInfo, EIP_AlienInterference))
				g_pGameCVars->hud_enableAlienInterference = GetPortBool(pActInfo, EIP_AlienInterference);
			if (IsPortActive(pActInfo, EIP_AlienInterferenceStrength))
				g_pGameCVars->hud_alienInterferenceStrength = GetPortFloat(pActInfo, EIP_AlienInterferenceStrength);
			if (IsPortActive(pActInfo, EIP_MapNotAvailable))
				SAFE_HUD_FUNC(SetMinimapNotAvailable(GetPortBool(pActInfo, EIP_MapNotAvailable)));
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

class CFlowNode_PDAScanner : public CFlowBaseNode 
{
	enum INPUTS
	{
		EIP_StartScan = 0,
		EIP_StopScan,
		EIP_Pos,
		EIP_Radius,
		EIP_KeepExisting,
	};

public:
	CFlowNode_PDAScanner( SActivationInfo * pActInfo ) { }

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void  ( "Start", _HELP("Trigger this port to start scanning" )),
			InputPortConfig_Void  ( "Stop",  _HELP("Trigger this port to stop scanning" )),
			InputPortConfig<Vec3> ( "Pos", Vec3(ZERO), _HELP("Origin of scanning" )),
			InputPortConfig<float>( "Radius", 100.0f, _HELP("Radius for scanning" )),
			InputPortConfig<bool> ( "KeepExisting", true, _HELP("Keep already scanned entities on the radar." )),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.sDescription = _HELP("HUD/PDA Entity Scanning");
		config.SetCategory(EFLN_WIP);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if(eFE_Activate == event)
		{
			CHUDRadar* pRadar = SAFE_HUD_FUNC_RET(GetRadar());
			if (pRadar == 0)
				return;
			if (IsPortActive(pActInfo, EIP_StopScan))
				pRadar->StopBroadScan();
			if (IsPortActive(pActInfo, EIP_StartScan))
			{
				const float fRadius = GetPortFloat(pActInfo, EIP_Radius);
				const bool bKeepExisting = GetPortBool(pActInfo, EIP_KeepExisting);
				const Vec3 pos = GetPortVec3(pActInfo, EIP_Pos);
				pRadar->StartBroadScan(true, bKeepExisting, pos, fRadius);
			}
		}
	}
};

class CFlowNode_HUDAirstrike : public CFlowBaseNode, public CHUD::IHUDListener
{
	static const int NUM_ENTITIES = 8;
	enum INPUTS
	{
		EIP_Enable = 0,
		EIP_Disable,
		EIP_Succeeded,
		EIP_Failed,
		EIP_Entity0 = EIP_Failed+1,
		EIP_EntityMax = EIP_Entity0+NUM_ENTITIES,
	};

	enum OUTPUTS
	{
		EOP_StartAirstrike = 0,
		EOP_CancelAirstrike,
	};

public:
	CFlowNode_HUDAirstrike( SActivationInfo * pActInfo )
	{
	}

	~CFlowNode_HUDAirstrike()
	{
		SAFE_HUD_FUNC(UnRegisterListener(this));
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_HUDAirstrike(pActInfo);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Enable",  _HELP("Enable Airstrike"  )),
			InputPortConfig_Void( "Disable", _HELP("Disable Airstrike" )),
			InputPortConfig_Void( "Succeeded", _HELP("Notify HUD that Airstrike succeeded" )),
			InputPortConfig_Void( "Failed", _HELP("Notify HUD that Airstrike failed" )),
			InputPortConfig<EntityId> ( "EntityId_1", _HELP("Potential Target Entity 1" )),
			InputPortConfig<EntityId> ( "EntityId_2", _HELP("Potential Target Entity 2" )),
			InputPortConfig<EntityId> ( "EntityId_3", _HELP("Potential Target Entity 3" )),
			InputPortConfig<EntityId> ( "EntityId_4", _HELP("Potential Target Entity 4" )),
			InputPortConfig<EntityId> ( "EntityId_5", _HELP("Potential Target Entity 5" )),
			InputPortConfig<EntityId> ( "EntityId_6", _HELP("Potential Target Entity 6" )),
			InputPortConfig<EntityId> ( "EntityId_7", _HELP("Potential Target Entity 7" )),
			InputPortConfig<EntityId> ( "EntityId_8", _HELP("Potential Target Entity 8" )),
			{0}
		};
		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig<EntityId> ( "StartAirstrike",  _HELP("Triggered with EntityId to start Airstrike" )),
			OutputPortConfig<EntityId> ( "CancelAirstrike",  _HELP("Triggered with EntityId to cancel Airstrike" )),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("HUD Airstrike Control");
		config.SetCategory(EFLN_WIP);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				SAFE_HUD_FUNC(RegisterListener(this));
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Enable))
				{
					SAFE_HUD_FUNC(ClearAirstrikeEntities());
					for (int i=EIP_Entity0; i<EIP_EntityMax; ++i)
					{
						SAFE_HUD_FUNC(AddAirstrikeEntity(GetPortEntityId(pActInfo, i)));
					}
					SAFE_HUD_FUNC(SetAirStrikeEnabled(true));
				}
				if (IsPortActive(pActInfo, EIP_Disable))
				{
					SAFE_HUD_FUNC(SetAirStrikeEnabled(false));
				}
				if (IsPortActive(pActInfo, EIP_Succeeded))
				{
					SAFE_HUD_FUNC(NotifyAirstrikeSucceeded(true));
				}
				if (IsPortActive(pActInfo, EIP_Failed))
				{
					SAFE_HUD_FUNC(NotifyAirstrikeSucceeded(false));
				}

			}
			break;
		}
	}

protected:
	virtual void HUDDestroyed() {}
	virtual void OnAirstrike(int mode, EntityId entityId) // mode: 0=stop, 1=start
	{
		if (mode == 0)
			ActivateOutput(&m_actInfo, EOP_CancelAirstrike, entityId);
		else if (mode == 1)
			ActivateOutput(&m_actInfo, EOP_StartAirstrike, entityId);
	}

	SActivationInfo m_actInfo;
};

class CFlowNode_HUDRadar : public CFlowBaseNode 
{
	enum INPUTS
	{
		EIP_Add = 0,
		EIP_Remove,
		EIP_AddTeamMate,
		EIP_RemoveTeamMate,
	};
	enum OUTPUTS
	{
		EOP_Done = 0,
	};

public:
	CFlowNode_HUDRadar( SActivationInfo * pActInfo ) { }

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "AddToRadar",    _HELP("Trigger this port to force adding entity to radar" )),
			InputPortConfig_Void( "RemoveFromRadar", _HELP("Trigger this port to force removing entity from radar" )),
			InputPortConfig_Void( "AddTeamMate",    _HELP("Trigger this port to add TeamMate entity to radar" )),
			InputPortConfig_Void( "RemoveTeamMate", _HELP("Trigger this port to remove TeamMate entity from radar" )),
			{0}
		};
		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void( "Done", _HELP("Triggered when done.")),
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Adds or removes a TeamMate/Normal entity from the Radar.");
		config.SetCategory(EFLN_WIP);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if(eFE_Activate == event && pActInfo->pEntity)
		{
			CHUDRadar* pRadar = SAFE_HUD_FUNC_RET(GetRadar());
			if (pRadar)
			{
				const EntityId entityId = pActInfo->pEntity->GetId();
				if (IsPortActive(pActInfo, EIP_Remove))
					pRadar->RemoveFromRadar(entityId);
				if (IsPortActive(pActInfo, EIP_Add))
					pRadar->AddEntityToRadar(entityId);
				if (IsPortActive(pActInfo, EIP_RemoveTeamMate))
					pRadar->SetTeamMate(entityId, false);
				if (IsPortActive(pActInfo, EIP_AddTeamMate))
					pRadar->SetTeamMate(entityId, true);
				ActivateOutput(pActInfo, EOP_Done, true);
			}
		}
	}
};


class CFlowNode_HUDMapInfo : public CFlowBaseNode 
{
	enum INPUTS
	{
		EIP_Add = 0,
		EIP_Remove,
		EIP_Label,
		EIP_Type,
	};

	enum OUTPUTS
	{
		EOP_Added = 0,
		EOP_Removed,
	};

public:
	CFlowNode_HUDMapInfo( SActivationInfo * pActInfo ) { }

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const char* uiConfig =
			"enum_int:Tank=1,APC=2,CivilCar=3,Truck=4,Hovercraft=5,SpeedBoat=6,"
			"PatrolBoat=7,SmallBoat=8,LTV=9,Heli=10,VTOL=11,AAA=12,NuclearWeapon=14,TechCharger=15,"
			"WayPoint=16,Player=17,TaggedEntity=18,SpawnPoint=19,FactoryAir=20,FactoryTank=21,"
			"FactoryPrototype=22,FactoryVehicle=23,FactorySea=24,Barracks=26,SpawnTruck=27,"
			"AmmoTruck=28,HQ2=29,HQ1=30,AmmoDepot=31,MineField=32,MachineGun=33,Canalization=34,Tutorial=35,SecretEntrance=36";

		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Add",    _HELP("Trigger this port to add Entity to map" )),
			InputPortConfig_Void( "Remove", _HELP("Trigger this port to remove Entity from map" )),
			InputPortConfig<string> ("Label", _HELP("Label on the map"), 0, _UICONFIG("dt=text")),
			InputPortConfig<int>    ("Type", EWayPoint, _HELP("Label on the map"), 0, uiConfig),
			{0}
		};
		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void( "Added",    _HELP("Trigger when Entity was added" )),
			OutputPortConfig_Void( "Removed", _HELP("Trigger when Entity was removed" )),
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Adds or removes an Entity from the PDA Map.");
		config.SetCategory(EFLN_WIP);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if(eFE_Activate == event && pActInfo->pEntity)
		{
			CHUDRadar* pRadar = SAFE_HUD_FUNC_RET(GetRadar());
			if (pRadar)
			{
				const EntityId entityId = pActInfo->pEntity->GetId();
				if (IsPortActive(pActInfo, EIP_Remove))
				{
					pRadar->RemoveStoryEntity(entityId);
					ActivateOutput(pActInfo, EOP_Removed, true);
				}
				if (IsPortActive(pActInfo, EIP_Add))
				{
					const string& label = GetPortString(pActInfo, EIP_Label);
					FlashRadarType radarType = static_cast<FlashRadarType> (GetPortInt(pActInfo, EIP_Type));
					pRadar->AddStoryEntity(entityId, radarType, label);
					ActivateOutput(pActInfo, EOP_Added, true);
				}
			}
		}
	}
};


class CFlowNode_HUDInterferenceFX : public CFlowBaseNode 
{
	enum INPUTS
	{
		EIP_Trigger = 0,
		EIP_Distortion,
		EIP_Displacement,
		EIP_Alpha,
		EIP_Decay,
	};

public:
	CFlowNode_HUDInterferenceFX( SActivationInfo * pActInfo ) { }

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void  ( "Trigger", _HELP("Trigger to start effect" )),
			InputPortConfig<float>( "Distortion", 20.0f, _HELP("Distortion Amount" )),
			InputPortConfig<float>( "Displacement", 10.0f, _HELP("Displacement Amount" )),
			InputPortConfig<float>( "Alpha",  1.0f, _HELP("Alpha Amount" )),
			InputPortConfig<float>( "Decay",  0.0f, _HELP("Decay Amount" )),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.sDescription = _HELP("HUD Interference Effect.");
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
			const float distortion = GetPortFloat(pActInfo, EIP_Distortion);
			const float displacement = GetPortFloat(pActInfo, EIP_Displacement);
			const float alpha = GetPortFloat(pActInfo, EIP_Alpha) * 100.0f;
			const float decay = GetPortFloat(pActInfo, EIP_Decay);
			SAFE_HUD_FUNC(StartInterference(distortion, displacement, alpha, decay));
		}
	}
};

class CFlowNode_HUDRadarJammer : public CFlowBaseNode 
{
	enum INPUTS
	{
		EIP_Activate = 0,
		EIP_Deactivate,
		EIP_Radius,

	};

	enum OUTPUTS
	{
		EOP_Activated = 0,
		EOP_Deactivated,
	};

public:
	CFlowNode_HUDRadarJammer( SActivationInfo * pActInfo ) { }

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void  ( "Activate", _HELP("Trigger to activate" )),
			InputPortConfig_Void  ( "Deactivate", _HELP("Trigger to deactivate" )),
			InputPortConfig<float>( "Radius", 1.0f, _HELP("Radius" )),
			{0}
		};
		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void  ( "Activated", _HELP("Triggered when activated" )),
			OutputPortConfig_Void  ( "Deactivated", _HELP("Triggered when deactivated" )),
			{0}
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("HUD Radar Jammer");
		config.SetCategory(EFLN_WIP);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if(eFE_Activate == event)
		{
			const bool bActivate = IsPortActive(pActInfo, EIP_Activate);
			const bool bDeactivate = IsPortActive(pActInfo, EIP_Deactivate);
			if (bActivate || bDeactivate)
			{
				if (CHUDRadar* pRadar = SAFE_HUD_FUNC_RET(GetRadar()))
				{
					if (bDeactivate)
					{
						pRadar->SetJammer(0);
					}
					if (bActivate)
					{
						IEntity* pEntity = pActInfo->pEntity;
						if (pEntity) // only set new Jammer, in case we have an entity
						{
							const float radius = GetPortFloat(pActInfo, EIP_Radius);
							pRadar->SetJammer(pEntity->GetId(), radius);
						}
					}
				}
			}
		}
	}
};

class CFlowNode_HUDSilhouettes : public CFlowBaseNode 
{
	enum INPUTS
	{
		EIP_Activate = 0,
		EIP_Deactivate,
		EIP_Color,
	};

public:
	CFlowNode_HUDSilhouettes( SActivationInfo * pActInfo ) { }

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void  ( "Activate", _HELP("Trigger to activate. This sets a permanent silhouette (until removed), overwriting automated ones." )),
			InputPortConfig_Void  ( "Deactivate", _HELP("Trigger to deactivate" )),
			InputPortConfig<Vec3>  ( "Color", Vec3(1.0f,0.0f,0.0f), _HELP("Color"), 0, _UICONFIG("dt=clr")),
			{0}
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_ports;
		config.sDescription = _HELP("HUD Silhouette Shader");
		config.SetCategory(EFLN_WIP);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if(eFE_Activate == event)
		{
			const bool bActivate = IsPortActive(pActInfo, EIP_Activate);
			const bool bDeactivate = IsPortActive(pActInfo, EIP_Deactivate);
			if (bActivate || bDeactivate)
			{
				if (CHUDSilhouettes* pSil = SAFE_HUD_FUNC_RET(GetSilhouettes()))
				{
					IEntity* pEntity = pActInfo->pEntity;
					if(pEntity)
					{
						if (bDeactivate)
							pSil->ResetFlowGraphSilhouette(pEntity->GetId());
						if (bActivate)
						{
							const Vec3& color = GetPortVec3(pActInfo, EIP_Color);
							pSil->SetFlowGraphSilhouette(pEntity, color.x, color.y, color.z, 1.0f, -1.0f);
						}
					}
				}
			}
		}
	}
};

class CFlowNode_HUDRadarTexture : public CFlowBaseNode 
{
	enum INPUTS
	{
		EIP_Activate = 0,
		EIP_MapID,
	};

	enum OUTPUTS
	{
		EOP_Activated = 0,
	};

public:
	CFlowNode_HUDRadarTexture( SActivationInfo * pActInfo ) { }

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void  ( "Activate", _HELP("Trigger to activate" )),
			InputPortConfig<int>( "MapID", 1, _HELP("Texture ID 1-3" )),
			{0}
		};
		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void  ( "Activated", _HELP("Triggered when activated" )),
			{0}
		};

		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("HUD Radar Texture Background");
		config.SetCategory(EFLN_WIP);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if(eFE_Activate == event)
		{
			const bool bActivate = IsPortActive(pActInfo, EIP_Activate);
			if (bActivate)
			{
				if (CHUD* pHUD = g_pGame->GetHUD())
				{
					if (CHUDRadar* pRadar = pHUD->GetRadar())
					{
						pRadar->SetMiniMapTexture(GetPortInt(pActInfo, EIP_MapID));
					}
				}
			}
		}
	}
};

class CFlowNode_HUDNightVision : public CFlowBaseNode, public CHUD::IHUDListener
{
	enum INPUTS
	{
		EIP_Activate = 0,
		EIP_Deactivate,
	};

	enum OUTPUTS
	{
		EOP_Activated = 0,
		EOP_Deactivated,
	};

public:
	CFlowNode_HUDNightVision( SActivationInfo * pActInfo )
	{
	}

	~CFlowNode_HUDNightVision()
	{
		SAFE_HUD_FUNC(UnRegisterListener(this));
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_HUDNightVision(pActInfo);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void( "Activate",  _HELP("Activate NightVision"  )),
			InputPortConfig_Void( "Deactivate", _HELP("Deactivate NightVision" )),
			{0}
		};
		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void ( "Activated",  _HELP("Triggered when NightVision is activated" )),
			OutputPortConfig_Void ( "Deactivated",  _HELP("Triggered when NightVision is deactivated" )),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("HUD NightVision");
		config.SetCategory(EFLN_WIP);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				SAFE_HUD_FUNC(RegisterListener(this));
			}
			break;
		case eFE_Activate:
			{
				const CGameActions &rGameActions = g_pGame->Actions();
				if (IsPortActive(pActInfo, EIP_Activate))
				{
					SAFE_HUD_FUNC(OnAction(rGameActions.hud_night_vision, eIS_Pressed, 0.0f));
				}
				if (IsPortActive(pActInfo, EIP_Deactivate))
				{
					SAFE_HUD_FUNC(OnAction(rGameActions.hud_night_vision, eIS_Released, 0.0f));
				}
			}
			break;
		}
	}

protected:
	virtual void OnNightVision(bool bEnabled)
	{
		ActivateOutput(&m_actInfo, bEnabled ? EOP_Activated : EOP_Deactivated, true);
	}

	SActivationInfo m_actInfo;
};

class CFlowNode_HUDBinoculars : public CFlowBaseNode, public CHUD::IHUDListener
{
	enum INPUTS
	{
		EIP_Show = 0,
		EIP_Hide,
		EIP_HideNoFade,
		EIP_TaggedEntity,
		EIP_PlayZoomIn,
		EIP_PlayZoomOut,
	};

	enum OUTPUTS
	{
		EOP_Activated = 0,
		EOP_Deactivated,
		EOP_EntityTagged,
	};

public:
	CFlowNode_HUDBinoculars( SActivationInfo * pActInfo )
	{
	}

	~CFlowNode_HUDBinoculars()
	{
		SAFE_HUD_FUNC(UnRegisterListener(this));
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_HUDBinoculars(pActInfo);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void ("Show", _HELP("Show the HUD binoculars interface.")),
			InputPortConfig_Void ("Hide", _HELP("Hide the HUD binoculars interface.")),
			InputPortConfig_Void ("HideNoFade", _HELP("Hide the HUD binoculars interface without fade-out.")),
			InputPortConfig<EntityId> ( "TaggedEntity", _HELP("Inform when this entity is tagged. If not set, any entity is reported." )),
			InputPortConfig_Void ("PlayZoomIn", _HELP("Play the HUD Binoculars Zoom-In sound.")),
			InputPortConfig_Void ("PlayZoomOut", _HELP("Play the HUD Binoculars Zoom-In sound.")),
			{0}
		};
		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void ( "Activated",  _HELP("Triggered when Binoculars are activated" )),
			OutputPortConfig_Void ( "Deactivated",  _HELP("Triggered when Binoculars are deactivated" )),
			OutputPortConfig<EntityId> ( "EntityTagged",  _HELP("Triggered when Entity has been tagged" )),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("HUD Binoculars");
		config.SetCategory(EFLN_WIP);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;
				SAFE_HUD_FUNC(RegisterListener(this));
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Hide) && SAFE_HUD_FUNC_RET(GetScopes()->IsBinocularsShown()))
				{
					SAFE_HUD_FUNC(GetScopes()->ShowBinoculars(false));
				}
				if (IsPortActive(pActInfo, EIP_HideNoFade) && SAFE_HUD_FUNC_RET(GetScopes()->IsBinocularsShown()))
				{
					SAFE_HUD_FUNC(GetScopes()->ShowBinoculars(false, false, true));
				}
				if (IsPortActive(pActInfo, EIP_Show) && !SAFE_HUD_FUNC_RET(GetScopes()->IsBinocularsShown()))
				{
					SAFE_HUD_FUNC(GetScopes()->ShowBinoculars(true, true));
				}
				if (IsPortActive(pActInfo, EIP_PlayZoomIn))
				{
					SAFE_HUD_FUNC(PlaySound(ESound_BinocularsZoomIn));
				}
				if (IsPortActive(pActInfo, EIP_PlayZoomOut))
				{
					SAFE_HUD_FUNC(PlaySound(ESound_BinocularsZoomOut));
				}
			}
			break;
		}
	}

protected:
	virtual void OnEntityAddedToRadar(EntityId entityId)
	{
		const EntityId id = GetPortEntityId(&m_actInfo, EIP_TaggedEntity);
		if (id == 0 || id == entityId)
			ActivateOutput(&m_actInfo, EOP_EntityTagged, entityId);
	}
	virtual void OnBinoculars(bool bShown)
	{
		ActivateOutput(&m_actInfo, bShown ? EOP_Activated : EOP_Deactivated, true);
	}
	SActivationInfo m_actInfo;
};

class CFlowNode_HUDProgressBar : public CFlowBaseNode
{
	enum INPUTS
	{
		EIP_Show = 0,
		EIP_Hide,
		EIP_Progress,
		EIP_Text,
		EIP_PosX,
		EIP_PosY,
		EIP_Align,
		EIP_LockingAsset,
	};

	enum OUTPUTS
	{
		EOP_Shown = 0,
		EOP_Hidden,
		EOP_Progress,
	};

public:
	CFlowNode_HUDProgressBar( SActivationInfo * pActInfo )
	{
	}

	~CFlowNode_HUDProgressBar()
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void   ( "Show", _HELP("Trigger to show progress bar" )),
			InputPortConfig_Void   ( "Hide", _HELP("Trigger to hide progress bar" )),
			InputPortConfig<int>   ( "Progress", 0, _HELP("Progress from 0-100" )),
			InputPortConfig<string>( "text_Text", _HELP("Text to display" )),
			InputPortConfig<float> ( "PosX", 400, _HELP("PosX" )),
			InputPortConfig<float> ( "PosY", 300, _HELP("PosY" )),
			InputPortConfig<int>   ( "Align", 0, _HELP("Alignment (Top or Bottom)"), 0, _UICONFIG("enum_int:Bottom=0,Top=1")),
			InputPortConfig<int>   ( "LockingAsset", 0, _HELP("Use TAC Launcher locking asset instead." )),
			{0}
		};
		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void ( "Show",  _HELP("Triggered when shown" )),
			OutputPortConfig_Void ( "Hide",  _HELP("Triggered when hidden" )),
			OutputPortConfig<int> ( "Progress", _HELP("Current progress value")),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("HUD Binoculars");
		config.SetCategory(EFLN_WIP);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event != eFE_Activate)
			return;

		if (IsPortActive(pActInfo, EIP_Hide))
		{
			SAFE_HUD_FUNC(ShowProgress(-1)); // hide
			ActivateOutput(pActInfo, EOP_Hidden, true);
		}
		if (IsPortActive(pActInfo, EIP_Show))
		{
			const float posX = GetPortFloat(pActInfo, EIP_PosX);
			const float posY = GetPortFloat(pActInfo, EIP_PosY);
			const string& text = GetPortString(pActInfo, EIP_Text);
			const int align = GetPortInt(pActInfo, EIP_Align);
			const int lockAsset = GetPortInt(pActInfo, EIP_LockingAsset);
			SAFE_HUD_FUNC(ShowProgress(0, true, (int)posX, (int)posY, text.c_str(), (align == 0)? false : true, (lockAsset==0)? false:true));
			ActivateOutput(pActInfo, EOP_Shown, true);
			ActivateOutput(pActInfo, EOP_Progress, 0);
		}
		if (IsPortActive(pActInfo, EIP_Progress))
		{
			const int progress = GetPortInt(pActInfo, EIP_Progress);
			SAFE_HUD_FUNC(ShowProgress(progress));
			if (progress < 0)
				ActivateOutput(pActInfo, EOP_Hidden, true);
			else
				ActivateOutput(pActInfo, EOP_Progress, progress);
		}
	}
};

class CFlowNode_HUDDataUpload : public CFlowBaseNode
{
	enum INPUTS
	{
		EIP_Activate = 0,
		EIP_Deactivate,
	};

	enum OUTPUTS
	{
		EOP_Activated = 0,
		EOP_Deactivated,
	};

public:
	CFlowNode_HUDDataUpload( SActivationInfo * pActInfo )
	{
	}

	~CFlowNode_HUDDataUpload()
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void   ( "Activate", _HELP("Trigger to activate HUD Upload" )),
			InputPortConfig_Void   ( "Deactivate", _HELP("Trigger to deactivate HUD Upload" )),
			{0}
		};
		static const SOutputPortConfig out_ports[] = 
		{
			OutputPortConfig_Void ( "Activated",  _HELP("Triggered when activated" )),
			OutputPortConfig_Void ( "Deactivated",  _HELP("Triggered when deactivated" )),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("HUD Upload Effect");
		config.SetCategory(EFLN_WIP);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event != eFE_Activate)
			return;

		if (IsPortActive(pActInfo, EIP_Activate))
		{
			SAFE_HUD_FUNC(ShowDataUpload(true));
			ActivateOutput(pActInfo, EOP_Activated, true);
		}
		if (IsPortActive(pActInfo, EIP_Deactivate))
		{
			SAFE_HUD_FUNC(ShowDataUpload(false));
			ActivateOutput(pActInfo, EOP_Deactivated, true);
		}
	}
};

class CFlowNode_HUDObjectives : public CFlowBaseNode
{
	enum INPUTS
	{
		EIP_SetGoal = 0,
		EIP_Goal,
		EIP_SetMain,
		EIP_Main,
	};

public:
	CFlowNode_HUDObjectives( SActivationInfo * pActInfo )
	{
	}

	~CFlowNode_HUDObjectives()
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void   ( "SetGoal", _HELP("Set Goal Objective" )),
			InputPortConfig<string> ("mission_Goal", _HELP("Goal Objective")),
			InputPortConfig_Void   ( "SetMain", _HELP("Set Main Objective"), _HELP("SetMainObjective") ),
			InputPortConfig<string> ("mission_Main", _HELP("Main Objective"), _HELP("MainObjective") ),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.sDescription = _HELP("HUD Objective support for Goal and MainObjective");
		config.SetCategory(EFLN_WIP);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event != eFE_Activate)
			return;

		if (IsPortActive(pActInfo, EIP_SetMain))
		{
			const string& v = GetPortString(pActInfo, EIP_Main);
			SAFE_HUD_FUNC(SetMainObjective(v.c_str(), false));
		}
		if (IsPortActive(pActInfo, EIP_SetGoal))
		{
			const string& v = GetPortString(pActInfo, EIP_Goal);
			SAFE_HUD_FUNC(SetMainObjective(v.c_str(), true));
		}
	}
};

class CFlowNode_HUDTutorial : public CFlowBaseNode
{
	enum INPUTS
	{
		EIP_Show = 0,
		EIP_Hide,
		EIP_Message,
		EIP_Position,
		EIP_Mode,
		EIP_Timeout,
	};

	enum OUTPUTS
	{
		EOP_Done = 0
	};

public:
	CFlowNode_HUDTutorial( SActivationInfo * pActInfo )
	{
	}

	~CFlowNode_HUDTutorial()
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void   ( "Show", _HELP("Show the message in the specified MessageBox" )),
			InputPortConfig_Void   ( "Hide", _HELP("Hide the MessageBox" )),
			InputPortConfig<string>( "Message", _HELP("Message to show"), 0, _UICONFIG("dt=text")),
			InputPortConfig<int>   ( "Position", 0, _HELP("Which message box"), 0, _UICONFIG("enum_int:Left=0,Right=1")),
			InputPortConfig<int>   ( "Mode", 0, _HELP("Mode: Add or Replace Message"), 0, _UICONFIG("enum_int:Add=0,Replace=1")),
			InputPortConfig<float> ( "Timeout", 0.0f, _HELP("How long to show message. 0.0 = Until hidden")),
			{0}
		};
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_Void( "Done", _HELP("Triggered when Done")),
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Show Tutorial Messages on the HUD");
		config.SetCategory(EFLN_WIP);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event != eFE_Activate)
			return;
		CHUD* pHUD = g_pGame->GetHUD();
		if (pHUD == 0)
			return;
	}
};

class CFlowNode_HUDOverlayMessage : public CFlowBaseNode
{
	enum INPUTS
	{
		EIP_Show = 0,
		EIP_Hide,
		EIP_Message,
		EIP_PosX,
		EIP_PosY,
		EIP_Color,
		EIP_Timeout,
	};

	enum OUTPUTS
	{
		EOP_Done = 0
	};

public:
	CFlowNode_HUDOverlayMessage( SActivationInfo * pActInfo )
	{
	}

	~CFlowNode_HUDOverlayMessage()
	{
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void GetConfiguration( SFlowNodeConfig& config )
	{
		static const SInputPortConfig in_ports[] = 
		{
			InputPortConfig_Void   ( "Show", _HELP("Show the message" )),
			InputPortConfig_Void   ( "Hide", _HELP("Hide the message" )),
			InputPortConfig<string>( "Message", _HELP("Message to show"), 0, _UICONFIG("dt=text")),
			InputPortConfig<int> ( "PosX", 400, _HELP("PosX")),
			InputPortConfig<int> ( "PosY", 300, _HELP("PosY")),
			InputPortConfig<Vec3>  ( "Color", Vec3(1.0f,1.0f,1.0f), _HELP("Color"), 0, _UICONFIG("dt=clr")),
			InputPortConfig<float> ( "Timeout", 3.0f, _HELP("How long to show message")),
			{0}
		};
		static const SOutputPortConfig out_ports[] =
		{
			{0}
		};
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Show Overlay Messages on the HUD");
		config.SetCategory(EFLN_WIP);
	}

	void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		if (event != eFE_Activate)
			return;
		CHUD* pHUD = g_pGame->GetHUD();
		if (pHUD == 0)
			return;
		if (IsPortActive(pActInfo, EIP_Show))
		{
			const int posX = GetPortInt(pActInfo, EIP_PosX);
			const int posY = GetPortInt(pActInfo, EIP_PosY);
			const string& msg = GetPortString(pActInfo, EIP_Message);
			const float duration = GetPortFloat(pActInfo, EIP_Timeout);
			const Vec3& color = GetPortVec3(pActInfo, EIP_Color);
			pHUD->DisplayBigOverlayFlashMessage(msg.c_str(), duration, posX, posY, color);
		}
		else if (IsPortActive(pActInfo, EIP_Hide))
		{
			pHUD->FadeOutBigOverlayFlashMessage();
		}
	}
};

REGISTER_FLOW_NODE_SINGLETON("HUD:DisplayPDAMessage",	 CFlowNode_PDAMessage);
REGISTER_FLOW_NODE_SINGLETON("HUD:SendEvent",	       	 CFlowNode_HUDEvent);
REGISTER_FLOW_NODE("HUD:PDAControl",					         CFlowNode_PDAControl);
REGISTER_FLOW_NODE_SINGLETON("HUD:ShowHUDMessage",		 CFlowNode_ShowHUDMessage);
REGISTER_FLOW_NODE_SINGLETON("HUD:HUDControl",			   CFlowNode_HUDControl);
REGISTER_FLOW_NODE_SINGLETON("HUD:HUDDataUpload",			 CFlowNode_HUDDataUpload);
REGISTER_FLOW_NODE_SINGLETON("HUD:PDAScanner",	       CFlowNode_PDAScanner);
REGISTER_FLOW_NODE_SINGLETON("HUD:RadarControl",			 CFlowNode_HUDRadar);
REGISTER_FLOW_NODE_SINGLETON("HUD:MapInfo",					   CFlowNode_HUDMapInfo);
REGISTER_FLOW_NODE("HUD:SilhouetteOutline",						 CFlowNode_HUDSilhouettes);
REGISTER_FLOW_NODE("HUD:AirstrikeControl",					   CFlowNode_HUDAirstrike);
REGISTER_FLOW_NODE("HUD:InterferenceEffect",					 CFlowNode_HUDInterferenceFX);
REGISTER_FLOW_NODE_SINGLETON("HUD:RadarJammer",				 CFlowNode_HUDRadarJammer);
REGISTER_FLOW_NODE_SINGLETON("HUD:RadarTexture",			 CFlowNode_HUDRadarTexture);
REGISTER_FLOW_NODE("HUD:NightVision",									 CFlowNode_HUDNightVision);
REGISTER_FLOW_NODE("HUD:Binoculars",									 CFlowNode_HUDBinoculars);
REGISTER_FLOW_NODE_SINGLETON("HUD:ProgressBar",				 CFlowNode_HUDProgressBar);
REGISTER_FLOW_NODE_SINGLETON("HUD:Objectives",				 CFlowNode_HUDObjectives);
// REGISTER_FLOW_NODE_SINGLETON("HUD:TutorialMsg",        CFlowNode_HUDTutorial);
REGISTER_FLOW_NODE_SINGLETON("HUD:OverlayMsg",         CFlowNode_HUDOverlayMessage);