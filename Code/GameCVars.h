// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#ifndef __GAMECVARS_H__
#define __GAMECVARS_H__

struct SCVars
{	
	static const float v_altitudeLimitDefault()
	{
		return 600.0f;
	}

	float cl_fov;
	float cl_bob;
	float cl_headBob;
	float cl_headBobLimit;
	float cl_tpvDist;
	float cl_tpvYaw;
	float cl_nearPlane;
	float cl_sprintShake;
	float cl_sensitivityZeroG;
	float cl_sensitivity;
	float cl_controllersensitivity;
	float cl_strengthscale;
	int		cl_invertMouse;
	int		cl_invertController;
	int		cl_crouchToggle;
	int		cl_fpBody;
	int   cl_hud;
	int		cl_debugSwimming;
	int   cl_g15lcdEnable;
	int   cl_g15lcdTick;

	ICVar* 	ca_GameControlledStrafingPtr;
	float pl_curvingSlowdownSpeedScale;
	float ac_enableProceduralLeaning;

	float cl_shallowWaterSpeedMulPlayer;
	float cl_shallowWaterSpeedMulAI;
	float cl_shallowWaterDepthLo;
	float cl_shallowWaterDepthHi;

	int		cl_debugFreezeShake;
	float cl_frozenSteps;
	float cl_frozenSensMin;
	float cl_frozenSensMax;
	float cl_frozenAngleMin;
	float cl_frozenAngleMax;
	float cl_frozenMouseMult;
	float cl_frozenKeyMult;
	float cl_frozenSoundDelta;
	int		goc_enable;
	int		goc_tpcrosshair;
	float goc_targetx;
	float goc_targety;
	float goc_targetz;

	// bullet time CVars
	int		bt_ironsight;
	int		bt_speed;
	int		bt_end_reload;
	int		bt_end_select;
	int		bt_end_melee;
	float bt_time_scale;
	float bt_pitch;
	float bt_energy_max;
	float bt_energy_decay;
	float bt_energy_regen;

	int		dt_enable;
	float dt_time;
	float dt_meleeTime;

	int   sv_votingTimeout;
  int   sv_votingCooldown;
  float sv_votingRatio;
  float sv_votingTeamRatio;

	int   sv_input_timeout;

	float hr_rotateFactor;
	float hr_rotateTime;
	float hr_dotAngle;
	float hr_fovAmt;
	float hr_fovTime;

	int		i_staticfiresounds;
	int		i_soundeffects;
	int		i_lighteffects;
	int		i_particleeffects;
	int		i_rejecteffects;
	float i_offset_front;
	float i_offset_up;
	float i_offset_right;
	int		i_unlimitedammo;
	int   i_iceeffects;
	int		i_lighteffectsShadows;
  
	float int_zoomAmount;
	float int_zoomInTime;
	float int_moveZoomTime;
	float int_zoomOutTime;

	float pl_inputAccel;

	float g_tentacle_joint_limit;
	int		g_godMode;
	int		g_detachCamera;
	int		g_enableSpeedLean;
	int   g_difficultyLevel;
	int		g_difficultyHintSystem;
	float g_difficultyRadius;
	int		g_difficultyRadiusThreshold;
	int		g_difficultySaveThreshold;
	int		g_playerHealthValue;
	float g_walkMultiplier;
	float g_suitRecoilEnergyCost;
	float g_suitSpeedMult;
	float g_suitSpeedMultMultiplayer;
	float g_suitArmorHealthValue;
	float g_suitSpeedEnergyConsumption;
	float g_suitSpeedEnergyConsumptionMultiplayer;
	float g_suitCloakEnergyDrainAdjuster;
	float g_AiSuitEnergyRechargeTime;
	float g_AiSuitHealthRegenTime;
	float g_AiSuitArmorModeHealthRegenTime;
	float g_playerSuitEnergyRechargeTime;
	float g_playerSuitEnergyRechargeTimeArmor;
	float g_playerSuitEnergyRechargeTimeArmorMoving;
	float g_playerSuitEnergyRechargeTimeMultiplayer;
	float g_playerSuitEnergyRechargeDelay;
	float g_playerSuitHealthRegenTime;
	float g_playerSuitHealthRegenTimeMoving;
	float g_playerSuitArmorModeHealthRegenTime;
	float g_playerSuitArmorModeHealthRegenTimeMoving;
	float g_playerSuitHealthRegenDelay;
	float g_frostDecay;
	float g_stanceTransitionSpeed;
	float g_stanceTransitionSpeedSecondary;
	int   g_debugaimlook;
	int		g_enableIdleCheck;
	int		g_playerRespawns;
	float g_playerLowHealthThreshold;
	float g_playerLowHealthThreshold2;
	float g_playerLowHealthThresholdMultiplayer;
	float g_playerLowHealthThreshold2Multiplayer;	
	int		g_punishFriendlyDeaths;
	int		g_enableMPStealthOMeter;
	int   g_meleeWhileSprinting;
	float g_AiSuitStrengthMeleeMult;
	float g_fallAndPlayThreshold;
	int		g_spRecordGameplay;
	float g_spGameplayRecorderUpdateRate;
	int		g_useHitSoundFeedback;

	int sv_pacifist;

	int g_empStyle;
	float g_empNanosuitDowntime;

	float g_pp_scale_income;
	float g_pp_scale_price;
	float g_energy_scale_income;
	float g_energy_scale_price;

	float g_dofset_minScale;
	float g_dofset_maxScale;
	float g_dofset_limitScale;

	float g_dof_minHitScale;
	float g_dof_maxHitScale;
	float g_dof_sampleAngle;
	float g_dof_minAdjustSpeed;
	float g_dof_maxAdjustSpeed;
	float g_dof_averageAdjustSpeed;
	float g_dof_distAppart;
	int		g_dof_ironsight;

	// explosion culling
	int		g_ec_enable;
	float g_ec_radiusScale;
	float g_ec_volume;
	float g_ec_extent;
	int		g_ec_removeThreshold;

	float g_radialBlur;
	int		g_playerFallAndPlay;

	float g_timelimit;
	int		g_teamlock;
	float g_roundtime;
	int		g_preroundtime;
	int		g_suddendeathtime;
	int		g_roundlimit;
	int		g_fraglimit;
  int		g_fraglead;
  float g_friendlyfireratio;
  int   g_revivetime; 
  int   g_autoteambalance;
	int		g_autoteambalance_threshold;
	int   g_minplayerlimit;
	int   g_minteamlimit;
	int		g_mpSpeedRechargeDelay;

	int   g_tk_punish;
	int		g_tk_punish_limit;

	int   g_debugNetPlayerInput;
	int   g_debugCollisionDamage;
	int   g_debugHits;

	float g_trooperProneMinDistance;
	/*	float g_trooperMaxPhysicAnimBlend;
	float g_trooperPhysicAnimBlendSpeed;
	float g_trooperPhysicsDampingAnim;
	*/
	float g_trooperTentacleAnimBlend;
	float g_trooperBankingMultiplier;
	float g_alienPhysicsAnimRatio;  

	int		g_debug_fscommand;
	int		g_debugDirectMPMenu;
	int		g_skipIntro;
	int		g_resetActionmapOnStart;
	int		g_useProfile;
	int		g_startFirstTime;
	float g_cutsceneSkipDelay;
	int		g_enableAutoSave;

	int   g_enableTracers;
	int		g_enableAlternateIronSight;

	float	g_ragdollMinTime;
	float	g_ragdollUnseenTime;
	float	g_ragdollPollTime;
	float	g_ragdollDistance;

	int		pl_debug_ladders;
	int		pl_debug_movement;
	int		pl_debug_jumping;
	ICVar*pl_debug_filter;

	int		aln_debug_movement;
	ICVar*aln_debug_filter;

	int   v_profileMovement;  
	int   v_draw_suspension;
	int   v_draw_slip;
	int   v_pa_surface;    
	int   v_invertPitchControl;  
	float v_wind_minspeed; 
	float v_sprintSpeed;
	int   v_dumpFriction;
	int   v_rockBoats;
  int   v_debugSounds;
	float v_altitudeLimit;
	ICVar* pAltitudeLimitCVar;
	float v_altitudeLimitLowerOffset;
	ICVar* pAltitudeLimitLowerOffsetCVar;
	float v_airControlSensivity;
	float v_stabilizeVTOL;
	int   v_help_tank_steering;
  int   v_debugMountedWeapon;
	ICVar* pVehicleQuality;
	int		v_newBrakingFriction;
	int		v_newBoost;

	float pl_swimBaseSpeed;
	float pl_swimBackSpeedMul;
	float pl_swimSideSpeedMul;
	float pl_swimVertSpeedMul;
	float pl_swimNormalSprintSpeedMul;
	float pl_swimSpeedSprintSpeedMul;
	float pl_swimUpSprintSpeedMul;
	float pl_swimJumpStrengthCost;
	float pl_swimJumpStrengthSprintMul;
	float pl_swimJumpStrengthBaseMul;
	float pl_swimJumpSpeedCost;
	float pl_swimJumpSpeedSprintMul;
	float pl_swimJumpSpeedBaseMul;

	float pl_fallDamage_Normal_SpeedSafe;
	float pl_fallDamage_Normal_SpeedFatal;
	float pl_fallDamage_Strength_SpeedSafe;
	float pl_fallDamage_Strength_SpeedFatal;
	float pl_fallDamage_SpeedBias;
	int pl_debugFallDamage;
	
  float pl_zeroGSpeedMultSpeed;
	float pl_zeroGSpeedMultSpeedSprint;
	float pl_zeroGSpeedMultNormal;
	float pl_zeroGSpeedMultNormalSprint;
	float pl_zeroGUpDown;
	float pl_zeroGBaseSpeed;
	float pl_zeroGSpeedMaxSpeed;
	float pl_zeroGSpeedModeEnergyConsumption;
	float	pl_zeroGDashEnergyConsumption;
	int		pl_zeroGSwitchableGyro;
	int		pl_zeroGEnableGBoots;
	float pl_zeroGThrusterResponsiveness;
	float pl_zeroGFloatDuration;
	int		pl_zeroGParticleTrail;
	int		pl_zeroGEnableGyroFade;
	float pl_zeroGGyroFadeAngleInner;
	float pl_zeroGGyroFadeAngleOuter;
	float pl_zeroGGyroFadeExp;
	float pl_zeroGGyroStrength;
	float pl_zeroGAimResponsiveness;

	int		hud_mpNamesDuration;
	int		hud_mpNamesNearDistance;
	int		hud_mpNamesFarDistance;
	int		hud_onScreenNearDistance;
	int		hud_onScreenFarDistance;
	float	hud_onScreenNearSize;
	float	hud_onScreenFarSize;
	int		hud_colorLine;
	int		hud_colorOver;
	int		hud_colorText;
	int		hud_voicemode;
	int		hud_enableAlienInterference;
	float	hud_alienInterferenceStrength;
	int		hud_godFadeTime;
	int		hud_crosshair_enable;
	int		hud_crosshair;
	int		hud_chDamageIndicator;
	int		hud_panoramicHeight;
	int		hud_showAllObjectives;
	int		hud_subtitles;
	int		hud_subtitlesDebug;
	int   hud_subtitlesRenderMode;
	int   hud_subtitlesHeight;
	int   hud_subtitlesFontSize;
	int   hud_subtitlesShowCharName;
	int   hud_subtitlesQueueCount;
	int   hud_subtitlesVisibleCount;
	int		hud_radarBackground;
	float	hud_radarJammingThreshold;
	float	hud_radarJammingEffectScale;
	int		hud_aspectCorrection;
	float hud_ctrl_Curve_X;
	float hud_ctrl_Curve_Z;
	float hud_ctrl_Coeff_X;
	float hud_ctrl_Coeff_Z;
	int		hud_ctrlZoomMode;
	int   hud_faderDebug;
	int		hud_attachBoughtEquipment;
	int		hud_startPaused;
	float hud_nightVisionRecharge;
	float hud_nightVisionConsumption;
	int		hud_showBigVehicleReload;
	float hud_binocsScanningDelay;
	float hud_binocsScanningWidth;
	//new crosshair spread code (Julien)
	float hud_fAlternateCrosshairSpreadCrouch;
	float hud_fAlternateCrosshairSpreadNeutral;
	int hud_iAlternateCrosshairSpread;

	float aim_assistSearchBox;
	float aim_assistMaxDistance;
	float aim_assistSnapDistance;
	float aim_assistVerticalScale;
	float aim_assistSingleCoeff;
	float aim_assistAutoCoeff;
	float aim_assistRestrictionTimeout;

	int aim_assistAimEnabled;
	int aim_assistTriggerEnabled;
	int hit_assistSingleplayerEnabled;
	int hit_assistMultiplayerEnabled;

  int aim_assistCrosshairSize;
  int aim_assistCrosshairDebug;
		
	float g_combatFadeTime;
	float g_combatFadeTimeDelay;
	float g_battleRange;

	ICVar*i_debuggun_1;
	ICVar*i_debuggun_2;

	float	tracer_min_distance;
	float	tracer_max_distance;
	float	tracer_min_scale;
	float	tracer_max_scale;
	int		tracer_max_count;
	float	tracer_player_radiusSqr;
	int		i_debug_projectiles;
	int		i_auto_turret_target;
	int		i_auto_turret_target_tacshells;
	int		i_debug_zoom_mods;
  int   i_debug_turrets;
  int   i_debug_sounds;
	int		i_debug_mp_flowgraph;
  
	float h_turnSpeed;
	int		h_useIK;
	int		h_drawSlippers;

  ICVar*  g_quickGame_map;
  ICVar*  g_quickGame_mode;
  int     g_quickGame_min_players;
  int     g_quickGame_prefer_lan;
  int     g_quickGame_prefer_favorites;
  int     g_quickGame_prefer_my_country;
  int     g_quickGame_ping1_level;
  int     g_quickGame_ping2_level;
  int     g_quickGame_debug;
	int			g_skip_tutorial;

  int     g_displayIgnoreList;
  int     g_buddyMessagesIngame;

  int			g_battleDust_enable;
	int			g_battleDust_debug;
	ICVar*  g_battleDust_effect;

	int			g_PSTutorial_Enabled;

	int			g_proneNotUsableWeapon_FixType;
	int			g_proneAimAngleRestrict_Enable;
	int			g_enableFriendlyFallAndPlay;

	int			g_spectate_TeamOnly;
	int			g_spectate_FixedOrientation;
	int			g_claymore_limit;
	int			g_avmine_limit;
	int			g_debugMines;
	int			g_deathCam;
	int			g_deathEffects;

	SCVars()
	{
		memset(this,0,sizeof(SCVars));
	}

	~SCVars() { ReleaseCVars(); }

	void InitCVars(IConsole *pConsole);
	void ReleaseCVars();
};

#endif //__GAMECVARS_H__
