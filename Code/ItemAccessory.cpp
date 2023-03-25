/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 8:9:2005   12:52 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "ItemSharedParams.h"
#include "Actor.h"
#include "Game.h"
#include "HUD/HUD.h"


//------------------------------------------------------------------------
CItem *CItem::AddAccessory(const ItemString& name)
{
	SAccessoryParams *pAccessoryParams=GetAccessoryParams(name);
	if (!pAccessoryParams)
	{
		GameWarning("Trying to add unknown accessory '%s' to item '%s'!", name.c_str(), GetEntity()->GetName());
		return 0;
	}

	char namebuf[128];
	_snprintf(namebuf, sizeof(namebuf), "%s_%s", GetEntity()->GetName(), name.c_str());
	namebuf[sizeof(namebuf)-1] = '\0';

	SEntitySpawnParams params;
	params.pClass = m_pEntitySystem->GetClassRegistry()->FindClass(name);
	params.sName = namebuf;
	params.nFlags = ENTITY_FLAG_NO_PROXIMITY | ENTITY_FLAG_CASTSHADOW;

	if (pAccessoryParams->client_only)
		params.nFlags |= ENTITY_FLAG_CLIENT_ONLY;

	if (!params.pClass)
	{
		GameWarning("Trying to add unknown accessory '%s' to item '%s'!", name.c_str(), GetEntity()->GetName());
		return 0;
	}

	EntityId accId=0;
	if (IEntity *pEntity = m_pEntitySystem->SpawnEntity(params))
		accId=pEntity->GetId();

	m_accessories.insert(TAccessoryMap::value_type(name, accId));
	if (accId)
		return static_cast<CItem *>(m_pItemSystem->GetItem(accId));
	return 0;
}

//------------------------------------------------------------------------
void CItem::RemoveAccessory(const ItemString& name)
{
	TAccessoryMap::iterator it = m_accessories.find(CONST_TEMPITEM_STRING(name));
	if (it != m_accessories.end())
	{
		m_pEntitySystem->RemoveEntity(it->second);
		m_accessories.erase(it);
	}
}

//------------------------------------------------------------------------
void CItem::RemoveAllAccessories()
{
	while(m_accessories.size() > 0)
		RemoveAccessory((m_accessories.begin())->first);
}

//------------------------------------------------------------------------
struct CItem::AttachAction
{
	AttachAction(CItem *pAccessory, SAccessoryParams *pParams)
		: accessory(pAccessory), params(pParams) {};
	void execute(CItem *item)
	{
		Vec3 position = item->GetSlotHelperPos(eIGS_ThirdPerson, params->attach_helper.c_str(), false);
		item->GetEntity()->AttachChild(accessory->GetEntity());
		Matrix34 tm(Matrix34::CreateIdentity());
		tm.SetTranslation(position);
		accessory->GetEntity()->SetLocalTM(tm);
		accessory->SetParentId(item->GetEntityId());
		accessory->SetViewMode(item->m_stats.viewmode);
		accessory->OnAttach(true);
		item->PlayLayer(params->attach_layer, eIPAF_Default|eIPAF_NoBlend);
		item->SetBusy(false);
		item->AccessoriesChanged();

		accessory->CloakSync(true);

		if(item->GetOwnerId() && !item->IsSelected())
		{
			accessory->Hide(true);
		}
	};

	CItem	*accessory;
	SAccessoryParams *params;
};

struct CItem::DetachAction
{
	DetachAction(CItem *pAccessory, SAccessoryParams *pParams)
	: accessory(pAccessory), params(pParams) {};
	void execute(CItem *item)
	{
		accessory->OnAttach(false);
		item->ResetCharacterAttachment(eIGS_FirstPerson, params->attach_helper.c_str());
		accessory->GetEntity()->DetachThis(0);
		accessory->SetParentId(0);
		accessory->Hide(true);
		ItemString accName (accessory->GetEntity()->GetClass()->GetName());
		item->RemoveAccessory(accName);
		item->SetBusy(false);
		item->AccessoriesChanged();
	};

	CItem	*accessory;
	SAccessoryParams *params;
};

void CItem::AttachAccessory(const ItemString& name, bool attach, bool noanim, bool force, bool initialSetup)
{
	if (!force && IsBusy())
		return;

	bool anim = !noanim && m_stats.fp;
	SAccessoryParams *params = GetAccessoryParams(name);
	if (!params)
		return;
	
	if (attach)
	{
		if (!IsAccessoryHelperFree(params->attach_helper))
			return;

		if (CItem *pAccessory = AddAccessory(name))
		{
			pAccessory->Physicalize(false, false);
			pAccessory->SetViewMode(m_stats.viewmode);

			if (!initialSetup)
				pAccessory->m_bonusAccessoryAmmo.clear();
			
			SetCharacterAttachment(eIGS_FirstPerson, params->attach_helper, pAccessory->GetEntity(), eIGS_FirstPerson, 0);
			SetBusy(true);

			AttachAction action(pAccessory, params);
			
			if (anim)
			{
				PlayAction(params->attach_action, 0, false, eIPAF_Default|eIPAF_NoBlend);
				m_scheduler.TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson), CSchedulerAction<AttachAction>::Create(action), false);
			}
			else
				action.execute(this);

		}
	}
	else
	{
		if (CItem *pAccessory = GetAccessory(name))
		{
			DetachAction action(pAccessory, params);

			if (anim)
			{
				StopLayer(params->attach_layer, eIPAF_Default|eIPAF_NoBlend);
				PlayAction(params->detach_action, 0, false, eIPAF_Default|eIPAF_NoBlend);
				m_scheduler.TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson), CSchedulerAction<DetachAction>::Create(action), false);
				SetBusy(true);
			}
			else
			{
				SetBusy(true);
				action.execute(this);
			}
		}
	}

	//Skip all this for the offhand
	if(GetEntity()->GetClass()!=CItem::sOffHandClass)
		FixAccessories(params, attach);

	//Attach silencer to 2nd SOCOM
	/////////////////////////////////////////////////////////////
	bool isDualWield = IsDualWieldMaster();
	CItem *dualWield = 0;

	if (isDualWield)
	{
		IItem *slave = GetDualWieldSlave();
		if (slave && slave->GetIWeapon())
			dualWield = static_cast<CItem *>(slave);
		else
			isDualWield = false;
	}

	if(isDualWield)
		dualWield->AttachAccessory(name,attach,noanim);

	//////////////////////////////////////////////////////////////
}

//------------------------------------------------------------------------
void CItem::AttachAccessoryPlaceHolder(const ItemString& name, bool attach)
{
	const SAccessoryParams *params = GetAccessoryParams(name);
	if (!params)
		return;

	CItem *pPlaceHolder = GetAccessoryPlaceHolder(name);

	if (!pPlaceHolder)
		return;

	if (attach)
	{
		SetCharacterAttachment(eIGS_FirstPerson, params->attach_helper, pPlaceHolder->GetEntity(), eIGS_FirstPerson, 0);
		PlayLayer(params->attach_layer, eIPAF_FirstPerson, false);
	}
	else
	{
		ResetCharacterAttachment(eIGS_FirstPerson, params->attach_helper);
		StopLayer(params->attach_layer, eIPAF_FirstPerson, false);
	}
}

//------------------------------------------------------------------------
CItem *CItem::GetAccessoryPlaceHolder(const ItemString& name)
{
	IInventory *pInventory = GetActorInventory(GetOwnerActor());
	if (!pInventory)
		return 0;

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
	int slotId = pInventory->FindNext(pClass, 0, -1, false);
	if (slotId >= 0)
		return static_cast<CItem *>(m_pItemSystem->GetItem(pInventory->GetItem(slotId)));

	return 0;
}

//------------------------------------------------------------------------
CItem *CItem::GetAccessory(const ItemString& name)
{
	TAccessoryMap::iterator it = m_accessories.find(CONST_TEMPITEM_STRING(name));
	if (it != m_accessories.end())
		return static_cast<CItem *>(m_pItemSystem->GetItem(it->second));
	
	return 0;
}

//------------------------------------------------------------------------
CItem::SAccessoryParams *CItem::GetAccessoryParams(const ItemString& name)
{
	TAccessoryParamsMap::iterator it = m_sharedparams->accessoryparams.find(CONST_TEMPITEM_STRING(name));
	if (it != m_sharedparams->accessoryparams.end())
		return &it->second;

	return 0;
}

//------------------------------------------------------------------------
bool CItem::IsAccessoryHelperFree(const ItemString& helper)
{
	for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); ++it)
	{
		SAccessoryParams *params = GetAccessoryParams(it->first);
		
		if (!params || params->attach_helper == helper)
			return false;
	}

	return true;
}

//------------------------------------------------------------------------
void CItem::InitialSetup()
{
	if(!(GetISystem()->IsSerializingFile() && GetGameObject()->IsJustExchanging()))
	{
		if (IsServer())
		{
			for (TInitialSetup::iterator it = m_initialSetup.begin(); it != m_initialSetup.end(); ++it)
			{
				//if (!IsClient())
				//	AddAccessory(*it);
				//else
				AttachAccessory(*it, true, true, true, true);
			}
		}
	}
}

//------------------------------------------------------------------------
void CItem::ReAttachAccessories()
{
	for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); ++it)
		ReAttachAccessory(it->first);
}

//------------------------------------------------------------------------
void CItem::ReAttachAccessory(const ItemString& name)
{
	CItem *pAccessory = GetAccessory(name);
	SAccessoryParams *params = GetAccessoryParams(name);

	if (pAccessory && params)
	{
		SetCharacterAttachment(eIGS_FirstPerson, params->attach_helper.c_str(), pAccessory->GetEntity(), eIGS_FirstPerson, 0);

		Vec3 position = GetSlotHelperPos(eIGS_ThirdPerson, params->attach_helper.c_str(), false);
		GetEntity()->AttachChild(pAccessory->GetEntity());
		Matrix34 tm(Matrix34::CreateIdentity());
		tm.SetTranslation(position);
		pAccessory->GetEntity()->SetLocalTM(tm);
		pAccessory->SetParentId(GetEntityId());
		pAccessory->Physicalize(false, false);
		PlayLayer(params->attach_layer, eIPAF_Default|eIPAF_NoBlend);
	}
}

//------------------------------------------------------------------------
void CItem::ReAttachAccessory(EntityId id)
{
	IEntity* pEntity = GetISystem()->GetIEntitySystem()->GetEntity(id);
	if(pEntity)
	{
		ItemString className = pEntity->GetClass()->GetName();
		m_accessories[className] = id;
		ReAttachAccessory(className);
	}
}

//------------------------------------------------------------------------
void CItem::AccessoriesChanged()
{
}

//------------------------------------------------------------------------
void CItem::SwitchAccessory(const ItemString& accessory)
{
	GetGameObject()->InvokeRMI(SvRequestAttachAccessory(), RequestAttachAccessoryParams(accessory), eRMI_ToServer);
}

//------------------------------------------------------------------------
void CItem::DoSwitchAccessory(const ItemString& inAccessory)
{
	const ItemString& accessory = (inAccessory == g_pItemStrings->SCARSleepAmmo) ? g_pItemStrings->SCARTagAmmo : inAccessory;

	SAccessoryParams *params = GetAccessoryParams(accessory);
	bool attached = false;
	if (params)
	{
		ItemString replacing;
		bool exclusive = false;
		for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); it++)
		{
			SAccessoryParams *p = GetAccessoryParams(it->first);

			if (p && p->attach_helper == params->attach_helper)
			{
				replacing = it->first;
				attached = true;
				exclusive = p->exclusive;
			}
		}
		if (attached)
		{
			AttachAccessory(replacing, false, true, true);
			if (accessory != replacing)
				AttachAccessory(accessory, true, true, true);
		}
	}

	if (!attached)
		AttachAccessory(accessory, true, true, true);
}

//------------------------------------------------------------------------
class ScheduleAttachClassItem
{
public:
	ScheduleAttachClassItem(const char *_name, bool _attach)
	{
		name = _name;
		attach = _attach;
	}
	virtual void execute(CItem *item)
	{
		item->AttachAccessory(name, attach, true);
	};

private:
	const char *name;
	bool attach;
};

void CItem::ScheduleAttachAccessory(const ItemString& accessoryName, bool attach)
{
	GetScheduler()->ScheduleAction(CSchedulerAction<ScheduleAttachClassItem>::Create(ScheduleAttachClassItem(accessoryName, attach)));
}

//------------------------------------------------------------------------
const char *CItem::CurrentAttachment(const ItemString& attachmentPoint)
{
	for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); it++)
	{
		SAccessoryParams *params= GetAccessoryParams(it->first);

		if (params && params->attach_helper == attachmentPoint)
			return it->first.c_str();
	}
	return 0;
}

//------------------------------------------------------------------------
void CItem::ResetAccessoriesScreen(CActor* pOwner)
{
	m_modifying = false;
	m_transitioning = false;

	StopLayer(g_pItemStrings->modify_layer, eIPAF_Default, false);

	if(pOwner && pOwner->IsClient())
	{
		if(m_params.update_hud)
		{
			SAFE_HUD_FUNC(WeaponAccessoriesInterface(false, true));
		}
	}
}

//-----------------------------------------------------------------------
void CItem::PatchInitialSetup()
{
	const char* temp = NULL;
	// check if the initial setup accessories has been overridden in the level
	SmartScriptTable props;
	if (GetEntity()->GetScriptTable() && GetEntity()->GetScriptTable()->GetValue("Properties", props))
	{
		if(props->GetValue("initialSetup",temp) && temp !=NULL && temp[0]!=0)
		{
			m_properties.initialSetup = temp;
		}
	}

	//Replace initial setup from weapon xml, with initial setup defined for the entity (if neccesary)
	if(!m_properties.initialSetup.empty())
	{
		m_initialSetup.resize(0);

		//Different accessory names are separated by ","

		string::size_type lastPos = m_properties.initialSetup.find_first_not_of(",", 0);
		string::size_type pos = m_properties.initialSetup.find_first_of(",", lastPos);

		while (string::npos != pos || string::npos != lastPos)
		{
			//Add to initial setup
			const char *name = m_properties.initialSetup.substr(lastPos, pos - lastPos).c_str();
			m_initialSetup.push_back(name);

			lastPos = m_properties.initialSetup.find_first_not_of(",", pos);
			pos = m_properties.initialSetup.find_first_of(",", lastPos);
		}		
	}
}

//------------------------------------------------------------------------
void CItem::OnAttach(bool attach)
{
	if (attach && m_parentId)
	{
		if (CItem *pParent=static_cast<CItem *>(m_pItemSystem->GetItem(m_parentId)))
		{
			const SStats &stats=pParent->GetStats();
			if (stats.mounted)
			{
				pParent->ForceSkinning(true);
				PlayAction(m_idleAnimation[eIGS_FirstPerson], 0, true);
				ForceSkinning(true);
			}
		}
	}
}