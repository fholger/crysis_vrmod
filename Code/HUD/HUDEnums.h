// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------

enum ESound
{
	ESound_Hud_First,
	ESound_GenericBeep,
	ESound_PresetNavigationBeep,
	ESound_TemperatureBeep,
	ESound_SuitMenuAppear,
	ESound_SuitMenuDisappear,
	ESound_WeaponModification,
	ESound_BinocularsZoomIn,
	ESound_BinocularsZoomOut,
	ESound_BinocularsSelect,
	ESound_BinocularsDeselect,
	ESound_BinocularsAmbience,
	ESound_NightVisionSelect,
	ESound_NightVisionDeselect,
	ESound_NightVisionAmbience,
	ESound_BinocularsLock,
	ESound_SniperZoomIn,
	ESound_SniperZoomOut,
	ESound_OpenPopup,
	ESound_ClosePopup,
	ESound_MapOpen,
	ESound_MapClose,
	ESound_TabChanged,
	ESound_WaterDroplets,
	ESound_BuyBeep,
	ESound_BuyError,
	ESound_Highlight,
	ESound_Select,
	ESound_ObjectiveUpdate,
	ESound_ObjectiveComplete,
	ESound_GameSaved,
	ESound_Target_Lock,
	ESound_Malfunction,
	ESound_Reboot,
	ESound_ReActivate,
	ESound_VehicleIn,
	ESound_LawLocking,
	ESound_DownloadStart,
	ESound_DownloadLoop,
	ESound_DownloadStop,
	ESound_SpecialHitFeedback,
	ESound_Hud_Last
};

enum EHUDBATTLELOG_EVENTTYPE
{
	eBLE_Information=2,
	eBLE_Currency=1,
	eBLE_Warning=3,
	eBLE_System
};

enum EWarningMessages
{
	EHUD_SPECTATOR,
	EHUD_SWITCHTOTAN,
	EHUD_SWITCHTOBLACK,
	EHUD_SUICIDE,
	EHUD_CONNECTION_LOST,
	EHUD_OK,
	EHUD_YESNO,
	EHUD_CANCEL
};

//HUDQuickMenu / PDa suit modes
enum EQuickMenuButtons
{
	EQM_ARMOR = 0,
	EQM_WEAPON,
	EQM_CLOAK,
	EQM_STRENGTH,
	EQM_SPEED
};

//locking types
enum ELockingType
{
	eLT_Locking			= 0,
	eLT_Locked			= 1,
	eLT_Static			= 2
};

enum EHUDGAMERULES
{
	EHUD_SINGLEPLAYER,
	EHUD_INSTANTACTION,
	EHUD_POWERSTRUGGLE,
	EHUD_TEAMACTION,
};

//radar objects on mini map and radar
enum FlashRadarType	//don't change order (unless you change the flash asset)
{
	EFirstType = 0,
	ETank,
	EAPC,
	ECivilCar,
	ETruck,
	EHovercraft,
	ESpeedBoat,
	EPatrolBoat,
	ESmallBoat,
	ELTV,
	EHeli,
	EVTOL,
	EAAA,	// should be AAA
	ESoundEffect,		// is empty, but we have neither Tank nor APC as an icon
	ENuclearWeapon,
	ETechCharger,
	EWayPoint,
	EPlayer,
	ETaggedEntity,
	ESpawnPoint,
	EFactoryAir,
	EFactoryTank,
	EFactoryPrototype,
	EFactoryVehicle,
	EFactorySea,
	EINVALID1, //EBase,
	EBarracks,
	ESpawnTruck,
	EAmmoTruck,
	EHeadquarter2,
	EHeadquarter,
	//single player only icons
	EAmmoDepot,
	EMineField,
	EMachineGun,
	ECanalization,
	ETutorial,
	ESecretEntrance,
	EHitZone,
	EHitZoneNK,
	EAlienEnergySource,
	EAlienEnergySourcePlus,
	EPlaceholderForJuliensAutoTurrets,
	ESecondaryObjective,
	ELastType
};

//factions on mini map
enum FlashRadarFaction
{
	ENeutral,
	EFriend,
	EEnemy,
	EAggressor,
	ESelf
};

//types of objective icons shown on screen
enum FlashOnScreenIcon
{
	eOS_MissionObjective = 0,
	eOS_TACTank,
	eOS_TACWeapon,
	eOS_FactoryAir,
	eOS_FactoryTank,
	eOS_FactoryPrototypes,
	eOS_FactoryVehicle,
	eOS_FactoryNaval,
	eOS_HQKorean,
	eOS_HQUS,
	eOS_Spawnpoint,
	eOS_TeamMate,
	eOS_Purchase,
	eOS_Left,
	eOS_TopLeft,
	eOS_Top,
	eOS_TopRight,
	eOS_Right,
	eOS_BottomRight,
	eOS_Bottom,
	eOS_BottomLeft,
	eOS_AlienEnergyPoint,
	eOS_AlienEnergyPointPlus,
	eOS_HQTarget,
	eOS_SPObjective,
	eOS_Claymore,
	eOS_Mine
};