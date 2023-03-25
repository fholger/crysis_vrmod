/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 5:9:2005   14:55 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "IActorSystem.h"
#include "Actor.h"
#include "Game.h"
#include "OffHand.h"


//------------------------------------------------------------------------
void CItem::OnStartUsing()
{
}

//------------------------------------------------------------------------
void CItem::OnStopUsing()
{
}

//------------------------------------------------------------------------
void CItem::OnSelect(bool select)
{
}

//------------------------------------------------------------------------
void CItem::OnSelected(bool selected)
{
}

//------------------------------------------------------------------------
void CItem::OnEnterFirstPerson()
{
	EnableUpdate(true, eIUS_General);
	EnableHiddenSkinning(true);
	SetViewMode(eIVM_FirstPerson);
	AttachToHand(false);
	AttachArms(true, true);
	RestoreLayers();

/*
	if(m_stats.mounted)
	{
		ICharacterInstance* pChar = GetOwnerActor()?GetOwnerActor()->GetEntity()->GetCharacter(0):NULL;
		if(pChar)
			pChar->HideMaster(1);
	}*/

}

//------------------------------------------------------------------------
void CItem::OnEnterThirdPerson()
{
	EnableHiddenSkinning(false);
	AttachToHand(true);
	AttachArms(false, false);
	SetViewMode(eIVM_ThirdPerson);

	if(CItem* pSlave = static_cast<CItem*>(GetDualWieldSlave()))
		pSlave->OnEnterThirdPerson();
/*
	if(m_stats.mounted)
	{
		ICharacterInstance* pChar = GetOwnerActor()?GetOwnerActor()->GetEntity()->GetCharacter(0):NULL;
		if(pChar)
			pChar->HideMaster(0);
	}*/
}

//------------------------------------------------------------------------
void CItem::OnReset()
{
	//Hidden entities must have physics disabled
	if(!GetEntity()->IsHidden())
		GetEntity()->EnablePhysics(true);

	DestroyedGeometry(false);  
	m_stats.health = m_properties.hitpoints;

	UpdateDamageLevel();

	if(m_params.scopeAttachment)
		DrawSlot(CItem::eIGS_Aux1,false); //Hide secondary FP scope

	if (m_properties.mounted && m_params.mountable)
	{
		MountAt(GetEntity()->GetWorldPos());

		SEntityPhysicalizeParams params;
		params.mass = 0;
		params.nSlot = -1; // todo: -1 doesn't work for characters
		params.type = PE_STATIC;
		GetEntity()->Physicalize(params);
	}
	else
	{
		SetViewMode(eIVM_ThirdPerson);

		if (m_properties.pickable)
		{
			Physicalize(true, m_properties.physics);
			Pickalize(true, false);
		}
		else
			Physicalize(m_properties.physics, true);
	}

	GetEntity()->InvalidateTM();
}

//------------------------------------------------------------------------
void CItem::OnHit(float damage, const char* damageType)
{  
	if(!m_properties.hitpoints)
		return;

	if(damageType && !stricmp(damageType, "repair"))
	{
		if (m_stats.health < m_properties.hitpoints) //repair only to maximum 
		{
			bool destroyed = m_stats.health<=0.f;
			m_stats.health = min(float(m_properties.hitpoints),m_stats.health+damage);

			UpdateDamageLevel();

			if(destroyed && m_stats.health>0.f)
				OnRepaired();
		}
	}
	else
	{
		if (m_stats.health > 0.0f)
		{ 
			m_stats.health -= damage;

			UpdateDamageLevel();

			if (m_stats.health <= 0.0f)
			{
				m_stats.health = 0.0f;
				OnDestroyed();

				int n=(int)m_damageLevels.size();
				for (int i=0;i<n; ++i)
				{
					SDamageLevel &level=m_damageLevels[i];
					if (level.min_health==0 && level.max_health==0)
					{
						int slot=(m_stats.viewmode&eIVM_FirstPerson)?eIGS_FirstPerson:eIGS_ThirdPerson;

						SpawnEffect(slot, level.effect, level.helper, Vec3Constants<float>::fVec3_Zero, 
							Vec3Constants<float>::fVec3_OneZ, level.scale);
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CItem::UpdateDamageLevel()
{
	if (m_properties.hitpoints<=0 || m_damageLevels.empty())
		return;

	int slot=(m_stats.viewmode&eIVM_FirstPerson)?eIGS_FirstPerson:eIGS_ThirdPerson;

	int n=(int)m_damageLevels.size();
	int health=(int)((100.0f*MAX(0.0f, m_stats.health))/m_properties.hitpoints);
	for (int i=0;i<n; ++i)
	{
		SDamageLevel &level=m_damageLevels[i];
		if (level.min_health==0 && level.max_health==0)
			continue;

		if (level.min_health<=health && health<level.max_health)
		{
			if (level.effectId==-1)
				level.effectId=AttachEffect(slot, 0, true, level.effect.c_str(), level.helper.c_str(), 
					Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_OneZ, level.scale, true);
		}
		else if (level.effectId!=-1)
		{
			AttachEffect(0, level.effectId, false);
			level.effectId=-1;
		}
	}
}

//------------------------------------------------------------------------
void CItem::OnDestroyed()
{ 
  /* MR, 2007-02-09: shouldn't be needed 
	for (int i=0; i<eIGS_Last; i++)
	{
		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(i);
		if (pCharacter)
			pCharacter->SetAnimationSpeed(0);
	}*/

	DestroyedGeometry(true);

	if(!gEnv->pSystem->IsSerializingFile()) //don't replay destroy animations/effects
		PlayAction(g_pItemStrings->destroy);

	EnableUpdate(false);
}

//------------------------------------------------------------------------
void CItem::OnRepaired()
{
	for (int i=0; i<eIGS_Last; i++)
	{
		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(i);
		if (pCharacter)
			pCharacter->SetAnimationSpeed(1.0f);
	}

	DestroyedGeometry(false);

	EnableUpdate(true);
}

//------------------------------------------------------------------------
void CItem::OnDropped(EntityId actorId)
{
	m_pItemSystem->RegisterForCollection(GetEntityId());
}

//------------------------------------------------------------------------
void CItem::OnPickedUp(EntityId actorId, bool destroyed)
{
	if(GetISystem()->IsSerializingFile() == 1)
		return;

	CActor *pActor=GetActor(actorId);
	if (!pActor)
		return;

	if(gEnv->bMultiplayer && pActor->IsClient() && IsSelected())
	{
		COffHand* pOffHand = static_cast<COffHand*>(pActor->GetWeaponByClass(CItem::sOffHandClass));
		if(pOffHand && pOffHand->GetOffHandState()==eOHS_HOLDING_GRENADE)
			pOffHand->FinishAction(eOHA_RESET);
	}

	if (!IsServer())
		return;

	//if (destroyed && m_params.unique)
	{
		if (!m_bonusAccessoryAmmo.empty())
		{
			for (TAccessoryAmmoMap::iterator it=m_bonusAccessoryAmmo.begin(); it!=m_bonusAccessoryAmmo.end(); ++it)
			{
				int count=it->second;

				AddAccessoryAmmoToInventory(it->first,count,pActor);
			}

			m_bonusAccessoryAmmo.clear();
		}
	}

	m_pItemSystem->UnregisterForCollection(GetEntityId());

}
