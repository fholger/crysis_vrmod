/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Item Scheduler

-------------------------------------------------------------------------
History:
- 1:9:2005   11:33 : Created by Márcio Martins

*************************************************************************/
#ifndef __ITEMSCHEDULER_H__
#define __ITEMSCHEDULER_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <vector>
#include <ITimer.h>
#include "PoolAllocator.h"


class CItem;

// never inherit directly from ISchedulerAction, instead write a class that looks like:
// class Blah { void execute(CItem *) { ... } };
// and use a pAction = CSchedulerAction<Blah>::Create(Blah(...));
struct ISchedulerAction
{
	virtual void execute(CItem *_this) = 0;
	virtual void destroy() = 0;
	virtual void GetMemoryStatistics(ICrySizer * s) = 0;
};

template <class T>
class CSchedulerAction : public ISchedulerAction
{
	// second sizeof(void*) is just for safety!
	typedef stl::PoolAllocator<sizeof(T) + sizeof(void*) + sizeof(void*), stl::PoolAllocatorSynchronizationSinglethreaded> Alloc;
	static Alloc m_alloc;

public:
	static CSchedulerAction * Create()
	{
		return new (m_alloc.Allocate()) CSchedulerAction();
	}
	static CSchedulerAction * Create( const T& from )
	{
		return new (m_alloc.Allocate()) CSchedulerAction(from);
	}

	void execute(CItem * _this) { m_impl.execute(_this); }
	void destroy() 
	{ 
		this->~CSchedulerAction();
		m_alloc.Deallocate(this);
	}
	void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); }

private:
	T m_impl;

	CSchedulerAction() {}
	CSchedulerAction( const T& from ) : m_impl(from) {}
	~CSchedulerAction() {}
};

template <class T>
typename CSchedulerAction<T>::Alloc CSchedulerAction<T>::m_alloc;


class CItemScheduler
{
	typedef struct SScheduledAction
	{
		ISchedulerAction	*action;
		bool							persist;
	};
	typedef struct STimerAction
	{
		ISchedulerAction	*action;
		float							time;
		bool							persist;
	};

	typedef std::vector<STimerAction>									TTimerActionVector;
	typedef std::vector<SScheduledAction>							TScheduledActionVector;

	struct compare_timers
	{
		bool operator() (const STimerAction &lhs, const STimerAction &rhs ) const
		{
			return lhs.time < rhs.time;
		}
	};

public:
	CItemScheduler(CItem *item);
	virtual ~CItemScheduler();
	void Reset(bool keepPersistent=false);
	void Update(float frameTime);
	void TimerAction(uint time, ISchedulerAction *action, bool persistent=false);
	void ScheduleAction(ISchedulerAction *action, bool persistent=false);
	void GetMemoryStatistics(ICrySizer * s);

	bool ILINE IsBusy() const { return m_busy; };
	void SetBusy(bool busy);

	void Lock(bool lock);
	bool IsLocked();

private:
	bool				m_locked;
	bool				m_busy;
	ITimer			*m_pTimer;
	CItem				*m_pItem;

	TTimerActionVector				m_timers;
	TTimerActionVector				m_actives;
	TScheduledActionVector		m_schedule;
};



#endif //__ITEMSCHEDULER_H__