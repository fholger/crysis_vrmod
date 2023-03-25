/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 6:9:2005   14:13 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "ItemSharedParams.h"
#include "Actor.h"


//------------------------------------------------------------------------
bool CItem::IsTwoHand() const
{
	return (m_params.two_hand != 0);
}

int CItem::TwoHandMode() const
{
	return m_params.two_hand;
}

//------------------------------------------------------------------------
bool CItem::SupportsDualWield(const char *itemName) const
{
	if (m_params.two_hand)
		return false;

	TDualWieldSupportMap::const_iterator it = m_sharedparams->dualWieldSupport.find(CONST_TEMPITEM_STRING(itemName));
	if (it != m_sharedparams->dualWieldSupport.end())
		return true;
	return false;
}

//------------------------------------------------------------------------
void CItem::ResetDualWield()
{
	if (m_dualWieldSlaveId)
	{
		IItem *pSlave = GetDualWieldSlave();
		if (pSlave)
			pSlave->ResetDualWield();
	}

	SetActionSuffix("");

	EnableSelect(true);
	m_dualWieldSlaveId = 0;
	m_dualWieldMasterId = 0;
}

//------------------------------------------------------------------------
IItem *CItem::GetDualWieldSlave() const
{
	return m_pItemSystem->GetItem(m_dualWieldSlaveId);
}


//------------------------------------------------------------------------
EntityId CItem::GetDualWieldSlaveId() const
{
	return m_dualWieldSlaveId;
}

//------------------------------------------------------------------------
IItem *CItem::GetDualWieldMaster() const
{
	return m_pItemSystem->GetItem(m_dualWieldMasterId);
}


//------------------------------------------------------------------------
EntityId CItem::GetDualWieldMasterId() const
{
	return m_dualWieldMasterId;
}


//------------------------------------------------------------------------
bool CItem::IsDualWield() const
{
	return (m_dualWieldSlaveId || m_dualWieldMasterId);
}

//------------------------------------------------------------------------
bool CItem::IsDualWieldSlave() const
{
	return m_dualWieldMasterId != 0;
}

//------------------------------------------------------------------------
bool CItem::IsDualWieldMaster() const
{
	return m_dualWieldSlaveId != 0;
}

//------------------------------------------------------------------------
void CItem::SetDualWieldMaster(EntityId masterId)
{
	m_dualWieldMasterId = masterId;

	SetActionSuffix(m_params.dual_wield_suffix.c_str());
}

//------------------------------------------------------------------------
void CItem::SetDualWieldSlave(EntityId slaveId)
{
	m_dualWieldSlaveId = slaveId;
	CItem *pSlave = static_cast<CItem *>(GetDualWieldSlave());
	if (!pSlave)
		return;

	SetActionSuffix(m_params.dual_wield_suffix.c_str());

	pSlave->EnableSelect(false);
	if (m_stats.hand == eIH_Left)
		pSlave->SetHand(eIH_Right);
	else
		pSlave->SetHand(eIH_Left);
}

//------------------------------------------------------------------------
void CItem::SetDualSlaveAccessory(bool noNetwork)
{
	if(!gEnv->bMultiplayer || (GetOwnerActor() && GetOwnerActor()->IsClient()))
	{
		CItem *pSlave = static_cast<CItem *>(GetDualWieldSlave());
		if (!pSlave)
			return;

		//Detach current accessories of the slave
		TAccessoryMap temp = pSlave->m_accessories;
		for (TAccessoryMap::const_iterator it=temp.begin(); it!=temp.end(); ++it)
		{
			if(m_accessories.find(it->first)==m_accessories.end())
				pSlave->SwitchAccessory(it->first.c_str()); //Only remove if not in the master
		}

		//Attach on the slave same accessories as "parent"
		for (TAccessoryMap::const_iterator it=m_accessories.begin(); it!=m_accessories.end(); ++it)
		{
			if(pSlave->m_accessories.find(it->first)==pSlave->m_accessories.end())
			{
				if(noNetwork)
					pSlave->AttachAccessory(it->first.c_str(), true, true, true);
				else
					pSlave->SwitchAccessory(it->first.c_str()); //Only add if not already attached
			}
		}
	}
}