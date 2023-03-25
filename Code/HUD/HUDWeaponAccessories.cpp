// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"

#include "HUD.h"
#include "Weapon.h"

//-----------------------------------------------------------------------------------------------------

void CHUD::LoadWeaponAccessories(XmlNodeRef weaponXmlNode)
{
	if(weaponXmlNode)
	{
		TMapWeaponAccessoriesHelperOffsets mapWeaponAccessoriesHelpersOffsets;

		int iNumChildren = weaponXmlNode->getChildCount();
		for(int iChild=0; iChild<iNumChildren; iChild++)
		{
			XmlNodeRef helperXmlNode = weaponXmlNode->getChild(iChild);
			SWeaponAccessoriesHelpersOffsets offsets;
			offsets.iX = atoi(helperXmlNode->getAttr("X"));
			offsets.iY = atoi(helperXmlNode->getAttr("Y"));
			mapWeaponAccessoriesHelpersOffsets.insert(std::make_pair(helperXmlNode->getTag(),offsets));
		}

		m_mapWeaponAccessoriesHelpersOffsets.insert(std::make_pair(weaponXmlNode->getTag(),mapWeaponAccessoriesHelpersOffsets));
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::LoadWeaponsAccessories()
{
	XmlNodeRef weaponAccessoriesXmlNode = GetISystem()->LoadXmlFile("Libs/UI/WeaponAccessories.xml");
	if(weaponAccessoriesXmlNode)
	{
		int iNumChildren = weaponAccessoriesXmlNode->getChildCount();
		for(int iChild=0; iChild<iNumChildren; iChild++)
		{
			LoadWeaponAccessories(weaponAccessoriesXmlNode->getChild(iChild));
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::AdjustWeaponAccessory(const char *szWeapon,const char *szHelper,Vec3 *pvScreenSpace)
{
	std::map<string,TMapWeaponAccessoriesHelperOffsets>::iterator iter1 = m_mapWeaponAccessoriesHelpersOffsets.find(szWeapon);
	if(iter1 != m_mapWeaponAccessoriesHelpersOffsets.end())
	{
		TMapWeaponAccessoriesHelperOffsets::iterator iter2 = (*iter1).second.find(szHelper);
		if(iter2 != (*iter1).second.end())
		{
			pvScreenSpace->x += ((*iter2).second).iX;
			pvScreenSpace->y += ((*iter2).second).iY;
		}
	}
}

//-----------------------------------------------------------------------------------------------------

bool CHUD::UpdateWeaponAccessoriesScreen()
{
	bool bWeaponHasAttachments = false;

	m_animWeaponAccessories.Invoke("clearAllSlotButtons");

	CWeapon *pCurrentWeapon = GetCurrentWeapon();
	if(pCurrentWeapon)
	{
		const CItem::THelperVector& helpers = pCurrentWeapon->GetAttachmentHelpers();
		for(int iHelper=0; iHelper<helpers.size(); iHelper++)
		{
			CItem::SAttachmentHelper helper = helpers[iHelper];
			if (helper.slot != CItem::eIGS_FirstPerson)
				continue;

			if(pCurrentWeapon->HasAttachmentAtHelper(helper.name))
			{
				// marcok: make sure the vars are correctly mapped
				SFlashVarValue args[3] = {helper.name.c_str(), "", ""};
				m_animWeaponAccessories.Invoke("addSlotButton", args, 3);
				//should really do not use dynamic strings here
				string strControl("Root.slot_");	strControl += helper.name.c_str();
				string strToken("hud.WS");	strToken += helper.name.c_str();	
				string strTokenX = strToken + "X";
				string strTokenY = strToken + "Y";
				m_animWeaponAccessories.AddVariable(strControl.c_str(),"_x",strTokenX.c_str(),1.0f,0.0f);
				m_animWeaponAccessories.AddVariable(strControl.c_str(),"_y",strTokenY.c_str(),1.0f,0.0f);

				string curAttach;
				{
					const char* szCurAttach = pCurrentWeapon->CurrentAttachment(helper.name);
					curAttach = (szCurAttach ? szCurAttach : "");
				}
				std::vector<string> attachments;
				pCurrentWeapon->GetAttachmentsAtHelper(helper.name, attachments);
				int iSelectedIndex = 0;
				int iCount = 0;
				if(attachments.size() > 0)
				{
					if(strcmp(helper.name,"magazine") && strcmp(helper.name,"attachment_front") && strcmp(helper.name,"energy_source_helper") && strcmp(helper.name,"shell_grenade"))
					{
						if(!strcmp(helper.name,"attachment_top"))
						{
							SFlashVarValue args[3] = {helper.name.c_str(), "@IronSight", "NoAttachment"};
							m_animWeaponAccessories.Invoke("addSlotButton", args, 3);
							++iCount;
						}
						else
						{
							SFlashVarValue args[3] = {helper.name.c_str(), "@NoAttachment", "NoAttachment"};
							m_animWeaponAccessories.Invoke("addSlotButton", args, 3);
							++iCount;
						}
					}
					for(int iAttachment=0; iAttachment<attachments.size(); iAttachment++)
					{
						// Ignore this item: it's a very special one!
						if(attachments[iAttachment] != "GrenadeShell")
						{
							string sName("@");
							sName.append(attachments[iAttachment]);
							SFlashVarValue args[3] = {helper.name.c_str(), sName.c_str(), attachments[iAttachment].c_str()};
							m_animWeaponAccessories.Invoke("addSlotButton", args, 3);
							if(curAttach == attachments[iAttachment])
							{
								iSelectedIndex = iCount;
							}
							bWeaponHasAttachments = bWeaponHasAttachments || iCount>0;
							++iCount;
						}
					}
				}
				if(curAttach)
				{
					SFlashVarValue args[2] = {helper.name.c_str(), iSelectedIndex};
					m_animWeaponAccessories.Invoke("selectSlotButton", args, 2);
				}
			}
			else ; // no attachment found for this helper
		}
		m_animWeaponAccessories.GetFlashPlayer()->Advance(0.25f);
	}

	return bWeaponHasAttachments;
}

//-----------------------------------------------------------------------------------------------------

