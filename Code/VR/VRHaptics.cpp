#include "StdAfx.h"
#include "ICryPak.h"
#include "VRHaptics.h"
#include "VRManager.h"
#include <HapticLibrary.h>
#include <protubevr.h>

#include "GameCVars.h"
#include "OpenXRRuntime.h"

namespace
{
	VRHaptics haptics;
}

VRHaptics* gHaptics = &haptics;


void VRHaptics::Init()
{
	InitialiseSync("crysisvr", "Crysis VR");
	InitRifle();
	InitEffects();
	CryLogAlways("Initialised bHaptics support");
}

void VRHaptics::Shutdown()
{
	Destroy();
}

void VRHaptics::RegisterBHapticsEffect(const char* key, const char* file)
{
	if (IsFeedbackRegistered(key))
	{
		return;
	}

	ICryPak* pak = gEnv->pCryPak;
	FILE* f = pak->FOpen(file, "r");
	if (!f)
	{
		CryLogAlways("BHaptics effect file not found: %s", file);
		return;
	}
	pak->FSeek(f, 0, SEEK_END);
	int size = pak->FTell(f);
	pak->FSeek(f, 0, SEEK_SET);

	char* buffer = new char[size+1];
	pak->FRead(buffer, size, f);
	pak->FClose(f);
	buffer[size] = 0;

	RegisterFeedbackFromTactFile(key, buffer);
	delete[] buffer;
}

void VRHaptics::TriggerBHapticsEffect(const char* key, float intensity, float offsetAngleX, float offsetY)
{
	SubmitRegisteredWithOption(key, key, intensity * g_pGameCVars->vr_bhaptics_strength, 1.0f, offsetAngleX, offsetY);
}

void VRHaptics::TriggerBHapticsEffect(const char* key, float intensity, const Vec3& pos, const Vec3& dir)
{
	CPlayer* player = gVR->GetLocalPlayer();
	if (!player)
		return;

	// figure out effect height in bHaptics units
	float effectHeight = pos.z - player->GetEntity()->GetWorldPos().z;
	float eyeHeight = gXR->GetHmdTransform().GetTranslation().z;
	// rough estimate: subtract a little for the head, then the vest effect area is approx. 50cm in length
	float vestMax = eyeHeight - 0.25f;
	float vestMin = vestMax - 0.5f;
	float vestMid = .5f * (vestMax + vestMin);
	float offsetY = clamp((effectHeight - vestMid) / (vestMax - vestMin), -0.5f, 0.5f);

	// determine yaw angle from direction
	Matrix33 transform = Matrix33::CreateRotationVDir(-dir);
	Ang3 angles (transform);
	// get offset to player angle
	Ang3 playerAng = player->GetEntity()->GetWorldAngles();
	float delta = fmodf(angles.z - playerAng.z, gf_PI2) * 180.f / gf_PI;
	if (angles.z > playerAng.z && delta >= 180.f)
		delta -= 360.f;
	else if (angles.z <= playerAng.z && delta <= -180.f)
		delta += 360.f;

	TriggerBHapticsEffect(key, intensity, delta, offsetY);
}

void VRHaptics::TriggerBHapticsEffectForSide(EVRHand hand, const char* keyLeft, const char* keyRight, float intensity)
{
	bool isLeftHand = gVR->GetHandSide(hand) == 0;
	TriggerBHapticsEffect(isLeftHand ? keyLeft : keyRight, intensity);
}

bool VRHaptics::IsBHapticsEffectPlaying(const char* key) const
{
	return IsPlayingKey(key);
}

void VRHaptics::StopBHapticsEffect(const char* key)
{
	TurnOffKey(key);
}

void VRHaptics::TriggerProtubeEffect(float kickPower, float rumblePower, float rumbleSeconds, bool offHand)
{
	if (kickPower > 0)
	{
		if (rumblePower > 0 && rumbleSeconds > 0)
			ProtubeShot(kickPower, rumblePower, rumbleSeconds, offHand);
		else
			ProtubeKick(kickPower, offHand);
	}
	else if (rumblePower > 0 && rumbleSeconds > 0)
	{
		ProtubeRumble(rumblePower, rumbleSeconds, offHand);
	}
}

void VRHaptics::TriggerProtubeEffectWeapon(float kickPower, float rumblePower, float rumbleSeconds)
{
	TriggerProtubeEffect(kickPower, rumblePower, rumbleSeconds);
	if (gVR->IsOffHandGrabbingWeapon())
		TriggerProtubeEffect(kickPower, rumblePower, rumbleSeconds, true);
}

void VRHaptics::InitEffects()
{
	RegisterBHapticsEffect("jump_vest", "bhaptics/vest/Jumping.tact");
	RegisterBHapticsEffect("land_vest", "bhaptics/vest/Landing.tact");
	RegisterBHapticsEffect("hit_vest", "bhaptics/vest/HitByBullet.tact");
	RegisterBHapticsEffect("explosion_vest", "bhaptics/vest/HitByExplosion.tact");
	RegisterBHapticsEffect("punch_l_vest", "bhaptics/vest/Punch_L.tact");
	RegisterBHapticsEffect("punch_r_vest", "bhaptics/vest/Punch_R.tact");
	RegisterBHapticsEffect("shoot_l_vest", "bhaptics/vest/ShootSMG_L.tact");
	RegisterBHapticsEffect("shoot_r_vest", "bhaptics/vest/ShootSMG_R.tact");
	RegisterBHapticsEffect("shotgun_l_vest", "bhaptics/vest/ShootShotgun_L.tact");
	RegisterBHapticsEffect("shotgun_r_vest", "bhaptics/vest/ShootShotgun_R.tact");
	RegisterBHapticsEffect("rpg_l_vest", "bhaptics/vest/ShootRPG_L.tact");
	RegisterBHapticsEffect("rpg_r_vest", "bhaptics/vest/ShootRPG_R.tact");
	RegisterBHapticsEffect("shake_vest", "bhaptics/vest/Shake.tact");
	RegisterBHapticsEffect("drowning_vest", "bhaptics/vest/Drowning.tact");
	RegisterBHapticsEffect("swimming_vest", "bhaptics/vest/Swimming.tact");
	RegisterBHapticsEffect("suitarmor_vest", "bhaptics/vest/SuitArmor.tact");
	RegisterBHapticsEffect("suitspeed_vest", "bhaptics/vest/SuitSpeed.tact");
	RegisterBHapticsEffect("suitstealth_vest", "bhaptics/vest/SuitStealth.tact");
	RegisterBHapticsEffect("suitstrength_vest", "bhaptics/vest/SuitStrength.tact");
	RegisterBHapticsEffect("heartbeat_vest", "bhaptics/vest/Heartbeat.tact");
	RegisterBHapticsEffect("speedsprint_vest", "bhaptics/vest/SpeedSprint.tact");
	RegisterBHapticsEffect("speedsprintwater_vest", "bhaptics/vest/SpeedSprintWater.tact");
	RegisterBHapticsEffect("vehicle_vest", "bhaptics/vest/VehicleRumble.tact");
	RegisterBHapticsEffect("shoot_l_arm", "bhaptics/arms/Recoil_Arm_L.tact");
	RegisterBHapticsEffect("shoot_r_arm", "bhaptics/arms/Recoil_Arm_R.tact");
}

void VRHaptics::ProtubeKick(float power, bool offHand)
{
	uint8 pw = clamp_tpl(power, 0.f, 1.f) * 255;
	ForceTubeVRChannel channel = offHand ? rifleBolt : rifleButt;
	KickChannel(pw, channel);
}

void VRHaptics::ProtubeRumble(float power, float seconds, bool offHand)
{
	uint8 pw = clamp_tpl(power, 0.f, 1.f) * 255;
	ForceTubeVRChannel channel = offHand ? rifleBolt : rifleButt;
	RumbleChannel(pw, seconds, channel);
}

void VRHaptics::ProtubeShot(float kickPower, float rumblePower, float rumbleSeconds, bool offHand)
{
	uint8 kpw = clamp_tpl(kickPower, 0.f, 1.f) * 255;
	uint8 rpw = clamp_tpl(rumblePower, 0.f, 1.f) * 255;
	ForceTubeVRChannel channel = offHand ? rifleBolt : rifleButt;
	ShotChannel(kpw, rpw, rumbleSeconds, channel);
}
