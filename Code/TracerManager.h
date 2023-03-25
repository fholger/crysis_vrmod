/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Tracer Manager

-------------------------------------------------------------------------
History:
- 17:1:2006   11:12 : Created by Márcio Martins

*************************************************************************/
#ifndef __TRACERMANAGER_H__
#define __TRACERMANAGER_H__

#if _MSC_VER > 1000
# pragma once
#endif


class CTracer
{
	friend class CTracerManager;
public:
	CTracer(const Vec3 &pos);
	virtual ~CTracer();

	void Reset(const Vec3 &pos);
	void CreateEntity();
	void SetGeometry(const char *name, float scale);
	void SetEffect(const char *name, float scale);
	void SetLifeTime(float lifeTime);
	bool Update(float frameTime, const Vec3 &campos);
	void UpdateVisual(const Vec3 &pos, const Vec3 &dir, float scale, float length);
	float GetAge() const;
	void GetMemoryStatistics(ICrySizer * s) const;

private:
	float				m_speed;
	Vec3				m_pos;
	Vec3				m_dest;
	Vec3				m_startingpos;
	float				m_age;
	float				m_lifeTime;
	bool        m_useGeometry;
	int         m_geometrySlot;

	EntityId		m_entityId;
};


class CTracerManager
{
	typedef std::vector<CTracer *>	TTracerPool;
	typedef std::vector<int>				TTracerIdVector;
public:
	CTracerManager();
	virtual ~CTracerManager();

	typedef struct STracerParams
	{
		const char *geometry;
		const char *effect;
		Vec3				position;
		Vec3				destination;
		float				speed;
		float				lifetime;
	};

	void EmitTracer(const STracerParams &params);
	void Update(float frameTime);
	void Reset();
	void GetMemoryStatistics(ICrySizer *);

private:
	TTracerPool			m_pool;
	TTracerIdVector m_updating;
	TTracerIdVector	m_actives;
	int							m_lastFree;
};


#endif //__TRACERMANAGER_H__