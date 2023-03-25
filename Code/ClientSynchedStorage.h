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
#ifndef __CLIENTSYNCHEDSTORAGE_H__
#define __CLIENTSYNCHEDSTORAGE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <NetHelpers.h>
#include "SynchedStorage.h"

#define DECLARE_GLOBAL_MESSAGE(classname) \
class classname: public CSetGlobalMsg \
	{ \
	public: \
	classname(int _channelId, CServerSynchedStorage *pStorage, TSynchedKey _key, TSynchedValue &_value); \
	EMessageSendResult WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq); \
	}; \

#define DECLARE_CHANNEL_MESSAGE(classname) \
class classname: public CSetChannelMsg \
	{ \
	public: \
	classname(int _channelId, CServerSynchedStorage *pStorage, TSynchedKey _key, TSynchedValue &_value); \
	EMessageSendResult WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq); \
	}; \


#define DECLARE_ENTITY_MESSAGE(classname) \
	class classname: public CSetEntityMsg \
	{ \
		public: \
		classname(int _channelId, CServerSynchedStorage *pStorage, EntityId id, TSynchedKey _key, TSynchedValue &_value); \
		EMessageSendResult WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq); \
	}; \


class CServerSynchedStorage;
class CClientSynchedStorage:
	public CNetMessageSinkHelper<CClientSynchedStorage, CSynchedStorage>
{
public:
	CClientSynchedStorage(IGameFramework *pGameFramework) { m_pGameFramework=pGameFramework; };
	virtual ~CClientSynchedStorage() {};

	void GetMemoryStatistics( ICrySizer * );

	// INetMessageSink
	virtual void DefineProtocol(IProtocolBuilder *pBuilder);
	// ~INetMessageSink

	//------------------------------------------------------------------------
	class CResetMsg: public INetMessage
	{
	public:
		CResetMsg(int _channelId, CServerSynchedStorage *pStorage);
		
		int											channelId;
		CServerSynchedStorage		*m_pStorage;

		EMessageSendResult WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq);
		void UpdateState(uint32 fromSeq, ENetSendableStateUpdate update);
		size_t GetSize();
	};

	//------------------------------------------------------------------------
	class CSetGlobalMsg: public INetMessage
	{
	public:
		CSetGlobalMsg(const SNetMessageDef *pDef, int _channelId, CServerSynchedStorage *pStorage, TSynchedKey _key, TSynchedValue &_value);

		int											channelId;
		CServerSynchedStorage		*m_pStorage;

		TSynchedKey							key;
		TSynchedValue						value;

		virtual EMessageSendResult WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq);
		virtual void UpdateState(uint32 fromSeq, ENetSendableStateUpdate update);
		virtual size_t GetSize();
	};

	DECLARE_GLOBAL_MESSAGE(CSetGlobalBoolMsg);
	DECLARE_GLOBAL_MESSAGE(CSetGlobalFloatMsg);
	DECLARE_GLOBAL_MESSAGE(CSetGlobalIntMsg);
	DECLARE_GLOBAL_MESSAGE(CSetGlobalEntityIdMsg);
	DECLARE_GLOBAL_MESSAGE(CSetGlobalStringMsg);

	//------------------------------------------------------------------------
	class CSetChannelMsg: public INetMessage
	{
	public:
		CSetChannelMsg(const SNetMessageDef *pDef, int _channelId, CServerSynchedStorage *pStorage, TSynchedKey _key, TSynchedValue &_value);

		int											channelId;
		CServerSynchedStorage		*m_pStorage;

		TSynchedKey							key;
		TSynchedValue						value;

		virtual EMessageSendResult WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq);
		virtual void UpdateState(uint32 fromSeq, ENetSendableStateUpdate update);
		virtual size_t GetSize();
	};

	DECLARE_CHANNEL_MESSAGE(CSetChannelBoolMsg);
	DECLARE_CHANNEL_MESSAGE(CSetChannelFloatMsg);
	DECLARE_CHANNEL_MESSAGE(CSetChannelIntMsg);
	DECLARE_CHANNEL_MESSAGE(CSetChannelEntityIdMsg);
	DECLARE_CHANNEL_MESSAGE(CSetChannelStringMsg);


	//------------------------------------------------------------------------
	class CSetEntityMsg: public INetMessage
	{
	public:
		CSetEntityMsg(const SNetMessageDef *pDef, int _channelId, CServerSynchedStorage *pStorage, EntityId id, TSynchedKey _key, TSynchedValue &_value);

		int											channelId;
		CServerSynchedStorage		*m_pStorage;

		EntityId								entityId;
		TSynchedKey							key;
		TSynchedValue						value;

		virtual EMessageSendResult WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq);
		virtual void UpdateState(uint32 fromSeq, ENetSendableStateUpdate update);
		virtual size_t GetSize();
	};

	DECLARE_ENTITY_MESSAGE(CSetEntityBoolMsg);
	DECLARE_ENTITY_MESSAGE(CSetEntityFloatMsg);
	DECLARE_ENTITY_MESSAGE(CSetEntityIntMsg);
	DECLARE_ENTITY_MESSAGE(CSetEntityEntityIdMsg);
	DECLARE_ENTITY_MESSAGE(CSetEntityStringMsg);

	//------------------------------------------------------------------------
	NET_DECLARE_IMMEDIATE_MESSAGE(ResetMsg);

	NET_DECLARE_IMMEDIATE_MESSAGE(SetGlobalBoolMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetGlobalFloatMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetGlobalIntMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetGlobalEntityIdMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetGlobalStringMsg);

	NET_DECLARE_IMMEDIATE_MESSAGE(SetChannelBoolMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetChannelFloatMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetChannelIntMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetChannelEntityIdMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetChannelStringMsg);

	NET_DECLARE_IMMEDIATE_MESSAGE(SetEntityBoolMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetEntityFloatMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetEntityIntMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetEntityEntityIdMsg);
	NET_DECLARE_IMMEDIATE_MESSAGE(SetEntityStringMsg);

protected:
	CCryMutex m_mutex;
};

#undef DECLARE_GLOBAL_MESSAGE
#undef DECLARE_ENTITY_MESSAGE

#endif //__CLIENTSYNCHEDSTORAGE_H__
