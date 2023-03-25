// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#ifndef __AMMOPARAMS_H__
#define __AMMOPARAMS_H__

#pragma once

#include <IItemSystem.h>
#include <IWeapon.h>

struct SScaledEffectParams
{
	const char* ppname;
	float radius;
	float delay;
	float fadeInTime;
	float fadeOutTime;
	float maxValue;
	float aiObstructionRadius;

	SScaledEffectParams(const IItemParamsNode* scaledEffect);
};

struct SCollisionParams
{
	IParticleEffect*	pParticleEffect;
	const char*				sound;
	float							scale;

	SCollisionParams(const IItemParamsNode* collision);
	~SCollisionParams();
};

struct SExplosionParams
{
	float minRadius;
	float maxRadius;
	float minPhysRadius;
	float maxPhysRadius;
	float pressure;
	float holeSize;
	float terrainHoleSize;
	IParticleEffect* pParticleEffect;
	const char *effectName;
	float effectScale;
	string type;
	int		hitTypeId;
	float maxblurdist;

	SExplosionParams(const IItemParamsNode* explosion);
	~SExplosionParams();
};

struct SFlashbangParams
{
	float maxRadius;
	float blindAmount;
	float flashbangBaseTime;

	SFlashbangParams(const IItemParamsNode* flashbang);
};

struct STrailParams
{
	const char* sound;
	const char*	effect;
	const char* effect_fp;
	float				scale;
	bool				prime;

	STrailParams(const IItemParamsNode* trail);
};

struct SWhizParams
{
	const char* sound;
	float				speed;

	SWhizParams(const IItemParamsNode* whiz);
};


// this structure holds cached XML attributes for fast acccess
struct SAmmoParams
{
	//flags
	uint	flags;
	int		serverSpawn;
	int		predictSpawn;

	// common parameters
	float	lifetime;
	float	showtime;
	ushort aiType;
	int		bulletType;
	int		hitPoints;
	bool   noBulletHits;
	bool	quietRemoval;
	float sleepTime;

	// physics parameters
	EPhysicalizationType	physicalizationType;
	float mass;
	float speed;
	int		maxLoggedCollisions;
	int		traceable;
	Vec3	spin;
	Vec3	spinRandom;

	ISurfaceType*							pSurfaceType;
	pe_params_particle*				pParticleParams;

	// firstperson geometry
	IStatObj*	fpGeometry;
	Matrix34	fpLocalTM;

	SScaledEffectParams*	pScaledEffect;
	SCollisionParams*			pCollision;
	SExplosionParams*			pExplosion;
	SFlashbangParams*			pFlashbang;
	SWhizParams*					pWhiz;
	SWhizParams*					pRicochet;
	STrailParams*					pTrail;
	STrailParams*					pTrailUnderWater;

	const IEntityClass*			pEntityClass;
	const IItemParamsNode*	pItemParams;

	SAmmoParams(const IItemParamsNode* pItemParams_ = 0, const IEntityClass* pEntityClass_=0);
	~SAmmoParams();

	void Init(const IItemParamsNode* pItemParams_, const IEntityClass* pEntityClass_);

	int GetMemorySize() const;

private:
	void LoadFlagsAndParams();
	void LoadPhysics();
	void LoadGeometry();
	void LoadScaledEffect();
	void LoadCollision();
	void LoadExplosion();
	void LoadFlashbang();
	void LoadTrailsAndWhizzes();
};

#endif//__AMMOPARAMS_H__
