/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 11:8:2004   10:50 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "GameCVars.h"
#include "GameRules.h"
#include "ItemSharedParams.h"

#include <INetwork.h>
#include <IGameObject.h>
#include <IActorSystem.h>
#include <IItemSystem.h>
#include "WeaponSystem.h"
#include "ServerSynchedStorage.h"
#include "ItemString.h"
#include "HUD/HUD.h"
#include "Menus/QuickGame.h"
#include "Environment/BattleDust.h"
#include "NetInputChainDebug.h"

#include "Menus/FlashMenuObject.h"
#include "Menus/MPHub.h"
#include "INetworkService.h"

static void BroadcastChangeSafeMode( ICVar * )
{
	SGameObjectEvent event(eCGE_ResetMovementController, eGOEF_ToExtensions);
	IEntitySystem * pES = gEnv->pEntitySystem;
	IEntityItPtr pIt = pES->GetEntityIterator();
	while (!pIt->IsEnd())
	{
		if (IEntity * pEnt = pIt->Next())
			if (IActor * pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEnt->GetId()))
				pActor->HandleEvent( event );
	}
}

void CmdBulletTimeMode( IConsoleCmdArgs* cmdArgs)
{
	g_pGameCVars->goc_enable = 0;
	g_pGameCVars->goc_tpcrosshair = 0;

	g_pGameCVars->bt_ironsight = 1;
	g_pGameCVars->bt_speed = 0;
	g_pGameCVars->bt_energy_decay = 2.5;
	g_pGameCVars->bt_end_reload = 1;
	g_pGameCVars->bt_end_select = 1;
	g_pGameCVars->bt_end_melee = 0;
}

void CmdGOCMode( IConsoleCmdArgs* cmdArgs)
{
	g_pGameCVars->goc_enable = 1;
	g_pGameCVars->goc_tpcrosshair = 1;
	
	g_pGameCVars->bt_ironsight = 1;
	g_pGameCVars->bt_speed = 0;
	g_pGameCVars->bt_energy_decay = 0;
	g_pGameCVars->bt_end_reload = 1;
	g_pGameCVars->bt_end_select = 1;
	g_pGameCVars->bt_end_melee = 0;

	//
	CPlayer *pPlayer = static_cast<CPlayer *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if(pPlayer && !pPlayer->IsThirdPerson())
	{
		pPlayer->ToggleThirdPerson();
	}
}

// game related cvars must start with an g_
// game server related cvars must start with sv_
// game client related cvars must start with cl_
// no other types of cvars are allowed to be defined here!
void SCVars::InitCVars(IConsole *pConsole)
{
	//client cvars
	pConsole->Register("cl_hud",&cl_hud,1,0,"Show/Hide the HUD", CHUDCommon::HUD);
	pConsole->Register("cl_fov", &cl_fov, 60.0f, 0, "field of view.");
	pConsole->Register("cl_bob", &cl_bob, 1.0f, 0, "view/weapon bobbing multiplier");
	pConsole->Register("cl_headBob", &cl_headBob, 1.0f, 0, "head bobbing multiplier");
	pConsole->Register("cl_headBobLimit", &cl_headBobLimit, 0.06f, 0, "head bobbing distance limit");
	pConsole->Register("cl_tpvDist", &cl_tpvDist, 3.5f, 0, "camera distance in 3rd person view");
	pConsole->Register("cl_tpvYaw", &cl_tpvYaw, 0, 0, "camera angle offset in 3rd person view");
	pConsole->Register("cl_nearPlane", &cl_nearPlane, 0, 0, "overrides the near clipping plane if != 0, just for testing.");
	pConsole->Register("cl_sprintShake", &cl_sprintShake, 0.0f, 0, "sprint shake");
	pConsole->Register("cl_sensitivityZeroG", &cl_sensitivityZeroG, 70.0f, VF_DUMPTODISK, "Set mouse sensitivity in ZeroG!");
	pConsole->Register("cl_sensitivity", &cl_sensitivity, 45.0f, VF_DUMPTODISK, "Set mouse sensitivity!");
	pConsole->Register("cl_controllersensitivity", &cl_controllersensitivity, 45.0f, VF_DUMPTODISK, "Set controller sensitivity!");
	pConsole->Register("cl_invertMouse", &cl_invertMouse, 0, VF_DUMPTODISK, "mouse invert?");
	pConsole->Register("cl_invertController", &cl_invertController, 0, VF_DUMPTODISK, "Controller Look Up-Down invert");
	pConsole->Register("cl_crouchToggle", &cl_crouchToggle, 0, VF_DUMPTODISK, "To make the crouch key work as a toggle");
	pConsole->Register("cl_fpBody", &cl_fpBody, 2, 0, "first person body");
	//FIXME:just for testing
	pConsole->Register("cl_strengthscale", &cl_strengthscale, 1.0f, 0, "nanosuit strength scale");

	/*
	// GOC
	pConsole->Register("goc_enable", &goc_enable, 0, VF_CHEAT, "gears of crysis");
	pConsole->Register("goc_tpcrosshair", &goc_tpcrosshair, 0, VF_CHEAT, "keep crosshair in third person");
	pConsole->Register("goc_targetx", &goc_targetx, 0.5f, VF_CHEAT, "target position of camera");
	pConsole->Register("goc_targety", &goc_targety, -2.5f, VF_CHEAT, "target position of camera");
	pConsole->Register("goc_targetz", &goc_targetz, 0.2f, VF_CHEAT, "target position of camera");
	pConsole->AddCommand("GOCMode", CmdGOCMode, VF_CHEAT, "Enable GOC mode");
	
	// BulletTime
	pConsole->Register("bt_speed", &bt_speed, 0, VF_CHEAT, "bullet-time when in speed mode");
	pConsole->Register("bt_ironsight", &bt_ironsight, 0, VF_CHEAT, "bullet-time when in ironsight");
	pConsole->Register("bt_end_reload", &bt_end_reload, 0, VF_CHEAT, "end bullet-time when reloading");
	pConsole->Register("bt_end_select", &bt_end_select, 0, VF_CHEAT, "end bullet-time when selecting a new weapon");
	pConsole->Register("bt_end_melee", &bt_end_melee, 0, VF_CHEAT, "end bullet-time when melee");
	pConsole->Register("bt_time_scale", &bt_time_scale, 0.2f, VF_CHEAT, "bullet-time time scale to apply");
	pConsole->Register("bt_pitch", &bt_pitch, -0.4f, VF_CHEAT, "sound pitch shift for bullet-time");
	pConsole->Register("bt_energy_max", &bt_energy_max, 1.0f, VF_CHEAT, "maximum bullet-time energy");
	pConsole->Register("bt_energy_decay", &bt_energy_decay, 2.5f, VF_CHEAT, "bullet time energy decay rate");
	pConsole->Register("bt_energy_regen", &bt_energy_regen, 0.5f, VF_CHEAT, "bullet time energy regeneration rate");
	pConsole->AddCommand("bulletTimeMode", CmdBulletTimeMode, VF_CHEAT, "Enable bullet time mode");
	*/

	pConsole->Register("dt_enable", &dt_enable, 0, 0, "suit actions activated by double-tapping");
	pConsole->Register("dt_time", &dt_time, 0.25f, 0, "time in seconds between double taps");
	pConsole->Register("dt_meleeTime", &dt_meleeTime, 0.3f, 0, "time in seconds between double taps for melee");

	pConsole->Register("i_staticfiresounds", &i_staticfiresounds, 1, VF_DUMPTODISK, "Enable/Disable static fire sounds. Static sounds are not unloaded when idle.");
	pConsole->Register("i_soundeffects", &i_soundeffects,	1, VF_DUMPTODISK, "Enable/Disable playing item sound effects.");
	pConsole->Register("i_lighteffects", &i_lighteffects, 1, VF_DUMPTODISK, "Enable/Disable lights spawned during item effects.");
	pConsole->Register("i_particleeffects", &i_particleeffects,	1, VF_DUMPTODISK, "Enable/Disable particles spawned during item effects.");
	pConsole->Register("i_rejecteffects", &i_rejecteffects, 1, VF_DUMPTODISK, "Enable/Disable ammo reject effects during weapon firing.");
	pConsole->Register("i_offset_front", &i_offset_front, 0.0f, 0, "Item position front offset");
	pConsole->Register("i_offset_up", &i_offset_up, 0.0f, 0, "Item position up offset");
	pConsole->Register("i_offset_right", &i_offset_right, 0.0f, 0, "Item position right offset");
	pConsole->Register("i_unlimitedammo", &i_unlimitedammo, 0, VF_CHEAT, "unlimited ammo");
	pConsole->Register("i_iceeffects", &i_iceeffects, 0, VF_CHEAT, "Enable/Disable specific weapon effects for ice environments");

	pConsole->Register("i_lighteffectShadows", &i_lighteffectsShadows, 0, VF_DUMPTODISK, "Enable/Disable shadow casting on weapon lights. 1 - Player only, 2 - Other players/AI, 3 - All (require i_lighteffects enabled).");

	// marcok TODO: seem to be only used on script side ... 
	pConsole->RegisterFloat("cl_motionBlur", 2, 0, "motion blur type (0=off, 1=accumulation-based, 2=velocity-based)");
	pConsole->RegisterFloat("cl_sprintBlur", 0.6f, 0, "sprint blur");
	pConsole->RegisterFloat("cl_hitShake", 1.25f, 0, "hit shake");
	pConsole->RegisterFloat("cl_hitBlur", 0.25f, 0, "blur on hit");

	pConsole->RegisterInt("cl_righthand", 1, 0, "Select right-handed weapon!");
	pConsole->RegisterInt("cl_screeneffects", 1, 0, "Enable player screen effects (depth-of-field, motion blur, ...).");
	
	pConsole->Register("cl_debugSwimming", &cl_debugSwimming, 0, VF_CHEAT, "enable swimming debugging");
	pConsole->Register("cl_g15lcdEnable", &cl_g15lcdEnable, 1, VF_DUMPTODISK, "enable support for Logitech G15 LCD");
	pConsole->Register("cl_g15lcdTick", &cl_g15lcdTick, 250, VF_DUMPTODISK, "milliseconds between lcd updates");

	ca_GameControlledStrafingPtr = pConsole->GetCVar("ca_GameControlledStrafing");
	pConsole->Register("pl_curvingSlowdownSpeedScale", &pl_curvingSlowdownSpeedScale, 0.5f, VF_CHEAT, "Player only slowdown speedscale when curving/leaning extremely.");
	pConsole->Register("ac_enableProceduralLeaning", &ac_enableProceduralLeaning, 1.0f, VF_CHEAT, "Enable procedural leaning (disabled asset leaning and curving slowdown).");

	pConsole->Register("cl_shallowWaterSpeedMulPlayer", &cl_shallowWaterSpeedMulPlayer, 0.6f, VF_CHEAT, "shallow water speed multiplier (Players only)");
	pConsole->Register("cl_shallowWaterSpeedMulAI", &cl_shallowWaterSpeedMulAI, 0.8f, VF_CHEAT, "Shallow water speed multiplier (AI only)");
	pConsole->Register("cl_shallowWaterDepthLo", &cl_shallowWaterDepthLo, 0.3f, VF_CHEAT, "Shallow water depth low (below has zero slowdown)");
	pConsole->Register("cl_shallowWaterDepthHi", &cl_shallowWaterDepthHi, 1.0f, VF_CHEAT, "Shallow water depth high (above has full slowdown)");

	pConsole->RegisterInt("g_grabLog", 0, 0, "verbosity for grab logging (0-2)");

	pConsole->Register("pl_inputAccel", &pl_inputAccel, 30.0f, 0, "Movement input acceleration");

	pConsole->RegisterInt("cl_actorsafemode", 0, VF_CHEAT, "Enable/disable actor safe mode", BroadcastChangeSafeMode);
	pConsole->Register("h_useIK", &h_useIK, 1, 0, "Hunter uses always IK");
	pConsole->Register("h_drawSlippers", &h_drawSlippers, 0, 0, "Red ball when tentacle is lifted, green when on ground");
	pConsole->Register("g_tentacle_joint_limit", &g_tentacle_joint_limit, -1.0f, 0, "forces specific tentacle limits; used for tweaking");
	pConsole->Register("g_enableSpeedLean", &g_enableSpeedLean, 0, 0, "Enables player-controlled curve leaning in speed mode.");
	//
	pConsole->Register("int_zoomAmount", &int_zoomAmount, 0.75f, VF_CHEAT, "Maximum zoom, between 0.0 and 1.0. Default = .75");
	pConsole->Register("int_zoomInTime", &int_zoomInTime, 5.0f, VF_CHEAT, "Number of seconds it takes to zoom in. Default = 5.0");
	pConsole->Register("int_moveZoomTime", &int_moveZoomTime, 0.1f, VF_CHEAT, "Number of seconds it takes to zoom out when moving. Default = 0.2");
	pConsole->Register("int_zoomOutTime", &int_zoomOutTime, 0.1f, VF_CHEAT, "Number of seconds it takes to zoom out when you stop firing. Default = 0.5");

	pConsole->RegisterFloat("aa_maxDist", 10.0f, VF_CHEAT, "max lock distance");

	pConsole->Register("hr_rotateFactor", &hr_rotateFactor, -.1f, VF_CHEAT, "rotate factor");
	pConsole->Register("hr_rotateTime", &hr_rotateTime, .07f, VF_CHEAT, "rotate time");
	pConsole->Register("hr_dotAngle", &hr_dotAngle, .75f, VF_CHEAT, "max angle for FOV change");
	pConsole->Register("hr_fovAmt", &hr_fovAmt, .03f, VF_CHEAT, "goal FOV when hit");
	pConsole->Register("hr_fovTime", &hr_fovTime, .05f, VF_CHEAT, "fov time");

	// frozen shake vars (for tweaking only)
	pConsole->Register("cl_debugFreezeShake", &cl_debugFreezeShake, 0, VF_CHEAT|VF_DUMPTODISK, "Toggle freeze shake debug draw");  
	pConsole->Register("cl_frozenSteps", &cl_frozenSteps, 3, VF_CHEAT, "Number of steps for unfreeze shaking");  
	pConsole->Register("cl_frozenSensMin", &cl_frozenSensMin, 1.0f, VF_CHEAT, "Frozen sensitivity min"); // was 0.2
	pConsole->Register("cl_frozenSensMax", &cl_frozenSensMax, 1.0f, VF_CHEAT, "Frozen sensitivity max"); // was 0.4
	pConsole->Register("cl_frozenAngleMin", &cl_frozenAngleMin, 1.f, VF_CHEAT, "Frozen clamp angle min");
	pConsole->Register("cl_frozenAngleMax", &cl_frozenAngleMax, 10.f, VF_CHEAT, "Frozen clamp angle max");
	pConsole->Register("cl_frozenMouseMult", &cl_frozenMouseMult, 0.00015f, VF_CHEAT, "Frozen mouseshake multiplier");
  pConsole->Register("cl_frozenKeyMult", &cl_frozenKeyMult, 0.02f, VF_CHEAT, "Frozen movement keys multiplier");
	pConsole->Register("cl_frozenSoundDelta", &cl_frozenSoundDelta, 0.004f, VF_CHEAT, "Threshold for unfreeze shake to trigger a crack sound");
	
	pConsole->Register("g_frostDecay", &g_frostDecay, 0.25f, VF_CHEAT, "Frost decay speed when freezing actors");

	pConsole->Register("g_stanceTransitionSpeed", &g_stanceTransitionSpeed, 15.0f, VF_CHEAT, "Set speed of camera transition from stance to stance");
	pConsole->Register("g_stanceTransitionSpeedSecondary", &g_stanceTransitionSpeedSecondary, 6.0f, VF_CHEAT, "Set speed of camera transition from stance to stance");

	pConsole->Register("g_playerHealthValue", &g_playerHealthValue, 100, VF_CHEAT, "Maximum player health.");
	pConsole->Register("g_walkMultiplier", &g_walkMultiplier, 1, VF_SAVEGAME, "Modify movement speed");
	pConsole->Register("g_suitRecoilEnergyCost", &g_suitRecoilEnergyCost, 15.0f, VF_CHEAT, "Subtracted energy when weapon is fired in strength mode.");
	pConsole->Register("g_suitSpeedMult", &g_suitSpeedMult, 1.85f, 0, "Modify speed mode effect.");
	pConsole->Register("g_suitSpeedMultMultiplayer", &g_suitSpeedMultMultiplayer, 0.35f, 0, "Modify speed mode effect for Multiplayer.");
	pConsole->Register("g_suitArmorHealthValue", &g_suitArmorHealthValue, 200.0f, 0, "This defines how much damage is reduced by 100% energy, not considering recharge. The value should be between 1 and <SuitMaxEnergy>.");
	pConsole->Register("g_suitSpeedEnergyConsumption", &g_suitSpeedEnergyConsumption, 110.0f, 0, "Energy reduction in speed mode per second.");
	pConsole->Register("g_suitSpeedEnergyConsumptionMultiplayer", &g_suitSpeedEnergyConsumptionMultiplayer, 50.0f, 0, "Energy reduction in speed mode per second in multiplayer.");
	pConsole->Register("g_suitCloakEnergyDrainAdjuster", &g_suitCloakEnergyDrainAdjuster, 1.0f, 0, "Multiplier for energy reduction in cloak mode.");
	pConsole->Register("g_mpSpeedRechargeDelay", &g_mpSpeedRechargeDelay, 1, VF_CHEAT, "Toggles delay when sprinting below 20% energy.");
	pConsole->Register("g_AiSuitEnergyRechargeTime", &g_AiSuitEnergyRechargeTime, 10.0f, VF_CHEAT, "Modify suit energy recharge for AI.");
	pConsole->Register("g_AiSuitStrengthMeleeMult", &g_AiSuitStrengthMeleeMult, 0.4f, VF_CHEAT, "Modify AI strength mode melee damage relative to player damage.");
	pConsole->Register("g_AiSuitHealthRegenTime", &g_AiSuitHealthRegenTime, 33.3f, VF_CHEAT, "Modify suit health recharge for AI.");
	pConsole->Register("g_AiSuitArmorModeHealthRegenTime", &g_AiSuitArmorModeHealthRegenTime, 20.0f, VF_CHEAT, "Modify suit health recharge for AI in armor mode.");
	pConsole->Register("g_playerSuitEnergyRechargeTime", &g_playerSuitEnergyRechargeTime, 10.0f, VF_CHEAT, "Modify suit energy recharge for Player.");
	pConsole->Register("g_playerSuitEnergyRechargeTimeArmor", &g_playerSuitEnergyRechargeTimeArmor, 10.0f, VF_CHEAT, "Modify suit energy recharge for Player in singleplayer in armor mode.");
	pConsole->Register("g_playerSuitEnergyRechargeTimeArmorMoving", &g_playerSuitEnergyRechargeTimeArmorMoving, 10.0f, VF_CHEAT, "Modify suit energy recharge for Player in singleplayer in armor mode while moving.");
	pConsole->Register("g_playerSuitEnergyRechargeTimeMultiplayer", &g_playerSuitEnergyRechargeTimeMultiplayer, 20.0f, VF_CHEAT, "Modify suit energy recharge for Player in multiplayer.");
	pConsole->Register("g_playerSuitEnergyRechargeDelay", &g_playerSuitEnergyRechargeDelay, 0.0f, VF_CHEAT, "Delay of energy recharge after the player has been hit.");
	pConsole->Register("g_playerSuitHealthRegenTime", &g_playerSuitHealthRegenTime, 35.0f, VF_CHEAT, "Modify suit health recharge for Player.");
	pConsole->Register("g_playerSuitHealthRegenTimeMoving", &g_playerSuitHealthRegenTimeMoving, 35.0f, VF_CHEAT, "Modify suit health recharge for moving Player.");
	pConsole->Register("g_playerSuitArmorModeHealthRegenTime", &g_playerSuitArmorModeHealthRegenTime, 15.0f, VF_CHEAT, "Modify suit health recharge for Player in armor mode.");
	pConsole->Register("g_playerSuitArmorModeHealthRegenTimeMoving", &g_playerSuitArmorModeHealthRegenTimeMoving, 30.0f, VF_CHEAT, "Modify suit health recharge for Player moving in armor mode.");
	pConsole->Register("g_playerSuitHealthRegenDelay", &g_playerSuitHealthRegenDelay, 0.0f, VF_CHEAT, "Delay of health regeneration after the player has been hit.");
	pConsole->Register("g_difficultyLevel", &g_difficultyLevel, 0, VF_CHEAT|VF_READONLY, "Difficulty level");
	pConsole->Register("g_difficultyHintSystem", &g_difficultyHintSystem, 2, VF_CHEAT|VF_READONLY, "Lower difficulty hint system (0 is off, 1 is radius based, 2 is save-game based)");
	pConsole->Register("g_difficultyRadius", &g_difficultyRadius, 300, VF_CHEAT|VF_READONLY, "Radius in which player needs to die to display lower difficulty level hint.");
	pConsole->Register("g_difficultyRadiusThreshold", &g_difficultyRadiusThreshold, 5, VF_CHEAT|VF_READONLY, "Number of times player has to die within radius to trigger difficulty hint.");
	pConsole->Register("g_difficultySaveThreshold", &g_difficultySaveThreshold, 5, VF_CHEAT|VF_READONLY, "Number of times player has to die with same savegame active to trigger difficulty hint.");
	pConsole->Register("g_pp_scale_income", &g_pp_scale_income, 1, 0, "Scales incoming PP.");
	pConsole->Register("g_pp_scale_price", &g_pp_scale_price, 1, 0, "Scales PP prices.");
	pConsole->Register("g_energy_scale_price", &g_energy_scale_price, 0, 0, "Scales energy prices.");
	pConsole->Register("g_energy_scale_income", &g_energy_scale_income, 1, 0, "Scales incoming energy.");
	pConsole->Register("g_enableFriendlyFallAndPlay", &g_enableFriendlyFallAndPlay, 0, 0, "Enables fall&play feedback for friendly actors.");
	
	pConsole->Register("g_playerRespawns", &g_playerRespawns, 0, VF_SAVEGAME, "Sets the player lives.");
	pConsole->Register("g_playerLowHealthThreshold", &g_playerLowHealthThreshold, 40.0f, VF_CHEAT, "The player health threshold when the low health effect kicks in.");
	pConsole->Register("g_playerLowHealthThreshold2", &g_playerLowHealthThreshold2, 60.0f, VF_CHEAT, "The player health threshold when the low health effect reaches maximum.");
	pConsole->Register("g_playerLowHealthThresholdMultiplayer", &g_playerLowHealthThresholdMultiplayer, 20.0f, VF_CHEAT, "The player health threshold when the low health effect kicks in.");
	pConsole->Register("g_playerLowHealthThreshold2Multiplayer", &g_playerLowHealthThreshold2Multiplayer, 30.0f, VF_CHEAT, "The player health threshold when the low health effect reaches maximum.");
	pConsole->Register("g_punishFriendlyDeaths", &g_punishFriendlyDeaths, 1, VF_CHEAT, "The player gets punished by death when killing a friendly unit.");
	pConsole->Register("g_enableMPStealthOMeter", &g_enableMPStealthOMeter, 0, VF_CHEAT, "Enables the stealth-o-meter to detect enemies in MP matches.");
	pConsole->Register("g_meleeWhileSprinting", &g_meleeWhileSprinting, 0, 0, "Enables option to melee while sprinting, using left mouse button.");
	pConsole->Register("g_fallAndPlayThreshold", &g_fallAndPlayThreshold, 50, VF_CHEAT, "Minimum damage for fall and play.");

	// Depth of Field control
	pConsole->Register("g_dofset_minScale", &g_dofset_minScale, 1.0f, VF_CHEAT, "Scale Dof_FocusMin param when it gets set Default = 1");
	pConsole->Register("g_dofset_maxScale", &g_dofset_maxScale, 3.0f, VF_CHEAT, "Scale Dof_FocusMax param when it gets set Default = 3");
	pConsole->Register("g_dofset_limitScale", &g_dofset_limitScale, 9.0f, VF_CHEAT, "Scale Dof_FocusLimit param when it gets set Default = 9");
	pConsole->Register("g_dof_minHitScale", &g_dof_minHitScale, 0.25f, VF_CHEAT, "Scale of ray hit distance which Min tries to approach. Default = 0.25");
	pConsole->Register("g_dof_maxHitScale", &g_dof_maxHitScale, 2.0f, VF_CHEAT, "Scale of ray hit distance which Max tries to approach. Default = 2.0f");
	pConsole->Register("g_dof_sampleAngle", &g_dof_sampleAngle, 5.0f, VF_CHEAT, "Sample angle in degrees. Default = 5");
	pConsole->Register("g_dof_minAdjustSpeed", &g_dof_minAdjustSpeed, 100.0f, VF_CHEAT, "Speed that DoF can adjust the min value with. Default = 100");
	pConsole->Register("g_dof_maxAdjustSpeed", &g_dof_maxAdjustSpeed, 200.0f, VF_CHEAT, "Speed that DoF can adjust the max value with. Default = 200");
	pConsole->Register("g_dof_averageAdjustSpeed", &g_dof_averageAdjustSpeed, 20.0f, VF_CHEAT, "Speed that the average between min and max can be approached. Default = 20");
	pConsole->Register("g_dof_distAppart", &g_dof_distAppart, 10.0f, VF_CHEAT, "Minimum distance that max and min can be apart. Default = 10");
	pConsole->Register("g_dof_ironsight", &g_dof_ironsight, 1, VF_CHEAT, "Enable ironsight dof. Default = 1");

	// explosion culling
	pConsole->Register("g_ec_enable", &g_ec_enable, 1, VF_CHEAT, "Enable/Disable explosion culling of small objects. Default = 1");
	pConsole->Register("g_ec_radiusScale", &g_ec_radiusScale, 2.0f, VF_CHEAT, "Explosion culling scale to apply to explosion radius for object query.");
	pConsole->Register("g_ec_volume", &g_ec_volume, 0.75f, VF_CHEAT, "Explosion culling volume which needs to be exceed for objects to not be culled.");
	pConsole->Register("g_ec_extent", &g_ec_extent, 2.0f, VF_CHEAT, "Explosion culling length of an AABB side which needs to be exceed for objects to not be culled.");
	pConsole->Register("g_ec_removeThreshold", &g_ec_removeThreshold, 20, VF_CHEAT, "At how many items in exploding area will it start removing items.");

	pConsole->Register("g_radialBlur", &g_radialBlur, 1.0f, VF_CHEAT, "Radial blur on explosions. Default = 1, 0 to disable");
	pConsole->Register("g_playerFallAndPlay", &g_playerFallAndPlay, 0, 0, "When enabled, the player doesn't die from direct damage, but goes to fall and play.");
	
	pConsole->Register("g_enableTracers", &g_enableTracers, 1, 0, "Enable/Disable tracers.");
	pConsole->Register("g_enableAlternateIronSight", &g_enableAlternateIronSight, 0, 0, "Enable/Disable alternate ironsight mode");
	pConsole->Register("g_ragdollMinTime", &g_ragdollMinTime, 10.0f, 0, "minimum time in seconds that a ragdoll will be visible");
	pConsole->Register("g_ragdollUnseenTime", &g_ragdollUnseenTime, 2.0f, 0, "time in seconds that the player has to look away from the ragdoll before it disappears");
	pConsole->Register("g_ragdollPollTime", &g_ragdollPollTime, 0.5f, 0, "time in seconds where 'unseen' polling is done");
	pConsole->Register("g_ragdollDistance", &g_ragdollDistance, 10.0f, 0, "distance in meters that the player has to be away from the ragdoll before it can disappear");
	pConsole->Register("g_debugaimlook", &g_debugaimlook, 0, VF_CHEAT, "Debug aim/look direction");
	pConsole->Register("g_enableIdleCheck", &g_enableIdleCheck, 1, 0);

	// Crysis supported gamemode CVars
	pConsole->Register("g_timelimit", &g_timelimit, 60.0f, 0, "Duration of a time-limited game (in minutes). Default is 0, 0 means no time-limit.");
	pConsole->Register("g_roundtime", &g_roundtime, 2.0f, 0, "Duration of a round (in minutes). Default is 0, 0 means no time-limit.");
	pConsole->Register("g_preroundtime", &g_preroundtime, 8, 0, "Frozen time before round starts. Default is 8, 0 Disables freeze time.");
	pConsole->Register("g_suddendeathtime", &g_suddendeathtime, 30, 0, "Number of seconds before round end to start sudden death. Default if 30. 0 Disables sudden death.");
	pConsole->Register("g_roundlimit", &g_roundlimit, 30, 0, "Maximum numbers of rounds to be played. Default is 0, 0 means no limit.");
	pConsole->Register("g_fraglimit", &g_fraglimit, 0, 0, "Number of frags before a round restarts. Default is 0, 0 means no frag-limit.");
  pConsole->Register("g_fraglead", &g_fraglead, 1, 0, "Number of frags a player has to be ahead of other players once g_fraglimit is reached. Default is 1.");
  pConsole->Register("g_friendlyfireratio", &g_friendlyfireratio, 1.0f, 0, "Sets friendly damage ratio.");
  pConsole->Register("g_revivetime", &g_revivetime, 20, 0, "Revive wave timer.");
  pConsole->Register("g_autoteambalance", &g_autoteambalance, 1, 0, "Enables auto team balance.");
	pConsole->Register("g_autoteambalance_threshold", &g_autoteambalance_threshold, 3, 0, "Sets the auto team balance player threshold.");

	pConsole->Register("g_minplayerlimit", &g_minplayerlimit, 2, 0, "Minimum number of players to start a match.");
	pConsole->Register("g_minteamlimit", &g_minteamlimit, 1, 0, "Minimum number of players in each team to start a match.");
	pConsole->Register("g_tk_punish", &g_tk_punish, 1, 0, "Turns on punishment for team kills");
	pConsole->Register("g_tk_punish_limit", &g_tk_punish_limit, 5, 0, "Number of team kills user will be banned for");
	pConsole->Register("g_teamlock", &g_teamlock, 2, 0, "Number of players one team needs to have over the other, for the game to deny joining it. 0 disables.");
	pConsole->Register("g_useHitSoundFeedback", &g_useHitSoundFeedback, 1, 0, "Switches hit readability feedback sounds on/off.");

	pConsole->Register("g_debugNetPlayerInput", &g_debugNetPlayerInput, 0, VF_CHEAT, "Show some debug for player input");
	pConsole->Register("g_debug_fscommand", &g_debug_fscommand, 0, 0, "Print incoming fscommands to console");
	pConsole->Register("g_debugDirectMPMenu", &g_debugDirectMPMenu, 0, 0, "Jump directly to MP menu on application start.");
	pConsole->Register("g_skipIntro", &g_skipIntro, 0, VF_CHEAT, "Skip all the intro videos.");
	pConsole->Register("g_resetActionmapOnStart", &g_resetActionmapOnStart, 0, 0, "Resets Keyboard mapping on application start.");
	pConsole->Register("g_useProfile", &g_useProfile, 1, 0, "Don't save anything to or load anything from profile.");
	pConsole->Register("g_startFirstTime", &g_startFirstTime, 1, VF_DUMPTODISK, "1 before the game was started first time ever.");
	pConsole->Register("g_cutsceneSkipDelay", &g_cutsceneSkipDelay, 0.0f, 0, "Skip Delay for Cutscenes.");
	pConsole->Register("g_enableAutoSave", &g_enableAutoSave, 1, 0, "Switches all savegames created by Flowgraph (checkpoints). Does not affect user generated saves or levelstart savegames.");

	//
  pConsole->Register("g_godMode", &g_godMode, 0, VF_CHEAT, "God Mode");
  pConsole->Register("g_detachCamera", &g_detachCamera, 0, VF_CHEAT, "Detach camera");

  pConsole->Register("g_debugCollisionDamage", &g_debugCollisionDamage, 0, VF_DUMPTODISK, "Log collision damage");
	pConsole->Register("g_debugHits", &g_debugHits, 0, VF_DUMPTODISK, "Log hits");
  pConsole->Register("g_trooperProneMinDistance", &g_trooperProneMinDistance, 10, VF_DUMPTODISK, "Distance to move for trooper to switch to prone stance");
//	pConsole->Register("g_trooperMaxPhysicsAnimBlend", &g_trooperMaxPhysicAnimBlend, 0, VF_DUMPTODISK, "Max value for trooper tentacle dynamic physics/anim blending");
//	pConsole->Register("g_trooperPhysicsAnimBlendSpeed", &g_trooperPhysicAnimBlendSpeed, 100.f, VF_DUMPTODISK, "Trooper tentacle dynamic physics/anim blending speed");
	pConsole->Register("g_trooperTentacleAnimBlend", &g_trooperTentacleAnimBlend, 0, VF_DUMPTODISK, "Trooper tentacle physic_anim blend (0..1) - overrides the physic_blend AG parameter when it's not 0");
	pConsole->Register("g_trooperBankingMultiplier", &g_trooperBankingMultiplier, 1, VF_DUMPTODISK, "Trooper banking multiplier coeff (0..x)");
	pConsole->Register("g_alienPhysicsAnimRatio", &g_alienPhysicsAnimRatio, 0.0f, VF_CHEAT ); 

	pConsole->Register("g_spRecordGameplay", &g_spRecordGameplay, 0, 0, "Write sp gameplay information to harddrive.");
	pConsole->Register("g_spGameplayRecorderUpdateRate", &g_spGameplayRecorderUpdateRate, 1.0f, 0, "Update-delta of gameplay recorder in seconds.");
  
	pConsole->Register("pl_debug_ladders", &pl_debug_ladders, 0, VF_CHEAT);
	pConsole->Register("pl_debug_movement", &pl_debug_movement, 0, VF_CHEAT);
	pConsole->Register("pl_debug_jumping", &pl_debug_jumping, 0, VF_CHEAT);
	pl_debug_filter = pConsole->RegisterString("pl_debug_filter","",VF_CHEAT);

	pConsole->Register("aln_debug_movement", &aln_debug_movement, 0, VF_CHEAT);
	aln_debug_filter = pConsole->RegisterString("aln_debug_filter","",VF_CHEAT);

	// emp grenade
	pConsole->Register("g_emp_style", &g_empStyle, 0, VF_CHEAT, "");
	pConsole->Register("g_emp_nanosuit_downtime", &g_empNanosuitDowntime, 10.0f, VF_CHEAT, "Time that the nanosuit is deactivated after leaving the EMP field.");

	// hud cvars
	pConsole->Register("hud_mpNamesDuration", &hud_mpNamesDuration, 2, 0, "MP names will fade after this duration.");
	pConsole->Register("hud_mpNamesNearDistance", &hud_mpNamesNearDistance, 1, 0, "MP names will be fully visible when nearer than this.");
	pConsole->Register("hud_mpNamesFarDistance", &hud_mpNamesFarDistance, 100, 0, "MP names will be fully invisible when farther than this.");
	pConsole->Register("hud_onScreenNearDistance", &hud_onScreenNearDistance, 10, 0, "On screen icons won't scale anymore, when nearer than this.");
	pConsole->Register("hud_onScreenFarDistance", &hud_onScreenFarDistance, 500, 0, "On screen icons won't scale anymore, when farther than this.");
	pConsole->Register("hud_onScreenNearSize", &hud_onScreenNearSize, 1.4f, 0, "On screen icon size when nearest.");
	pConsole->Register("hud_onScreenFarSize", &hud_onScreenFarSize, 0.7f, 0, "On screen icon size when farthest.");
	pConsole->Register("hud_colorLine", &hud_colorLine, 4481854, 0, "HUD line color.");
	pConsole->Register("hud_colorOver", &hud_colorOver, 14125840, 0, "HUD hovered color.");
	pConsole->Register("hud_colorText", &hud_colorText, 12386209, 0, "HUD text color.");
	pConsole->Register("hud_voicemode", &hud_voicemode, 1, 0, "Usage of the voice when switching of Nanosuit mode.");
	pConsole->Register("hud_enableAlienInterference", &hud_enableAlienInterference, 1, VF_SAVEGAME, "Switched the alien interference effect.");
	pConsole->Register("hud_alienInterferenceStrength", &hud_alienInterferenceStrength, 0.8f, VF_SAVEGAME, "Scales alien interference effect strength.");
	pConsole->Register("hud_godFadeTime", &hud_godFadeTime, 3, VF_CHEAT, "sets the fade time of the god mode message");
	pConsole->Register("hud_crosshair_enable", &hud_crosshair_enable, 1,0, "Toggles singleplayer crosshair visibility.", CHUD::OnCrosshairCVarChanged);
	pConsole->Register("hud_crosshair", &hud_crosshair, 1,0, "Select the crosshair (1-8)", CHUD::OnCrosshairCVarChanged);
	pConsole->Register("hud_alternateCrosshairSpread",&hud_iAlternateCrosshairSpread,0, 0, "Switch new crosshair spread code on/off.");
	pConsole->Register("hud_alternateCrosshairSpreadCrouch",&hud_fAlternateCrosshairSpreadCrouch,12.0f, VF_CHEAT);
	pConsole->Register("hud_alternateCrosshairSpreadNeutral",&hud_fAlternateCrosshairSpreadNeutral,6.0f, VF_CHEAT);
	pConsole->Register("hud_chDamageIndicator", &hud_chDamageIndicator, 1,0,"Switch crosshair-damage indicator... (1 on, 0 off)");
	pConsole->Register("hud_showAllObjectives", &hud_showAllObjectives, 0, 0, "Show all on screen objectives, not only the active one.");
	pConsole->Register("hud_panoramicHeight", &hud_panoramicHeight, 10,0,"Set screen border for 'cinematic view' in percent.", CHUD::OnSubtitlePanoramicHeightCVarChanged);
	pConsole->Register("hud_subtitles", &hud_subtitles, 0,0,"Subtitle mode. 0==Off, 1=All, 2=CutscenesOnly", CHUD::OnSubtitleCVarChanged);
	pConsole->Register("hud_subtitlesDebug", &hud_subtitlesDebug, 0,0,"Debug subtitles");
	pConsole->Register("hud_subtitlesRenderMode", &hud_subtitlesRenderMode, 0,0,"Subtitle RenderMode. 0==Flash, 1=3DEngine");
	pConsole->Register("hud_subtitlesFontSize", &hud_subtitlesFontSize, 16, 0, "FontSize for Subtitles.");
	pConsole->Register("hud_subtitlesHeight", &hud_subtitlesHeight, 10, 0,"Height of Subtitles in Percent. Normally same as hud_PanoramicHeight");
	pConsole->Register("hud_subtitlesShowCharName", &hud_subtitlesShowCharName, 1, 0,"Show Character talking along with Subtitle");
	pConsole->Register("hud_subtitlesQueueCount", &hud_subtitlesQueueCount, 1, 0,"Maximum amount of subtitles in Update Queue");
	pConsole->Register("hud_subtitlesVisibleCount", &hud_subtitlesVisibleCount, 1, 0,"Maximum amount of subtitles in Visible Queue");
	pConsole->Register("hud_attachBoughEquipment", &hud_attachBoughtEquipment, 0,VF_CHEAT,"Attach equipment in PS equipment packs to the last bought/selected weapon.");
	pConsole->Register("hud_radarBackground", &hud_radarBackground, 1, 0, "Switches the miniMap-background for the radar.");
	pConsole->Register("hud_radarJammingEffectScale", &hud_radarJammingEffectScale, 0.75f, 0, "Scales the intensity of the radar jamming effect.");
	pConsole->Register("hud_radarJammingThreshold", &hud_radarJammingThreshold, 0.99f, 0, "Threshold to disable the radar (independent from effect).");
	pConsole->Register("hud_startPaused", &hud_startPaused, 1, 0, "The game starts paused, waiting for user input.");
	pConsole->Register("hud_faderDebug", &hud_faderDebug, 0, 0, "Show Debug Information for FullScreen Faders.");
	pConsole->Register("hud_nightVisionConsumption", &hud_nightVisionConsumption, 0.5f, VF_CHEAT, "Scales the energy consumption of the night vision.");
	pConsole->Register("hud_nightVisionRecharge", &hud_nightVisionRecharge, 2.0f, VF_CHEAT, "Scales the energy recharge of the night vision.");
	pConsole->Register("hud_showBigVehicleReload", &hud_showBigVehicleReload, 0, 0, "Enables an additional reload bar around the crosshair in big vehicles.");
	pConsole->Register("hud_radarScanningDelay", &hud_binocsScanningDelay, 0.55f, VF_CHEAT, "Defines the delay in seconds the binoculars take to scan an object.");
	pConsole->Register("hud_binocsScanningWidth", &hud_binocsScanningWidth, 0.3f, VF_CHEAT, "Defines the width/height in which the binocular raycasts are offset from the center to scan objects.");

	// Controller aim helper cvars
	pConsole->Register("aim_assistSearchBox", &aim_assistSearchBox, 100.0f, 0, "The area autoaim looks for enemies within");
	pConsole->Register("aim_assistMaxDistance", &aim_assistMaxDistance, 150.0f, 0, "The maximum range at which autoaim operates");
	pConsole->Register("aim_assistSnapDistance", &aim_assistSnapDistance, 3.0f, 0, "The maximum deviation autoaim is willing to compensate for");
	pConsole->Register("aim_assistVerticalScale", &aim_assistVerticalScale, 0.75f, 0, "The amount of emphasis on vertical correction (the less the number is the more vertical component is compensated)");
	pConsole->Register("aim_assistSingleCoeff", &aim_assistSingleCoeff, 1.0f, 0, "The scale of single-shot weapons' aim assistance");
	pConsole->Register("aim_assistAutoCoeff", &aim_assistAutoCoeff, 0.5f, 0, "The scale of auto weapons' aim assistance at continuous fire");
	pConsole->Register("aim_assistRestrictionTimeout", &aim_assistRestrictionTimeout, 20.0f, 0, "The restriction timeout on aim assistance after user uses a mouse");


	
	// Controller control
	pConsole->Register("hud_aspectCorrection", &hud_aspectCorrection, 2, 0, "Aspect ratio corrections for controller rotation: 0-off, 1-direct, 2-inverse");
	pConsole->Register("hud_ctrl_Curve_X", &hud_ctrl_Curve_X, 3.0f, 0, "Analog controller X rotation curve");
	pConsole->Register("hud_ctrl_Curve_Z", &hud_ctrl_Curve_Z, 3.0f, 0, "Analog controller Z rotation curve");
	pConsole->Register("hud_ctrl_Coeff_X", &hud_ctrl_Coeff_X, 3.5f*3.5f, 0, "Analog controller X rotation scale"); // was 3.5*3.5 but aspect ratio correction does the scaling now! adjust only if that gives no satisfactory results
	pConsole->Register("hud_ctrl_Coeff_Z", &hud_ctrl_Coeff_Z, 5.0f*5.0f, 0, "Analog controller Z rotation scale");
	pConsole->Register("hud_ctrlZoomMode", &hud_ctrlZoomMode, 0, 0, "Weapon aiming mode with controller. 0 is same as mouse zoom, 1 cancels at release");

	pConsole->Register("g_combatFadeTime", &g_combatFadeTime, 17.0f, 0, "sets the battle fade time in seconds ");
	pConsole->Register("g_combatFadeTimeDelay", &g_combatFadeTimeDelay, 7.0f, 0, "waiting time before the battle starts fading out, in seconds ");
	pConsole->Register("g_battleRange", &g_battleRange, 50.0f, 0, "sets the battle range in meters ");

	// Assistance switches
	pConsole->Register("aim_assistAimEnabled", &aim_assistAimEnabled, 1, 0, "Enable/disable aim assitance on aim zooming");
	pConsole->Register("aim_assistTriggerEnabled", &aim_assistTriggerEnabled, 1, 0, "Enable/disable aim assistance on firing the weapon");
	pConsole->Register("hit_assistSingleplayerEnabled", &hit_assistSingleplayerEnabled, 1, 0, "Enable/disable minimum damage hit assistance");
	pConsole->Register("hit_assistMultiplayerEnabled", &hit_assistMultiplayerEnabled, 1, 0, "Enable/disable minimum damage hit assistance in multiplayer games");

	//movement cvars
  pConsole->Register("v_profileMovement", &v_profileMovement, 0, 0, "Used to enable profiling of the current vehicle movement (1 to enable)");    
  pConsole->Register("v_pa_surface", &v_pa_surface, 1, VF_CHEAT, "Enables/disables vehicle surface particles");
  pConsole->Register("v_wind_minspeed", &v_wind_minspeed, 0.f, VF_CHEAT, "If non-zero, vehicle wind areas always set wind >= specified value");
  pConsole->Register("v_draw_suspension", &v_draw_suspension, 0, VF_DUMPTODISK, "Enables/disables display of wheel suspension, for the vehicle that has v_profileMovement enabled");
  pConsole->Register("v_draw_slip", &v_draw_slip, 0, VF_DUMPTODISK, "Draw wheel slip status");  
  pConsole->Register("v_invertPitchControl", &v_invertPitchControl, 0, VF_DUMPTODISK, "Invert the pitch control for driving some vehicles, including the helicopter and the vtol");
  pConsole->Register("v_sprintSpeed", &v_sprintSpeed, 0.f, 0, "Set speed for acceleration measuring");
  pConsole->Register("v_rockBoats", &v_rockBoats, 1, 0, "Enable/disable boats idle rocking");  
  pConsole->Register("v_dumpFriction", &v_dumpFriction, 0, 0, "Dump vehicle friction status");
  pConsole->Register("v_debugSounds", &v_debugSounds, 0, 0, "Enable/disable vehicle sound debug drawing");
  pConsole->Register("v_debugMountedWeapon", &v_debugMountedWeapon, 0, 0, "Enable/disable vehicle mounted weapon camera debug draw");
	pConsole->Register("v_newBrakingFriction", &v_newBrakingFriction, 1, VF_CHEAT, "Change rear wheel friction under handbraking (true/false)");
	pConsole->Register("v_newBoost", &v_newBoost, 1, VF_CHEAT, "Apply new boost scheme (true/false)");

	pAltitudeLimitCVar = pConsole->Register("v_altitudeLimit", &v_altitudeLimit, v_altitudeLimitDefault(), VF_CHEAT, "Used to restrict the helicopter and VTOL movement from going higher than a set altitude. If set to zero, the altitude limit is disabled.");
	pAltitudeLimitLowerOffsetCVar = pConsole->Register("v_altitudeLimitLowerOffset", &v_altitudeLimitLowerOffset, 0.1f, VF_CHEAT, "Used in conjunction with v_altitudeLimit to set the zone when gaining altitude start to be more difficult.");
  pConsole->Register("v_help_tank_steering", &v_help_tank_steering, 0, 0, "Enable tank steering help for AI");

	pConsole->Register("v_stabilizeVTOL", &v_stabilizeVTOL, 0.35f, VF_DUMPTODISK, "Specifies if the air movements should automatically stabilize");

  	
	pConsole->Register("pl_swimBaseSpeed", &pl_swimBaseSpeed, 4.0f, VF_CHEAT, "Swimming base speed.");
	pConsole->Register("pl_swimBackSpeedMul", &pl_swimBackSpeedMul, 0.8f, VF_CHEAT, "Swimming backwards speed mul.");
	pConsole->Register("pl_swimSideSpeedMul", &pl_swimSideSpeedMul, 0.9f, VF_CHEAT, "Swimming sideways speed mul.");
	pConsole->Register("pl_swimVertSpeedMul", &pl_swimVertSpeedMul, 0.5f, VF_CHEAT, "Swimming vertical speed mul.");
	pConsole->Register("pl_swimNormalSprintSpeedMul", &pl_swimNormalSprintSpeedMul, 1.5f, VF_CHEAT, "Swimming Non-Speed sprint speed mul.");
	pConsole->Register("pl_swimSpeedSprintSpeedMul", &pl_swimSpeedSprintSpeedMul, 2.5f, VF_CHEAT, "Swimming Speed sprint speed mul.");
	pConsole->Register("pl_swimUpSprintSpeedMul", &pl_swimUpSprintSpeedMul, 2.0f, VF_CHEAT, "Swimming sprint while looking up (dolphin rocket).");
	pConsole->Register("pl_swimJumpStrengthCost", &pl_swimJumpStrengthCost, 50.0f, VF_CHEAT, "Swimming strength shift+jump energy cost (dolphin rocket).");
	pConsole->Register("pl_swimJumpStrengthSprintMul", &pl_swimJumpStrengthSprintMul, 2.5f, VF_CHEAT, "Swimming strength shift+jump velocity mul (dolphin rocket).");
	pConsole->Register("pl_swimJumpStrengthBaseMul", &pl_swimJumpStrengthBaseMul, 1.0f, VF_CHEAT, "Swimming strength normal jump velocity mul (dolphin rocket).");
	pConsole->Register("pl_swimJumpSpeedCost", &pl_swimJumpSpeedCost, 50.0f, VF_CHEAT, "Swimming speed shift+jump energy cost (dolphin rocket).");
	pConsole->Register("pl_swimJumpSpeedSprintMul", &pl_swimJumpSpeedSprintMul, 2.5f, VF_CHEAT, "Swimming speed shift+jump velocity mul (dolphin rocket).");
	pConsole->Register("pl_swimJumpSpeedBaseMul", &pl_swimJumpSpeedBaseMul, 1.0f, VF_CHEAT, "Swimming speed normal jump velocity mul (dolphin rocket).");

	pConsole->Register("pl_fallDamage_SpeedSafe", &pl_fallDamage_Normal_SpeedSafe, 8.0f, VF_CHEAT, "Safe fall speed (in all modes, including strength jump on flat ground).");
	pConsole->Register("pl_fallDamage_SpeedFatal", &pl_fallDamage_Normal_SpeedFatal, 13.7f, VF_CHEAT, "Fatal fall speed in armor mode (13.5 m/s after falling freely for ca 20m).");
	pConsole->Register("pl_fallDamage_SpeedBias", &pl_fallDamage_SpeedBias, 1.5f, VF_CHEAT, "Damage bias for medium fall speed: =1 linear, <1 more damage, >1 less damage.");
	pConsole->Register("pl_debugFallDamage", &pl_debugFallDamage, 0, VF_CHEAT, "Enables console output of fall damage information.");
	

	pConsole->Register("pl_zeroGSpeedMultNormal", &pl_zeroGSpeedMultNormal, 1.2f, VF_CHEAT, "Modify movement speed in zeroG, in normal mode.");
	pConsole->Register("pl_zeroGSpeedMultNormalSprint", &pl_zeroGSpeedMultNormalSprint, 1.7f, VF_CHEAT, "Modify movement speed in zeroG, in normal sprint.");
  pConsole->Register("pl_zeroGSpeedMultSpeed", &pl_zeroGSpeedMultSpeed, 1.7f, VF_CHEAT, "Modify movement speed in zeroG, in speed mode.");
	pConsole->Register("pl_zeroGSpeedMultSpeedSprint", &pl_zeroGSpeedMultSpeedSprint, 5.0f, VF_CHEAT, "Modify movement speed in zeroG, in speed sprint.");
	pConsole->Register("pl_zeroGUpDown", &pl_zeroGUpDown, 1.0f, 0, "Scales the z-axis movement speed in zeroG.");
	pConsole->Register("pl_zeroGBaseSpeed", &pl_zeroGBaseSpeed, 3.0f, 0, "Maximum player speed request limit for zeroG.");
	pConsole->Register("pl_zeroGSpeedMaxSpeed", &pl_zeroGSpeedMaxSpeed, -1.0f, 0, "(DEPRECATED) Maximum player speed request limit for zeroG while in speed mode.");
	pConsole->Register("pl_zeroGSpeedModeEnergyConsumption", &pl_zeroGSpeedModeEnergyConsumption, 0.5f, 0, "Percentage consumed per second while speed sprinting in ZeroG.");
	pConsole->Register("pl_zeroGDashEnergyConsumption", &pl_zeroGDashEnergyConsumption, 0.25f, 0, "Percentage consumed when doing a dash in ZeroG.");
	pConsole->Register("pl_zeroGSwitchableGyro", &pl_zeroGSwitchableGyro, 0, 0, "MERGE/REVERT");
	pConsole->Register("pl_zeroGEnableGBoots", &pl_zeroGEnableGBoots, 0, 0, "Switch G-Boots action on/off (if button assigned).");
	pConsole->Register("pl_zeroGThrusterResponsiveness", &pl_zeroGThrusterResponsiveness, 0.3f, VF_CHEAT, "Thrusting responsiveness.");
	pConsole->Register("pl_zeroGFloatDuration", &pl_zeroGFloatDuration, 1.25f, VF_CHEAT, "Floating duration until full stop (after stopped thrusting).");
	pConsole->Register("pl_zeroGParticleTrail", &pl_zeroGParticleTrail, 0, 0, "Enable particle trail when in zerog.");
	pConsole->Register("pl_zeroGEnableGyroFade", &pl_zeroGEnableGyroFade, 2, VF_CHEAT, "Enable fadeout of gyro-stabilizer for vertical view angles (2=disable speed fade as well).");
	pConsole->Register("pl_zeroGGyroFadeAngleInner", &pl_zeroGGyroFadeAngleInner, 20.0f, VF_CHEAT, "ZeroG gyro inner angle (default is 20).");
	pConsole->Register("pl_zeroGGyroFadeAngleOuter", &pl_zeroGGyroFadeAngleOuter, 60.0f, VF_CHEAT, "ZeroG gyro outer angle (default is 60).");
	pConsole->Register("pl_zeroGGyroFadeExp", &pl_zeroGGyroFadeExp, 2.0f, VF_CHEAT, "ZeroG gyro angle bias (default is 2.0).");
	pConsole->Register("pl_zeroGGyroStrength", &pl_zeroGGyroStrength, 1.0f, VF_CHEAT, "ZeroG gyro strength (default is 1.0).");
	pConsole->Register("pl_zeroGAimResponsiveness", &pl_zeroGAimResponsiveness, 8.0f, VF_CHEAT, "ZeroG aim responsiveness vs. inertia (default is 8.0).");

	// weapon system
	i_debuggun_1 = pConsole->RegisterString("i_debuggun_1", "ai_statsTarget", VF_DUMPTODISK, "Command to execute on primary DebugGun fire");
	i_debuggun_2 = pConsole->RegisterString("i_debuggun_2", "ag_debug", VF_DUMPTODISK, "Command to execute on secondary DebugGun fire");

	pConsole->Register("tracer_min_distance", &tracer_min_distance, 4.0f, 0, "Distance at which to start scaling/lengthening tracers.");
	pConsole->Register("tracer_max_distance", &tracer_max_distance, 50.0f, 0, "Distance at which to stop scaling/lengthening tracers.");
	pConsole->Register("tracer_min_scale", &tracer_min_scale, 0.5f, 0, "Scale at min distance.");
	pConsole->Register("tracer_max_scale", &tracer_max_scale, 5.0f, 0, "Scale at max distance.");
	pConsole->Register("tracer_max_count", &tracer_max_count, 32, 0, "Max number of active tracers.");
	pConsole->Register("tracer_player_radiusSqr", &tracer_player_radiusSqr, 400.0f, 0, "Sqr Distance around player at which to start decelerate/acelerate tracer speed.");

	pConsole->Register("i_debug_projectiles", &i_debug_projectiles, 0, VF_CHEAT, "Displays info about projectile status, where available.");
	pConsole->Register("i_auto_turret_target", &i_auto_turret_target, 1, VF_CHEAT, "Enables/Disables auto turrets aquiring targets.");
	pConsole->Register("i_auto_turret_target_tacshells", &i_auto_turret_target_tacshells, 0, 0, "Enables/Disables auto turrets aquiring TAC shells as targets");

	pConsole->Register("i_debug_zoom_mods", &i_debug_zoom_mods, 0, VF_CHEAT, "Use zoom mode spread/recoil mods");
  pConsole->Register("i_debug_sounds", &i_debug_sounds, 0, VF_CHEAT, "Enable item sound debugging");
  pConsole->Register("i_debug_turrets", &i_debug_turrets, 0, VF_CHEAT, 
    "Enable GunTurret debugging.\n"
    "Values:\n"
    "0:  off"
    "1:  basics\n"
    "2:  prediction\n"
    "3:  sweeping\n"
    "4:  searching\n"      
    "5:  deviation\n"    
    );
	pConsole->Register("i_debug_mp_flowgraph", &i_debug_mp_flowgraph, 0, VF_CHEAT, "Displays info on the MP flowgraph node");
  
	pConsole->Register("h_turnSpeed", &h_turnSpeed, 1.3f, 0);

  // quick game

  g_quickGame_map = pConsole->RegisterString("g_quickGame_map","",VF_DUMPTODISK, "QuickGame option");
  g_quickGame_mode = pConsole->RegisterString("g_quickGame_mode","PowerStruggle", VF_DUMPTODISK, "QuickGame option");
  pConsole->Register("g_quickGame_min_players",&g_quickGame_min_players,0,VF_DUMPTODISK,"QuickGame option");
  pConsole->Register("g_quickGame_prefer_lan",&g_quickGame_prefer_lan,0,VF_DUMPTODISK,"QuickGame option");
  pConsole->Register("g_quickGame_prefer_favorites",&g_quickGame_prefer_favorites,0,VF_DUMPTODISK,"QuickGame option");
  pConsole->Register("g_quickGame_prefer_mycountry",&g_quickGame_prefer_my_country,0,VF_DUMPTODISK,"QuickGame option");
  pConsole->Register("g_quickGame_ping1_level",&g_quickGame_ping1_level,80,VF_DUMPTODISK,"QuickGame option");
  pConsole->Register("g_quickGame_ping2_level",&g_quickGame_ping2_level,170,VF_DUMPTODISK,"QuickGame option");

	pConsole->Register("g_quickGame_debug",&g_quickGame_debug,0,VF_CHEAT,"QuickGame option");
	
	pConsole->Register("g_displayIgnoreList",&g_displayIgnoreList,1,VF_DUMPTODISK,"Display ignore list in chat tab.");
  pConsole->Register("g_buddyMessagesIngame",&g_buddyMessagesIngame,1,VF_DUMPTODISK,"Output incoming buddy messages in chat while playing game.");

	pConsole->RegisterInt("g_showIdleStats", 0, 0);

	// battledust
	pConsole->Register("g_battleDust_enable", &g_battleDust_enable, 1, 0, "Enable/Disable battledust");
	pConsole->Register("g_battleDust_debug", &g_battleDust_debug, 0, 0, "0: off, 1: text, 2: text+gfx");
	g_battleDust_effect = pConsole->RegisterString("g_battleDust_effect", "misc.battledust.light", 0, "Sets the effect to use for battledust");
	
	pConsole->Register("g_PSTutorial_Enabled", &g_PSTutorial_Enabled, 1, 0, "Enable/disable powerstruggle tutorial");

	pConsole->Register("g_proneNotUsableWeapon_FixType", &g_proneNotUsableWeapon_FixType, 1, 0, "Test various fixes for not selecting hurricane while prone");
	pConsole->Register("g_proneAimAngleRestrict_Enable", &g_proneAimAngleRestrict_Enable, 1, 0, "Test fix for matching aim restrictions between 1st and 3rd person");

  pConsole->Register("sv_voting_timeout", &sv_votingTimeout, 60, 0, "Voting timeout");
  pConsole->Register("sv_voting_cooldown", &sv_votingCooldown, 180, 0, "Voting cooldown");
  pConsole->Register("sv_voting_ratio",&sv_votingRatio, 0.51f, 0, "Part of player's votes needed for successful vote.");
	pConsole->Register("sv_voting_team_ratio",&sv_votingTeamRatio, 0.67f, 0, "Part of team member's votes needed for successful vote.");

	pConsole->Register("sv_input_timeout",&sv_input_timeout, 0, 0, "Experimental timeout in ms to stop interpolating client inputs since last update.");

	pConsole->Register("g_spectate_TeamOnly", &g_spectate_TeamOnly, 1, 0, "If true, you can only spectate players on your team");
	pConsole->Register("g_spectate_FixedOrientation", &g_spectate_FixedOrientation, 0, 0, "If true, spectator camera is fixed behind player. Otherwise spectator controls orientation");
	pConsole->Register("g_claymore_limit", &g_claymore_limit, 3, 0, "Max claymores a player can place (recycled above this value)");
	pConsole->Register("g_avmine_limit", &g_avmine_limit, 3, 0, "Max avmines a player can place (recycled above this value)");
	pConsole->Register("g_debugMines", &g_debugMines, 0, 0, "Enable debug output for mines and claymores");

  pConsole->Register("aim_assistCrosshairSize", &aim_assistCrosshairSize, 25, VF_CHEAT, "screen size used for crosshair aim assistance");
  pConsole->Register("aim_assistCrosshairDebug", &aim_assistCrosshairDebug, 0, VF_CHEAT, "debug crosshair aim assistance");

	pConsole->Register("g_MPDeathCam", &g_deathCam, 1, 0, "Enables / disables the MP death camera (shows the killer's location)");
	pConsole->Register("g_MPDeathEffects", &g_deathEffects, 0, 0, "Enables / disables the MP death screen-effects");

	pConsole->Register("sv_pacifist", &sv_pacifist, 0, 0, "Pacifist mode (only works on dedicated server)");
 
	pVehicleQuality = pConsole->GetCVar("v_vehicle_quality");		assert(pVehicleQuality);

  NetInputChainInitCVars();
}

//------------------------------------------------------------------------
void SCVars::ReleaseCVars()
{
	IConsole* pConsole = gEnv->pConsole;

	pConsole->UnregisterVariable("cl_fov", true);
	pConsole->UnregisterVariable("cl_bob", true);
	pConsole->UnregisterVariable("cl_tpvDist", true);
	pConsole->UnregisterVariable("cl_tpvYaw", true);
	pConsole->UnregisterVariable("cl_nearPlane", true);
	pConsole->UnregisterVariable("cl_sprintShake", true);
	pConsole->UnregisterVariable("cl_sensitivityZeroG", true);
	pConsole->UnregisterVariable("cl_sensitivity", true);
	pConsole->UnregisterVariable("cl_controllersensitivity", true);
	pConsole->UnregisterVariable("cl_invertMouse", true);
	pConsole->UnregisterVariable("cl_invertController", true);
	pConsole->UnregisterVariable("cl_crouchToggle", true);
	pConsole->UnregisterVariable("cl_fpBody", true);
	pConsole->UnregisterVariable("cl_hud", true);

	pConsole->UnregisterVariable("i_staticfiresounds", true);
	pConsole->UnregisterVariable("i_soundeffects", true);
	pConsole->UnregisterVariable("i_lighteffects", true);
	pConsole->UnregisterVariable("i_lighteffectShadows", true);
	pConsole->UnregisterVariable("i_particleeffects", true);
	pConsole->UnregisterVariable("i_offset_front", true);
	pConsole->UnregisterVariable("i_offset_up", true);
	pConsole->UnregisterVariable("i_offset_right", true);
	pConsole->UnregisterVariable("i_unlimitedammo", true);
	pConsole->UnregisterVariable("i_iceeffects", true);

	pConsole->UnregisterVariable("cl_strengthscale", true);

	pConsole->UnregisterVariable("cl_motionBlur", true);
	pConsole->UnregisterVariable("cl_sprintBlur", true);
	pConsole->UnregisterVariable("cl_hitShake", true);
	pConsole->UnregisterVariable("cl_hitBlur", true);

	pConsole->UnregisterVariable("cl_righthand", true);
	pConsole->UnregisterVariable("cl_screeneffects", true);

	pConsole->UnregisterVariable("pl_inputAccel", true);

	pConsole->UnregisterVariable("cl_actorsafemode", true);
	pConsole->UnregisterVariable("g_hunterIK", true);
	pConsole->UnregisterVariable("g_tentacle_joint_limit", true);
	pConsole->UnregisterVariable("g_enableSpeedLean", true);

	pConsole->UnregisterVariable("int_zoomAmount", true);
	pConsole->UnregisterVariable("int_zoomInTime", true);
	pConsole->UnregisterVariable("int_moveZoomTime", true);
	pConsole->UnregisterVariable("int_zoomOutTime", true);

	pConsole->UnregisterVariable("aa_maxDist", true);

	pConsole->UnregisterVariable("hr_rotateFactor", true);
	pConsole->UnregisterVariable("hr_rotateTime", true);
	pConsole->UnregisterVariable("hr_dotAngle", true);
	pConsole->UnregisterVariable("hr_fovAmt", true);
	pConsole->UnregisterVariable("hr_fovTime", true);

	pConsole->UnregisterVariable("cl_debugFreezeShake", true);  
	pConsole->UnregisterVariable("cl_frozenSteps", true);  
	pConsole->UnregisterVariable("cl_frozenSensMin", true);
	pConsole->UnregisterVariable("cl_frozenSensMax", true);
	pConsole->UnregisterVariable("cl_frozenAngleMin", true);
	pConsole->UnregisterVariable("cl_frozenAngleMax", true);
	pConsole->UnregisterVariable("cl_frozenMouseMult", true);
	pConsole->UnregisterVariable("cl_frozenKeyMult", true);
	pConsole->UnregisterVariable("cl_frozenSoundDelta", true);
	
	pConsole->UnregisterVariable("g_frostDecay", true);

	pConsole->UnregisterVariable("g_stanceTransitionSpeed", true);
	pConsole->UnregisterVariable("g_stanceTransitionSpeedSecondary", true);

	pConsole->UnregisterVariable("g_playerHealthValue", true);
	pConsole->UnregisterVariable("g_walkMultiplier", true);
	pConsole->UnregisterVariable("g_suitSpeedMult", true);
	pConsole->UnregisterVariable("g_suitRecoilEnergyCost", true);
	pConsole->UnregisterVariable("g_suitSpeedMultMultiplayer", true);
	pConsole->UnregisterVariable("g_suitArmorHealthValue", true);
	pConsole->UnregisterVariable("g_suitSpeedEnergyConsumption", true);
	pConsole->UnregisterVariable("g_suitSpeedEnergyConsumptionMultiplayer", true);

	pConsole->UnregisterVariable("g_AiSuitEnergyRechargeTime", true);
	pConsole->UnregisterVariable("g_AiSuitHealthRechargeTime", true);
	pConsole->UnregisterVariable("g_AiSuitArmorModeHealthRegenTime", true);
	pConsole->UnregisterVariable("g_AiSuitStrengthMeleeMult", true);
	pConsole->UnregisterVariable("g_playerSuitEnergyRechargeTime", true);
	pConsole->UnregisterVariable("g_playerSuitEnergyRechargeTimeArmor", true);
	pConsole->UnregisterVariable("g_playerSuitEnergyRechargeTimeArmorMoving", true);
	pConsole->UnregisterVariable("g_playerSuitEnergyRechargeTimeMultiplayer", true);
	pConsole->UnregisterVariable("g_playerSuitHealthRechargeTime", true);
	pConsole->UnregisterVariable("g_playerSuitArmorModeHealthRegenTime", true);
	pConsole->UnregisterVariable("g_playerSuitHealthRechargeTimeMoving", true);
	pConsole->UnregisterVariable("g_playerSuitArmorModeHealthRegenTimeMoving", true);
	pConsole->UnregisterVariable("g_useHitSoundFeedback", true);

	pConsole->UnregisterVariable("g_playerLowHealthThreshold", true);
	pConsole->UnregisterVariable("g_playerLowHealthThreshold2", true);
	pConsole->UnregisterVariable("g_playerLowHealthThresholdMultiplayer", true);
	pConsole->UnregisterVariable("g_playerLowHealthThreshold2Multiplayer", true);

	pConsole->UnregisterVariable("g_pp_scale_income", true);
	pConsole->UnregisterVariable("g_pp_scale_price", true);

	pConsole->UnregisterVariable("g_radialBlur", true);
	pConsole->UnregisterVariable("g_PlayerFallAndPlay", true);
	pConsole->UnregisterVariable("g_fallAndPlayThreshold", true);

	pConsole->UnregisterVariable("g_enableAlternateIronSight",true);
	pConsole->UnregisterVariable("g_enableTracers", true);
	pConsole->UnregisterVariable("g_meleeWhileSprinting", true);

	pConsole->UnregisterVariable("g_timelimit", true);
	pConsole->UnregisterVariable("g_teamlock", true);
	pConsole->UnregisterVariable("g_roundlimit", true);
	pConsole->UnregisterVariable("g_preroundtime", true);
	pConsole->UnregisterVariable("g_suddendeathtime", true);
	pConsole->UnregisterVariable("g_roundtime", true);
	pConsole->UnregisterVariable("g_fraglimit", true);
	pConsole->UnregisterVariable("g_fraglead", true);
	pConsole->UnregisterVariable("g_debugNetPlayerInput", true);
	pConsole->UnregisterVariable("g_debug_fscommand", true);
	pConsole->UnregisterVariable("g_debugDirectMPMenu", true);
	pConsole->UnregisterVariable("g_skipIntro", true);
	pConsole->UnregisterVariable("g_resetActionmapOnStart", true);
	pConsole->UnregisterVariable("g_useProfile", true);
	pConsole->UnregisterVariable("g_startFirstTime", true);

	pConsole->UnregisterVariable("g_tk_punish", true);
	pConsole->UnregisterVariable("g_tk_punish_limit", true);

	pConsole->UnregisterVariable("g_godMode", true);
	pConsole->UnregisterVariable("g_detachCamera", true);

	pConsole->UnregisterVariable("g_debugCollisionDamage", true);
	pConsole->UnregisterVariable("g_debugHits", true);
	pConsole->UnregisterVariable("g_trooperProneMinDistance", true);
	pConsole->UnregisterVariable("g_trooperTentacleAnimBlend", true);
	pConsole->UnregisterVariable("g_trooperBankingMultiplier", true);

	pConsole->UnregisterVariable("v_profileMovement", true);    
	pConsole->UnregisterVariable("v_pa_surface", true);
	pConsole->UnregisterVariable("v_wind_minspeed", true);
	pConsole->UnregisterVariable("v_draw_suspension", true);
	pConsole->UnregisterVariable("v_draw_slip", true);  
	pConsole->UnregisterVariable("v_invertPitchControl", true);
	pConsole->UnregisterVariable("v_sprintSpeed", true);
	pConsole->UnregisterVariable("v_rockBoats", true);  
  pConsole->UnregisterVariable("v_debugMountedWeapon", true);  
	pConsole->UnregisterVariable("v_zeroGSpeedMultSpeed", true);
	pConsole->UnregisterVariable("v_zeroGSpeedMultSpeedSprint", true);
	pConsole->UnregisterVariable("v_zeroGSpeedMultNormal", true);
	pConsole->UnregisterVariable("v_zeroGSpeedMultNormalSprint", true);
	pConsole->UnregisterVariable("v_zeroGUpDown", true);
	pConsole->UnregisterVariable("v_zeroGMaxSpeed", true);
	pConsole->UnregisterVariable("v_zeroGSpeedMaxSpeed", true);
	pConsole->UnregisterVariable("v_zeroGSpeedModeEnergyConsumption", true);
	pConsole->UnregisterVariable("v_zeroGSwitchableGyro", true);
	pConsole->UnregisterVariable("v_zeroGEnableGBoots", true);
	pConsole->UnregisterVariable("v_dumpFriction", true);
  pConsole->UnregisterVariable("v_debugSounds", true);
	pConsole->UnregisterVariable("v_altitudeLimit", true);
	pConsole->UnregisterVariable("v_altitudeLimitLowerOffset", true);
	pConsole->UnregisterVariable("v_airControlSensivity", true);

	// variables from CPlayer
	pConsole->UnregisterVariable("player_DrawIK", true);
	pConsole->UnregisterVariable("player_NoIK", true);
	pConsole->UnregisterVariable("g_enableIdleCheck", true);
	pConsole->UnregisterVariable("pl_debug_ladders", true);
	pConsole->UnregisterVariable("pl_debug_movement", true);
	pConsole->UnregisterVariable("pl_debug_filter", true);

	// alien debugging
	pConsole->UnregisterVariable("aln_debug_movement", true);
	pConsole->UnregisterVariable("aln_debug_filter", true);

	// variables from CPlayerMovementController
	pConsole->UnregisterVariable("g_showIdleStats", true);
	pConsole->UnregisterVariable("g_debugaimlook", true);

	// variables from CHUD
	pConsole->UnregisterVariable("hud_mpNamesNearDistance", true);
	pConsole->UnregisterVariable("hud_mpNamesFarDistance", true);
	pConsole->UnregisterVariable("hud_onScreenNearDistance", true);
	pConsole->UnregisterVariable("hud_onScreenFarDistance", true);
	pConsole->UnregisterVariable("hud_onScreenNearSize", true);
	pConsole->UnregisterVariable("hud_onScreenFarSize", true);
	pConsole->UnregisterVariable("hud_colorLine", true);
	pConsole->UnregisterVariable("hud_colorOver", true);
	pConsole->UnregisterVariable("hud_colorText", true);
	pConsole->UnregisterVariable("hud_showAllObjectives", true);
	pConsole->UnregisterVariable("hud_godfadetime", true);
	pConsole->UnregisterVariable("g_combatfadetime", true);
	pConsole->UnregisterVariable("g_combatfadetimedelay", true);
	pConsole->UnregisterVariable("g_battlerange", true);
	pConsole->UnregisterVariable("hud_centernames", true);
	pConsole->UnregisterVariable("hud_crosshair", true);
	pConsole->UnregisterVariable("hud_chdamageindicator", true);
	pConsole->UnregisterVariable("hud_panoramic", true);
	pConsole->UnregisterVariable("hud_voicemode", true);
	pConsole->UnregisterVariable("hud_enableAlienInterference", true);
	pConsole->UnregisterVariable("hud_alienInterferenceStrength", true);
	pConsole->UnregisterVariable("hud_crosshair_enable", true);
	pConsole->UnregisterVariable("hud_attachBoughEquipment", true);
	pConsole->UnregisterVariable("hud_subtitlesRenderMode", true);
	pConsole->UnregisterVariable("hud_panoramicHeight", true);
	pConsole->UnregisterVariable("hud_subtitles", true);
	pConsole->UnregisterVariable("hud_subtitlesFontSize", true);
	pConsole->UnregisterVariable("hud_subtitlesHeight", true);
	pConsole->UnregisterVariable("hud_startPaused", true);
	pConsole->UnregisterVariable("hud_nightVisionRecharge", true);
	pConsole->UnregisterVariable("hud_nightVisionConsumption", true);
	pConsole->UnregisterVariable("hud_showBigVehicleReload", true);
	pConsole->UnregisterVariable("hud_binocsScanningDelay", true);
	pConsole->UnregisterVariable("hud_binocsScanningWidth", true);
	pConsole->UnregisterVariable("hud_alternateCrosshairSpread", true);
	pConsole->UnregisterVariable("hud_alternateCrosshairSpreadCrouch", true);
	pConsole->UnregisterVariable("hud_alternateCrosshairSpreadNeutral", true);

	// variables from CHUDRadar
	pConsole->UnregisterVariable("hud_radarBackground", true);
	pConsole->UnregisterVariable("hud_radarJammingThreshold", true);
	pConsole->UnregisterVariable("hud_radarJammingEffectScale", true);

	// Controller aim helper cvars
	pConsole->UnregisterVariable("aim_assistSearchBox", true);
	pConsole->UnregisterVariable("aim_assistMaxDistance", true);
	pConsole->UnregisterVariable("aim_assistSnapDistance", true);
	pConsole->UnregisterVariable("aim_assistVerticalScale", true);
	pConsole->UnregisterVariable("aim_assistSingleCoeff", true);
	pConsole->UnregisterVariable("aim_assistAutoCoeff", true);
	pConsole->UnregisterVariable("aim_assistRestrictionTimeout", true);

	pConsole->UnregisterVariable("hud_aspectCorrection", true);
	pConsole->UnregisterVariable("hud_ctrl_Curve_X", true);
	pConsole->UnregisterVariable("hud_ctrl_Curve_Z", true);
	pConsole->UnregisterVariable("hud_ctrl_Coeff_X", true);
	pConsole->UnregisterVariable("hud_ctrl_Coeff_Z", true);
	pConsole->UnregisterVariable("hud_ctrlZoomMode", true);

	// Aim assitance switches
	pConsole->UnregisterVariable("aim_assistAimEnabled", true);
	pConsole->UnregisterVariable("aim_assistTriggerEnabled", true);
	pConsole->UnregisterVariable("hit_assistSingleplayerEnabled", true);
	pConsole->UnregisterVariable("hit_assistMultiplayerEnabled", true);
		
	// weapon system
	pConsole->UnregisterVariable("i_debuggun_1", true);
	pConsole->UnregisterVariable("i_debuggun_2", true);

	pConsole->UnregisterVariable("tracer_min_distance", true);
	pConsole->UnregisterVariable("tracer_max_distance", true);
	pConsole->UnregisterVariable("tracer_min_scale", true);
	pConsole->UnregisterVariable("tracer_max_scale", true);
	pConsole->UnregisterVariable("tracer_max_count", true);
	pConsole->UnregisterVariable("tracer_player_radiusSqr", true);

	pConsole->UnregisterVariable("i_debug_projectiles", true);
	pConsole->UnregisterVariable("i_auto_turret_target", true);
	pConsole->UnregisterVariable("i_auto_turret_target_tacshells", true);

  pConsole->UnregisterVariable("i_debug_zoom_mods", true);
	pConsole->UnregisterVariable("i_debug_mp_flowgraph", true);

  pConsole->UnregisterVariable("g_quickGame_map",true);
  pConsole->UnregisterVariable("g_quickGame_mode",true);
  pConsole->UnregisterVariable("g_quickGame_min_players",true);
  pConsole->UnregisterVariable("g_quickGame_prefer_lan",true);
  pConsole->UnregisterVariable("g_quickGame_prefer_favorites",true);
  pConsole->UnregisterVariable("g_quickGame_prefer_mycountry",true);
  pConsole->UnregisterVariable("g_quickGame_ping1_level",true);
  pConsole->UnregisterVariable("g_quickGame_ping2_level",true);
	pConsole->UnregisterVariable("g_quickGame_debug",true);
	pConsole->UnregisterVariable("g_skip_tutorial",true);

	pConsole->UnregisterVariable("g_displayIgnoreList",true);
  pConsole->UnregisterVariable("g_buddyMessagesIngame",true);

  pConsole->UnregisterVariable("g_battleDust_enable", true);
  pConsole->UnregisterVariable("g_battleDust_debug", true);
	pConsole->UnregisterVariable("g_battleDust_effect", true);

  pConsole->UnregisterVariable("g_PSTutorial_Enabled", true);

  pConsole->UnregisterVariable("g_proneNotUsableWeapon_FixType", true);
	pConsole->UnregisterVariable("g_proneAimAngleRestrict_Enable", true);

  pConsole->UnregisterVariable("sv_voting_timeout",true);
  pConsole->UnregisterVariable("sv_voting_cooldown",true);
  pConsole->UnregisterVariable("sv_voting_ratio",true);
  pConsole->UnregisterVariable("sv_voting_team_ratio",true);

	pConsole->UnregisterVariable("g_spectate_TeamOnly", true);
	pConsole->UnregisterVariable("g_claymore_limit", true);
	pConsole->UnregisterVariable("g_avmine_limit", true);
	pConsole->UnregisterVariable("g_debugMines", true);

 pConsole->UnregisterVariable("aim_assistCrosshairSize", true);
  pConsole->UnregisterVariable("aim_assistCrosshairDebug", true);
}

//------------------------------------------------------------------------
void CGame::CmdDumpSS(IConsoleCmdArgs *pArgs)
{
	g_pGame->GetSynchedStorage()->Dump();
}

//------------------------------------------------------------------------
void CGame::RegisterConsoleVars()
{
	assert(m_pConsole);

	if (m_pCVars)
	{
		m_pCVars->InitCVars(m_pConsole);    
	}
}

//------------------------------------------------------------------------
void CmdDumpItemNameTable(IConsoleCmdArgs *pArgs)
{
	SharedString::CSharedString::DumpNameTable();
}

//------------------------------------------------------------------------
void CGame::RegisterConsoleCommands()
{
	assert(m_pConsole);

	m_pConsole->AddCommand("quit", "System.Quit()", VF_RESTRICTEDMODE, "Quits the game");
	m_pConsole->AddCommand("goto", "g_localActor:SetWorldPos({x=%1, y=%2, z=%3})", VF_CHEAT, "Sets current player position.");
	m_pConsole->AddCommand("gotoe", "local e=System.GetEntityByName(%1); if (e) then g_localActor:SetWorldPos(e:GetWorldPos()); end", VF_CHEAT, "Sets current player position.");
	m_pConsole->AddCommand("freeze", "g_gameRules:SetFrozenAmount(g_localActor,1)", 0, "Freezes player");

	m_pConsole->AddCommand("loadactionmap", CmdLoadActionmap, 0, "Loads a key configuration file");
	m_pConsole->AddCommand("restartgame", CmdRestartGame, 0, "Restarts Crysis completely.");

	m_pConsole->AddCommand("lastinv", CmdLastInv, 0, "Selects last inventory item used.");
	m_pConsole->AddCommand("name", CmdName, VF_RESTRICTEDMODE, "Sets player name.");
	m_pConsole->AddCommand("team", CmdTeam, VF_RESTRICTEDMODE, "Sets player team.");
	m_pConsole->AddCommand("loadLastSave", CmdLoadLastSave, 0, "Loads the last savegame if available.");
	m_pConsole->AddCommand("spectator", CmdSpectator, 0, "Sets the player as a spectator.");
	m_pConsole->AddCommand("join_game", CmdJoinGame, VF_RESTRICTEDMODE, "Enter the current ongoing game.");
	m_pConsole->AddCommand("kill", CmdKill, VF_RESTRICTEDMODE, "Kills the player.");
  m_pConsole->AddCommand("v_kill", CmdVehicleKill, VF_CHEAT, "Kills the players vehicle.");
	m_pConsole->AddCommand("sv_restart", CmdRestart, 0, "Restarts the round.");
	m_pConsole->AddCommand("sv_say", CmdSay, 0, "Broadcasts a message to all clients.");
	m_pConsole->AddCommand("i_reload", CmdReloadItems, 0, "Reloads item scripts.");

	m_pConsole->AddCommand("dumpss", CmdDumpSS, 0, "test synched storage.");
	m_pConsole->AddCommand("dumpnt", CmdDumpItemNameTable, 0, "Dump ItemString table.");

  m_pConsole->AddCommand("g_reloadGameRules", CmdReloadGameRules, 0, "Reload GameRules script");
  m_pConsole->AddCommand("g_quickGame", CmdQuickGame, 0, "Quick connect to good server.");
  m_pConsole->AddCommand("g_quickGameStop", CmdQuickGameStop, 0, "Cancel quick game search.");

  m_pConsole->AddCommand("g_nextlevel", CmdNextLevel,0,"Switch to next level in rotation or restart current one.");
  m_pConsole->AddCommand("vote", CmdVote, VF_RESTRICTEDMODE, "Vote on current topic.");
  m_pConsole->AddCommand("startKickVoting",CmdStartKickVoting, VF_RESTRICTEDMODE, "Initiate voting.");
	m_pConsole->AddCommand("listplayers",CmdListPlayers, VF_RESTRICTEDMODE, "Initiate voting.");
  m_pConsole->AddCommand("startNextMapVoting",CmdStartNextMapVoting, VF_RESTRICTEDMODE, "Initiate voting.");

	m_pConsole->AddCommand("g_battleDust_reload", CmdBattleDustReload, 0, "Reload the battle dust parameters xml");
	m_pConsole->AddCommand("login",CmdLogin, 0, "Log in to GameSpy using nickname and password as arguments");
	m_pConsole->AddCommand("login_profile", CmdLoginProfile, 0, "Log in to GameSpy using email, profile and password as arguments");
	m_pConsole->AddCommand("register", CmdRegisterNick, VF_CHEAT, "Register nickname with email, nickname and password");
	m_pConsole->AddCommand("connect_crynet",CmdCryNetConnect,0,"Connect to online game server");
	m_pConsole->AddCommand("preloadforstats","PreloadForStats()",VF_CHEAT,"Preload multiplayer assets for memory statistics.");
}

//------------------------------------------------------------------------
void CGame::UnregisterConsoleCommands()
{
	assert(m_pConsole);

	m_pConsole->RemoveCommand("quit");
	m_pConsole->RemoveCommand("goto");
	m_pConsole->RemoveCommand("freeze");

	m_pConsole->RemoveCommand("loadactionmap");
	m_pConsole->RemoveCommand("restartgame");

	m_pConsole->RemoveCommand("name");
	m_pConsole->RemoveCommand("team");
	m_pConsole->RemoveCommand("kill");
  m_pConsole->RemoveCommand("v_kill");
	m_pConsole->RemoveCommand("sv_restart");
	m_pConsole->RemoveCommand("sv_say");
	m_pConsole->RemoveCommand("i_reload");

	m_pConsole->RemoveCommand("dumpss");

	m_pConsole->RemoveCommand("g_reloadGameRules");
  m_pConsole->RemoveCommand("g_quickGame");
  m_pConsole->RemoveCommand("g_quickGameStop");

  m_pConsole->RemoveCommand("g_nextlevel");
  m_pConsole->RemoveCommand("vote");
  m_pConsole->RemoveCommand("startKickVoting");
  m_pConsole->RemoveCommand("startNextMapVoting");
	m_pConsole->RemoveCommand("listplayers");


	m_pConsole->RemoveCommand("g_battleDust_reload");
	m_pConsole->RemoveCommand("bulletTimeMode");
	m_pConsole->RemoveCommand("GOCMode");

	// variables from CHUDCommon
	m_pConsole->RemoveCommand("ShowGODMode");
}

//------------------------------------------------------------------------
void CGame::CmdLastInv(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->bClient)
		return;

	if (CActor *pClientActor=static_cast<CActor *>(g_pGame->GetIGameFramework()->GetClientActor()))
		pClientActor->SelectLastItem(true);
}

//------------------------------------------------------------------------
void CGame::CmdName(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->bClient)
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
		pGameRules->RenamePlayer(pGameRules->GetActorByEntityId(pClientActor->GetEntityId()), pArgs->GetArg(1));
}

//------------------------------------------------------------------------
void CGame::CmdTeam(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->bClient)
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
		pGameRules->ChangeTeam(pGameRules->GetActorByEntityId(pClientActor->GetEntityId()), pArgs->GetArg(1));
}

//------------------------------------------------------------------------
void CGame::CmdLoadLastSave(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->bClient || gEnv->bMultiplayer)
		return;

	if(g_pGame->GetMenu() && g_pGame->GetMenu()->IsActive())
		return;

	string* lastSave = NULL;
	if(g_pGame->GetMenu())
		lastSave = g_pGame->GetMenu()->GetLastInGameSave();
	if(lastSave && lastSave->size())
	{
		if(!g_pGame->GetIGameFramework()->LoadGame(lastSave->c_str(), true))
			g_pGame->GetIGameFramework()->LoadGame(g_pGame->GetLastSaveGame().c_str(), false);
	}
	else
	{
		const string& file = g_pGame->GetLastSaveGame().c_str();
		if(!g_pGame->GetIGameFramework()->LoadGame(file.c_str(), true))
			g_pGame->GetIGameFramework()->LoadGame(file.c_str(), false);
	}
}

//------------------------------------------------------------------------
void CGame::CmdSpectator(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->bClient)
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		int mode=2;
		if (pArgs->GetArgCount()==2)
			mode=atoi(pArgs->GetArg(1));
		pGameRules->ChangeSpectatorMode(pGameRules->GetActorByEntityId(pClientActor->GetEntityId()), mode, 0, true);
	}
}

//------------------------------------------------------------------------
void CGame::CmdJoinGame(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->bClient)
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	if (g_pGame->GetGameRules()->GetTeamCount()>0)
		return;
	
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
		pGameRules->ChangeSpectatorMode(pGameRules->GetActorByEntityId(pClientActor->GetEntityId()), 0, 0, true);
}

//------------------------------------------------------------------------
void CGame::CmdKill(IConsoleCmdArgs *pArgs)
{
	if (!gEnv->bClient)
		return;

	IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
	if (!pClientActor)
		return;

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		HitInfo suicideInfo(pClientActor->GetEntityId(), pClientActor->GetEntityId(), pClientActor->GetEntityId(),
			-1, 0, 0, -1, 0, ZERO, ZERO, ZERO);
		suicideInfo.SetDamage(10000);

		pGameRules->ClientHit(suicideInfo);
	}
}

//------------------------------------------------------------------------
void CGame::CmdVehicleKill(IConsoleCmdArgs *pArgs)
{
  if (!gEnv->bClient)
    return;

  IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
  if (!pClientActor)
    return;

  IVehicle* pVehicle = pClientActor->GetLinkedVehicle();
  if (!pVehicle)
    return;
  
  CGameRules *pGameRules = g_pGame->GetGameRules();
  if (pGameRules)
  {
    HitInfo suicideInfo(pVehicle->GetEntityId(), pVehicle->GetEntityId(), pVehicle->GetEntityId(),
      -1, 0, 0, -1, 0, pVehicle->GetEntity()->GetWorldPos(), ZERO, ZERO);
		suicideInfo.SetDamage(10000);

    pGameRules->ClientHit(suicideInfo);
  }
}

//------------------------------------------------------------------------
void CGame::CmdRestart(IConsoleCmdArgs *pArgs)
{
	if(g_pGame && g_pGame->GetGameRules())
		g_pGame->GetGameRules()->Restart();
}

//------------------------------------------------------------------------
void CGame::CmdSay(IConsoleCmdArgs *pArgs)
{
	if (pArgs->GetArgCount()>1 && gEnv->bServer)
	{
		const char *msg=pArgs->GetCommandLine()+strlen(pArgs->GetArg(0))+1;
		g_pGame->GetGameRules()->SendTextMessage(eTextMessageServer, msg, eRMI_ToAllClients);

		if (!gEnv->bClient)
			CryLogAlways("** Server: %s **", msg);
	}
}

//------------------------------------------------------------------------
void CGame::CmdLoadActionmap(IConsoleCmdArgs *pArgs)
{
	if(pArgs->GetArg(1))
		g_pGame->LoadActionMaps(pArgs->GetArg(1));
}

//------------------------------------------------------------------------
void CGame::CmdRestartGame(IConsoleCmdArgs *pArgs)
{
	GetISystem()->Relaunch(true);
	GetISystem()->Quit();
}

//------------------------------------------------------------------------
void CGame::CmdReloadItems(IConsoleCmdArgs *pArgs)
{
	g_pGame->GetItemSharedParamsList()->Reset();
	g_pGame->GetIGameFramework()->GetIItemSystem()->Reload();
	g_pGame->GetWeaponSystem()->Reload();
}

//------------------------------------------------------------------------
void CGame::CmdReloadGameRules(IConsoleCmdArgs *pArgs)
{
  if (gEnv->bMultiplayer)
    return;

  IGameRulesSystem* pGameRulesSystem = g_pGame->GetIGameFramework()->GetIGameRulesSystem();
  IGameRules* pGameRules = pGameRulesSystem->GetCurrentGameRules();
    
  const char* name = "SinglePlayer";
  IEntityClass* pEntityClass = 0; 
  
  if (pGameRules)    
  {
    pEntityClass = pGameRules->GetEntity()->GetClass();
    name = pEntityClass->GetName();
  }  
  else
    pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);

  if (pEntityClass)
  {
    pEntityClass->LoadScript(true);
  
    if (pGameRulesSystem->CreateGameRules(name))
      CryLog("reloaded GameRules <%s>", name);
    else
      GameWarning("reloading GameRules <%s> failed!", name);
  }  
}

void CGame::CmdNextLevel(IConsoleCmdArgs* pArgs)
{
  ILevelRotation *pLevelRotation = g_pGame->GetIGameFramework()->GetILevelSystem()->GetLevelRotation();
  if (pLevelRotation->GetLength())
    pLevelRotation->ChangeLevel(pArgs);
}

void CGame::CmdStartKickVoting(IConsoleCmdArgs* pArgs)
{
  if (!gEnv->bClient)
    return;

  if (pArgs->GetArgCount() < 2)
  {
    CryLogAlways("Usage: startKickVoting player_id");
		CryLogAlways("Use listplayers to get a list of player ids.");

    return;
  }

  IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
  if (!pClientActor)
    return;

  if (CGameRules *pGameRules = g_pGame->GetGameRules())
	{
		if (CActor *pActor=pGameRules->GetActorByChannelId(atoi(pArgs->GetArg(1))))
			pGameRules->StartVoting(static_cast<CActor *>(pClientActor), eVS_kick, pActor->GetEntityId(), "");
	}
}

void CGame::CmdStartNextMapVoting(IConsoleCmdArgs* pArgs)
{
  if (!gEnv->bClient)
    return;

  IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
  if (!pClientActor)
    return;

  if (CGameRules *pGameRules = g_pGame->GetGameRules())
    pGameRules->StartVoting(static_cast<CActor *>(pClientActor), eVS_nextMap, 0, "");
}


void CGame::CmdVote(IConsoleCmdArgs* pArgs)
{
  if (!gEnv->bClient)
    return;

  IActor *pClientActor=g_pGame->GetIGameFramework()->GetClientActor();
  if (!pClientActor)
    return;

  if (CGameRules *pGameRules = g_pGame->GetGameRules())
    pGameRules->Vote(pGameRules->GetActorByEntityId(pClientActor->GetEntityId()), true);
}

void CGame::CmdListPlayers(IConsoleCmdArgs* pArgs)
{
	if (CGameRules *pGameRules = g_pGame->GetGameRules())
	{
		CGameRules::TPlayers players;
		pGameRules->GetPlayers(players);

		if (!players.empty())
		{
			CryLogAlways("  [id]  [name]");

			for (CGameRules::TPlayers::iterator it=players.begin(); it!=players.end(); ++it)
			{
				if (CActor *pActor=pGameRules->GetActorByEntityId(*it))
					CryLogAlways(" %5d  %s", pActor->GetChannelId(), pActor->GetEntity()->GetName());
			}
		}
	}
}

void CGame::CmdQuickGame(IConsoleCmdArgs* pArgs)
{
  g_pGame->GetMenu()->GetMPHub()->OnQuickGame();
}

void CGame::CmdQuickGameStop(IConsoleCmdArgs* pArgs)
{
  
}

void CGame::CmdBattleDustReload(IConsoleCmdArgs* pArgs)
{
	if(CBattleDust* pBD = g_pGame->GetGameRules()->GetBattleDust())
	{
		pBD->ReloadXml();
	}
}

static bool GSCheckComplete()
{
  INetworkService* serv = gEnv->pNetwork->GetService("GameSpy");
  if(!serv)
    return true;
  return serv->GetState() != eNSS_Initializing;
}

static bool GSLoggingIn()
{
  return !g_pGame->GetMenu()->GetMPHub()->IsLoggingIn();
}

void CGame::CmdLogin(IConsoleCmdArgs* pArgs)
{
  if(pArgs->GetArgCount()>2)
  {
    g_pGame->BlockingProcess(&GSCheckComplete);
    INetworkService* serv = gEnv->pNetwork->GetService("GameSpy");
    if(!serv || serv->GetState() != eNSS_Ok)
      return;
		if(gEnv->pSystem->IsDedicated())
		{
			if(INetworkProfile* profile = serv->GetNetworkProfile())
			{
				profile->Login(pArgs->GetArg(1),pArgs->GetArg(2));
			}			
		}
		else
		{
			if(g_pGame->GetMenu() && g_pGame->GetMenu()->GetMPHub())
			{
				g_pGame->GetMenu()->GetMPHub()->DoLogin(pArgs->GetArg(1),pArgs->GetArg(2));
				g_pGame->BlockingProcess(&GSLoggingIn);
			}
		}
  }
  else
    GameWarning("Invalid parameters.");
}

static bool GSRegisterNick()
{
	INetworkService* serv = gEnv->pNetwork->GetService("GameSpy");
	if(!serv)
		return true;
	INetworkProfile* profile = serv->GetNetworkProfile();
	if(!profile)
		return true;
	return !profile->IsLoggingIn() || profile->IsLoggedIn();
}

void CGame::CmdRegisterNick(IConsoleCmdArgs* pArgs)
{
	if(!gEnv->pSystem->IsDedicated())
	{
		GameWarning("This can be used only on dedicated server.");
		return;
	}

	if(pArgs->GetArgCount()>3)
	{
		g_pGame->BlockingProcess(&GSCheckComplete);
		INetworkService* serv = gEnv->pNetwork->GetService("GameSpy");
		if(!serv || serv->GetState() != eNSS_Ok)
			return;
		if(INetworkProfile* profile = serv->GetNetworkProfile())
		{
			profile->Register(pArgs->GetArg(1), pArgs->GetArg(2), pArgs->GetArg(3), "", SRegisterDayOfBirth(ZERO));
		}
		g_pGame->BlockingProcess(&GSRegisterNick);
		if(INetworkProfile* profile = serv->GetNetworkProfile())
		{
			profile->Logoff();
		}
	}
	else
		GameWarning("Invalid parameters.");	
}

void CGame::CmdLoginProfile(IConsoleCmdArgs* pArgs)
{
	if(pArgs->GetArgCount()>3)
	{
		g_pGame->BlockingProcess(&GSCheckComplete);
		INetworkService* serv = gEnv->pNetwork->GetService("GameSpy");
		if(!serv || serv->GetState() != eNSS_Ok)
			return;
		g_pGame->GetMenu()->GetMPHub()->DoLoginProfile(pArgs->GetArg(1),pArgs->GetArg(2),pArgs->GetArg(3));

		g_pGame->BlockingProcess(&GSLoggingIn);
	}
	else
		GameWarning("Invalid parameters.");
}

static bool gGSConnecting = false;

struct SCryNetConnectListener : public IServerListener
{
  virtual void RemoveServer(const int id){}
  virtual void UpdatePing(const int id,const int ping){}
  virtual void UpdateValue(const int id,const char* name,const char* value){}
  virtual void UpdatePlayerValue(const int id,const int playerNum,const char* name,const char* value){}
  virtual void UpdateTeamValue(const int id,const int teamNum,const char *name,const char* value){}
  virtual void UpdateComplete(bool cancelled){}

  //we only need this thing to connect to server
  
  virtual void OnError(const EServerBrowserError)
  {
    End(false);
  }
  
  virtual void NewServer(const int id,const SBasicServerInfo* info)
  {
    UpdateServer(id, info);
  }

  virtual void UpdateServer(const int id,const SBasicServerInfo* info)
  {
    m_port = info->m_hostPort;
  }

  virtual void ServerUpdateFailed(const int id)
  {
    End(false);
  }
  virtual void ServerUpdateComplete(const int id)
  { 
    m_browser->CheckDirectConnect(id,m_port);
  }

  virtual void ServerDirectConnect(bool neednat, uint ip, ushort port)
  {
    string connect;
    if(neednat)
    {
      int cookie = rand() + (rand()<<16);
      connect.Format("connect <nat>%d|%d.%d.%d.%d:%d",cookie,ip&0xFF,(ip>>8)&0xFF,(ip>>16)&0xFF,(ip>>24)&0xFF,port);
      m_browser->SendNatCookie(ip,port,cookie);
    }
    else
    {
      connect.Format("connect %d.%d.%d.%d:%d",ip&0xFF,(ip>>8)&0xFF,(ip>>16)&0xFF,(ip>>24)&0xFF,port);
    }
    m_browser->Stop();
    End(true);
    g_pGame->GetIGameFramework()->ExecuteCommandNextFrame(connect.c_str());
  }

  void End(bool success)
  {
    if(!success)
      CryLog("Server is not responding.");
    gGSConnecting = false;
    m_browser->Stop();
    m_browser->SetListener(0);
    delete this;
  }

  IServerBrowser* m_browser;
  ushort m_port;
};


static bool GSConnect()
{
  return !gGSConnecting;
}

void CGame::CmdCryNetConnect(IConsoleCmdArgs* pArgs)
{
  ushort port = 64087;

  if(pArgs->GetArgCount()>2)
    port = atoi(pArgs->GetArg(2));
  else
  {
    ICVar* pv = GetISystem()->GetIConsole()->GetCVar("cl_serverport");
    if(pv)
      port = pv->GetIVal();
  }
  if(pArgs->GetArgCount()>1)
  {
    g_pGame->BlockingProcess(&GSCheckComplete);
    INetworkService* serv = gEnv->pNetwork->GetService("GameSpy");
    if(!serv || serv->GetState() != eNSS_Ok)
      return;
    IServerBrowser* sb = serv->GetServerBrowser();

    SCryNetConnectListener* lst = new SCryNetConnectListener();
    lst->m_browser = sb;
    sb->SetListener(lst);
    sb->Start(false);
    sb->BrowseForServer(pArgs->GetArg(1),port);
    gGSConnecting = true;
    g_pGame->BlockingProcess(&GSConnect);
  }
  else
    GameWarning("Invalid parameters.");
}
