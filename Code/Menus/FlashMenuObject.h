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

*************************************************************************/
#ifndef __FLASHMENUOBJECT_H__
#define __FLASHMENUOBJECT_H__

//-----------------------------------------------------------------------------------------------------

#include "IGameFramework.h"
#include "IInput.h"
#include "IFlashPlayer.h"
#include "ILevelSystem.h"
#include "IHardwareMouse.h"

//-----------------------------------------------------------------------------------------------------

class CFlashMenuScreen;
class CGameFlashAnimation;
class CMPHub;
struct IAVI_Reader;
struct IMusicSystem;
struct IVideoPlayer;

//-----------------------------------------------------------------------------------------------------

class CFlashMenuObject : public IGameFrameworkListener, IHardwareMouseEventListener, IInputEventListener, IFSCommandHandler, ILevelSystemListener, IFlashLoadMovieHandler
{
public:

	enum EMENUSCREEN
	{
		MENUSCREEN_FRONTENDSTART,
		MENUSCREEN_FRONTENDINGAME,
		MENUSCREEN_FRONTENDLOADING,
		MENUSCREEN_FRONTENDRESET,
		MENUSCREEN_FRONTENDTEST,
		MENUSCREEN_FRONTENDSPLASH,
		MENUSCREEN_COUNT
	};

	enum ESaveCompare
	{
		eSAVE_COMPARE_FIRST = 0,
		eSAVE_COMPARE_NAME,
		eSAVE_COMPARE_DATE,
		eSAVE_COMPARE_LEVEL,
		eSAVE_COMPARE_TIME_PLAYED,
		eSAVE_COMPARE_TIME_LEVEL_PLAYED,
		eSAVE_COMPARE_LAST
	};

	struct FSAAMode
	{
		int samples, quality;
		FSAAMode() : samples(0), quality(0)
		{}
		FSAAMode(int smpl, int qual) : samples(smpl), quality(qual)
		{}
	};

	static const int VIDEOPLAYER_LOCALIZED_AUDIOCHANNEL = -1;

	void GetMemoryStatistics( ICrySizer * );

	// IGameFrameworkListener
	virtual void OnPostUpdate(float fDeltaTime);
	virtual void OnSaveGame(ISaveGame* pSaveGame);
	virtual void OnLoadGame(ILoadGame* pLoadGame);
	virtual void OnLevelEnd(const char* pNextLevel);
  virtual void OnActionEvent(const SActionEvent& event);
	// ~IGameFrameworkListener

	// IInputEventListener
	virtual bool OnInputEvent(const SInputEvent &rInputEvent);
	virtual bool OnInputEventUI(const SInputEvent &rInputEvent);
	// ~IInputEventListener

	// IFSCommandHandler
	void HandleFSCommand(const char *strCommand,const char *strArgs);
	// ~IFSCommandHandler

	// IFlashLoadMovieImage
	IFlashLoadMovieImage* LoadMovie(const char* pFilePath);
	// ~IFlashLoadMovieImage

	// ILevelSystemListener
	virtual void OnLevelNotFound(const char *levelName) {}
	virtual void OnLoadingStart(ILevelInfo *pLevel);
	virtual void OnLoadingComplete(ILevel *pLevel);
	virtual void OnLoadingError(ILevelInfo *pLevel, const char *error);
	virtual void OnLoadingProgress(ILevelInfo *pLevel, int progressAmount);
		//~ILevelSystemListener

	static SFlashKeyEvent MapToFlashKeyEvent(const SInputEvent &inputEvent);

	// IHardwareMouseEventListener
	virtual void OnHardwareMouseEvent(int iX,int iY,EHARDWAREMOUSEEVENT eHardwareMouseEvent);
	// ~IHardwareMouseEventListener

	//Profiles screen
	virtual void SetProfile();
	virtual void UpdateMenuColor();
	virtual bool ColorChanged();
	virtual void SetColorChanged();
	static CFlashMenuObject *GetFlashMenuObject();

	ILINE bool IsActive() { return m_bUpdate; }

	void ShowMainMenu();
	void ShowInGameMenu(bool bShow=true);
  void HideInGameMenuNextFrame(bool bRestoreGameMusic);
  void UnloadHUDMovies();
  void ReloadHUDMovies();
  bool PlayFlashAnim(const char* pFlashFile);
	// if audioCh is VIDEOPLAYER_LOCALIZED_AUDIOCHANNEL, we'll use the audio channel for the current language according to g_languageMapping
	bool PlayVideo(const char* pVideoFile, bool origUpscaleMode = true, unsigned int videoOptions = 0, int audioCh = 0, int voiceCh = -1, bool useSubtitles = false, bool exclusiveVideo = false);
	void StopVideo();
	void PlayTutorialVideo();
	bool StopTutorialVideo();
	void NextIntroVideo();
	void UpdateRatio();
	void StartSplashScreenCountDown();
	void StartResolutionCountDown();
	void HardwareEvaluation();
	void ShowMenuMessage(const char *message);
  bool IsOnScreen(EMENUSCREEN screen);
	bool IsControllerConnected() const { return m_bControllerConnected; }
	void InitStartMenu();
	void InitIngameMenu();
	void DestroyMenusAtNextFrame();

	ILINE bool WaitingForStart() const { return m_bLoadingDone; }

	string* GetLastInGameSave() { return &m_sLastSaveGame; }

  CMPHub* GetMPHub()const;

	CFlashMenuScreen *GetMenuScreen(EMENUSCREEN screen) const;

	bool Load();

	void LoadDifficultyConfig(int iDiff);
	void SetDifficulty(int level = 0);

	void DestroyStartMenu();
	void DestroyIngameMenu();
	FSAAMode GetFSAAMode(const char* name);
	string GetFSAAMode(int samples, int quality);

	enum EEntryMovieState
	{
		eEMS_Start,
		eEMS_SPDEMO,
		eEMS_EA,
		eEMS_Crytek,
		eEMS_NVidia,
		eEMS_Intel,
		eEMS_PEGI,
		eEMS_RatingLogo,
		eEMS_Rating,
		eEMS_Legal,
		eEMS_Stop,
		eEMS_Done,
		eEMS_GameStart,
		eEMS_GameIntro,
		eEMS_GameStop,
		eEMS_GameDone
	};

						CFlashMenuObject();
	virtual ~	CFlashMenuObject();

private:

	struct SaveGameMetaData
	{
		const char *name;
		const char *description;
		const char *humanName;
		const char *levelName;
		const char *gameRules;
		int fileVersion;
		const char * buildVersion;
		float levelPlayTimeSec;
		float gamePlayTimeSec;
		time_t saveTime;
		int kills, difficulty;
	};

	struct SaveGameDataCompare
	{
		CFlashMenuObject::ESaveCompare m_mode;

		SaveGameDataCompare(CFlashMenuObject::ESaveCompare mode) : m_mode(mode) {}

		bool operator()( const SaveGameMetaData& a, const SaveGameMetaData& b ) const
		{
			if(m_mode == eSAVE_COMPARE_NAME)
			{
				return strcmp(a.humanName, b.humanName) < 0;
			}
			else if(m_mode == eSAVE_COMPARE_DATE)
			{
				return a.saveTime > b.saveTime;
			}
			else if(m_mode == eSAVE_COMPARE_LEVEL)
			{
				return strcmp(a.levelName, b.levelName) < 0;
			}
			else if(m_mode == eSAVE_COMPARE_TIME_PLAYED)
			{
				return a.gamePlayTimeSec > b.gamePlayTimeSec;
			}
			else if(m_mode == eSAVE_COMPARE_TIME_LEVEL_PLAYED)
			{
				return a.levelPlayTimeSec > b.levelPlayTimeSec;
			}
			return false;
		}
	};

	void UpdateLevels(const char *gamemode);

	//Profiles screen
	void UpdateProfiles();
	void AddProfile(const char *profileName);
	void SelectProfile(const char *profileName, bool silent = false, bool keepOldSettings = false);
	void DeleteProfile(const char *profileName);
	void SwitchProfiles(const char *oldProfile, const char *newProfile);
	void SelectActiveProfile();

	//Singleplayer screen
	void UpdateSingleplayerDifficulties();
	void StartSingleplayerGame(const char *strDifficulty);
	void UpdateSaveGames();
	void LoadGame(const char *FileName);
	bool SaveGame(const char *FileName);
	void DeleteSaveGame(const char *FileName);
	const char* ValidateName(const char *fileName);
	void UpdateMods();

	//Options screen
	void SaveActionToMap(const char* actionmap, const char* action, const char *key);
	void SetCVar(const char *command, const string& value);
	void UpdateCVar(const char *command);
	void UpdateKeyMenu();
	void RestoreDefaults();

	// subtitle display for video player
	void DisplaySubtitles(float fDeltaTime);

	void SetDisplayFormats();
	void SetAntiAliasingModes();

	void MP_ResetBegin		();
	void MP_ResetEnd			();
	void MP_ResetProgress	(int iProgress);

	void CloseWaitingScreen();
	void UpdateLaptop(float fDeltaTime);
	void UpdateNetwork(float fDeltaTime);

	enum ESound
	{
		ESound_RollOver,
		ESound_Click1,
		ESound_Click2,
		ESound_MenuHighlight,
		ESound_MainHighlight,
		ESound_MenuSelect,
		ESound_MenuSelectDialog,
		ESound_MenuOpen,
		ESound_MenuClose,
		ESound_MenuStart,
		ESound_MenuFirstOpen,
		ESound_MenuFirstClose,
		ESound_MenuWarningOpen,
		ESound_MenuWarningClose,
		Esound_MenuDifficulty,
		Esound_MenuCheckbox,
		ESound_MenuSlider,
		ESound_MenuDropDown,
		ESound_ScreenChange,
		ESound_MenuAmbience,
		ESound_Last
	};

	//this is a list of sound ids to be able to stop them
	tSoundID m_soundIDs[ESound_Last];

	void PlaySound(ESound eSound,bool bPlay=true);

	void LockPlayerInputs(bool bLock);

	// helper function to map the "default" profile name to a localized version
	const wchar_t* GetMappedProfileName(const char* profileName);

	static CFlashMenuObject *s_pFlashMenuObject;

	CFlashMenuScreen *m_apFlashMenuScreens[MENUSCREEN_COUNT];
	CFlashMenuScreen *m_pCurrentFlashMenuScreen;
	CFlashMenuScreen *m_pSubtitleScreen;
	CFlashMenuScreen *m_pAnimLaptopScreen;

	IPlayerProfileManager* m_pPlayerProfileManager;

	IMusicSystem *m_pMusicSystem;

	struct SLoadSave
	{
		string name;
		bool save;
	} m_sLoadSave;

	typedef std::map<string,Vec2> ButtonPosMap;
	ButtonPosMap m_currentButtons;
	std::vector<ButtonPosMap> m_buttonPositions;
	string m_sCurrentButton;

	void UpdateButtonSnap(const Vec2 mouse);
	void SnapToNextButton(const Vec2 dir);
	void HWMouse2Flash(Vec2 &vec);
	void Flash2HWMouse(Vec2 &vec);
	void GetButtonClientPos(ButtonPosMap::iterator button, Vec2 &pos);
	void HighlightButton(ButtonPosMap::iterator button);
	void PushButton(ButtonPosMap::iterator button, bool press, bool force);
	ButtonPosMap::iterator FindButton(const TKeyName &shortcut);

	bool ShouldIgnoreInGameEvent();

	int m_iMaxProgress;

	bool m_bExclusiveVideo;

	float m_resolutionTimer;
	float m_fMusicFirstTime;

	bool m_bNoMoveEnabled;
	bool m_bNoMouseEnabled;
	bool m_bLanQuery;
	bool m_bUpdate;
	bool m_bIgnoreEsc;
	bool m_bClearScreen;
	bool m_bCatchNextInput;
	bool m_bDestroyStartMenuPending;
	bool m_bDestroyInGameMenuPending;
	bool m_bIgnorePendingEvents;
	bool m_bVirtualKeyboardFocus;
	bool m_bVKMouseDown;
	string m_sActionToCatch;
	string m_sActionMapToCatch;
	bool m_bColorChanged;
	bool m_bControllerConnected;
	bool m_bInLoading;
	bool m_bLoadingDone;
	bool m_bTutorialVideo;
	int m_nBlackGraceFrames;
	int m_nLastFrameUpdateID;
	int m_iGamepadsConnected;
	int m_iWidth;
	int m_iHeight;
	int m_iOldWidth;
	int m_iOldHeight;
	int m_nRenderAgainFrameId;
	EEntryMovieState m_stateEntryMovies;
	IFlashPlayer* m_pFlashPlayer;
	IVideoPlayer* m_pVideoPlayer;
	IAVI_Reader* m_pAVIReader;
	ESaveCompare m_eSaveGameCompareMode;
	bool				 m_bSaveGameSortUp;
	
	string m_currentSubtitleLabel;
	//last known savegame
	string	m_sLastSaveGame;

	CMPHub*      m_multiplayerMenu;

	// key repeat
	float					m_repeatTimer;
	float					m_splashScreenTimer;
	SInputEvent		m_repeatEvent;

	unsigned long m_ulBatteryLifeTime;
	int m_iBatteryLifePercent;
	int m_iWLanSignalStrength;
	float m_fLaptopUpdateTime;
	bool m_bForceLaptopUpdate;
	bool m_bIsEndingGameContext;
	bool m_textfieldFocus;

	int m_tempWidth, m_tempHeight;
	wstring m_tempMappedProfileName;

	std::map<string, FSAAMode> m_fsaaModes;






};

//-----------------------------------------------------------------------------------------------------

#endif

//-----------------------------------------------------------------------------------------------------
