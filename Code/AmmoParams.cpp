// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "AmmoParams.h"
#include "ItemParamReader.h"
#include "GameRules.h"

SScaledEffectParams::SScaledEffectParams(const IItemParamsNode* scaledEffect)
: ppname(0)
, radius(0.0f)
, delay(2.0f)
, fadeInTime(1.0f)
, fadeOutTime(1.0f)
, maxValue(1.0f)
, aiObstructionRadius(5.0f)
{
	CItemParamReader reader(scaledEffect);
	reader.Read("postEffect", ppname);
	if (ppname)
	{
		reader.Read("radius", radius);
		reader.Read("delay", delay);
		reader.Read("fadeintime", fadeInTime);
		reader.Read("fadeouttime", fadeOutTime);
		reader.Read("maxvalue", maxValue);
		reader.Read("AIObstructionRadius", aiObstructionRadius);
	}
}

SCollisionParams::SCollisionParams(const IItemParamsNode* collision)
: pParticleEffect(0)
, sound(0)
, scale(1.0f)
{
	CItemParamReader reader(collision);

	reader.Read("sound", sound);
	reader.Read("scale", scale);

	const char* effect;
	reader.Read("effect", effect);

	if (effect && effect[0])
	{
		pParticleEffect = gEnv->p3DEngine->FindParticleEffect(effect);
		if (pParticleEffect)
			pParticleEffect->AddRef();
	}
}

SCollisionParams::~SCollisionParams()
{
	SAFE_RELEASE(pParticleEffect);
}

SExplosionParams::SExplosionParams(const IItemParamsNode* explosion)
: minRadius(2.5f)
, maxRadius(5.0f)
, minPhysRadius(2.5f)
, maxPhysRadius(5.0f)
, pressure(200.0f)
, holeSize(0.0f)
, terrainHoleSize(3.0f)
, effectScale(1)
, effectName(0)
, maxblurdist(10)
, hitTypeId(0)
, pParticleEffect(0)
{
	const char* effect = 0;

	CItemParamReader reader(explosion);
	reader.Read("max_radius", maxRadius);
	minRadius = maxRadius * 0.8f;
	maxPhysRadius = min(maxRadius, 5.0f);
	minPhysRadius = maxPhysRadius * 0.8f;

	reader.Read("min_radius", minRadius);
	reader.Read("min_phys_radius", minPhysRadius);
	reader.Read("max_phys_radius", maxPhysRadius);
	reader.Read("pressure", pressure);
	reader.Read("hole_size", holeSize);
	reader.Read("terrain_hole_size", terrainHoleSize);
	reader.Read("effect", effect);
	reader.Read("effect_scale", effectScale);
	reader.Read("radialblurdist", maxblurdist);
	reader.Read("type", type);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
		hitTypeId = pGameRules->GetHitTypeId(type.c_str());

	if (effect && effect[0])
	{
		effectName=effect;
		pParticleEffect = gEnv->p3DEngine->FindParticleEffect(effect);
		if (pParticleEffect)
			pParticleEffect->AddRef();
	}
}

SExplosionParams::~SExplosionParams()
{
	SAFE_RELEASE(pParticleEffect);
}

SFlashbangParams::SFlashbangParams(const IItemParamsNode* flashbang)
: maxRadius(25.0f),
blindAmount(0.7f),
flashbangBaseTime(2.5f)
{
	CItemParamReader reader(flashbang);
	reader.Read("max_radius", maxRadius);
	reader.Read("blindAmount", blindAmount);
	reader.Read("flashbangBaseTime", flashbangBaseTime);
}

STrailParams::STrailParams(const IItemParamsNode* trail)
:	sound(0)
, effect(0)
, effect_fp(0)
, scale(1.0f)
, prime(true)
{
	CItemParamReader reader(trail);
	reader.Read("sound", sound);
	if (sound && !sound[0])
		sound = 0;

	reader.Read("effect", effect);
	if (effect && !effect[0])
		effect = 0;
	reader.Read("effect_fp", effect_fp);
	if (effect_fp && !effect_fp[0])
		effect_fp = 0;

	reader.Read("scale", scale);
	reader.Read("prime", prime);
}

SWhizParams::SWhizParams(const IItemParamsNode* whiz)
: sound(0)
, speed(20.0f)
{
	CItemParamReader reader(whiz);
	reader.Read("sound", sound);
	if (sound && !sound[0])
		sound = 0;

	reader.Read("speed", speed);
}

SAmmoParams::SAmmoParams(const IItemParamsNode* pItemParams_, const IEntityClass* pEntityClass_)
: flags(0)
, serverSpawn(0)
, predictSpawn(0)
, lifetime(0.0f)
, showtime(0.0f)
, aiType(AIOBJECT_NONE)
, bulletType(-1)
, hitPoints(-1)
, noBulletHits(false)
, quietRemoval(false)
, physicalizationType(ePT_None)
, mass(1.0f)
, speed(0.0f)
, maxLoggedCollisions(1)
, traceable(0)
, spin(ZERO)
, spinRandom(ZERO)
, pSurfaceType(0)
, pParticleParams(0)
, fpGeometry(0)
, pScaledEffect(0)
, pCollision(0)
, pExplosion(0)
, pFlashbang(0)
, pWhiz(0)
, pRicochet(0)
, pTrail(0)
, pTrailUnderWater(0)
, pItemParams(0)
, pEntityClass(0)
, sleepTime(0.0f)
{
	Init(pItemParams_, pEntityClass_);
}

SAmmoParams::~SAmmoParams()
{
	delete pScaledEffect;
	delete pCollision;
	delete pExplosion;
	delete pFlashbang;
	delete pWhiz;
	delete pRicochet;
	delete pTrail;
	delete pTrailUnderWater;
	SAFE_RELEASE(pItemParams);
	SAFE_RELEASE(fpGeometry);
}

void SAmmoParams::Init(const IItemParamsNode* pItemParams_, const IEntityClass* pEntityClass_)
{
	pItemParams = pItemParams_;
	pEntityClass = pEntityClass_;

	if (!pItemParams || !pEntityClass)
	{
		assert(0);
		return;
	}

	pItemParams->AddRef();

	LoadFlagsAndParams();
	LoadPhysics();
	LoadGeometry();
	LoadScaledEffect();
	LoadCollision();
	LoadExplosion();
	LoadFlashbang();
	LoadTrailsAndWhizzes();
}

int SAmmoParams::GetMemorySize() const
{
	int nSize = sizeof(*this);

	if (pItemParams)
	{
		nSize += pItemParams->GetMemorySize();
	}

	return nSize;
}

void SAmmoParams::LoadFlagsAndParams()
{
	const IItemParamsNode* flagsNode = pItemParams->GetChild("flags");
	if (flagsNode)
	{
		int flag=0;
		CItemParamReader reader(flagsNode);
		reader.Read("ClientOnly", flag); flags |= flag?ENTITY_FLAG_CLIENT_ONLY:0; flag=0;
		reader.Read("ServerOnly", flag); flags |= flag?ENTITY_FLAG_SERVER_ONLY:0; flag=0;
		reader.Read("ServerSpawn", serverSpawn);
		if (serverSpawn)
			reader.Read("PredictSpawn", predictSpawn);
	}

	const IItemParamsNode* paramsNode = pItemParams->GetChild("params");
	if (paramsNode)
	{
		CItemParamReader reader(paramsNode);
		reader.Read("lifetime", lifetime);
		reader.Read("showtime", showtime);
		reader.Read("bulletType",bulletType);
		reader.Read("hitPoints",hitPoints);
		reader.Read("noBulletHits",noBulletHits);
		reader.Read("quietRemoval",quietRemoval);
		reader.Read("sleepTime", sleepTime);

		const char* typeName=0;
		reader.Read("aitype", typeName);
		
		if (typeName && typeName[0])
		{
			if (!stricmp(typeName, "grenade"))
				aiType=AIOBJECT_GRENADE;
			else if (!stricmp(typeName, "rpg"))
				aiType=AIOBJECT_RPG;
		}
	}
}

void SAmmoParams::LoadPhysics()
{
	const IItemParamsNode *physics = pItemParams->GetChild("physics");

	if (!physics)
		return;

	const char *typ=physics->GetAttribute("type");
	if (typ)
	{
		if (!strcmpi(typ, "particle"))
		{
			physicalizationType = ePT_Particle;
		}
		else if (!strcmpi(typ, "rigid"))
		{
			physicalizationType = ePT_Rigid;
		}
		else if(!strcmpi(typ, "static"))
		{
			physicalizationType = ePT_Static;
		}
		else
		{
			GameWarning("Unknow physicalization type '%s' for projectile '%s'!", typ, pEntityClass->GetName());
		}
	}

	CItemParamReader reader(physics);
	if(physicalizationType != ePT_Static)
	{
		reader.Read("mass", mass);
		reader.Read("speed", speed);
		reader.Read("max_logged_collisions", maxLoggedCollisions);
		reader.Read("traceable", traceable);
		reader.Read("spin", spin);
		reader.Read("spin_random", spinRandom);
	}

	// material
	const char *material=0;
	reader.Read("material", material);
	if (material)
	{
		pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeByName(material);
	}

	if (physicalizationType == ePT_Particle)
	{
		pParticleParams = new pe_params_particle();
		float radius=0.005f;
		reader.Read("radius", radius);
		pParticleParams->thickness = radius*2.0f;
		pParticleParams->size = radius*2.0f;
		reader.Read("air_resistance", pParticleParams->kAirResistance);
		reader.Read("water_resistance", pParticleParams->kWaterResistance);
		reader.Read("gravity", pParticleParams->gravity);
		reader.Read("water_gravity", pParticleParams->waterGravity);
		reader.Read("thrust", pParticleParams->accThrust);
		reader.Read("lift", pParticleParams->accLift);
		reader.Read("min_bounce_speed", pParticleParams->minBounceVel);
		reader.Read("pierceability", pParticleParams->iPierceability);

		if (pSurfaceType)
			pParticleParams->surface_idx = pSurfaceType->GetId();

		int flag=0;
		reader.Read("single_contact", flag);
		pParticleParams->flags = flag?particle_single_contact:0; flag=0;
		reader.Read("no_roll", flag);
		pParticleParams->flags |= flag?particle_no_roll:0; flag=0;
		reader.Read("no_spin", flag);
		pParticleParams->flags |= flag?particle_no_spin:0; flag=0;
		reader.Read("no_path_alignment", flag);
		pParticleParams->flags |= flag?particle_no_path_alignment:0; flag=0;

		pParticleParams->mass= mass;
	}
}

void SAmmoParams::LoadGeometry()
{
	const IItemParamsNode *geometry = pItemParams->GetChild("geometry");
	if (!geometry)
		return;

	const IItemParamsNode *firstperson = geometry->GetChild("firstperson");
	if (firstperson)
	{
		const char *modelName = firstperson->GetAttribute("name");

		if (modelName && modelName[0])
		{
			IStatObj* oldGeom = fpGeometry;

			Ang3 angles(0,0,0);
			Vec3 position(0,0,0);
			float scale=1.0f;
			firstperson->GetAttribute("position", position);
			firstperson->GetAttribute("angles", angles);
			firstperson->GetAttribute("scale", scale);

			fpLocalTM = Matrix34(Matrix33::CreateRotationXYZ(DEG2RAD(angles)));
			fpLocalTM.Scale(Vec3(scale, scale, scale));
			fpLocalTM.SetTranslation(position);

			fpGeometry = gEnv->p3DEngine->LoadStatObj(modelName);
			if (fpGeometry)
				fpGeometry->AddRef();

			if (oldGeom)
				oldGeom->Release();
		}
	}
}

void SAmmoParams::LoadScaledEffect()
{
	const IItemParamsNode* scaledEffect = pItemParams->GetChild("scaledeffect");
	if (scaledEffect)
	{
		pScaledEffect = new SScaledEffectParams(scaledEffect);
		if (!pScaledEffect->ppname)
		{
			delete pScaledEffect;
			pScaledEffect = 0;
		}
	}
}

void SAmmoParams::LoadCollision()
{
	const IItemParamsNode* collision = pItemParams->GetChild("collision");
	if (collision)
	{
		pCollision = new SCollisionParams(collision);
		if (!pCollision->pParticleEffect && !pCollision->sound)
		{
			delete pCollision;
			pCollision = 0;
		}
	}
}

void SAmmoParams::LoadExplosion()
{
	const IItemParamsNode* explosion = pItemParams->GetChild("explosion");
	if (explosion)
		pExplosion = new SExplosionParams(explosion);
}

void SAmmoParams::LoadFlashbang()
{
	const IItemParamsNode* flashbang = pItemParams->GetChild("flashbang");
	if (flashbang)
		pFlashbang = new SFlashbangParams(flashbang);
}

void SAmmoParams::LoadTrailsAndWhizzes()
{
	const IItemParamsNode* whiz = pItemParams->GetChild("whiz");
	if (whiz)
	{
		pWhiz = new SWhizParams(whiz);
		if (!pWhiz->sound)
		{
			delete pWhiz;
			pWhiz = 0;
		}
	}
	
	const IItemParamsNode* ricochet = pItemParams->GetChild("ricochet");
	if (ricochet)
	{
		pRicochet = new SWhizParams(ricochet);
		if (!pRicochet->sound)
		{
			delete pRicochet;
			pRicochet = 0;
		}
	}

	const IItemParamsNode* trail = pItemParams->GetChild("trail");
	if (trail)
	{
		pTrail = new STrailParams(trail);
		if (!pTrail->sound && !pTrail->effect)
		{
			delete pTrail;
			pTrail = 0;
		}
	}

	const IItemParamsNode* trailUnderWater = pItemParams->GetChild("trailUnderWater");
	if (trailUnderWater)
	{
		pTrailUnderWater = new STrailParams(trailUnderWater);
		if (!pTrailUnderWater->sound && !pTrailUnderWater->effect)
		{
			delete pTrailUnderWater;
			pTrailUnderWater = 0;
		}
	}
}
