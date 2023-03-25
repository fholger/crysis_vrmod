/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 8:12:2005   11:26 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "ItemSharedParams.h"
#include "Actor.h"


//------------------------------------------------------------------------
EntityId CItem::NetGetOwnerId() const
{
	return m_ownerId;
}

//------------------------------------------------------------------------
void CItem::NetSetOwnerId(EntityId id)
{
	if (id==m_ownerId)
		return;

	CryLogAlways("%s::NetSetOwnerId(%s)", GetEntity()->GetName(), GetActor(id)?GetActor(id)->GetEntity()->GetName():"null");

	if (id)
		PickUp(id, true);
	else
	{
		Drop();

		CActor *pActor=GetOwnerActor();
		if (pActor)
			pActor->GetInventory()->SetCurrentItem(0);
	}
}

//------------------------------------------------------------------------
void CItem::InitClient(int channelId)
{
	// send the differences between the current, and the initial setup
	for (TAccessoryMap::iterator it=m_accessories.begin(); it != m_accessories.end(); ++it)
		GetGameObject()->InvokeRMI(ClAttachAccessory(), RequestAttachAccessoryParams(it->first.c_str()), eRMI_ToClientChannel, channelId);

	IActor *pOwner=GetOwnerActor();
	if (!pOwner)
		return;

	// only send the pickup message if the player is connecting
	// for items spawned during gameplay, CItem::PickUp is already sending the pickup message
	INetChannel *pNetChannel=m_pGameFramework->GetNetChannel(channelId);
	if (pNetChannel && pNetChannel->GetContextViewState()<eCVS_InGame)
	{
		if (!m_stats.mounted && !m_stats.used)
		{
			pOwner->GetGameObject()->InvokeRMIWithDependentObject(CActor::ClPickUp(), 
				CActor::PickItemParams(GetEntityId(), m_stats.selected, false), eRMI_ToClientChannel, GetEntityId(), channelId);
			//GetOwnerActor()->GetGameObject()->InvokeRMI(CActor::ClPickUp(), 
			//	CActor::PickItemParams(GetEntityId(), m_stats.selected, false), eRMI_ToClientChannel, channelId);
		}
	}

	if (m_stats.mounted && m_stats.used)
	{
		pOwner->GetGameObject()->InvokeRMIWithDependentObject(CActor::ClStartUse(), 
			CActor::ItemIdParam(GetEntityId()), eRMI_ToClientChannel, GetEntityId(), channelId);
	}
}

//------------------------------------------------------------------------
void CItem::PostInitClient(int channelId)
{
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, SvRequestAttachAccessory)
{
	if (IInventory *pInventory=GetActorInventory(GetOwnerActor()))
	{
		if (pInventory->GetCountOfClass(params.accessory.c_str())>0)
		{
			DoSwitchAccessory(params.accessory.c_str());
			GetGameObject()->InvokeRMI(ClAttachAccessory(), params, eRMI_ToAllClients|eRMI_NoLocalCalls);

			return true;
		}
	}
	
	return true; // set this to false later
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, ClAttachAccessory)
{
	DoSwitchAccessory(params.accessory.c_str());

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, SvRequestEnterModify)
{
	GetGameObject()->InvokeRMI(ClEnterModify(), params, eRMI_ToOtherClients, m_pGameFramework->GetGameChannelId(pNetChannel));

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, SvRequestLeaveModify)
{
	GetGameObject()->InvokeRMI(ClLeaveModify(), params, eRMI_ToOtherClients, m_pGameFramework->GetGameChannelId(pNetChannel));

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, ClEnterModify)
{
	PlayAction(g_pItemStrings->enter_modify, 0, false, eIPAF_Default | eIPAF_RepeatLastFrame);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CItem, ClLeaveModify)
{
	PlayAction(g_pItemStrings->leave_modify, 0);

	return true;
}