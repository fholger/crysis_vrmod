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

#ifndef __BATTLEDUST_H__
#define __BATTLEDUST_H__

#pragma once

#include <list>
#include <IGameObject.h>

// possible events that might cause dust
enum EBattleDustEventType
{
	eBDET_ShotFired,
	eBDET_Explosion,
	eBDET_ShotImpact,
	eBDET_VehicleExplosion,
};

// a game object created when a particle effect is needed for an event
class CBattleEvent : public CGameObjectExtensionHelper<CBattleEvent, IGameObjectExtension>
{
public:
	friend class CBattleDust;

	CBattleEvent();
	virtual ~CBattleEvent();

	// IGameObjectExtension
	virtual bool Init(IGameObject *pGameObject);
	virtual void InitClient(int channelId) {};
	virtual void PostInit(IGameObject *pGameObject);
	virtual void PostInitClient(int channelId) {};
	virtual void Release();
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );
	virtual void PostSerialize() {}
	virtual void SerializeSpawnInfo( TSerialize ser ) {}
	virtual ISerializableInfoPtr GetSpawnInfo() {return 0;}
	virtual void Update( SEntityUpdateContext &ctx, int updateSlot);
	virtual void PostUpdate(float frameTime ) {};
	virtual void PostRemoteSpawn() {};
	virtual void HandleEvent( const SGameObjectEvent &) {};
	virtual void ProcessEvent(SEntityEvent &) {};
	virtual void SetChannelId(uint16 id) {};
	virtual void SetAuthority(bool auth) {};
	virtual void GetMemoryStatistics(ICrySizer * s);
	//~IGameObjectExtension

protected:

	static const int PROPERTIES_ASPECT = eEA_GameServerStatic;

	Vec3	m_worldPos;					// where in the world are we?
	float m_radius;						// how big now
	float m_peakRadius;				// how big it has been
	float m_lifetime;					// how long we will live (total)
	float m_lifeRemaining;		// how long before we are removed
	float m_numParticles;
	IParticleEffect* m_pParticleEffect;
	EntityId m_entityId;			// needed so we can find this event in the list after adding it.
};

// since weapon events have lifetime as well as power
struct SBattleEventParameter
{
	SBattleEventParameter() : m_name(""), m_power(0), m_lifetime(0), m_pClass(0) {}
	string m_name;
	float m_power;
	float m_lifetime;
	IEntityClass* m_pClass;		// to save strcmp all the time
};

// main class to manage where battle dust appears in the world
class CBattleDust
{
public:
	CBattleDust();
	~CBattleDust();

	void ReloadXml();
	void RecordEvent(EBattleDustEventType event, Vec3 worldPos, const IEntityClass* pClass);
	void Update();
	void NewBattleArea(CBattleEvent* pEvent);
	void RemoveBattleArea(CBattleEvent* pEvent);

	void Serialize(TSerialize ser);

protected:
	bool GetEventParams(EBattleDustEventType event, const IEntityClass* pClass, SBattleEventParameter& out);
	
	// if two areas overlap, make a big one instead
	bool CheckForMerging(CBattleEvent* pEvent);								
	bool CheckIntersection(CBattleEvent* pEventOne, Vec3& pos, float radius);
	bool MergeAreas(CBattleEvent* pExisting, Vec3& pos, float radius);

	void UpdateParticlesForArea(CBattleEvent* pEvent);

	void RemoveAllEvents();

	CBattleEvent* FindEvent(EntityId id);

	float m_entitySpawnPower;																	// how many events lead to an entity
	float m_defaultLifetime;																	// how long each event lasts (unless overridden)
	float m_maxLifetime;																			// max amount of time an event can last
	float m_maxEventPower;																		// stop expanding / merging events at this power
	float m_minParticleCount;																	// at m_entitySpawnPower there will be this many particles (linear int. between)
	float m_maxParticleCount;																	// at m_maxEventPower there will be this many particles (linear int. between)
	float m_distanceBetweenEvents;														// above this distance it's a seperate event

	SBattleEventParameter m_defaultWeapon;
	SBattleEventParameter m_defaultExplosion;
	SBattleEventParameter m_defaultVehicleExplosion;
	SBattleEventParameter m_defaultBulletImpact;

	std::list<EntityId> m_eventIdList;												// what has happened recently
	std::vector<SBattleEventParameter> m_weaponPower;					// what effect each shot has
	std::vector<SBattleEventParameter> m_explosionPower;			// what effect each explosion has
	std::vector<SBattleEventParameter> m_vehicleExplosionPower;// similar for vehicle explosions
	std::vector<SBattleEventParameter> m_bulletImpactPower;		// and for bullet impacts

	IEntityClass* m_pBattleEventClass;

	// for debugging: this is output to server's log file on exit.
	int m_maxBattleEvents;
};


#endif // __BATTLEDUST_H__