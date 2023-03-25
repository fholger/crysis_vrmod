/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 22:8:2005   12:50 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include <IEntitySystem.h>
#include <IScriptSystem.h>
#include <IActionMapManager.h>
#include <IGameObject.h>
#include <IGameObjectSystem.h>
#include <IVehicleSystem.h>
#include "WeaponSystem.h"
#include "Weapon.h"
#include "ISerialize.h"
#include "ScriptBind_Weapon.h"
#include "Player.h"
#include "BulletTime.h"
#include "HUD/HUD.h"
#include "HUD/HUDRadar.h"
#include "HUD/HUDCrosshair.h"
#include "GameRules.h"
#include "ItemParamReader.h"
#include "Projectile.h"
#include "OffHand.h"
#include "Lam.h"
#include "GameActions.h"
#include "IronSight.h"
#include "Single.h"
#include "Environment/BattleDust.h"

#include "CryCharAnimationParams.h"

#include "IPlayerInput.h"
#include <IWorldQuery.h>

//------------------------------------------------------------------------
CWeapon::CWeapon()
: m_fm(0),
	m_melee(0),
	m_zm(0),
	m_zmId(0),
	m_fmDefaults(0),
	m_zmDefaults(0),
	m_xmlparams(NULL),
	m_pFiringLocator(0),
	m_fire_alternation(false),
  m_destination(0,0,0),
	m_forcedHitMaterial(-1),
	m_dofSpeed(0.0f),
	m_dofValue(0.0f),
	m_focusValue(0.0f),
	m_currentViewMode(0),
	m_useViewMode(false),
	m_restartZoom(false),
	m_restartZoomStep(0),
	m_targetOn(false),
	m_silencerAttached(false),
	m_weaponRaised(false),
	m_weaponLowered(false),
	m_switchingFireMode(false),
	m_switchLeverLayers(false),
	m_firedRockets(0),
	m_AIUseEyeOffset(false),
	m_raiseProbability(0.0f),
	m_requestedFire(false),
	m_nextShotTime(0.0f),
	m_shootSeqN(1)
{
	RegisterActions();
}

//------------------------------------------------------------------------1
CWeapon::~CWeapon()
{
	// deactivate everything
	for (TFireModeVector::iterator it = m_firemodes.begin(); it != m_firemodes.end(); it++)
		(*it)->Activate(false);
	// deactivate zoommodes
	for (TZoomModeVector::iterator it = m_zoommodes.begin(); it != m_zoommodes.end(); it++)
		(*it)->Activate(false);

	// clean up firemodes
	for (TFireModeVector::iterator it = m_firemodes.begin(); it != m_firemodes.end(); it++)
		(*it)->Release();
	// clean up zoommodes
	for (TZoomModeVector::iterator it = m_zoommodes.begin(); it != m_zoommodes.end(); it++)
		(*it)->Release();

	if (m_pFiringLocator)
		m_pFiringLocator->WeaponReleased();

	if (m_fmDefaults)
		m_fmDefaults->Release();
	if (m_zmDefaults)
		m_zmDefaults->Release();
	if (m_xmlparams)
		m_xmlparams->Release();

}

//------------------------------------------------------------------------
bool CWeapon::ReadItemParams(const IItemParamsNode *root)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if (!CItem::ReadItemParams(root))
		return false;

	// read params
	string melee_attack_firemode;
	const IItemParamsNode *params = root->GetChild("params");
	{
		CItemParamReader reader(params);
		reader.Read("melee_attack_firemode", melee_attack_firemode);
	}

	const IItemParamsNode *firemodes = root->GetChild("firemodes");
	InitFireModes(firemodes);

	const IItemParamsNode *zoommodes = root->GetChild("zoommodes");
	InitZoomModes(zoommodes);

	const IItemParamsNode *ammos = root->GetChild("ammos");
	InitAmmos(ammos);

	const IItemParamsNode *aiData = root->GetChild("ai_descriptor");
	InitAIData(aiData);

	m_xmlparams = root;
	m_xmlparams->AddRef();

	if (!melee_attack_firemode.empty())
	{
		m_melee = GetFireMode(melee_attack_firemode.c_str());
		if (m_melee)
			m_melee->Enable(false);
	}

	CacheRaisePose();

	return true;
}

//------------------------------------------------------------------------
const IItemParamsNode *CWeapon::GetFireModeParams(const char *name)
{
	if (!m_xmlparams)
		return 0;

	const IItemParamsNode *firemodes = m_xmlparams->GetChild("firemodes");
	if (!firemodes)
		return 0;

	int n = firemodes->GetChildCount();
	for (int i=0; i<n; i++)
	{
		const IItemParamsNode *fm = firemodes->GetChild(i);

		const char *fmname = fm->GetAttribute("name");
		if (!fmname || !fmname[0] || stricmp(name, fmname))
			continue;

		return fm;
	}

	return 0;
}

//------------------------------------------------------------------------
const IItemParamsNode *CWeapon::GetZoomModeParams(const char *name)
{
	if (!m_xmlparams)
		return 0;

	const IItemParamsNode *zoommodes = m_xmlparams->GetChild("zoommodes");
	if (!zoommodes)
		return 0;

	int n = zoommodes->GetChildCount();
	for (int i=0; i<n; i++)
	{
		const IItemParamsNode *zm = zoommodes->GetChild(i);

		const char *zmname = zm->GetAttribute("name");
		if (!zmname || !zmname[0] || stricmp(name, zmname))
			continue;

		return zm;
	}

	return 0;
}

//------------------------------------------------------------------------
void CWeapon::InitFireModes(const IItemParamsNode *firemodes)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	m_firemodes.resize(0);
	m_fmIds.clear();
	m_fm = 0;

	if (!firemodes)
		return;

	int n = firemodes->GetChildCount();

	// find the default params
	m_fmDefaults = 0;
	for (int k=0; k<n; k++)
	{
		const IItemParamsNode *fm = firemodes->GetChild(k);
		const char *typ = fm->GetAttribute("type");

		if (typ && !strcmpi(typ, "default"))
		{
			m_fmDefaults = fm;
			m_fmDefaults->AddRef();
			break;
		}
	}

	for (int i=0; i<n; i++)
	{
		const IItemParamsNode *fm = firemodes->GetChild(i);

		int enabled = 1;
		const char *name = fm->GetAttribute("name");
		const char *typ = fm->GetAttribute("type");
		fm->GetAttribute("enabled", enabled);
		
		if (!typ || !typ[0])
		{
			GameWarning("Missing type for firemode in weapon '%s'! Skipping...", GetEntity()->GetName());
			continue;
		}

		if (!strcmpi(typ, "default"))
			continue;

		if (!name || !name[0])
		{
			GameWarning("Missing name for firemode in weapon '%s'! Skipping...", GetEntity()->GetName());
			continue;
		}

		IFireMode *pFireMode = g_pGame->GetWeaponSystem()->CreateFireMode(typ);
		if (!pFireMode)
		{
			GameWarning("Cannot create firemode '%s' in weapon '%s'! Skipping...", typ, GetEntity()->GetName());
			continue;
		}
		pFireMode->SetName(name);

		pFireMode->Init(this, m_fmDefaults);
		pFireMode->PatchParams(fm);
		pFireMode->Enable(enabled!=0);

		m_firemodes.push_back(pFireMode);
		m_fmIds.insert(TFireModeIdMap::value_type(name, m_firemodes.size()-1));
	}

	GetGameObject()->SetAspectProfile(ASPECT_FIREMODE, 0);
}

//------------------------------------------------------------------------
void CWeapon::InitZoomModes(const IItemParamsNode *zoommodes)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	m_zoommodes.resize(0);
	m_zmIds.clear();
	m_zmId = 0;
	m_zm = 0;

	m_viewModeList.resize(0);
	m_useViewMode = false;

	if (!zoommodes)
		return;

	int n = zoommodes->GetChildCount();

	// find the default params
	m_zmDefaults = 0;
	for (int k=0; k<n; k++)
	{
		const IItemParamsNode *zm = zoommodes->GetChild(k);
		const char *typ = zm->GetAttribute("type");

		if (typ && !strcmpi(typ, "default"))
		{
			m_zmDefaults = zm;
			m_zmDefaults->AddRef();
			break;
		}
	}

	for (int i=0; i<n; i++)
	{
		const IItemParamsNode *zm = zoommodes->GetChild(i);

		int enabled = 1;
		const char *name = zm->GetAttribute("name");
		const char *typ = zm->GetAttribute("type");
		zm->GetAttribute("enabled", enabled);
		
		if (!typ || !typ[0])
		{
			GameWarning("Missing type for zoommode in weapon '%s'! Skipping...", GetEntity()->GetName());
			continue;
		}

		if (!strcmpi(typ, "default"))
			continue;

		if (!name || !name[0])
		{
			GameWarning("Missing name for zoommode in weapon '%s'! Skipping...", GetEntity()->GetName());
			continue;
		}

		IZoomMode *pZoomMode = g_pGame->GetWeaponSystem()->CreateZoomMode(typ);
		if (!pZoomMode)
		{
			GameWarning("Cannot create zoommode '%s' in weapon '%s'! Skipping...", typ, GetEntity()->GetName());
			continue;
		}

		// viewmodes - used for binoculars
		const IItemParamsNode *viewModes = zm->GetChild("viewmodes");
		if (viewModes)
		{
			for (int j = 0; j < viewModes->GetChildCount(); j++)
			{
				const IItemParamsNode *viewMode = viewModes->GetChild(j);
				if (viewMode)
				{
					const char *modeValue = 0;
					CItemParamReader reader(viewMode);
					reader.Read("mode", modeValue);

					if (modeValue)
					{
						if (!strcmp(modeValue, "normal"))
						{
							m_currentViewMode = m_viewModeList.size(); // set default view mode
							m_defaultViewMode = m_currentViewMode;
						}

						m_viewModeList.push_back(modeValue);
						m_useViewMode = true;
					}
				}
			}
		}

		pZoomMode->Init(this, m_zmDefaults);
		pZoomMode->PatchParams(zm);
		pZoomMode->Enable(enabled!=0);

		m_zoommodes.push_back(pZoomMode);
		m_zmIds.insert(TZoomModeIdMap::value_type(name, m_zoommodes.size()-1));
	}
}

//------------------------------------------------------------------------
void CWeapon::InitAmmos(const IItemParamsNode *ammos)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	m_ammo.clear();

	if (!ammos)
		return;

	for (int i=0; i<ammos->GetChildCount(); i++)
	{
		const IItemParamsNode *ammo = ammos->GetChild(i);
		if (!strcmpi(ammo->GetName(), "ammo"))
		{
			int extra=0;
			int amount=0;
			int accessoryAmmo=0;
			int minAmmo=0;

			const char* name = ammo->GetAttribute("name");
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			assert(pClass);

			ammo->GetAttribute("amount", amount);
			ammo->GetAttribute("extra", extra);
			ammo->GetAttribute("accessoryAmmo", accessoryAmmo);
			ammo->GetAttribute("minAmmo", minAmmo);

			if(accessoryAmmo)
			{
				m_accessoryAmmo[pClass]=accessoryAmmo;
				m_ammo[pClass]=accessoryAmmo;
			}
			else if (amount)
				m_ammo[pClass]=amount;

			if (extra)
				m_bonusammo[pClass]=extra;

			if (minAmmo)
				m_minDroppedAmmo[pClass]=minAmmo;


		}
	}
}

//------------------------------------------------------------------------
void CWeapon::InitAIData(const IItemParamsNode *aiDescriptor)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if (!aiDescriptor)
		return;
//	<ai_descriptor	hit="instant" speed="20" damage_radius="45" charge_time="2.5" />
	
	aiDescriptor->GetAttribute("speed", m_aiWeaponDescriptor.fSpeed);
	aiDescriptor->GetAttribute("damage_radius", m_aiWeaponDescriptor.fDamageRadius);
	aiDescriptor->GetAttribute("charge_time", m_aiWeaponDescriptor.fChargeTime);
	aiDescriptor->GetAttribute("burstBulletCountMin", m_aiWeaponDescriptor.burstBulletCountMin);
	aiDescriptor->GetAttribute("burstBulletCountMax", m_aiWeaponDescriptor.burstBulletCountMax);
	aiDescriptor->GetAttribute("burstPauseTimeMin", m_aiWeaponDescriptor.burstPauseTimeMin);
	aiDescriptor->GetAttribute("burstPauseTimeMax", m_aiWeaponDescriptor.burstPauseTimeMax);
	aiDescriptor->GetAttribute("singleFireTriggerTime", m_aiWeaponDescriptor.singleFireTriggerTime);
	aiDescriptor->GetAttribute("spreadRadius", m_aiWeaponDescriptor.spreadRadius);
	aiDescriptor->GetAttribute("coverFireTime", m_aiWeaponDescriptor.coverFireTime);
	aiDescriptor->GetAttribute("sweep_width", m_aiWeaponDescriptor.sweepWidth);
	aiDescriptor->GetAttribute("sweep_frequency", m_aiWeaponDescriptor.sweepFrequency);
	aiDescriptor->GetAttribute("draw_time", m_aiWeaponDescriptor.drawTime);
	m_aiWeaponDescriptor.smartObjectClass = aiDescriptor->GetAttribute("smartobject_class");
	m_aiWeaponDescriptor.firecmdHandler = aiDescriptor->GetAttribute("handler");
	int	signalOnShoot(0);
	aiDescriptor->GetAttribute("signal_on_shoot", signalOnShoot);
	m_aiWeaponDescriptor.bSignalOnShoot = signalOnShoot != 0;

	if(m_aiWeaponDescriptor.smartObjectClass != "")
	{
		const char* smartObjectClassProperties = NULL;
		// check if the smartobject class has been overridden in the level
		SmartScriptTable props;
		if (GetEntity()->GetScriptTable() && GetEntity()->GetScriptTable()->GetValue("Properties", props))
		{
			if(!props->GetValue("soclasses_SmartObjectClass",smartObjectClassProperties) || 
				(smartObjectClassProperties ==NULL || smartObjectClassProperties[0]==0))
			{
				props->SetValue("soclasses_SmartObjectClass",m_aiWeaponDescriptor.smartObjectClass.c_str());
			}
		}
	}

	InitAIOffsets( aiDescriptor->GetChild("weaponOffset") );
}

//------------------------------------------------------------------------
void CWeapon::InitAIOffsets(const IItemParamsNode *aiOffsetData)
{
	if(!aiOffsetData)
		return;

	if(aiOffsetData->GetChild("useEyeOffset"))
	{
		m_AIUseEyeOffset = true;
	}
	else
	{
		int n = aiOffsetData->GetChildCount();
		for (int i=0; i<n; ++i)
		{
			const IItemParamsNode *offsetNode = aiOffsetData->GetChild(i);
			const char *stance = offsetNode->GetAttribute("stanceId");
			// retrieve enum value from scripts, using stance name
			int curStanceInt(STANCE_NULL);
			if(!gEnv->pScriptSystem->GetGlobalValue(stance,curStanceInt))
				continue;
			EStance	curStance((EStance)curStanceInt);
			if(curStance==STANCE_NULL)
				continue;
			Vec3	curOffset;
			if(!offsetNode->GetAttribute("weaponOffset", curOffset))
				continue;
			m_StanceWeponOffset[curStance] = curOffset;
			if(!offsetNode->GetAttribute("weaponOffsetLeanLeft", curOffset))
				continue;
			m_StanceWeponOffsetLeanLeft[curStance] = curOffset;
			if(!offsetNode->GetAttribute("weaponOffsetLeanRight", curOffset))
				continue;
			m_StanceWeponOffsetLeanRight[curStance] = curOffset;
		}
	}
}


//------------------------------------------------------------------------
bool CWeapon::Init( IGameObject * pGameObject )
{
	if (!CItem::Init(pGameObject))
		return false;

	g_pGame->GetWeaponScriptBind()->AttachTo(this);

	return true;
}

//------------------------------------------------------------------------
void CWeapon::Release()
{
	delete this;
}

//------------------------------------------------------------------------
bool CWeapon::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if (!CItem::NetSerialize(ser, aspect, profile, flags))
		return false;

	if (aspect == ASPECT_FIREMODE) // only the current firemode id, and the current firemode are serialized
	{
		if (profile < m_firemodes.size())
			m_firemodes[profile]->Serialize(ser);
	}

	if (aspect == ASPECT_AMMO)
		ser.Value("ammo", this, &CWeapon::NetGetCurrentAmmoCount, &CWeapon::NetSetCurrentAmmoCount, 'ammo');

	// zoom modes are not serialized

	return true;
}

//------------------------------------------------------------------------
void CWeapon::FullSerialize( TSerialize ser )
{
	CItem::FullSerialize(ser);

	ser.BeginGroup("WeaponAmmo");
	if(ser.IsReading())
	{
		m_ammo.clear();
		m_bonusammo.clear();
		m_minDroppedAmmo.clear();
	}
	TAmmoMap::iterator it = m_ammo.begin();
	int ammoAmount = m_ammo.size();
	ser.Value("AmmoAmount", ammoAmount);
	for(int i = 0; i < ammoAmount; ++i, ++it)
	{
		string name;
		int amount = 0;
		if(ser.IsWriting() && it->first)
		{
			name = it->first->GetName();
			amount = it->second;
		}

		ser.BeginGroup("Ammo");
		ser.Value("AmmoName", name);
		ser.Value("Bullets", amount);
		ser.EndGroup();

		if(ser.IsReading())
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			assert(pClass);
			m_ammo[pClass] = amount;
		}
	}
	it = m_bonusammo.begin();
	ammoAmount = m_bonusammo.size();
	ser.Value("BonusAmmoAmount", ammoAmount);
	for(int i = 0; i < ammoAmount; ++i, ++it)
	{
		string name;
		int amount = 0;
		if(ser.IsWriting() && it->first)
		{
			name = it->first->GetName();
			amount = it->second;
		}

		ser.BeginGroup("Ammo");
		ser.Value("AmmoName", name);
		ser.Value("Bullets", amount);
		ser.EndGroup();

		if(ser.IsReading())
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			assert(pClass);
			m_bonusammo[pClass] = amount;
		}
	}
	it = m_minDroppedAmmo.begin();
	ammoAmount = m_minDroppedAmmo.size();
	ser.Value("minDroppedAmmo", ammoAmount);
	for(int i = 0; i < ammoAmount; ++i, ++it)
	{
		string name;
		int amount = 0;
		if(ser.IsWriting() && it->first)
		{
			name = it->first->GetName();
			amount = it->second;
		}

		ser.BeginGroup("Ammo");
		ser.Value("AmmoName", name);
		ser.Value("Bullets", amount);
		ser.EndGroup();

		if(ser.IsReading())
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			assert(pClass);
			m_minDroppedAmmo[pClass] = amount;
		}
	}
	ser.EndGroup();

	ser.BeginGroup("CrossHairStats");
	ser.Value("CHVisible", m_crosshairstats.visible);
	ser.Value("CHOpacity", m_crosshairstats.opacity);
	ser.Value("CHFading", m_crosshairstats.fading);
	ser.Value("CHFadeFrom", m_crosshairstats.fadefrom);
	ser.Value("CHFadeTo", m_crosshairstats.fadeto);
	ser.Value("CHFadeTime", m_crosshairstats.fadetime);
	ser.Value("CHFadeTimer", m_crosshairstats.fadetimer);
	ser.EndGroup();

	if(GetOwnerId())
	{
		ser.BeginGroup("WeaponStats");
		ser.Value("silencer", m_silencerAttached);

		//if (m_fm)
     // m_fm->Serialize(ser);
		int numFiremodes = GetNumOfFireModes();
		ser.Value("numFiremodes", numFiremodes);
		if(ser.IsReading())
		{
			assert ( numFiremodes == GetNumOfFireModes() );
			if(numFiremodes != GetNumOfFireModes())
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Num of firemodes changed - loading will be corrupted.");
		}
		for(int i = 0; i < numFiremodes; ++i)
			m_firemodes[i]->Serialize(ser);
    
		bool hasZoom = (m_zm)?true:false;
		ser.Value("hasZoom", hasZoom);
		if(hasZoom)
		{
			int zoomMode = m_zmId;
			ser.Value("ZoomMode", zoomMode);
			bool isZoomed = m_zm->IsZoomed();
			ser.Value("Zoomed", isZoomed);
			int zoomStep = m_zm->GetCurrentStep();
			ser.Value("ZoomStep", zoomStep);

			m_zm->Serialize(ser);

			if(ser.IsReading())
			{
				if(m_zmId != zoomMode)
					SetCurrentZoomMode(zoomMode);

				m_restartZoom = isZoomed;
				m_restartZoomStep = (zoomStep<=1)?zoomStep:1; //Only enter first zoom step, not succesive

        if (!isZoomed)
          m_zm->ExitZoom();
			}
		}

		//serialize special post processing visions
		if(ser.IsWriting())
			m_zoomViewMode = m_currentViewMode;
		ser.Value("currentViewMode", m_zoomViewMode);

		bool reloading = false;
		if(ser.IsWriting())    
			reloading = m_fm ? m_fm->IsReloading() : 0;
		ser.Value("FireModeReloading", reloading);
		if(ser.IsReading() && reloading)
			Reload();
		ser.Value("Alternation", m_fire_alternation);
		ser.Value("ForcedHitMaterial", m_forcedHitMaterial);
		ser.EndGroup();
	}
}

//------------------------------------------------------------------------
void CWeapon::PostSerialize()
{
	m_nextShotTime = 0.0f; //Reset this after loading

	CItem::PostSerialize();

	for (TAccessoryMap::iterator it=m_accessories.begin(); it!=m_accessories.end(); ++it)
		FixAccessories(GetAccessoryParams(it->first), true);

	//Firemodes 
	int numFiremodes = GetNumOfFireModes();
	for(int i = 0; i < numFiremodes; ++i)
		m_firemodes[i]->PostSerialize();
}

//------------------------------------------------------------------------
void CWeapon::SerializeLTL(TSerialize ser)
{
	CItem::SerializeLTL(ser);

	ser.BeginGroup("WeaponAmmo");
	if(ser.IsReading())
	{
		m_ammo.clear();
		m_bonusammo.clear();
		m_nextShotTime = 0.0f; //Reset this after loading
	}
	TAmmoMap::iterator it = m_ammo.begin();
	int ammoAmount = m_ammo.size();
	ser.Value("AmmoAmount", ammoAmount);
	for(int i = 0; i < ammoAmount; ++i, ++it)
	{
		string name;
		int amount = 0;
		if(ser.IsWriting() && it->first)
		{
			name = it->first->GetName();
			amount = it->second;
		}

		ser.BeginGroup("Ammo");
		ser.Value("AmmoName", name);
		ser.Value("Bullets", amount);
		ser.EndGroup();

		if(ser.IsReading())
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			assert(pClass);
			m_ammo[pClass] = amount;
		}
	}
	ser.EndGroup();

	if(GetOwnerId())
	{
		ser.BeginGroup("WeaponStats");
		ser.Value("silencer", m_silencerAttached);

		//if (m_fm)
		// m_fm->Serialize(ser);
		int numFiremodes = GetNumOfFireModes();
		ser.Value("numFiremodes", numFiremodes);
		if(ser.IsReading())
		{
			assert ( numFiremodes == GetNumOfFireModes() );
			if(numFiremodes != GetNumOfFireModes())
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Num of firemodes changed - loading will be corrupted.");
		}
		for(int i = 0; i < numFiremodes; ++i)
			m_firemodes[i]->Serialize(ser);
		int currentFireMode = GetCurrentFireMode();
		ser.Value("currentFireMode", currentFireMode);
		if(ser.IsReading())
			SetCurrentFireMode(currentFireMode);

		bool hasZoom = (m_zm)?true:false;
		ser.Value("hasZoom", hasZoom);
		if(hasZoom)
		{
			int zoomMode = m_zmId;
			ser.Value("ZoomMode", zoomMode);
			bool isZoomed = m_zm->IsZoomed();
			ser.Value("Zoomed", isZoomed);
			int zoomStep = m_zm->GetCurrentStep();
			ser.Value("ZoomStep", zoomStep);

			m_zm->Serialize(ser);

			if(ser.IsReading())
			{
				if(m_zmId != zoomMode)
					SetCurrentZoomMode(zoomMode);

				m_restartZoom = isZoomed;
				m_restartZoomStep = zoomStep;

				if (!isZoomed)
					m_zm->ExitZoom();
			}
		}

		//serialize special post processing visions
		if(ser.IsWriting())
			m_zoomViewMode = m_currentViewMode;
		ser.Value("currentViewMode", m_zoomViewMode);

		bool reloading = false;
		if(ser.IsWriting())    
			reloading = m_fm ? m_fm->IsReloading() : 0;
		ser.Value("FireModeReloading", reloading);
		if(ser.IsReading() && reloading)
			Reload();
		ser.Value("Alternation", m_fire_alternation);
		ser.Value("ForcedHitMaterial", m_forcedHitMaterial);
		ser.EndGroup();
	}
}

//------------------------------------------------------------------------
void CWeapon::Update( SEntityUpdateContext& ctx, int update)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if (m_frozen || IsDestroyed())
		return;

	switch (update)
	{
	case eIUS_FireMode:
		if (m_fm)
			m_fm->Update(ctx.fFrameTime, ctx.nFrameID);
		if (m_melee)
			m_melee->Update(ctx.fFrameTime, ctx.nFrameID);
		break;

	case eIUS_Zooming:
		if (m_zm)
			m_zm->Update(ctx.fFrameTime, ctx.nFrameID);
		break;
	}
  
	CItem::Update(ctx, update);


	if (update==eIUS_General)
	{
		if (fabsf(m_dofSpeed)>0.001f)
		{
			m_dofValue += m_dofSpeed*ctx.fFrameTime;
			m_dofValue = CLAMP(m_dofValue, 0, 1);

			//GameWarning("Actual DOF value = %f",m_dofValue);
			if(m_dofSpeed < 0.0f)
			{
				m_focusValue -= m_dofSpeed*ctx.fFrameTime*150.0f;
				gEnv->p3DEngine->SetPostEffectParam("Dof_FocusLimit", 20.0f + m_focusValue);
			}
			gEnv->p3DEngine->SetPostEffectParam("Dof_BlurAmount", m_dofValue);
		}
	}
}

void CWeapon::PostUpdate(float frameTime )
{
	if (m_fm)
		m_fm->PostUpdate(frameTime);
}

//------------------------------------------------------------------------
void CWeapon::HandleEvent( const SGameObjectEvent& event)
{
	CItem::HandleEvent(event);
}

//------------------------------------------------------------------------
void CWeapon::ProcessEvent(SEntityEvent& event)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	switch(event.event)
	{
		case ENTITY_EVENT_HIDE:
			{
				if(m_fm && !m_fm->AllowZoom())
					m_fm->Cancel();
				StopFire();
				break;
			}
		case ENTITY_EVENT_RESET:
			{
				//Leaving game mode
				if(gEnv->pSystem->IsEditor() && !event.nParam[0])
				{
					m_listeners.clear(); 
					break;
				}
			}
	}

	CItem::ProcessEvent(event);
}
//--------------------------------	----------------------------------------
void CWeapon::SetAuthority(bool auth)
{
	CItem::SetAuthority(auth);
}

//------------------------------------------------------------------------
void CWeapon::Reset()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	// deactivate everything
	for (TFireModeVector::iterator it = m_firemodes.begin(); it != m_firemodes.end(); it++)
		(*it)->Activate(false);
	// deactivate zoommodes
	for (TZoomModeVector::iterator it = m_zoommodes.begin(); it != m_zoommodes.end(); it++)
		(*it)->Activate(false);

	// clean up firemodes
	for (TFireModeVector::iterator it = m_firemodes.begin(); it != m_firemodes.end(); it++)
		(*it)->Release();
  	
  // clean up zoommodes
	for (TZoomModeVector::iterator it = m_zoommodes.begin(); it != m_zoommodes.end(); it++)
		(*it)->Release();
  

	m_firemodes.resize(0);
  m_fm=0;

	m_zoommodes.resize(0);
  m_zm=0;

	if (m_fmDefaults)
	{
		m_fmDefaults->Release();
		m_fmDefaults=0;
	}
	if (m_zmDefaults)
	{
		m_zmDefaults->Release();
		m_zmDefaults=0;
	}
	if (m_xmlparams)
	{
		m_xmlparams->Release();
		m_xmlparams=0;
	}

	CItem::Reset();
	ClearInputFlags();

	SetCurrentFireMode(0);
	SetCurrentZoomMode(0);

	
	// have to refix them here.. (they get overriden by SetCurrentFireMode above)
	for (TAccessoryMap::iterator it=m_accessories.begin(); it!=m_accessories.end(); ++it)
		FixAccessories(GetAccessoryParams(it->first), true);

	m_firedRockets = 0;
	m_raiseProbability = 0.0f;
}

//------------------------------------------------------------------------
void CWeapon::UpdateFPView(float frameTime)
{
	CItem::UpdateFPView(frameTime);

	UpdateWeaponRaising(frameTime);
	UpdateWeaponLowering(frameTime);

	UpdateCrosshair(frameTime);
	if (m_fm)
		m_fm->UpdateFPView(frameTime);
	if (m_zm)
		m_zm->UpdateFPView(frameTime);

}

//------------------------------------------------------------------------
void CWeapon::MeleeAttack()
{
	if (!CanMeleeAttack())
		return;

	if (m_melee)
	{
		if (m_fm)
		{
			if(!m_fm->AllowZoom())
				m_fm->Cancel();

			m_fm->StopFire();
			if(m_fm->IsReloading())
			{
				if(m_fm->CanCancelReload())
					RequestCancelReload();
				else
				{
					RequestCancelReload();
					return;
				}
			}
		}
		m_melee->Activate(true);
		m_melee->StartFire();
		m_melee->StopFire();
	}
}

//------------------------------------------------------------------------
bool CWeapon::CanMeleeAttack() const
{
	if(m_modifying || m_transitioning)
		return false;

	CActor *act = GetOwnerActor();
	if (act)
	{
		if (IMovementController *pMV = act->GetMovementController())
		{
			SMovementState state;
			pMV->GetMovementState(state);
			if (state.stance == STANCE_PRONE)
				return false;
		}
	}
	return m_melee && m_melee->CanFire();
}

//------------------------------------------------------------------------
void CWeapon::Select(bool select)
{
	CActor *pOwner = GetOwnerActor();
	if (select && (IsDestroyed() || (pOwner && pOwner->GetHealth() <= 0)))
		return;
	
	bool isClient = pOwner?pOwner->IsClient():false;

	//If actor is grabbed by player, don't let him select weapon
	if (select && pOwner && pOwner->GetActorStats() && pOwner->GetActorStats()->isGrabbed)
	{
		pOwner->HolsterItem(true);
		return;
	}

	CItem::Select(select);
	// apply frozen material layer
	if (g_pGame->GetWeaponSystem()->IsFrozenEnvironment())
	{
		FrostEnable(select, false);
	}
	ClearInputFlags();

	if (g_pGameCVars && g_pGameCVars->bt_end_select && isClient)
		g_pGame->GetBulletTime()->Activate(false);

	if (!select)
	{
		if(GetEntity()->GetClass() == CItem::sTACGunFleetClass)
			SAFE_HUD_FUNC(ShowProgress(-1));

		if(IsZoomed() || IsZooming())
			ExitZoom();

		ExitViewmodes();

		//Prevent offhand deselection to clear Dof effect
		if(isClient && (GetEntity()->GetClass()!=CItem::sOffHandClass))
			gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 0.0f);

		m_switchingFireMode = false;
	}

	if(m_weaponRaised)
	{
		SetWeaponRaised(false);
		if(IsDualWieldSlave())
			SetDefaultIdleAnimation(eIGS_FirstPerson,g_pItemStrings->idle);
	}

	RestorePlayerSprintingStats();
	FadeCrosshair(0, 1.0f, WEAPON_FADECROSSHAIR_SELECT);

	if(!select)
		SetNextShotTime(false);

	if (m_fm)
		m_fm->Activate(select);

	if (m_zm && !IsDualWield())
		m_zm->Activate(select);

	if(select)
		SetNextShotTime(true);
}

//------------------------------------------------------------------------
void CWeapon::Drop(float impulseScale, bool selectNext, bool byDeath)
{
	CActor* pOwner = GetOwnerActor();
	if(pOwner && !pOwner->IsPlayer())
	{
		for (TFireModeVector::iterator it=m_firemodes.begin(); it!=m_firemodes.end(); ++it)
		{
			IFireMode *fm=*it;
			if (!fm)
				continue;

			IEntityClass* ammo=fm->GetAmmoType();
			
			bool done = false;
			//AI have infinite ammo, we give the player always some extra ammunition
			for(TAmmoMap::const_iterator it = m_minDroppedAmmo.begin();it!=m_minDroppedAmmo.end();it++)
			{
				if(ammo==it->first)
				{
					//Do it only once
					m_ammo[ammo] = min(it->second+m_ammo[ammo],fm->GetClipSize());
					done = true;
					break;
				}
			}
			if(done)
				break;
		}
	
		m_minDroppedAmmo.clear();
	}
	else if (byDeath)
	{
		for (TFireModeVector::iterator it=m_firemodes.begin(); it!=m_firemodes.end(); ++it)
		{
			IFireMode *fm=*it;
			if (!fm)
				continue;

			IEntityClass* ammo=fm->GetAmmoType();
			int invCount=GetInventoryAmmoCount(ammo);
			if (invCount)
			{
				SetInventoryAmmoCount(ammo, 0);
				m_bonusammo[ammo]=invCount;
			}
		}
	}

	if(IsDualWieldMaster() && ((gEnv->bMultiplayer && IsServer()) || (pOwner && pOwner->IsClient())))
	{
		IEntityClass* pAmmoType = m_fm?m_fm->GetAmmoType():NULL;

		if(pAmmoType)
		{
			int ammoCount	= GetAmmoCount(pAmmoType);
			int slaveAmmo = 0;
			CWeapon* pSlave = GetDualWieldSlave()?static_cast<CWeapon*>(GetDualWieldSlave()->GetIWeapon()):NULL;
			if(pSlave)
			{
				//Check for same ammo as master
				slaveAmmo = pSlave->GetAmmoCount(pAmmoType);
				if(slaveAmmo>ammoCount)
				{
					//Swap ammo counts, it's like dropping the gun with less ammo
					pSlave->SetAmmoCount(pAmmoType,ammoCount);
					SetAmmoCount(pAmmoType,slaveAmmo);


				}
			}
		}
	}

	CItem::Drop(impulseScale, selectNext, byDeath);
}

//------------------------------------------------------------------------
void CWeapon::Freeze(bool freeze)
{
	CItem::Freeze(freeze);

	if (freeze && m_fm)
	{
		m_fm->Activate(false);
		m_fm->Activate(true);

		StopFire();
	}
}

//------------------------------------------------------------------------
void CWeapon::SetFiringLocator(IWeaponFiringLocator *pLocator)
{
	if (m_pFiringLocator && m_pFiringLocator != pLocator)
		m_pFiringLocator->WeaponReleased();
	m_pFiringLocator = pLocator;
};

//------------------------------------------------------------------------
IWeaponFiringLocator *CWeapon::GetFiringLocator() const
{
	return m_pFiringLocator;
};

//------------------------------------------------------------------------
void CWeapon::AddEventListener(IWeaponEventListener *pListener, const char *who)
{
	for (TEventListenerVector::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		if (it->pListener == pListener)
			return;
	}

	SListenerInfo info;
	info.pListener=pListener;
#ifdef _DEBUG
	memset(info.who, 0, sizeof(info.who));
	strncpy(info.who, who, 64);
	info.who[63]=0;
#endif
	m_listeners.push_back(info);
}

//------------------------------------------------------------------------
void CWeapon::RemoveEventListener(IWeaponEventListener *pListener)
{
	for (TEventListenerVector::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		if (it->pListener == pListener)
		{
			m_listeners.erase(it);
			return;
		}
	}
}

//------------------------------------------------------------------------
Vec3 CWeapon::GetFiringPos(const Vec3 &probableHit) const
{
	if (m_fm)
		return m_fm->GetFiringPos(probableHit);

	return Vec3(0,0,0);
}

//------------------------------------------------------------------------
Vec3 CWeapon::GetFiringDir(const Vec3 &probableHit, const Vec3& firingPos) const
{
	if (m_fm)
		return m_fm->GetFiringDir(probableHit, firingPos);

	return Vec3(0,0,0);
}

//------------------------------------------------------------------------
void CWeapon::StartFire()
{
	CActor *pOwner = GetOwnerActor();
  if (IsDestroyed())
    return;

	if(pOwner)
	{
		bool isPlayer = pOwner->IsPlayer();
		if((pOwner->GetHealth() <= 0) || (!pOwner->CanFire()))
			return;
		//Dual socoms for AI
		if(!gEnv->bMultiplayer && !isPlayer && IsDualWieldMaster() && FireSlave(GetOwnerId(),true))
			return;
	}

	if (m_fm)
		m_fm->StartFire();
}

//------------------------------------------------------------------------
void CWeapon::StopFire()
{
	//Stop slave
	if(IsDualWieldMaster())
		FireSlave(GetOwnerId(), false);

	if (m_fm)
		m_fm->StopFire();
}

//------------------------------------------------------------------------
bool CWeapon::CanFire() const
{
	return m_fm && m_fm->CanFire();
}

//------------------------------------------------------------------------
void CWeapon::StartZoom(EntityId actorId, int zoomed)
{
	IActor *pOwner = GetOwnerActor();
	if (IsDestroyed() || (pOwner && pOwner->GetHealth() <= 0))
		return;

	CPlayer* pPlayer = static_cast<CPlayer*>(pOwner);
	if(pPlayer && pPlayer->IsSprinting())
		return;

	if (m_zm)
	{
		bool stayZoomed = (m_zm->IsZoomed() && m_zm->IsZooming() && !m_zm->IsToggle());
		m_zm->StartZoom(stayZoomed, true, zoomed);
	}
	else
	{
		// If the view does not zoom, we need to force aim assistance
		AssistAiming(1, true);
	}
}

//------------------------------------------------------------------------
void CWeapon::StopZoom(EntityId actorId)
{
	if (m_zm)
		m_zm->StopZoom();
}

//------------------------------------------------------------------------
bool CWeapon::CanZoom() const
{
	return m_zm && m_zm->CanZoom();
}

//------------------------------------------------------------------------
void CWeapon::ExitZoom()
{
	if (m_zm)
		m_zm->ExitZoom();
}

//------------------------------------------------------------------------
bool CWeapon::IsZoomed() const
{
	return m_zm && m_zm->IsZoomed();
}

//------------------------------------------------------------------------
bool CWeapon::IsZooming() const
{
	return m_zm && m_zm->IsZooming();
}

//---------------------------------------------------------------------
bool CWeapon::IsReloading() const
{
	return m_fm && m_fm->IsReloading();
}

void CWeapon::AssistAiming(float magnification/*=1.0f*/, bool accurate/*=false*/)
{
	// Check for assistance switches
	if (!accurate && !g_pGameCVars->aim_assistTriggerEnabled)
		return;

	if (accurate && !g_pGameCVars->aim_assistAimEnabled)
		return;
		
	// Check for assistance restriction
	if (!SAFE_HUD_FUNC_RET(IsInputAssisted()))
		return;

	IEntity *res=NULL;

	IActor *pSelfActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pSelfActor)
		return;

	IEntity *pSelf=pSelfActor->GetEntity();
	if (GetOwner() != pSelf)
		return;

	// Do not use in case of vehicle mounted weaponry
	if (pSelfActor->GetLinkedVehicle())
		return;

	IPhysicalEntity *pSelfPhysics=pSelf->GetPhysics();
	IMovementController * pMC = pSelfActor->GetMovementController();
	if(!pMC || !pSelfPhysics)
		return;

	SMovementState info;
	pMC->GetMovementState(info);

	// If already having a valid target, don't do anything
	ray_hit hit;
	if (gEnv->pPhysicalWorld->RayWorldIntersection(info.eyePosition, info.eyeDirection*500.0f, ent_all, (13&rwi_pierceability_mask), &hit, 1, &pSelfPhysics, 1))
	{
		if (!hit.bTerrain && hit.pCollider != pSelfPhysics)
		{
			if (IsValidAssistTarget(gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider), pSelf))
				return;
		}
	}

	const CCamera &camera=GetISystem()->GetViewCamera();
	Vec3 origo=camera.GetPosition();
	// PARAMETER: Autotarget search radius
	const float radius=g_pGameCVars->aim_assistSearchBox;
	// Using radar's proximity check result for optimization
	/*SEntityProximityQuery query;
	query.box= AABB(Vec3(origo.x-radius,origo.y-radius,origo.z-radius),
									Vec3(origo.x+radius,origo.y+radius,origo.z+radius));
	query.nEntityFlags = ENTITY_FLAG_ON_RADAR;
	gEnv->pEntitySystem->QueryProximity(query);*/
	
	// Some other ray may be used too, like weapon aiming or firing ray 
	Line aim=Line(info.eyePosition, info.eyeDirection);

	int betsTarget=-1;
	// PARAMETER: The largest deviance between aiming and target compensated by autoaim
	//	Magnification is used for scopes, but greatly downscaled to avoid random snaps in viewfield
	float maxSnapSqr=g_pGameCVars->aim_assistSnapDistance*sqrt(magnification);
	maxSnapSqr*=maxSnapSqr;
	float bestSnapSqr=maxSnapSqr;
	// PARAMETER: maximum range at which autotargetting works
	float maxDistanceSqr=g_pGameCVars->aim_assistMaxDistance;
	maxDistanceSqr*=maxDistanceSqr;
	Vec3 bestTarget;

	const std::vector<EntityId> *entitiesInProximity = SAFE_HUD_FUNC_RET(GetRadar()->GetNearbyEntities());
	IEntitySystem *pEntitySystem = gEnv->pEntitySystem;

	for(int iEntity=0; iEntity<entitiesInProximity->size(); ++iEntity)
	{
		IEntity *pEntity = pEntitySystem->GetEntity((*entitiesInProximity)[iEntity]);
		if(!pEntity)
			continue;

		// Check for target validity
		if (!IsValidAssistTarget(pEntity, pSelf))
			continue;

		IPhysicalEntity *pPhysics=pEntity->GetPhysics();

		if (!pPhysics)
			continue;

		Vec3 target=pEntity->GetWorldPos();
		pe_status_dynamics dyn;
		if (pPhysics->GetStatus(&dyn))
			target=dyn.centerOfMass;
				
		if (target.GetSquaredDistance(origo) < maxDistanceSqr &&
			camera.IsPointVisible(target))
		{
			float dst=LinePointDistanceSqr(aim, target, g_pGameCVars->aim_assistVerticalScale);
			
			if (dst<bestSnapSqr)
			{
				// Check if can be hit directly after auto aim
				ray_hit chkhit;
				if (gEnv->pPhysicalWorld->RayWorldIntersection(info.eyePosition, (target-info.eyePosition)*500.0f, ent_all, (13&rwi_pierceability_mask), &chkhit, 1, &pSelfPhysics, 1))
				{
					IEntity *pFound=gEnv->pEntitySystem->GetEntityFromPhysics(chkhit.pCollider);
					if (pFound != pEntity)
						continue;
				}
				else
				{
					continue;
				}

				res=pEntity;
				bestSnapSqr=dst;
				bestTarget=target;
			}
		}
	}

	if (res)
	{
		CMovementRequest req;
		Vec3 v0=info.eyeDirection;
		Vec3 v1=bestTarget-info.eyePosition;
		v0.Normalize();
		v1.Normalize();

		Matrix34 mInvWorld = pSelf->GetWorldTM();
		mInvWorld.Invert();
		//Matrix34 mInvWorld = pSelf->GetLocalTM();

		v0 = mInvWorld.TransformVector( v0 );
		v1 = mInvWorld.TransformVector( v1 );

		Ang3 deltaR=Ang3(Quat::CreateRotationV0V1(v0, v1));
		float scale=1.0f;
		
		if (!accurate)
		{
			IFireMode *pFireMode = GetFireMode(GetCurrentFireMode());
			if (!pFireMode)
				return;

			string modetype (pFireMode->GetType());
			if (modetype.empty())
				return;

			float assistMod=1.0f;

			if (!modetype.compareNoCase("Automatic")||
				!modetype.compareNoCase("Beam") ||
				!modetype.compareNoCase("Burst") ||
				!modetype.compareNoCase("Rapid"))
				assistMod=g_pGameCVars->aim_assistAutoCoeff;
			else
				assistMod=g_pGameCVars->aim_assistSingleCoeff;
				
			scale=sqr(1.0f-bestSnapSqr/maxSnapSqr)*assistMod;
			//GetISystem()->GetILog()->Log("AutoAim mod: %f at mode %s", assistMod, modetype.c_str());
		}

		//GetISystem()->GetILog()->Log("AutoAim scale: %f", scale);
		req.AddDeltaRotation(deltaR*scale);
		pMC->RequestMovement(req);
	}
}

bool CWeapon::IsValidAssistTarget(IEntity *pEntity, IEntity *pSelf,bool includeVehicles/*=false*/)
{
	if(!pEntity)
		return false;

	IActor *pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());
	IAIObject *pAI = pEntity->GetAI();

	if(!pActor && includeVehicles && pAI)
	{
		IVehicle *pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(pEntity->GetId()); 
		if (pVehicle && pVehicle->GetStatus().health > 0.f && pAI->IsHostile(pSelf->GetAI(),false))
			return true;
	}

	// Check for target validity
	if(!pActor)
		return false;

	if (!pAI)
	{
		if (pActor->IsPlayer() && pEntity != pSelf && !pEntity->IsHidden() &&
				pActor->GetHealth() > 0)
		{
			int ownteam=g_pGame->GetGameRules()->GetTeam(pSelf->GetId());
			int targetteam=g_pGame->GetGameRules()->GetTeam(pEntity->GetId());
			
			// Assist aiming on non-allied players only
			return (targetteam == 0 && ownteam == 0) || (targetteam != 0 && targetteam != ownteam);
		}
		else
		{
			return false;
		}
	}
	
	return (pEntity != pSelf &&!pEntity->IsHidden() && 
		pActor->GetHealth() > 0 &&	pAI->GetAIType() != AIOBJECT_VEHICLE &&
		pAI->IsHostile(pSelf->GetAI(),false));
}

//-----------------------------------------------------------------------
//Slightly modified version on AssistAiming
void CWeapon::AdvancedAssistAiming(float range, const Vec3& pos, Vec3 &dir)
{
	IEntity *res=NULL;

	IActor *pSelfActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pSelfActor)
		return;

	IEntity *pSelf=pSelfActor->GetEntity();
	if (GetOwner() != pSelf)
		return;

	// If already having a valid target, don't do anything
	ray_hit hit;
	IPhysicalEntity* pSkipEnts[10];
	int nSkip = CSingle::GetSkipEntities(this, pSkipEnts, 10);	
	if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, dir*500.0f, ent_all, (13&rwi_pierceability_mask), &hit, 1, pSkipEnts, nSkip))
	{
		if (!hit.bTerrain)
		{
			if (IsValidAssistTarget(gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider), pSelf, true))
				return;
		}
	}

	const CCamera &camera=GetISystem()->GetViewCamera();
	Vec3 origo=camera.GetPosition();

	// Some other ray may be used too, like weapon aiming or firing ray 
	Line aim=Line(pos,dir);

	float bestSnapSqr=range*range;
	// PARAMETER: maximum range at which autotargetting works
	float maxDistanceSqr=g_pGameCVars->aim_assistMaxDistance;
	maxDistanceSqr*=maxDistanceSqr;
	Vec3 bestTarget;

	CHUDRadar *pHUDRadar = SAFE_HUD_FUNC_RET(GetRadar());
	if(!pHUDRadar)
		return;

	const std::vector<EntityId> *entitiesInProximity = pHUDRadar->GetNearbyEntities();

	for(int iEntity=0; iEntity<entitiesInProximity->size(); ++iEntity)
	{
		IEntity *pEntity = m_pEntitySystem->GetEntity((*entitiesInProximity)[iEntity]);
		if(!pEntity)
			continue;

		// Check for target validity
		if (!IsValidAssistTarget(pEntity, pSelf, true))
			continue;

		IPhysicalEntity *pPhysics=pEntity->GetPhysics();

		if (!pPhysics)
			continue;

		Vec3 target=pEntity->GetWorldPos();
		pe_status_dynamics dyn;
		if (pPhysics->GetStatus(&dyn))
			target=dyn.centerOfMass;

		if (target.GetSquaredDistance(origo) < maxDistanceSqr &&
			camera.IsPointVisible(target))
		{
			float dst=LinePointDistanceSqr(aim, target, g_pGameCVars->aim_assistVerticalScale);

			if (dst<bestSnapSqr)
			{
				res=pEntity;
				bestSnapSqr=dst;
				bestTarget=target;
			}
		}
	}

	//Correct direction to hit the target
	if (res)
	{
		dir = bestTarget-pos;
		dir.Normalize();
	}
}

//------------------------------------------------------------------------
void CWeapon::RestartZoom(bool force)
{
	if(m_restartZoom || force)
	{
		if(m_zm && !IsBusy() && m_zm->CanZoom())
		{
			m_zm->StartZoom(true, false, m_restartZoomStep);

			m_restartZoom = false;

			if(m_zoomViewMode < m_viewModeList.size())
			{
				const ItemString& curMode = m_viewModeList[m_zoomViewMode];
				if(!curMode.empty())
				{
					gEnv->p3DEngine->SetPostEffectParam(curMode, 1.0f);
					m_currentViewMode = m_zoomViewMode;
					m_zoomViewMode = 0;
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::MountAt(const Vec3 &pos)
{
	CItem::MountAt(pos);

	AIObjectParameters params;
	GetEntity()->RegisterInAISystem(AIOBJECT_MOUNTEDWEAPON, params);
}

//------------------------------------------------------------------------
void CWeapon::MountAtEntity(EntityId entityId, const Vec3 &pos, const Ang3 &angles)
{
	CItem::MountAtEntity(entityId, pos, angles);

	if (gEnv->bServer && !m_bonusammo.empty())
	{
		for (TAmmoMap::iterator it=m_bonusammo.begin(); it!=m_bonusammo.end(); ++it)
		{
			SetInventoryAmmoCount(it->first, GetInventoryAmmoCount(it->first)+it->second);
		}

		m_bonusammo.clear();
	}
}

//------------------------------------------------------------------------
void CWeapon::Reload(bool force)
{
	CActor *pOwner = GetOwnerActor();
	bool isClient = false;

	if(pOwner)
	{ 
		isClient = pOwner->IsClient();
		if(pOwner->IsPlayer())
		{
			if(IItem *pItem = pOwner->GetItemByClass(CItem::sOffHandClass))
			{
				COffHand *pOffHand = static_cast<COffHand*> (pItem);
				CPlayer *pPlayer = static_cast<CPlayer*> (pOwner);
				if(pOffHand->GetOffHandState()!=eOHS_INIT_STATE)
					return;
			}

			//Player can not reload while sprinting
			if(pOwner->IsSprinting())
				return;
		}
		else
		{
			if(IsDualWieldMaster())
				ReloadSlave();
		}
	}

	if (m_fm && (m_fm->CanReload() || force))
	{
		if (g_pGameCVars->bt_end_reload && isClient)
			g_pGame->GetBulletTime()->Activate(false);

		if (m_zm)
			m_fm->Reload(m_zm->GetCurrentStep());
		else
			m_fm->Reload(0);


		if (!pOwner || pOwner->IsClient())
			RequestReload();
	}
}

//------------------------------------------------------------------------
bool CWeapon::CanReload() const
{
	if (m_fm)
		return m_fm->CanReload();
	return true;
}

//------------------------------------------------------------------------
bool CWeapon::OutOfAmmo(bool allFireModes) const
{
	if (!allFireModes)
		return m_fm && m_fm->OutOfAmmo();

	for (int i=0; i<m_firemodes.size(); i++)
		if (!m_firemodes[i]->OutOfAmmo())
			return false;

	return true;
}

//------------------------------------------------------------------------
float CWeapon::GetDamage(int fmId, float distance) const
{
	if (fmId<0)
		fmId=GetCurrentFireMode();
	if (fmId>=m_firemodes.size())
		return 0.0f;

	return m_firemodes[fmId]->GetDamage(distance);
}

//------------------------------------------------------------------------
const char *CWeapon::GetDamageType(int fmId) const
{
	if (fmId<0)
		fmId=GetCurrentFireMode();
	if (fmId>=m_firemodes.size())
		return "";

	return m_firemodes[fmId]->GetDamageType();
}

//------------------------------------------------------------------------
int CWeapon::GetAmmoCount(IEntityClass* pAmmoType) const
{
	TAmmoMap::const_iterator it = m_ammo.find(pAmmoType);
	if (it != m_ammo.end())
		return it->second;
	return 0;
}

//------------------------------------------------------------------------
void CWeapon::SetAmmoCount(IEntityClass* pAmmoType, int count)
{
	TAmmoMap::iterator it = m_ammo.find(pAmmoType);
	if (it != m_ammo.end())
		it->second=count;
	else
		m_ammo.insert(TAmmoMap::value_type(pAmmoType, count));

	GetGameObject()->ChangedNetworkState(ASPECT_AMMO);
}

//------------------------------------------------------------------------
int CWeapon::GetInventoryAmmoCount(IEntityClass* pAmmoType) const
{
	if (m_hostId)
	{
		IVehicle *pVehicle=m_pGameFramework->GetIVehicleSystem()->GetVehicle(m_hostId);
		if (pVehicle)
			return pVehicle->GetAmmoCount(pAmmoType);

		return 0;
	}

	IInventory *pInventory=GetActorInventory(GetOwnerActor());
	if (!pInventory)
		return 0;

	return pInventory->GetAmmoCount(pAmmoType);
}

//------------------------------------------------------------------------
void CWeapon::SetInventoryAmmoCount(IEntityClass* pAmmoType, int count)
{
	if(!pAmmoType)
		return;
	if (m_hostId)
	{
		IVehicle *pVehicle=m_pGameFramework->GetIVehicleSystem()->GetVehicle(m_hostId);
		if (pVehicle)
			pVehicle->SetAmmoCount(pAmmoType, count);
		
		return;
	}

	IInventory *pInventory=GetActorInventory(GetOwnerActor());
	if (!pInventory)
		return;

	int capacity = pInventory->GetAmmoCapacity(pAmmoType);
	int current = pInventory->GetAmmoCount(pAmmoType);
	if((!gEnv->pSystem->IsEditor()) && (count > capacity))
	{
		if(GetOwnerActor()->IsClient())
		{
			CryFixedStringT<64> ammoName = "@";
			if(!strcmp(GetEntity()->GetClass()->GetName(), "CustomAmmoPickup"))
			{
				SmartScriptTable props;
				if (GetEntity()->GetScriptTable() && GetEntity()->GetScriptTable()->GetValue("Properties", props))
				{
					const char *name = NULL;
					props->GetValue("AmmoName", name);
					ammoName.append(name);
				}
				SAFE_HUD_FUNC(DisplayFlashMessage("@ammo_maxed_out", 2, ColorF(1.0f, 0,0), true, ammoName.c_str()));
			}
			else
			{
				ammoName.append(GetEntity()->GetClass()->GetName());
				SAFE_HUD_FUNC(DisplayFlashMessage("@weapon_ammo_full", 2, ColorF(1.0f, 0,0), true, ammoName.c_str()));
			}
		}
		
		//If still there's some place, full inventory to maximum...
		if(current<capacity)
		{
			pInventory->SetAmmoCount(pAmmoType,capacity);
			if(GetOwnerActor()->IsClient() && capacity - current > 0)
			{
				/*char buffer[5];
				itoa(capacity - current, buffer, 10);
				SAFE_HUD_FUNC(DisplayFlashMessage("@grab_ammo", 3, Col_Wheat, true, (string("@")+pAmmoType->GetName()).c_str(), buffer));*/
				SAFE_HUD_FUNC(DisplayAmmoPickup(pAmmoType->GetName(), capacity - current));
			}
			if (IsServer())
				GetOwnerActor()->GetGameObject()->InvokeRMI(CActor::ClSetAmmo(), CActor::AmmoParams(pAmmoType->GetName(), count), eRMI_ToRemoteClients);
		}
	}
	else
	{
		pInventory->SetAmmoCount(pAmmoType, count);
		if(GetOwnerActor()->IsClient() && count - current > 0)
		{
			/*char buffer[5];
			itoa(count - current, buffer, 10);
			SAFE_HUD_FUNC(DisplayFlashMessage("@grab_ammo", 3, Col_Wheat, true, (string("@")+pAmmoType->GetName()).c_str(), buffer));*/
			SAFE_HUD_FUNC(DisplayAmmoPickup(pAmmoType->GetName(), count - current));
		}
		if (IsServer())
			GetOwnerActor()->GetGameObject()->InvokeRMI(CActor::ClSetAmmo(), CActor::AmmoParams(pAmmoType->GetName(), count), eRMI_ToRemoteClients);
	}
}

//------------------------------------------------------------------------
IFireMode *CWeapon::GetFireMode(int idx) const
{
	if (idx >= 0 && idx < m_firemodes.size())
		return m_firemodes[idx];
	return 0;
}

//------------------------------------------------------------------------
IFireMode *CWeapon::GetFireMode(const char *name) const
{
	TFireModeIdMap::const_iterator it = m_fmIds.find(CONST_TEMP_STRING(name));
	if (it == m_fmIds.end())
		return 0;

	return GetFireMode(it->second);
}

//------------------------------------------------------------------------
int CWeapon::GetFireModeIdx(const char *name) const
{
	TFireModeIdMap::const_iterator it = m_fmIds.find(CONST_TEMP_STRING(name));
	if (it != m_fmIds.end())
		return it->second;
	return -1;
}

//------------------------------------------------------------------------
int CWeapon::GetCurrentFireMode() const
{
	return GetGameObject()->GetAspectProfile(ASPECT_FIREMODE);
}

//------------------------------------------------------------------------
void CWeapon::SetCurrentFireMode(int idx)
{
	if (m_firemodes.empty())
		return;

	GetGameObject()->SetAspectProfile(ASPECT_FIREMODE, idx);
}

//------------------------------------------------------------------------
bool CWeapon::SetAspectProfile( EEntityAspects aspect, uint8 profile )
{
	if (aspect == ASPECT_FIREMODE)
	{
		if (m_fm)
			m_fm->Activate(false);

		if (profile >= m_firemodes.size())
			m_fm = 0;
		else
			m_fm = m_firemodes[profile];

		if (m_fm)
		{
			m_fm->Activate(true);

			if (IsServer() && GetOwnerId())
				m_pGameplayRecorder->Event(GetOwner(), GameplayEvent(eGE_WeaponFireModeChanged, m_fm->GetName(), profile, (void *)GetEntityId()));
		}

		return true;
	}

	return CItem::SetAspectProfile(aspect, profile);
}

//------------------------------------------------------------------------
uint8 CWeapon::GetDefaultProfile( EEntityAspects aspect )
{
	if (aspect == ASPECT_FIREMODE)
	{
		return MaxProfilesPerAspect-1;
	}
	else
		return CItem::GetDefaultProfile(aspect);
}

//------------------------------------------------------------------------
void CWeapon::SetCurrentFireMode(const char *name)
{
	TFireModeIdMap::iterator it = m_fmIds.find(CONST_TEMP_STRING(name));
	if (it == m_fmIds.end())
		return;

	SetCurrentFireMode(it->second);
}

//------------------------------------------------------------------------
void CWeapon::ChangeFireMode()
{
	int newId = GetNextFireMode(GetCurrentFireMode());

	if (newId != GetCurrentFireMode())
		RequestFireMode(newId);
}

//------------------------------------------------------------------------
int CWeapon::GetNextFireMode(int currMode) const
{
	if (m_firemodes.empty() || (currMode > (m_firemodes.size()-1)))
		return 0;

	int t = currMode;
	do {
		t++;
		if (t == m_firemodes.size())
			t = 0;
		if (IFireMode* pFM = GetFireMode(t))
			if(pFM->IsEnabled())
				return t;
	} while(t!=currMode);

	return t;
}

//------------------------------------------------------------------------
void CWeapon::EnableFireMode(int idx, bool enable)
{
	IFireMode *pFireMode = GetFireMode(idx);
	if (pFireMode)
		pFireMode->Enable(enable);
}

//------------------------------------------------------------------------
IZoomMode *CWeapon::GetZoomMode(int idx) const
{
	if (idx >= 0 && idx < m_zoommodes.size())
		return m_zoommodes[idx];
	return 0;
}

//------------------------------------------------------------------------
IZoomMode *CWeapon::GetZoomMode(const char *name) const
{
	TZoomModeIdMap::const_iterator it = m_zmIds.find(CONST_TEMP_STRING(name));
	if (it == m_zmIds.end())
		return 0;

	return GetZoomMode(it->second);
}

//------------------------------------------------------------------------
int CWeapon::GetZoomModeIdx(const char *name) const
{
	TZoomModeIdMap::const_iterator it = m_zmIds.find(CONST_TEMP_STRING(name));
	if (it != m_zmIds.end())
		return it->second;
	return -1;
}

//------------------------------------------------------------------------
int CWeapon::GetCurrentZoomMode() const
{
	return m_zmId;
}

//------------------------------------------------------------------------
void CWeapon::SetCurrentZoomMode(int idx)
{
	if (m_zoommodes.empty())
		return;

	m_zm = m_zoommodes[idx];
	m_zmId = idx;
	m_zm->Activate(true);
}

//------------------------------------------------------------------------
void CWeapon::SetCurrentZoomMode(const char *name)
{
	TZoomModeIdMap::iterator it = m_zmIds.find(CONST_TEMP_STRING(name));
	if (it == m_zmIds.end())
		return;

	SetCurrentZoomMode(it->second);
}

//------------------------------------------------------------------------
void CWeapon::ChangeZoomMode()
{
	if (m_zoommodes.empty())
		return;
/*
	if (m_zmId+1<m_zoommodes.size())
		SetCurrentZoomMode(m_zmId+1);
	else if (m_zoommodes.size()>1)
		SetCurrentZoomMode(0);
		*/
	int t = m_zmId;
	do {
		t++;
		if (t == m_zoommodes.size())
			t = 0;
		if (GetZoomMode(t)->IsEnabled())
			SetCurrentZoomMode(t);
	} while(t!=m_zmId);
}

//------------------------------------------------------------------------
void CWeapon::EnableZoomMode(int idx, bool enable)
{
	IZoomMode *pZoomMode = GetZoomMode(idx);
	if (pZoomMode)
		pZoomMode->Enable(enable);
}

//------------------------------------------------------------------------
bool CWeapon::IsServerSpawn(IEntityClass* pAmmoType) const
{
	return g_pGame->GetWeaponSystem()->IsServerSpawn(pAmmoType);
}

//------------------------------------------------------------------------
CProjectile *CWeapon::SpawnAmmo(IEntityClass* pAmmoType, bool remote)
{
	if(gEnv->bServer && g_pGame->GetGameRules())
	{
		if(CBattleDust* pBD = g_pGame->GetGameRules()->GetBattleDust())
		{
			pBD->RecordEvent(eBDET_ShotFired, GetEntity()->GetWorldPos(), GetEntity()->GetClass());
		}
	}

	return g_pGame->GetWeaponSystem()->SpawnAmmo(pAmmoType, remote);
}

//------------------------------------------------------------------------
void CWeapon::SetCrosshairVisibility(bool visible)
{
	m_crosshairstats.visible = visible;
}

//------------------------------------------------------------------------
bool CWeapon::GetCrosshairVisibility() const
{
	return m_crosshairstats.visible;
}

//------------------------------------------------------------------------
void CWeapon::SetCrosshairOpacity(float opacity)
{
	m_crosshairstats.opacity = opacity; 
}

//------------------------------------------------------------------------
float CWeapon::GetCrosshairOpacity() const
{
	return m_crosshairstats.opacity;
}

//------------------------------------------------------------------------
void CWeapon::FadeCrosshair(float from, float to, float time)
{
	m_crosshairstats.fading = true;
	m_crosshairstats.fadefrom = from;
	m_crosshairstats.fadeto = to;
	m_crosshairstats.fadetime = MAX(0, time);
	m_crosshairstats.fadetimer = m_crosshairstats.fadetime;

	SetCrosshairOpacity(from);
}

//------------------------------------------------------------------------
void CWeapon::UpdateCrosshair(float frameTime)
{
	if (m_crosshairstats.fading)
	{
		if (m_crosshairstats.fadetimer>0.0f)
		{
			m_crosshairstats.fadetimer -= frameTime;
			if (m_crosshairstats.fadetimer<0.0f)
				m_crosshairstats.fadetimer=0.0f;

			float t = (m_crosshairstats.fadetime-m_crosshairstats.fadetimer)/m_crosshairstats.fadetime;
			float d = (m_crosshairstats.fadeto-m_crosshairstats.fadefrom);

			if (t >= 1.0f)
				m_crosshairstats.fading = false;

			if (d < 0.0f)
				t = 1.0f-t;

			if(m_crosshairstats.fadefrom == m_crosshairstats.fadeto)
				m_crosshairstats.opacity = m_crosshairstats.fadeto;
			else
				m_crosshairstats.opacity = fabsf(t*d);
		}
		else
		{
			m_crosshairstats.opacity = m_crosshairstats.fadeto;
			m_crosshairstats.fading = false;
		}
	}

	bool bUpdateHUD = IsSelected() && GetOwnerActor() && GetOwnerActor()->IsClient();

	if(bUpdateHUD)
	{
		SAFE_HUD_FUNC(GetCrosshair()->SetOpacity(m_crosshairstats.opacity));
	}

	if(m_restartZoom)
		RestartZoom();

	if(bUpdateHUD && !IsDualWieldSlave())
	{
		SAFE_HUD_FUNC(SetFireMode(this, m_fm));
	}
}

//------------------------------------------------------------------------
void CWeapon::AccessoriesChanged()
{
	int i=0;
	for (TFireModeVector::iterator it=m_firemodes.begin(); it!=m_firemodes.end(); ++it)
	{
		for (TFireModeIdMap::iterator iit=m_fmIds.begin(); iit!=m_fmIds.end(); ++iit)
		{
			if (iit->second == i)
			{
				IFireMode *pFireMode = *it;
				pFireMode->ResetParams(0);
				if (m_fmDefaults)
					pFireMode->PatchParams(m_fmDefaults);
				PatchFireModeWithAccessory(pFireMode, "default");

				const IItemParamsNode *fmparams = GetFireModeParams(iit->first.c_str());
				if (fmparams)
					pFireMode->PatchParams(fmparams);

				PatchFireModeWithAccessory(pFireMode, iit->first.c_str());
				break;
			}
		}

		++i;
	}

	i=0;
	for (TZoomModeVector::iterator it=m_zoommodes.begin(); it!=m_zoommodes.end(); ++it)
	{
		for (TZoomModeIdMap::iterator iit=m_zmIds.begin(); iit!=m_zmIds.end(); ++iit)
		{
			if (iit->second == i)
			{
				IZoomMode *pZoomMode = *it;
				pZoomMode->ResetParams(0);
				if (m_zmDefaults)
					pZoomMode->PatchParams(m_zmDefaults);
				PatchZoomModeWithAccessory(pZoomMode, "default");

				const IItemParamsNode *zmparams = GetZoomModeParams(iit->first.c_str());
				if (zmparams)
					pZoomMode->PatchParams(zmparams);

				PatchZoomModeWithAccessory(pZoomMode, iit->first.c_str());
				break;
			}
		}
		++i;
	}

	//Second SOCOM
	bool isDualWield = IsDualWieldMaster();
	CWeapon *dualWield = 0;

	if (isDualWield)
	{
		IItem *slave = GetDualWieldSlave();
		if (slave && slave->GetIWeapon())
			dualWield = static_cast<CWeapon *>(slave);
		else
			isDualWield = false;
	}

	if(isDualWield)
		dualWield->AccessoriesChanged();

	if(IActor *pOwner = GetOwnerActor())
		if(pOwner->IsClient())
			SAFE_HUD_FUNC(UpdateCrosshair());
}

//------------------------------------------------------------------------
void CWeapon::PatchFireModeWithAccessory(IFireMode *pFireMode, const char *firemodeName)
{
	if(!g_pItemStrings)
		return;

	bool silencerAttached = false;
	bool lamAttached = false;
	m_lamID = 0;

	// patch defaults with accessory defaults
	for (TAccessoryMap::iterator ait=m_accessories.begin(); ait!=m_accessories.end(); ++ait)
	{

		//Attach silencer (and LAM)
		if (ait->first == g_pItemStrings->Silencer || ait->first == g_pItemStrings->SOCOMSilencer)
			silencerAttached = true;

		SAccessoryParams *params=GetAccessoryParams(ait->first);
		if (params && params->params)
		{
			const IItemParamsNode *firemodes = params->params->GetChild("firemodes");
			if (!firemodes)
				continue;

			int n=firemodes->GetChildCount();
			for (int i=0; i<n; i++)
			{
				const IItemParamsNode *firemode = firemodes->GetChild(i);
				const char *name=firemode->GetAttribute("name");
				if (name && !stricmp(name, firemodeName))
				{
					pFireMode->PatchParams(firemode);
					break;
				}
				const char *typ=firemode->GetAttribute("type");
				if (typ && !stricmp(typ, firemodeName))
				{
					pFireMode->PatchParams(firemode);
					break;
				}
			}
		}
	}
	//Fix muzzleflash reseting problem when attaching silencer
	pFireMode->Activate(false);
	pFireMode->Activate(true);

	//Is silencer?
	m_silencerAttached = silencerAttached;
}

//------------------------------------------------------------------------
void CWeapon::PatchZoomModeWithAccessory(IZoomMode *pZoomMode, const char *zoommodeName)
{
	// patch defaults with accessory defaults
	for (TAccessoryMap::iterator ait=m_accessories.begin(); ait!=m_accessories.end(); ++ait)
	{
		SAccessoryParams *params=GetAccessoryParams(ait->first);
		if (params && params->params)
		{
			const IItemParamsNode *zoommodes = params->params->GetChild("zoommodes");
			if (!zoommodes)
				continue;

			int n=zoommodes->GetChildCount();
			for (int i=0; i<n; i++)
			{
				const IItemParamsNode *zoommode = zoommodes->GetChild(i);
				const char *name=zoommode->GetAttribute("name");
				if (name && !stricmp(name, zoommodeName))
				{
					pZoomMode->PatchParams(zoommode);
					break;
				}
				const char *typ=zoommode->GetAttribute("type");
				if (typ && !stricmp(typ, zoommodeName))
				{
					pZoomMode->PatchParams(zoommode);
					break;
				}
			}
		}
	}
}

//------------------------------------------------------------------------
float CWeapon::GetSpinUpTime() const
{
	if (m_fm)
		return m_fm->GetSpinUpTime();
	return 0.0f;
}

//------------------------------------------------------------------------
float CWeapon::GetSpinDownTime() const
{
	if (m_fm)
		return m_fm->GetSpinDownTime();
	return 0.0f;
}

//------------------------------------------------------------------------
void CWeapon::SetHostId(EntityId hostId)
{
	m_hostId=hostId;
}

//------------------------------------------------------------------------
EntityId CWeapon::GetHostId() const
{
	return m_hostId;
}

//------------------------------------------------------------------------
void CWeapon::FixAccessories(SAccessoryParams *params, bool attach)
{
	if (!attach)
	{
		if (params)
		{
			for (int i = 0; i < params->firemodes.size(); i++)
			{
				if (params->exclusive && GetFireModeIdx(params->firemodes[i]) != -1)
				{
					EnableFireMode(GetFireModeIdx(params->firemodes[i]), false);
				}
			}
			if (IFireMode * pFM = GetFireMode(GetCurrentFireMode()))
			{
				if(!pFM->IsEnabled())
					ChangeFireMode();
			}
			
			if (GetZoomModeIdx(params->zoommode) != -1)
			{
				EnableZoomMode(GetZoomModeIdx(params->zoommode), false);
				ChangeZoomMode();
			}
		}
	}
	else if(params)
	{
		if (!params->switchToFireMode.empty())
			SetCurrentFireMode(params->switchToFireMode.c_str());

		for (int i = 0; i < params->firemodes.size(); i++)
		{
			if (GetFireModeIdx(params->firemodes[i]) != -1)
			{
				GetFireMode(params->firemodes[i].c_str())->Enable(true);
			}
		}
		if (GetZoomModeIdx(params->zoommode) != -1)
		{
			EnableZoomMode(GetZoomModeIdx(params->zoommode), true);
			SetCurrentZoomMode(GetZoomModeIdx(params->zoommode));
		}
	}	
}

//------------------------------------------------------------------------
void CWeapon::SetDestinationEntity(EntityId targetId)
{
  // default: Set bbox center as destination
  IEntity* pEntity = gEnv->pEntitySystem->GetEntity(targetId);

  if (pEntity)
  {
    AABB box;
    pEntity->GetWorldBounds(box);
    
    SetDestination(box.GetCenter());
  }
}

//------------------------------------------------------------------------
bool CWeapon::PredictProjectileHit(IPhysicalEntity *pShooter, const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speed,
																	 Vec3& predictedPosOut, float& projectileSpeedOut,
																	 Vec3* pTrajectory, unsigned int* trajectorySizeInOut) const
{
	IFireMode *pFireMode = GetFireMode(GetCurrentFireMode());
	if (!pFireMode)
		return false;

	IEntityClass* pAmmoType = pFireMode->GetAmmoType();
	if (!pAmmoType)
		return false;

	CProjectile* pTestProjectile = g_pGame->GetWeaponSystem()->SpawnAmmo(pAmmoType);
	if (!pTestProjectile)
		return false;
	IPhysicalEntity* pProjectilePhysEntity = pTestProjectile->GetEntity()->GetPhysics();
	if (!pProjectilePhysEntity)
		return false;


	projectileSpeedOut = pTestProjectile->GetSpeed();

	pTestProjectile->SetVelocity(pos, dir, velocity, speed/projectileSpeedOut);

	pe_params_flags particleFlags;
	particleFlags.flagsAND = ~(pef_log_collisions & pef_traceable & pef_log_poststep);
	pProjectilePhysEntity->SetParams(&particleFlags);

	pe_params_particle partPar;
	partPar.pColliderToIgnore = pShooter;
	pProjectilePhysEntity->SetParams(&partPar);

	pe_params_pos paramsPos;
	paramsPos.iSimClass = 6;
	paramsPos.pos = pos;
	pProjectilePhysEntity->SetParams(&paramsPos);

	unsigned int n = 0;
	const unsigned int maxSize = trajectorySizeInOut ? *trajectorySizeInOut : 0;

	if (pTrajectory && n < maxSize)
		pTrajectory[n++] = pos;

	const float	dt = 0.12f;
	const float	lifeTime = pTestProjectile->GetLifeTime();

	int stationary = 0;
	Vec3 lastPos = pos;

	for (float t = 0.0f; t < lifeTime; t += dt)
	{
		pProjectilePhysEntity->DoStep(dt);

		pe_status_pos	statusPos;
		pProjectilePhysEntity->GetStatus(&statusPos);
		float distSq = Distance::Point_PointSq(lastPos, statusPos.pos);
		lastPos = statusPos.pos;

		// Early out when almost stationary.
		if (distSq < sqr(0.01f))
			stationary++;
		else
			stationary = 0;
		if (stationary > 2)
			break;

		if (pTrajectory && n < maxSize)
		{
			pe_status_pos	statusPos;
			pProjectilePhysEntity->GetStatus(&statusPos);
			pTrajectory[n++] = statusPos.pos;
		}
	}

	if (trajectorySizeInOut)
		*trajectorySizeInOut = n;

	pe_status_pos	statusPos;
	pProjectilePhysEntity->GetStatus(&statusPos);
	gEnv->pEntitySystem->RemoveEntity(pTestProjectile->GetEntity()->GetId(), true);

	predictedPosOut = statusPos.pos;

	return true;
}


//------------------------------------------------------------------------
const AIWeaponDescriptor& CWeapon::GetAIWeaponDescriptor( ) const
{
	return m_aiWeaponDescriptor;
}

//------------------------------------------------------------------------
void CWeapon::OnHit(float damage, const char* damageType)
{ 
  CItem::OnHit(damage,damageType);
}

//------------------------------------------------------------------------
void CWeapon::OnDestroyed()
{ 
  CItem::OnDestroyed();

  if (m_fm)
  {
    if (m_fm->IsFiring())
      m_fm->StopFire();
  }
}

bool CWeapon::HasAttachmentAtHelper(const char *helper)
{
	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if(pPlayer)
	{
		IInventory *pInventory = pPlayer->GetInventory();
		if(pInventory)
		{
			for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); it++)
			{
				SAccessoryParams *params = GetAccessoryParams(it->first.c_str());
				if (params && !strcmp(params->attach_helper.c_str(), helper))
				{	
					// found a child item that can be used
					return true;
				}
			}
			for (int i = 0; i < pInventory->GetCount(); i++)
			{
				EntityId id = pInventory->GetItem(i);
				IItem *cur = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(id);

				if (cur)
				{
					SAccessoryParams *invAccessory = GetAccessoryParams(cur->GetEntity()->GetClass()->GetName());
					if (invAccessory && !strcmp(invAccessory->attach_helper.c_str(), helper))
					{
						// found an accessory in the inventory that can be used
						return true;
					}
				}
			}


		}
	}
	return false;
}

void CWeapon::GetAttachmentsAtHelper(const char *helper, std::vector<string> &rAttachments)
{
	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if(pPlayer)
	{
		IInventory *pInventory = pPlayer->GetInventory();
		if(pInventory)
		{
			rAttachments.clear();
			for (int i = 0; i < pInventory->GetCount(); i++)
			{
				EntityId id = pInventory->GetItem(i);
				IItem *cur = gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(id);

				if (cur)
				{
					SAccessoryParams *invAccessory = GetAccessoryParams(cur->GetEntity()->GetClass()->GetName());
					if (invAccessory && !strcmp(invAccessory->attach_helper.c_str(), helper))
					{
						rAttachments.push_back(cur->GetEntity()->GetClass()->GetName());
					}
				}
			}
		}
	}
}

void CWeapon::IncrementViewmode()
{
	string oldMode(m_viewModeList[m_currentViewMode].c_str());
	bool oldNormal = m_currentViewMode == m_defaultViewMode;
	m_currentViewMode++;
	if (m_currentViewMode > (m_viewModeList.size() - 1))
		m_currentViewMode = 0;
	string newMode(m_viewModeList[m_currentViewMode].c_str());
	bool newNormal = m_currentViewMode == m_defaultViewMode;

	// deactivate old mode
	if (!oldNormal)
		gEnv->p3DEngine->SetPostEffectParam(oldMode.c_str(), 0.0f);

	// activate new mode
	if (!newNormal)
		gEnv->p3DEngine->SetPostEffectParam(newMode.c_str(), 1.0f);
}

void CWeapon::ExitViewmodes()
{
	if (!m_useViewMode)
		return;

	// deactivate old mode
	if (!m_currentViewMode == m_defaultViewMode)
		gEnv->p3DEngine->SetPostEffectParam(m_viewModeList[m_currentViewMode].c_str(), 0.0f);

	m_currentViewMode = m_defaultViewMode;
}

void CWeapon::EnterWater(bool enter)
{
}

struct CWeapon::EndChangeFireModeAction
{
	EndChangeFireModeAction(CWeapon *_weapon): weapon(_weapon){};
	CWeapon *weapon;

	void execute(CItem *_this)
	{
		weapon->EndChangeFireMode();
	}
};

struct CWeapon::PlayLeverLayer
{
	PlayLeverLayer(CWeapon *_weapon, bool _activate): weapon(_weapon), activate(_activate){};
	CWeapon *weapon;
	bool activate;

	void execute(CItem *_this)
	{
		if(activate)
		{
			weapon->StopLayer(g_pItemStrings->lever_layer_2);
			weapon->PlayLayer(g_pItemStrings->lever_layer_1);
		}
		else
		{
			weapon->StopLayer(g_pItemStrings->lever_layer_1);
			weapon->PlayLayer(g_pItemStrings->lever_layer_2);
		}
	}
};

void CWeapon::StartChangeFireMode()
{
	if(IsBusy() || IsZooming() || (m_fm && m_fm->IsReloading()) || m_modifying)
		return;

	//Check left socom
	IItem *slave = GetDualWieldSlave();
	CWeapon *pSlave = NULL;
	if (slave && slave->GetIWeapon())
	{
		//Do not start if slave can not change firemode too
		pSlave = static_cast<CWeapon *>(slave);
		if(pSlave->IsBusy() || pSlave->IsReloading())
			return;
	}

	//Check if the weapon has enough firemodes (Melee is always there)
	if(m_fmIds.size()<=2 || GetEntity()->GetClass() == CItem::sAlienMountClass)
	{
		if(m_ownerId == LOCAL_PLAYER_ENTITY_ID)
		{
			if(GetEntity()->GetClass() == CItem::sFistsClass)
			{
				SAFE_HUD_FUNC(FireModeSwitch(true));
			}
			else
			{
				SAFE_HUD_FUNC(FireModeSwitch());
			}
		}

		return;
	}

	int zoomed = 0;

	if(m_zm)
		zoomed = m_zm->GetCurrentStep();

	//Deactivate target display if needed
	if(m_fm && !m_fm->AllowZoom() && IsTargetOn())
		m_fm->Cancel();

	StopFire();
	SetBusy(true);
	m_switchingFireMode = true;

	if(zoomed)
	{
		//Only sound
		PlayAction(g_pItemStrings->change_firemode_zoomed);

		//TODO: (SOCOM has anim)... if we get animations for all the weapons remove this...
		if(SupportsDualWield(GetEntity()->GetClass()->GetName()))
		{
			GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson)>>1, CSchedulerAction<PlayLeverLayer>::Create(PlayLeverLayer(this, m_switchLeverLayers)), false);
			GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson), CSchedulerAction<EndChangeFireModeAction>::Create(EndChangeFireModeAction(this)), false);
		}
		else
		{
			GetScheduler()->TimerAction(350, CSchedulerAction<PlayLeverLayer>::Create(PlayLeverLayer(this, m_switchLeverLayers)), false);
			GetScheduler()->TimerAction(350, CSchedulerAction<EndChangeFireModeAction>::Create(EndChangeFireModeAction(this)), false);
		}
		return;
	}

	//If NANO speed selected...
	float speedOverride = -1.0f;
	float mult = 1.0f;
	CActor *owner = GetOwnerActor();
	if (owner && owner->GetActorClass() == CPlayer::GetActorClassType())
	{
		CPlayer *pPlayer = (CPlayer *)owner;
		if(pPlayer->GetNanoSuit())
		{
			if (pPlayer->GetNanoSuit()->GetMode() == NANOMODE_SPEED)
			{
				speedOverride = 1.75f;
				mult = 1.0f/1.75f;
			}
		}
	}

	if (speedOverride > 0.0f)
		PlayAction(g_pItemStrings->change_firemode, 0, false, CItem::eIPAF_Default, speedOverride);
	else
		PlayAction(g_pItemStrings->change_firemode);

	GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson)>>1, CSchedulerAction<PlayLeverLayer>::Create(PlayLeverLayer(this, m_switchLeverLayers)), false);
	GetScheduler()->TimerAction(GetCurrentAnimationTime(eIGS_FirstPerson), CSchedulerAction<EndChangeFireModeAction>::Create(EndChangeFireModeAction(this)), false);

	if(pSlave)
		pSlave->StartChangeFireMode();
}

void CWeapon::EndChangeFireMode()
{
	//Real change is here
	ChangeFireMode();

	m_switchLeverLayers = !m_switchLeverLayers;	
	
	SetBusy(false);
	ForcePendingActions();

	m_switchingFireMode = false;
}

//-----------------------------------------------------------------
EntityId CWeapon::GetLAMAttachment()
{
	for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); it++)
	{
		if (it->first == g_pItemStrings->LAM || it->first == g_pItemStrings->LAMRifle)
			return it->second;
	}

	return 0;
}

//-----------------------------------------------------------------
bool CWeapon::IsLamAttached()
{
	return GetLAMAttachment() != 0;
}

//------------------------------------------------------------
EntityId CWeapon::GetFlashlightAttachment()
{
	for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); it++)
	{
		if (it->first == g_pItemStrings->LAMFlashLight || it->first == g_pItemStrings->LAMRifleFlashLight)
			return it->second;
	}

	return 0;
}

//-----------------------------------------------------------------
bool CWeapon::IsFlashlightAttached()
{
	return GetFlashlightAttachment() != 0;
}

//-----------------------------------------------------------------
void CWeapon::ActivateLamLaser(bool activate, bool aiRequest /* = true */)
{
	m_lamID = GetLAMAttachment();
	m_lamAttached = m_lamID != 0;

	//Only if LAM is attached
	if (m_lamAttached)
	{
		CLam* pLam = static_cast<CLam*>(m_pItemSystem->GetItem(m_lamID));
		if (pLam)
			pLam->ActivateLaser(activate, aiRequest);
	}
	else
	{
		GameWarning("No LAM attached!! Laser could not be activated/deactivated");
	}
}

//------------------------------------------------------------------
void CWeapon::ActivateLamLight(bool activate, bool aiRequest /* = true */)
{
	m_lamID = GetLAMAttachment();
	if(m_lamID==0)
		m_lamID = GetFlashlightAttachment(); 
	m_lamAttached = m_lamID != 0;

	//Only if LAM is attached
	if (m_lamAttached)
	{
		CLam* pLam = static_cast<CLam*>(m_pItemSystem->GetItem(m_lamID));
		if (pLam)
			pLam->ActivateLight(activate,aiRequest);
	}
	else
	{
		GameWarning("No LAM attached!! Light could not be activated/deactivated");
	}
}

//------------------------------------------------------------------
bool CWeapon::IsLamLaserActivated()
{
	m_lamID = GetLAMAttachment();
	m_lamAttached = m_lamID != 0;

	//Only if LAM is attached
	if (m_lamAttached)
	{
		CLam* pLam = static_cast<CLam*>(m_pItemSystem->GetItem(m_lamID));
		if (pLam)
			return pLam->IsLaserActivated();
	}

	return false;
}

//------------------------------------------------------------------
bool CWeapon::IsLamLightActivated()
{
	m_lamID = GetLAMAttachment();
	if(m_lamID==0)
		m_lamID = GetFlashlightAttachment(); 
	m_lamAttached = m_lamID != 0;

	//Only if LAM is attached
	if (m_lamAttached)
	{
		CLam* pLam = static_cast<CLam*>(m_pItemSystem->GetItem(m_lamID));
		if (pLam)
			return pLam->IsLightActivated();
	}

	return false;
}

void CWeapon::RaiseWeapon(bool raise, bool faster /* = false */)
{
	if(m_params.raiseable)
	{
		if(raise && !m_weaponRaised)
		{
			//Play the sound anyways if necessary...
			CPlayer *pPlayer = static_cast<CPlayer*>(GetOwnerActor());
			if ((pPlayer != NULL) && pPlayer->IsClient())
			{
				SPlayerStats *stats = static_cast<SPlayerStats*>(pPlayer->GetActorStats());
				if(stats)
				{
					Vec3 vel = stats->velocity;
					if(vel.z<0.0f)
						vel.z = 0.0f;

					float velLenghtSqr = vel.len2();
					static float lastFXTime = 0.0f;
					if(velLenghtSqr>25.0f && ((gEnv->pTimer->GetCurrTime()-lastFXTime)>0.5f))
					{
						lastFXTime = gEnv->pTimer->GetCurrTime();

						pPlayer->PlaySound(CPlayer::ESound_Hit_Wall,true,true,"speed",0.6f);
						//FX feedback
						IMovementController *pMC = pPlayer->GetMovementController();
						if(pMC)
						{
							SMovementState state;
							pMC->GetMovementState(state);
							IParticleEffect *pEffect = gEnv->p3DEngine->FindParticleEffect("collisions.footsteps.dirt");
							if (pEffect)
							{
								Matrix34 tm = IParticleEffect::ParticleLoc(state.eyePosition + state.eyeDirection*0.5f);
								pEffect->Spawn(true,tm);
							}	
						}		
					}

					//Do not raise while moving
					if(velLenghtSqr>0.5f)
					{
						m_raiseProbability = 0.0f;
						return;
					}
				}

				//Weapon zoomed, reloading, ...
				if(IsBusy() || IsZooming() || IsZoomed() || IsModifying() || (m_fm && (m_fm->IsFiring()||m_fm->IsReloading())))
					return;

				m_raiseProbability +=gEnv->pTimer->GetFrameTime();

				if (m_raiseProbability<0.25f)
					return;
			}

			if(m_fm && !m_fm->AllowZoom())
				m_fm->Cancel();

			StopFire();

			//If NANO speed selected...
			float speedOverride = -1.0f;

			CActor *owner = GetOwnerActor();
			if (owner && owner->GetActorClass() == CPlayer::GetActorClassType())
			{
				CPlayer *pPlayer = (CPlayer *)owner;
				if(pPlayer->GetNanoSuit())
				{
					if (pPlayer->GetNanoSuit()->GetMode() == NANOMODE_SPEED)
					{
						speedOverride = 1.75f;
					}
				}
			}

			SetWeaponRaised(true); 

			if (speedOverride > 0.0f)
				PlayAction(g_pItemStrings->raise, 0, false, CItem::eIPAF_Default, speedOverride);
			else
				PlayAction(g_pItemStrings->raise);

			SetDefaultIdleAnimation(eIGS_FirstPerson,g_pItemStrings->idle_raised);

			RequestWeaponRaised(true);

		}
		else if(!raise && m_weaponRaised)
		{

			//If NANO speed selected...
			float speedOverride = -1.0f;
		
			CActor *owner = GetOwnerActor();
			if (owner && owner->GetActorClass() == CPlayer::GetActorClassType())
			{
				CPlayer *pPlayer = (CPlayer *)owner;
				if(pPlayer->GetNanoSuit())
					if (pPlayer->GetNanoSuit()->GetMode() == NANOMODE_SPEED || faster)
						speedOverride = 1.75f;
			}

			if (speedOverride > 0.0f)
				PlayAction(g_pItemStrings->lower, 0, false, CItem::eIPAF_Default, speedOverride);
			else
				PlayAction(g_pItemStrings->lower);

			SetDefaultIdleAnimation(eIGS_FirstPerson,g_pItemStrings->idle);

			SetWeaponRaised(false);

			m_raiseProbability = 0.0f;

			RequestWeaponRaised(false);

			ForcePendingActions();

		}
		else
			m_raiseProbability = 0.0f;
	}
}

//-----------------------------------------------------
void CWeapon::StartUse(EntityId userId)
{
	CItem::StartUse(userId);

	SendMusicLogicEvent(eMUSICLOGICEVENT_WEAPON_MOUNT);
}

//-----------------------------------------------------
void CWeapon::StopUse(EntityId userId)
{
	SendMusicLogicEvent(eMUSICLOGICEVENT_WEAPON_UNMOUNT);

	CItem::StopUse(userId);

	if(m_stats.mounted)
	{
		if(IsZoomed() || IsZooming())
			ExitZoom();

		ExitViewmodes();
		StopFire(); //Stop firing just in case
	}
}

//-------------------------------------------------------
void CWeapon::AutoDrop()
{
	if(m_fm)
	{
		m_firedRockets--;

		CActor* pOwner = GetOwnerActor();
		// no need to auto-drop for AI
		if(pOwner && !pOwner->IsPlayer())
			return;

		if((GetAmmoCount(m_fm->GetAmmoType())<=0)&&(m_firedRockets<=0))
		{
			if( pOwner )
			{
				if(IsSelected())
					pOwner->DropItem(GetEntityId(),2.0f,true,false);
				else
					pOwner->DropItem(GetEntityId(),2.0f,false,false);
			}
			if(!gEnv->bMultiplayer)
				g_pGame->GetGameRules()->ScheduleEntityRemoval(GetEntityId(),5.0f,true);
		}
	}
}

//------------------------------------------------------
bool CWeapon::CheckAmmoRestrictions(EntityId pickerId)
{
	if(g_pGameCVars->i_unlimitedammo != 0)
		return true;

	if(gEnv->pSystem->IsEditor())
		return true;

	IActor* pPicker = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pickerId);
	if(pPicker)
	{

		IInventory *pInventory = pPicker->GetInventory();
		if(pInventory)
		{
			const char* className = GetEntity()->GetClass()->GetName();
			if(pInventory->GetCountOfClass(className) == 0)
				return true;
			
			//Dual SOCOM
			if(pInventory->GetCountOfClass(className) == 1 && SupportsDualWield(className))
				return true;

			//Check for accessories that give ammo
			if(!m_accessories.empty())
			{
				for (TAccessoryMap::iterator it=m_accessories.begin(); it!=m_accessories.end(); ++it)
				{
					if(CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(it->second)))
					{
						if(pItem->GivesAmmo() && pItem->CheckAmmoRestrictions(pickerId))
							return true;
					}
				}
			}

			if (!m_bonusammo.empty())
			{
				for (TAmmoMap::const_iterator it=m_bonusammo.begin(); it!=m_bonusammo.end(); ++it)
				{
					int invAmmo  = pInventory->GetAmmoCount(it->first);
					int invLimit = pInventory->GetAmmoCapacity(it->first);

					if(invAmmo>=invLimit)
						return false;
				}
			}

			if(!m_ammo.empty())
			{
				for (TAmmoMap::const_iterator it=m_ammo.begin(); it!=m_ammo.end(); ++it)
				{
					int invAmmo  = pInventory->GetAmmoCount(it->first);
					int invLimit = pInventory->GetAmmoCapacity(it->first);

					if(invAmmo>=invLimit && m_accessoryAmmo.find(it->first)==m_accessoryAmmo.end())
						return false;
				}
			}

		}		
	}

	return true;
}

//-------------------------------------------------------------
int CWeapon::GetMaxZoomSteps()
{
	if(m_zm)
		return m_zm->GetMaxZoomSteps();

	return 0;
}

//-------------------------------------------------------------
bool CWeapon::FireSlave(EntityId actorId, bool fire)
{
	CWeapon *dualWield = NULL;

	IItem *slave = GetDualWieldSlave();

	if (slave && slave->GetIWeapon())
		dualWield = static_cast<CWeapon *>(slave);

	if(!dualWield)
		return false;

	if(!fire)
	{
		dualWield->StopFire();
		return true;
	}

	m_fire_alternation = !m_fire_alternation;

	if (!m_fire_alternation && dualWield->CanFire())
	{
		dualWield->StartFire();
		return true;
	}

	return false;
}

//--------------------------------------------------------
void CWeapon::ReloadSlave()
{
	CWeapon *dualWield = NULL;

	IItem *slave = GetDualWieldSlave();

	if (slave && slave->GetIWeapon())
		dualWield = static_cast<CWeapon *>(slave);

	if(!dualWield)
		return;

	dualWield->Reload();
}

//----------------------------------------------------------
void CWeapon::SendMusicLogicEvent(EMusicLogicEvents event)
{
	CActor* pOwner = GetOwnerActor();
	if(pOwner && pOwner->IsClient() && pOwner->GetHealth()>0)
	{
		m_pGameFramework->GetMusicLogic()->SetEvent(event);
	}
}

//----------------------------------------------------------
void CWeapon::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	CItem::GetMemoryStatistics(s);
/*
	if (m_fm)
		m_fm->GetMemoryStatistics(s);
	if (m_zm)
		m_zm->GetMemoryStatistics(s);
*/
	{
		SIZER_COMPONENT_NAME(s, "FireModes");
		s->AddContainer(m_fmIds);
		s->AddContainer(m_firemodes);
		for (TFireModeIdMap::iterator iter = m_fmIds.begin(); iter != m_fmIds.end(); ++iter)
			s->Add(iter->first);
		for (size_t i=0; i<m_firemodes.size(); i++)
			if (m_firemodes[i])
				m_firemodes[i]->GetMemoryStatistics(s);
	}
	{
		SIZER_COMPONENT_NAME(s, "ZoomModes");
		s->AddContainer(m_zmIds);
		s->AddContainer(m_zoommodes);
		for (TZoomModeIdMap::iterator iter = m_zmIds.begin(); iter != m_zmIds.end(); ++iter)
			s->Add(iter->first);
		for (size_t i=0; i<m_zoommodes.size(); i++)
			if (m_zoommodes[i])
				m_zoommodes[i]->GetMemoryStatistics(s);
	}

	{
		SIZER_COMPONENT_NAME(s, "Ammo");
		s->AddContainer(m_ammo);
		s->AddContainer(m_bonusammo);
		s->AddContainer(m_accessoryAmmo);
		s->AddContainer(m_minDroppedAmmo);
	}

	s->AddContainer(m_listeners);
	s->Add( m_aiWeaponDescriptor.firecmdHandler );
	s->Add( m_aiWeaponDescriptor.smartObjectClass );
	s->AddContainer( m_viewModeList );
	for (std::vector<ItemString>::iterator iter = m_viewModeList.begin(); iter != m_viewModeList.end(); ++iter)
		s->Add(*iter);
}


//--------------------------------------------------------
bool CWeapon::AIUseOverrideOffset(EStance stance, float lean, Vec3& offset) const
{
	// do checks for if(found) here
	if(m_StanceWeponOffsetLeanLeft.empty() || m_StanceWeponOffsetLeanRight.empty() || m_StanceWeponOffset.empty())
		return false;
	TStanceWeaponOffset::const_iterator itrOffsetLeft(m_StanceWeponOffsetLeanLeft.find(stance));
	if(itrOffsetLeft == m_StanceWeponOffsetLeanLeft.end())
		return false;
	TStanceWeaponOffset::const_iterator itrOffsetRight(m_StanceWeponOffsetLeanRight.find(stance));
	if(itrOffsetRight == m_StanceWeponOffsetLeanRight.end())
		return false;
	TStanceWeaponOffset::const_iterator itrOffset(m_StanceWeponOffset.find(stance));
	if(itrOffset == m_StanceWeponOffset.end())
		return false;
	const Vec3& normal(itrOffset->second);
	const Vec3& lLeft(itrOffsetLeft->second);
	const Vec3& lRightt(itrOffsetRight->second);
	offset = SStanceInfo::GetOffsetWithLean(lean, normal, lLeft, lRightt );
	return true;
}

//----------------------------------------------------------

namespace 
{
	bool raycast(CActor* pActor, Vec3 pos, Vec3 dir, float length)
	{
		const int flags = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (8 & rwi_pierceability_mask) | (geom_colltype14 << rwi_colltype_bit);

		ray_hit hit;
		int hits=gEnv->pPhysicalWorld->RayWorldIntersection(pos, dir * length, ent_static|ent_rigid|ent_sleeping_rigid,flags,
																												&hit, 1, pActor->GetEntity()->GetPhysics());

		// Only raise the weapon when ray hits a nearly-vertical surface
		if (hits && hit.pCollider && (abs(hit.n.z) < 0.15f))
		{
			if(hit.pCollider->GetType()==PE_RIGID)
			{
				//Prevent raising in front of small rigid
				pe_params_part part;
				part.ipart = 0;
				if(hit.pCollider->GetParams(&part) && part.pPhysGeom)
				{
					if(part.pPhysGeom->V<0.4)
						return false;
				}
				else
					return false;
			}
			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(255,255,0,255), hit.pt, ColorB(255,0,0,255), 2.0f);
			return true;
		}

		//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pos, ColorB(0,255,0,255), pos + dir * length, ColorB(0,255,0,255), 2.0f);
		return false;
	}
}

void CWeapon::UpdateWeaponRaising(float frameTime)
{
	if(!CanBeRaised() || IsDualWieldSlave())
		return;

	CActor* pActor = GetOwnerActor();
	if(pActor)
	{
		COffHand* pOffHand = static_cast<COffHand*>(pActor->GetItemByClass(COffHand::sOffHandClass));
		if(pOffHand && pOffHand->IsSelected())
			return;

		IMovementController* pMC = pActor->GetMovementController();
		SPlayerStats *stats = static_cast<SPlayerStats*>(pActor->GetActorStats());

		if(pMC && stats)
		{
			SMovementState info;
			pMC->GetMovementState(info);

			Vec3 pos = info.weaponPosition;
			Vec3 dir = info.aimDirection;
			Vec3 rightDir = -info.upDirection.Cross(dir); rightDir.Normalize();

			float distance = GetRaiseDistance();

			//If it's dual wield we need some more tests
			if(IsDualWield())
			{

				//Cross product to get the "right" direction
				pos = pos - rightDir*0.13f;

				//Raytrace for the left SOCOM
				//Left SOCOM up/down
				if (raycast(pActor, pos, dir, distance))
				{
					CWeapon *pSlave = static_cast<CWeapon*>(GetDualWieldSlave());
					if(pSlave)
						pSlave->RaiseWeapon(true);
				}
				else
				{
					CWeapon *pSlave = static_cast<CWeapon*>(GetDualWieldSlave());
					if(pSlave)
						pSlave->RaiseWeapon(false);
				}

				//Raytrace for the right SOCOM
				pos = pos + rightDir*0.26f;

				//Right SOCOM up/down
				if (raycast(pActor, pos, dir, distance))
					RaiseWeapon(true);
				else
					RaiseWeapon(false);

			}
			else
			{
				//If it's not dualWield, just trace a ray using the position and aiming direction
				if (raycast(pActor, pos , dir, distance))
				{
					RaiseWeapon(true);
				}
				else
					RaiseWeapon(false);
			}

		}
	}
}

//----------------------------------------------------------
namespace
{
	bool IsFriendlyEntity(IEntity* pEntity, IActor* pAI, IActor* pPlayer)
	{
		bool sucess = false;

		//Only for actors (not vehicles)
		if(pAI && pEntity && pEntity->GetAI())
		{
			if(!pEntity->GetAI()->IsHostile(pPlayer->GetEntity()->GetAI(),false))
				return true;
			else
				return false;
		}


		//Special case (Animated objects), check for script table value "bFriendly"
		//Check script table (maybe is not possible to grab)
		if(pEntity)
		{
			SmartScriptTable props;
			IScriptTable* pScriptTable = pEntity->GetScriptTable();
			if(!pScriptTable || !pScriptTable->GetValue("Properties", props))
				return false;

			int isFriendly = 0;
			if(props->GetValue("bNoFriendlyFire", isFriendly) && isFriendly!=0)
				return true;
		}

		//for vehicles
		if( pEntity && pEntity->GetId() )
		{
			IVehicle *pVehicle = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(pEntity->GetId());
			if ( pVehicle )
			{
				if ( pPlayer->GetEntity() && pVehicle->HasFriendlyPassenger( pPlayer->GetEntity() ) )
					return true;
			}				
		}

		return false;
	}
}

void CWeapon::UpdateWeaponLowering(float frameTime)
{
	if(gEnv->bMultiplayer || IsDualWieldSlave())
		return;

	CWeapon *pSlave = NULL;
	if(IsDualWield())
		pSlave = static_cast<CWeapon*>(GetDualWieldSlave());

	CActor * pActor = GetOwnerActor();
	SPlayerStats* stats = pActor?static_cast<SPlayerStats*>(pActor->GetActorStats()):NULL;

	if(!stats)
		return;

	stats->bLookingAtFriendlyAI = false;
	LowerWeapon(false);
	if(pSlave)
		pSlave->LowerWeapon(false);

	if(IsWeaponRaised() || IsModifying())
		return;

	const ray_hit* hit = pActor->GetGameObject()->GetWorldQuery()->GetLookAtPoint(200.0f);
	IEntity* pLookAtEntity = NULL;
	EntityId entityId = 0;
	float    hitDistance = -1.0f;
	if(hit && hit->pCollider)
	{
		pLookAtEntity = m_pEntitySystem->GetEntityFromPhysics(hit->pCollider);
		if(pLookAtEntity)
			entityId=pLookAtEntity->GetId();
		hitDistance = hit->dist;
	}
	
	CActor *pActorAI = static_cast<CActor*>(gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId));

	//First check, direct ray
	if(IsFriendlyEntity(pLookAtEntity,pActorAI,pActor))
	{
		LowerWeapon(true);
		if(GetEntity()->GetClass()!=CItem::sOffHandClass)
			StopFire();//Just in case
		if(pSlave)
		{
			pSlave->LowerWeapon(true);
			pSlave->StopFire();
		}
		Vec3 dis = pLookAtEntity->GetWorldPos()-pActor->GetEntity()->GetWorldPos();
		if(dis.len2()<5.0f)
			stats->bLookingAtFriendlyAI = true;

		return;
	}

	pLookAtEntity = pActor->GetGameObject()->GetWorldQuery()->GetEntityInFrontOf();
	if(pLookAtEntity)
	{
		pActorAI = static_cast<CActor*>(gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pLookAtEntity->GetId()));

		//Check if there's something in between
		if(hitDistance>0.0f && hitDistance<5.0f)
		{
			Vec3 entityPos = pLookAtEntity->GetWorldPos();
			Vec3 hitPos    = hit->pt;
			Vec3 playerPos = pActor->GetEntity()->GetWorldPos();
			entityPos.z = hitPos.z = playerPos.z = 0.0f;
			if((entityPos-playerPos).len2()>(hitPos-playerPos).len2())
				return;
		}
	}

	//If not, check entity in front
	if(IsFriendlyEntity(pLookAtEntity,pActorAI,pActor))
	{
		LowerWeapon(true);
		if(GetEntity()->GetClass()!=CItem::sOffHandClass)
			StopFire();//Just in case
		if(pSlave)
		{
			pSlave->LowerWeapon(true);
			pSlave->StopFire();
		}
		Vec3 dis = pLookAtEntity->GetWorldPos()-pActor->GetEntity()->GetWorldPos();
		if(dis.len2()<5.0f)
			stats->bLookingAtFriendlyAI = true;
	}

}

//----------------------------------------------------------
bool CWeapon::FilterView(SViewParams &viewParams)
{
	bool ret = CItem::FilterView(viewParams);

	if(m_zm && m_zm->IsZoomed())
		m_zm->FilterView(viewParams);

	return ret;
}

//--------------------------------------------------
void CWeapon::PostFilterView(struct SViewParams &viewParams)
{
	CItem::PostFilterView(viewParams);

	if(m_zm && m_zm->IsZoomed())
		m_zm->PostFilterView(viewParams);
}

//------------------------------------------------
void CWeapon::RestorePlayerSprintingStats()
{
	if(gEnv->bMultiplayer)
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(GetOwnerActor());
		if(pPlayer && pPlayer->IsClient())
			if(SPlayerStats *pStats = static_cast<SPlayerStats*>(pPlayer->GetActorStats()))
				pStats->bIgnoreSprinting = false;
	}
}

//-------------------------------------------------
void CWeapon::OnZoomIn()
{
	bool hasSniperScope = false;;
	for(TAccessoryMap::const_iterator it = m_accessories.begin();it!=m_accessories.end();it++)
	{
		CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(it->second));
		if(pItem && pItem->GetParams().scopeAttachment)
		{
			if(SAccessoryParams *params = GetAccessoryParams(it->first))
			{
				pItem->DrawSlot(eIGS_FirstPerson,false);
				ResetCharacterAttachment(eIGS_FirstPerson, params->attach_helper.c_str());
				pItem->DrawSlot(eIGS_Aux1,false);
				SetCharacterAttachment(eIGS_FirstPerson, params->attach_helper, pItem->GetEntity(), eIGS_Aux1, 0);
				hasSniperScope = true;
			}	
		}
	}

	if(!hasSniperScope)
		Hide(true);
}

//-------------------------------------------------
void CWeapon::OnZoomOut()
{
	bool hasSniperScope = false;;
	for(TAccessoryMap::const_iterator it = m_accessories.begin();it!=m_accessories.end();it++)
	{
		CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(it->second));
		if(pItem && pItem->GetParams().scopeAttachment)
		{
			if(SAccessoryParams *params = GetAccessoryParams(it->first))
			{
				pItem->DrawSlot(eIGS_Aux1,false);
				ResetCharacterAttachment(eIGS_FirstPerson, params->attach_helper.c_str());
				pItem->DrawSlot(eIGS_FirstPerson,false);
				SetCharacterAttachment(eIGS_FirstPerson, params->attach_helper, pItem->GetEntity(), eIGS_FirstPerson, 0);
				hasSniperScope = true;
			}	

		}
	}

	if(!hasSniperScope)
		Hide(false);
}

//-------------------------------------------------
bool CWeapon::GetScopePosition(Vec3& pos)
{
	for(TAccessoryMap::const_iterator it = m_accessories.begin();it!=m_accessories.end();it++)
	{
		CItem* pItem = static_cast<CItem*>(m_pItemSystem->GetItem(it->second));
		if(pItem && pItem->GetParams().scopeAttachment)
		{
			if(SAccessoryParams *params = GetAccessoryParams(it->first))
			{
				pos = GetSlotHelperPos(eIGS_FirstPerson,params->attach_helper.c_str(),true);
				Matrix33 rot = GetSlotHelperRotation(eIGS_FirstPerson,params->attach_helper.c_str(),true);
				Vec3 dirZ = rot.GetColumn1();
				if(pItem->GetParams().scopeAttachment==1)
				{
					const float sniperZOfffset = 0.029f;
					pos += (sniperZOfffset*dirZ);
				}
				else if(pItem->GetParams().scopeAttachment==2)
				{
					const float lawZOffset = -0.028f;
					const float lawXOffset = -0.017f;
					pos += (lawZOffset*dirZ);
					Vec3 dirX = rot.GetColumn2();
					pos += (lawXOffset*dirX);
				}

				return true;
			}	
		}
	}
	return false;
}

//------------------------------------------------------
void CWeapon::SetNextShotTime(bool activate)
{
	if(activate)
	{
		// MUST BE CALLED from Select(true), after firemode activation
		// Prevent exploit fire rate by switching weapons
		if(m_fm && m_nextShotTime > 0.0f)
		{
			CTimeValue time = gEnv->pTimer->GetFrameStartTime();
			float dt = m_nextShotTime - time.GetSeconds();
			if(dt > 0.0f)
				m_fm->SetNextShotTime(dt);
			m_nextShotTime = 0.0f;
		}
	}
	else
	{
		// MUST BE CALLED from Select(false), before firemode deactivation
		// save game time when the weapon can next be fired
		m_nextShotTime = 0.0f;
		if(m_fm)
		{
			float delay = m_fm->GetNextShotTime();
			if(delay > 0.0f)
			{
				CTimeValue time = gEnv->pTimer->GetFrameStartTime();
				m_nextShotTime = time.GetSeconds() + delay;
			}
		}
	}
}

//------------------------------------------------
void CWeapon::CacheRaisePose()
{
	const char* name = m_params.pose.c_str();
	m_raisePose = (uint8)eWeaponRaisedPose_None;

	if (strcmp(name, "nw") == 0)
		m_raisePose = (uint8)eWeaponRaisedPose_Fists;
	else if (strcmp(name, "pistol") == 0)
		m_raisePose = (uint8)eWeaponRaisedPose_Pistol;
	else if (strcmp(name, "rifle") == 0)
		m_raisePose = (uint8)eWeaponRaisedPose_Rifle;
	else if (strcmp(name, "rocket") == 0)
		m_raisePose = (uint8)eWeaponRaisedPose_Rocket;
	else if (strcmp(name, "mg") == 0)
		m_raisePose = (uint8)eWeaponRaisedPose_MG;
}