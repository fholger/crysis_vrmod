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

#include "StdAfx.h"
#include "WeaponAttachmentManager.h"
#include "Actor.h"
#include "Item.h"


#define MAX_WEAPON_ATTACHMENTS 3 

//Bone & Attachment table (This should be in a nice file!!)
namespace
{
	const char gBoneTable[MAX_WEAPON_ATTACHMENTS][32] =
	{
		"Bip01 Spine2",
		"Bip01 Spine2",
		"alt_weapon_bone01"
		//"weaponPos_pistol_R_leg",
		//"weaponPos_pistol_L_leg"
	};
	const char gAttachmentTable[MAX_WEAPON_ATTACHMENTS][32] = 
	{
		"back_item_attachment_01",
		"back_item_attachment_02",
		"left_hand_grenade_attachment"
		//"pistol_attachment_right",
		//"pistol_attachment_left"
	};
	const Vec3 gOffsetTable[MAX_WEAPON_ATTACHMENTS] = 
	{
		Vec3(0.21944635f,-0.14831634f,1.3135500f),
		Vec3(-0.18169385f,-0.15057199f,1.4539498f),
		Vec3(-0.74055958f,0.17096956f,1.2127678f)
		//Vec3(0,0,0),
		//Vec3(0,0,0)
	};
	const Quat gRotationTable[MAX_WEAPON_ATTACHMENTS] =
	{
		Quat(-0.53083998f, 0.42361471f,0.54312921f,-0.49373195f),
		Quat(0.53463978f,-0.41727185f,0.52943635f,-0.50965005f),
		Quat(0.7229929f,-0.37515444f,0.46481827f,0.34710974f)
		//Quat(Quat::CreateIdentity()),
		//Quat(Quat::CreateIdentity())
	};
	
}

CWeaponAttachmentManager::CWeaponAttachmentManager(CActor* _pOwner):
m_pOwner(_pOwner),
m_itemToBack(0),
m_itemToHand(0)
{
	m_boneAttachmentMap.clear();
	m_attachedWeaponList.clear();
}

CWeaponAttachmentManager::~CWeaponAttachmentManager()
{
	m_boneAttachmentMap.clear();
	m_attachedWeaponList.clear();
}

bool CWeaponAttachmentManager::Init()
{
	if(gEnv->bMultiplayer)
	{
		CreatePlayerBoneAttachments(); //Only create the attachment, some are needed
		return false;
	}
	else if(m_pOwner->GetActorSpecies()!=eGCT_HUMAN)
		return false;

	m_pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();

	CreatePlayerBoneAttachments();
	CreatePlayerProjectedAttachments();

	return true;
}

//======================================================================
void CWeaponAttachmentManager::CreatePlayerBoneAttachments()
{
		
	if (ICharacterInstance* pCharInstance = m_pOwner->GetEntity()->GetCharacter(0))
	{
		IAttachmentManager* pAttachmentManager = pCharInstance->GetIAttachmentManager(); 
		IAttachment *pAttachment = NULL;
		
		for(int i=0; i<MAX_WEAPON_ATTACHMENTS; i++)
		{
			pAttachment = pAttachmentManager->GetInterfaceByName(gAttachmentTable[i]);
			if(!pAttachment)
			{
				//Attachment doesn't exist, create it
				pAttachment = pAttachmentManager->CreateAttachment(gAttachmentTable[i],CA_BONE,gBoneTable[i]);
				if(pAttachment)
				{
					m_boneAttachmentMap.insert(TBoneAttachmentMap::value_type(gAttachmentTable[i],0));
					if(pAttachment && !gOffsetTable[i].IsZero())
					{
						pAttachment->SetAttAbsoluteDefault( QuatT(gRotationTable[i],gOffsetTable[i]) );
						pAttachment->ProjectAttachment();
					}
				}
			}
		}

	}

}

//=======================================================================
void CWeaponAttachmentManager::CreatePlayerProjectedAttachments()
{

	Vec3 c4FrontPos(-0.0105f,0.2233f,1.297f),c4BackPos(0.00281f,-0.2493f,1.325f);
	Quat c4FrontRot(-0.0368f,-0.0278f,0.0783f,-0.9958f),c4BackRot(1,0,0,0);
	//Creates on init c4 face attachments
	if (ICharacterInstance* pCharInstance = m_pOwner->GetEntity()->GetCharacter(0))
	{
		IAttachmentManager* pAttachmentManager = pCharInstance->GetIAttachmentManager(); 
		IAttachment *pAttachment = NULL;
		pAttachment = pAttachmentManager->GetInterfaceByName("c4_front");
		if(!pAttachment)
		{
			//Attachment doesn't exist, create it
			pAttachment= pAttachmentManager->CreateAttachment("c4_front",CA_FACE,0);
			if(pAttachment)
			{
				pAttachment->SetAttAbsoluteDefault( QuatT(c4FrontRot,c4FrontPos) );
				pAttachment->ProjectAttachment();
			}
		}
		pAttachment = NULL;
		pAttachment = pAttachmentManager->GetInterfaceByName("c4_back");
		if(!pAttachment)
		{
			//Attachment doesn't exist, create it
			pAttachment= pAttachmentManager->CreateAttachment("c4_back",CA_FACE,0);
			if(pAttachment)
			{
				pAttachment->SetAttAbsoluteDefault( QuatT(c4BackRot,c4BackPos) );
				pAttachment->ProjectAttachment();
			}
		}
	}
}

//======================================================================
void CWeaponAttachmentManager::DoHandToBackSwitch()
{
	CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(m_itemToBack));
	if(pItem)
	{
		pItem->AttachToHand(false);
		pItem->AttachToBack(true);
	}
	m_itemToBack = 0;
}

//=====================================================================
void CWeaponAttachmentManager::DoBackToHandSwitch()
{
	CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(m_itemToHand));
	if(pItem)
	{
		pItem->AttachToBack(false);
		pItem->AttachToHand(true);
	}
	m_itemToHand = 0;
}

//=======================================================================
void CWeaponAttachmentManager::SetWeaponAttachment(bool attach, const char* attachmentName, EntityId weaponId)
{
	TBoneAttachmentMap::iterator it = m_boneAttachmentMap.find(CONST_TEMPITEM_STRING(attachmentName));
	if(it!=m_boneAttachmentMap.end())
	{
		if(attach)
		{
			it->second = weaponId;
			stl::push_back_unique(m_attachedWeaponList,weaponId);
		}
		else
		{
			it->second = 0;
			m_attachedWeaponList.remove(weaponId);
		}
	}
}

//=========================================================================
bool CWeaponAttachmentManager::IsAttachmentFree(const char* attachmentName)
{
	TBoneAttachmentMap::iterator it = m_boneAttachmentMap.find(CONST_TEMPITEM_STRING(attachmentName));
	if(it!=m_boneAttachmentMap.end())
	{
		if(it->second==0)
			return true;
		else
			return false;
	}

	return false;
}

//========================================================================
void CWeaponAttachmentManager::HideAllAttachments(bool hide)
{
	TAttachedWeaponsList::const_iterator it = m_attachedWeaponList.begin();
	while(it!=m_attachedWeaponList.end())
	{
		CItem *pItem = static_cast<CItem*>(m_pItemSystem->GetItem(*it));
		if(pItem)
			pItem->Hide(hide);
		it++;
	}
}