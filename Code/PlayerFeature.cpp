/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 23:10:2005   12:30 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "PlayerFeature.h"
#include "ItemParamReader.h"
#include "Player.h"

//--------------------------------------------------------------------
CPlayerFeature::CPlayerFeature()
{
	m_notPickUp = false;
}

//------------------------------------------------------------------------
void CPlayerFeature::PostInit(IGameObject *pGameObject)
{
	CItem::PostInit(pGameObject);

	pGameObject->DisablePostUpdates(this);
	pGameObject->DisableUpdateSlot(this, eIUS_General);
	pGameObject->DisableUpdateSlot(this, eIUS_Scheduler);
	
	GetEntity()->Activate(0);
}

//------------------------------------------------------------------------
bool CPlayerFeature::ReadItemParams(const IItemParamsNode *root)
{
	if (!CItem::ReadItemParams(root))
		return false;

	m_features.resize(0);
	m_notPickUp = false;

	const IItemParamsNode *features = root->GetChild("features");
	if (features)
	{
		int n = features->GetChildCount();
		for (int i=0; i<n; i++)
		{
			const IItemParamsNode *feature = features->GetChild(i);
			if (feature)
			{
				const char *name = feature->GetAttribute("name");
				if (name && name[0])
					m_features.push_back(name);
				
				int noPickUp = 0;
				feature->GetAttribute("noPickUp",noPickUp);
				if(noPickUp!=0)
					m_notPickUp = true;
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CPlayerFeature::OnReset()
{
	CItem::OnReset();

	CActor *pActor=GetOwnerActor();
	for (std::vector<string>::const_iterator it=m_features.begin(); it!=m_features.end(); ++it)
		ActivateFeature(pActor, it->c_str());
}

//-------------------------------------------------------------------------
void CPlayerFeature::PickUp(EntityId pickerId, bool sound, bool select, bool keepHistory)
{
	if(m_notPickUp)
		OnPickedUp(pickerId,true);
	else
		CItem::PickUp(pickerId,sound,select,keepHistory);
}
//------------------------------------------------------------------------
void CPlayerFeature::OnPickedUp(EntityId pickerId, bool destroyed)
{
	CItem::OnPickedUp(pickerId, destroyed);

	CActor *pActor=GetActor(pickerId);
	for (std::vector<string>::const_iterator it=m_features.begin(); it!=m_features.end(); ++it)
		ActivateFeature(pActor, it->c_str());
}

//------------------------------------------------------------------------
void CPlayerFeature::ActivateFeature(CActor *pActor, const char *feature)
{
	if (!stricmp("nanosuit", feature))
		NanoSuit(pActor);
	else if (!stricmp("parachute", feature))
		Parachute(pActor);
	else if (!stricmp("aliencloak", feature))
		AlienCloak(pActor);
	else if (!stricmp("nightvision", feature))
		NightVision(pActor);
	else if (!stricmp("dualSOCOM", feature))
		DualSOCOM(pActor);
	else
		CryLogAlways("%s - Unknown Player Feature '%s'...", GetEntity()->GetName(), feature);
}

//------------------------------------------------------------------------
void CPlayerFeature::NanoSuit(CActor *pActor)
{
	if (pActor && pActor->GetActorClass()==CPlayer::GetActorClassType())
	{
		CPlayer *pPlayer=static_cast<CPlayer *>(pActor);
		pPlayer->ActivateNanosuit(true);
	}
}

//------------------------------------------------------------------------
void CPlayerFeature::Parachute(CActor *pActor)
{
	if (pActor && pActor->GetActorClass()==CPlayer::GetActorClassType())
	{
		CPlayer *pPlayer=static_cast<CPlayer *>(pActor);
		pPlayer->EnableParachute(true);
	}
}

//------------------------------------------------------------------------
void CPlayerFeature::AlienCloak(CActor *pActor)
{
	if (pActor && pActor->GetActorClass()==CPlayer::GetActorClassType())
	{
		CPlayer *pPlayer=static_cast<CPlayer *>(pActor);
		CNanoSuit *pSuit=pPlayer->GetNanoSuit();

		if (pSuit)
		{
			pSuit->ActivateMode(NANOMODE_CLOAK, true);
			pSuit->SetCloakLevel(CLOAKMODE_REFRACTION);
		}
	}
}

//------------------------------------------------------------------------
void CPlayerFeature::NightVision(CActor *pActor)
{
	if (pActor && pActor->GetActorClass()==CPlayer::GetActorClassType())
	{
		CPlayer *pPlayer=static_cast<CPlayer *>(pActor);
		if (CNanoSuit *pSuit=pPlayer->GetNanoSuit())
			pSuit->EnableNightVision(true);
	}
}

//-------------------------------------------------------------------------
void CPlayerFeature::DualSOCOM(CActor *pActor)
{
	if(pActor && !pActor->IsPlayer())
	{
		IInventory *pInventory=GetActorInventory(pActor);
		if (pInventory)
		{
			if (IsServer())
			{
					m_pItemSystem->GiveItem(pActor, "SOCOM", false, false,false);
					//m_pItemSystem->SetActorItem(pActor,"SOCOM",true);
			}
		}
	}
}