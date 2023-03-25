/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 5:7:2006   16:00 : Created by Márcio Martins

*************************************************************************/
#ifndef __SERVERSYNCHEDSTORAGE_H__
#define __SERVERSYNCHEDSTORAGE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <NetHelpers.h>
#include "SynchedStorage.h"
#include "ClientSynchedStorage.h"

#include <deque>


class CServerSynchedStorage:
	public CNetMessageSinkHelper<CServerSynchedStorage, CSynchedStorage>
{
public:
	CServerSynchedStorage(IGameFramework *pGameFramework) { m_pGameFramework=pGameFramework; };
	virtual ~CServerSynchedStorage() {};

	void GetMemoryStatistics( ICrySizer * );

	// INetMessageSink
	virtual void DefineProtocol(IProtocolBuilder *pBuilder);
	// ~INetMessageSink

	virtual void Reset();
	virtual void ResetChannel(int channelId);

	virtual bool OnSetGlobalMsgComplete(CClientSynchedStorage::CSetGlobalMsg *pMsg, int channelId, uint32 fromSeq, bool ack);
	virtual bool OnSetChannelMsgComplete(CClientSynchedStorage::CSetChannelMsg *pMsg, int channelId, uint32 fromSeq, bool ack);
	virtual bool OnSetEntityMsgComplete(CClientSynchedStorage::CSetEntityMsg *pMsg, int channelId, uint32 fromSeq, bool ack);

	// these should only be called from the main thread
	virtual void AddToGlobalQueue(TSynchedKey key);
	virtual void AddToEntityQueue(EntityId entityId, TSynchedKey key);
	virtual void AddToChannelQueue(int channelId, TSynchedKey key);

	virtual void AddToGlobalQueueFor(int channelId, TSynchedKey key);
	virtual void AddToEntityQueueFor(int channelId, EntityId entityId, TSynchedKey key);
	
	virtual void FullSynch(int channelId, bool reset);

	virtual void OnClientConnect(int channelId);
	virtual void OnClientDisconnect(int channelId, bool onhold);
	virtual void OnClientEnteredGame(int channelId);

	virtual void OnGlobalChanged(TSynchedKey key, const TSynchedValue &value);
	virtual void OnChannelChanged(int channelId, TSynchedKey key, const TSynchedValue &value);
	virtual void OnEntityChanged(EntityId entityId, TSynchedKey key, const TSynchedValue &value);

	struct SChannel
	{
		SChannel()
		: local(false), pNetChannel(0), onhold(false) {};
		SChannel(INetChannel *_pNetChannel, bool isLocal)
		: local(isLocal), pNetChannel(_pNetChannel), onhold(false) {};
		INetChannel *pNetChannel;
		SSendableHandle     lastOrderedMessage;
		bool				local:1;
		bool				onhold:1;
	};

	SChannel *GetChannel(int channelId);
	SChannel *GetChannel(INetChannel *pNetChannel);
	int GetChannelId(INetChannel *pNetChannel) const;

protected:
	struct SChannelQueueEnt
	{
		SChannelQueueEnt() {}
		SChannelQueueEnt(int c, TSynchedKey k) : channel(c), key(k) {}
		int channel;
		TSynchedKey key;

		bool operator<( const SChannelQueueEnt& rhs ) const
		{
			return rhs.channel < channel || (rhs.channel == channel && rhs.key < key);
		}
	};

	struct SChannelEntityQueueEnt
	{
		SChannelEntityQueueEnt() {}
		SChannelEntityQueueEnt(int c, EntityId e, TSynchedKey k) : channel(c), entity(e), key(k) {}
		int channel;
		EntityId entity;
		TSynchedKey key;

		bool operator<( const SChannelEntityQueueEnt& rhs ) const
		{
			if (channel < rhs.channel)
				return true;
			else if (channel > rhs.channel)
				return false;
			else if (key < rhs.key)
				return true;
			else if (key > rhs.key)
				return false;
			else if (entity < rhs.entity)
				return true;
			else if (entity > rhs.entity)
				return false;
			return false;
		}
	};

	typedef std::map<SChannelQueueEnt, SSendableHandle>								TChannelQueueMap;
	typedef std::map<SChannelEntityQueueEnt, SSendableHandle>					TChannelEntityQueueMap;
	typedef std::map<int, SChannel>																		TChannelMap;

	TChannelQueueMap				m_globalQueue;
	TChannelEntityQueueMap	m_entityQueue;
	TChannelQueueMap				m_channelQueue;

	TChannelMap							m_channels;

	CCryMutex								m_mutex;
};

#endif //__SERVERSYNCHEDSTORAGE_H__
