// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include <ISound.h>
#include "HUD.h"
#include "../Actor.h"
#include "../Game.h"

void CHUD::PlaySound(ESound eSound, bool play)
{
	if(!gEnv->pSoundSystem)
		return;

	const char *szSound = NULL;

	switch(eSound)
	{
	case ESound_GenericBeep:
		szSound = "Sounds/interface:suit:generic_beep";
		break;
	case ESound_PresetNavigationBeep:
		szSound = "Sounds/interface:suit:preset_navigation_beep";
		break;
	case ESound_TemperatureBeep:
		szSound = "Sounds/interface:suit:temperature_beep";
		break;
	case ESound_SuitMenuAppear:
		szSound = "sounds/interface:suit:modification_menu_appear";
		break;
	case ESound_SuitMenuDisappear:
		szSound = "Sounds/interface:suit:modification_menu_disappear";
		break;
	case ESound_WeaponModification:
		szSound = "Sounds/interface:suit:weapon_modification_beep";
		break;
	case ESound_BinocularsZoomIn:
		szSound = "Sounds/interface:suit:binocular_zoom_in";
		break;
	case ESound_BinocularsZoomOut:
		szSound = "Sounds/interface:suit:binocular_zoom_out";
		break;
	case ESound_BinocularsSelect:
		szSound = "Sounds/interface:suit:binocular_select";
		break;
	case ESound_BinocularsDeselect:
		szSound = "Sounds/interface:suit:binocular_deselect";
		break;
	case ESound_BinocularsAmbience:
		szSound = "Sounds/interface:suit:binocular_ambience";
		break;
	case ESound_NightVisionSelect:
		szSound = "Sounds/interface:suit:night_vision_select";
		break;
	case ESound_NightVisionDeselect:
		szSound = "Sounds/interface:suit:night_vision_deselect";
		break;
	case ESound_NightVisionAmbience:
		szSound = "Sounds/interface:suit:night_vision_ambience";
		break;
	case ESound_BinocularsLock:
		szSound = "Sounds/interface:suit:binocular_target_locked";
		break;
	case ESound_OpenPopup:
		szSound = "Sounds/interface:menu:pop_up";
		break;
	case ESound_ClosePopup:
		szSound = "Sounds/interface:menu:close";
		break;
	case ESound_MapOpen:
		szSound = "Sounds/interface:hud:hud_map_open";
		break;
	case ESound_MapClose:
		szSound = "Sounds/interface:hud:hud_map_close";
		break;
	case ESound_TabChanged:
		szSound = "Sounds/interface:menu:screen_change";
		break;
	case ESound_WaterDroplets:
		szSound = "Sounds/interface:hud:hud_water";
		break;
	case ESound_BuyBeep:
		szSound = "Sounds/interface:menu:buy_beep";
		break;
	case ESound_BuyError:
		szSound = "Sounds/interface:menu:buy_error";
		break;
	case ESound_SniperZoomIn:
		szSound = "Sounds/interface:hud:sniper_scope_zoom_in";
		break;
	case ESound_SniperZoomOut:
		szSound = "Sounds/interface:hud:sniper_scope_zoom_out";
		break;
	case ESound_Highlight:
		szSound = "sounds/interface:menu:rollover";
		break;
	case ESound_Select:
		szSound = "sounds/interface:menu:click1";
		break;
	case ESound_ObjectiveUpdate:
		szSound = "sounds/interface:hud:pda_update";
		break;
	case ESound_ObjectiveComplete:
		szSound = "sounds/interface:hud:objective_completed";
		break;
	case ESound_GameSaved:
		szSound = "sounds/interface:menu:confirm";
		break;
	case ESound_Target_Lock:
		szSound = "sounds/interface:hud:vehicle_target_lock";
		break;
	case ESound_Malfunction:
		szSound = "sounds/interface:hud:hud_malfunction";
		break;
	case ESound_Reboot:
		szSound = "sounds/interface:hud:hud_reboot";
		break;
	case ESound_ReActivate:
		szSound = "sounds/interface:hud:hud_activate";
		break;
	case ESound_VehicleIn:
		szSound = "sounds/interface:hud:hud_vehicle_in";
		break;
	case ESound_LawLocking:
		szSound = "sounds/interface:hud:hud_law_locking";
		break;
	case ESound_DownloadStart:
		szSound = "sounds/interface:hud:hud_download_start";
		break;
	case ESound_DownloadLoop:
		szSound = "sounds/interface:hud:hud_download_loop";
		break;
	case ESound_DownloadStop:
		szSound = "sounds/interface:hud:hud_download_stop";
		break;
	case ESound_SpecialHitFeedback:
		szSound = "sounds/interface:hud:hud_explo_feedback";
		break;
	default:
		assert(0);
		return;
	}

	if(play)
	{
		_smart_ptr<ISound> pSound = gEnv->pSoundSystem->CreateSound(szSound,0);
		if(pSound)
		{
			if(m_soundIDs[eSound])
			{
				ISound *pOldSound = gEnv->pSoundSystem->GetSound(m_soundIDs[eSound]);

				if(pOldSound)
					pOldSound->Stop(ESoundStopMode_AtOnce);

				m_soundIDs[eSound] = 0;
			}

			pSound->SetSemantic(eSoundSemantic_HUD);
			pSound->Play();
			m_soundIDs[eSound] = pSound->GetId();
		}
	}
	else if(m_soundIDs[eSound] != INVALID_SOUNDID)
	{
		ISound *pSound = gEnv->pSoundSystem->GetSound(m_soundIDs[eSound]);
		if(pSound)
		{
			pSound->Stop();
			m_soundIDs[eSound] = INVALID_SOUNDID;
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUD::PlayStatusSound(const char* name, bool forceSuitSound)
{
	if(!gEnv->pSoundSystem)
		return;

	//string strSound("languages/dialog/suit/");
	string strSound;

	//we don't want to play the suit sounds at once (because of quick mode change key)
	if(!forceSuitSound &&
			(!stricmp(name, "normal_cloak_on") ||
			!stricmp(name, "maximum_speed") ||
			!stricmp(name, "maximum_strength") ||
			!stricmp(name, "maximum_armor")))
	{
		// look for "m_fSuitChangeSoundTimer"
		return;
	}

	switch(m_iVoiceMode)
	{
	case 1: //male
		strSound.append("suit/male/suit_");
		break;
	case 2: //female
		strSound.append("suit/female/suit_");
		break;
	case 3: //michaelR
		strSound.append("suit/mr/suit_");
		break;
	default:
		return;
		break;
	}

	//VS2 workaround ...
	if(!strcmp(name, "normal_cloak_on"))
		strSound.append("modification_engaged");
	else if(!strcmp(name, "normal_cloak_off"))
		return;
	else
		strSound.append(name);

	_smart_ptr<ISound> pSound = gEnv->pSoundSystem->CreateSound(strSound,FLAG_SOUND_3D|FLAG_SOUND_VOICE);
	if ( pSound )
	{
		pSound->SetSemantic(eSoundSemantic_NanoSuit);
		pSound->SetPosition(g_pGame->GetIGameFramework()->GetClientActor()->GetEntity()->GetWorldPos());
		pSound->Play();
	}
}
