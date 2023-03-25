/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Battle dust system (aka Fog of War)

-------------------------------------------------------------------------
History:
- 01:03:2007: Created by Steve Humphreys

*************************************************************************/

#include "StdAfx.h"

#include "BattleDust.h"

#include "Game.h"
#include "GameCVars.h"
#include "GameRules.h"
#include "IRenderAuxGeom.h"
#include "IEntitySystem.h"

//////////////////////////////////////////////////////////////////////////
//	CBattleEvent
//////////////////////////////////////////////////////////////////////////

CBattleEvent::CBattleEvent()
: m_worldPos(Vec3(0,0,0))
, m_radius(0)
, m_peakRadius(0)
, m_lifetime(1)
, m_lifeRemaining(1)
, m_numParticles(0)
, m_pParticleEffect(NULL)
, m_entityId(0)
{
}

CBattleEvent::~CBattleEvent()
{
}

bool CBattleEvent::Init(IGameObject *pGameObject)
{
	SetGameObject(pGameObject);

	if(!GetGameObject()->BindToNetwork())
		return false;

	if(GetEntity())
		m_entityId = GetEntity()->GetId();

	if(gEnv->bServer && g_pGame->GetGameRules())
	{
		CBattleDust* pBD = g_pGame->GetGameRules()->GetBattleDust();
		if(pBD)
			pBD->NewBattleArea(this);
	}

	return true;
}
//------------------------------------------------------------------------
void CBattleEvent::PostInit(IGameObject *pGameObject)
{
	GetGameObject()->EnableUpdateSlot(this, 0);
}

//------------------------------------------------------------------------
void CBattleEvent::Release()
{
	// once this object is released it must also be removed from the list in CBattleDust
	if(gEnv->bServer && g_pGame && g_pGame->GetGameRules())
	{
		CBattleDust* pBD = g_pGame->GetGameRules()->GetBattleDust();
		if(pBD)
			pBD->RemoveBattleArea(this);		
	}
	delete this;
}

//------------------------------------------------------------------------
void CBattleEvent::FullSerialize(TSerialize ser)
{
	ser.BeginGroup("BattleEvent");
	ser.Value("worldPos", m_worldPos);
	ser.Value("numParticles", m_numParticles);
	ser.Value("radius", m_radius);
	ser.Value("m_peakRadius", m_peakRadius);
	ser.Value("m_lifetime", m_lifetime);
	ser.Value("m_lifeRemaining", m_lifeRemaining);
	ser.EndGroup();
	if(ser.IsReading())
	{
		m_pParticleEffect = NULL;
		m_entityId = (GetEntity() != NULL) ? GetEntity()->GetId() : 0;
	}
}

bool CBattleEvent::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if (aspect == PROPERTIES_ASPECT)
	{
		ser.Value("worldPos", m_worldPos, 'wrld');
		ser.Value("numParticles", m_numParticles, 'iii');
	}
	return true;
}

void CBattleEvent::Update(SEntityUpdateContext &ctx, int updateSlot)
{
	IEntity* pEntity = GetEntity();
	if(pEntity)
	{
		Matrix34 tm = pEntity->GetWorldTM();
		tm.SetTranslation(m_worldPos);
		pEntity->SetWorldTM(tm);

		if(m_numParticles > 0 && !m_pParticleEffect)
		{
			// attach the particle effect to this entity now
			m_pParticleEffect = gEnv->p3DEngine->FindParticleEffect(g_pGameCVars->g_battleDust_effect->GetString());
			if (m_pParticleEffect)
			{
				pEntity->LoadParticleEmitter(0, m_pParticleEffect, 0, true, true);
				Matrix34 tm = IParticleEffect::ParticleLoc(Vec3(0,0,0));
				pEntity->SetSlotLocalTM(0, tm);
			}
		}

		if(m_pParticleEffect)
		{
			SEntitySlotInfo info;
			pEntity->GetSlotInfo(0, info);
			if(info.pParticleEmitter)
			{
				SpawnParams sp;
				sp.fCountScale = (float)m_numParticles/60.0f;
				info.pParticleEmitter->SetSpawnParams(sp);
			}
		}

		if(g_pGameCVars->g_battleDust_debug != 0)
		{
			if(g_pGameCVars->g_battleDust_debug >= 2)
			{
				if(m_numParticles > 0)
				{
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_worldPos, m_numParticles, ColorF(0.0f,1.0f,0.0f,0.2f));
				}
				else
				{
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_worldPos, 0.5f, ColorF(1.0f,0.0f,0.0f,0.2f));
				}
			}
			else
			{
				if(m_numParticles > 0)
				{
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_worldPos, 0.5f, ColorF(0.0f,1.0f,0.0f,0.2f));
				}
				else
				{
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_worldPos, 0.5f, ColorF(1.0f,0.0f,0.0f,0.2f));
				}
			}
		}
	}
}

//-------------------------------------------------------------------------

void CBattleEvent::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
}


//////////////////////////////////////////////////////////////////////////
//	CBattleDust
//////////////////////////////////////////////////////////////////////////

CBattleDust::CBattleDust()
{
	m_entitySpawnPower = 0.0f;
	m_defaultLifetime = 0.0f;
	m_maxLifetime = 0.0f;
	m_maxEventPower = 0.0f;
	m_pBattleEventClass = NULL;
	m_minParticleCount = 0;
	m_maxParticleCount = 0;
	m_distanceBetweenEvents = 0;

	m_maxBattleEvents = 0;

	// load xml file and process it

	if(!gEnv->bServer)
		return;

	ReloadXml();
}

CBattleDust::~CBattleDust()
{
}

void CBattleDust::ReloadXml()
{
	// first empty out the previous stuff
	m_weaponPower.clear();
	m_explosionPower.clear();
	m_vehicleExplosionPower.clear();
	m_bulletImpactPower.clear();

	IXmlParser*	pxml = g_pGame->GetIGameFramework()->GetISystem()->GetXmlUtils()->CreateXmlParser();
	if(!pxml)
		return;

	XmlNodeRef node = GetISystem()->LoadXmlFile("Game/Scripts/GameRules/BattleDust.xml");
	if(!node)
		return;

	XmlNodeRef paramsNode = node->findChild("params");
	if(paramsNode)
	{
		paramsNode->getAttr("fogspawnpower", m_entitySpawnPower);
		paramsNode->getAttr("defaultlifetime", m_defaultLifetime);
		paramsNode->getAttr("maxlifetime", m_maxLifetime);
		paramsNode->getAttr("maxeventpower", m_maxEventPower);
		paramsNode->getAttr("minparticlecount", m_minParticleCount);
		paramsNode->getAttr("maxparticlecount", m_maxParticleCount);
		paramsNode->getAttr("distancebetweenevents", m_distanceBetweenEvents);
	}

	XmlNodeRef eventsNode = node->findChild("events");
	if(eventsNode)
	{
		// corresponds to the eBDET_ShotFired event
		XmlNodeRef shotNode = eventsNode->findChild("shotfired");
		if(shotNode)
		{
			for (int i = 0; i < shotNode->getChildCount(); ++i)
			{
				XmlNodeRef weaponNode = shotNode->getChild(i);
				if(weaponNode)
				{
					XmlString name;
					float power = 1.0f;
					float lifetime = 1.0f;
					weaponNode->getAttr("name", name);
					weaponNode->getAttr("power", power);
					weaponNode->getAttr("lifetime", lifetime);

					if(!strcmp(name, "default"))
					{
						m_defaultWeapon.m_power = power;
						m_defaultWeapon.m_lifetime = lifetime;
					}
					else
					{
						SBattleEventParameter param;
						param.m_name = name;
						param.m_pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
						param.m_power = power;
						param.m_lifetime = lifetime;
						m_weaponPower.push_back(param);
					}
				}
			}
		}

		XmlNodeRef explodeNode = eventsNode->findChild("explosion");
		if(explodeNode)
		{
			for(int i=0; i < explodeNode->getChildCount(); ++i)
			{
				XmlNodeRef explosiveNode = explodeNode->getChild(i);
				if(explosiveNode)
				{
					XmlString name;
					float power = 1.0f;
					float lifetime = 1.0f;
					explosiveNode->getAttr("name", name);
					explosiveNode->getAttr("power", power);
					explosiveNode->getAttr("lifetime", lifetime);

					if(!strcmp(name, "default"))
					{
						m_defaultExplosion.m_power = power;
						m_defaultExplosion.m_lifetime = lifetime;
					}
					else
					{
						SBattleEventParameter param;
						param.m_name = name;
						param.m_pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
						param.m_power = power;
						param.m_lifetime = lifetime;
						m_explosionPower.push_back(param);
					}
				}
			}
		}

		XmlNodeRef vehicleExplodeNode = eventsNode->findChild("vehicleexplosion");
		if(vehicleExplodeNode)
		{
			for(int i=0; i < vehicleExplodeNode->getChildCount(); ++i)
			{
				XmlNodeRef vehicleNode = vehicleExplodeNode->getChild(i);
				if(vehicleNode)
				{
					XmlString name;
					float power = 1.0f;
					float lifetime = 1.0f;
					vehicleNode->getAttr("name", name);
					vehicleNode->getAttr("power", power);
					vehicleNode->getAttr("lifetime", lifetime);

					if(!strcmp(name, "default"))
					{
						m_defaultVehicleExplosion.m_power = power;
						m_defaultVehicleExplosion.m_lifetime = lifetime;
					}
					else
					{
						SBattleEventParameter param;
						param.m_name = name;
						param.m_pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
						param.m_power = power;
						param.m_lifetime = lifetime;
						m_vehicleExplosionPower.push_back(param);
					}
				}
			}
		}

		XmlNodeRef impactNode = eventsNode->findChild("bulletimpact");
		if(impactNode)
		{
			for(int i=0; i < impactNode->getChildCount(); ++i)
			{
				XmlNodeRef impact = impactNode->getChild(i);
				if(impact)
				{
					XmlString name;
					float power = 1.0f;
					float lifetime = 1.0f;
					impact->getAttr("name", name);
					impact->getAttr("power", power);
					impact->getAttr("lifetime", lifetime);

					if(!strcmp(name, "default"))
					{
						m_defaultBulletImpact.m_power = power;
						m_defaultBulletImpact.m_lifetime = lifetime;
					}
					else
					{
						SBattleEventParameter param;
						param.m_name = name;
						param.m_pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
						param.m_lifetime = lifetime;
						param.m_power = power;
						m_bulletImpactPower.push_back(param);
					}
				}
			}
		}
	}
}

void CBattleDust::RecordEvent(EBattleDustEventType event, Vec3 worldPos, const IEntityClass* pClass)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if(!g_pGameCVars->g_battleDust_enable)
		return;

	if(!gEnv->bServer)
		return;

	// this typically means the xml file failed to load. Turn off the dust.
	if(m_maxParticleCount == 0)
		return;

	SBattleEventParameter param;
	if(!GetEventParams(event, pClass, param))
		return;
	
	if(param.m_power == 0 || worldPos.IsEquivalent(Vec3(0,0,0)))
		return;

	if(m_pBattleEventClass == NULL)
		m_pBattleEventClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( "BattleEvent" );

	// first check if we need a new event
	bool newEvent = true;
	for(std::list<EntityId>::iterator it = m_eventIdList.begin(); it != m_eventIdList.end(); ++it)
	{
		EntityId areaId = (*it);
		CBattleEvent *pBattleArea = FindEvent(areaId);
		if(pBattleArea && CheckIntersection(pBattleArea, worldPos, param.m_power))
		{
			// don't need a new event as this one is within an existing one. Just merge them.
			MergeAreas(pBattleArea, worldPos, param.m_power);
			pBattleArea->m_lifeRemaining += param.m_lifetime;
			pBattleArea->m_lifetime = pBattleArea->m_lifeRemaining;
			pBattleArea->m_lifetime = CLAMP(pBattleArea->m_lifetime, 0.0f, m_maxLifetime);
			pBattleArea->m_lifeRemaining = CLAMP(pBattleArea->m_lifeRemaining, 0.0f, m_maxLifetime);
			newEvent = false;			
			break;
		}
	}
 
	if(newEvent)
	{
		IEntitySystem * pEntitySystem = gEnv->pEntitySystem;
 		SEntitySpawnParams esp;
 		esp.id = 0;
 		esp.nFlags = 0;
 		esp.pClass = m_pBattleEventClass;
 		if (!esp.pClass)
 			return;
 		esp.pUserData = NULL;
 		esp.sName = "BattleDust";
 		esp.vPosition	= worldPos;
	
		// when CBattleEvent is created it will add itself to the list
		IEntity * pEntity = pEntitySystem->SpawnEntity( esp );
		if(pEntity)
		{
			// find the just-added entity in the list, and set it's properties
			IGameObject* pGO = g_pGame->GetIGameFramework()->GetGameObject(pEntity->GetId());
			if(pGO)
			{
				CBattleEvent* pNewEvent = static_cast<CBattleEvent*>(pGO->QueryExtension("BattleEvent"));
				if(pNewEvent)
				{
					pNewEvent->m_radius = param.m_power;
					pNewEvent->m_peakRadius = param.m_power;
					pNewEvent->m_lifetime = param.m_lifetime;
					pNewEvent->m_lifeRemaining = param.m_lifetime;
					pNewEvent->m_worldPos = worldPos;

					pNewEvent->m_lifetime = CLAMP(pNewEvent->m_lifetime, 0.0f, m_maxLifetime);
					pNewEvent->m_lifeRemaining = CLAMP(pNewEvent->m_lifeRemaining, 0.0f, m_maxLifetime);

					pGO->ChangedNetworkState(CBattleEvent::PROPERTIES_ASPECT);
				}
			}
		}
	}
}

void CBattleDust::NewBattleArea(CBattleEvent* pEvent)
{
	if(!g_pGameCVars->g_battleDust_enable)
		return;

	int num = 0;
	if (pEvent)
 	{
		m_eventIdList.push_back(pEvent->GetEntityId());
		num = m_eventIdList.size();
	}
}

void CBattleDust::RemoveBattleArea(CBattleEvent* pEvent)
{
	int numRemain = 0;
	if(pEvent)
	{
		stl::find_and_erase(m_eventIdList, pEvent->GetEntityId());

		numRemain = m_eventIdList.size();
	}
}

void CBattleDust::Update()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if(!g_pGameCVars->g_battleDust_enable)
	{
		RemoveAllEvents();
		return;
	}

	if(!gEnv->bServer)
		return;

	if(g_pGameCVars->g_battleDust_debug != 0)
	{
		float col[] = {1,1,1,1};
		gEnv->pRenderer->Draw2dLabel(50, 40, 2.0f, col, false, "Num BD areas: %d (max %d)", m_eventIdList.size(), m_maxBattleEvents);
	}
	float ypos = 60.0f;

	// go through the list of areas, remove any which are too small
	m_maxBattleEvents = MAX(m_maxBattleEvents, m_eventIdList.size());
	std::list<EntityId>::iterator next;
	std::list<EntityId>::iterator it = m_eventIdList.begin();
	for(; it != m_eventIdList.end(); it = next)
	{
		next = it; ++next;
		EntityId areaId = (*it);
		CBattleEvent *pBattleArea = FindEvent(areaId);
		if(!pBattleArea)
			continue;

		if(pBattleArea->m_lifetime > 0.0f)
		{
			pBattleArea->m_lifeRemaining -= gEnv->pTimer->GetFrameTime();
			pBattleArea->m_radius = pBattleArea->m_peakRadius * (pBattleArea->m_lifeRemaining / pBattleArea->m_lifetime);
		}

		if(g_pGameCVars->g_battleDust_debug != 0)
		{
			float col[] = {1,1,1,1};
			gEnv->pRenderer->Draw2dLabel(50, ypos, 1.4f, col, false, "Area: (%.2f, %.2f, %.2f), Radius: %.2f/%.2f, Particles: %.0f, Lifetime: %.2f/%.2f", pBattleArea->m_worldPos.x, pBattleArea->m_worldPos.y, pBattleArea->m_worldPos.z, pBattleArea->m_radius, pBattleArea->m_peakRadius, pBattleArea->m_numParticles, pBattleArea->m_lifeRemaining, pBattleArea->m_lifetime);
			ypos += 10.0f;
		}

		if(pBattleArea->m_lifeRemaining < 0.0f)
		{
			// remove it (NB this will also call RemoveBattleArea(), which will remove it from the list)
			gEnv->pEntitySystem->RemoveEntity(areaId);
		}
		else
		{
			if(pBattleArea->GetEntity())
			{
				UpdateParticlesForArea(pBattleArea);
			}
		}
	}
}

void CBattleDust::RemoveAllEvents()
{
	// go through the list and remove all entities (eg if user switches off battledust)
	std::list<EntityId>::iterator next;
	std::list<EntityId>::iterator it = m_eventIdList.begin();
	for(; it != m_eventIdList.end(); it = next)
	{
		next = it; ++next;
		EntityId areaId = (*it);

		// remove it (NB this will also call RemoveBattleArea(), which will remove it from the list)
		gEnv->pEntitySystem->RemoveEntity(areaId);
	}
}

bool CBattleDust::GetEventParams(EBattleDustEventType event, const IEntityClass* pClass, SBattleEventParameter& out)
{
	if(!g_pGameCVars->g_battleDust_enable)
		return false;

	switch(event)
	{
		case eBDET_ShotFired:
			out = m_defaultWeapon;
			if(pClass != NULL)
			{
				// find weapon name in list.
				for(int i=0; i<m_weaponPower.size(); ++i)
				{
					if(pClass == m_weaponPower[i].m_pClass)
					{
						out = m_weaponPower[i];
						return true;
					}
				}
			}
			break;

		case eBDET_Explosion:
			out = m_defaultExplosion;
			if(pClass != NULL)
			{
				for(int i=0; i<m_explosionPower.size(); ++i)
				{
					if(pClass == m_explosionPower[i].m_pClass)
					{
						out = m_explosionPower[i];
						return true;
					}
				}
			}
			break;

		case eBDET_VehicleExplosion:
			out = m_defaultVehicleExplosion;
			if(pClass != NULL)
			{
				for(int i=0; i<m_vehicleExplosionPower.size(); ++i)
				{
					if(pClass == m_vehicleExplosionPower[i].m_pClass)
					{
						out = m_vehicleExplosionPower[i];
						return true;
					}
				}
			}
			break;

		case eBDET_ShotImpact:
			out = m_defaultBulletImpact;
			if(pClass != NULL)
			{
				for(int i=0; i<m_bulletImpactPower.size(); ++i)
				{
					if(pClass == m_bulletImpactPower[i].m_pClass)
					{
						out = m_bulletImpactPower[i];
						return true;
					}
				}
			}
			break;

		default:
			break;
	}

	return (out.m_power != 0);
}

bool CBattleDust::CheckForMerging(CBattleEvent* pEvent)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if(!g_pGameCVars->g_battleDust_enable)
		return false;

	if(!pEvent)
		return false;

	if(!gEnv->bServer)
		return false;

	// check if area can merge with nearby areas
	for(std::list<EntityId>::iterator it = m_eventIdList.begin(); it != m_eventIdList.end(); ++it)
	{
		EntityId areaId = (*it);
		CBattleEvent *pBattleArea = FindEvent(areaId);
		if(!pBattleArea)
			continue;

		if(CheckIntersection(pEvent, pBattleArea->m_worldPos, pBattleArea->m_radius) && pBattleArea->m_radius > 0 && (pBattleArea != pEvent) && pBattleArea->GetEntity())
		{
			MergeAreas(pBattleArea, pEvent->m_worldPos, pEvent->m_radius);
			return true;
		}
	}

	return false;
}

bool CBattleDust::MergeAreas(CBattleEvent* pExisting, Vec3& pos, float radius)
{
	if(!pExisting)
		return false;

	// increase the size of existing area to take into account toAdd which overlaps.
	// NB we don't need the new volume to completely enclose both starting volumes,
	//	so for now:
	//	- new centre pos is the average position, weighted by initial radius
	//	- new radius is total of the two starting radii

	float totalRadii = pExisting->m_radius + radius;
	float oldFraction = pExisting->m_radius / totalRadii;
	float newFraction = radius / totalRadii;

	pExisting->m_worldPos = (oldFraction * pExisting->m_worldPos) + (newFraction * pos);
	pExisting->m_radius = CLAMP(totalRadii, 0.0f, m_maxEventPower);
	pExisting->m_peakRadius = pExisting->m_radius;

	// position has moved, so need to serialize
	if(pExisting->GetGameObject())
	{
		pExisting->GetGameObject()->ChangedNetworkState(CBattleEvent::PROPERTIES_ASPECT);
	}

	return true;
}

void CBattleDust::UpdateParticlesForArea(CBattleEvent* pEvent)
{
	float oldParticleCount = pEvent->m_numParticles;
	float fraction = CLAMP((pEvent->m_radius - m_entitySpawnPower) / (m_maxEventPower - m_entitySpawnPower), 0.0f, 1.0f);
	pEvent->m_numParticles = LERP(m_minParticleCount, m_maxParticleCount, fraction);

	if(pEvent->GetGameObject() && oldParticleCount != pEvent->m_numParticles)
	{
		pEvent->GetGameObject()->ChangedNetworkState(CBattleEvent::PROPERTIES_ASPECT);
	}
}

bool CBattleDust::CheckIntersection(CBattleEvent* pEventOne, Vec3& pos, float radius)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if(!pEventOne)
		return false;

	Vec3 centreToCentre = pos - pEventOne->m_worldPos;
	float sumRadiiSquared = (radius * radius) + (pEventOne->m_radius * pEventOne->m_radius);
	float distanceSquared = centreToCentre.GetLengthSquared();

	return ((distanceSquared < sumRadiiSquared) && (distanceSquared < m_distanceBetweenEvents*m_distanceBetweenEvents));
}

void CBattleDust::Serialize(TSerialize ser)
{
	if(ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("BattleDust");
		int amount = m_eventIdList.size();
		ser.Value("AmountOfBattleEvents", amount);
		std::list<EntityId>::iterator begin = m_eventIdList.begin();
		std::list<EntityId>::iterator end = m_eventIdList.end();

		if(ser.IsReading())
		{
			m_eventIdList.clear();
			for(int i = 0; i < amount; ++i)
			{
				EntityId id = 0;
				ser.BeginGroup("BattleEventId");
				ser.Value("BattleEventId", id);
				ser.EndGroup();
				m_eventIdList.push_back(id);
			}
		}
		else
		{
			for(; begin != end; ++begin)
			{
				EntityId id = (*begin);
				ser.BeginGroup("BattleEventId");
				ser.Value("BattleEventId", id);
				ser.EndGroup();
			}
		}

		ser.EndGroup();
	}
}

CBattleEvent* CBattleDust::FindEvent(EntityId id)
{		
	IGameObject *pBattleEventGameObject = g_pGame->GetIGameFramework()->GetGameObject(id);
	if(pBattleEventGameObject)
	{
		return static_cast<CBattleEvent*>(pBattleEventGameObject->QueryExtension("BattleEvent"));
	}

	return NULL;
}