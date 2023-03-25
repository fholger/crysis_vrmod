/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: C4 Implementation

-------------------------------------------------------------------------
History:
- 2:3:2006   16:05 : Created by Márcio Martins

*************************************************************************/
#ifndef __C4_H__
#define __C4_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IItemSystem.h>
#include "Weapon.h"


class CC4 :
	public CWeapon
{
public:
	CC4();
	virtual ~CC4();

	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);

	virtual void PickUp(EntityId pickerId, bool sound, bool select, bool keepHistory);
	virtual bool CanSelect() const;
	virtual void Select(bool select);
	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CWeapon::GetMemoryStatistics(s); }
	virtual void Drop(float impulseScale, bool selectNext/* =true */, bool byDeath/* =false */);


	struct SetProjectileIdParams
	{
		SetProjectileIdParams(): id(0), fmId(0) {};
		SetProjectileIdParams(EntityId _id, int _fmId): id(_id), fmId(_fmId) {};

		EntityId	id;
		int				fmId;

		void SerializeWith(TSerialize ser)
		{
			ser.Value("fmId", fmId, 'fmod');
			ser.Value("projectileId", id, 'eid');
		}
	};

	struct RequestTimeParams
	{
		RequestTimeParams(): time(0.0f),  fmId(0) {};
		RequestTimeParams(float _time, int _fmId): time(_time), fmId(_fmId) {};

		float time;
		int		fmId;

		void SerializeWith(TSerialize ser)
		{
			ser.Value("fmId", fmId, 'fmod');
			ser.Value("time", time, 'fsec');
		}
	};

	DECLARE_CLIENT_RMI_NOATTACH(ClSetProjectileId, SetProjectileIdParams, eNRT_ReliableUnordered);
	DECLARE_SERVER_RMI_NOATTACH(SvRequestTime, RequestTimeParams, eNRT_ReliableUnordered);

private:

	static TActionHandler<CC4> s_actionHandler;

	bool OnActionSelectDetonator(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	void SelectDetonator();
};

#endif // __C4_H__