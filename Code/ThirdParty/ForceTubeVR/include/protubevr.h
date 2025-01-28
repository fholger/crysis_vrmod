#pragma once

enum ForceTubeVRChannel { all, rifle, rifleButt, rifleBolt, pistol1, pistol2, other, vest };

extern "C"
{
	__declspec(dllimport) void InitRifle();
	__declspec(dllimport) void InitPistol();
	__declspec(dllimport) void KickChannel(uint8 power, ForceTubeVRChannel channel);
	__declspec(dllimport) void RumbleChannel(uint8 power, float rumbleDuration, ForceTubeVRChannel channel);
	__declspec(dllimport) void ShotChannel(uint8 kickPower, uint8 rumblePower, uint8 rumbleDuration, ForceTubeVRChannel channel);
	__declspec(dllimport) uint8 TempoToKickPower(float tempo);
	__declspec(dllimport) uint8 GetBatteryLevel();
	__declspec(dllimport) void SetActive(bool nActive);
}
