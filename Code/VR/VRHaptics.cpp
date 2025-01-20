#include "StdAfx.h"
#include "ICryPak.h"
#include "VRHaptics.h"
#include "VRManager.h"
#include <HapticLibrary.h>

#include "GameCVars.h"

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
}

void VRHaptics::TriggerBHapticsEffect(const char* key, float intensity, float offsetAngleX, float offsetY)
{
	SubmitRegisteredWithOption(key, key, intensity * g_pGameCVars->vr_bhaptics_strength, 1.0f, offsetAngleX, offsetY);
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
}
