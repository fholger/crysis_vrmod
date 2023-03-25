/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a class which handle client actions on vehicles.

-------------------------------------------------------------------------
History:
- 17:10:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "IGame.h"
#include "IActorSystem.h"
#include "IVehicleSystem.h"
#include "VehicleClient.h"
#include "GameCVars.h"
#include "Game.h"
#include "Weapon.h"
#include "Player.h"
#include "HUD/HUD.h"

#define VEHICLE_GAMEPAD_DEADZONE 0.2f
//------------------------------------------------------------------------
bool CVehicleClient::Init()
{
  m_actionNameIds.clear();	
 
  m_actionNameIds.insert(TActionNameIdMap::value_type("use", eVAI_Exit));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeseat", eVAI_ChangeSeat));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeseat1", eVAI_ChangeSeat1));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeseat2", eVAI_ChangeSeat2));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeseat3", eVAI_ChangeSeat3));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeseat4", eVAI_ChangeSeat4));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeseat5", eVAI_ChangeSeat5));

  m_actionNameIds.insert(TActionNameIdMap::value_type("v_changeview", eVAI_ChangeView));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_viewoption", eVAI_ViewOption));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_zoom_in", eVAI_ZoomIn));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_zoom_out", eVAI_ZoomOut));

  m_actionNameIds.insert(TActionNameIdMap::value_type("attack1", eVAI_Attack1));
  m_actionNameIds.insert(TActionNameIdMap::value_type("zoom", eVAI_Attack2));
	m_actionNameIds.insert(TActionNameIdMap::value_type("v_attack2", eVAI_Attack2));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_zoom", eVAI_Attack2));
  m_actionNameIds.insert(TActionNameIdMap::value_type("firemode", eVAI_FireMode));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_lights", eVAI_ToggleLights));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_horn", eVAI_Horn));

  m_actionNameIds.insert(TActionNameIdMap::value_type("v_rotateyaw", eVAI_RotateYaw));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_rotatepitch", eVAI_RotatePitch));

  m_actionNameIds.insert(TActionNameIdMap::value_type("v_moveforward", eVAI_MoveForward));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_moveback", eVAI_MoveBack));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_moveup", eVAI_MoveUp));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_movedown", eVAI_MoveDown));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_turnleft", eVAI_TurnLeft));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_turnright", eVAI_TurnRight));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_strafeleft", eVAI_StrafeLeft));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_straferight", eVAI_StrafeRight));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_rollleft", eVAI_RollLeft));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_rollright", eVAI_RollRight));

	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_v_rotateyaw", eVAI_XIRotateYaw));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_v_rotatepitch", eVAI_XIRotatePitch));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_v_movey", eVAI_XIMoveY));
	m_actionNameIds.insert(TActionNameIdMap::value_type("xi_v_movex", eVAI_XIMoveX));

  m_actionNameIds.insert(TActionNameIdMap::value_type("v_pitchup", eVAI_PitchUp));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_pitchdown", eVAI_PitchDown));

  m_actionNameIds.insert(TActionNameIdMap::value_type("v_brake", eVAI_Brake));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_afterburner", eVAI_AfterBurner));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_boost", eVAI_Boost));

  m_actionNameIds.insert(TActionNameIdMap::value_type("v_debug_1", eVAI_Debug_1));
  m_actionNameIds.insert(TActionNameIdMap::value_type("v_debug_2", eVAI_Debug_2));
  
	m_xiRotation.Set(0,0,0);
	m_bMovementFlagForward = false;
	m_bMovementFlagBack = false;
	m_bMovementFlagRight = false;
	m_bMovementFlagLeft = false;
  m_fLeftRight = 0.f;
  m_fForwardBackward = 0.f;
  m_tp = false;

  return true;
}

//------------------------------------------------------------------------
void CVehicleClient::Reset()
{
	m_tp = false;
}

//------------------------------------------------------------------------
void CVehicleClient::OnAction(IVehicle* pVehicle, EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	assert(pVehicle);
 	if (!pVehicle)
		return;
	
  TActionNameIdMap::const_iterator ite = m_actionNameIds.find(actionId);
	if (ite == m_actionNameIds.end())
    return;

	switch (ite->second)
  {
  case (eVAI_XIMoveX):	
    {

			IVehicleMovement *pMovement = pVehicle->GetMovement();
			if (value < VEHICLE_GAMEPAD_DEADZONE && value > -VEHICLE_GAMEPAD_DEADZONE)
				value = 0.0f;

			if(pMovement && pMovement->GetMovementType() == IVehicleMovement::eVMT_Air)
			{
				//strafe instead of turning for air vehicles
				if(value>0.f)
				{
					pVehicle->OnAction(eVAI_StrafeRight, eAAM_OnPress, value, actorId);
					m_bMovementFlagRight = true;
				}
				else if(value==0.f)
				{
					if(m_bMovementFlagRight)
					{
						pVehicle->OnAction(eVAI_StrafeRight, eAAM_OnRelease, 0.f, actorId);
						m_bMovementFlagRight = false;
					}
					else if(m_bMovementFlagLeft)
					{
						pVehicle->OnAction(eVAI_StrafeLeft, eAAM_OnRelease, 0.f, actorId);
						m_bMovementFlagLeft = false;
					}
				}
				else//value<0
				{
					pVehicle->OnAction(eVAI_StrafeLeft, eAAM_OnPress, -value, actorId);
					m_bMovementFlagLeft = true;
				}
			}
			else
			{
				if(value>0.f)
				{
					pVehicle->OnAction(eVAI_TurnRight, eAAM_OnPress, value, actorId);
					m_bMovementFlagRight = true;
				}
				else if(value==0.f)
				{
					if(m_bMovementFlagRight)
					{
						pVehicle->OnAction(eVAI_TurnRight, eAAM_OnRelease, 0.f, actorId);
						m_bMovementFlagRight = false;
					}
					else if(m_bMovementFlagLeft)
					{
						pVehicle->OnAction(eVAI_TurnLeft, eAAM_OnRelease, 0.f, actorId);
						m_bMovementFlagLeft = false;
					}
				}
				else//value<0
				{
					pVehicle->OnAction(eVAI_TurnLeft, eAAM_OnPress, -value, actorId);
					m_bMovementFlagLeft = true;
				}
			}
			break;
		}
  case (eVAI_XIMoveY):
		{
			EVehicleActionIds eForward = eVAI_MoveForward;
			EVehicleActionIds eBack = eVAI_MoveBack;
			if(!strcmp("Asian_helicopter",pVehicle->GetEntity()->GetClass()->GetName()))
			{
				eForward = eVAI_MoveUp;
				eBack = eVAI_MoveDown;
			}

			if(value>0.f)
			{
				pVehicle->OnAction(eForward, eAAM_OnPress, value, actorId);
				m_bMovementFlagForward = true;
			}
			else if(value==0.f)
			{
				if(m_bMovementFlagForward)
				{
					pVehicle->OnAction(eForward, eAAM_OnRelease, 0.f, actorId);
					m_bMovementFlagForward = false;
				}
				else if(m_bMovementFlagBack)
				{
					pVehicle->OnAction(eBack, eAAM_OnRelease, 0.f, actorId);
					m_bMovementFlagBack = false;
				}
			}			
			else//value<0.f
			{
				pVehicle->OnAction(eBack, eAAM_OnPress, -value, actorId);
				m_bMovementFlagBack = true;
			}
      break;
		}
  case (eVAI_XIRotateYaw):
		{
			IVehicleMovement *pMovement = pVehicle->GetMovement();
			if(pMovement && pMovement->GetMovementType() == IVehicleMovement::eVMT_Air && pVehicle->IsPlayerDriving())
			{
				pVehicle->OnAction(eVAI_RotateYaw, eAAM_OnPress, value, actorId);
			}
			else
			{
				m_xiRotation.x = (5.0f*value)*(5.0f*value)*value;
			}
      break;
		}
  case (eVAI_XIRotatePitch):
		{
			IVehicleMovement *pMovement = pVehicle->GetMovement();
			if(pMovement && pMovement->GetMovementType() == IVehicleMovement::eVMT_Air && pVehicle->IsPlayerDriving())
			{
				pVehicle->OnAction(eVAI_RotatePitch, eAAM_OnPress, g_pGameCVars->cl_invertController ? -value : value, actorId);
			}
			else
			{
				m_xiRotation.y = (3.5f*value)*(3.5f*value)*(-value);
				if(g_pGameCVars->cl_invertController)
					m_xiRotation.y*=-1;
			}
      break;
		}
  case (eVAI_RotatePitch):
    {
      if (g_pGameCVars->cl_invertMouse)
        value *= -1.f;
      pVehicle->OnAction(ite->second, activationMode, value, actorId);
      break;
    }
  case (eVAI_TurnLeft):
    {
      if (activationMode == eAAM_OnPress || activationMode == eAAM_OnRelease)
        m_fLeftRight -= value*2.f - 1.f;
			m_fLeftRight = CLAMP(m_fLeftRight, -1.0f, 1.0f);
      pVehicle->OnAction(ite->second, activationMode, -m_fLeftRight, actorId);
      break;
    }
  case (eVAI_TurnRight):
    {
      if (activationMode == eAAM_OnPress || activationMode == eAAM_OnRelease)
        m_fLeftRight += value*2.f - 1.f;
			m_fLeftRight = CLAMP(m_fLeftRight, -1.0f, 1.0f);
      pVehicle->OnAction(ite->second, activationMode, m_fLeftRight, actorId);
      break;
    }  
  case (eVAI_MoveForward):
    {
      if (activationMode == eAAM_OnPress || activationMode == eAAM_OnRelease)
        m_fForwardBackward += value*2.f - 1.f;

			if(activationMode == eAAM_OnRelease)
				m_fForwardBackward = CLAMP(m_fForwardBackward, 0.0f, 1.0f);
			else
				m_fForwardBackward = CLAMP(m_fForwardBackward, -1.0f, 1.0f);
      pVehicle->OnAction(ite->second, activationMode, m_fForwardBackward, actorId);
      break;
    }
  case (eVAI_MoveBack):
    {
      if (activationMode == eAAM_OnPress || activationMode == eAAM_OnRelease)
        m_fForwardBackward -= value*2.f - 1.f;

			if(activationMode == eAAM_OnRelease)
				m_fForwardBackward = CLAMP(m_fForwardBackward, -1.0f, 0.0f);
			else
				m_fForwardBackward = CLAMP(m_fForwardBackward, -1.0f, 1.0f);
      pVehicle->OnAction(ite->second, activationMode, -m_fForwardBackward, actorId);
      break;
    }  
	case (eVAI_ZoomIn):
	case (eVAI_ZoomOut):
		if(SAFE_HUD_FUNC_RET(GetModalHUD()))
			break;
  default:
		pVehicle->OnAction(ite->second, activationMode, value, actorId);
    break;		
	}
}

//------------------------------------------------------------------------

void CVehicleClient::PreUpdate(IVehicle* pVehicle, EntityId actorId)
{
	// Controller framerate compensation needs frame time! 
	// The constant is to compensate for small frame time values.
	if(fabsf(m_xiRotation.x) > 0.001)
	{
		pVehicle->OnAction(eVAI_RotateYaw, eAAM_OnPress, m_xiRotation.x*gEnv->pTimer->GetFrameTime()*30.0f, actorId);
	}
	if(fabsf(m_xiRotation.y) > 0.001)
	{
		pVehicle->OnAction(eVAI_RotatePitch, eAAM_OnPress, m_xiRotation.y*gEnv->pTimer->GetFrameTime()*30.0f, actorId);
	}
}

//------------------------------------------------------------------------
void CVehicleClient::OnEnterVehicleSeat(IVehicleSeat* pSeat)
{
	m_bMovementFlagRight=m_bMovementFlagLeft=m_bMovementFlagForward=m_bMovementFlagBack=false;
  m_fLeftRight = m_fForwardBackward = 0.f;

	IVehicle* pVehicle = pSeat->GetVehicle();
	assert(pVehicle);

	IActorSystem* pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	assert(pActorSystem);

	IActor* pActor = pActorSystem->GetActor(pSeat->GetPassenger());
	bool isThirdPerson = pActor->IsThirdPerson() || m_tp;

	TVehicleViewId viewId = InvalidVehicleViewId;
	TVehicleViewId firstViewId = InvalidVehicleViewId;

	while (viewId = pSeat->GetNextView(viewId))
	{
		if (viewId == firstViewId)
			break;

		if (firstViewId == InvalidVehicleViewId)
			firstViewId = viewId;

		if (IVehicleView* pView = pSeat->GetView(viewId))
		{
			if (pView->IsThirdPerson() == isThirdPerson)
				break;
		}
	}

	if (viewId != InvalidVehicleViewId)
		pSeat->SetView(viewId);

	IActionMapManager* pMapManager = gEnv->pGame->GetIGameFramework()->GetIActionMapManager();
	assert(pMapManager);

	pMapManager->EnableActionMap("landvehicle", false);
	pMapManager->EnableActionMap("seavehicle", false);
	pMapManager->EnableActionMap("helicopter", false);
	pMapManager->EnableActionMap("vtol", false);
}

//------------------------------------------------------------------------
void CVehicleClient::OnExitVehicleSeat(IVehicleSeat* pSeat)
{
	if(pSeat && pSeat->IsDriver())
	{
		m_bMovementFlagRight=m_bMovementFlagLeft=m_bMovementFlagForward=m_bMovementFlagBack=false;
	  m_fLeftRight = m_fForwardBackward = 0.f;
	}
  
  TVehicleViewId viewId = pSeat->GetCurrentView();
	
  if (viewId != InvalidVehicleViewId)  
  {
    if (IVehicleView* pView = pSeat->GetView(viewId))    
      m_tp = pView->IsThirdPerson();
    
    pSeat->SetView(InvalidVehicleViewId);
  }
}


//------------------------------------------------------------------------
CVehicleClient::SVehicleClientInfo& CVehicleClient::GetVehicleClientInfo(IVehicle* pVehicle)
{
	IEntityClass* pClass = pVehicle->GetEntity()->GetClass();

	TVehicleClientInfoMap::iterator ite = m_vehiclesInfo.find(pClass);

	if (ite == m_vehiclesInfo.end())
	{
		// we need to add this class in our list
		SVehicleClientInfo clientInfo;
		clientInfo.seats.resize(pVehicle->GetSeatCount());

		TVehicleSeatClientInfoVector::iterator seatInfoIte = clientInfo.seats.begin();
		TVehicleSeatClientInfoVector::iterator seatInfoEnd = clientInfo.seats.end();
		TVehicleSeatId seatId = InvalidVehicleSeatId;

		for (; seatInfoIte != seatInfoEnd; ++seatInfoIte)
		{
			seatId++;

			SVehicleSeatClientInfo& seatInfo = *seatInfoIte;
			seatInfo.seatId = seatId;
			seatInfo.viewId = InvalidVehicleViewId;
		}


		m_vehiclesInfo.insert(TVehicleClientInfoMap::value_type(pClass, clientInfo));

		ite = m_vehiclesInfo.find(pClass);
	}

	// this will never happen
	assert(ite != m_vehiclesInfo.end());
	
	return ite->second;
}

//------------------------------------------------------------------------
CVehicleClient::SVehicleSeatClientInfo& 
	CVehicleClient::GetVehicleSeatClientInfo(SVehicleClientInfo& vehicleClientInfo, TVehicleSeatId seatId)
{
	TVehicleSeatClientInfoVector::iterator seatInfoIte = vehicleClientInfo.seats.begin();
	TVehicleSeatClientInfoVector::iterator seatInfoEnd = vehicleClientInfo.seats.end();

	for (; seatInfoIte != seatInfoEnd; ++seatInfoIte)
	{
		SVehicleSeatClientInfo& seatClientInfo = *seatInfoIte;
		if (seatClientInfo.seatId == seatId)
			return *seatInfoIte;
	}

	// will never happen, unless the vehicle has new seat created after the 
	// game started
	assert(0);
	return vehicleClientInfo.seats[0];
}
