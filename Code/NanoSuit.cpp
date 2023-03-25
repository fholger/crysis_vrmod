/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 22:11:2005: Created by Filippo De Luca
	- 31:01:2006: taken over by Jan Müller

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include "Player.h" 
#include "GameUtils.h"
#include "HUD/HUD.h"
#include "GameRules.h"
#include "NetInputChainDebug.h"
#include "BulletTime.h"
#include "SoundMoods.h"
#include "WeaponSystem.h"
#include "OffHand.h"

#include <ISound.h>
#include <ISerialize.h>
#include <IGameTokens.h>
#include <IMaterialEffects.h>

CNanoSuit::SNanoMaterial g_USNanoMats[NANOMODE_LAST];
CNanoSuit::SNanoMaterial g_AsianNanoMats[NANOMODE_LAST];
static const float HIT_EFFECT_TIME = 0.5f;

static void PrecacheMaterials(bool bCacheAsian)
{
	// preload materials
	IMaterialManager* matMan = gEnv->p3DEngine->GetMaterialManager();
	if (!g_USNanoMats[NANOMODE_SPEED].body)
	{
		g_USNanoMats[NANOMODE_SPEED].body = matMan->LoadMaterial("objects/characters/human/us/nanosuit/nanosuit_us_speed.mtl");
		g_USNanoMats[NANOMODE_SPEED].helmet = matMan->LoadMaterial("objects/characters/human/us/nanosuit/nanosuit_us_helmet_speed.mtl");
		g_USNanoMats[NANOMODE_SPEED].arms = matMan->LoadMaterial( "objects/weapons/arms_global/arms_nanosuit_us_speed.mtl");
		g_USNanoMats[NANOMODE_STRENGTH].body = matMan->LoadMaterial("objects/characters/human/us/nanosuit/nanosuit_us_strength.mtl");
		g_USNanoMats[NANOMODE_STRENGTH].helmet = matMan->LoadMaterial("objects/characters/human/us/nanosuit/nanosuit_us_helmet_strength.mtl");
		g_USNanoMats[NANOMODE_STRENGTH].arms = matMan->LoadMaterial( "objects/weapons/arms_global/arms_nanosuit_us_strength.mtl");
		g_USNanoMats[NANOMODE_CLOAK].body = matMan->LoadMaterial("objects/characters/human/us/nanosuit/nanosuit_us_cloak.mtl");
		g_USNanoMats[NANOMODE_CLOAK].helmet = matMan->LoadMaterial("objects/characters/human/us/nanosuit/nanosuit_us_helmet_cloak.mtl");
		g_USNanoMats[NANOMODE_CLOAK].arms = matMan->LoadMaterial( "objects/weapons/arms_global/arms_nanosuit_us_cloak.mtl");
		g_USNanoMats[NANOMODE_DEFENSE].body = matMan->LoadMaterial("objects/characters/human/us/nanosuit/nanosuit_us.mtl");
		g_USNanoMats[NANOMODE_DEFENSE].helmet = matMan->LoadMaterial("objects/characters/human/us/nanosuit/nanosuit_us_helmet.mtl");
		g_USNanoMats[NANOMODE_DEFENSE].arms = matMan->LoadMaterial( "objects/weapons/arms_global/arms_nanosuit_us.mtl");
		g_USNanoMats[NANOMODE_INVULNERABILITY].body = matMan->LoadMaterial("objects/characters/human/us/nanosuit/nanosuit_us_invulnerability.mtl");
		g_USNanoMats[NANOMODE_INVULNERABILITY].helmet = matMan->LoadMaterial("objects/characters/human/us/nanosuit/nanosuit_us_helmet_invulnerability.mtl");
		g_USNanoMats[NANOMODE_INVULNERABILITY].arms = matMan->LoadMaterial( "objects/weapons/arms_global/arms_nanosuit_us_invulnerability.mtl");
		g_USNanoMats[NANOMODE_DEFENSE_HIT_REACTION].body = matMan->LoadMaterial("objects/characters/human/us/nanosuit/nanosuit_us_invulnerability.mtl");
		g_USNanoMats[NANOMODE_DEFENSE_HIT_REACTION].helmet = matMan->LoadMaterial("objects/characters/human/us/nanosuit/nanosuit_us_helmet_invulnerability.mtl");
		g_USNanoMats[NANOMODE_DEFENSE_HIT_REACTION].arms = matMan->LoadMaterial( "objects/weapons/arms_global/arms_nanosuit_us_invulnerability.mtl");
		// strategically leak it
		for (int i=0; i<NANOMODE_LAST; ++i)
		{
			g_USNanoMats[i].body->AddRef();
			g_USNanoMats[i].helmet->AddRef();
			g_USNanoMats[i].arms->AddRef();
		}
	}

	if (bCacheAsian && !g_AsianNanoMats[NANOMODE_SPEED].body)
	{
		g_AsianNanoMats[NANOMODE_SPEED].body = matMan->LoadMaterial("objects/characters/human/asian/nanosuit/nanosuit_asian_speed.mtl");
		g_AsianNanoMats[NANOMODE_SPEED].helmet = matMan->LoadMaterial("objects/characters/human/asian/nanosuit/nanosuit_asian_helmet_speed.mtl");
		g_AsianNanoMats[NANOMODE_SPEED].arms = matMan->LoadMaterial("objects/weapons/arms_global/arms_nanosuit_asian_speed.mtl");
		g_AsianNanoMats[NANOMODE_STRENGTH].body = matMan->LoadMaterial("objects/characters/human/asian/nanosuit/nanosuit_asian_strength.mtl");
		g_AsianNanoMats[NANOMODE_STRENGTH].helmet = matMan->LoadMaterial("objects/characters/human/asian/nanosuit/nanosuit_asian_helmet_strength.mtl");
		g_AsianNanoMats[NANOMODE_STRENGTH].arms = matMan->LoadMaterial("objects/weapons/arms_global/arms_nanosuit_asian_strength.mtl");
		g_AsianNanoMats[NANOMODE_CLOAK].body = matMan->LoadMaterial("objects/characters/human/asian/nanosuit/nanosuit_asian_cloak.mtl");
		g_AsianNanoMats[NANOMODE_CLOAK].helmet = matMan->LoadMaterial("objects/characters/human/asian/nanosuit/nanosuit_asian_helmet_cloak.mtl");
		g_AsianNanoMats[NANOMODE_CLOAK].arms = matMan->LoadMaterial("objects/weapons/arms_global/arms_nanosuit_asian_cloak.mtl");
		g_AsianNanoMats[NANOMODE_DEFENSE].body = matMan->LoadMaterial("objects/characters/human/asian/nanosuit/nanosuit_asian.mtl");
		g_AsianNanoMats[NANOMODE_DEFENSE].helmet = matMan->LoadMaterial("objects/characters/human/asian/nanosuit/nanosuit_asian_helmet.mtl");
		g_AsianNanoMats[NANOMODE_DEFENSE].arms = matMan->LoadMaterial("objects/weapons/arms_global/arms_nanosuit_asian.mtl");
		g_AsianNanoMats[NANOMODE_INVULNERABILITY].body = matMan->LoadMaterial("objects/characters/human/asian/nanosuit/nanosuit_asian_invulnerability.mtl");
		g_AsianNanoMats[NANOMODE_INVULNERABILITY].helmet = matMan->LoadMaterial("objects/characters/human/asian/nanosuit/nanosuit_asian_helmet_invulnerability.mtl");
		g_AsianNanoMats[NANOMODE_INVULNERABILITY].arms = matMan->LoadMaterial("objects/weapons/arms_global/arms_nanosuit_asian_invulnerability.mtl");
		g_AsianNanoMats[NANOMODE_DEFENSE_HIT_REACTION].body = matMan->LoadMaterial("objects/characters/human/asian/nanosuit/nanosuit_asian_invulnerability.mtl");
		g_AsianNanoMats[NANOMODE_DEFENSE_HIT_REACTION].helmet = matMan->LoadMaterial("objects/characters/human/asian/nanosuit/nanosuit_asian_helmet_invulnerability.mtl");
		g_AsianNanoMats[NANOMODE_DEFENSE_HIT_REACTION].arms = matMan->LoadMaterial("objects/weapons/arms_global/arms_nanosuit_asian_invulnerability.mtl");
		// strategically leak it
		for (int i=0; i<NANOMODE_LAST; ++i)
		{
			g_AsianNanoMats[i].body->AddRef();
			g_AsianNanoMats[i].helmet->AddRef();
			g_AsianNanoMats[i].arms->AddRef();
		}
	}
}

CNanoSuit::SNanoMaterial* CNanoSuit::GetNanoMaterial(ENanoMode mode, bool bAsian)
{
	const int nIndex = mode;
	if (nIndex < 0 || nIndex >= NANOMODE_LAST)
		return 0;

	PrecacheMaterials(bAsian);
	if (bAsian == false)
		return &g_USNanoMats[nIndex];
	else
		return &g_AsianNanoMats[nIndex];
}

bool CNanoSuit::AssignNanoMaterialToEntity(IEntity* pEntity, CNanoSuit::SNanoMaterial* pNanoMat)
{
	bool bSuccess = false;
	SEntitySlotInfo slotInfo;
	bool isClient = false;

	CActor* pActor = static_cast<CActor*> (g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()));
	if (pActor != 0 && pActor->IsClient())
	{
		isClient = true;
	}

	if(pNanoMat && pEntity->GetSlotInfo(0, slotInfo) && slotInfo.pCharacter!=0)
	{
		// this should be the legs of the character
		slotInfo.pCharacter->SetMaterial(pNanoMat->body);
		IAttachmentManager* pMan = slotInfo.pCharacter->GetIAttachmentManager();

		IAttachment* pAttachment = pMan->GetInterfaceByName("upper_body");
		if (pAttachment)
		{
			IAttachmentObject* pAttachmentObj = pAttachment->GetIAttachmentObject();
			if (pAttachmentObj)
			{
				ICharacterInstance* pCharInstance = pAttachmentObj->GetICharacterInstance();
				if (pCharInstance)
				{
					// needed to support "fp3p"
					if (isClient)
						pCharInstance->SetMaterial(pNanoMat->arms);
					else
						pCharInstance->SetMaterial(pNanoMat->body);
				}
			}
		}

		pAttachment = pMan->GetInterfaceByName("helmet");
		if (pAttachment)
		{
			IAttachmentObject* pAttachmentObj = pAttachment->GetIAttachmentObject();
			if (pAttachmentObj)
			{
				// TODO: maybe reduce just to pAttachmentObj->SetMaterial...
				ICharacterInstance* pCharInstance = pAttachmentObj->GetICharacterInstance();
				if (pCharInstance)
					pCharInstance->SetMaterial(pNanoMat->helmet);
				else
					pAttachmentObj->SetMaterial(pNanoMat->helmet);
			}
		}

		// arms ... these indices are a bit workaround
		if (pEntity->GetSlotInfo(3, slotInfo) && slotInfo.pCharacter!=0)
		{
			slotInfo.pCharacter->SetMaterial(pNanoMat->arms);
		}
		// second set of arms for dual socom
		if (pEntity->GetSlotInfo(4, slotInfo) && slotInfo.pCharacter!=0)
		{
			slotInfo.pCharacter->SetMaterial(pNanoMat->arms);
		}

		bSuccess = true;
	}
	return bSuccess;
}


void SNanoCloak::Update(CNanoSuit *pNano)
{
	if (!pNano || !pNano->GetOwner() || !pNano->GetOwner()->IsClient())
		return;

	CPlayer *pOwner = const_cast<CPlayer*>(pNano->GetOwner());

	//disable cloaking if health is too low (for the temperature camo) or suit energy goes too low (for any camo type I suppose)
	bool disableNormal(GetState() && pNano->GetSuitEnergy()<20);
	bool disableHeat(GetState()==3 && pOwner->GetHealth()<25);
	
	if (disableNormal || disableHeat)
	{
		CHUD *pHUD = g_pGame->GetHUD();
		if (pHUD)
		{
			string msg = "@" + m_HUDMessage;
			msg.append("_disabled");

			pHUD->TextMessage(msg);
			
			//FIXME:special message for the temperature cloak
			if (disableHeat)
			{
				pHUD->TextMessage("temperature_health_low");
			}
		}

		pNano->SetMode(NANOMODE_DEFENSE);
	}
}

//
CNanoSuit::CNanoSuit()
: m_pGameFramework(0)
, m_pNanoMaterial(0)
, m_activationTime(0.0f)
, m_invulnerabilityTimeout(0.0f)
, m_invulnerable(false)
{
	for(int i = 0; i < ESound_Suit_Last; ++i)
	{
		m_sounds[i].ID = INVALID_SOUNDID;
		m_sounds[i].bLooping = false;
		m_sounds[i].b3D = false;
		m_sounds[i].nMassIndex = -1;
		m_sounds[i].nSpeedIndex = -1;
		m_sounds[i].nStrengthIndex = -1;
	}

	m_energy = NANOSUIT_ENERGY;

	Reset(NULL);
}

CNanoSuit::~CNanoSuit()
{
}

void CNanoSuit::Reset(CPlayer *owner)
{
	m_healTime = 0;

	m_active = false;
	m_pOwner = owner;
	m_lastTap = eNA_None;
	m_pendingAction = eNA_None;
	m_bWasSprinting = false;
	m_bSprintUnderwater = false;
	m_energy = 0.0f;

	m_bNightVisionEnabled = false;

	for(int k = 0; k < NANOSLOT_LAST; ++k)
		m_slots[k].desiredVal = 50.0f;

	ResetEnergy();
	
	m_energyRechargeRate = 0.0f;
	m_healthRegenRate = 0.0f;
	m_healthAccError = 0.0f;
	m_fLastSoundPlayedMedical = 0;
	m_startedSprinting = 0;
	m_now = 0;
	m_lastTimeUsedThruster = 0;
	m_activationTime=0.0f;
	m_invulnerabilityTimeout=0.0f;
	m_invulnerable=false;
	m_defenseHitTimer = 0.0f;

	for(int i=0; i<ESound_Suit_Last; ++i)
	{
		if(m_sounds[i].ID != INVALID_SOUNDID)
		{
			if(gEnv->pSoundSystem)
				if(ISound *pSound = gEnv->pSoundSystem->GetSound(m_sounds[i].ID))
					pSound->Stop();
			
			m_sounds[i].ID = INVALID_SOUNDID;
			m_sounds[i].bLooping = false;
			m_sounds[i].b3D = false;
			m_sounds[i].nMassIndex = -1;
			m_sounds[i].nSpeedIndex = -1;
			m_sounds[i].nStrengthIndex = -1;
		}
	}

	m_healthRegenDelay = 0.0f;
	m_energyRechargeDelay = 0.0f;

	m_currentMode = NANOMODE_DEFENSE;
	// needs to be set before call to SetMode
	m_featureMask = 31; //5 features with 5 flags each (0000000000011111)
	//reset the cloaking
	SetCloak(false, true);
	m_cloak.Reset();

	//ActivateMode(NANOMODE_CLOAK, false);
	ActivateMode(NANOMODE_STRENGTH, true);
	ActivateMode(NANOMODE_SPEED, true);
	ActivateMode(NANOMODE_DEFENSE, true);
	ActivateMode(NANOMODE_CLOAK, true);

	Precache();
}

void CNanoSuit::SetParams(SmartScriptTable &rTable,bool resetFirst)
{
	//
	int mode = 1;
	rTable->GetValue("cloakType", mode);
	m_cloak.m_mode = ENanoCloakMode(mode);	
	rTable->GetValue("cloakEnergyCost",m_cloak.m_energyCost);
	rTable->GetValue("cloakHealthCost",m_cloak.m_healthCost);
	rTable->GetValue("cloakVisualDamp",m_cloak.m_visualDamp);
	rTable->GetValue("cloakSoundDamp",m_cloak.m_soundDamp);
	rTable->GetValue("cloakHeatDamp",m_cloak.m_heatDamp);
	
	const char *pHUDMessage;
	if (rTable->GetValue("cloakHudMessage",pHUDMessage))
		m_cloak.m_HUDMessage = string(pHUDMessage);
}

void CNanoSuit::SetInvulnerability(bool invulnerable)
{
	m_invulnerable=invulnerable;
	m_invulnerabilityTimeout=0.0f;
	SelectSuitMaterial();
	
	if (m_pOwner)
		m_pOwner->GetGameObject()->ChangedNetworkState(CPlayer::ASPECT_NANO_SUIT_INVULNERABLE);
}

void CNanoSuit::SetInvulnerabilityTimeout(float timeout)
{
	m_invulnerabilityTimeout=timeout;
}

void CNanoSuit::SetCloakLevel(ENanoCloakMode mode)
{
	// Currently only on/off supported!
	ENanoCloakMode oldMode = m_cloak.GetType();
	m_cloak.SetType(mode);

	if(oldMode != mode && m_cloak.IsActive())
	{
		SetCloak(false, true);
		SetCloak(true, true);
	}
}

void CNanoSuit::Update(float frameTime)
{
	if (!m_pOwner || m_pOwner->GetHealth()<=0 || m_pOwner->IsFrozen())
		return;

	// invulnerability effect works even with a powered down suit
	// it's a spawn protection mechanism, so we need to make sure
	// nanogrenades don't disrupt this spawn protection
	if (gEnv->bServer)
	{
		if (!m_invulnerable)
			m_invulnerabilityTimeout=0.0f;

		if (m_invulnerable && m_invulnerabilityTimeout>0.0f)
		{
			m_invulnerabilityTimeout-=frameTime;
			if (m_invulnerabilityTimeout<=0.0f)
			{
				m_invulnerabilityTimeout=0.0f;

				SetInvulnerability(false);
			}
		}
	}

	// the suit can take some time to power up
	if (!m_active && m_activationTime>0.0f)
	{
		m_activationTime-=frameTime;
		if (m_activationTime>0.0f)
			return;

		CHUD* pHUD = g_pGame->GetHUD();
		if (pHUD)
			pHUD->RebootHUD();

		m_active=true;
		m_activationTime=0.0f;
	}

	if(m_defenseHitTimer > 0.0f)
	{
		m_defenseHitTimer -= frameTime;
		if(m_defenseHitTimer <= 0.0f)
		{
			m_defenseHitTimer = 0.0f;
			SelectSuitMaterial();
			if (m_pOwner)
				m_pOwner->GetGameObject()->ChangedNetworkState(CPlayer::ASPECT_NANO_SUIT_DEFENSE_HIT);
		}
	}

	if (!m_active)
		return;

	bool isServer=gEnv->bServer;

	bool isAI = !m_pOwner->IsPlayer();

	//update health
	int32 currentHealth = m_pOwner->GetHealth();
	int32 maxHealth(m_pOwner->GetMaxHealth());
	float recharge = 0.0f;
	float rechargeTime = 20.0f;

	const SPlayerStats stats = *(static_cast<SPlayerStats*>(m_pOwner->GetActorStats()));

	if (isAI)
		rechargeTime=g_pGameCVars->g_AiSuitEnergyRechargeTime;
	else
	{
		if (gEnv->bMultiplayer)
			rechargeTime=g_pGameCVars->g_playerSuitEnergyRechargeTimeMultiplayer;
		else
		{
			if(m_currentMode != NANOMODE_DEFENSE)
				rechargeTime=g_pGameCVars->g_playerSuitEnergyRechargeTime;
			else
			{
				if(stats.speedFlat > 0.1f) //moving
					rechargeTime=g_pGameCVars->g_playerSuitEnergyRechargeTimeArmorMoving;
				else
					rechargeTime=g_pGameCVars->g_playerSuitEnergyRechargeTimeArmor;
			}
		}
	}

	recharge = NANOSUIT_ENERGY / max(0.01f, rechargeTime);

	m_energyRechargeRate = recharge;

	m_now = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();

	if (currentHealth < maxHealth || m_cloak.m_active)
	{
		//check for low health and play sound
		if(currentHealth < maxHealth*0.9f && GetSlotValue(NANOSLOT_MEDICAL, true) > 50)
		{
			if(m_now - m_fLastSoundPlayedMedical > 30000.0f)
			{
				m_fLastSoundPlayedMedical = m_now;
				PlaySound(MEDICAL_SOUND);
			}
		}

		if(m_currentMode == NANOMODE_DEFENSE) //some additional energy in defense mode
		{
			if (isAI)
				m_healthRegenRate = maxHealth / max(0.01f, g_pGameCVars->g_AiSuitArmorModeHealthRegenTime);
			else
			{
				if(stats.speedFlat > 0.1f)
					m_healthRegenRate = maxHealth / max(0.01f, g_pGameCVars->g_playerSuitArmorModeHealthRegenTimeMoving);
				else
					m_healthRegenRate = maxHealth / max(0.01f, g_pGameCVars->g_playerSuitArmorModeHealthRegenTime);
			}
		}
		else
		{
			if (isAI)
				m_healthRegenRate = maxHealth / max(0.01f, g_pGameCVars->g_AiSuitHealthRegenTime);
			else
			{
				if(stats.speedFlat > 0.1f)
					m_healthRegenRate = maxHealth / max(0.01f, g_pGameCVars->g_playerSuitHealthRegenTimeMoving);
				else
					m_healthRegenRate = maxHealth / max(0.01f, g_pGameCVars->g_playerSuitHealthRegenTime);
			}
		}

		//cap the health regeneration rate to a maximum (for AIs with lots of health)
		m_healthRegenRate = min(m_healthRegenRate, NANOSUIT_MAXIMUM_HEALTH_REGEN);

		m_healthRegenRate -= (m_cloak.m_active?m_cloak.m_healthCost:0.0f);
	}
	
	//subtract energy from suit for cloaking
	if(m_cloak.m_active)
	{
		float energyCost = m_cloak.m_energyCost * g_pGameCVars->g_suitCloakEnergyDrainAdjuster;
		if(stats.inFreefall)
			recharge = min(recharge-max(1.0f, energyCost*8.0f),-max(1.0f, energyCost*8.0f));
		else if(stats.isOnLadder)
			recharge = min(recharge-max(1.0f, energyCost*stats.speedFlat),-max(1.0f, energyCost*stats.speedFlat));
		else
			recharge = min(recharge-max(1.0f, energyCost*(stats.speedFlat * 0.5f)),-max(1.0f, energyCost*(stats.speedFlat * 0.5f)));
	}

	//this deals with sprinting
	UpdateSprinting(recharge, stats, frameTime);

	NETINPUT_TRACE(m_pOwner->GetEntityId(), m_energy);
	NETINPUT_TRACE(m_pOwner->GetEntityId(), recharge);

	if (isServer)
	{
		if (recharge < 0.0f || m_energyRechargeDelay <= 0.0f)
		{
			SetSuitEnergy(clamp(m_energy + recharge*frameTime, 0.0f, NANOSUIT_ENERGY));
		}
	}

	//CryLogAlways("%s Suit Energy: %.3f", m_pOwner->GetEntity()->GetName(), m_energy);

	if (m_healthRegenDelay > 0.0f)
	{
		bool regenAfterFullEnergy = g_pGameCVars->g_playerSuitHealthRegenDelay < 0.0f;
		if (!regenAfterFullEnergy || GetSuitEnergy() >= NANOSUIT_ENERGY)
			m_healthRegenDelay = max(0.0f, m_healthRegenDelay - frameTime);
	}

	if (m_energyRechargeDelay > 0.0f)
		m_energyRechargeDelay = max(0.0f, m_energyRechargeDelay - frameTime);

	for (int i=0;i<NANOSLOT_LAST;++i)
		m_slots[i].realVal = m_slots[i].desiredVal;

	if (isServer)
	{
		//adjust the player health.
		if (m_healthRegenDelay <= 0.0f)
		{
			m_healTime -= frameTime;
			if (m_healTime < 0.0f)
			{
				m_healTime += NANOSUIT_HEALTH_REGEN_INTERVAL;

				// Calculate the new health increase
				float healthInc = m_healthAccError + m_healthRegenRate * NANOSUIT_HEALTH_REGEN_INTERVAL;
				int healthIncInt = (int32)healthInc;
				// Since the health is measured as integer, carry on the fractions for the next addition
				// to get more accurate result in the health regeneration rate.
				m_healthAccError = healthInc - healthIncInt;

				int newHealth = min(maxHealth,(int32)(currentHealth + healthIncInt));
				if (currentHealth != newHealth)
					m_pOwner->SetHealth(newHealth);
			}
		}
	}

	if (m_energy!=m_lastEnergy)
	{
		if (isServer)
			m_pOwner->GetGameObject()->ChangedNetworkState(CPlayer::ASPECT_NANO_SUIT_ENERGY);

		// call listeners on nano energy change
		if (m_listeners.empty() == false)
		{
			std::vector<INanoSuitListener*>::iterator iter = m_listeners.begin();
			while (iter != m_listeners.end())
			{
				(*iter)->EnergyChanged(m_energy);
				++iter;
			}
		}
		//CryLogAlways("[nano]-- updating %s's nanosuit energy: %f", m_pOwner->GetEntity()->GetName(), m_energy);
	}

	Balance(m_energy);
	NETINPUT_TRACE(m_pOwner->GetEntityId(), m_slots[NANOSLOT_SPEED].realVal);
	NETINPUT_TRACE(m_pOwner->GetEntityId(), m_slots[NANOSLOT_SPEED].desiredVal);

	m_cloak.Update(this);

	//update object motion blur amount
	float motionBlurAmt(0.0f);
	if (m_currentMode == NANOMODE_SPEED)
		motionBlurAmt = 1.0f;

	IEntityRenderProxy * pRenderProxy = (IEntityRenderProxy*)m_pOwner->GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	if (pRenderProxy && stats.bSprinting)
	{ 
		float amt(pRenderProxy->GetMotionBlurAmount());
		amt += (motionBlurAmt - amt) * frameTime * 3.3f;
		pRenderProxy->SetMotionBlurAmount(amt);
	}
			
	CItem *currentItem = (CItem *)gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_pOwner->GetInventory()->GetCurrentItem());
	if (currentItem)
	{
		pRenderProxy = (IEntityRenderProxy*)currentItem->GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
		if (pRenderProxy)
		{
			float amt(pRenderProxy->GetMotionBlurAmount());
			amt += (motionBlurAmt - amt) * frameTime * 3.3f;
			pRenderProxy->SetMotionBlurAmount(amt);
		}
	}

	m_lastEnergy = m_energy;
}

void CNanoSuit::Balance(float energy)
{
	for(int i = 0; i < NANOSLOT_LAST; i++)
	{
		float slotPerCent = m_slots[i].desiredVal / NANOSUIT_ENERGY; //computes percentage for NANOSUIT_ENERGY total ...
			m_slots[i].realVal = energy * slotPerCent;
	}
}

void CNanoSuit::SetSuitEnergy(float value, bool playerInitiated /* = false */)
{
	value = clamp(value, 0.0f, NANOSUIT_ENERGY);
	if (m_pOwner && value!=m_energy && gEnv->bServer)
		m_pOwner->GetGameObject()->ChangedNetworkState(CPlayer::ASPECT_NANO_SUIT_ENERGY);

	if (!gEnv->bMultiplayer)
	{
		if (value < m_energy)
		{
			m_energyRechargeDelay = g_pGameCVars->g_playerSuitEnergyRechargeDelay;
		}
	}


	if (value != m_energy)
	{
		// call listeners on nano energy change
		if (m_listeners.empty() == false)
		{
			std::vector<INanoSuitListener*>::iterator iter = m_listeners.begin();
			while (iter != m_listeners.end())
			{
				(*iter)->EnergyChanged(value);
				++iter;
			}
		}
	}

	if (value < m_energy)
	{
		if (!playerInitiated)
		{
			//armor mode hit fx (in armor mode energy is decreased by damage
			/*if (m_energy-value>=NANOSUIT_ENERGY * 0.2f) //now always happening on hit
			{
				if(m_pOwner && !m_pOwner->IsGod() && !m_pOwner->IsThirdPerson() && (m_currentMode == NANOMODE_DEFENSE))
				{
					IMaterialEffects* pMaterialEffects = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects();
					SMFXRunTimeEffectParams params;
					params.pos = m_pOwner->GetEntity()->GetWorldPos();
					params.soundSemantic = eSoundSemantic_NanoSuit;
					TMFXEffectId id = pMaterialEffects->GetEffectIdByName("player_fx", "player_damage_armormode");
					pMaterialEffects->ExecuteEffect(id, params);
				}
			}*/
			if (gEnv->bMultiplayer && ((value/NANOSUIT_ENERGY)<=0.2f) && (m_energy>value) && g_pGameCVars->g_mpSpeedRechargeDelay) // if we cross the 20% boundary we don't regenerate for 3secs
				m_energyRechargeDelay=3.0f;
		}

		// spending energy cancels invulnerability
		if (m_invulnerable && gEnv->bServer)
			SetInvulnerability(false);
	}

	m_energy = value;
}

void CNanoSuit::Hit(int damage)
{
	//server only

	if (gEnv->bMultiplayer)
		m_energyRechargeDelay = MAX(m_energyRechargeDelay, 3.0f);
	else
		m_healthRegenDelay = fabsf(g_pGameCVars->g_playerSuitHealthRegenDelay);

	if(m_pOwner && m_pOwner->IsClient())
	{
		IMaterialEffects* pMaterialEffects = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects();
		SMFXRunTimeEffectParams params;
		params.pos = m_pOwner->GetEntity()->GetWorldPos();
		params.soundSemantic = eSoundSemantic_HUD;
		TMFXEffectId id = pMaterialEffects->GetEffectIdByName("player_fx", "player_damage_armormode");
		pMaterialEffects->ExecuteEffect(id, params);
	}

	/*if(damage > 10.0f && m_pOwner && m_pOwner->GetHealth() > 0)
	{
		m_defenseHitTimer = HIT_EFFECT_TIME;
		if(gEnv->bClient)
			SelectSuitMaterial();
		m_pOwner->GetGameObject()->ChangedNetworkState(CPlayer::ASPECT_NANO_SUIT_DEFENSE_HIT);
	}*/
}

bool CNanoSuit::SetAllSlots(float armor, float strength, float speed)
{
	float energy = armor + strength + speed;
	if(energy > NANOSUIT_ENERGY)
		return false;
	m_slots[NANOSLOT_ARMOR].desiredVal = armor;
	m_slots[NANOSLOT_STRENGTH].desiredVal = strength;
	m_slots[NANOSLOT_SPEED].desiredVal = speed;
	m_slots[NANOSLOT_MEDICAL].desiredVal = NANOSUIT_ENERGY - energy;

	return true;
}

bool CNanoSuit::SetMode(ENanoMode mode, bool forceUpdate, bool keepInvul)
{
	if (!m_active)
		return false;

	/*if(m_pOwner && (!stricmp("Kyong1", m_pOwner->GetEntity()->GetName()) || !stricmp("ai_kyong", m_pOwner->GetEntity()->GetName())))
	{
		mode = NANOMODE_STRENGTH;
	}*/

	if(m_currentMode == mode && !forceUpdate)
		return false;

	if(!(m_featureMask & (1<<mode)) && !forceUpdate)
		return false;

	ENanoMode lastMode = m_currentMode;
	m_currentMode = mode;

	const char* effectName = "";
	switch(mode)
	{
		case NANOMODE_SPEED:
			SetAllSlots(25.0f, 50.0f, 100.0f);
			if(!forceUpdate)
				PlaySound(ESound_SuitSpeedActivate);
			SetCloak(false);
			effectName = "suit_speedmode";
			//marcok: don't touch please
			if (g_pGameCVars->bt_speed)
			{
				IItem *pItem = m_pOwner->GetCurrentItem();
				IWeapon *pWeapon = pItem ? pItem->GetIWeapon() : NULL;
				if (!g_pGameCVars->bt_ironsight || (pWeapon && pWeapon->IsZoomed()))
				{
					g_pGame->GetBulletTime()->Activate(true);
				}
			}
			break;
		case NANOMODE_STRENGTH:
			SetAllSlots(50.0f, 100.0f, 25.0f);
			if(!forceUpdate)
				PlaySound(ESound_SuitStrengthActivate);
			SetCloak(false);
			effectName = "suit_strengthmode";
			break;
		case NANOMODE_DEFENSE:
			SetAllSlots(75.0f, 25.0f, 25.0f);
			if(!forceUpdate)
				PlaySound(ESound_SuitArmorActivate);
			SetCloak(false);
			effectName = "suit_armormode";
			break;
		case NANOMODE_CLOAK:
			SetAllSlots(50.0f, 50.0f, 50.0f);
			SetCloak(true, forceUpdate);
			effectName = "suit_cloakmode";
			break;
		default:
			assert(0);
			GameWarning("Non existing NANOMODE selected: %d", mode);
			return false;
	}

	//marcok: don't touch please
	if (g_pGameCVars->bt_speed)
	{
		if (lastMode != m_currentMode)
		{
			if (lastMode == NANOMODE_SPEED)
			{
				g_pGame->GetBulletTime()->Activate(false);
			}
		}
	}

	if(m_pOwner)
	{
		if(mode != NANOMODE_CLOAK && !m_pOwner->IsPlayer())
			PlaySound(ESound_AISuitHumming);
		else
			PlaySound(ESound_AISuitHumming, 0.0f, true);

		m_pGameFramework->GetIGameplayRecorder()->Event(m_pOwner->GetEntity(), GameplayEvent(eGE_SuitModeChanged, 0, (float)mode));

		if (gEnv->bServer && !keepInvul)
		{
			if (m_invulnerable)
				SetInvulnerability(false);
		}

		//draw some screen effect
		if(m_pOwner == m_pGameFramework->GetClientActor() && !m_pOwner->IsThirdPerson())
		{
			IMaterialEffects* pMaterialEffects = gEnv->pGame->GetIGameFramework()->GetIMaterialEffects();
			SMFXRunTimeEffectParams params;
			params.pos = m_pOwner->GetEntity()->GetWorldPos();
			params.soundSemantic = eSoundSemantic_NanoSuit;
			TMFXEffectId id = pMaterialEffects->GetEffectIdByName("player_fx", effectName);
			pMaterialEffects->ExecuteEffect(id, params);
		}
	}

	// call listeners on nano mode change
	if (m_listeners.empty() == false)
	{
		std::vector<INanoSuitListener*>::iterator iter = m_listeners.begin();
		while (iter != m_listeners.end())
		{
			(*iter)->ModeChanged(mode);
			++iter;
		}
	}

	SelectSuitMaterial();

	//stop hit timer if active
	if(m_currentMode != NANOMODE_DEFENSE)
		m_defenseHitTimer = 0.0f;

	if (m_pOwner)
		m_pOwner->GetGameObject()->ChangedNetworkState(CPlayer::ASPECT_NANO_SUIT_SETTING);

	// player's squadmates mimicking nanosuit modifications
	if (gEnv->pAISystem)
	{
		IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();//AI System will be the owner of this data
		pData->iValue = mode;
		if(m_pOwner && m_pOwner->GetEntity()->GetAI())
			gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER,1,"OnNanoSuitMode",m_pOwner->GetEntity()->GetAI(),pData);
	}

	// Report cloak usage to AI system.
	if (lastMode == NANOMODE_CLOAK && m_currentMode != NANOMODE_CLOAK)
	{
		if (GetOwner()->GetEntity() && GetOwner()->GetEntity()->GetAI())
			GetOwner()->GetEntity()->GetAI()->Event(AIEVENT_PLAYER_STUNT_UNCLOAK, 0);
	}
	if (lastMode != NANOMODE_CLOAK && m_currentMode == NANOMODE_CLOAK)
	{
		if (GetOwner()->GetEntity() && GetOwner()->GetEntity()->GetAI())
			GetOwner()->GetEntity()->GetAI()->Event(AIEVENT_PLAYER_STUNT_CLOAK, 0);
	}

	return true;
}

void CNanoSuit::SetCloak(bool on, bool force)
{
	if (!m_pOwner)
		return;

	bool switched(m_cloak.m_active!=on);

	if(true && !m_pOwner->IsPlayer())
	{
		if(on)
			PlaySound(ESound_AISuitCloakFeedback);
		else
			PlaySound(ESound_AISuitCloakFeedback, 0.0f, true);
	}

	m_cloak.m_active = on;
	if (switched || force)
	{
		if (m_pOwner)
		{
			if(!force && on)
			{
				PlaySound(ESound_SuitCloakActivate);
				if (!m_pOwner->IsClient())
					PlaySound(ESound_SuitCloakFeedback);
				else
				{
					m_pOwner->SendMusicLogicEvent(eMUSICLOGICEVENT_CLOAKMODE_ENTER);
				}
			}
			else if (!on)
			{
				if (!m_pOwner->IsClient())
					PlaySound(ESound_SuitCloakFeedback, 1, true);
				else
				{
					m_pOwner->SendMusicLogicEvent(eMUSICLOGICEVENT_CLOAKMODE_LEAVE);
				}
			}

			if(on)
			{
				SAFE_SOUNDMOODS_FUNC(AddSoundMood(SOUNDMOOD_ENTER_CLOAK));
			}
			else
			{
				SAFE_SOUNDMOODS_FUNC(AddSoundMood(SOUNDMOOD_LEAVE_CLOAK));
			}

			ENanoCloakMode cloakMode = m_cloak.GetType();

			// new cloak effect
			IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)m_pOwner->GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
			uint8 mask = pRenderProxy->GetMaterialLayersMask();
			uint32 blend = pRenderProxy->GetMaterialLayersBlend();
			mask = g_pGame->GetWeaponSystem()->IsFrozenEnvironment() ? mask|MTL_LAYER_DYNAMICFROZEN : mask&~MTL_LAYER_DYNAMICFROZEN;
			pRenderProxy->SetMaterialLayersMask(on? mask|MTL_LAYER_CLOAK : mask&~MTL_LAYER_CLOAK);
			pRenderProxy->SetMaterialLayersBlend((blend & 0xffffff00) | ((mask&MTL_LAYER_DYNAMICFROZEN) ?0xff : 0x00));
			if (CItem* pItem = static_cast<CItem*>(m_pOwner->GetCurrentItem(true)))
			{
				pItem->CloakSync(!force);
			}
			if (COffHand* pOffHand = static_cast<COffHand*>(m_pOwner->GetItemByClass(CItem::sOffHandClass)))
			{
				pOffHand->CloakSync(pOffHand->GetOffHandState() != eOHS_INIT_STATE);
			}

			// take care of the attachments on the back
			if (ICharacterInstance *pOwnerCharacter = m_pOwner->GetEntity()->GetCharacter(0))
			{
				if (IAttachmentManager *pAttachmentManager = pOwnerCharacter->GetIAttachmentManager())
				{
					int32 count = pAttachmentManager->GetAttachmentCount();
					for (uint32 i=0; i<count; ++i)
					{
						if (IAttachment* pAttachment = pAttachmentManager->GetInterfaceByIndex(i))
						{
							if(IAttachmentObject *pAO = pAttachment->GetIAttachmentObject())
							{
								if(pAO->GetAttachmentType()==IAttachmentObject::eAttachment_Entity)
								{
									CEntityAttachment* pEA = static_cast<CEntityAttachment*>(pAO);
									if (CItem* pItem = static_cast<CItem*>(gEnv->pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pEA->GetEntityId())))
									{
										pItem->CloakSync(!force);
									}
								}
							}
						}
					}
				}
			}

			m_pOwner->CreateScriptEvent("cloaking",on?cloakMode:0);
								
			// player's squadmates mimicking nanosuit modifications
			if (m_pOwner->GetEntity()->GetAI())
				gEnv->pAISystem->SendSignal(SIGNALFILTER_SENDER,1, (on?"OnNanoSuitCloak":"OnNanoSuitUnCloak"),m_pOwner->GetEntity()->GetAI());
		}
	}
}

void CNanoSuit::SelectSuitMaterial()
{
	if(!m_pOwner)
		return;

	if(m_currentMode == NANOMODE_CLOAK && m_cloak.GetType() != CLOAKMODE_CHAMELEON)
		return;

	IEntity* pEntity = m_pOwner->GetEntity();
	if (pEntity == 0)
		return;

	int mode=m_currentMode;
 	if (m_invulnerable)
		mode=NANOMODE_INVULNERABILITY;

	if(mode == NANOMODE_DEFENSE && m_defenseHitTimer > 0.0f)
		mode = NANOMODE_DEFENSE_HIT_REACTION;

	SNanoMaterial* pNanoMat = &m_pNanoMaterial[mode];
	AssignNanoMaterialToEntity(pEntity, pNanoMat);
}

void CNanoSuit::Precache()
{
	m_pNanoMaterial = g_USNanoMats;
	bool cacheAsian = false;

	SEntitySlotInfo slotInfo;
	if(m_pOwner && m_pOwner->GetEntity()->GetSlotInfo(0, slotInfo) && slotInfo.pCharacter!=0)
	{
		// default are US suits
		// have to do this workaround check, because we don't have "teams" in singleplayer
		const char* filePath = slotInfo.pCharacter->GetICharacterModel()->GetModelFilePath();
		if (strstr(filePath, "/us/") == NULL)
		{
			m_pNanoMaterial = g_AsianNanoMats;
			cacheAsian = true;
		}
	}

	m_pGameFramework = g_pGame->GetIGameFramework();
	PrecacheMaterials(cacheAsian);
}

float CNanoSuit::GetSlotValue(ENanoSlot slot,bool desired) const
{
	if (m_active && slot>=0 && slot<NANOSLOT_LAST)
		return (desired?m_slots[slot].desiredVal:m_slots[slot].realVal);

	return 0.0f;
}

bool CNanoSuit::GetSoundIsPlaying(ENanoSound sound) const
{
	if(!m_active || !gEnv->pGame->GetIGameFramework()->IsGameStarted())
		return false;

	if(m_sounds[sound].ID != INVALID_SOUNDID && gEnv->pSoundSystem)
	{
		ISound *pSound = gEnv->pSoundSystem->GetSound(m_sounds[sound].ID);
		if(pSound)
			return pSound->IsPlaying();
	}
	return false;
}

void CNanoSuit::DeactivateSuit(float time)
{
	SetSuitEnergy(0);
}

void CNanoSuit::PlaySound(ENanoSound sound, float param, bool stopSound)
{
	if(!gEnv->pSoundSystem || !m_pOwner || !m_active)
		return;

	int soundFlag = 0; //localActor will get 2D sounds
	ESoundSemantic eSemantic = eSoundSemantic_None;
	ISound *pSound = NULL;
	bool	setParam = false;
	bool	force3DSound = false;
	bool	bAppendPostfix=true;
	static string soundName;
  soundName.resize(0);

	switch(sound)
	{
	case SPEED_SOUND:
		soundName = "Sounds/interface:suit:suit_speed_use";
		eSemantic = eSoundSemantic_NanoSuit;
		if(m_pOwner->IsClient())
			if (gEnv->pInput && !stopSound) gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.05f, 0.0f, 0.6f) );
		break;
	case SPEED_IN_WATER_SOUND:
		soundName = "Sounds/interface:suit:suit_speed_use_underwater";
		eSemantic = eSoundSemantic_NanoSuit;
		if(m_pOwner->IsClient())
			if (gEnv->pInput && !stopSound) gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.05f, 0.0f, 0.9f) );
		break;
	case SPEED_SOUND_STOP:
		soundName = "Sounds/interface:suit:suit_speed_stop";
		eSemantic = eSoundSemantic_NanoSuit;
		break;
	case SPEED_IN_WATER_SOUND_STOP:
		soundName = "Sounds/interface:suit:suit_speed_stop_underwater";
		eSemantic = eSoundSemantic_NanoSuit;
		break;
	case STRENGTH_SOUND:
		soundName = "Sounds/interface:suit:suit_strength_use";
		eSemantic = eSoundSemantic_NanoSuit;
		setParam = true;
		if(m_pOwner->IsClient())
			if (gEnv->pInput && !stopSound) gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.05f, 1.0f, 0.5f) );
		break;
	case STRENGTH_LIFT_SOUND:
		soundName = "Sounds/interface:suit:suit_strength_lift";
		eSemantic = eSoundSemantic_NanoSuit;
		setParam = true;
		break;
	case STRENGTH_THROW_SOUND:
		soundName = "Sounds/interface:suit:suit_strength_use";
		eSemantic = eSoundSemantic_NanoSuit;
		if(m_pOwner->IsClient())
			if (gEnv->pInput && !stopSound)  gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.1f, 0.0f, 0.3f*param) );
		setParam = true;
		break;
	case STRENGTH_JUMP_SOUND:
		soundName = "Sounds/interface:suit:suit_strength_jump";
		eSemantic = eSoundSemantic_NanoSuit;
		if(m_pOwner->IsClient())
			if (gEnv->pInput && !stopSound)  gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.10f, 0.2f*param, 0.1f*param) );
		setParam = true;
		break;
	case STRENGTH_MELEE_SOUND:
		soundName = "Sounds/interface:suit:suit_strength_punch";
		eSemantic = eSoundSemantic_NanoSuit;
		if(m_pOwner->IsClient())
			if (gEnv->pInput && !stopSound) gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.05f, 1.0f*param, 0.5f*param) );
		setParam = true;
		break;
	case ARMOR_SOUND:
		soundName = "Sounds/interface:suit:suit_armor_use";
		eSemantic = eSoundSemantic_NanoSuit;
		if(m_pOwner->IsClient())
			if (gEnv->pInput && !stopSound) gEnv->pInput->ForceFeedbackEvent( SFFOutputEvent(eDI_XI, eFF_Rumble_Basic, 0.02f, 0.8f, 0.0f) );
		break;
	case MEDICAL_SOUND:
		soundName = "Sounds/interface:suit:suit_medical_repair";
		eSemantic = eSoundSemantic_NanoSuit;
		break;
	case ESound_SuitStrengthActivate:
		soundName = "Sounds/interface:suit:suit_strength_activate";
		eSemantic = eSoundSemantic_NanoSuit;
		break;
	case ESound_SuitSpeedActivate:
		soundName = "Sounds/interface:suit:suit_speed_activate";
		eSemantic = eSoundSemantic_NanoSuit;
		break;
	case ESound_SuitArmorActivate:
		soundName = "Sounds/interface:suit:suit_armor_activate";
		eSemantic = eSoundSemantic_NanoSuit;
		break;
	case ESound_SuitCloakActivate:
		soundName = "Sounds/interface:suit:suit_cloak_activate";
		eSemantic = eSoundSemantic_NanoSuit;
		break;
	case ESound_SuitCloakFeedback:
		soundName = "sounds/interface:hud:cloak_feedback";
		eSemantic = eSoundSemantic_NanoSuit;
		force3DSound = true;
		break;
	case ESound_GBootsActivated:
		soundName = "Sounds/interface:suit:suit_gravity_boots_activate";
		eSemantic = eSoundSemantic_NanoSuit;
		break;
	case ESound_GBootsDeactivated:
		soundName = "Sounds/interface:suit:suit_gravity_boots_deactivate";
		eSemantic = eSoundSemantic_NanoSuit;
		break;
	case ESound_ZeroGThruster:
		soundName = "Sounds/interface:suit:thrusters_1p";
		eSemantic = eSoundSemantic_NanoSuit;
		setParam = true;
		force3DSound = true;	//the thruster sound is only as 3D version available
		break;
	case ESound_AISuitHumming:
		soundName = "Sounds/interface:hud:nk_suit_feedback";
		eSemantic = eSoundSemantic_NanoSuit;
		break;
	case ESound_AISuitCloakFeedback:
		soundName = "Sounds/interface:hud:cloak_feedback";
		eSemantic = eSoundSemantic_NanoSuit;
		break;
	case ESound_GBootsLanded:
		soundName = "Sounds/physics:player_foley:bodyfall_gravity_boots";
		eSemantic = eSoundSemantic_Player_Foley;
		force3DSound = true;
		setParam = true;
		break;
	case ESound_FreeFall:
		soundName = "Sounds/physics:player_foley:falling_deep_loop";
		eSemantic = eSoundSemantic_Player_Foley;
		bAppendPostfix=false;
		break;
	case ESound_ColdBreath:
		soundName = "Sounds/physics:player_foley:cold_feedback";
		eSemantic = eSoundSemantic_Player_Foley;
		bAppendPostfix=false;
		break;
	case DROP_VS_THROW_SOUND:
		soundName = "sounds/interface:suit:suit_grab_vs_throw";
		eSemantic = eSoundSemantic_NanoSuit;
		bAppendPostfix = false;
	default:
		break;
	}

	if(!force3DSound && m_pOwner == m_pGameFramework->GetClientActor() && !m_pOwner->IsThirdPerson() && soundName.size())
	{
		if (bAppendPostfix)
			soundName.append("_fp");
	}
	
	IEntitySoundProxy* pSoundProxy = (IEntitySoundProxy*)m_pOwner->GetEntity()->CreateProxy(ENTITY_PROXY_SOUND);
	if (!pSoundProxy)
		return;

	if(soundName.size())		//get / create or stop sound
	{
		if(m_sounds[sound].ID != INVALID_SOUNDID)
		{
			pSound = pSoundProxy->GetSound(m_sounds[sound].ID);
			if(stopSound)
			{
				if(pSound)
					pSound->Stop();

				m_sounds[sound].ID = INVALID_SOUNDID;
				return;
			}
		}
		if(!pSound && !stopSound)
		{
			pSound = gEnv->pSoundSystem->CreateSound(soundName, soundFlag);

			if (pSound)
			{
				pSound->SetSemantic(eSemantic);
				float fTemp = 0.0f;
				m_sounds[sound].ID = pSound->GetId();
				m_sounds[sound].bLooping = (pSound->GetFlags() & FLAG_SOUND_LOOP) != 0;
				m_sounds[sound].b3D = (pSound->GetFlags() & FLAG_SOUND_3D) != 0;
				m_sounds[sound].nMassIndex = pSound->GetParam("mass", &fTemp, false);
				m_sounds[sound].nSpeedIndex = pSound->GetParam("speed", &fTemp, false);
				m_sounds[sound].nStrengthIndex = pSound->GetParam("strength", &fTemp, false);
			}
		}
	}

	if ( pSound )		//set params and play
	{
		//pSound->SetPosition(m_pOwner->GetEntity()->GetWorldPos());

		if(setParam)
		{
			if (m_sounds[sound].nMassIndex != -1)
				pSound->SetParam(m_sounds[sound].nMassIndex, param);
			
			if (m_sounds[sound].nSpeedIndex != -1)
				pSound->SetParam(m_sounds[sound].nSpeedIndex, param);

			if (m_sounds[sound].nStrengthIndex != -1)
				pSound->SetParam(m_sounds[sound].nStrengthIndex, param);
		}

		if(!(m_sounds[sound].bLooping && pSound->IsPlaying()))
		{
			pSoundProxy->PlaySound(pSound);
		}

	}
}

void CNanoSuit::Serialize(TSerialize ser, unsigned aspects)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("Nanosuit");
		ser.Value("nanoSuitEnergy", m_energy);
		ser.Value("nanoSuitActive", m_active);
		ser.Value("m_activationTime", m_activationTime);
		ser.Value("m_energyRechargeRate", m_energyRechargeRate);
		ser.Value("m_healthRegenRate", m_healthRegenRate);
		ser.Value("m_healthAccError", m_healthAccError);
		ser.Value("m_healTime", m_healTime);
		ser.Value("m_healthRegenDelay", m_healthRegenDelay);
		ser.Value("m_energyRechargeDelay", m_energyRechargeDelay);
		ser.Value("m_featureMask", m_featureMask);
		ser.Value("m_defenseHitTimer", m_defenseHitTimer);
		ser.EnumValue("currentMode", m_currentMode, NANOMODE_SPEED, NANOMODE_LAST);
		if(ser.IsReading())
			SetMode(m_currentMode, true);
		ser.EndGroup();

		ser.BeginGroup("NanoCloak");
		ser.Value("CloakActive", m_cloak.m_active);
		int mode = m_cloak.m_mode;
		ser.Value("CloakType", mode);
		ser.Value("CloakEnergyCost", m_cloak.m_energyCost);
		ser.Value("CloakHealthCost", m_cloak.m_healthCost);
		ser.Value("CloakVisDamp", m_cloak.m_visualDamp);
		ser.Value("CloakSoundDamp", m_cloak.m_soundDamp);
		ser.Value("CloakHeatDamp", m_cloak.m_heatDamp);
		ser.Value("HudMessage", m_cloak.m_HUDMessage);

		ser.Value("m_bNightVisionEnabled", m_bNightVisionEnabled);

		if(ser.IsReading())
		{
			m_cloak.m_mode = ENanoCloakMode(mode);
			if(m_cloak.IsActive())
			{
				SetCloak(false, true);
				SetCloak(true, true);
			}
		}
		ser.EndGroup();
	}
	else 
	{
		if (aspects&CPlayer::ASPECT_NANO_SUIT_SETTING)
		{
			uint8 mode = m_currentMode;
			ser.Value("mode", mode, 'ui3');
			if(ser.IsReading() && (mode != m_currentMode))
				SetMode((ENanoMode)mode);
		}
		if (aspects&CPlayer::ASPECT_NANO_SUIT_ENERGY)
		{
			ser.Value("energy", m_energy, 'nNRG');
			if (ser.IsReading())
				Balance(m_energy);
		}
		if (aspects&CPlayer::ASPECT_NANO_SUIT_INVULNERABLE)
		{
			bool invulnerable=m_invulnerable;
			ser.Value("invulnerable", invulnerable, 'bool');
			if (ser.IsReading() && (invulnerable!=m_invulnerable))
				SetInvulnerability(invulnerable);
		}
		if(aspects&CPlayer::ASPECT_NANO_SUIT_DEFENSE_HIT)
		{
			float oldTimer = m_defenseHitTimer;
			ser.Value("m_defenseHitTimer", m_defenseHitTimer);
			if(ser.IsReading())
			{
				if(oldTimer != m_defenseHitTimer)
					SelectSuitMaterial();
			}
		}
	}
}

bool CNanoSuit::Tap(ENanoAction nanoAction)
{
	if (!g_pGameCVars->dt_enable || !m_pOwner || m_pOwner->GetHealth()<=0)
		return false;

	// double taps don't work when using a mounted weapon
	CItem* pCurrentItem = static_cast<CItem*>(m_pOwner->GetCurrentItem());
	if (nanoAction == eNA_Jump && pCurrentItem && pCurrentItem->IsMounted())
		return false;

	static float lastActivation = -1.0f;

	float now = gEnv->pTimer->GetAsyncCurTime();
	float time = g_pGameCVars->dt_time;
	if (m_lastTap == eNA_Melee)
	{
		time = g_pGameCVars->dt_meleeTime;
	}
	if ((now - lastActivation) > time || nanoAction != m_lastTap)
	{
		lastActivation = now;
		m_lastTap = nanoAction;
		return false;
	}
	lastActivation = -1.0f;
	return AttemptAction(nanoAction);
}

bool CNanoSuit::AttemptAction(ENanoAction nanoAction)
{
	m_pendingAction = eNA_None;
	m_lastTap = eNA_None;
	switch(nanoAction)
	{
	case eNA_Jump:
		{
			SetMode(NANOMODE_STRENGTH);
			m_pendingAction = eNA_Jump;
		}
		break;
	case eNA_Forward:
		{
			SetMode(NANOMODE_SPEED);
		}
		break;
	case eNA_Backward:
		{
			SetMode(NANOMODE_DEFENSE);
		}
		break;
	case eNA_Crouch:
		{
			SetMode(NANOMODE_CLOAK);
		}
		break;
	case eNA_Melee:
		{
			SetMode(NANOMODE_STRENGTH);
		}
		break;
	case eNA_Skin:
		{
			SetMode(NANOMODE_CLOAK);
		}
		break;
	default:
		return false;
	}
	return true;
}

void CNanoSuit::ConsumeAction()
{
	m_pendingAction = eNA_None;
}

void CNanoSuit::Death()
{
	if (m_bWasSprinting)
	{
		if (m_bSprintUnderwater)
			PlaySound(SPEED_IN_WATER_SOUND, 0.0f, true);
		else
			PlaySound(SPEED_SOUND, 0.0f, true);

		if (m_bSprintUnderwater)
			PlaySound(SPEED_IN_WATER_SOUND_STOP);
		else
			PlaySound(SPEED_SOUND_STOP);
	}

	if (m_currentMode==NANOMODE_CLOAK)
		SetCloak(false, true);

	PlaySound(ESound_AISuitHumming, 0.0f, true);
	PlaySound(ESound_AISuitCloakFeedback, 0.0f, true);

	if(m_defenseHitTimer)
	{
		m_defenseHitTimer = 0.0f;
		SelectSuitMaterial();
	}
}

void CNanoSuit::AddListener(CNanoSuit::INanoSuitListener *pListener)
{
	stl::push_back_unique(m_listeners, pListener);
}

void CNanoSuit::RemoveListener(CNanoSuit::INanoSuitListener *pListener)
{
	stl::find_and_erase(m_listeners, pListener);
}

int CNanoSuit::GetButtonFromMode(ENanoMode mode)
{
	switch(mode)
	{
	case NANOMODE_SPEED:
		return EQM_SPEED;
		break;
	case NANOMODE_STRENGTH:
		return EQM_STRENGTH;
		break;
	case NANOMODE_CLOAK:
		return EQM_CLOAK;
		break;
	}
	return EQM_ARMOR;
}

void  CNanoSuit::Activate(bool activate, float activationTime)
{
	if (activate)
	{
		if (activationTime<0.01f)
		{
			m_active=true;
			m_activationTime=0.0f;
		}
		else
			m_activationTime=activationTime;
	}
	else
	{
		SetMode(NANOMODE_DEFENSE, false, true);
		m_active=false;
	}
}


void CNanoSuit::ActivateMode(ENanoMode mode, bool active)
{
	if(!m_pOwner)
		return;

	if(active)
	{
		CHUD* pHUD = g_pGame->GetHUD();
		if(pHUD && m_pOwner->IsClient() && !pHUD->IsQuickMenuButtonDefect(EQuickMenuButtons(GetButtonFromMode(mode))))
		{
			m_featureMask |= 1<<mode;
			pHUD->ActivateQuickMenuButton(EQuickMenuButtons(GetButtonFromMode(mode)), true);
		}
	}
	else
	{
		m_featureMask &= ~(1<<mode);
		CHUD* pHUD = g_pGame->GetHUD();
		if(pHUD && m_pOwner->IsClient())
			pHUD->ActivateQuickMenuButton(EQuickMenuButtons(GetButtonFromMode(mode)), false);
	}
}

void CNanoSuit::SetModeDefect(ENanoMode mode, bool defect)
{

	CHUD* pHUD = g_pGame->GetHUD();
	if(pHUD && m_pOwner->IsClient())
		pHUD->SetQuickMenuButtonDefect(EQuickMenuButtons(GetButtonFromMode(mode)), defect);

	if(defect)
	{
		if(IsModeActive(mode))
			ActivateMode(mode, false);
	}
	else
	{
		if(!IsModeActive(mode))
			ActivateMode(mode, true);
	}
}

float CNanoSuit::GetSprintMultiplier(bool strafing)
{
	if(m_pOwner && !m_pOwner->GetActorStats()->inZeroG && m_currentMode == NANOMODE_SPEED && m_startedSprinting)
	{
		if (gEnv->bMultiplayer)
		{
			if(m_energy >= 1.0f)
			{
				float time = m_now - m_startedSprinting;
				float speedMult = g_pGameCVars->g_suitSpeedMultMultiplayer;
				float result = 1.0f + max(0.0f, speedMult*min(1.3f, time*0.001f));

				if (strafing)
					result=MAX(1.0f, result*0.35f);

				return result;
			}
		}
		else
		{
			if(m_energy > NANOSUIT_ENERGY * 0.2f)
			{
				float time = m_now - m_startedSprinting;
				return 1.0f + max(0.3f, g_pGameCVars->g_suitSpeedMult*min(1.0f, time*0.001f));
			}
			else if(m_energy > 0.0f)
				return 1.4f;
			else
				return 1.3f;
		}
	}
	return 1.0f;
}

void CNanoSuit::UpdateSprinting(float &recharge, const SPlayerStats &stats, float frametime)
{
	if(!stats.inZeroG)
	{
		if (m_currentMode == NANOMODE_SPEED && stats.bSprinting)
		{
			if(m_energy > NANOSUIT_ENERGY * 0.2f)
			{
				if(!m_bWasSprinting)
				{
					m_bWasSprinting = true;
					if(stats.headUnderWaterTimer < 0.0f)
					{
						if(m_pOwner->GetStance() != STANCE_PRONE)
							PlaySound(SPEED_SOUND);

						m_bSprintUnderwater = false;
					}
					else
					{
						PlaySound(SPEED_IN_WATER_SOUND);
						m_bSprintUnderwater = true;
					}
				}
				else
				{
					//when we sprinted into the water -> change sound
					if((stats.headUnderWaterTimer > 0.0f) && m_bSprintUnderwater)
					{
						PlaySound(SPEED_IN_WATER_SOUND, 0.0, true);
						PlaySound(SPEED_SOUND);
					}
					else if((stats.headUnderWaterTimer > 0.0f) && !m_bSprintUnderwater)
					{
						PlaySound(SPEED_SOUND, 0.0, true);
						PlaySound(SPEED_IN_WATER_SOUND);
					}
				}

				//recharge -= std::max(1.0f, g_pGameCVars->g_suitSpeedEnergyConsumption*frametime);
				float consumption=gEnv->bMultiplayer?g_pGameCVars->g_suitSpeedEnergyConsumptionMultiplayer:g_pGameCVars->g_suitSpeedEnergyConsumption;
				recharge -= m_pOwner->ShouldSwim()?consumption*1.25f:consumption;
			}
			else
			{
				if(m_bWasSprinting)
				{
					PlaySound(SPEED_SOUND, 0.0f, true);
					PlaySound(SPEED_IN_WATER_SOUND, 0.0f, true);
					if(stats.headUnderWaterTimer < 0.0f)
						PlaySound(SPEED_SOUND_STOP);
					else
						PlaySound(SPEED_IN_WATER_SOUND_STOP);
					m_bWasSprinting = false;
				}
				recharge -= 28.0f;
			}

			if(!m_startedSprinting)
				m_startedSprinting = m_now;
		}
		else if(m_bWasSprinting)
		{
			PlaySound(SPEED_SOUND, 0.0f, true);
			PlaySound(SPEED_IN_WATER_SOUND, 0.0f, true);
			if(stats.headUnderWaterTimer < 0.0f)
				PlaySound(SPEED_SOUND_STOP);
			else
				PlaySound(SPEED_IN_WATER_SOUND_STOP);

			m_bWasSprinting = false;
			m_startedSprinting = 0;
		}
		else if(gEnv->bMultiplayer)	//fix me : in mp the running loop apparently can get out of sync
			PlaySound(SPEED_SOUND, 0.0f, true);
	}
}

void CNanoSuit::ResetEnergy()
{
	SetSuitEnergy(NANOSUIT_ENERGY);
	m_energy = m_lastEnergy = NANOSUIT_ENERGY;
	for(int i = 0; i < NANOSLOT_LAST; i++)
		m_slots[i].realVal = m_slots[i].desiredVal;

	if (m_pOwner && gEnv->bServer)
		m_pOwner->GetGameObject()->ChangedNetworkState(CPlayer::ASPECT_NANO_SUIT_ENERGY);
}


void CNanoSuit::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_COMPONENT_NAME(s, "NanoSuit");
	s->Add(*this);
	s->AddContainer(m_listeners);
	m_cloak.GetMemoryStatistics(s);
}