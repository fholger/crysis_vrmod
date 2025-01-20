#include "StdAfx.h"
#include "ICryPak.h"
#include "VRHaptics.h"
#include "VRManager.h"
#include <HapticLibrary.h>

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
	FILE* f = pak->FOpen(file, "rb");
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

	CryLogAlways("bHaptics effect %s loaded from %s", key, file);
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

bool VRHaptics::IsBHapticsEffectPlaying(const char* key) const
{
	return IsPlayingKey(key);
}

void VRHaptics::StopBHapticsEffect(const char* key)
{
	TurnOffKey(key);
}

void VRHaptics::InitEffects()
{
	RegisterBHapticsEffect("jump_vest", "bhaptics/vest/Jumping.tact");
	RegisterBHapticsEffect("land_vest", "bhaptics/vest/Landing.tact");
	RegisterBHapticsEffect("hit_vest", "bhaptics/vest/HitByBullet.tact");
	RegisterBHapticsEffect("punch_l_vest", "bhaptics/vest/Punch_L.tact");
	RegisterBHapticsEffect("punch_r_vest", "bhaptics/vest/Punch_R.tact");
}
