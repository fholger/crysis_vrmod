/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 12:10:2007   14:58 : Created by Márcio Martins

*************************************************************************/
#ifndef __SHOTVALIDATOR_H__
#define __SHOTVALIDATOR_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "IGameRulesSystem.h"


class CGameRules;

class CShotValidator
{
	typedef struct TShot
	{
		TShot(uint16 seqn, EntityId wpnId, CTimeValue t, uint8 lives): seq(seqn), weaponId(wpnId), time(t), life(lives) {};
		bool ILINE operator==(const TShot &that) const {
			return seq==that.seq && weaponId==that.weaponId;
		};
		bool ILINE operator!=(const TShot &that) const {
			return !(*this == that);
		};
		bool ILINE operator<(const TShot &that) const {
			return weaponId<that.weaponId || (weaponId==that.weaponId && seq<that.seq);
		};

		uint16			seq;
		EntityId		weaponId;
		
		CTimeValue	time;
		uint8				life;

	} TShot;

	typedef struct THit
	{
		THit(const HitInfo &hit, CTimeValue t): info(hit), time(t) {};

		CTimeValue	time;
		HitInfo			info;

	} THit;

	typedef std::set<TShot>																TShots;
	typedef std::multimap<TShot, THit, std::less<TShot> >	THits;

	typedef std::map<int, TShots>													TChannelShots;
	typedef std::map<int, THits>													TChannelHits;

	typedef std::map<int, uint16>													TChannelExpiredHits;

public:
	CShotValidator(CGameRules *pGameRules, IItemSystem *pItemSystem, IGameFramework *pGameFramework);
	~CShotValidator();

	void AddShot(EntityId playerId, EntityId weaponId, uint16 seq, uint8 seqr);
	bool ProcessHit(const HitInfo &hit);

	void Reset();
	void Update();

	void Connected(int channelId);
	void Disconnected(int channelId);

private:
	bool CanHit(const HitInfo &hit) const;
	bool Expired(const CTimeValue &now, const TShot &shot) const;
	bool Expired(const CTimeValue &now, const THit &hit) const;

	void DeclareExpired(int channelId, const HitInfo &hit);

	CGameRules					*m_pGameRules;
	IItemSystem					*m_pItemSystem;
	IGameFramework			*m_pGameFramework;

	TChannelShots				m_shots;
	TChannelHits				m_pendinghits;
	bool								m_doingHit;
	TChannelExpiredHits	m_expired;
};


#endif //__SHOTVALIDATOR_H__