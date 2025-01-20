#pragma once

class VRHaptics
{
public:
	void Init();
	void Shutdown();

	void RegisterBHapticsEffect(const char* key, const char* file);

	void TriggerBHapticsEffect(const char* key, float intensity = 1.0f, float offsetAngleX = 0, float offsetY = 0);
	bool IsBHapticsEffectPlaying(const char* key) const;
	void StopBHapticsEffect(const char* key);

private:
	void InitEffects();
};

extern VRHaptics* gHaptics;
