/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Flash menu screens manager

-------------------------------------------------------------------------
History:
- 07:18:2006: Created by Julien Darre
- 2007 : Taken over by Jan Neugebauer

*************************************************************************/
#include "StdAfx.h"
#include <StlUtils.h>

#include <IVideoPlayer.h>
#include <time.h>

#include "FlashMenuObject.h"
#include "FlashMenuScreen.h"
#include "IGameFramework.h"
#include "IPlayerProfiles.h"
#include "IUIDraw.h"
#include "IMusicSystem.h"
#include "ISound.h"
#include "Game.h"
#include "GameCVars.h"
#include <CryPath.h>
#include <ISaveGame.h>
#include <ILoadGame.h>
#include "MPHub.h"
#include "HUD/HUD.h"
#include "HUD/HUDRadar.h"
#include "HUD/HUDTextChat.h"
#include "OptionsManager.h"
#include "IAVI_Reader.h"
#include <StringUtils.h>
#include "IActionMapManager.h"
#include "GameActions.h"
#include "IViewSystem.h"
#include "LaptopUtil.h"
#include "GameNetworkProfile.h"
#include "SPAnalyst.h"

//both are defined again in FlashMenuObjectOptions
static const char* scuiControlCodePrefix = "@cc_"; // "@cc_"; // AlexL 03/04/2007: enable this when keys/controls are fully localized
static const size_t scuiControlCodePrefixLen = strlen(scuiControlCodePrefix);

//-----------------------------------------------------------------------------------------------------

static const int BLACK_FRAMES = 4;

static TKeyValuePair<CFlashMenuObject::EEntryMovieState,const char*>
gMovies[] = {
	{CFlashMenuObject::eEMS_Start,""},
	{CFlashMenuObject::eEMS_SPDEMO,"Localized/Video/Trailer_DemoLegal.sfd"},
	{CFlashMenuObject::eEMS_EA,"Localized/Video/Trailer_EA.sfd"},
	{CFlashMenuObject::eEMS_Crytek,"Localized/Video/Trailer_Crytek.sfd"},
	{CFlashMenuObject::eEMS_NVidia,"Localized/Video/Trailer_NVidia.sfd"},
	{CFlashMenuObject::eEMS_Intel,"Localized/Video/Trailer_Intel.sfd"},
	{CFlashMenuObject::eEMS_PEGI,"Localized/Video/Trailer_PEGI.sfd"},
	{CFlashMenuObject::eEMS_RatingLogo,"Localized/Video/Trailer_Rating_Logo.sfd"},
	{CFlashMenuObject::eEMS_Rating,"Localized/Video/%s/Trailer_Rating.sfd"},
	{CFlashMenuObject::eEMS_Legal,"Localized/Video/Trailer_CrytekC.sfd"},
	{CFlashMenuObject::eEMS_Stop,""},
	{CFlashMenuObject::eEMS_Done,"Localized/Video/bg.sfd"},
	{CFlashMenuObject::eEMS_GameStart,""},
	{CFlashMenuObject::eEMS_GameIntro,"Localized/Video/Intro.sfd"},
	{CFlashMenuObject::eEMS_GameStop,""}, 
	{CFlashMenuObject::eEMS_GameDone,""}
};

static TKeyValuePair<CFlashMenuObject::EEntryMovieState,int>
gSkip[] = {
	{CFlashMenuObject::eEMS_Start,0},
	{CFlashMenuObject::eEMS_SPDEMO,2},
	{CFlashMenuObject::eEMS_EA,2},
	{CFlashMenuObject::eEMS_Crytek,1},
	{CFlashMenuObject::eEMS_NVidia,1},
	{CFlashMenuObject::eEMS_Intel,1},
	{CFlashMenuObject::eEMS_PEGI,2},
	{CFlashMenuObject::eEMS_RatingLogo,2},
	{CFlashMenuObject::eEMS_Rating,2},
	{CFlashMenuObject::eEMS_Legal,2},
	{CFlashMenuObject::eEMS_Stop,0},
	{CFlashMenuObject::eEMS_Done,0},
	{CFlashMenuObject::eEMS_GameStart,0},
	{CFlashMenuObject::eEMS_GameIntro,1},
	{CFlashMenuObject::eEMS_GameStop,0},
	{CFlashMenuObject::eEMS_GameDone,0}
};







//-----------------------------------------------------------------------------------------------------

CFlashMenuObject *CFlashMenuObject::s_pFlashMenuObject = NULL;

//-----------------------------------------------------------------------------------------------------

CFlashMenuObject::CFlashMenuObject()
: m_pFlashPlayer(0)
, m_pVideoPlayer(0)
, m_multiplayerMenu(0)
{
	s_pFlashMenuObject = this;

	for(int i=0; i<MENUSCREEN_COUNT; i++)
	{
		m_apFlashMenuScreens[i] = NULL;
	}

	m_pCurrentFlashMenuScreen	= NULL;
	m_pSubtitleScreen = NULL;
	m_pAnimLaptopScreen = NULL;
	m_pPlayerProfileManager = NULL;
	m_bControllerConnected = false;
	m_iGamepadsConnected = 0;
	m_splashScreenTimer = 0.0f;

	for(int iSound=0; iSound<ESound_Last; iSound++)
	{
		m_soundIDs[iSound] = INVALID_SOUNDID;
	}

	// THIS IS VERY VERY BAD -> FIX IT!
	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated()) return;

	m_bVKMouseDown = false;

	m_iMaxProgress = 100;
	m_iWidth = m_iHeight = 0;
	m_bNoMoveEnabled = false;
	m_bNoMouseEnabled	= false;
	m_bLanQuery = false;
	m_bIgnoreEsc = true;
	m_bUpdate = true;
	m_bVirtualKeyboardFocus = false;
	m_nBlackGraceFrames = 0;
	m_nLastFrameUpdateID = 0;
	m_nRenderAgainFrameId = 0;
	m_bColorChanged = true;
	m_bIsEndingGameContext = false;

	m_bClearScreen = false;
	m_bCatchNextInput = false;

	m_bDestroyStartMenuPending = false;
	m_bDestroyInGameMenuPending = false;
	m_bIgnorePendingEvents = false;

	m_stateEntryMovies = eEMS_Start;
	m_eSaveGameCompareMode = eSAVE_COMPARE_DATE;
	m_bSaveGameSortUp = true;
	m_bInLoading = false;
	m_bLoadingDone = false;
	m_bTutorialVideo = false;

	m_resolutionTimer = 0.0f;
	m_fMusicFirstTime = -1.0f;

	m_textfieldFocus = false;

	m_tempWidth = gEnv->pRenderer->GetWidth();
	m_tempHeight = gEnv->pRenderer->GetHeight();

	SAFE_HARDWARE_MOUSE_FUNC(AddListener(this));

	if (gEnv->pInput) gEnv->pInput->AddEventListener(this);

	gEnv->pGame->GetIGameFramework()->RegisterListener(this, "flashmenu", FRAMEWORKLISTENERPRIORITY_MENU);
	gEnv->pGame->GetIGameFramework()->GetILevelSystem()->AddListener(this);

	if(gEnv->pCryPak->GetLvlResStatus())
	{
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING] = new CFlashMenuScreen;
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Load("Libs/UI/Menus_Loading_MP.gfx");
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET] = new CFlashMenuScreen;
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->Load("Libs/UI/HUD_MP_RestartScreen.gfx");
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART] = new CFlashMenuScreen;
#ifdef CRYSIS_BETA
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Load("Libs/UI/Menus_StartMenu_Beta.gfx");
#else
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Load("Libs/UI/Menus_StartMenu.gfx");
#endif
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME] = new CFlashMenuScreen;

#ifdef CRYSIS_BETA
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Load("Libs/UI/Menus_IngameMenu_Beta.gfx");
#else
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Load("Libs/UI/Menus_IngameMenu.gfx");
#endif

		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Unload();
		SAFE_DELETE(m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]);
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->Unload();
		SAFE_DELETE(m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]);
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Unload();
		SAFE_DELETE(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]);
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Unload();
		SAFE_DELETE(m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]);
	}

	// Laptop Gaming TDK returns -1 when functions failed
	m_ulBatteryLifeTime		= -1;
	m_iBatteryLifePercent	= -1;
	m_iWLanSignalStrength	= -1;
	m_fLaptopUpdateTime = gEnv->pTimer->GetAsyncTime().GetSeconds();
	m_bForceLaptopUpdate = true;

	if(SAFE_LAPTOPUTIL_FUNC_RET(IsLaptop()))
	{
		m_pAnimLaptopScreen = new CFlashMenuScreen;
		m_pAnimLaptopScreen->Load("Libs/UI/HUD_Battery.gfx");
		m_pAnimLaptopScreen->SetDock(eFD_Right);
		m_pAnimLaptopScreen->RepositionFlashAnimation();
	}

	m_pMusicSystem = gEnv->pSystem->GetIMusicSystem();

	m_multiplayerMenu = new CMPHub();
	m_bExclusiveVideo = false;

	if(gEnv->bEditor)
		LoadDifficultyConfig(2);	//set normal diff in editor

	// create the avi reader; 
	//m_pAVIReader = g_pISystem->CreateAVIReader();
	//m_pAVIReader->OpenFile("Crysis_main_menu_background.avi");





}

//-----------------------------------------------------------------------------------------------------

CFlashMenuObject::~CFlashMenuObject()
{
  SAFE_DELETE(m_multiplayerMenu);

	SAFE_RELEASE(m_pFlashPlayer);
	SAFE_RELEASE(m_pVideoPlayer);
	SAFE_DELETE(m_pSubtitleScreen);
	SAFE_DELETE(m_pAnimLaptopScreen);

	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated()) return;

	gEnv->pGame->GetIGameFramework()->UnregisterListener(this);
	gEnv->pGame->GetIGameFramework()->GetILevelSystem()->RemoveListener(this);

	if (gEnv->pInput) gEnv->pInput->RemoveEventListener(this);

	SAFE_HARDWARE_MOUSE_FUNC(RemoveListener(this));

	DestroyStartMenu();
	DestroyIngameMenu();

	for(int i=0; i<MENUSCREEN_COUNT; i++)
	{
		SAFE_DELETE(m_apFlashMenuScreens[i]);
	}
  
	m_pCurrentFlashMenuScreen	= NULL;
	
	s_pFlashMenuObject = NULL;

	// g_pISystem->ReleaseAVIReader(m_pAVIReader);
}

//-----------------------------------------------------------------------------------------------------

CFlashMenuObject *CFlashMenuObject::GetFlashMenuObject()
{
	return s_pFlashMenuObject;
}


//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::StartSplashScreenCountDown()
{
	if(m_splashScreenTimer>0.0f) return;
	m_splashScreenTimer = 10.0f;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::StartResolutionCountDown()
{
	if(m_resolutionTimer>0.0f) return;
	m_iOldWidth = m_iWidth;
	m_iOldHeight = m_iHeight;
	m_resolutionTimer = 15.0f;
	if(m_pCurrentFlashMenuScreen)
	{
		m_pCurrentFlashMenuScreen->Invoke("showErrorMessageYesNo","resolution_countdown");
		m_pCurrentFlashMenuScreen->Invoke("setErrorTextNonLocalized","15");
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::UpdateRatio()
{
	for(int i=0; i<MENUSCREEN_COUNT; i++)
	{
		if(m_apFlashMenuScreens[i])
		{
			m_apFlashMenuScreens[i]->UpdateRatio();
		}
	}

	if(m_pAnimLaptopScreen)
	{
		m_pAnimLaptopScreen->RepositionFlashAnimation();
	}

	StopVideo();

	const char* movie = VALUE_BY_KEY(m_stateEntryMovies, gMovies);
	if(movie)
	{
		if(m_stateEntryMovies == eEMS_Done)
			PlayVideo(movie, false, IVideoPlayer::LOOP_PLAYBACK);
		else
			PlayVideo(movie, true);
	}

	m_iWidth = gEnv->pRenderer->GetWidth();
	m_iHeight = gEnv->pRenderer->GetHeight();
}

//-----------------------------------------------------------------------------------------------------

bool CFlashMenuObject::ColorChanged()
{
	if(m_bColorChanged)
	{
		m_bColorChanged = false;
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::SetColorChanged()
{
	m_bColorChanged = true;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::PlaySound(ESound eSound,bool bPlay)
{
	if(!gEnv->pSoundSystem) return;

	const char *szSound = NULL;

	ESoundSemantic eSoundSemantic = eSoundSemantic_None;
	uint32 nFlags = 0;

	switch(eSound)
	{
	case ESound_RollOver:
		szSound = "Sounds/interface:menu:rollover";
		break;
	case ESound_Click1:
		szSound = "Sounds/interface:menu:click1";
		break;
	case ESound_Click2:
		szSound = "Sounds/interface:menu:click2";
		break;
	case ESound_ScreenChange:
		szSound = "Sounds/interface:menu:screen_change";
		break;
	case ESound_MenuHighlight:
		szSound = "sounds/interface:menu:rollover";
		break;
	case ESound_MainHighlight:
		szSound = "sounds/interface:menu:main_rollover";
		break;
	case ESound_MenuSelect:
		szSound = "sounds/interface:menu:confirm";
		break;
	case ESound_MenuSelectDialog:
		szSound = "village/nomad_village_ab1_335EBA78";
		nFlags = FLAG_SOUND_VOICE;	
		break;
	case ESound_MenuClose:
		szSound = "sounds/interface:menu:close";
		break;
	case ESound_MenuOpen:
		szSound = "sounds/interface:menu:open";
		break;
	case ESound_MenuStart:
		szSound = "sounds/interface:menu:main_start";
		break;
	case ESound_MenuFirstOpen:
		szSound = "sounds/interface:menu:main_open";
		break;
	case ESound_MenuFirstClose:
		szSound = "sounds/interface:menu:main_close";
		break;
	case ESound_MenuWarningOpen:
		szSound = "sounds/interface:menu:quit_open";
		break;
	case ESound_MenuWarningClose:
		szSound = "sounds/interface:menu:quit_close";
		break;
	case Esound_MenuDifficulty:
		szSound = "sounds/interface:menu:difficulty";
		break;
	case Esound_MenuCheckbox:
		szSound = "sounds/interface:menu:flag";
		break;
	case ESound_MenuSlider:
		szSound = "sounds/interface:menu:slider";
		break;
	case ESound_MenuDropDown:
		szSound = "sounds/interface:menu:dropdown";
		break;
	case ESound_MenuAmbience:
		szSound = "sounds/interface:menu:main_ambience";
		eSoundSemantic = eSoundSemantic_Ambience;
		break;
	default:
		assert(0);
		return;
	}

	if(bPlay)
	{
		if (m_soundIDs[eSound] != INVALID_SOUNDID)
		{
			ISound* pOldSound = gEnv->pSoundSystem->GetSound(m_soundIDs[eSound]);
			if (pOldSound)
				pOldSound->Stop();

			m_soundIDs[eSound] = INVALID_SOUNDID;
		}

		_smart_ptr<ISound> pSound = gEnv->pSoundSystem->CreateSound(szSound, nFlags);
		if(pSound)
		{
			pSound->SetSemantic(eSoundSemantic);
			m_soundIDs[eSound] = pSound->GetId();
			pSound->Play();

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

SFlashKeyEvent CFlashMenuObject::MapToFlashKeyEvent(const SInputEvent &inputEvent)
{
	assert(inputEvent.deviceId == eDI_Keyboard);
	// at some point we should also support eIS_Down to make text input more convenient (repeating cursor events, backspace, etc)!
	assert(inputEvent.state == eIS_Pressed || inputEvent.state == eIS_Released || inputEvent.state == eIS_UI);

	SFlashKeyEvent::EKeyCode keyCode(SFlashKeyEvent::VoidSymbol);
	unsigned char asciiCode(0);

	// !!! mapping of keycodes in the following switch statement and 
	//     retrieval of the ascii and unicode character needs overhaul !!!

	if (inputEvent.state != eIS_UI)
	{
		EKeyId keyId(inputEvent.keyId);
		switch (keyId)
		{
		case eKI_Backspace: keyCode = SFlashKeyEvent::Backspace; break;
		case eKI_Tab:				keyCode = SFlashKeyEvent::Tab;			break;
		case eKI_Enter:			keyCode = SFlashKeyEvent::Return;		break;
		case eKI_LShift:
		case eKI_RShift:		keyCode = SFlashKeyEvent::Shift;		break;
		case eKI_LCtrl:
		case eKI_RCtrl:			keyCode = SFlashKeyEvent::Control;	break;
		case eKI_LAlt:
		case eKI_RAlt:			keyCode = SFlashKeyEvent::Alt;			break;
		case eKI_Escape:		keyCode = SFlashKeyEvent::Escape;		break;
		case eKI_PgUp:			keyCode = SFlashKeyEvent::PageUp;		break;
		case eKI_PgDn:			keyCode = SFlashKeyEvent::PageDown; break;
		case eKI_End:				keyCode = SFlashKeyEvent::End;			break;
		case eKI_Home:			keyCode = SFlashKeyEvent::Home;			break;
		case eKI_Left:			keyCode = SFlashKeyEvent::Left;			break;
		case eKI_Up:				keyCode = SFlashKeyEvent::Up;				break;
		case eKI_Right:			keyCode = SFlashKeyEvent::Right;		break;
		case eKI_Down:			keyCode = SFlashKeyEvent::Down;			break;
		case eKI_Insert:		keyCode = SFlashKeyEvent::Insert;		break;
		case eKI_Delete:		keyCode = SFlashKeyEvent::Delete;		break;
		}
	}

	unsigned char specialKeyState(0);
	specialKeyState |= ((inputEvent.modifiers & (eMM_LShift | eMM_RShift)) != 0) ? SFlashKeyEvent::eShiftPressed : 0;
	specialKeyState |= ((inputEvent.modifiers & (eMM_LCtrl | eMM_RCtrl)) != 0) ? SFlashKeyEvent::eCtrlPressed : 0;
	specialKeyState |= ((inputEvent.modifiers & (eMM_LAlt | eMM_RAlt)) != 0) ? SFlashKeyEvent::eAltPressed : 0;
	specialKeyState |= ((inputEvent.modifiers & eMM_CapsLock) != 0) ? SFlashKeyEvent::eCapsToggled : 0;
	specialKeyState |= ((inputEvent.modifiers & eMM_NumLock) != 0) ? SFlashKeyEvent::eNumToggled : 0;
	specialKeyState |= ((inputEvent.modifiers & eMM_ScrollLock) != 0) ? SFlashKeyEvent::eScrollToggled : 0;

	if (inputEvent.state == eIS_UI)
	{
		keyCode = SFlashKeyEvent::A; // !!! workaround !!! We should properly map key code here, too!
		return SFlashKeyEvent(SFlashKeyEvent::eKeyDown, keyCode, specialKeyState, inputEvent.keyName[0], inputEvent.timestamp);
	}
	else
	{
		return SFlashKeyEvent(inputEvent.state == eIS_Pressed ? SFlashKeyEvent::eKeyDown : SFlashKeyEvent::eKeyUp, keyCode, specialKeyState, asciiCode, asciiCode);
	}
}

//-----------------------------------------------------------------------------------------------------

bool CFlashMenuObject::OnInputEvent(const SInputEvent &rInputEvent)
{
	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated() || rInputEvent.keyId == eKI_SYS_Commit)
		return false;

	if(rInputEvent.deviceId==eDI_Mouse || rInputEvent.deviceId==eDI_Keyboard)
	{
		if(rInputEvent.state==eIS_Pressed)
			m_bVKMouseDown = true;
		else if(rInputEvent.state==eIS_Released)
			m_bVKMouseDown = false;
	}

	if(m_bVirtualKeyboardFocus && m_bUpdate && m_pCurrentFlashMenuScreen)
	{
		if(rInputEvent.deviceId==eDI_Keyboard || rInputEvent.deviceId==eDI_Mouse)
		{
			if (gEnv->pHardwareMouse)
			{
				gEnv->pHardwareMouse->IncrementCounter();
			}
			m_bVirtualKeyboardFocus = false;
			//user is using keyboard, we don't need the virtual keyboard anymore
			m_pCurrentFlashMenuScreen->Invoke("enableVirtualKeyboard",false);
		}
	}

	if(m_bLoadingDone)
	{
		if(g_pGame->GetIGameFramework())
		{
			if((rInputEvent.deviceId == eDI_Mouse || rInputEvent.deviceId == eDI_XI //GC2007 : press any button/key
				|| rInputEvent.deviceId == eDI_Keyboard) && rInputEvent.state == eIS_Released)
			{
				if (gEnv->pConsole->IsOpened())
					return false;
				CloseWaitingScreen();
				return true;
			}
		}
	}

  if(m_pCurrentFlashMenuScreen && m_bCatchNextInput && !gEnv->pConsole->GetStatus())
	{
		if(eIS_Pressed == rInputEvent.state)
		{
			const char* key = rInputEvent.keyName.c_str();

			const bool bGamePad = false;
			if (rInputEvent.deviceId == eDI_Keyboard || rInputEvent.deviceId == eDI_Mouse)
			{
				if(rInputEvent.keyId==eKI_Escape)
				{
					m_pCurrentFlashMenuScreen->Invoke("_root.Root.MainMenu.PressBtnDialog.gotoAndPlay","close");
				}
				else
				{
					CryFixedStringT<64> ui_key (scuiControlCodePrefix, scuiControlCodePrefixLen);
					ui_key+=key;
					SFlashVarValue args[3] = {m_sActionMapToCatch.c_str(), m_sActionToCatch.c_str(), ui_key.c_str()};
					m_pCurrentFlashMenuScreen->Invoke("Root.MainMenu.Options.updateAction", args, 3);
				}
				m_sActionMapToCatch.clear();
				m_sActionToCatch.clear();
				m_bCatchNextInput = false;
			}
			return false;
		}
	}

	if(IsActive())
	{
		//handling skip and back stuff
		if(!gEnv->pConsole->GetStatus() && eIS_Pressed == rInputEvent.state)
		{
			bool check = false;
			//skip movies
			if(	rInputEvent.keyId == eKI_Space ||
					rInputEvent.keyId == eKI_Escape ||
					rInputEvent.keyId == eKI_XI_Start || 
					rInputEvent.keyId == eKI_PS3_Start)
			{
				if(m_stateEntryMovies!=eEMS_Done && m_stateEntryMovies!=eEMS_Stop)
				{
					int skip = VALUE_BY_KEY(m_stateEntryMovies, gSkip);
					bool firstStart = g_pGame->GetOptions()->IsFirstStart();
					if(skip==0 || ((skip==1 && !firstStart) || gEnv->pSystem->IsDevMode()))
						StopVideo();
					check = true;
				}
				if(!check && m_bTutorialVideo)
					check = StopTutorialVideo();
			}
			if(	!check && (	rInputEvent.keyId == eKI_PS3_Start ||
											rInputEvent.keyId == eKI_XI_Start))
			{
				if(!m_bTutorialVideo && m_pCurrentFlashMenuScreen)
					m_pCurrentFlashMenuScreen->CheckedInvoke("onOk");
			}
			if(	!check && (	rInputEvent.keyId == eKI_Escape ||
											rInputEvent.keyId == eKI_XI_Back ||
											rInputEvent.keyId == eKI_XI_B ||
											rInputEvent.keyId == eKI_PS3_Select))
			 {
				if(!m_bTutorialVideo && m_pCurrentFlashMenuScreen)
					m_pCurrentFlashMenuScreen->CheckedInvoke("onBack");
				check = true;
			}

			if(	rInputEvent.keyId == eKI_Tab)
			{
				if(m_pCurrentFlashMenuScreen)
					m_pCurrentFlashMenuScreen->CheckedInvoke("onTab");
				check = true;
			}

			if(m_textfieldFocus && eKI_Enter == rInputEvent.keyId)
			{
				if(m_pCurrentFlashMenuScreen)
					m_pCurrentFlashMenuScreen->CheckedInvoke("onEnter","keyboard");
				check = true;
			}

			if(check) return true;
		}
	}

	if(IsActive())
	{
		if(rInputEvent.deviceId==eDI_Mouse && rInputEvent.state==eIS_Pressed)
		{
			if(rInputEvent.keyId==eKI_MouseWheelDown)
			{
				if(m_pCurrentFlashMenuScreen)
				{
					m_pCurrentFlashMenuScreen->Invoke("mouseWheelDown");
				}
			}
			else if(rInputEvent.keyId==eKI_MouseWheelUp)
			{
				if(m_pCurrentFlashMenuScreen)
				{
					m_pCurrentFlashMenuScreen->Invoke("mouseWheelUp");
				}
			}
		}
	}

	if(eDI_Keyboard == rInputEvent.deviceId)
	{
		if(gEnv->pConsole->GetStatus())
		{
			m_repeatEvent.keyId = eKI_Unknown;
			return false;
		}

		if(m_bUpdate && (eIS_Pressed == rInputEvent.state || eIS_Released == rInputEvent.state))
		{
			if (rInputEvent.state == eIS_Released)
				m_repeatEvent.keyId = eKI_Unknown;
			else
			{
				float repeatDelay = 200.0f;
				float now = gEnv->pTimer->GetAsyncTime().GetMilliSeconds();

				m_repeatTimer = now+repeatDelay;
				m_repeatEvent = rInputEvent;
			}


			SFlashKeyEvent keyEvent(MapToFlashKeyEvent(rInputEvent));

			if (m_pCurrentFlashMenuScreen && m_pCurrentFlashMenuScreen->GetFlashPlayer())
			{
				if(eIS_Pressed == rInputEvent.state)
					m_pCurrentFlashMenuScreen->CheckedInvoke("onPressedKey", rInputEvent.keyName.c_str());
				m_pCurrentFlashMenuScreen->GetFlashPlayer()->SendKeyEvent(keyEvent);
			}

			if (m_pFlashPlayer)
				m_pFlashPlayer->SendKeyEvent(keyEvent);
		}
	}
	else if(eDI_XI == rInputEvent.deviceId)		//x-gamepad controls
	{
		int oldGamepads = m_iGamepadsConnected;
		if(rInputEvent.keyId == eKI_XI_Connect)
			(m_iGamepadsConnected>=0)?m_iGamepadsConnected++:(m_iGamepadsConnected=1);
		else if (rInputEvent.keyId == eKI_XI_Disconnect)
			(m_iGamepadsConnected>0)?m_iGamepadsConnected--:(m_iGamepadsConnected=0);
		
		if (ICVar* requireinputdevice = gEnv->pConsole->GetCVar("sv_requireinputdevice"))
		{
			if(!strcmpi(requireinputdevice->GetString(), "gamepad") && !m_iGamepadsConnected && !IsActive())
				ShowInGameMenu(true);
		}

		if(m_iGamepadsConnected != oldGamepads)
		{
			bool connected = (m_iGamepadsConnected > 0)?true:false;
			if(connected != m_bControllerConnected)
			{
				bool handled = false;
				if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->IsLoaded())
				{
					m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("GamepadAvailable", m_iGamepadsConnected?true:false);
					handled = true;
				}
				if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->IsLoaded())
				{
					m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("GamepadAvailable", m_iGamepadsConnected?true:false);
					handled = true;
				}
				
				if(!handled)
					SAFE_HUD_FUNC(ShowGamepadConnected(connected));
			}

			m_bControllerConnected = connected;
		}
	}

	//X-gamepad virtual keyboard input
	bool move			= false;
	bool genericA	= false;
	bool commit		= rInputEvent.state == eIS_Pressed;
	const char* direction = "";
	Vec2 dirvec(0,0);
	if(rInputEvent.keyId == eKI_Up || rInputEvent.keyId == eKI_XI_DPadUp || rInputEvent.keyId == eKI_PS3_Up || rInputEvent.keyId == eKI_XI_ThumbLUp)
	{
		move = true;
		direction = "up";
		dirvec = Vec2(0,-1);
	}
	else if(rInputEvent.keyId == eKI_Down || rInputEvent.keyId == eKI_XI_DPadDown || rInputEvent.keyId == eKI_PS3_Down || rInputEvent.keyId == eKI_XI_ThumbLDown)
	{
		move = true;
		direction = "down";
		dirvec = Vec2(0,1);
	}
	else if(rInputEvent.keyId == eKI_Left || rInputEvent.keyId == eKI_XI_DPadLeft || rInputEvent.keyId == eKI_PS3_Left || rInputEvent.keyId == eKI_XI_ThumbLLeft)
	{
		move = true;
		direction = "left";
		dirvec = Vec2(-1,0);
	}
	else if(rInputEvent.keyId == eKI_Right || rInputEvent.keyId == eKI_XI_DPadRight || rInputEvent.keyId == eKI_PS3_Right || rInputEvent.keyId == eKI_XI_ThumbLRight)
	{
		move = true;
		direction = "right";
		dirvec = Vec2(1,0);
	}
	else if(rInputEvent.keyId == eKI_Enter || rInputEvent.keyId == eKI_XI_A || rInputEvent.keyId == eKI_XI_Start || rInputEvent.keyId == eKI_PS3_Square)
	{
		move = true;
		// Note: commit on release so no release events come after closing virtual keyboard!
		//commit = rInputEvent.state == eIS_Released;
		direction = "press";

		genericA=FindButton("xi_a") == m_currentButtons.end();
	}
	
	if (m_bVirtualKeyboardFocus)
	{
		// Virtual keyboard navigation
		if(move && commit)
		{			
			if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->IsLoaded())
				m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("moveCursor", direction);
			if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->IsLoaded())
				m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("moveCursor", direction);
		}
	}
	else if (m_pCurrentFlashMenuScreen && m_pCurrentFlashMenuScreen->IsLoaded())
	{
		// Controller menu navigation:
		//	Select menu item
		if (genericA)
		{
			// Better simply emulate a mouse button push in this case so push works for non-tracked UI elements too!
			//PushButton(m_currentButtons.find(m_sCurrentButton), rInputEvent.state == eIS_Pressed, false);
#if !defined(PS3)
			float x, y;
			SAFE_HARDWARE_MOUSE_FUNC(GetHardwareMouseClientPosition(&x, &y));
			SAFE_HARDWARE_MOUSE_FUNC(Event((int)x, (int)y, rInputEvent.state == eIS_Pressed ? HARDWAREMOUSEEVENT_LBUTTONDOWN : HARDWAREMOUSEEVENT_LBUTTONUP));


#endif
		}
		//	Navigate
		else if(!m_textfieldFocus && dirvec.GetLength()>0.0 && commit)
		{
			SnapToNextButton(dirvec);
		}
		//	Try find button for shortcut action and press it
		else
		{
			PushButton(FindButton(rInputEvent.keyName), rInputEvent.state == eIS_Pressed, true);
		}
	}		
	return false;
}


//////////////////////////////////////////////////////////////////////////
bool CFlashMenuObject::OnInputEventUI( const SInputEvent &rInputEvent )
{
	if(gEnv->pConsole->GetStatus())
	{
		return false;
	}

	if(m_bUpdate)
	{
		SFlashKeyEvent keyEvent(MapToFlashKeyEvent(rInputEvent));

		if (m_pCurrentFlashMenuScreen && m_pCurrentFlashMenuScreen->GetFlashPlayer())
		{
			//if(eIS_Pressed == rInputEvent.state)
			//	m_pCurrentFlashMenuScreen->CheckedInvoke("onPressedKey", rInputEvent.keyName.c_str());
			m_pCurrentFlashMenuScreen->GetFlashPlayer()->SendKeyEvent(keyEvent);
			keyEvent.m_state = SFlashKeyEvent::eKeyUp;
			m_pCurrentFlashMenuScreen->GetFlashPlayer()->SendKeyEvent(keyEvent);
			keyEvent.m_state = SFlashKeyEvent::eKeyDown;
		}

		if (m_pFlashPlayer)
		{
			m_pFlashPlayer->SendKeyEvent(keyEvent);
			keyEvent.m_state = SFlashKeyEvent::eKeyUp;
			m_pFlashPlayer->SendKeyEvent(keyEvent);
			keyEvent.m_state = SFlashKeyEvent::eKeyDown;
		}
	}

	//AddInputChar( event.keyName[0] );

	return false;
}
//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::MP_ResetBegin()
{
	StopVideo();	//stop background video
	StopTutorialVideo();

	if(m_pCurrentFlashMenuScreen == m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME])
		ShowInGameMenu(false);

	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated()) return;

	m_iMaxProgress = 100;

	m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET] = new CFlashMenuScreen;

	if(!m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->IsLoaded())
	{
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->Load("Libs/UI/HUD_MP_RestartScreen.gfx");
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->GetFlashPlayer()->SetFSCommandHandler(this);
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->Invoke("SetProgress",0.0f);
		
		UpdateMenuColor();
	}

	SAFE_HUD_FUNC(MP_ResetBegin());

	m_pCurrentFlashMenuScreen = m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET];

	m_bUpdate = true;
	m_nBlackGraceFrames = 0; 
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::MP_ResetEnd()
{
	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated()) return;

	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET])
	{
		m_nBlackGraceFrames = gEnv->pRenderer->GetFrameID(false) + BLACK_FRAMES;

		if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->IsLoaded())
		{
			m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->Unload();
			m_bUpdate = false;
		}
		SAFE_DELETE(m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]);
		m_pCurrentFlashMenuScreen = NULL;
	}

	SAFE_HUD_FUNC(MP_ResetEnd());
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::MP_ResetProgress(int iProgress)
{
	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated()) return;

	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->GetFlashPlayer())
	{
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->Invoke("SetProgress", float(iProgress));
	}

	const bool bStandAlone = gEnv->pRenderer->EF_Query(EFQ_RecurseLevel) <= 0;
	if (bStandAlone)
		gEnv->pSystem->RenderBegin();
	m_bIgnorePendingEvents = true;
	OnPostUpdate(0.1f);
	m_bIgnorePendingEvents = false;
	if (bStandAlone)
		gEnv->pSystem->RenderEnd();
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::OnLoadingStart(ILevelInfo *pLevel)
{
	m_bInLoading = true;

	SAFE_HUD_FUNC(OnLoadingStart(pLevel));

	StopVideo();	//stop background video
	StopTutorialVideo();

	if(m_stateEntryMovies!=eEMS_Done)
	{
		if (m_stateEntryMovies < eEMS_Done)
			SAFE_HARDWARE_MOUSE_FUNC(IncrementCounter());
		m_stateEntryMovies=eEMS_Done;
	}

	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated()) return;

	if(m_bLanQuery)
	{
		gEnv->pGame->GetIGameFramework()->EndCurrentQuery();
		m_bLanQuery = false;
	}
	if(pLevel)
	{
		m_iMaxProgress = pLevel->GetDefaultGameType()->cgfCount;
	}
	else
	{
		m_iMaxProgress = 100;
	}
	//DestroyStartMenu();
	if(!m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->IsLoaded())
	{
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Load("Libs/UI/Menus_Loading_MP.gfx");
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->GetFlashPlayer()->SetFSCommandHandler(this);
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Invoke("SetProgress", 0.0f);
		
		UpdateMenuColor();
	}

	string rootName;
	if (pLevel)
		rootName = (pLevel->GetPath());
	else // assume its the currently loaded level
		rootName = string("levels/") + g_pGame->GetIGameFramework()->GetLevelName();

	//now load the actual map
	string mapName = rootName;
	int slashPos = mapName.rfind('\\');
	if(slashPos == -1)
		slashPos = mapName.rfind('/');
	mapName = mapName.substr(slashPos+1, mapName.length()-slashPos);

	string sXml = rootName;
	sXml.append("/");
	sXml.append(mapName);
	sXml.append(".xml");
	XmlNodeRef mapInfo = GetISystem()->LoadXmlFile(sXml.c_str());
	std::vector<string> screenArray;

	const char* header = NULL;
	const char* description = NULL;

	if(mapInfo == 0)
	{
		GameWarning("Did not find a map info file %s in %s.", sXml.c_str(), mapName.c_str());
	}
	else
	{
		//retrieve the coordinates of the map
		if(mapInfo)
		{
			for(int n = 0; n < mapInfo->getChildCount(); ++n)
			{
				XmlNodeRef mapNode = mapInfo->getChild(n);
				const char* name = mapNode->getTag();
				if(!stricmp(name, "LoadingScreens"))
				{
					int attribs = mapNode->getNumAttributes();
					const char* key;
					const char* value;
					for(int i = 0; i < attribs; ++i)
					{
						mapNode->getAttributeByIndex(i, &key, &value);
						screenArray.push_back(value);
					}
				}
				else if(!stricmp(name, "HeaderText"))
				{
					int attribs = mapNode->getNumAttributes();
					const char* key;
					for(int i = 0; i < attribs; ++i)
					{
						mapNode->getAttributeByIndex(i, &key, &header);
						if(!stricmp(key,"text"))
						{
							break;
						}
					}
				}
				else if(!stricmp(name, "DescriptionText"))
				{
					int attribs = mapNode->getNumAttributes();
					const char* key;
					for(int i = 0; i < attribs; ++i)
					{
						mapNode->getAttributeByIndex(i, &key, &description);
						if(!stricmp(key,"text"))
						{
							break;
						}
					}
				}

			}
		}
	}

	int size = screenArray.size();
	if(size<=0)
	{
		screenArray.push_back("loading.dds");
		size = 1;
	}

	if(!header)
	{
		header = "";
	}
	if(!description)
	{
		description = "";
	}

	uint iUse = cry_rand()%size;
	string sImg = rootName;
	sImg.append("/");
	sImg.append(screenArray[iUse]);

	SFlashVarValue arg[2] = {header,description};
	m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Invoke("setText",arg,2);
	m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Invoke("setMapBackground",SFlashVarValue(sImg));
	m_pCurrentFlashMenuScreen = m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING];

	m_bUpdate = true;
	m_nBlackGraceFrames = 0; 

	if(m_pMusicSystem && m_fMusicFirstTime == -1.0f)
	{
		m_pMusicSystem->SetMood("multiplayer_high");
	}

	if(!gEnv->pSystem->IsSerializingFile())
	{
		m_sLastSaveGame = "";
		SetDifficulty(); //set difficulty setting at game/level start
	}

	//remove the hud to be re-created
	if(g_pGame)
		g_pGame->DestroyHUD();
}

//-----------------------------------------------------------------------------------------------------

bool CFlashMenuObject::ShouldIgnoreInGameEvent()
{
	return !gEnv->pSystem->IsEditor() && !gEnv->bMultiplayer && m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->IsLoaded() && gEnv->pSystem->IsSerializingFile() != 1 && !g_pGame->IsReloading() && g_pGameCVars->hud_startPaused;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::OnLoadingComplete(ILevel *pLevel)
{
	SAFE_HUD_FUNC(OnLoadingComplete(pLevel));

	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated()) 
	{
		return;
	}

	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART])
	{
		if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->IsLoaded())
			m_bDestroyStartMenuPending = true;
	}

	m_nBlackGraceFrames = gEnv->pRenderer->GetFrameID(false) + BLACK_FRAMES;

	if (ShouldIgnoreInGameEvent())
	{
		if(!gEnv->bMultiplayer)
			g_pGame->GetIGameFramework()->PauseGame(true ,false);

		m_bLoadingDone = true;
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Invoke("setLoadingDone");
	}

	m_bInLoading = false;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::OnLoadingError(ILevelInfo *pLevel, const char *error)
{
	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->IsLoaded())
	{
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Unload();
	}

	ShowMainMenu();
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::OnLoadingProgress(ILevelInfo *pLevel, int progressAmount)
{
	if(m_bIsEndingGameContext || !m_bInLoading)
		return;

	if (pLevel == 0)
		return;

	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated() || gEnv->pGame->GetIGameFramework()->IsGameStarted() ) 
	{
		return;
	}

	// TODO: seems that OnLoadingProgress can be called *after* OnLoadingComplete ...
	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->GetFlashPlayer())
	{
		float fProgress = progressAmount / (float) m_iMaxProgress * 100.0f;
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Invoke("SetProgress", fProgress);
	}

	const bool bStandAlone = gEnv->pRenderer->EF_Query(EFQ_RecurseLevel) <= 0;
	if (bStandAlone)
		gEnv->pSystem->RenderBegin();
	m_bIgnorePendingEvents = true;
	OnPostUpdate(0.1f);
	m_bIgnorePendingEvents = false;
	if (bStandAlone)
		gEnv->pSystem->RenderEnd();
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::ShowMainMenu()
{
	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated())
    return;

	// reset any game volume back to normal
	if (gEnv->pSoundSystem)
		gEnv->pSoundSystem->Pause(false, true);

	// When typing "disconnect" on loading "waiting" screen, we go back to main menu, so we have to kill loading screen
	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->IsLoaded())
	{
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Unload();
		m_bLoadingDone = false;
	}

  m_bUpdate = true;
	SetColorChanged();
	InitStartMenu();
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::LockPlayerInputs(bool bLock)
{
	// In multiplayer, we can't pause the game, so we have to disable the player inputs
	IActionMapManager *pActionMapManager = gEnv->pGame->GetIGameFramework()->GetIActionMapManager();
	if(!pActionMapManager)
		return;

	pActionMapManager->Enable(!bLock);
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::ShowInGameMenu(bool bShow)
{
	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated()) return;

	// prevent the menu of showing if the 'Press Fire to start level' screen is present
	if (m_bLoadingDone && g_pGameCVars->hud_startPaused && bShow)
		return;

	if(m_bUpdate != bShow)
	{
		if(bShow)
		{
			// In the case we press ESC very quickly three times in a row while playing:
			// The menu is created, set in destroy pending event and hidden, then recreated, then destroyed by the pending event at the next OnPostUpdate
			// To avoid that, simply cancel the destroy pending event when creating
			m_bDestroyInGameMenuPending = false;
			if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME])
				m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->SetVisible(true);
			//

			InitIngameMenu();
		}
		else
		{
			DestroyIngameMenu();
		}
		m_repeatEvent = SInputEvent();
	}

	if (gEnv->pInput) gEnv->pInput->ClearKeyState();
	
	m_bUpdate = bShow;

	m_pCurrentFlashMenuScreen = m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME];
	if(m_pCurrentFlashMenuScreen && !bShow)
	{
		m_pCurrentFlashMenuScreen->Invoke("Root.MainMenu.StartMenu.gotoAndPlay", "on");
	} 

	if(bShow) //this causes some stalling, but reduces flash memory pool peak by ~ 10MB
	{
		UnloadHUDMovies();
		//stuff is reloaded after "m_DestroyInGameMenuPending" ...
	}

	LockPlayerInputs(m_bUpdate);
	
	if (bShow && gEnv->pSoundSystem)
	{
		// prevents to play ambience if window got minimized or alt-tapped
		if (!gEnv->pSoundSystem->IsPaused())
			PlaySound(ESound_MenuAmbience);
	}

  if(!gEnv->bMultiplayer)
	  g_pGame->GetIGameFramework()->PauseGame(m_bUpdate,false);

	// stop game music and trigger menu music *after* pausing the game
	if (bShow && m_pMusicSystem)
	{
		m_pMusicSystem->SerializeInternal(true);
		m_pMusicSystem->EndTheme(EThemeFade_FadeOut, 0, true);
		m_pMusicSystem->SetTheme("menu", true, false);
		m_pMusicSystem->SetMood("menu_music", true, true);
	}

	SAFE_HUD_FUNC(SetInMenu(m_bUpdate));
	if(bShow)
	{
		SAFE_HUD_FUNC(UpdateHUDElements());
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::HideInGameMenuNextFrame(bool bRestoreGameMusic)
{
  if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->IsLoaded())
	{
		if (gEnv->pInput) gEnv->pInput->ClearKeyState();

		m_bDestroyInGameMenuPending = true;
		m_bUpdate = false;
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->SetVisible(false);
	  
		LockPlayerInputs(m_bUpdate);

		// stop menu music *before* pausing the game
		if(m_pMusicSystem)
		{
			m_pMusicSystem->EndTheme(EThemeFade_FadeOut, 0, true);
			if (bRestoreGameMusic)
				m_pMusicSystem->SerializeInternal(false);
			PlaySound(ESound_MenuAmbience,false);
		}

		if(!gEnv->bMultiplayer)
			g_pGame->GetIGameFramework()->PauseGame(m_bUpdate,false);

		SAFE_HUD_FUNC(SetInMenu(m_bUpdate));
		SAFE_HUD_FUNC(UpdateHUDElements());
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::UnloadHUDMovies()
{
  SAFE_HUD_FUNC(UnloadVehicleHUD(true));	//removes vehicle hud to save memory (pool spike)
  SAFE_HUD_FUNC(UnloadSimpleHUDElements(true)); //removes hud elements to save memory (pool spike)
  SAFE_HUD_FUNC(GetMapAnim()->Unload());
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::ReloadHUDMovies()
{
  SAFE_HUD_FUNC(UnloadSimpleHUDElements(false)); //removes hud elements to save memory (pool spike)
  CGameFlashAnimation *mapAnim = SAFE_HUD_FUNC_RET(GetMapAnim());
  if(mapAnim)
  {
    mapAnim->Reload();
    SAFE_HUD_FUNC(GetRadar()->ReloadMiniMap());
    if(mapAnim == SAFE_HUD_FUNC_RET(GetModalHUD()))
    {
			SAFE_HUD_FUNC(ShowPDA(false));
			SAFE_HUD_FUNC(ShowPDA(true));
    }
  }
  SAFE_HUD_FUNC(UnloadVehicleHUD(false));	//removes vehicle hud to save memory (pool spike)
}

//-----------------------------------------------------------------------------------------------------

bool CFlashMenuObject::PlayFlashAnim(const char* pFlashAnim)
{
	SAFE_RELEASE(m_pFlashPlayer);
	if (pFlashAnim && pFlashAnim[0])
	{
		m_pFlashPlayer = gEnv->pSystem->CreateFlashPlayerInstance();
		if (m_pFlashPlayer && m_pFlashPlayer->Load(pFlashAnim))
		{
			// TODO: SetViewport should also be called when we resize the app window to scale flash animation accordingly
			int flashWidth(m_pFlashPlayer->GetWidth());
			int flashHeight(m_pFlashPlayer->GetHeight());

			int screenWidth(gEnv->pRenderer->GetWidth());
			int screenHeight(gEnv->pRenderer->GetHeight());

			float scaleX((float)screenWidth / (float)flashWidth);
			float scaleY((float)screenHeight / (float)flashHeight);

			float scale(scaleY);
			if (scaleY * flashWidth > screenWidth)
				scale = scaleX;

			int w((int)(flashWidth * scale));
			int h((int)(flashHeight * scale));
			int x((screenWidth - w) / 2);
			int y((screenHeight - h) / 2);

			m_pFlashPlayer->SetViewport(x, y, w, h);
			m_pFlashPlayer->SetScissorRect(x, y, w, h);
			m_pFlashPlayer->SetBackgroundAlpha(0);
			// flash player is now initialized, frames are rendered in PostUpdate( ... )
		}
		else
		{
			SAFE_RELEASE(m_pFlashPlayer);
			return false;
		}
	}
	return true;
}

// this maps languages to audio channels
static const char* g_languageMapping[] =
{
	"English",	// channel 0
	"French",		// channel 1
	"German",		// channel 2
	"Italian",	// channel 3
	"Russian",	// channel 4
	"Spanish",	// channel 5
	"Turkish",	// channel 6
	"Japanese"  // channel 7
};

static const size_t g_languageMappingCount = (sizeof(g_languageMapping) / sizeof(g_languageMapping[0]) );

// returns -1 if we don't have localized audio for the language (so subtitles will be needed)
static int ChooseLocalizedAudioChannel()
{
	const char* language = gEnv->pSystem->GetLocalizationManager()->GetLanguage();
	if (language == 0 || *language == 0)
		return -1; 

	for (int i = 0; i < g_languageMappingCount; ++i)
	{
		if (stricmp(g_languageMapping[i], language) == 0)
			return i;
	}
	return -1;
}

//-----------------------------------------------------------------------------------------------------

bool CFlashMenuObject::PlayVideo(const char* pVideoFile, bool origUpscaleMode, unsigned int videoOptions, int audioCh, int voiceCh, bool useSubtitles, bool exclusiveVideo)
{
	SAFE_RELEASE(m_pVideoPlayer);
	m_bExclusiveVideo = false;
	ICVar* pCVar = gEnv->pConsole->GetCVar("g_language");
	bool french = false;
	if(pCVar)
		french = stricmp(pCVar->GetString(),"french")==0;
	useSubtitles = g_pGameCVars->hud_subtitles || french;
	if (pVideoFile && pVideoFile[0])
	{
		if (audioCh == VIDEOPLAYER_LOCALIZED_AUDIOCHANNEL)
		{
			if(french && !stricmp(pVideoFile, "Localized/Video/PS_Tutorial.sfd"))
				audioCh = -1;
			else
				audioCh = ChooseLocalizedAudioChannel();
			if (audioCh >= 0) // we found a matching language which has localized audio, turn off subtitles
			{
				// if we have localized audio, we turn off subtitles unless somebody explicitly turned them in
				if (g_pGame->GetCVars()->hud_subtitles == 0) // nobody turned on subtitles, so really turn off
				{
					useSubtitles = false;
					CryLog("CFlashMenuObject::PlayVideo: Turning off subtitles for Video '%s' as localized version is available and Subtitle Option is not selected.", pVideoFile);
				}
			}
			else
			{
				// Use English and turn on subtitles
				audioCh = 0;
				useSubtitles = true;
			}
		}
		m_pVideoPlayer = gEnv->pRenderer->CreateVideoPlayerInstance();
		if (m_pVideoPlayer && m_pVideoPlayer->Load(pVideoFile, videoOptions, audioCh, voiceCh, useSubtitles))	
		{
			// TODO: SetViewport should also be called when we resize the app window to scale flash animation accordingly
			int videoWidth(m_pVideoPlayer->GetWidth());
			int videoHeight(m_pVideoPlayer->GetHeight());

			int screenWidth(gEnv->pRenderer->GetWidth());
			int screenHeight(gEnv->pRenderer->GetHeight());

			float scaleX((float)screenWidth / (float)videoWidth);
			float scaleY((float)screenHeight / (float)videoHeight);

			float scale(scaleY);

			if (origUpscaleMode)
			{
				if (scaleY * videoWidth > screenWidth)
					scale = scaleX;
			}
			else
			{
				float videoRatio((float)videoWidth / (float)videoHeight);
				float screenRatio((float)screenWidth / (float)screenHeight);

				if (videoRatio < screenRatio)
					scale = scaleX;
			}

			int w((int)(videoWidth * scale));
			int h((int)(videoHeight * scale));
			int x((screenWidth - w) / 2);
			int y((screenHeight - h) / 2);

			m_pVideoPlayer->SetViewport(x, y, w, h);

			m_bExclusiveVideo = exclusiveVideo;
			if (useSubtitles)
			{
				if (m_pSubtitleScreen == 0)
				{
					m_pSubtitleScreen = new CFlashMenuScreen;
					m_pSubtitleScreen->Load("Libs/UI/HUD_Subtitle.gfx");
				}
				else
				{
					SFlashVarValue args[1] = { "" };
					m_pSubtitleScreen->Invoke("setText", args, 1);
				}
				m_currentSubtitleLabel.clear();
			}
			else 
			{
				SAFE_DELETE(m_pSubtitleScreen);
			}
		}
		else
			return false;
	}
	return true;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::StopVideo()
{
  SAFE_RELEASE(m_pVideoPlayer);
	SAFE_DELETE(m_pSubtitleScreen);
	m_currentSubtitleLabel.clear();
	m_bExclusiveVideo = false;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::PlayTutorialVideo()
{
	if(PlayVideo("Localized/Video/PS_Tutorial.sfd",false,0,CFlashMenuObject::VIDEOPLAYER_LOCALIZED_AUDIOCHANNEL,-1,true,true))
	{
		SAFE_HARDWARE_MOUSE_FUNC(DecrementCounter());
		if(m_pMusicSystem)
			m_pMusicSystem->EndTheme(EThemeFade_FadeOut, 0, true);
		PlaySound(ESound_MenuAmbience,false);
		m_bTutorialVideo = true;
	}
}

//-----------------------------------------------------------------------------------------------------

bool CFlashMenuObject::StopTutorialVideo()
{
	if(m_bTutorialVideo)
	{
		SAFE_HARDWARE_MOUSE_FUNC(IncrementCounter());
		StopVideo();
		PlayVideo("Localized/Video/bg.sfd", false, IVideoPlayer::LOOP_PLAYBACK);
		if(m_pMusicSystem)
			m_pMusicSystem->SetMood("menu_music", true, true);
		PlaySound(ESound_MenuAmbience);
		m_bTutorialVideo = false;
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::NextIntroVideo()
{
	m_stateEntryMovies = (EEntryMovieState)(((int)m_stateEntryMovies) + 1);
	if(m_stateEntryMovies!=eEMS_Stop)
	{
		const char* movie = VALUE_BY_KEY(m_stateEntryMovies, gMovies);
		if(m_stateEntryMovies==eEMS_Rating)
		{
#ifdef SP_DEMO
			return;
#endif
			const char* language = "english";
			ICVar* pCVar = gEnv->pConsole->GetCVar("g_language");
			if(pCVar)
			{
				language = pCVar->GetString();
			}
			char langMovie[256];
			sprintf(langMovie, movie,language);
			movie = langMovie;
		}
		else if(m_stateEntryMovies == eEMS_RatingLogo||m_stateEntryMovies == eEMS_PEGI||m_stateEntryMovies == eEMS_SPDEMO)
		{
#ifndef SP_DEMO
			return;
#endif
		}
		if(movie)
			PlayVideo(movie, false);
	}
}

//-----------------------------------------------------------------------------------------------------

bool CFlashMenuObject::IsOnScreen(EMENUSCREEN screen)
{
  if(m_apFlashMenuScreens[screen] && m_pCurrentFlashMenuScreen == m_apFlashMenuScreens[screen])
    return m_bUpdate;
  return false;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::OnHardwareMouseEvent(int iX,int iY,EHARDWAREMOUSEEVENT eHardwareMouseEvent)
{
	if(HARDWAREMOUSEEVENT_LBUTTONDOUBLECLICK == eHardwareMouseEvent)
	{
		if(m_pCurrentFlashMenuScreen && m_pCurrentFlashMenuScreen->GetFlashPlayer())
		{
			int x(iX), y(iY);
			m_pCurrentFlashMenuScreen->GetFlashPlayer()->ScreenToClient(x,y);
			SFlashVarValue args[2] = {x,y};
			m_pCurrentFlashMenuScreen->CheckedInvoke("_root.Root.MainMenu.MultiPlayer.DoubleClick",args,2);
			m_pCurrentFlashMenuScreen->CheckedInvoke("DoubleClick",args,2);
		}
	}
	else
	{
		SFlashCursorEvent::ECursorState eCursorState = SFlashCursorEvent::eCursorMoved;
		if(HARDWAREMOUSEEVENT_LBUTTONDOWN == eHardwareMouseEvent)
		{
			eCursorState = SFlashCursorEvent::eCursorPressed;
		}
		else if(HARDWAREMOUSEEVENT_LBUTTONUP == eHardwareMouseEvent)
		{
			eCursorState = SFlashCursorEvent::eCursorReleased;




		}

		if(m_pCurrentFlashMenuScreen && m_pCurrentFlashMenuScreen->GetFlashPlayer())
		{
			int x(iX), y(iY);
			m_pCurrentFlashMenuScreen->GetFlashPlayer()->ScreenToClient(x,y);
			m_pCurrentFlashMenuScreen->GetFlashPlayer()->SendCursorEvent(SFlashCursorEvent(eCursorState,x,y));
			UpdateButtonSnap(Vec2(x,y));
		}

		if(m_pFlashPlayer)
		{
			int x(iX), y(iY);
			m_pFlashPlayer->ScreenToClient(x,y);
			m_pFlashPlayer->SendCursorEvent(SFlashCursorEvent(eCursorState,x,y));
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::UpdateButtonSnap(const Vec2 mouse)
{




	Vec2 mouseNew(mouse);
	HWMouse2Flash(mouseNew);
	if(m_currentButtons.empty()) return;
	ButtonPosMap::iterator bestEst = m_currentButtons.end();
	float fBestDist = -1.0f;
	for(ButtonPosMap::iterator it = m_currentButtons.begin(); it != m_currentButtons.end(); ++it)
	{
		Vec2 pos = it->second;
		float dist = (mouseNew-pos).GetLength();
		if(dist<200.0f && (fBestDist<0.0f || dist<fBestDist))
		{
			fBestDist = dist;
			bestEst = it;
		}
	}
	if(bestEst != m_currentButtons.end())
		m_sCurrentButton = bestEst->first;

}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::SnapToNextButton(const Vec2 dir)
{
	if(!m_bUpdate) return;
	if(m_currentButtons.empty()) return;
	ButtonPosMap::iterator current = m_currentButtons.find(m_sCurrentButton);


























	if(current == m_currentButtons.end())
	{
		current = m_currentButtons.begin();
	}

	// Used only for 2D navigation mode
	/*if (dir.y > 0.0)
	{
		++current;

		if (current == m_currentButtons.end())
			current = m_currentButtons.begin();		
	}
	else
	{
		if (current == m_currentButtons.begin())
			current = m_currentButtons.end();

		--current;
	}

	if (current == m_currentButtons.end())
	{
		m_sCurrentButton="";
	}
	else
	{
		m_sCurrentButton=current->first;
		HighlightButton(current);
	}
	*/

	// The original implementation with four directions
	Vec2 curPos = current->second;

	ButtonPosMap::iterator bestEst = m_currentButtons.end();
	float fBestValue = -1.0f;
	for(ButtonPosMap::iterator it = m_currentButtons.begin(); it != m_currentButtons.end(); ++it)
	{
		if(it == current) continue;
		Vec2 btndir = it->second - curPos;
		float dist = btndir.GetLength();
		btndir = btndir.GetNormalizedSafe();
		float arc = dir.Dot(btndir);
		if(arc<=0.01) continue;
		float curValue = (dist/arc);
		if(fBestValue<0 || curValue<fBestValue)
		{
			fBestValue = curValue;
			bestEst = it;
			m_sCurrentButton = it->first;
		}
	}

	// Wrap around
	if(bestEst==m_currentButtons.end())
	{
		Vec2 round=dir*-1.0f;
		fBestValue = -1.0f;

		for(ButtonPosMap::iterator it = m_currentButtons.begin(); it != m_currentButtons.end(); ++it)
		{
			if(it == current) continue;
			Vec2 btndir = it->second - curPos;
			float dist = btndir.GetLength();
			btndir = btndir.GetNormalizedSafe();
			float arc = round.Dot(btndir);
			if(arc<=0.01) continue;
			if(dist>fBestValue)
			{
				fBestValue = dist;
				bestEst = it;
				m_sCurrentButton = it->first;
			}
		}
	}

/*	if(bestEst==m_currentButtons.end())
	{
		fBestValue = -1.0f;
		for(ButtonPosMap::iterator it = m_currentButtons.begin(); it != m_currentButtons.end(); ++it)
		{
			if(it == current) continue;
			Vec2 btndir = it->second - curPos;
			float dist = btndir.GetLength();
			btndir = btndir.GetNormalizedSafe();
			float arc = dir.Dot(btndir);
			if(arc<=0.0) continue;
			if(fBestValue<0 || dist<fBestValue)
			{
				fBestValue = dist;
				bestEst = it;
				m_sCurrentButton = it->first;
			}
		}
	}
*/
	if(bestEst!=m_currentButtons.end())
		HighlightButton(bestEst);
	else if(current!=m_currentButtons.end())
		HighlightButton(current);

}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::GetButtonClientPos(ButtonPosMap::iterator button, Vec2 &pos)
{
	pos = button->second;

	if(!m_pCurrentFlashMenuScreen)
		return;

	IRenderer *pRenderer = gEnv->pRenderer;

	float movieWidth		= (float)m_pCurrentFlashMenuScreen->GetFlashPlayer()->GetWidth();
	float movieHeight		= (float)m_pCurrentFlashMenuScreen->GetFlashPlayer()->GetHeight();
	float movieRatio		=	movieWidth / movieHeight;

	float renderWidth		=	pRenderer->GetWidth();
	float renderHeight	=	pRenderer->GetHeight();
	float renderRatio	=	renderWidth / renderHeight;

	float offsetX = 0.0f;
	float offsetY = 0.0f;

	if(renderRatio != movieRatio)
	{
		if(renderRatio < 4.0f / 3.0f)
		{
			float visibleHeight = (renderWidth * 3.0f / 4.0f);
			offsetY = (renderHeight-visibleHeight) * 0.5;
			renderHeight = visibleHeight;
		}
		offsetX = ( renderWidth - (renderHeight * movieRatio) ) * 0.5;
	}
	pos*=renderHeight/movieHeight;
	pos.x+=offsetX;
	pos.y+=offsetY;





}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::HWMouse2Flash(Vec2 &pos)
{
	if(!m_pCurrentFlashMenuScreen)
		return;

	IRenderer *pRenderer = gEnv->pRenderer;

	float movieWidth		= (float)m_pCurrentFlashMenuScreen->GetFlashPlayer()->GetWidth();
	float movieHeight		= (float)m_pCurrentFlashMenuScreen->GetFlashPlayer()->GetHeight();

	float renderWidth		=	pRenderer->GetWidth();
	float renderHeight	=	pRenderer->GetHeight();
	float renderRatio	=	renderWidth / renderHeight;

	if(renderRatio < 4.0f / 3.0f)
	{
		float visibleHeight = (renderWidth * 3.0f / 4.0f);
		renderHeight = visibleHeight;
	}

	pos*=movieHeight/renderHeight;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::HighlightButton(ButtonPosMap::iterator button)
{
	Vec2 pos;
	GetButtonClientPos(button, pos);
	
#if !defined(PS3)
	SAFE_HARDWARE_MOUSE_FUNC(SetHardwareMouseClientPosition(pos.x, pos.y));


#endif
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::PushButton(ButtonPosMap::iterator button, bool press, bool force)
{
	if (button == m_currentButtons.end())
		return;

	Vec2 pos;
	GetButtonClientPos(button, pos);
	
	if (!force)
	{
#if !defined(PS3)
		SAFE_HARDWARE_MOUSE_FUNC(Event((int)pos.x, (int)pos.y, press ? HARDWAREMOUSEEVENT_LBUTTONDOWN : HARDWAREMOUSEEVENT_LBUTTONUP));


#endif
	}
	else if (!press)
	{
		string method=button->first;
		method.append(".pressButton");
		m_pCurrentFlashMenuScreen->GetFlashPlayer()->Invoke0(method);
	}
}

//-----------------------------------------------------------------------------------------------------

CFlashMenuObject::ButtonPosMap::iterator CFlashMenuObject::FindButton(const TKeyName &shortcut)
{
	if(m_currentButtons.empty())
		return m_currentButtons.end();

	// FIXME: Try to find a more elgant way to identify shortcuts
	string sc;
	if (shortcut == "xi_a")
		sc="_a";
	else if (shortcut == "xi_b")
		sc="_b";
	else if (shortcut == "xi_x")
		sc="_x";
	else if (shortcut == "xi_y")
		sc="_y";
	else
		return m_currentButtons.end();


	for(ButtonPosMap::iterator it = m_currentButtons.begin(); it != m_currentButtons.end(); ++it)
	{
		if (it->first.substr(it->first.length()-sc.length(), sc.length()) == sc)
			return it;
	}

	return m_currentButtons.end();
}

//-----------------------------------------------------------------------------------------------------
//ALPHA workaround
void CFlashMenuObject::UpdateLevels(const char* gamemode)
{
	m_pCurrentFlashMenuScreen->Invoke("resetMultiplayerLevel");
	ILevelSystem *pLevelSystem = gEnv->pGame->GetIGameFramework()->GetILevelSystem();
	if(pLevelSystem)
	{
    SFlashVarValue args[2] = {"","@ui_ANY"};
    m_pCurrentFlashMenuScreen->Invoke("addMultiplayerLevel", args, 2);
		for(int l = 0; l < pLevelSystem->GetLevelCount(); ++l)
		{
			ILevelInfo *pLevelInfo = pLevelSystem->GetLevelInfo(l);
			if(pLevelInfo && pLevelInfo->SupportsGameType(gamemode))
			{
				string display(pLevelInfo->GetDisplayName());
				string file(pLevelInfo->GetName());
				SFlashVarValue args[2] = {file.c_str(),display.empty()?file.c_str():display.c_str()};
				m_pCurrentFlashMenuScreen->Invoke("addMultiplayerLevel", args, 2);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::HandleFSCommand(const char *szCommand,const char *szArgs)
{
  if(g_pGameCVars->g_debug_fscommand)
    CryLog("HandleFSCommand : %s %s\n", szCommand, szArgs);

	if(g_pGame->GetOptions()->HandleFSCommand(szCommand, szArgs))
		return;

	if(!stricmp(szCommand, "menu_highlight"))
	{
		PlaySound(ESound_MenuHighlight);
	}
	else if(!stricmp(szCommand, "main_highlight"))
	{
		PlaySound(ESound_MainHighlight);
	}
	else if(!stricmp(szCommand, "menu_select"))
	{
		PlaySound(ESound_MenuSelect);
	}
	else if(!stricmp(szCommand, "menu_select_dialog"))
	{
		PlaySound(ESound_MenuSelectDialog);
	}
	else if(!stricmp(szCommand, "menu_winopen"))
	{
		PlaySound(ESound_MenuOpen);
	}
	else if(!stricmp(szCommand, "menu_winclose"))
	{
		PlaySound(ESound_MenuClose);
	}
	else if(!stricmp(szCommand, "main_start"))
	{
		PlaySound(ESound_MenuStart);
	}
	else if(!stricmp(szCommand, "main_open"))
	{
		PlaySound(ESound_MenuFirstOpen);
	}
	else if(!stricmp(szCommand, "menu_close"))
	{
		PlaySound(ESound_MenuFirstClose);
	}
	else if(!stricmp(szCommand, "main_warning_open"))
	{
		PlaySound(ESound_MenuWarningOpen);
	}
	else if(!stricmp(szCommand, "main_warning_close"))
	{
		PlaySound(ESound_MenuWarningClose);
	}
	else if(!stricmp(szCommand, "menu_checkbox_select"))
	{
		PlaySound(Esound_MenuCheckbox);
	}
	else if(!stricmp(szCommand, "menu_difficulty"))
	{
		PlaySound(Esound_MenuDifficulty);
	}
	else if(!stricmp(szCommand, "menu_changeSlider"))
	{
		PlaySound(ESound_MenuSlider);
	}
	else if(!stricmp(szCommand, "menu_dropdown_select"))
	{
		PlaySound(ESound_MenuDropDown);
	}
	else if(!strcmp(szCommand, "EnterLoginScreen"))
	{
		m_multiplayerMenu->SetIsInLogin(true);
	}
	else if(!strcmp(szCommand, "LeaveLoginScreen"))
	{
		m_multiplayerMenu->SetIsInLogin(false);
	}
	else if(!strcmp(szCommand, "GotoLink_Terms"))
	{
		gEnv->pGame->GetIGameFramework()->ShowPageInBrowser("https://login.ign.com/tos.aspx");
	}
	else if(!strcmp(szCommand, "ResolutionChange"))
	{
		if(szArgs)
		{
			int accept(atoi(szArgs));
			if(accept==1)
			{
				//accept
				m_resolutionTimer = 0.0f;
				g_pGame->GetOptions()->SaveProfile();
				g_pGame->GetOptions()->UpdateFlashOptions();
			}
			else
			{
				//deny
				char status[64];
				sprintf(status, "%dx%d",m_iOldWidth, m_iOldHeight);
				
				g_pGame->GetOptions()->SetVideoMode(status);
				m_resolutionTimer = 0.0f;
				if(m_pCurrentFlashMenuScreen)
				{
					SFlashVarValue args[2] = {"SetVideoMode",status};
					m_pCurrentFlashMenuScreen->Invoke("Root.MainMenu.Options.SetOption",args,2);
				}

				g_pGame->GetOptions()->SaveProfile();
				g_pGame->GetOptions()->UpdateFlashOptions();
			}
		}
	}
	else if(!strcmp(szCommand, "FlashGetKeyFocus"))
	{
		gEnv->pConsole->EnableActivationKey(false);
		m_textfieldFocus = true;
		//show virtual keyboard
		if(!m_bVKMouseDown && m_pCurrentFlashMenuScreen && m_iGamepadsConnected>0)
		{
			m_pCurrentFlashMenuScreen->Invoke("enableVirtualKeyboard",true);
		}
	}
	else if(!strcmp(szCommand, "FlashLostKeyFocus"))
	{
		gEnv->pConsole->EnableActivationKey(true);
		m_textfieldFocus = false;
	}
	else if(!strcmp(szCommand, "UpdateModList"))
	{
		UpdateMods();
	}
	else if(!strcmp(szCommand, "LoadMod"))
	{
		string command("g_loadMod ");
		if(szArgs)
			command.append(szArgs);
		gEnv->pConsole->ExecuteString(command);
		CryLogAlways("Load mod '%s' and restart game...",szArgs);
	}
	else if(!strcmp(szCommand, "UnloadMOD"))
	{
		string command("g_loadMod ");
		gEnv->pConsole->ExecuteString(command);
		CryLogAlways("Unload mod '%s' and restart game...",szArgs);
	}
	else if(!strcmp(szCommand, "UpdateDefaultLevels"))
	{
		UpdateLevels(g_pGameCVars->g_quickGame_mode->GetString());
	}
	else if(!strcmp(szCommand, "UpdateLevels"))
	{
		string mode = szArgs;
		if(szArgs)
			UpdateLevels(szArgs);
	}
	else if(!strcmp(szCommand,"LayerCallBack"))
	{
		if(!strcmp(szArgs,"Clear"))
		{
			m_buttonPositions.clear();
			m_currentButtons.clear();
		}
		else if(!strcmp(szArgs,"AddOnTop"))
		{
			m_buttonPositions.push_back(m_currentButtons);
			m_currentButtons.clear();
		}
		else if(!strcmp(szArgs,"RemoveFromTop"))
		{
			if(!m_buttonPositions.empty())
			{
				m_currentButtons = m_buttonPositions.back();
				m_buttonPositions.pop_back();
			}
			else
			{
				m_currentButtons.clear();
			}
		}
	}
	else if(!strcmp(szCommand, "SetTemporaryResolution"))
	{
		CryFixedStringT<32> resolution(szArgs);
		int pos = resolution.find('x');
		if(pos != CryFixedStringT<64>::npos)
		{
			CryFixedStringT<32> width = resolution.substr(0, pos);
			CryFixedStringT<32> height = resolution.substr(pos+1, resolution.size());
			m_tempWidth = atoi(width.c_str());
			m_tempHeight = atoi(height.c_str());
		}
		SetAntiAliasingModes();
	}
	else if(!strcmp(szCommand,"BtnCallBack"))
	{
		string sTemp(szArgs);
		int iSep = sTemp.find("|");
		string sName = sTemp.substr(0,iSep);
		sTemp = sTemp.substr(iSep+1,sTemp.length());
		iSep = sTemp.find("|");
		string sX = sTemp.substr(0,iSep);
		string sY = sTemp.substr(iSep+1,sTemp.length());
		m_currentButtons.insert(ButtonPosMap::iterator::value_type(sName, Vec2(atoi(sX), atoi(sY))));











		//if (m_iGamepadsConnected && sName.substr(sName.length()-1, 1) == "1")
			//HighlightButton(m_currentButtons.find(sName));
	}
	else if(!strcmp(szCommand,"VirtualKeyboard"))
	{
		bool prevVal = m_bVirtualKeyboardFocus;
		m_bVirtualKeyboardFocus = strcmp(szArgs,"On")?false:true;
		if (m_bVirtualKeyboardFocus!=prevVal)
		{
			if (m_bVirtualKeyboardFocus && gEnv->pHardwareMouse)
			{
				gEnv->pHardwareMouse->DecrementCounter();
			}
			else
			{
				gEnv->pHardwareMouse->IncrementCounter();
			}
		}
		if(!m_bVirtualKeyboardFocus && m_pCurrentFlashMenuScreen)
		{
			m_pCurrentFlashMenuScreen->Invoke("onEnter","gamepad");
		}
	}
	else if(!strcmp(szCommand,"Back"))
	{
    /*if(m_QuickGame)
    {
      gEnv->pConsole->ExecuteString("g_quickGameStop");
      m_QuickGame = false;
    }*/
    if(m_multiplayerMenu)
      m_multiplayerMenu->HandleFSCommand(szCommand,szArgs);
    
    m_pCurrentFlashMenuScreen = m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART];
		if(m_pMusicSystem)
		{
			m_pMusicSystem->SetMood("menu_music", true, true);
			m_fMusicFirstTime = -1.0f;
		}
	}
	else if(!strcmp(szCommand,"Resume"))
	{
		if (m_bVirtualKeyboardFocus && gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->IncrementCounter();
		}

		gEnv->pConsole->EnableActivationKey(true);
		m_textfieldFocus = false;
		m_bVirtualKeyboardFocus = false;
		m_bUpdate = !m_bUpdate;
		if(m_bUpdate)
      ShowInGameMenu(m_bUpdate);
    else
      HideInGameMenuNextFrame(true);

		if(IActor *pPlayer = g_pGame->GetIGameFramework()->GetClientActor())
		{
			if(pPlayer->GetHealth() <= 0)
			{
				string lastSaveGame = string(GetLastInGameSave()->c_str());
				if(!lastSaveGame.size())
					lastSaveGame = g_pGame->GetLastSaveGame();
				if(lastSaveGame.size())
				{
					SAFE_HUD_FUNC(DisplayFlashMessage("", 2));	//removing warning / loading text
					m_sLoadSave.save = false;
					m_sLoadSave.name = lastSaveGame;
				}
			}
		}
	}
	else if(!strcmp(szCommand,"Restart"))
	{
		if(gEnv->bMultiplayer)
		{
			gEnv->pConsole->ExecuteString("sv_restart");
		}
		else
		{
			string file = gEnv->pGame->InitMapReloading();
			if(file.size())
			{
				m_sLastSaveGame = file;
				int pos = m_sLastSaveGame.rfind('/');
				if(pos != string::npos)
					m_sLastSaveGame = m_sLastSaveGame.substr(pos+1, m_sLastSaveGame.size() - pos);
			}
			HideInGameMenuNextFrame(false);
			m_bClearScreen = true;
		}
	}
	else if(!strcmp(szCommand,"Return"))
	{
		m_bIsEndingGameContext = true;
		if(INetChannel* pCh = g_pGame->GetIGameFramework()->GetClientChannel())
      pCh->Disconnect(eDC_UserRequested,"User left the game");
    g_pGame->GetIGameFramework()->EndGameContext();
		HideInGameMenuNextFrame(false);
		if(!gEnv->bMultiplayer && g_pGameCVars->g_spRecordGameplay)
			g_pGame->GetSPAnalyst()->StopRecording();
		m_bIsEndingGameContext = false;
		m_bClearScreen = true;
	}
	else if(!strcmp(szCommand,"Quit"))
	{
#ifdef SP_DEMO
		StartSplashScreenCountDown();
#else
		gEnv->pSystem->Quit();
#endif
	}
#ifdef SP_DEMO
	else if(!strcmp(szCommand,"preorder"))
	{
		gEnv->pGame->GetIGameFramework()->ShowPageInBrowser("http://www.buycrysis.ea.com");
		gEnv->pSystem->Quit();
	}
#endif
	else if(!strcmp(szCommand,"RollOver"))
	{
		PlaySound(ESound_RollOver);
	}
	else if(!strcmp(szCommand,"Click"))
	{
		PlaySound(ESound_Click1);
	}
	else if(!strcmp(szCommand,"ScreenChange"))
	{
		PlaySound(ESound_ScreenChange);
	}
	else if(!strcmp(szCommand,"AddProfile"))
	{
		AddProfile(szArgs);
	}
	else if(!strcmp(szCommand,"SelectProfile"))
	{
		SelectProfile(szArgs);
	}
	else if(!strcmp(szCommand,"DeleteProfile"))
	{
		if(szArgs)
			DeleteProfile(szArgs);
	}
	else if(!strcmp(szCommand, "SetDifficulty"))
	{
		if(szArgs)
			SetDifficulty(atoi(szArgs));
	}
	else if(!strcmp(szCommand,"UpdateSingleplayerDifficulties"))
	{
		UpdateSingleplayerDifficulties();
	}
	else if(!strcmp(szCommand,"StartSingleplayerGame"))
	{
		StartSingleplayerGame(szArgs);
	}
	else if(!strcmp(szCommand,"LoadGame"))
	{
		m_sLoadSave.save = false;
		m_sLoadSave.name = szArgs;
    HideInGameMenuNextFrame(false);
		if(gEnv->pGame->GetIGameFramework()->IsGameStarted())
			m_bClearScreen = true;
	}
	else if(!strcmp(szCommand,"DeleteSaveGame"))
	{
		if(szArgs)
			DeleteSaveGame(szArgs);
	}
	else if(!strcmp(szCommand,"SaveGame"))
	{
		m_sLoadSave.save = true;
		m_sLoadSave.name = szArgs;
	}
	else if(!strcmp(szCommand,"UpdateHUD"))
	{
		SetColorChanged();
		UpdateMenuColor();
	}
	else if(!strcmp(szCommand,"setSaveGameSortMode"))
	{
		if(szArgs)
		{
			int mode = atoi(szArgs);
			if(mode > eSAVE_COMPARE_FIRST && mode < eSAVE_COMPARE_LAST)
			{
				m_eSaveGameCompareMode = ESaveCompare(mode);
				UpdateSaveGames();
			}
		}
	}
	else if(!strcmp(szCommand,"setSaveGameSortModeUpDown"))
	{
		if(szArgs)
			m_bSaveGameSortUp = (atoi(szArgs))?true:false;
	}
	else if(!strcmp(szCommand,"CatchNextInput"))
	{
		string sTemp(szArgs);
		int iSep = sTemp.find("//");
		string s1 = sTemp.substr(0,iSep);
		string s2 = sTemp.substr(iSep+2,sTemp.length());
		m_bCatchNextInput = true;
		m_sActionToCatch = s2;
		m_sActionMapToCatch = s1;
	}
	else if(!strcmp(szCommand,"UpdateActionmaps"))
	{
		UpdateKeyMenu();
	}
	else if(!strcmp(szCommand,"UpdateSaveGames"))
	{
		UpdateSaveGames();
	}
	else if(!strcmp(szCommand,"SetCVar"))
	{
		string sTemp(szArgs);
		int iSep = sTemp.find("//");
		string s1 = sTemp.substr(0,iSep);
		string s2 = sTemp.substr(iSep+2,sTemp.length());
		SetCVar(s1, s2);
	}
	else if(!strcmp(szCommand,"TabStopPutMouse"))
	{
/*		string sTemp(szArgs);
		int iSep = sTemp.find("//");
		string s1 = sTemp.substr(0,iSep);
		string s2 = sTemp.substr(iSep+2,sTemp.length());

		float x;
		float y;

		TFlowInputData data = (TFlowInputData)s1;
		data.GetValueWithConversion(x);
		data = (TFlowInputData)s2;
		data.GetValueWithConversion(y);

		
		UpdateMousePosition();
*/
	}
	else if(!strcmp(szCommand,"SetKey"))
	{
		string sTmp(szArgs);
		int iSep = sTmp.find("//");
		string s1 = sTmp.substr(0,iSep);
		sTmp = sTmp.substr(iSep+2,sTmp.length());
		iSep = sTmp.find("//");
		string s2 = sTmp.substr(0,iSep);
		string s3 = sTmp.substr(iSep+2,sTmp.length());
		SaveActionToMap(s1, s2, s3);
	}
	else if(!strcmp(szCommand,"RestoreDefaults"))
	{
		RestoreDefaults();
	}


	// Credits
	else if(!strcmp(szCommand,"credits_start"))
	{
		PlaySound(ESound_MenuAmbience, false);

		if(m_pMusicSystem)
		{
			m_pMusicSystem->EndTheme(EThemeFade_StopAtOnce, 0, true);
			m_pMusicSystem->SetTheme("reactor_slow", false, false);
			m_pMusicSystem->SetMood("ambient", true, false);
		}	
	}
	else if(!strcmp(szCommand,"credits_stop"))
	{
		PlaySound(ESound_MenuAmbience);
		m_pMusicSystem->SetTheme("menu", true, false);
		m_pMusicSystem->SetMood("menu_music", true, true);
	}
	else if(!strcmp(szCommand,"credit_group1"))
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "reactor_slow") != 0)
			m_pMusicSystem->SetTheme("reactor_slow", false, false);

		m_pMusicSystem->SetMood("ambient", true, false);
	}
	else if(!strcmp(szCommand,"credit_group2"))												// R&D
	{
		m_pMusicSystem->SetMood("middle", false, false);
	}
	else if(!strcmp(szCommand,"credit_group3"))												// GAME PROGRAMMING
	{
		m_pMusicSystem->SetMood("action", false, false);
	}
	else if(!strcmp(szCommand,"credit_group4"))												// GAME DESIGN
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "reactor_slow") != 0)
			m_pMusicSystem->SetTheme("reactor_slow", false, false);

		m_pMusicSystem->SetMood("middle", false, false);
	}
	else if(!strcmp(szCommand,"credit_group4b"))											// ADD. GAME DESIGN
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "reactor_slow") != 0)
			m_pMusicSystem->SetTheme("reactor_slow", false, false);

		m_pMusicSystem->SetMood("ambient", false, false);
	}
	else if(!strcmp(szCommand,"credit_group5"))												// ART
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "ghosttown") != 0)
			m_pMusicSystem->SetTheme("ghosttown", false, false);

		m_pMusicSystem->SetMood("action", true, false);
	}
	else if(!strcmp(szCommand,"credit_group6"))												// ANIMATION
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "ghosttown") != 0)
			m_pMusicSystem->SetTheme("ghosttown", false, false);

		m_pMusicSystem->SetMood("middle", false, false);
	}
	else if(!strcmp(szCommand,"credit_group6b"))											// QA CRYTEK
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "ghosttown") != 0)
			m_pMusicSystem->SetTheme("ghosttown", false, false);

		m_pMusicSystem->SetMood("ambient", false, false);
	}
	else if(!strcmp(szCommand,"credit_group7"))												// PERFORMANCE CONS
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "mine_kyong") != 0)
			m_pMusicSystem->SetTheme("mine_kyong", false, false);

		m_pMusicSystem->SetMood("middle", true, false);
	}
	else if(!strcmp(szCommand,"credit_group8"))												// voice actors MOVEd !?
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "mine_kyong") != 0)
			m_pMusicSystem->SetTheme("mine_kyong", false, false);

		m_pMusicSystem->SetMood("ambient", false, false);
	}
	else if(!strcmp(szCommand,"credit_group9"))												// additional sound design
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "rescue_boatride_slow") != 0)
			m_pMusicSystem->SetTheme("rescue_boatride_slow", false, false);

		m_pMusicSystem->SetMood("ambient", true, false);
	}
	else if(!strcmp(szCommand,"credit_group10"))											// CRYTEK FAMILY
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "rescue_boatride_slow") != 0)
			m_pMusicSystem->SetTheme("rescue_boatride_slow", false, false);

		m_pMusicSystem->SetMood("middle", false, false);
	}
	else if(!strcmp(szCommand,"credit_group11"))											// SEPCIAL THANKS (NVIDIA)
	{
	}
	else if(!strcmp(szCommand,"credit_group12"))											// FROM CRYTEK TEAM
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "rescue_boatride_slow") != 0)
			m_pMusicSystem->SetTheme("rescue_boatride_slow", false, false);

		m_pMusicSystem->SetMood("ambient", false, false);
	}
	else if(!strcmp(szCommand,"credit_group13"))											// 3RD Party SOFTWARE
	{
	}
	else if(!strcmp(szCommand,"credit_group14"))											// EA CREDITS
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "reinforcements") != 0)
		m_pMusicSystem->SetTheme("reinforcements", false, false);

		m_pMusicSystem->SetMood("middle", true, false);
	}
	else if(!strcmp(szCommand,"credit_group14b"))											// ASIA MARKETING
	{
	}
	else if(!strcmp(szCommand,"credit_group14c"))											// THAI MARKETING
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "reinforcements") != 0)
			m_pMusicSystem->SetTheme("reinforcements", false, false);

		m_pMusicSystem->SetMood("ambient", false, false);
	}
	else if(!strcmp(szCommand,"credit_group15"))										// LOCALIZATION
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "airfield_battle") != 0)
			m_pMusicSystem->SetTheme("airfield_battle", false, false);

		m_pMusicSystem->SetMood("middle", true, false);
	}
	else if(!strcmp(szCommand,"credit_group16"))										//QA
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "airfield_battle") != 0)
			m_pMusicSystem->SetTheme("airfield_battle", false, false);

		m_pMusicSystem->SetMood("action", false, false);
	}
	else if(!strcmp(szCommand,"credit_group17"))										// COMPLIENCE LEADS
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "airfield_battle") != 0)
			m_pMusicSystem->SetTheme("airfield_battle", false, false);

		m_pMusicSystem->SetMood("ambient", false, false);
	}
	else if(!strcmp(szCommand,"credit_group17b"))										// EALA MASTERING
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "rescue_swamp") != 0)
			m_pMusicSystem->SetTheme("rescue_swamp", false, false);

		m_pMusicSystem->SetMood("middle", true, false);
	}
	else if(!strcmp(szCommand,"credit_group18"))										// TECHNICAL COMPLIENCE
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "rescue_swamp") != 0)
			m_pMusicSystem->SetTheme("rescue_swamp", false, false);

		m_pMusicSystem->SetMood("action", false, false);
	}
	else if(!strcmp(szCommand,"credit_group18b"))										// GAMEPLAY DIVISION
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "rescue_swamp") != 0)
			m_pMusicSystem->SetTheme("rescue_swamp", false, false);

		m_pMusicSystem->SetMood("ambient", false, false);
	}
	else if(!strcmp(szCommand,"credit_group18c"))										// NETWORK DIVISION
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "sewers") != 0)
			m_pMusicSystem->SetTheme("sewers", false, false);

		m_pMusicSystem->SetMood("middle", true, false);
	}
	else if(!strcmp(szCommand,"credit_group19"))										// ECG SUBMISSION
	{
	}
	else if(!strcmp(szCommand,"credit_group19b"))										// VOICE TALENT
	{
		if (strcmp(m_pMusicSystem->GetTheme(), "sewers") != 0)
			m_pMusicSystem->SetTheme("sewers", false, false);

		m_pMusicSystem->SetMood("action", false, false);
	}
	else if(!strcmp(szCommand,"credit_group20"))										// SPECIAL THANKS
	{
	}

	// Credits End

	else if(m_multiplayerMenu && m_multiplayerMenu->HandleFSCommand(szCommand,szArgs))
  {
    //handled by Multiplayer menu
  }
  else 
	{
		// Dev mode: we clicked on a button which should execute something like "map MapName"
		gEnv->pConsole->ExecuteString(szCommand);
	}
}

//-----------------------------------------------------------------------------------------------------

bool CFlashMenuObject::Load()
{
	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated()) return true;

	m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING] = new CFlashMenuScreen;

	if(g_pGameCVars->g_skipIntro==1)
	{
		m_stateEntryMovies = eEMS_Stop;
	}
	InitStartMenu();
	if(g_pGameCVars->g_resetActionmapOnStart==1)
		RestoreDefaults();
	return true;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::InitStartMenu()
{
	for(int iSound=0; iSound<ESound_Last; iSound++)
	{
		m_soundIDs[iSound] = INVALID_SOUNDID;
	}

	if(!m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART])
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART] = new CFlashMenuScreen;

	if(m_stateEntryMovies==eEMS_Done)
		m_stateEntryMovies = eEMS_Stop;

	if(!m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->IsLoaded())
	{
#ifdef CRYSIS_BETA
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Load("Libs/UI/Menus_StartMenu_Beta.gfx");
#else
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Load("Libs/UI/Menus_StartMenu.gfx");
#endif
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->GetFlashPlayer()->SetFSCommandHandler(this);

		// not working yet, gets reset on loadMovie within .swf/.gfx
		// m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->GetFlashPlayer()->SetLoadMovieHandler(this);

		if(g_pGameCVars->g_debugDirectMPMenu)
		{
			m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->SetVariable("MainWindow",2);
			m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->SetVariable("SubWindow",2);
		}

		char strProductVersion[256];
		gEnv->pSystem->GetProductVersion().ToString(strProductVersion);
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("setGameVersion", strProductVersion);
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->SetVariable("GamepadAvailableVar",m_iGamepadsConnected?1:0);
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->SetVariable("PBLoaded",gEnv->pNetwork->IsPbInstalled());
#if defined(WIN64)
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("set64Bit",true);
#endif

		SModInfo info;
		if(g_pGame->GetIGameFramework()->GetModInfo(&info))
		{
			m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("addLoadedModText",info.m_name);
		}

		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("Directx10", (gEnv->pRenderer->GetRenderType() == eRT_DX10)?true:false);
		time_t today = time(NULL);
		struct tm theTime;
		theTime = *localtime(&today);
		SFlashVarValue args[3] = {(theTime.tm_mon+1),theTime.tm_mday, (theTime.tm_year+1900)};
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("setToday", args, 3);
		
		if(!gEnv->pSystem->IsEditor())
			UpdateMenuColor();
	}

//  m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->SetVariable("Authorized","1");
  /*m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->SetVariable("MainWindow","2");
	m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->SetVariable("SubWindow","2");*/

	m_pCurrentFlashMenuScreen = m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART];
	HardwareEvaluation();
  if(m_multiplayerMenu)
  {
    m_multiplayerMenu->SetCurrentFlashScreen(m_pCurrentFlashMenuScreen->GetFlashPlayer(),false);
  }
	m_bIgnoreEsc = true;
	m_bExclusiveVideo = false;

	if(g_pGame->GetOptions())
		g_pGame->GetOptions()->UpdateFlashOptions();
	SetDisplayFormats();
	SetAntiAliasingModes();

	SetProfile();
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::DestroyStartMenu()
{
	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->IsLoaded())
	{
    if(m_multiplayerMenu)
      m_multiplayerMenu->SetCurrentFlashScreen(0,false);
		SAFE_HARDWARE_MOUSE_FUNC(DecrementCounter());
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Unload();
	}

	SAFE_DELETE(m_pSubtitleScreen);

	m_bIgnoreEsc = false;
	m_bDestroyStartMenuPending = false;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::InitIngameMenu()
{
	for(int iSound=0; iSound<ESound_Last; iSound++)
	{
		m_soundIDs[iSound] = INVALID_SOUNDID;
	}

	if(!m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME])
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME] = new CFlashMenuScreen;

	if(!m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->IsLoaded())
	{
		SAFE_HARDWARE_MOUSE_FUNC(IncrementCounter());

#ifdef CRYSIS_BETA
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Load("Libs/UI/Menus_IngameMenu_Beta.gfx");
#else
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Load("Libs/UI/Menus_IngameMenu.gfx");
#endif
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->GetFlashPlayer()->SetFSCommandHandler(this);
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("setLineColor", SFlashVarValue(g_pGameCVars->hud_colorLine));
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("setOverColor", SFlashVarValue(g_pGameCVars->hud_colorOver));
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("setTextColor", SFlashVarValue(g_pGameCVars->hud_colorText));

		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->SetVariable("GamepadAvailableVar",m_iGamepadsConnected?true:false);
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("setGameMode", gEnv->bMultiplayer?"MP":"SP");

		if(g_pGame->GetIGameFramework()->CanSave() == false)
			m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("disableSaveGame", true);

		char strProductVersion[256];
		gEnv->pSystem->GetProductVersion().ToString(strProductVersion);
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("setGameVersion", strProductVersion);
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->SetVariable("PBLoaded",gEnv->pNetwork->IsPbInstalled());
#if defined(WIN64)
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("set64Bit",true);
#endif

		SModInfo info;
		if(g_pGame->GetIGameFramework()->GetModInfo(&info))
		{
			m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("addLoadedModText",info.m_name);
		}

		if(m_pPlayerProfileManager)
		{
			IPlayerProfile *pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
			if(pProfile)
				m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("setActiveProfile", GetMappedProfileName(pProfile->GetName()));
		}

		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("Directx10", (gEnv->pRenderer->GetRenderType() == eRT_DX10)?true:false);
		time_t today = time(NULL);
		struct tm theTime;
		theTime = *localtime(&today);
		SFlashVarValue args[3] = {(theTime.tm_mon+1),theTime.tm_mday, (theTime.tm_year+1900)};
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("setToday", args, 3);


		if(m_pPlayerProfileManager)
		{
			IPlayerProfile *pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
			if(pProfile)
				m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("setActiveProfile", GetMappedProfileName(pProfile->GetName()));
		}
	}
	m_pCurrentFlashMenuScreen = m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME];
	HardwareEvaluation();
	if(m_multiplayerMenu)
	{
		m_multiplayerMenu->SetCurrentFlashScreen(m_pCurrentFlashMenuScreen->GetFlashPlayer(),true);
	}

	g_pGame->GetOptions()->UpdateFlashOptions();
	SetDisplayFormats();
	SetAntiAliasingModes();
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::DestroyIngameMenu()
{
	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->IsLoaded())
	{
    if(m_multiplayerMenu)
      m_multiplayerMenu->SetCurrentFlashScreen(0,true);
		SAFE_HARDWARE_MOUSE_FUNC(DecrementCounter());
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Unload();
	}
  if(g_pGame->GetIGameFramework()->IsGameStarted())
    ReloadHUDMovies();
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::DestroyMenusAtNextFrame()
{
	m_bDestroyStartMenuPending		= true;
	m_bDestroyInGameMenuPending		= true;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::HardwareEvaluation()
{
	if(!m_pCurrentFlashMenuScreen->IsLoaded())
		return;

	int hardware = gEnv->pSystem->GetMaxConfigSpec();
	if ((gEnv->pRenderer->GetFeatures() & (RFT_HW_PS2X | RFT_HW_PS30)) == 0)
		hardware = 1;
	m_pCurrentFlashMenuScreen->Invoke("Root.MainMenu.Options.Hardware", hardware);
}

//-----------------------------------------------------------------------------------------------------

CMPHub* CFlashMenuObject::GetMPHub()const
{
  return m_multiplayerMenu;
}

//-----------------------------------------------------------------------------------------------------

CFlashMenuScreen *CFlashMenuObject::GetMenuScreen(EMENUSCREEN screen) const 
{ 
	return m_apFlashMenuScreens[screen]; 
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::UpdateMenuColor()
{
	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->IsLoaded())
	{
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->CheckedInvoke("setLineColor", SFlashVarValue(g_pGameCVars->hud_colorLine));
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("setOverColor", SFlashVarValue(g_pGameCVars->hud_colorOver));
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Invoke("setTextColor", SFlashVarValue(g_pGameCVars->hud_colorText));
	}
	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->IsLoaded())
	{
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->CheckedInvoke("setLineColor", SFlashVarValue(g_pGameCVars->hud_colorLine));
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Invoke("setOverColor", SFlashVarValue(g_pGameCVars->hud_colorOver));
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Invoke("setTextColor", SFlashVarValue(g_pGameCVars->hud_colorText));
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Invoke("resetColor");
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->GetFlashPlayer()->Advance(0.2f);
	}
	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->IsLoaded())
	{
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->CheckedInvoke("setLineColor", SFlashVarValue(g_pGameCVars->hud_colorLine));
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->Invoke("setOverColor", SFlashVarValue(g_pGameCVars->hud_colorOver));
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->Invoke("setTextColor", SFlashVarValue(g_pGameCVars->hud_colorText));
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->Invoke("resetColor");
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDRESET]->GetFlashPlayer()->Advance(0.2f);
	}
	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->IsLoaded())
	{
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->CheckedInvoke("setLineColor", SFlashVarValue(g_pGameCVars->hud_colorLine));
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("setOverColor", SFlashVarValue(g_pGameCVars->hud_colorOver));
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME]->Invoke("setTextColor", SFlashVarValue(g_pGameCVars->hud_colorText));
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::OnPostUpdate(float fDeltaTime)
{
	if(gEnv->pSystem->IsEditor() || gEnv->pSystem->IsDedicated()) return;

	fDeltaTime = gEnv->pTimer->GetFrameTime(ITimer::ETIMER_UI);

	if (ICVar* requireinputdevice = gEnv->pConsole->GetCVar("sv_requireinputdevice"))
	{
		if(!strcmpi(requireinputdevice->GetString(), "gamepad") && !m_iGamepadsConnected && !IsActive())
			ShowInGameMenu(true);
	}

	if(m_bLoadingDone) //might be necessary to re-pause
	{
		if(!gEnv->bMultiplayer && !g_pGame->GetIGameFramework()->IsGamePaused())
			g_pGame->GetIGameFramework()->PauseGame(true ,false);
	}

	if(m_bDestroyInGameMenuPending && !m_bIgnorePendingEvents)
	{
		// Do no stop update if user hasn't asked the game to start!
		if(!m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->IsLoaded())
			m_bUpdate = false; 
		DestroyIngameMenu();
		m_bDestroyInGameMenuPending = false;
	}

	if(m_bDestroyStartMenuPending && !m_bIgnorePendingEvents)
		DestroyStartMenu();

	int width = gEnv->pRenderer->GetWidth();
	int height = gEnv->pRenderer->GetHeight();

	bool doesDiffer = (m_iWidth!=width || m_iHeight!=height);

	if(doesDiffer)
	{
		UpdateRatio();
	}

	if(m_bLoadingDone && !g_pGameCVars->hud_startPaused)
	{
		CloseWaitingScreen();
		return;
	}

	if(m_stateEntryMovies!=eEMS_Done)
	{
		if(m_stateEntryMovies!=eEMS_Stop && m_stateEntryMovies!=eEMS_GameStop && m_stateEntryMovies!=eEMS_GameDone)
		{
			if(m_pVideoPlayer==NULL)
			{
				NextIntroVideo();
				return;
			}
			IVideoPlayer::EPlaybackStatus status = m_pVideoPlayer->GetStatus();
			if(status == IVideoPlayer::PBS_STOPPED)
			{
				//restart video, because something went wrong with out
				m_stateEntryMovies = (EEntryMovieState)(((int)m_stateEntryMovies) - 1);
				NextIntroVideo();
				return;
			}
			if(status == IVideoPlayer::PBS_ERROR || status == IVideoPlayer::PBS_FINISHED)
			{
				StopVideo();
				ColorF cBlack(Col_Black);
				gEnv->pRenderer->ClearBuffer(FRT_CLEAR | FRT_CLEAR_IMMEDIATE,&cBlack);
				return;
			}
			if(m_pVideoPlayer)
			{
				m_pVideoPlayer->Render();
				DisplaySubtitles(fDeltaTime);
				return;
			}
			else
			{
				m_stateEntryMovies = eEMS_Stop;
			}
		}
	}

	if(m_bTutorialVideo)
	{
		IVideoPlayer::EPlaybackStatus status = m_pVideoPlayer->GetStatus();
		if(status == IVideoPlayer::PBS_FINISHED)
		{
			StopTutorialVideo();
		}
	}

	if(m_stateEntryMovies==eEMS_Stop)
	{
		m_stateEntryMovies = eEMS_Done;
		SAFE_HARDWARE_MOUSE_FUNC(IncrementCounter());
		const char* movie = VALUE_BY_KEY(m_stateEntryMovies, gMovies);
		if(movie)
			PlayVideo(movie, false, IVideoPlayer::LOOP_PLAYBACK);

		if(m_pMusicSystem)
		{
			m_pMusicSystem->LoadFromXML("music/act1.xml",true, false);
			m_pMusicSystem->SetTheme("menu", true, false);
			m_pMusicSystem->SetDefaultMood("menu_music_1st_time");
			m_fMusicFirstTime = gEnv->pTimer->GetAsyncTime().GetSeconds();
		}	

		if (gEnv->pSoundSystem)
		{
			// prevents to play ambience if window got minimized or alt-tapped
			if (!gEnv->pSoundSystem->IsPaused())
				PlaySound(ESound_MenuAmbience);
		}
	}

	if(m_stateEntryMovies==eEMS_GameStop)
	{
		// map load
		m_stateEntryMovies = eEMS_GameDone;
		m_bUpdate = false;
		if(m_pMusicSystem)
			m_pMusicSystem->SetMood("menu_music", true, true);
		PlaySound(ESound_MenuAmbience);

		gEnv->pConsole->ExecuteString("map island nonblocking");

		return;
	}

	if(m_stateEntryMovies==eEMS_GameDone)
	{
		//game intro video was shown and loading is pending
		return;
	}

#ifdef SP_DEMO
	if(m_splashScreenTimer>0.0f)
	{
		if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->IsLoaded())
		{
			m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART]->Unload();
		}
		if(!m_apFlashMenuScreens[MENUSCREEN_FRONTENDSPLASH] || !m_apFlashMenuScreens[MENUSCREEN_FRONTENDSPLASH]->IsLoaded())
		{
			m_apFlashMenuScreens[MENUSCREEN_FRONTENDSPLASH] = new CFlashMenuScreen();
			m_apFlashMenuScreens[MENUSCREEN_FRONTENDSPLASH]->Load("Libs/UI/SplashScreen_Singleplayer.gfx");
			m_pCurrentFlashMenuScreen = m_apFlashMenuScreens[MENUSCREEN_FRONTENDSPLASH];
			m_apFlashMenuScreens[MENUSCREEN_FRONTENDSPLASH]->GetFlashPlayer()->SetFSCommandHandler(this);

		}
		m_splashScreenTimer -= fDeltaTime;
		if(m_splashScreenTimer<=0.0f)
		{
			gEnv->pSystem->Quit();
		}
	}
	else
#endif
	if(!IsOnScreen(MENUSCREEN_FRONTENDINGAME) && !IsOnScreen(MENUSCREEN_FRONTENDSTART))
  {
    if(!g_pGame->GetIGameFramework()->StartedGameContext() && !g_pGame->GetIGameFramework()->GetClientChannel() && !m_bIgnorePendingEvents)// if nothing happens we should show user a menu so she can interact with the game
    {
      g_pGame->DestroyHUD();
      ShowMainMenu();
    }
  }

	if(m_resolutionTimer>0.0)
	{
		int resBefore = (int)(m_resolutionTimer+0.5f);
		m_resolutionTimer -= fDeltaTime;
		int resAfter = (int)(m_resolutionTimer+0.5f);
		if(m_resolutionTimer<=0.0f)
		{
			char status[64];
			sprintf(status, "%dx%d",m_iOldWidth, m_iOldHeight);
			g_pGame->GetOptions()->SetVideoMode(status);
			m_resolutionTimer = 0.0f;
			if(m_pCurrentFlashMenuScreen)
			{
				m_pCurrentFlashMenuScreen->Invoke("closeErrorMessageYesNo");
				SFlashVarValue args[2] = {"SetVideoMode",status};
				m_pCurrentFlashMenuScreen->Invoke("Root.MainMenu.Options.SetOption",args,2);
			}
			g_pGame->GetOptions()->SaveProfile();
			g_pGame->GetOptions()->UpdateFlashOptions();
		}
		else if(resBefore!=resAfter)
		{
			//update countdown
			if(m_pCurrentFlashMenuScreen)
			{
				ILocalizationManager* pLocMgr = gEnv->pSystem->GetLocalizationManager();
				wstring localizedString, finalString;
				pLocMgr->LocalizeLabel("@ui_menu_KEEPRESOLUTION", localizedString);
				char status[64];
				sprintf(status, "%d",resBefore);
				wstring outString;
				outString.resize(strlen(status));
				wchar_t* dst = outString.begin();
				const char* src = status;
				while (const wchar_t c=(wchar_t)(*src++))
				{
					*dst++ = c;
				}

				pLocMgr->FormatStringMessage(finalString, localizedString, outString, 0, 0, 0);
				m_pCurrentFlashMenuScreen->Invoke("setErrorTextNonLocalized", SFlashVarValue(finalString));
			}
		}
	}

	if(m_bUpdate && m_pCurrentFlashMenuScreen && m_pCurrentFlashMenuScreen == m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME])
	{
		bool connected = g_pGame->GetGameRules()?true:false;
		//update the ingame menu background, show if not connected
		m_pCurrentFlashMenuScreen->Invoke("setServerConnected",connected);
	}

	if(m_sLoadSave.name.empty() == false)
	{
		string temp = m_sLoadSave.name;
		m_sLoadSave.name.resize(0);
		if(m_sLoadSave.save)
		{
			bool valid = SaveGame(temp.c_str());
			if(valid)
			{
				// we turn off buffer swapping and don't render the menu for 2 frames
				// to prevent screenshots/thumbnails with menu on
				// m_nRenderAgainFrameId will additionally be set to 0 when screenshot is done
				// but there is also a safety check below
				m_nRenderAgainFrameId = gEnv->pRenderer->GetFrameID(false) + 2;
				gEnv->pRenderer->EnableSwapBuffers(false);

				// show messsage, that game saving was successful
				ShowMenuMessage("@ui_GAMESAVEDSUCCESSFUL");
			}
		}
		else
			LoadGame(temp.c_str());
	}

	int curFrameID = gEnv->pRenderer->GetFrameID(false);
	// if (curFrameID == m_nLastFrameUpdateID)
	// 	return;
	m_nLastFrameUpdateID = curFrameID;

	if (m_nBlackGraceFrames > 0)
	{
/*		IUIDraw *pUIDraw = gEnv->pGame->GetIGameFramework()->GetIUIDraw();

		pUIDraw->PreRender();
		//const uchar *pImage=m_pAVIReader->QueryFrame();
		pUIDraw->DrawImage(*pImage,0,0,800,600,0,1,1,1,1);
		pUIDraw->PostRender();
*/
		ColorF cBlack(Col_Black);
		gEnv->pRenderer->ClearBuffer(FRT_CLEAR | FRT_CLEAR_IMMEDIATE,&cBlack);

		if (curFrameID >= m_nBlackGraceFrames)
			m_nBlackGraceFrames = 0;
	}

	if(NULL == m_pCurrentFlashMenuScreen || !m_pCurrentFlashMenuScreen->IsLoaded())
	{
		UpdateLaptop(fDeltaTime);
		return;
	}

	if(m_pMusicSystem && m_fMusicFirstTime != -1.0f)
	{
		if(gEnv->pTimer->GetAsyncTime().GetSeconds() >= m_fMusicFirstTime+78.0f)
		{
			m_pMusicSystem->SetMood("menu_music", true, true);
			m_fMusicFirstTime = -1.0f;
		}
	}
		
	if (m_nRenderAgainFrameId > 0)
	{
		if (curFrameID >= m_nRenderAgainFrameId)
		{
			m_nRenderAgainFrameId = 0;
			gEnv->pRenderer->EnableSwapBuffers(true);
		}
		else
			return;
	}
	
	if(m_bUpdate)
	{
		// handle key repeat
		if (m_repeatEvent.keyId!=eKI_Unknown)
		{
			float repeatSpeed = 40.0;
			float nextTimer = (1000.0f/repeatSpeed); // repeat speed

			float now = gEnv->pTimer->GetAsyncTime().GetMilliSeconds();

			if (now - m_repeatTimer > nextTimer)
			{
				OnInputEvent(m_repeatEvent);
				m_repeatTimer = now + nextTimer;
			}
		}

		if(m_pCurrentFlashMenuScreen != m_apFlashMenuScreens[MENUSCREEN_FRONTENDINGAME])
		{
			if(m_pVideoPlayer)
			{
				m_pVideoPlayer->Render();
				DisplaySubtitles(fDeltaTime);
			}
		}

		if(m_bExclusiveVideo==false && m_pCurrentFlashMenuScreen && m_pCurrentFlashMenuScreen->IsLoaded())
		{
			m_pCurrentFlashMenuScreen->GetFlashPlayer()->Advance(fDeltaTime);
			m_pCurrentFlashMenuScreen->GetFlashPlayer()->Render();
		}
	}
	else if (m_nBlackGraceFrames > 0)
	{
		ColorF cBlack(Col_Black);
		gEnv->pRenderer->ClearBuffer(FRT_CLEAR | FRT_CLEAR_IMMEDIATE,&cBlack);
		if (curFrameID >= m_nBlackGraceFrames)
			m_nBlackGraceFrames = 0;
	}

	if(m_bExclusiveVideo==false && m_pFlashPlayer)
	{
		m_pFlashPlayer->Advance(fDeltaTime);
		m_pFlashPlayer->Render();
	}

	UpdateLaptop(fDeltaTime);
	UpdateNetwork(fDeltaTime);

	// When we quit the game for the main menu or we load a game while we are already playing, there is a
	// few frames where there is no more ingame menu and the main menu is not yet initialized/rendered.
	// We fix the problem by displaying a black screen during this time.
	if(m_bClearScreen)
	{
		ColorF cClear(Col_Black);
		gEnv->pRenderer->ClearBuffer(FRT_CLEAR|FRT_CLEAR_IMMEDIATE,&cClear);
	}
	if(m_pCurrentFlashMenuScreen == m_apFlashMenuScreens[MENUSCREEN_FRONTENDSTART])
	{
		m_bClearScreen = false;
	}

	// TODO: just some quick code from Craig to get connection state displayed; please move to a suitable location later
	/*IGameFramework * pGFW = g_pGame->GetIGameFramework();
	if (INetChannel * pNC = pGFW->GetClientChannel())
	{
		bool show = true;
		char status[512];
		switch (pNC->GetChannelConnectionState())
		{
		case eCCS_StartingConnection:
			strcpy(status, "Waiting for server");
			break;
		case eCCS_InContextInitiation:
			{
				const char * state = "<unknown state>";
				switch (pNC->GetContextViewState())
				{
				case eCVS_Initial:
					state = "Requesting Game Environment";
					break;
				case eCVS_Begin:
					state = "Receiving Game Environment";
					break;
				case eCVS_EstablishContext:
					state = "Loading Game Assets";
					break;
				case eCVS_ConfigureContext:
					state = "Configuring Game Settings";
					break;
				case eCVS_SpawnEntities:
					state = "Spawning Entities";
					break;
				case eCVS_PostSpawnEntities:
					state = "Initializing Entities";
					break;
				case eCVS_InGame:
					state = "In Game";
					break;
				}
				sprintf(status, "%s [%d]", state, pNC->GetContextViewStateDebugCode());
			}
			break;
		case eCCS_InGame:
			show = false;
			strcpy(status, "In Game");
			break;
		case eCCS_Disconnecting:
			strcpy(status, "Disconnecting");
			break;
		default:
			strcpy(status, "Unknown State");
			break;
		}

		float white[] = {1,1,1,1};
		if (show)
			gEnv->pRenderer->Draw2dLabel( 10, 750, 1, white, false, "Connection State: %s", status );
	}*/
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::ShowMenuMessage(const char *message)
{
	if(m_pCurrentFlashMenuScreen && m_pCurrentFlashMenuScreen->IsLoaded())
		m_pCurrentFlashMenuScreen->Invoke("showStatusMessage", message);
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::SetDisplayFormats()
{
	if(!m_pCurrentFlashMenuScreen)
		return;

	m_pCurrentFlashMenuScreen->Invoke("clearResolutionList");

	SDispFormat *formats = NULL;
	int numFormats = gEnv->pRenderer->EnumDisplayFormats(NULL);
	if(numFormats)
	{
		formats = new SDispFormat[numFormats];
		gEnv->pRenderer->EnumDisplayFormats(formats);
	}

	int lastWidth, lastHeight;
	lastHeight = lastWidth = -1;

	char buffer[64];
	for(int i = 0; i < numFormats; ++i)
	{
		if(lastWidth == formats[i].m_Width && lastHeight == formats[i].m_Height) //only pixel resolution, don't care about color
			continue;
		if(formats[i].m_Width < 800)
			continue;

		itoa(formats[i].m_Width, buffer, 10);
		string command(buffer);
		command.append("x");
		itoa(formats[i].m_Height, buffer, 10);
		command.append(buffer);
		m_pCurrentFlashMenuScreen->Invoke("addResolution", command.c_str());

		lastHeight = formats[i].m_Height;
		lastWidth = formats[i].m_Width;
	}

	if(formats)
		delete[] formats;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::SetAntiAliasingModes()
{
	m_pCurrentFlashMenuScreen->Invoke("_root.Root.MainMenu.Options.clearAntiAliasingModes");
	m_fsaaModes.clear();

	SDispFormat DispFmt;

	DispFmt.m_Width=m_tempWidth;
	DispFmt.m_Height=m_tempHeight;
	DispFmt.m_BPP=32;

	SAAFormat *aaFormats = NULL;
	int numAAFormats = gEnv->pRenderer->EnumAAFormats(DispFmt,NULL);
	if(numAAFormats)
	{
		aaFormats = new SAAFormat[numAAFormats];
		gEnv->pRenderer->EnumAAFormats(DispFmt,aaFormats);
	}

	for(int a = 0; a < numAAFormats; ++a)
	{
		m_fsaaModes[aaFormats[a].szDescr] = FSAAMode(aaFormats[a].nSamples, aaFormats[a].nQuality);
		m_pCurrentFlashMenuScreen->Invoke("_root.Root.MainMenu.Options.addAntiAliasingMode", aaFormats[a].szDescr);
	}

	if(aaFormats)
		delete[] aaFormats;
}

//-----------------------------------------------------------------------------------------------------

CFlashMenuObject::FSAAMode CFlashMenuObject::GetFSAAMode(const char* name)
{
	string mode(name);
	std::map<string,FSAAMode>::iterator it = m_fsaaModes.begin();
	std::map<string,FSAAMode>::iterator end = m_fsaaModes.end();
	for(; it != end; ++it)
	{
		if(it->first == mode)
			return it->second;
	}
	return FSAAMode();
}

//-----------------------------------------------------------------------------------------------------

string CFlashMenuObject::GetFSAAMode(int samples, int quality)
{
	std::map<string,FSAAMode>::iterator it = m_fsaaModes.begin();
	std::map<string,FSAAMode>::iterator end = m_fsaaModes.end();
	for(; it != end; ++it)
	{
		if(it->second.samples == samples && it->second.quality == quality)
			return it->first;
	}
	return string();
}

//-----------------------------------------------------------------------------------------------------

class CFlashLoadMovieImage : public IFlashLoadMovieImage
{
public:
	CFlashLoadMovieImage(ISaveGameThumbailPtr pThumbnail) : m_pThumbnail(pThumbnail)
	{
		SwapRB();
	}

	virtual void Release()
	{
		delete this;
	}

	virtual int GetWidth() const 
	{
		return m_pThumbnail->GetWidth();
	}

	virtual int GetHeight() const 
	{
		return m_pThumbnail->GetHeight();
	}

	virtual int GetPitch() const 
	{
		return m_pThumbnail->GetWidth() * m_pThumbnail->GetDepth();
	}

	virtual void* GetPtr() const 
	{
		return (void*) m_pThumbnail->GetImageData();
	}

	// Scaleform Textures must be RGB / RGBA.
	// thumbnails are BGR/BGRA -> swap necessary
	virtual EFmt GetFormat() const
	{
		const int depth = m_pThumbnail->GetDepth();
		if (depth == 3)
			return eFmt_RGB_888;
		if (depth == 4)
			return eFmt_ARGB_8888;
		return eFmt_None;
	}

	void SwapRB()
	{
		if (m_pThumbnail)
		{
			// Scaleform Textures must be RGB / RGBA.
			// thumbnails are BGR/BGRA -> swap necessary
			const int depth = m_pThumbnail->GetDepth();
			const int height = m_pThumbnail->GetHeight();
			const int width = m_pThumbnail->GetWidth();
			const int bpl = depth * width;
			if (depth == 3 || depth == 4)
			{
				uint8* pData = const_cast<uint8*> (m_pThumbnail->GetImageData());
				for (int y = 0; y < height; ++y)
				{
					uint8* pCol = pData + bpl * y;
					for (int x = 0; x< width; ++x, pCol+=depth)
					{
						std::swap(pCol[0], pCol[2]);
					}
				}
			}
		}
	}

	ISaveGameThumbailPtr m_pThumbnail;
};

//-----------------------------------------------------------------------------------------------------

IFlashLoadMovieImage* CFlashMenuObject::LoadMovie(const char* pFilePath)
{
	bool bResolved = false;
	if (stricmp (PathUtil::GetExt(pFilePath), "thumbnail") == 0)
	{
		string saveGameName = pFilePath;
		PathUtil::RemoveExtension(saveGameName);

		if (m_pPlayerProfileManager == 0)
			return 0;

		IPlayerProfile *pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
		if (pProfile == 0)
			return 0;

		ISaveGameEnumeratorPtr pSGE = pProfile->CreateSaveGameEnumerator();
		if (pSGE == 0)
			return 0;
		ISaveGameThumbailPtr pThumbnail = pSGE->GetThumbnail(saveGameName);
		if (pThumbnail == 0)
			return 0;
		return new CFlashLoadMovieImage(pThumbnail);
	}
	return 0;
}

//-----------------------------------------------------------------------------------------------------

static const int  THUMBNAIL_DEFAULT_WIDTH  = 256;   // 16:9 
static const int  THUMBNAIL_DEFAULT_HEIGHT = 144;   //
static const int  THUMBNAIL_DEFAULT_DEPTH = 4;   // write out with alpha
static const bool THUMBNAIL_KEEP_ASPECT_RATIO = true; // keep renderer's aspect ratio and surround with black borders
static const char LEVELSTART_POSTFIX[] = "_crysis.CRYSISJMSF";
static const size_t LEVELSTART_POSTFIX_LEN = sizeof(LEVELSTART_POSTFIX)-1;

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::OnSaveGame(ISaveGame* pSaveGame)
{
	int fc = gEnv->pRenderer->GetFrameID(false);
	bool bUseScreenShot = true;

	const char* saveGameName = pSaveGame->GetFileName();
	if (saveGameName)
	{
		const char* levelName = gEnv->pGame->GetIGameFramework()->GetLevelName();
		ILevelInfo* pLevelInfo = gEnv->pGame->GetIGameFramework()->GetILevelSystem()->GetLevelInfo(levelName);
		
		if (pLevelInfo)
		{
			// check if it ends with '_levelstart.CRYSISJMSF'
			const size_t len = strlen(saveGameName);
			if (len > LEVELSTART_POSTFIX_LEN && strnicmp(saveGameName + len - LEVELSTART_POSTFIX_LEN, LEVELSTART_POSTFIX, LEVELSTART_POSTFIX_LEN) == 0)
			{
				CryFixedStringT<256> path (pLevelInfo->GetPath());
				path+='/';
				//path+=("LevelStart.bmp"); //because of the french law we can't do this ...
				path+="Crysis.bmp";
				if (pSaveGame->SetThumbnailFromBMP(path) == true)
					bUseScreenShot = false;
			}
		}
	}

	if (bUseScreenShot)
	{
		int w = gEnv->pRenderer->GetWidth();
		int h = gEnv->pRenderer->GetHeight();

		const int imageDepth = THUMBNAIL_DEFAULT_DEPTH;
		int imageHeight = std::min(THUMBNAIL_DEFAULT_HEIGHT, h);
		int imageWidth = imageHeight * 16 / 9;
		// this initializes the data as well (fills with zero currently)
		uint8* pData = pSaveGame->SetThumbnail(0,imageWidth,imageHeight,imageDepth);

		// no thumbnail supported
		if (pData == 0)
			return;

		// initialize to stretch thumbnail
		int captureDestWidth  = imageWidth;
		int captureDestHeight = imageHeight;
		int captureDestOffX   = 0;
		int captureDestOffY   = 0;

		const bool bKeepAspectRatio = THUMBNAIL_KEEP_ASPECT_RATIO;

		// should we keep the aspect ratio of the renderer?
		if (bKeepAspectRatio)
		{
			captureDestHeight = imageHeight;
			captureDestWidth  = captureDestHeight * w / h;

			// adjust for SCOPE formats, like 2.35:1 
			if (captureDestWidth > THUMBNAIL_DEFAULT_WIDTH)
			{
				captureDestHeight = captureDestHeight * THUMBNAIL_DEFAULT_WIDTH / captureDestWidth;
				captureDestWidth  = THUMBNAIL_DEFAULT_WIDTH;
			}

			captureDestOffX = (imageWidth  - captureDestWidth) >> 1;
			captureDestOffY = (imageHeight - captureDestHeight) >> 1;

			// CryLogAlways("CXMLRichSaveGame: TEST_THUMBNAIL_AUTOCAPTURE: capWidth=%d capHeight=%d (off=%d,%d) thmbw=%d thmbh=%d rw=%d rh=%d", 
			//	captureDestWidth, captureDestHeight, captureDestOffX, captureDestOffY, m_thumbnailWidth, m_thumbnailHeight, w,h);

			if (captureDestWidth > imageWidth || captureDestHeight > imageHeight)
			{
				assert (false);
				GameWarning("CFlashMenuObject::OnSaveGame: capWidth=%d capHeight=%d", captureDestWidth, captureDestHeight);
				captureDestHeight = imageHeight;
				captureDestWidth = imageWidth;
				captureDestOffX = captureDestOffY = 0;
			}
		}

		const bool bAlpha = imageDepth == 4;
		const int bpl = imageWidth * imageDepth;
		uint8* pBuf = pData + captureDestOffY * bpl + captureDestOffX * imageDepth;
		gEnv->pRenderer->ReadFrameBuffer(pBuf, imageWidth, w, h, eRB_BackBuffer, bAlpha, captureDestWidth, captureDestHeight); // no inverse needed
	}

	if (m_nRenderAgainFrameId > 0)
	{
		// if buffer swaps were disabled by menu (for taking screenshot without menu)
		// it's now safe to turn buffer swap back on
		gEnv->pRenderer->EnableSwapBuffers(true);
		m_nRenderAgainFrameId = 0;
	}

	const char* file = pSaveGame->GetFileName();
	if(file)
	{
		m_sLastSaveGame = file;
		int pos = m_sLastSaveGame.rfind('/');
		if(pos != string::npos)
			m_sLastSaveGame = m_sLastSaveGame.substr(pos+1, m_sLastSaveGame.size() - pos);
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::OnLoadGame(ILoadGame* pLoadGame)
{
	m_bClearScreen = false;

	//patch last save game when loading different level
	/*{
		string lastSaveGameLevel;
		const string& lastSaveGame = g_pGame->GetLastSaveGame(lastSaveGameLevel);

		//if the current savegame level differs from the to be loaded level -> set current savegame to loaded one
		if(lastSaveGame.size() != 0 && lastSaveGameLevel.size() != 0)
		{
			string currentLevel = " ";
			if(ILevel * curLevel = g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel())
				currentLevel = curLevel->GetLevelInfo()->GetName();

			if(strcmp(lastSaveGameLevel.c_str(), currentLevel.c_str()))
			{
				const char* file = pLoadGame->GetFileName();
				if(file && strlen(file) > 1)
					m_sLastSaveGame = file;
			}
		}
	}*/
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::OnLevelEnd(const char *pNextLevel)
{
	if (pNextLevel == 0 || pNextLevel[0] == '\0')
	{
		const char* levelName = g_pGame->GetIGameFramework()->GetLevelName();

		ShowMainMenu();
		OnPostUpdate(gEnv->pTimer->GetFrameTime());
		if(INetChannel* pCh = g_pGame->GetIGameFramework()->GetClientChannel())
			pCh->Disconnect(eDC_UserRequested,"User has finished the level.");
		g_pGame->GetIGameFramework()->EndGameContext();
		if (levelName && CryStringUtils::stristr(levelName, "fleet") != 0)
			m_pCurrentFlashMenuScreen->Invoke("showCredits", true);
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::OnActionEvent(const SActionEvent& event)
{
	switch(event.m_event)
	{
	case eAE_connectFailed:
		if (m_multiplayerMenu) m_multiplayerMenu->OnUIEvent(SUIEvent(eUIE_connectFailed,event.m_value,event.m_description));
		break;
	case eAE_disconnected:
		//TODO: Show start menu if game not started
		if (m_multiplayerMenu) m_multiplayerMenu->OnUIEvent(SUIEvent(eUIE_disconnect,event.m_value,event.m_description));
		break;
	case eAE_channelCreated:
		if (m_multiplayerMenu) m_multiplayerMenu->OnUIEvent(SUIEvent(eUIE_connect));
		break;
	case eAE_resetBegin:
		MP_ResetBegin();
		if(g_pGame->GetIGameFramework()->IsGameStarted())
			UnloadHUDMovies();
//		OnLoadingStart(g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel()->GetLevelInfo());
		m_bUpdate = true;
		break;
	case eAE_resetEnd:
		if(g_pGame->GetIGameFramework()->IsGameStarted())
		{
			ReloadHUDMovies();		
		}
		MP_ResetEnd();
//		OnLoadingComplete(g_pGame->GetIGameFramework()->GetILevelSystem()->GetCurrentLevel());
		break;
	case eAE_resetProgress:
		MP_ResetProgress(event.m_value);
//		OnLoadingProgress(NULL, event.m_value);
		break;
	case eAE_inGame:
		if (!ShouldIgnoreInGameEvent() && m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING] && m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->IsLoaded())
		{
			m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Unload();
			m_bLoadingDone = false;
			m_bUpdate = false;
		}
		break;
	case eAE_earlyPreUpdate:
		// we might have to re-pause the game here on early update so subsystem's don't get updated
		if(gEnv->bEditor == false && m_bLoadingDone && g_pGameCVars->hud_startPaused)
		{
			if(!gEnv->bMultiplayer && !g_pGame->GetIGameFramework()->IsGamePaused())
				g_pGame->GetIGameFramework()->PauseGame(true ,false);
		}
		break;
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);

	for(int i=0; i<MENUSCREEN_COUNT; i++)
	{
		if(m_apFlashMenuScreens[i])
		{
			m_apFlashMenuScreens[i]->GetMemoryStatistics(s);
		}
	}
}

//-----------------------------------------------------------------------------------------------------

struct ForceSetEntrySink : public ILoadConfigurationEntrySink
{
	ForceSetEntrySink(const char* who) : m_who(who) {}

	void OnLoadConfigurationEntry( const char *szKey, const char *szValue, const char *szGroup )
	{
		ICVar* pCVar = gEnv->pConsole->GetCVar(szKey);
		if (pCVar)
		{
			pCVar->ForceSet(szValue);
		}
		else
		{
			GameWarning("%s : Can only set existing CVars during loading (no commands!) (%s = %s)", m_who, szKey, szValue);
		}
	}

	const char* m_who;
};

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::LoadDifficultyConfig(int iDiff)
{
	static const char *diffFilenames[] = 
	{
		"config/diff_normal.cfg",
		"config/diff_easy.cfg",
		"config/diff_normal.cfg",
		"config/diff_hard.cfg",
		"config/diff_bauer.cfg"
	};
	static const size_t diffCount = sizeof(diffFilenames) / sizeof(diffFilenames[0]);
	iDiff = iDiff < 1 ? 0 : ( iDiff < diffCount ? iDiff : 0);
	const char* filename = diffFilenames[iDiff];
	ForceSetEntrySink cvarSink ("CFlashMenuObject::SetDifficulty");
	CryLog("Loading configuration file '%s' ... ", filename );
	gEnv->pSystem->LoadConfiguration(filename, &cvarSink);
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::SetDifficulty(int level)
{
	if(m_pPlayerProfileManager)
	{
		IPlayerProfile *pProfile = m_pPlayerProfileManager->GetCurrentProfile(m_pPlayerProfileManager->GetCurrentUser());
		if(pProfile)
		{
			int iDiff = level;

			if(level > 0 && level < 5)
			{
				pProfile->SetAttribute("Singleplayer.LastSelectedDifficulty", level);
			}
			else
			{
				TFlowInputData data;
				if(pProfile->GetAttribute("Singleplayer.LastSelectedDifficulty", data, false))
					data.GetValueWithConversion(iDiff);
				else
					iDiff = 2;	//default
			}

			g_pGameCVars->g_difficultyLevel = iDiff;
			LoadDifficultyConfig(iDiff);
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::DisplaySubtitles(float fDeltaTime)
{
	if (m_pVideoPlayer && m_pSubtitleScreen)
	{
		char subtitleLabel[512];
		subtitleLabel[0] = 0;
		m_pVideoPlayer->GetSubtitle(0, subtitleLabel, sizeof(subtitleLabel));
		subtitleLabel[sizeof(subtitleLabel)-1] = 0; // safe termination

		if (m_currentSubtitleLabel != subtitleLabel)
		{
			wstring locString;
			if (subtitleLabel[0] == '@')
			{
				ILocalizationManager* pLocMgr = gEnv->pSystem->GetLocalizationManager();
				pLocMgr->LocalizeLabel(subtitleLabel, locString);
			}
			SFlashVarValue args[1] = { locString.c_str() };
			m_pSubtitleScreen->GetFlashPlayer()->Invoke("setText", args, sizeof(args) / sizeof(args[0]));
			m_currentSubtitleLabel = subtitleLabel;
		}
		m_pSubtitleScreen->GetFlashPlayer()->Advance(fDeltaTime);
		m_pSubtitleScreen->GetFlashPlayer()->Render();
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::CloseWaitingScreen()
{
	if(!gEnv->bMultiplayer)
	{
    if(m_pMusicSystem)
		{
		  m_pMusicSystem->EndTheme(EThemeFade_FadeOut, 0, true);
			PlaySound(ESound_MenuAmbience,false);
		}
		g_pGame->GetIGameFramework()->PauseGame(false ,false);
	}

	if(m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->IsLoaded())
	{
		m_apFlashMenuScreens[MENUSCREEN_FRONTENDLOADING]->Unload();
	}

	m_bLoadingDone = false;
	m_bUpdate = false;
	m_nBlackGraceFrames = gEnv->pRenderer->GetFrameID(false) + BLACK_FRAMES;
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::UpdateLaptop(float fDeltaTime)
{
	if(m_pAnimLaptopScreen)
	{
		float fTime = gEnv->pTimer->GetAsyncTime().GetSeconds();
		if(fTime >= m_fLaptopUpdateTime+1.0f || m_bForceLaptopUpdate)
		{
			SAFE_LAPTOPUTIL_FUNC(Update());
			m_fLaptopUpdateTime = fTime;
		}
		unsigned long ulBatteryLifeTime = SAFE_LAPTOPUTIL_FUNC_RET(GetBattteryLifeTime());
		int iBatteryLifePercent = SAFE_LAPTOPUTIL_FUNC_RET(GetBatteryLife());
		int iWLanSignalStrength = -1;
		if(SAFE_LAPTOPUTIL_FUNC_RET(IsWLan()))
		{
			iWLanSignalStrength = SAFE_LAPTOPUTIL_FUNC_RET(GetWLanSignalStrength());
		}
		// Update only when necessary
		if(	m_ulBatteryLifeTime		!= ulBatteryLifeTime		||
				m_iBatteryLifePercent != iBatteryLifePercent	||
				m_iWLanSignalStrength != iWLanSignalStrength	||
				m_bForceLaptopUpdate)
		{
			char szBatteryLifeTime[256];
			char szBatteryLifePercent[256];
			char szWLanSignalStrength[256];
			if(-1 == m_ulBatteryLifeTime)
			{
				// Remaining seconds is unknown
				sprintf(szBatteryLifeTime,"N/A");
			}
			else
			{
				uint uiBatteryLifeTimeHours	= ulBatteryLifeTime / 3600;
				uint uiBatteryLifeTimeMinutes = (ulBatteryLifeTime % 3600) / 60;
				uint uiBatteryLifeTimeSeconds = (ulBatteryLifeTime % 3600) % 60;
				sprintf(szBatteryLifeTime,"%2d:%2d:%2d",uiBatteryLifeTimeHours,uiBatteryLifeTimeMinutes,uiBatteryLifeTimeSeconds);
			}
			sprintf(szBatteryLifePercent,"%d",iBatteryLifePercent);
			sprintf(szWLanSignalStrength,"%d",iWLanSignalStrength);
			SFlashVarValue args[3] = {szBatteryLifeTime,szBatteryLifePercent,szWLanSignalStrength};
			m_pAnimLaptopScreen->CheckedInvoke("setNotebookStatus",args,3);
			m_ulBatteryLifeTime		= ulBatteryLifeTime;
			m_iBatteryLifePercent = iBatteryLifePercent;
			m_iWLanSignalStrength = iWLanSignalStrength;
			m_bForceLaptopUpdate = false;
		}

		m_pAnimLaptopScreen->GetFlashPlayer()->Advance(fDeltaTime);
		m_pAnimLaptopScreen->GetFlashPlayer()->Render();
	}
}

//-----------------------------------------------------------------------------------------------------

void CFlashMenuObject::UpdateNetwork(float fDeltaTime)
{
	if(!gEnv || !gEnv->pNetwork || !m_pCurrentFlashMenuScreen || !m_multiplayerMenu)
		return;

	if (!m_multiplayerMenu->IsInLobby() && !m_multiplayerMenu->IsInLogin() && !gEnv->bMultiplayer)
		m_pCurrentFlashMenuScreen->CheckedInvoke("setNetwork",true);
	else if(IsOnScreen(MENUSCREEN_FRONTENDSTART) || IsOnScreen(MENUSCREEN_FRONTENDINGAME))
	{
		bool bNetwork = gEnv->pNetwork->HasNetworkConnectivity();
		m_pCurrentFlashMenuScreen->CheckedInvoke("setNetwork",bNetwork);
	}
}

//-----------------------------------------------------------------------------------------------------
