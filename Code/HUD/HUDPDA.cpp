// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include <StlUtils.h>

#include "HUD.h"
#include "GameFlashAnimation.h"
#include "Menus/FlashMenuObject.h"
#include "../Game.h"
#include "../GameCVars.h"
#include "../GameRules.h"

#include "HUDPowerStruggle.h"

#undef HUD_CALL_LISTENERS
#define HUD_CALL_LISTENERS(func) \
{ \
	if (m_hudListeners.empty() == false) \
	{ \
		m_hudTempListeners = m_hudListeners; \
		for (std::vector<IHUDListener*>::iterator tmpIter = m_hudTempListeners.begin(); tmpIter != m_hudTempListeners.end(); ++tmpIter) \
			(*tmpIter)->func; \
	} \
}
//-----------------------------------------------------------------------------------------------------

void CHUD::InitPDA()
{
	if(m_pHUDPowerStruggle)
		m_pHUDPowerStruggle->InitEquipmentPacks();
}

//------------------------------------------------------------------------

int CHUD::CallScriptFunction(IEntity *pEntity,const char *function)
{
	int result=0;

	if (IScriptTable *pScriptTable=pEntity->GetScriptTable())
	{	
		HSCRIPTFUNCTION pfnGetBuyFlags=0;
		if (pScriptTable->GetValue(function, pfnGetBuyFlags))
		{
			Script::CallReturn(gEnv->pScriptSystem, pfnGetBuyFlags, pScriptTable, result);
			gEnv->pScriptSystem->ReleaseFunc(pfnGetBuyFlags);
		}
	}
	return result;
}

//-----------------------------------------------------------------------------------------------------


void CHUD::HandleFSCommandPDA(const char *strCommand,const char *strArgs)
{
	if(0 == strcmp(strCommand,"PDA"))
	{
		if(0 == strcmp(strArgs,"Open"))
		{
			// We do this on the fly because the Buy Menu could be updated as the level increases.
			InitPDA();
		}
		else if(0 == strcmp(strArgs,"Close"))
		{
		}
		else if(0 == strcmp(strArgs,"TabChanged"))
		{
			PlaySound(ESound_TabChanged);
		}
		else if(0 == strcmp(strArgs,"SelfClose"))
		{
			if(m_pModalHUD == &m_animPDA)
			{
				ShowPDA(false);

				if (m_changedSpawnGroup)
				{
					RequestRevive();
					m_changedSpawnGroup=false;
				}
			}
			else if(m_pModalHUD == &m_animBuyMenu)
				ShowBuyMenu(false);
		}
	}
	else if(0 == strcmp(strCommand,"UpdateBuyList"))
	{
		if(m_pHUDPowerStruggle)
		{
			m_pHUDPowerStruggle->UpdateBuyList(strArgs);
			HUD_CALL_LISTENERS(OnShowBuyMenuPage(m_pHUDPowerStruggle->m_eCurBuyMenuPage));
		}
	}
	else if(0 == strcmp(strCommand,"UpdatePackageList"))
	{
		if(m_pHUDPowerStruggle)
			m_pHUDPowerStruggle->UpdatePackageList();
	}
	else if(0 == strcmp(strCommand,"UpdatePackageItemList"))
	{
		if(strArgs && m_pHUDPowerStruggle)
			m_pHUDPowerStruggle->UpdatePackageItemList(strArgs);
	}
	else if(0 == strcmp(strCommand,"UpdateModifyPackage"))
	{
		if(strArgs && m_pHUDPowerStruggle)
		{
			int index = atoi(strArgs);
			m_pHUDPowerStruggle->UpdateModifyPackage(index);
		}
	}
	else if(0 == strcmp(strCommand,"ModifyPackage"))
	{
		if(strArgs && m_pHUDPowerStruggle)
		{
			int index = atoi(strArgs);
			const char *name = NULL;
			SFlashVarValue value(name);
			m_animBuyMenu.GetFlashPlayer()->GetVariable("m_backName",&value);
			m_pHUDPowerStruggle->SavePackage(value.GetConstStrPtr(),index);
		}
	}
	else if(0 == strcmp(strCommand,"CreatePackage"))
	{
		if(m_pHUDPowerStruggle)
		{
			const char *name = NULL;
			SFlashVarValue value(name);
			m_animBuyMenu.GetFlashPlayer()->GetVariable("m_backName",&value);
			name = value.GetConstStrPtr();
			m_pHUDPowerStruggle->SavePackage(name);
		}

	}
	else if(0 == strcmp(strCommand,"DeletePackage"))
	{
		if(strArgs && m_pHUDPowerStruggle)
		{
			int index = atoi(strArgs);
			m_pHUDPowerStruggle->DeletePackage(index);
		}
	}
	else if(0 == strcmp(strCommand,"SelectPackage"))
	{
		if(strArgs && m_pHUDPowerStruggle)
		{
			int index = atoi(strArgs);
			m_pHUDPowerStruggle->OnSelectPackage(index);
		}
	}
	else if(0 == strcmp(strCommand,"BuyPackage"))
	{
		if(strArgs && m_pHUDPowerStruggle)
		{
			int index = atoi(strArgs);
			m_pHUDPowerStruggle->BuyPackage(index);
		}
	}
	else if((0 == strcmp(strCommand,"Buy")) || (0 == strcmp(strCommand,"BuyAmmo")))
	{
		if(strArgs && m_pHUDPowerStruggle)
			m_pHUDPowerStruggle->Buy(strArgs);
	}
	else if(0 == strcmp(strCommand,"Drop"))
	{
		if(strArgs)
		{
			EntityId id = atoi(strArgs);
			if (CActor *pActor=static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor()))
			{
				if(IItem *pItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(id))
				{
					if (pItem->CanDrop())
						pActor->DropItem(pItem->GetEntityId());
				}
			}
		}
	}
	else if(0 == strcmp(strCommand,"PDATabChanged"))
	{
		PlaySound(ESound_TabChanged);
		int tab = atoi(strArgs);
		HUD_CALL_LISTENERS(PDATabChanged(tab));
	}
/*	else if(0 == strcmp(strCommand,"ChangeTeam"))
	{
		char strConsole[256];
		sprintf(strConsole,"team %s",strArgs);
		gEnv->pConsole->ExecuteString(strConsole);
	}*/
}

//-----------------------------------------------------------------------------------------------------

void CHUD::ActivateQuickMenuButton(EQuickMenuButtons button, bool active)
{
	if(active && !IsQuickMenuButtonActive(button)) //activate
		m_activeButtons += 1<<button; 
	else if(!active && IsQuickMenuButtonActive(button)) //deactivate
		m_activeButtons -= 1<<button;
	else
		return;

	m_animQuickMenu.Invoke("setActive", m_activeButtons);
}

//-----------------------------------------------------------------------------------------------------

void CHUD::SetQuickMenuButtonDefect(EQuickMenuButtons button, bool defect)
{
	if(defect && !IsQuickMenuButtonDefect(button)) //activate
		m_defectButtons += 1<<button; 
	else if(!defect && IsQuickMenuButtonDefect(button)) //deactivate
		m_defectButtons -= 1<<button;
	else
		return;

	m_animQuickMenu.Invoke("setDefect", m_defectButtons);
}

//-----------------------------------------------------------------------------------------------------

