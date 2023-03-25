/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: PowerStruggle mode HUD code (refactored from old HUD code)

-------------------------------------------------------------------------
History:
- 21:02:20067  20:00 : Created by Jan Müller

*************************************************************************/

#include "StdAfx.h"
#include "HUDPowerStruggle.h"

#include "HUD.h"
#include "GameFlashAnimation.h"
#include "Menus/FlashMenuObject.h"
#include "../Game.h"
#include "../GameCVars.h"
#include "../GameRules.h"
#include "Weapon.h"
#include "HUDVehicleInterface.h"
#include "Menus/OptionsManager.h"

#define HUD_CALL_LISTENERS_PS(func) \
{ \
	if (g_pHUD->m_hudListeners.empty() == false) \
	{ \
	g_pHUD->m_hudTempListeners = g_pHUD->m_hudListeners; \
	for (std::vector<CHUD::IHUDListener*>::iterator tmpIter = g_pHUD->m_hudTempListeners.begin(); tmpIter != g_pHUD->m_hudTempListeners.end(); ++tmpIter) \
	(*tmpIter)->func; \
	} \
}

static inline bool SortByPrice(const CHUDPowerStruggle::SItem &rItem1,const CHUDPowerStruggle::SItem &rItem2)
{
	return rItem1.iPrice < rItem2.iPrice;
}

//-----------------------------------------------------------------------------------------------------

CHUDPowerStruggle::CHUDPowerStruggle(CHUD *pHUD, CGameFlashAnimation *pBuyMenu, CGameFlashAnimation *pHexIcon) : 
g_pHUD(pHUD), g_pBuyMenu(pBuyMenu), g_pHexIcon(pHexIcon)
{
	Reset();
	m_animSwingOMeter.Load("Libs/UI/HUD_Swing-O-Meter.gfx", eFD_Center, eFAF_ManualRender|eFAF_Visible);
	m_animSwingOMeter.GetFlashPlayer()->SetVisible(true);
}

CHUDPowerStruggle::~CHUDPowerStruggle()
{
}

void CHUDPowerStruggle::Reset()
{
	m_bInBuyZone = false;
	m_bInServiceZone = false;
	m_eCurBuyMenuPage = E_LOADOUT;
	m_nkLeft = true; //NK is left on default
	m_factoryTypes.resize(5);
	m_serviceZoneTypes.resize(5);
	m_teamId = 0;
	m_gotpowerpoints = false;
	m_protofactory = 0;
	m_capturing = false;
	m_captureProgress = 0.0f;

	m_constructing=false;
	m_constructionQueued=false;
	m_constructionTime=0.0f;
	m_constructionTimer=0.0f;
	m_lastConstructionTime = -1;
	m_lastBuildingTime = -1;

	m_lastPurchase.iPrice = 0;
	m_thisPurchase.iPrice = 0;

	m_lastTeamCol = -1;
	m_lastPowerPoints = -1;
	m_lastAlienArg1 = m_lastAlienArg2 = m_lastPowerArg1 = m_lastPowerArg2 = -1;

	m_currentHexIconState = E_HEX_ICON_NONE;
	m_currentBuyZones.clear();
	m_currentServiceZones.clear();
}

void DrawBar(float x, float y, float width, float height, float border, float progress, const ColorF &color0, const ColorF &color1, const char *text, const ColorF &textColor, float bgalpha)
{
	float sy=gEnv->pRenderer->ScaleCoordY(y);
	float sx=gEnv->pRenderer->ScaleCoordX(x+width*0.5f);

	ColorF interp;
	interp.lerpFloat(color0, color1, progress);

	float currw=width*progress;
	gEnv->pRenderer->Draw2dImage(x-border, y-border, width+border+border, height+border+border, 0, 0, 0, 0, 0, 0, 0.0f,0.0f, 0.0f, bgalpha);

	gEnv->pRenderer->Draw2dImage(x, y, currw, height, 0, 0, 0, 0, 0, 0, interp.r, interp.g, interp.b, 0.75f);
	gEnv->pRenderer->Draw2dImage(x+currw, y, width-currw, height, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.35f);

	if (text && text[0])
		gEnv->pRenderer->Draw2dLabel(sx, sy, 1.6f, (float*)&textColor, true, "%s", text);
}

void CHUDPowerStruggle::Update(float fDeltaTime)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	if (pGameRules && m_animSwingOMeter.IsLoaded() && !stricmp(pGameRules->GetEntity()->GetClass()->GetName(), "PowerStruggle"))
	{
		int teamId=0;
		IActor *pLocalActor=g_pGame->GetIGameFramework()->GetClientActor();
		if (pLocalActor)
		{
			teamId=pGameRules->GetTeam(pLocalActor->GetEntityId());

			// if local actor is currently spectating another player, use the team of the player we're watching...
			CActor* pCActor = static_cast<CActor*>(pLocalActor);
			if(pCActor && pCActor->GetSpectatorMode() == CActor::eASM_Follow)
			{
				teamId = pGameRules->GetTeam(pCActor->GetSpectatorTarget());
			}
		}

		if (!m_gotpowerpoints)
		{
			m_powerpoints.resize(0);

			IEntityClass *pAlienEnergyPoint=gEnv->pEntitySystem->GetClassRegistry()->FindClass("AlienEnergyPoint");
			IEntityClass *pHQ=gEnv->pEntitySystem->GetClassRegistry()->FindClass("HQ");
			IEntityClass *pFactory=gEnv->pEntitySystem->GetClassRegistry()->FindClass("Factory");

			IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();
			while (!pIt->IsEnd())
			{
				if (IEntity *pEntity = pIt->Next())
				{
					IEntityClass *pClass=pEntity->GetClass();

					if (pClass == pAlienEnergyPoint)
					{
						m_powerpoints.push_back(pEntity->GetId());
					}
					else if (pClass == pFactory)
					{
						SmartScriptTable props;
						if (pEntity->GetScriptTable() && pEntity->GetScriptTable()->GetValue("Properties", props))
						{
							int proto=0;
							if (props->GetValue("bPowerStorage", proto) && proto)
								m_protofactory=pEntity->GetId();
						}
					}
					else if (pClass == pHQ)
						m_hqs.push_back(pEntity->GetId());
				}
			}

			m_gotpowerpoints=true;
			int points = (int)m_powerpoints.size();
			if(m_lastPowerPoints != points)
			{
				m_animSwingOMeter.Invoke("setAliens", SFlashVarValue((int)m_powerpoints.size()));
				m_lastPowerPoints = points;
			}
		}

		if (teamId==0)
			teamId=1;

		if (teamId!=m_teamId)
		{
			m_animSwingOMeter.Invoke("setOwnTeam", teamId==1?"NK":"US");
			m_teamId=teamId;
		}

		float power[2]={0.0f};
		float hq[2]={0.0f};
		int aliens[2]={0};
		EntityId proto[2]={0};

		GetTeamStatus(1, power[0], hq[0], aliens[0], proto[0]);
		GetTeamStatus(2, power[1], hq[1], aliens[1], proto[1]);

		int ihq[2]={(int)(hq[0]*100.0f), (int)(hq[1]*100.0f)};
		static char chq[2][16];
		memset(chq, 0, 2*16);
		itoa(ihq[0], chq[0], 10);
		itoa(ihq[1], chq[1], 10);

		wstring hqFormatter[2];
		hqFormatter[0] = g_pGame->GetHUD()->LocalizeWithParams("@mp_HQLife", false, chq[0]);
		hqFormatter[1] = g_pGame->GetHUD()->LocalizeWithParams("@mp_HQLife", false, chq[1]);

		SFlashVarValue aliensarg[2]={aliens[(teamId==1)?0:1], aliens[(teamId==1)?1:0]};
		if(aliensarg[0].GetInt() != m_lastAlienArg1 || aliensarg[1].GetInt() != m_lastAlienArg2)
		{
			m_animSwingOMeter.Invoke("showAliens", aliensarg, 2);
			m_lastAlienArg1 = aliensarg[0].GetInt();
			m_lastAlienArg2 = aliensarg[1].GetInt();
		}

		SFlashVarValue hqarg[2]={hqFormatter[(teamId==1)?0:1].c_str(), hqFormatter[(teamId==1)?1:0].c_str()};
		if(wcscmp(hqarg[0].GetConstWstrPtr(), m_lastHQArg1.c_str()) || wcscmp(hqarg[1].GetConstWstrPtr(), m_lastHQArg2.c_str()) )
		{
			m_animSwingOMeter.Invoke("setStatusBars", hqarg, 2);
			m_lastHQArg1 = hqarg[0].GetConstWstrPtr();
			m_lastHQArg2 = hqarg[1].GetConstWstrPtr();
		}
		
		SFlashVarValue powerarg[2]={int(cry_floorf(power[(teamId==1)?0:1])), int(cry_floorf(power[(teamId==1)?1:0]))};
		if(powerarg[0].GetInt() != m_lastPowerArg1 || powerarg[1].GetInt() != m_lastPowerArg2)
		{
			if(teamId==1 && m_lastPowerArg1!=power[0])
				UpdateEnergyBuyList(m_lastPowerArg1, power[0]);
			else if(teamId==2 && m_lastPowerArg2!=power[1])
				UpdateEnergyBuyList(m_lastPowerArg1, power[1]);
			m_animSwingOMeter.Invoke("setLoadBar", powerarg, 2);
			m_lastPowerArg1 = powerarg[0].GetInt();
			m_lastPowerArg2 =	powerarg[1].GetInt();
		}

		if (teamId!=0)
		{
			int teamCol = 0;
			bool blue=(teamId==1 && proto[0]) || (teamId==2 && proto[1]);
			bool red=(teamId==1 && !proto[0] && proto[1]) || (teamId==2 && !proto[1] && proto[0]);
			if(blue)
				teamCol = 1;
			else if(red)
				teamCol = 2;
			if(teamCol != m_lastTeamCol)
			{
				m_animSwingOMeter.Invoke("showGlow", teamCol);
				m_lastTeamCol = teamCol;
			}
		}

		IEntityScriptProxy *pScriptProxy=static_cast<IEntityScriptProxy *>(pGameRules->GetEntity()->GetProxy(ENTITY_PROXY_SCRIPT));
		if (pScriptProxy)
		{
			if (!stricmp(pScriptProxy->GetState(), "InGame") && pGameRules->IsTimeLimited())
			{
				int time = (int)(pGameRules->GetRemainingGameTime());

				int mins=time/60;
				int secs=time-(mins*60);
				CryFixedStringT<32> timeFormatter;
				timeFormatter.Format("%02d:%02d", mins, secs);
				SFlashVarValue timearg(timeFormatter.c_str());
				m_animSwingOMeter.Invoke("setTimer", &timearg, 1);
			}
			else
				m_animSwingOMeter.Invoke("setTimer", "");
		}

		m_animSwingOMeter.GetFlashPlayer()->Advance(fDeltaTime);
		m_animSwingOMeter.GetFlashPlayer()->Render();

		static char text[32];
		if (m_capturing)
		{
			int icap=(int)(m_captureProgress*100.0f);
			//sprintf(text, "%d%%", icap);
			//DrawBar(16.0f, 80.0f, 72.0f, 14.0f, 2.0f, m_captureProgress, Col_DarkGray, Col_LightGray, text, Col_White, fabsf(cry_sinf(gEnv->pTimer->GetCurrTime()*2.5f)));
			if(icap != m_lastBuildingTime)
			{
				m_currentHexIconState = E_HEX_ICON_CAPTURING;
				g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
				g_pHexIcon->Invoke("setHexProgress", icap);
				m_lastBuildingTime = icap;
			}
		}
		else if(m_currentHexIconState == E_HEX_ICON_CAPTURING)
		{
			m_currentHexIconState = E_HEX_ICON_NONE;
			g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
		}

		bool constructing = false;
		if (m_constructing)
		{
			if (!m_constructionQueued)
			{
				constructing = true;
				int conTime = 0;
				if(m_constructionTime != 0.0f)
					conTime = int(100.0f*(1.0f-(m_constructionTimer/m_constructionTime)));
				if(conTime != m_lastConstructionTime)
				{
					if(m_currentHexIconState != E_HEX_ICON_CAPTURING) //capturing overwrites building by design
					{
						m_currentHexIconState = E_HEX_ICON_BUILDING;
						g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
						g_pHexIcon->Invoke("setHexProgress", conTime);
						m_lastConstructionTime = conTime;
					}
				}
				m_constructionTimer-=fDeltaTime;
				if (m_constructionTimer<0.0f)
					m_constructionTimer=0.0f;
			}
		}

		if(!constructing)
		{
			if(m_currentHexIconState == E_HEX_ICON_BUILDING)
			{
				m_currentHexIconState = E_HEX_ICON_BUY;
				g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
			}
			if(!m_bInBuyZone && !(m_bInServiceZone && g_pHUD->GetVehicleInterface()->IsAbleToBuy()) && m_currentHexIconState == E_HEX_ICON_BUY)
			{
				m_currentHexIconState = E_HEX_ICON_NONE;
				g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
			}
			else if(!m_capturing && m_currentHexIconState != E_HEX_ICON_BUY && (m_bInBuyZone || (m_bInServiceZone && g_pHUD->GetVehicleInterface()->IsAbleToBuy())))
			{
				m_currentHexIconState = E_HEX_ICON_BUY;
				g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
			}

		}
		//gEnv->pRenderer->Set2DMode(false, 0, 0);
	}
}

//-----------------------------------------------------------------------------------------------------

int CHUDPowerStruggle::GetPlayerPP()
{
	int pp = 0;
	CGameRules *pGameRules = g_pGame->GetGameRules();
	IScriptTable *pScriptTable = pGameRules->GetEntity()->GetScriptTable();
	if(pScriptTable)
	{
		int key = 0;
		pScriptTable->GetValue("PP_AMOUNT_KEY", key);
		pGameRules->GetSynchedEntityValue(g_pGame->GetIGameFramework()->GetClientActor()->GetEntityId(), TSynchedKey(key), pp);
	}
	return pp;
}

//-----------------------------------------------------------------------------------------------------

int CHUDPowerStruggle::GetPlayerTeamScore()
{
	int iTeamScore = 0;
	CGameRules *pGameRules = g_pGame->GetGameRules();
	IScriptTable *pScriptTable = pGameRules->GetEntity()->GetScriptTable();
	if(pScriptTable)
	{
		int key = 0;
		pScriptTable->GetValue("TEAMSCORE_TEAM0_KEY", key);
		EntityId uiPlayerID = g_pGame->GetIGameFramework()->GetClientActor()->GetEntityId();
		int iTeamID = pGameRules->GetTeam(uiPlayerID);
		pGameRules->GetSynchedGlobalValue(TSynchedKey(key+iTeamID), iTeamScore);
	}
	return iTeamScore;
}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::UpdateLastPurchase()
{
	if(m_thisPurchase.iPrice>0)
	{
		m_lastPurchase = m_thisPurchase;
		m_thisPurchase.iPrice = 0;
		m_thisPurchase.itemArray.resize(0);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::Buy(const char* item, bool reload)
{
	string buy("buy ");

	SItem itemdef;
	if (GetItemFromName(item, itemdef))
	{
		if(itemdef.bAmmoType)
			buy="buyammo ";
	}

	buy.append(item);
	gEnv->pConsole->ExecuteString(buy.c_str());

	HUD_CALL_LISTENERS_PS(OnBuyItem(item));

	if(reload)
	{
		InitEquipmentPacks();
		UpdatePackageList();
		UpdateBuyList();
	}
}

//-----------------------------------------------------------------------------------------------------
void CHUDPowerStruggle::UpdateEnergyBuyList(int energy_before, int energy_after)
{

	std::vector<SItem> itemList;
	EBuyMenuPage itemType = m_eCurBuyMenuPage;
	if(itemType!=E_PROTOTYPES) return;

	GetItemList(itemType, itemList);
	bool update = false;
	for(std::vector<SItem>::iterator iter=itemList.begin(); iter!=itemList.end(); ++iter)
	{
		SItem item = (*iter);
		if(energy_before<item.level && energy_after>=item.level)
		{
			update = true;
			break;
		}
	}
	if(update)
		UpdateBuyList();
}

//-----------------------------------------------------------------------------------------------------
void CHUDPowerStruggle::BuyPackage(SEquipmentPack equipmentPackage)
{
	bool equipAttachments = (g_pGameCVars->hud_attachBoughtEquipment==1)?true:false;

	CPlayer *pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetClientActor());
	if(pPlayer && pPlayer->GetInventory())
	{
		std::vector<SItem>::const_iterator it = equipmentPackage.itemArray.begin();
		for(; it != equipmentPackage.itemArray.end(); ++it)
		{
			SItem item = (*it);
			if(item.bAmmoType)
				Buy(item.strName, false);
			else
			{
				if(item.iInventoryID==0  || item.isUnique==0)
					Buy(item.strName, false);

				if(equipAttachments)
				{
					if(g_pHUD->GetCurrentWeapon())
					{
						IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(item.strClass.c_str());
						IItem *pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pPlayer->GetInventory()->GetItemByClass(pClass));
						if(pItem && pItem != g_pHUD->GetCurrentWeapon())
						{
							const bool bAddAccessory = g_pHUD->GetCurrentWeapon()->GetAccessory(item.strClass.c_str()) == 0;
							if(bAddAccessory)
							{
								g_pHUD->GetCurrentWeapon()->SwitchAccessory(item.strClass.c_str());
								HUD_CALL_LISTENERS_PS(WeaponAccessoryChanged(g_pHUD->GetCurrentWeapon(), item.strClass.c_str(), bAddAccessory));
							}
						}
					}
				}
			}
		}
	}
	InitEquipmentPacks();
	UpdatePackageList();
	UpdateCurrentPackage();
}

//-----------------------------------------------------------------------------------------------------
void CHUDPowerStruggle::BuyPackage(int index)
{
	if(index==-1)
	{
		BuyPackage(m_lastPurchase);
	}
	else if(index>=0 && m_EquipmentPacks.size()>index)
	{
		BuyPackage(m_EquipmentPacks[index]);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::DeletePackage(int index)
{
	if(index>=0 && m_EquipmentPacks.size()>index)
	{
		m_EquipmentPacks.erase(m_EquipmentPacks.begin() + index);
		if(m_EquipmentPacks.size()<=index)
		{
			--index;
		}
		OnSelectPackage(index);
	}
	SaveEquipmentPacks();
}

//-----------------------------------------------------------------------------------------------------

CHUDPowerStruggle::SEquipmentPack CHUDPowerStruggle::LoadEquipmentPack(int index)
{
	SEquipmentPack newPack;
	newPack.iPrice = 0;
	char strPathName[256];
	sprintf(strPathName,"Multiplayer.EquipmentPacks.%d.Name",index);
	char strPathNums[256];
	sprintf(strPathNums,"Multiplayer.EquipmentPacks.%d.NumItems",index);
	g_pGame->GetOptions()->GetProfileValue(strPathName, newPack.strName);
	int numEntries = 0;
	if(g_pGame->GetOptions()->GetProfileValue(strPathNums,numEntries))
	{
		for(int i(0); i<numEntries; ++i)
		{
			string entry;
			char strPath[256];
			sprintf(strPath,"Multiplayer.EquipmentPacks.%d.%d",index,i);
			if(g_pGame->GetOptions()->GetProfileValue(strPath,entry))
			{
				SItem item;
				if(GetItemFromName(entry, item))
				{
					newPack.itemArray.push_back(item);
					if(item.iInventoryID<=0)
						newPack.iPrice += item.iPrice;
				}
			}
		}
	}
	return newPack;
}

void CHUDPowerStruggle::RequestNewLoadoutName(string &name, const char* bluePrint)
{
	//define name
	int maxZahl(99);
	int target(0);
	for(int zahl(1); zahl<maxZahl; ++zahl)
	{
		bool found(false);
		for(int i(0); i<m_EquipmentPacks.size(); ++i)
		{
			SEquipmentPack *pack = &m_EquipmentPacks[i];
			string strPackName = pack->strName;
			string checkOne(strPackName);
			string checkTwo(bluePrint);
			checkOne.MakeLower();
			checkTwo.MakeLower();
			int index = checkOne.find(checkTwo);
			if(index==0)
			{
				strPackName = strPackName.substr(strlen(bluePrint),strlen(strPackName));
				int number = atoi(strPackName);
				if(number==zahl)
				{
					found = true;
					break;
				}
			}
		}
		if(found==false)
		{
			target = zahl;
			break;
		}
	}
	if(target>0)
	{
		char number[64];
		sprintf(number,"%02d",target);
		name = bluePrint;
		name.append(number);
	}
	else
	{
		name="LowtecWasHere";
	}
}

bool CHUDPowerStruggle::CheckDoubleLoadoutName(const char *name)
{
	for(int i(0); i<m_EquipmentPacks.size(); ++i)
	{
		SEquipmentPack pack = m_EquipmentPacks[i];
		if(!stricmp(pack.strName.c_str(), name)) return false;
	}
	return true;
}

void CHUDPowerStruggle::SavePackage(const char *name, int index)
{
	SEquipmentPack pack;
	if(!name || !name[0])
	{
		RequestNewLoadoutName(pack.strName, "");
	}
	else if(!CheckDoubleLoadoutName(name))
	{
		RequestNewLoadoutName(pack.strName, name);
	}
	else
	{
		pack.strName = name;
	}
	pack.iPrice = 0;

	std::vector<const char*> getArray;
	if(g_pBuyMenu)
	{
		int iSize = g_pBuyMenu->GetFlashPlayer()->GetVariableArraySize("m_backArray");
		getArray.resize(iSize);
		g_pBuyMenu->GetFlashPlayer()->GetVariableArray(FVAT_ConstStrPtr,"m_backArray", 0, &getArray[0], iSize);
		int size = getArray.size();
	}
	std::vector<const char*>::const_iterator it = getArray.begin();
	for(; it != getArray.end(); ++it)
	{
		SItem item;
		if(GetItemFromName(*it,item))
			pack.itemArray.push_back(item);
		pack.iPrice += item.iPrice;
	}

	if(index>=0 && m_EquipmentPacks.size()>index)
	{
		m_EquipmentPacks[index] = pack;
	}
	else
	{
		m_EquipmentPacks.push_back(pack);
	}
	SaveEquipmentPacks();
	UpdatePackageList();
}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::SaveEquipmentPacks()
{

	g_pGame->GetOptions()->SaveValueToProfile("Multiplayer.EquipmentPacks.NumPacks",(int)m_EquipmentPacks.size());

	{
		for(int i(0); i<m_EquipmentPacks.size(); ++i)
		{
			SEquipmentPack pack = m_EquipmentPacks[i];
			char strPathNum[256];
			sprintf(strPathNum,"Multiplayer.EquipmentPacks.%d.NumItems",i);
			char strPathName[256];
			sprintf(strPathName,"Multiplayer.EquipmentPacks.%d.Name",i);
			g_pGame->GetOptions()->SaveValueToProfile(strPathNum,(int)pack.itemArray.size());
			g_pGame->GetOptions()->SaveValueToProfile(strPathName,pack.strName);
			for(int j(0); j<pack.itemArray.size(); ++j)
			{
				char strPath[256];
				sprintf(strPath,"Multiplayer.EquipmentPacks.%d.%d",i,j);
				g_pGame->GetOptions()->SaveValueToProfile(strPath,pack.itemArray[j].strName);
			}
		}
	}
	g_pGame->GetOptions()->SaveProfile();
}


//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::InitEquipmentPacks()
{
	m_EquipmentPacks.resize(0);
	string strPathNums = "Multiplayer.EquipmentPacks.NumPacks";
	int numEntries = 0;
	if(g_pGame->GetOptions()->GetProfileValue(strPathNums,numEntries))
	{
		for(int i(0); i<numEntries; ++i)
		{
			SEquipmentPack pack = LoadEquipmentPack(i);
			m_EquipmentPacks.push_back(pack);
		}
	}
}

//-----------------------------------------------------------------------------------------------------
bool CHUDPowerStruggle::GetItemFromName(const char *name, SItem &item)
{
	IScriptTable *pGameRulesScriptTable = g_pGame->GetGameRules()->GetEntity()->GetScriptTable();
	SmartScriptTable pItemListScriptTable;

	float scale = g_pGameCVars->g_pp_scale_price;

	std::vector<string> tableList;
	tableList.push_back("weaponList");
	tableList.push_back("ammoList");
	tableList.push_back("vehicleList");
	tableList.push_back("equipList");
	tableList.push_back("protoList");

	std::vector<string>::const_iterator it = tableList.begin();
	for(; it != tableList.end(); ++it)
	{
		if(pGameRulesScriptTable->GetValue(*it,pItemListScriptTable))
		{
			IScriptTable::Iterator iter = pItemListScriptTable->BeginIteration();
			while(pItemListScriptTable->MoveNext(iter))
			{
				if(ANY_TTABLE != iter.value.type)
					continue;

				IScriptTable *pEntry = iter.value.table;

				bool invisible=false;
				if (pEntry->GetValue("invisible", invisible) && invisible)
					continue;

				const char *id=0;
				if (pEntry->GetValue("id", id) && !strcmp(id, name))
				{
					bool type=false;

					CreateItemFromTableEntry(pEntry, item);

					item.bAmmoType = pEntry->GetValue("ammo", type) && type;
					item.bVehicleType = pEntry->GetValue("vehicle", type) && type;
					pEntry->EndIteration(iter);
					return true;
				}
			}
			pItemListScriptTable->EndIteration(iter);
		}
	}
	return false;
}

//------------------------------------------------------------------------

void CHUDPowerStruggle::ShowCaptureProgress(bool show)
{
	if (m_capturing!=show)
	{
		m_captureProgress=-1.0f;
	}

	if(show)
	{
		if(m_currentHexIconState != E_HEX_ICON_CAPTURING)
		{
			m_currentHexIconState = E_HEX_ICON_CAPTURING;
			g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
		}
	}
	else if(m_currentHexIconState == E_HEX_ICON_CAPTURING)
	{
		if(m_bInBuyZone)
		{
			m_currentHexIconState = E_HEX_ICON_BUY;
			g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
		}
		else if(m_currentHexIconState != E_HEX_ICON_BUILDING)
		{
			m_currentHexIconState = E_HEX_ICON_NONE;
			g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
		}
	}

	m_capturing=show;
}

//------------------------------------------------------------------------
void CHUDPowerStruggle::SetCaptureProgress(float progress)
{
	if(m_currentHexIconState != E_HEX_ICON_CAPTURING)
	{
		m_currentHexIconState = E_HEX_ICON_CAPTURING;
		g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
	}

	m_captureProgress=progress;
}

void CHUDPowerStruggle::SetCaptureContested(bool contested)
{
	g_pHexIcon->Invoke("setCapturePointContested", contested);
}


//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::CreateItemFromTableEntry(IScriptTable *pItemScriptTable, SItem &item)
{
	char *strId = NULL;
	char *strDesc = NULL;
	char *strClass = NULL;
	int	iPrice = 0;
	float level = 0.0f;
	int unique = 0;

	pItemScriptTable->GetValue("id",strId);
	pItemScriptTable->GetValue("name",strDesc);
	pItemScriptTable->GetValue("class",strClass);
	pItemScriptTable->GetValue("price",iPrice);
	pItemScriptTable->GetValue("uniqueId",unique);
	pItemScriptTable->GetValue("level",level);
	
	float scale = g_pGameCVars->g_pp_scale_price;

	item.strName = strId;
	item.strDesc = strDesc;
	item.iPrice = (int)(iPrice*scale);
	item.level = level;
	item.strClass = strClass;
	item.isUnique = unique;

	EntityId inventoryItem = 0;
	if(strClass)
	{
		IActor *pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
		if(pActor)
		{
			IInventory *pInventory = pActor->GetInventory();
			if(pInventory)
			{
				IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(strClass);
				inventoryItem = pInventory->GetItemByClass(pClass);

				if(IItem *pItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(inventoryItem))
				{
					if(pClass == CItem::sSOCOMClass)
					{
						bool bSlave = pItem->IsDualWieldSlave();
						bool bMaster = pItem->IsDualWieldMaster();

						if(!bSlave && !bMaster)
							inventoryItem = 0;
					}

					if (!pItem->CanSelect())
						inventoryItem = 0;
				}
			}
		}
	}
	item.iInventoryID = (int)inventoryItem;
};


bool CHUDPowerStruggle::WeaponUseAmmo(CWeapon *pWeapon, IEntityClass* pAmmoType)
{
	int nfm=pWeapon->GetNumOfFireModes();

	for (int fm=0; fm<nfm; fm++)
	{
		IFireMode *pFM = pWeapon->GetFireMode(fm);
		if (pFM && pFM->IsEnabled() && (pFM->GetAmmoType() == pAmmoType) && (pFM->GetClipSize()!=-1))
			return true;
	}

	return false;
}

bool CHUDPowerStruggle::CanUseAmmo(IEntityClass* pAmmoType)
{
	if (!pAmmoType)
		return true;

	IActor *pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
	if(!pActor)
		return false;

	if (IVehicle *pVehicle=pActor->GetLinkedVehicle())
	{
		int n=pVehicle->GetWeaponCount();
		for (int i=0; i<n; i++)
		{
			if (EntityId weaponId=pVehicle->GetWeaponId(i))
			{
				if (IItem *pItem=gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(weaponId))
				{
					CWeapon *pWeapon=static_cast<CWeapon *>(pItem->GetIWeapon());
					if (!pWeapon)
						continue;

					if (WeaponUseAmmo(pWeapon, pAmmoType))
						return true;
				}
			}
		}
	}
	else
	{
		IInventory *pInventory = pActor->GetInventory();
		if (!pInventory)
			return false;


		int n=pInventory->GetCount();
		for (int i=0; i<n; i++)
		{
			if (EntityId itemId=pInventory->GetItem(i))
			{
				if (IItem *pItem=gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(itemId))
				{
					CWeapon *pWeapon=static_cast<CWeapon *>(pItem->GetIWeapon());
					if (!pWeapon)
						continue;

					if (WeaponUseAmmo(pWeapon, pAmmoType))
						return true;
				}
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::GetItemList(EBuyMenuPage itemType, std::vector<SItem> &itemList, bool bBuyMenu)
{
	const char *list = "weaponList";
	if(itemType==E_AMMO)
	{
		list = "ammoList";
	}
	else if(itemType==E_VEHICLES)
	{
		list = "vehicleList";
	}
	else if(itemType==E_EQUIPMENT)
	{
		list = "equipList";
	}
	else if(itemType==E_PROTOTYPES)
	{
		list = "protoList";
	}

	SmartScriptTable pItemListScriptTable;

	IScriptTable *pGameRulesScriptTable = g_pGame->GetGameRules()->GetEntity()->GetScriptTable();

	IActor *pActor = gEnv->pGame->GetIGameFramework()->GetClientActor();
	if(!pActor)
		return;
	IInventory *pInventory = pActor->GetInventory();
	if (!pInventory)
		return;

	float scale = g_pGameCVars->g_pp_scale_price;

	if(pGameRulesScriptTable && pGameRulesScriptTable->GetValue(list,pItemListScriptTable))
	{
		IScriptTable::Iterator iter = pItemListScriptTable->BeginIteration();
		while(pItemListScriptTable->MoveNext(iter))
		{
			if(ANY_TTABLE != iter.value.type)
				continue;

			IScriptTable *pItemScriptTable = iter.value.table;

			char *strId = NULL;
			char *strName = NULL;
			char *strClass = NULL;
			char *strCategory = NULL;
			int iPrice = 0;
			float level = 0.0f;
			int iUnique = 0;
			bool bInvisible = false;
			bool bVehicle = false;
			bool bAmmoType=false;
			bool loadout = false;
			bool special = false;
			bool vehicleAmmo = false;
			char *ammoClass = NULL;

			int uniqueGroup = 0;
			int uniqueCount = 0;

			pItemScriptTable->GetValue("invisible",bInvisible);
			if (bInvisible)
				continue;

			pItemScriptTable->GetValue("id",strId);
			pItemScriptTable->GetValue("level", level);

			if (pItemScriptTable->GetValue("ammo", bAmmoType) && bAmmoType)
			{
				IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(strId);
				if (bBuyMenu && (itemType==E_AMMO) && !CanUseAmmo(pClass))
					continue;
			}


			pItemScriptTable->GetValue("name",strName);
			pItemScriptTable->GetValue("class",strClass);
			pItemScriptTable->GetValue("uniqueId",iUnique);
			pItemScriptTable->GetValue("uniqueloadoutgroup",uniqueGroup);
			pItemScriptTable->GetValue("uniqueloadoutcount",uniqueCount);
			pItemScriptTable->GetValue("price",iPrice);
			pItemScriptTable->GetValue("vehicle",bVehicle);
			pItemScriptTable->GetValue("category",strCategory);
			pItemScriptTable->GetValue("loadout",loadout);
			pItemScriptTable->GetValue("special",special);
			pItemScriptTable->GetValue("buyammo",ammoClass);
			pItemScriptTable->GetValue("vehicleammo",vehicleAmmo);

			if (special)
				loadout=false;

			SItem item;
			item.strName = strId;
			item.strDesc = strName;
			item.strClass = strClass;
			item.iPrice = (int)(iPrice*scale);
			item.level = level;
			item.isUnique = iUnique;
			item.iCount = 0;
			item.iMaxCount = 1;
			item.uniqueLoadoutGroup = uniqueGroup;
			item.uniqueLoadoutCount = uniqueCount;
			item.bVehicleType = bVehicle;
			item.strCategory = strCategory;
			item.bAmmoType = bAmmoType;
			item.loadout = loadout;
			item.special = special;
			item.isWeapon = false;

			EntityId inventoryItem = 0;
			bool pistols = false;
			if(strClass)
			{
				IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(strClass);
				inventoryItem = pInventory->GetItemByClass(pClass);
				if(ammoClass)
				{
					IEntityClass* pAmmoClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammoClass);
					item.iMaxCount = pInventory->GetAmmoCapacity(pAmmoClass);
					item.iCount = pInventory->GetAmmoCount(pAmmoClass);
				}
				IItem *pItem = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(inventoryItem);
				if(pItem)
				{
					item.isWeapon = pItem->CanDrop();

					if(pClass == CItem::sSOCOMClass)
					{
						pistols = true;
						bool bSlave = pItem->IsDualWieldSlave();
						bool bMaster = pItem->IsDualWieldMaster();
						if(!bSlave && !bMaster)
							//change to double pistols
							inventoryItem = -1;
					}
					if (!pItem->CanSelect())
						inventoryItem = -2;
				}
				else
				{
					inventoryItem = 0;
				}
			}
			else
			{
				IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(strId);
				if(pClass)
				{
					if(vehicleAmmo)
					{
						IVehicle* pVehicle = pActor->GetLinkedVehicle();
						if(pVehicle)
						{
							item.iMaxCount = pVehicle->GetAmmoCapacity(pClass);
							item.iCount = pVehicle->GetAmmoCount(pClass);
						}
					}
					else
					{
						item.iMaxCount = pInventory->GetAmmoCapacity(pClass);
						item.iCount = pInventory->GetAmmoCount(pClass);
					}
				}
			}

			if (iUnique!=0 || (int)inventoryItem < 0 || pistols)
				item.iInventoryID = (int)inventoryItem;
			else
				item.iInventoryID = 0;
			itemList.push_back(item);
		}
		pItemListScriptTable->EndIteration(iter);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::DetermineCurrentBuyZone(bool sendToFlash)
{
	IActor *pActor=g_pGame->GetIGameFramework()->GetClientActor();
	if(!pActor) return;
	
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if(!pGameRules) return;

	EntityId uiPlayerID = pActor->GetEntityId();

	int playerTeam = pGameRules->GetTeam(uiPlayerID);
	bool isDead = pGameRules->IsDead(uiPlayerID);

	EBuyMenuPage factory = E_LOADOUT;

	if (isDead)
	{
		factory = E_WEAPONS;
	}
	else
	{
		std::vector<EntityId>::const_iterator it = m_currentBuyZones.begin();
		for(; it != m_currentBuyZones.end(); ++it)
		{
			if(pGameRules->GetTeam(*it) == playerTeam)
			{
				if(IsFactoryType(*it,E_PROTOTYPES))
				{
					factory = E_PROTOTYPES;
					break;
				}
				else if(IsFactoryType(*it,E_VEHICLES))
				{
					factory = E_VEHICLES;
					break;
				}
				else if(IsFactoryType(*it,E_WEAPONS))
				{
					factory = E_WEAPONS;
					break;
				}
				else if(IsFactoryType(*it,E_AMMO))
				{
					factory = E_AMMO;
					break;
				}
			}
		}
		if(factory == E_LOADOUT && g_pHUD->GetVehicleInterface()->IsAbleToBuy())
		{
			std::vector<EntityId>::const_iterator it = m_currentServiceZones.begin();
			for(; it != m_currentServiceZones.end(); ++it)
			{
				if(pGameRules->GetTeam(*it) == playerTeam)
				{
					if(m_eCurBuyMenuPage!=E_LOADOUT)
						factory = E_AMMO;
					break;
				}
			}
		}
	}

	m_eCurBuyMenuPage = factory;
	if(sendToFlash)
	{
		int page = 1;
		if(m_eCurBuyMenuPage==E_AMMO)
			page = 2;
		else if(m_eCurBuyMenuPage==E_EQUIPMENT)
			page = 3;
		else if(m_eCurBuyMenuPage==E_VEHICLES)
			page = 4;
		else if(m_eCurBuyMenuPage==E_PROTOTYPES)
			page = 5;
		else if(m_eCurBuyMenuPage==E_LOADOUT)
			page = 6;
		g_pBuyMenu->Invoke("Root.TabBar.gotoTab", (int)page);
		PopulateBuyList();
	}

}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::UpdateBuyList(const char *page)
{
	if(page && page[0])
	{
		g_pHUD->m_buyMenuKeyLog.m_state = CHUD::SBuyMenuKeyLog::eBMKL_Frame;
	}
	if(gEnv->bMultiplayer)
	{
		if(page)
		{
			m_eCurBuyMenuPage = ConvertToBuyList(page);
		}
		PopulateBuyList();
	}
}

//-----------------------------------------------------------------------------------------------------

CHUDPowerStruggle::EBuyMenuPage CHUDPowerStruggle::ConvertToBuyList(const char *page)
{
	if(page)
	{
		static EBuyMenuPage pages[] = { E_WEAPONS, E_WEAPONS, E_AMMO, E_EQUIPMENT, E_VEHICLES, E_PROTOTYPES, E_LOADOUT };
		static const int numPages = sizeof(pages) / sizeof(pages[0]);
		int pageIndex = atoi(page); // returns 0, if no conversion made
		pageIndex = pageIndex < 0 ? 0 : (pageIndex < numPages ? pageIndex : 0);
		return pages[pageIndex];
	}
	return E_WEAPONS;
}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::PopulateBuyList()
{
	if (!g_pGame->GetIGameFramework()->GetClientActor())
		return;

	if(!g_pBuyMenu)
		return;

	if(!g_pHUD->IsBuyMenuActive())
		return;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if(!pGameRules)
		return;

	EBuyMenuPage itemType = m_eCurBuyMenuPage;

	IFlashPlayer *pFlashPlayer = g_pBuyMenu->GetFlashPlayer();

	std::vector<SItem> itemList;

	pFlashPlayer->Invoke0("clearAllEntities");

	GetItemList(itemType, itemList);

	IEntity *pFirstVehicleFactory = NULL;

	EntityId uiPlayerID = g_pGame->GetIGameFramework()->GetClientActor()->GetEntityId();
	int playerTeam = pGameRules->GetTeam(uiPlayerID);
	bool bInvalidBuyZone = true;
	float CurBuyZoneLevel = 0.0f;
	{
		if(g_pHUD->GetVehicleInterface()->IsAbleToBuy())
		{
			std::vector<EntityId>::const_iterator it = m_currentServiceZones.begin();
			for(; it != m_currentServiceZones.end(); ++it)
			{
				if(pGameRules->GetTeam(*it) == playerTeam)
				{
					IEntity *pEntity = gEnv->pEntitySystem->GetEntity(*it);
					if(IsFactoryType(*it, itemType))
					{
						float level = g_pHUD->CallScriptFunction(pEntity,"GetPowerLevel");
						if(level>CurBuyZoneLevel)
							CurBuyZoneLevel = level;
						bInvalidBuyZone = false;
						if(itemType == E_VEHICLES || itemType == E_PROTOTYPES)
						{
							pFirstVehicleFactory = pEntity;
						}
					}
				}
			}
		}
		std::vector<EntityId>::const_iterator it = m_currentBuyZones.begin();
		for(; it != m_currentBuyZones.end(); ++it)
		{
			if(pGameRules->GetTeam(*it) == playerTeam)
			{
				IEntity *pEntity = gEnv->pEntitySystem->GetEntity(*it);
				if(IsFactoryType(*it, itemType))
				{
					float level = g_pHUD->CallScriptFunction(pEntity,"GetPowerLevel");
					if(level>CurBuyZoneLevel)
						CurBuyZoneLevel = level;
					bInvalidBuyZone = false;
					if(itemType == E_VEHICLES || itemType == E_PROTOTYPES)
					{
						pFirstVehicleFactory = pEntity;
					}
				}
			}
		}
	}

	bool isDead = pGameRules->IsDead(uiPlayerID);

	//std::sort(itemList.begin(),itemList.end(),SortByPrice);
	std::vector<string> itemArray;

	char tempBuf[256];

	for(std::vector<SItem>::iterator iter=itemList.begin(); iter!=itemList.end(); ++iter)
	{
		SItem item = (*iter);
		const char* sReason = "ready";

		if(bInvalidBuyZone && !isDead)
		{
			sReason = "buyzone";
		}
		else
		{
			if(CurBuyZoneLevel<item.level)
				sReason = "level";

			if(itemType == E_VEHICLES || (itemType == E_PROTOTYPES && item.bVehicleType==true))
			{
				if(!CanBuild(pFirstVehicleFactory,item.strName.c_str()))
				{
					continue;
				}
			}
		}
		if(!item.isWeapon)
		{
			if(item.iInventoryID>0 && item.isUnique!=0)
			{
				sReason = "inventory";
			}
			if(item.iInventoryID==-2 && item.isUnique!=0)
			{
				sReason = "inventory";
			}
			if(item.iCount >= item.iMaxCount)
			{
				sReason = "inventory";
			}
			item.iInventoryID = 0;
		}

		if (item.special && !IsPlayerSpecial())
			sReason="level";

		itemArray.push_back(item.strName.c_str());
		itemArray.push_back(item.strDesc.c_str());
		// Autobuy, this one is special!
		if(item.strName == "")
		{
			// Retrieve its current price, which depends on the weapon the player has
			IScriptTable *pGameRulesScriptTable = g_pGame->GetGameRules()->GetEntity()->GetScriptTable();
			HSCRIPTFUNCTION hGetAutoBuyPrice = NULL;
			if(pGameRulesScriptTable && pGameRulesScriptTable->GetValue("GetAutoBuyPrice",hGetAutoBuyPrice))
			{
				Script::CallReturn(gEnv->pScriptSystem,hGetAutoBuyPrice,pGameRulesScriptTable,item.iPrice);
				gEnv->pScriptSystem->ReleaseFunc(hGetAutoBuyPrice);
			}
		}
		_snprintf(tempBuf,sizeof(tempBuf),"%d",item.iPrice);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemArray.push_back(tempBuf);
		itemArray.push_back(sReason);
		_snprintf(tempBuf,sizeof(tempBuf),"%d",item.iInventoryID);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemArray.push_back(tempBuf);
		if(!item.strCategory.empty())
		{
			itemArray.push_back(item.strCategory);
		}
		else
		{
			itemArray.push_back("");
		}
		_snprintf(tempBuf,sizeof(tempBuf),"%d",item.iCount);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemArray.push_back(tempBuf);
		_snprintf(tempBuf,sizeof(tempBuf),"%d",item.iMaxCount);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemArray.push_back(tempBuf);
	}
	
	ActivateBuyMenuTabs();

	char buffer[10];
	itoa(m_lastPurchase.iPrice, buffer, 10);

	wstring localized;
	localized = g_pHUD->LocalizeWithParams("@ui_buy_REPEATLASTBUY", true, buffer);
	g_pBuyMenu->Invoke("setLastPurchase", SFlashVarValue(localized));

	int size = itemArray.size();
	if(size)
	{
		std::vector<const char*> pushArray;
		pushArray.reserve(size);
		for (int i(0); i<size; ++i)
		{
			pushArray.push_back(itemArray[i].c_str());
		}

		pFlashPlayer->SetVariableArray(FVAT_ConstStrPtr, "m_allValues", 0, &pushArray[0], size);
	}

	pFlashPlayer->Invoke0("updateList");
}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::ActivateBuyMenuTabs()
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if(!pGameRules) return;

	IActor *pLocalActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pLocalActor) return;

	EntityId id=pLocalActor->GetEntityId();
	
	bool isDead = pGameRules->IsDead(id);
	bool inVehicle = g_pHUD->GetVehicleInterface()->IsAbleToBuy();

	bool b0, b1, b2, b3, b4;
	b0 = (m_serviceZoneTypes[0]&&inVehicle)||m_factoryTypes[0]||isDead;
	b1 = (m_serviceZoneTypes[1]&&inVehicle)||m_factoryTypes[1]||isDead;
	b2 = (m_serviceZoneTypes[2]&&inVehicle)||m_factoryTypes[2]||isDead;
	b3 = ((m_serviceZoneTypes[3]&&inVehicle)||m_factoryTypes[3]) && !isDead;
	b4 = ((m_serviceZoneTypes[4]&&inVehicle)||m_factoryTypes[4]) && !isDead;

	SFlashVarValue arg[5] = {(int)b0, (int)b1, (int)b2, (int)b3, (int)b4};
	g_pBuyMenu->Invoke("activateTabs",arg,5);
}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::UpdatePackageList()
{
	if(!g_pBuyMenu)
		return;

	g_pHUD->m_buyMenuKeyLog.m_state = CHUD::SBuyMenuKeyLog::eBMKL_NoInput;

	std::vector<string>itemList;
	char tempBuf[256];
	std::vector<SEquipmentPack>::const_iterator it = m_EquipmentPacks.begin();
	for(; it != m_EquipmentPacks.end(); ++it)
	{
		SEquipmentPack pack = (*it);
		itemList.push_back(pack.strName);
		_snprintf(tempBuf,sizeof(tempBuf),"%d",pack.iPrice);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemList.push_back(tempBuf);
	}

	ActivateBuyMenuTabs();

	int size = itemList.size();

	if(size)
	{
		std::vector<const char*> pushArray;
		pushArray.reserve(size);
		for (int i(0); i<size; ++i)
		{
			pushArray.push_back(itemList[i].c_str());
		}

		g_pBuyMenu->GetFlashPlayer()->SetVariableArray(FVAT_ConstStrPtr, "m_allValues", 0, &pushArray[0], pushArray.size());
	}

	g_pBuyMenu->Invoke("updatePackageList");
}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::UpdateCurrentPackage()
{
	if(!g_pBuyMenu)
		return;

	g_pBuyMenu->Invoke("updateCurrentPackage");
}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::OnSelectPackage(int index)
{
	if(index<0)
	{
		g_pBuyMenu->Invoke("updatePackageContent");
		return;
	}
	SEquipmentPack pack = m_EquipmentPacks[index];
	std::vector<string>itemList;
	char tempBuf[256];
	std::vector<SItem>::const_iterator it = pack.itemArray.begin();
	for(; it != pack.itemArray.end(); ++it)
	{
		const SItem *item = &*it;
		itemList.push_back(item->strName);
		itemList.push_back(item->strDesc);
		_snprintf(tempBuf,sizeof(tempBuf),"%d",item->iPrice);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemList.push_back(tempBuf);
		_snprintf(tempBuf,sizeof(tempBuf),"%d",item->iInventoryID);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemList.push_back(tempBuf);
		_snprintf(tempBuf,sizeof(tempBuf),"%d",item->isUnique);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemList.push_back(tempBuf);
	}
	int size = itemList.size();

	if(size)
	{
		std::vector<const char*> pushArray;
		pushArray.reserve(size);
		for (int i(0); i<size; ++i)
		{
			pushArray.push_back(itemList[i].c_str());
		}

		g_pBuyMenu->GetFlashPlayer()->SetVariableArray(FVAT_ConstStrPtr, "m_allValues", 0, &pushArray[0], size);
	}
	g_pBuyMenu->Invoke("updatePackageContent");
}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::UpdateModifyPackage(int index)
{
	SEquipmentPack pack = m_EquipmentPacks[index];
	std::vector<string>itemList;
	std::vector<SItem>::const_iterator it = pack.itemArray.begin();
	for(; it != pack.itemArray.end(); ++it)
	{
		const SItem *item = &*it;
		itemList.push_back(item->strName);
		itemList.push_back(item->strDesc);
		char strArg2[16];
		sprintf(strArg2,"%d",item->iPrice);
		itemList.push_back(strArg2);
	}
	int size = itemList.size();

	if(size)
	{
		std::vector<const char*> pushArray;
		pushArray.reserve(size);
		for (int i(0); i<size; ++i)
		{
			pushArray.push_back(itemList[i].c_str());
		}

		g_pBuyMenu->GetFlashPlayer()->SetVariableArray(FVAT_ConstStrPtr, "m_allValues", 0, &pushArray[0], size);
	}
	g_pBuyMenu->Invoke("updatePackageContentList");
}

//-----------------------------------------------------------------------------------------------------


void CHUDPowerStruggle::UpdatePackageItemList(const char *page)
{
	if (!g_pGame->GetIGameFramework()->GetClientActor())
		return;

	if(!g_pBuyMenu)
		return;

	IFlashPlayer *pFlashPlayer = g_pBuyMenu->GetFlashPlayer();

	std::vector<SItem> itemList;
	EBuyMenuPage itemType = ConvertToBuyList(page);

	GetItemList(itemType, itemList, false);

	EntityId uiPlayerID = g_pGame->GetIGameFramework()->GetClientActor()->GetEntityId();
	int playerTeam = g_pGame->GetGameRules()->GetTeam(uiPlayerID);
	CGameRules *pGameRules = g_pGame->GetGameRules();

	std::vector<string> itemArray;
	char tempBuf[256];

	//std::sort(itemList.begin(),itemList.end(),SortByPrice);

	for(std::vector<SItem>::iterator iter=itemList.begin(); iter!=itemList.end(); ++iter)
	{
		SItem item = (*iter);
		if(item.iPrice == 0) continue;
		if(item.loadout == false) continue;

		itemArray.push_back(item.strName.c_str());
		itemArray.push_back(item.strDesc.c_str());
		_snprintf(tempBuf,sizeof(tempBuf),"%d",item.iPrice);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemArray.push_back(tempBuf);
		_snprintf(tempBuf,sizeof(tempBuf),"%d",(int)item.iInventoryID);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemArray.push_back(tempBuf);
		_snprintf(tempBuf,sizeof(tempBuf),"%d",item.isUnique);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemArray.push_back(tempBuf);
		_snprintf(tempBuf,sizeof(tempBuf),"%d",(int)item.iMaxCount);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemArray.push_back(tempBuf);
		_snprintf(tempBuf,sizeof(tempBuf),"%d",(int)item.uniqueLoadoutGroup);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemArray.push_back(tempBuf);
		_snprintf(tempBuf,sizeof(tempBuf),"%d",(int)item.uniqueLoadoutCount);
		tempBuf[sizeof(tempBuf)-1]='\0';
		itemArray.push_back(tempBuf);
	}

	int size = itemArray.size();

	if(size)
	{
		std::vector<const char*> pushArray;
		pushArray.reserve(size);
		for (int i(0); i<size; ++i)
		{
			pushArray.push_back(itemArray[i].c_str());
		}

		pFlashPlayer->SetVariableArray(FVAT_ConstStrPtr, "m_allValues", 0, &pushArray[0], size);
	}

	pFlashPlayer->Invoke0("updatePackageItemList");
}

//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::UpdateBuyZone(bool trespassing, EntityId zone)
{
	bool wasInBuyZone=m_bInBuyZone||m_bInServiceZone;

	if(zone)
	{
		if(trespassing)
		{
			if(!stl::find(m_currentBuyZones, zone))
				m_currentBuyZones.push_back(zone);
		}
		else
		{
			std::vector<EntityId>::iterator it = m_currentBuyZones.begin();
			for(; it != m_currentBuyZones.end(); ++it)
			{
				if(*it == zone)
				{
					m_currentBuyZones.erase(it);
					break;
				}
			}
		}
	}

	m_factoryTypes[0] = false;
	m_factoryTypes[1] = false;
	m_factoryTypes[2] = false;
	m_factoryTypes[3] = false;
	m_factoryTypes[4] = false;

	if (IActor *pActor=g_pGame->GetIGameFramework()->GetClientActor())
	{
		//check whether the player is in a buy zone, he can use ...
		EntityId uiPlayerID = pActor->GetEntityId();
		int playerTeam = g_pGame->GetGameRules()->GetTeam(uiPlayerID);
		int inBuyZone = false;
		CGameRules *pGameRules = g_pGame->GetGameRules();
		{
			std::vector<EntityId>::const_iterator it = m_currentBuyZones.begin();
			for(; it != m_currentBuyZones.end(); ++it)
			{
				if(pGameRules->GetTeam(*it) == playerTeam)
				{
					if(IsFactoryType(*it,E_WEAPONS))
						m_factoryTypes[0] = true;
					if(IsFactoryType(*it,E_AMMO))
						m_factoryTypes[1] = true;
					if(IsFactoryType(*it,E_EQUIPMENT))
						m_factoryTypes[2] = true;
					if(IsFactoryType(*it,E_VEHICLES))
						m_factoryTypes[3] = true;
					if(IsFactoryType(*it,E_PROTOTYPES))
						m_factoryTypes[4] = true;
					inBuyZone = true;
				}
			}
		}

		if(inBuyZone)
		{
			if(g_pHUD->GetVehicleInterface()->IsAbleToBuy())
				inBuyZone = 2;	//this is a service zone
		}

		if(g_pHUD->IsBuyMenuActive() && m_bInBuyZone!=((inBuyZone)?true:false))
		{
			ActivateBuyMenuTabs();
			DetermineCurrentBuyZone(true);
		}

		//update buy zone
		m_bInBuyZone = (inBuyZone)?true:false;

		// if we leave buy zone, close the buy menu
		if(!m_bInBuyZone && (!m_bInServiceZone || !g_pHUD->GetVehicleInterface()->IsAbleToBuy()) && wasInBuyZone)
			g_pHUD->ShowPDA(false, true);

		if(g_pHexIcon)
		{
			if(!m_capturing && (m_bInBuyZone || (m_bInServiceZone && g_pHUD->GetVehicleInterface()->IsAbleToBuy())))
			{
				if(m_currentHexIconState != E_HEX_ICON_BUY && m_currentHexIconState != E_HEX_ICON_BUILDING)
				{
					m_currentHexIconState = E_HEX_ICON_BUY;
					g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
				}
			}
			else if(m_currentHexIconState != E_HEX_ICON_BUILDING && m_currentHexIconState != E_HEX_ICON_CAPTURING && m_currentHexIconState != E_HEX_ICON_NONE)
			{
				m_currentHexIconState = E_HEX_ICON_NONE;

				g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
			}
		}
	}
}
//-----------------------------------------------------------------------------------------------------

void CHUDPowerStruggle::UpdateServiceZone(bool trespassing, EntityId zone)
{
	bool wasInBuyZone=m_bInBuyZone || (m_bInServiceZone && g_pHUD->GetVehicleInterface()->IsAbleToBuy());

	if(zone)
	{
		if(trespassing)
		{
			if(!stl::find(m_currentServiceZones, zone))
				m_currentServiceZones.push_back(zone);
		}
		else
		{
			std::vector<EntityId>::iterator it = m_currentServiceZones.begin();
			for(; it != m_currentServiceZones.end(); ++it)
			{
				if(*it == zone)
				{
					m_currentServiceZones.erase(it);
					break;
				}
			}
		}
	}

	m_serviceZoneTypes[0] = false;
	m_serviceZoneTypes[1] = false;
	m_serviceZoneTypes[2] = false;
	m_serviceZoneTypes[3] = false;
	m_serviceZoneTypes[4] = false;

	if (IActor *pActor=g_pGame->GetIGameFramework()->GetClientActor())
	{
		//check whether the player is in a buy zone, he can use ...
		EntityId uiPlayerID = pActor->GetEntityId();
		int playerTeam = g_pGame->GetGameRules()->GetTeam(uiPlayerID);
		int inBuyZone = false;
		CGameRules *pGameRules = g_pGame->GetGameRules();
		{
			std::vector<EntityId>::const_iterator it = m_currentServiceZones.begin();
			for(; it != m_currentServiceZones.end(); ++it)
			{
				if(pGameRules->GetTeam(*it) == playerTeam)
				{
					m_serviceZoneTypes[1] = true;
/*					if(IsFactoryType(*it,E_WEAPONS))
						m_serviceZoneTypes[0] = true;
					if(IsFactoryType(*it,E_AMMO))
						m_serviceZoneTypes[1] = true;
					if(IsFactoryType(*it,E_EQUIPMENT))
						m_serviceZoneTypes[2] = true;
					if(IsFactoryType(*it,E_VEHICLES))
						m_serviceZoneTypes[3] = true;
					if(IsFactoryType(*it,E_PROTOTYPES))
						m_serviceZoneTypes[4] = true;
*/					inBuyZone = true;
				}
			}
		}

		if(inBuyZone)
		{
			if(g_pHUD->GetVehicleInterface()->IsAbleToBuy())
				inBuyZone = 2;	//this is a service zone
		}

		if(g_pHUD->IsBuyMenuActive())
		{
			ActivateBuyMenuTabs();
		}

		//update buy zone
		//m_bInBuyZone = (inBuyZone)?true:false;
		m_bInServiceZone = (inBuyZone)?true:false;

		// if we leave buy zone, close the buy menu
		if(!m_bInBuyZone && !m_bInServiceZone && wasInBuyZone)
		{
			g_pHUD->ShowPDA(false, true);
		}

		if(g_pHexIcon)
		{
			if(!m_capturing && (m_bInBuyZone || (m_bInServiceZone && g_pHUD->GetVehicleInterface()->IsAbleToBuy())))
			{
				if(m_currentHexIconState != E_HEX_ICON_BUY && m_currentHexIconState != E_HEX_ICON_BUILDING)
				{
					m_currentHexIconState = E_HEX_ICON_BUY;
					g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
				}
			}
			else if(m_currentHexIconState != E_HEX_ICON_BUILDING && m_currentHexIconState != E_HEX_ICON_CAPTURING && m_currentHexIconState != E_HEX_ICON_NONE)
			{
				m_currentHexIconState = E_HEX_ICON_NONE;
				g_pHexIcon->Invoke("setHexIcon", m_currentHexIconState);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

bool CHUDPowerStruggle::IsFactoryType(EntityId entity, EBuyMenuPage type)
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entity);
	if(!pEntity) return false;
	int iBuyZoneFlags = g_pHUD->CallScriptFunction(pEntity,"GetBuyFlags");
	if(iBuyZoneFlags&((int)type))
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------------------------------

bool CHUDPowerStruggle::CanBuild(IEntity *pEntity, const char *vehicle)
{
	if(!pEntity) return false;
	IScriptTable *pScriptTable = pEntity->GetScriptTable();
	HSCRIPTFUNCTION scriptFuncHelper = NULL;

	bool buyable = false;
	//sum up available vehicles if tech level is high enough
	if(pScriptTable && pScriptTable->GetValue("CanBuild", scriptFuncHelper) && scriptFuncHelper)
	{
		Script::CallReturn(gEnv->pScriptSystem, scriptFuncHelper, pScriptTable, vehicle, buyable);
		gEnv->pScriptSystem->ReleaseFunc(scriptFuncHelper);
		scriptFuncHelper = NULL;
	}
	return buyable;
}

//------------------------------------------------------------------------
bool CHUDPowerStruggle::IsPlayerSpecial()
{
	if (INetChannel *pNetChannel=g_pGame->GetIGameFramework()->GetClientChannel())
		return pNetChannel->IsPreordered();
	return false;
}

//-----------------------------------------------------------------------------------------------------
void CHUDPowerStruggle::HideSOM(bool hide)
{
	if(hide)
	{
		if(m_animSwingOMeter.IsLoaded() && m_animSwingOMeter.GetFlashPlayer()->GetVisible())
			m_animSwingOMeter.GetFlashPlayer()->SetVisible(false);
	}
	else 
	{
		if(m_animSwingOMeter.IsLoaded())
			if(!m_animSwingOMeter.GetFlashPlayer()->GetVisible())
				m_animSwingOMeter.GetFlashPlayer()->SetVisible(true);
	}
}

void CHUDPowerStruggle::GetTeamStatus(int teamId, float &power, float &hq, int &controlledAliens, EntityId &prototypeFactoryId)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();

	power=0.0f;
	if (pGameRules->GetSynchedGlobalValueType(300+teamId)==eSVT_Int)
	{
		int p;
		pGameRules->GetSynchedGlobalValue(300+teamId, p);
		power=(float)p;
	}
	else
		pGameRules->GetSynchedGlobalValue(300+teamId, power);

	int owned=0;
	for (std::vector<EntityId>::iterator it=m_powerpoints.begin(); it!=m_powerpoints.end(); it++)
	{
		if (pGameRules->GetTeam(*it)==teamId)
			owned++;
	}

	controlledAliens=owned;

	float maxHP=1.0f;
	float currentHP=0.0f;
	for (int h=0;h<m_hqs.size();h++)
	{
		if (pGameRules->GetTeam(m_hqs[h])!=teamId)
			continue;

		if (IEntity *pEntity=gEnv->pEntitySystem->GetEntity(m_hqs[h]))
		{
			if (IScriptTable *pScriptTable=pEntity->GetScriptTable())
			{
				SmartScriptTable props;
				if (pScriptTable->GetValue("Properties", props))
					props->GetValue("nHitPoints", maxHP);

				HSCRIPTFUNCTION pfnGetHealth=0;
				if (pScriptTable->GetValue("GetHealth", pfnGetHealth) && pfnGetHealth)
				{
					Script::CallReturn(gEnv->pScriptSystem, pfnGetHealth, pScriptTable, currentHP);
					gEnv->pScriptSystem->ReleaseFunc(pfnGetHealth);
				}
			}
		}
	}

	hq=max(0.0f, currentHP/maxHP);

	// draw proto factory ownership indicator
	prototypeFactoryId=0;
	if (m_protofactory && pGameRules->GetTeam(m_protofactory)==teamId)
		prototypeFactoryId=m_protofactory;
}

void CHUDPowerStruggle::Scroll(int direction)
{
	if(!g_pBuyMenu) return;

	g_pBuyMenu->Invoke("scrollList",direction);
}

void CHUDPowerStruggle::ShowConstructionProgress(bool show, bool queued, float time)
{
	m_constructing=show;
	m_constructionQueued=queued;
	m_constructionTime=time;
	m_constructionTimer=time;
}

