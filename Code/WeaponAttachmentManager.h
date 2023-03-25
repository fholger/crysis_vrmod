/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: C++ WeaponAttachmentManager Implementation

-------------------------------------------------------------------------
History:
- 30:5:2007   16:55 : Created by Benito Gangoso Rodriguez

*************************************************************************/

#ifndef __WEAPON_ATTACHMENT_MANAGER_H__
#define __WEAPON_ATTACHMENT_MANAGER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <IItemSystem.h>
#include "ItemString.h"

class CActor;

class CWeaponAttachmentManager
{
public:

	//TODO: Change to ItemStrings
	typedef std::map<ItemString,EntityId> TBoneAttachmentMap;
	typedef std::list<EntityId>       TAttachedWeaponsList;

	CWeaponAttachmentManager(CActor* _pOwner);
	~CWeaponAttachmentManager();

	bool Init();
	void HideAllAttachments(bool hide);

	void RequestAttachWeaponToBack(EntityId weaponId) { m_itemToBack = weaponId; }
	void RequestAttachWeaponToHand(EntityId weaponId) { m_itemToHand = weaponId; }

	void DoHandToBackSwitch();
	void DoBackToHandSwitch();

	void SetWeaponAttachment(bool attach, const char* attachmentName, EntityId weaponId);
	TAttachedWeaponsList GetAttachedWeapons() { return m_attachedWeaponList; }

	bool IsAttachmentFree(const char* attachmentName);
	
protected:

	//Create different weapon attachments
	void CreatePlayerBoneAttachments();
	
	//Create c4 projected attachments
	void CreatePlayerProjectedAttachments();

private:
	
	CActor *m_pOwner;
	
	TBoneAttachmentMap		m_boneAttachmentMap;
	TAttachedWeaponsList	m_attachedWeaponList;

	EntityId		m_itemToBack;
	EntityId    m_itemToHand;

	IItemSystem* m_pItemSystem;
};

#endif //__WEAPON_ATTACHMENT_MANAGER_H__