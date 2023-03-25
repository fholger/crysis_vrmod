/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 5:7:2006   16:01 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ClientSynchedStorage.h"
#include "ServerSynchedStorage.h"


//------------------------------------------------------------------------
void CClientSynchedStorage::DefineProtocol(IProtocolBuilder *pBuilder)
{
	pBuilder->AddMessageSink(this, CServerSynchedStorage::GetProtocolDef(), CClientSynchedStorage::GetProtocolDef());
}

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, ResetMsg, eNRT_ReliableOrdered, eMPF_BlocksStateChange)
{
	CCryMutex::CLock lock(m_mutex);

	Reset();

	return true;
}

//------------------------------------------------------------------------
CClientSynchedStorage::CResetMsg::CResetMsg(int _channelId, CServerSynchedStorage *pStorage)
: INetMessage(CClientSynchedStorage::ResetMsg),
	channelId(_channelId),
	m_pStorage(pStorage)
{
};

//------------------------------------------------------------------------
EMessageSendResult CClientSynchedStorage::CResetMsg::WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq)
{
	return eMSR_SentOk;
}

//------------------------------------------------------------------------
void CClientSynchedStorage::CResetMsg::UpdateState(uint32 fromSeq, ENetSendableStateUpdate)
{
}

//------------------------------------------------------------------------
size_t CClientSynchedStorage::CResetMsg::GetSize()
{
	return sizeof(this);
};


//------------------------------------------------------------------------
// GLOBAL
//------------------------------------------------------------------------

#define DEFINE_GLOBAL_MESSAGE(class, type, msgdef) \
	CClientSynchedStorage::class::class(int _channelId, CServerSynchedStorage *pStorage, TSynchedKey _key, TSynchedValue &_value) \
	:	CSetGlobalMsg(CClientSynchedStorage::msgdef, _channelId, pStorage, _key, _value) {}; \
	EMessageSendResult CClientSynchedStorage::class::WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq) \
{ m_pStorage->SerializeValue(ser, key, value, NTypelist::IndexOf<type, TSynchedValueTypes>::value); \
	return eMSR_SentOk; }

#define IMPLEMENT_IMMEDIATE_GLOBAL_MESSAGE(type) \
	CCryMutex::CLock lock(m_mutex); \
	TSynchedKey		key; \
	TSynchedValue value; \
	SerializeValue(ser, key, value, NTypelist::IndexOf<type, TSynchedValueTypes>::value); \
	return true;


NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetGlobalBoolMsg, eNRT_ReliableUnordered, 0)
{
	IMPLEMENT_IMMEDIATE_GLOBAL_MESSAGE(bool);
}

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetGlobalFloatMsg, eNRT_ReliableUnordered, 0)
{
	IMPLEMENT_IMMEDIATE_GLOBAL_MESSAGE(float);
}

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetGlobalIntMsg, eNRT_ReliableUnordered, 0)
{
	IMPLEMENT_IMMEDIATE_GLOBAL_MESSAGE(int);
}

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetGlobalEntityIdMsg, eNRT_ReliableUnordered, 0)
{
	IMPLEMENT_IMMEDIATE_GLOBAL_MESSAGE(EntityId);
}

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetGlobalStringMsg, eNRT_ReliableUnordered, 0)
{
	IMPLEMENT_IMMEDIATE_GLOBAL_MESSAGE(string);
}

//------------------------------------------------------------------------
CClientSynchedStorage::CSetGlobalMsg::CSetGlobalMsg(const SNetMessageDef *pDef, int _channelId, CServerSynchedStorage *pStorage, TSynchedKey _key, TSynchedValue &_value)
:	channelId(_channelId),
	m_pStorage(pStorage),
	key(_key),
	value(_value),
	INetMessage(pDef)
{
	SetGroup( 'stor' );
};

//------------------------------------------------------------------------
EMessageSendResult CClientSynchedStorage::CSetGlobalMsg::WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq)
{
	return eMSR_SentOk;
}

//------------------------------------------------------------------------
void CClientSynchedStorage::CSetGlobalMsg::UpdateState(uint32 fromSeq, ENetSendableStateUpdate)
{
}

//------------------------------------------------------------------------
size_t CClientSynchedStorage::CSetGlobalMsg::GetSize()
{
	return sizeof(this);
};

DEFINE_GLOBAL_MESSAGE(CSetGlobalBoolMsg, bool, SetGlobalBoolMsg);
DEFINE_GLOBAL_MESSAGE(CSetGlobalFloatMsg, float, SetGlobalFloatMsg);
DEFINE_GLOBAL_MESSAGE(CSetGlobalIntMsg, int, SetGlobalIntMsg);
DEFINE_GLOBAL_MESSAGE(CSetGlobalEntityIdMsg, EntityId, SetGlobalEntityIdMsg);
DEFINE_GLOBAL_MESSAGE(CSetGlobalStringMsg, string, SetGlobalStringMsg);

#undef DEFINE_GLOBAL_MESSAGE
#undef IMPLEMENT_IMMEDIATE_GLOBAL_MESSAGE



//------------------------------------------------------------------------
// CHANNEL
//------------------------------------------------------------------------

#define DEFINE_CHANNEL_MESSAGE(class, type, msgdef) \
	CClientSynchedStorage::class::class(int _channelId, CServerSynchedStorage *pStorage, TSynchedKey _key, TSynchedValue &_value) \
	:	CSetChannelMsg(CClientSynchedStorage::msgdef, _channelId, pStorage, _key, _value) {}; \
	EMessageSendResult CClientSynchedStorage::class::WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq) \
{ m_pStorage->SerializeValue(ser, key, value, NTypelist::IndexOf<type, TSynchedValueTypes>::value); \
	return eMSR_SentOk; }

#define IMPLEMENT_IMMEDIATE_CHANNEL_MESSAGE(type) \
	CCryMutex::CLock lock(m_mutex); \
	TSynchedKey		key; \
	TSynchedValue value; \
	SerializeValue(ser, key, value, NTypelist::IndexOf<type, TSynchedValueTypes>::value); \
	return true;


NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetChannelBoolMsg, eNRT_ReliableUnordered, 0)
{
	IMPLEMENT_IMMEDIATE_CHANNEL_MESSAGE(bool);
}

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetChannelFloatMsg, eNRT_ReliableUnordered, 0)
{
	IMPLEMENT_IMMEDIATE_CHANNEL_MESSAGE(float);
}

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetChannelIntMsg, eNRT_ReliableUnordered, 0)
{
	IMPLEMENT_IMMEDIATE_CHANNEL_MESSAGE(int);
}

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetChannelEntityIdMsg, eNRT_ReliableUnordered, 0)
{
	IMPLEMENT_IMMEDIATE_CHANNEL_MESSAGE(EntityId);
}

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetChannelStringMsg, eNRT_ReliableUnordered, 0)
{
	IMPLEMENT_IMMEDIATE_CHANNEL_MESSAGE(string);
}

//------------------------------------------------------------------------
CClientSynchedStorage::CSetChannelMsg::CSetChannelMsg(const SNetMessageDef *pDef, int _channelId, CServerSynchedStorage *pStorage, TSynchedKey _key, TSynchedValue &_value)
:	channelId(_channelId),
	m_pStorage(pStorage),
	key(_key),
	value(_value),
	INetMessage(pDef)
{
	SetGroup( 'stor' );
};

//------------------------------------------------------------------------
EMessageSendResult CClientSynchedStorage::CSetChannelMsg::WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq)
{
	return eMSR_SentOk;
}

//------------------------------------------------------------------------
void CClientSynchedStorage::CSetChannelMsg::UpdateState(uint32 fromSeq, ENetSendableStateUpdate)
{
}

//------------------------------------------------------------------------
size_t CClientSynchedStorage::CSetChannelMsg::GetSize()
{
	return sizeof(this);
};

DEFINE_CHANNEL_MESSAGE(CSetChannelBoolMsg, bool, SetChannelBoolMsg);
DEFINE_CHANNEL_MESSAGE(CSetChannelFloatMsg, float, SetChannelFloatMsg);
DEFINE_CHANNEL_MESSAGE(CSetChannelIntMsg, int, SetChannelIntMsg);
DEFINE_CHANNEL_MESSAGE(CSetChannelEntityIdMsg, EntityId, SetChannelEntityIdMsg);
DEFINE_CHANNEL_MESSAGE(CSetChannelStringMsg, string, SetChannelStringMsg);

#undef DEFINE_CHANNEL_MESSAGE
#undef IMPLEMENT_IMMEDIATE_CHANNEL_MESSAGE

//------------------------------------------------------------------------
// ENTITY
//------------------------------------------------------------------------

#define DEFINE_ENTITY_MESSAGE(class, type, msgdef) \
	CClientSynchedStorage::class::class(int _channelId, CServerSynchedStorage *pStorage, EntityId id, TSynchedKey _key, TSynchedValue &_value) \
	:	CSetEntityMsg(CClientSynchedStorage::msgdef, _channelId, pStorage, id, _key, _value) {}; \
	EMessageSendResult CClientSynchedStorage::class::WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq) \
{ ser.Value("entityId", entityId, 'eid'); \
	m_pStorage->SerializeValue(ser, key, value, NTypelist::IndexOf<type, TSynchedValueTypes>::value); \
	return eMSR_SentOk; }

#define IMPLEMENT_IMMEDIATE_ENTITY_MESSAGE(type) \
	CCryMutex::CLock lock(m_mutex); \
	TSynchedKey		key; \
	TSynchedValue value; \
	EntityId			id; \
	ser.Value("entityId", id, 'eid'); \
	SerializeEntityValue(ser, id, key, value, NTypelist::IndexOf<type, TSynchedValueTypes>::value); \
	return true;

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetEntityBoolMsg, eNRT_ReliableUnordered, eMPF_AfterSpawning)
{
	IMPLEMENT_IMMEDIATE_ENTITY_MESSAGE(bool);
}

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetEntityFloatMsg, eNRT_ReliableUnordered, eMPF_AfterSpawning)
{
	IMPLEMENT_IMMEDIATE_ENTITY_MESSAGE(float);
}

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetEntityIntMsg, eNRT_ReliableUnordered, eMPF_AfterSpawning)
{
	IMPLEMENT_IMMEDIATE_ENTITY_MESSAGE(int);
}

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetEntityEntityIdMsg, eNRT_ReliableUnordered, eMPF_AfterSpawning)
{
	IMPLEMENT_IMMEDIATE_ENTITY_MESSAGE(EntityId);
}

//------------------------------------------------------------------------
NET_IMPLEMENT_IMMEDIATE_MESSAGE(CClientSynchedStorage, SetEntityStringMsg, eNRT_ReliableUnordered, eMPF_AfterSpawning)
{
	IMPLEMENT_IMMEDIATE_ENTITY_MESSAGE(string);
}

//------------------------------------------------------------------------
CClientSynchedStorage::CSetEntityMsg::CSetEntityMsg(const SNetMessageDef *pDef, int _channelId, CServerSynchedStorage *pStorage, EntityId id, TSynchedKey _key, TSynchedValue &_value)
:	channelId(_channelId),
	m_pStorage(pStorage),
	entityId(id), 
	key(_key),
	value(_value),
	INetMessage(pDef)
{
	SetGroup( 'stor' );
};

//------------------------------------------------------------------------
EMessageSendResult CClientSynchedStorage::CSetEntityMsg::WritePayload(TSerialize ser, uint32 currentSeq, uint32 basisSeq)
{
	return eMSR_SentOk;
}

//------------------------------------------------------------------------
void CClientSynchedStorage::CSetEntityMsg::UpdateState(uint32 fromSeq, ENetSendableStateUpdate)
{
}

//------------------------------------------------------------------------
size_t CClientSynchedStorage::CSetEntityMsg::GetSize()
{
	return sizeof(this);
};

DEFINE_ENTITY_MESSAGE(CSetEntityBoolMsg, bool, SetEntityBoolMsg);
DEFINE_ENTITY_MESSAGE(CSetEntityFloatMsg, float, SetEntityFloatMsg);
DEFINE_ENTITY_MESSAGE(CSetEntityIntMsg, int, SetEntityIntMsg);
DEFINE_ENTITY_MESSAGE(CSetEntityEntityIdMsg, EntityId, SetEntityEntityIdMsg);
DEFINE_ENTITY_MESSAGE(CSetEntityStringMsg, string, SetEntityStringMsg);

#undef DEFINE_ENTITY_MESSAGE
#undef IMPLEMENT_IMMEDIATE_ENTITY_MESSAGE

//------------------------------------------------------------------------
void CClientSynchedStorage::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s,"ClientSychedStorage");
	s->Add(*this);
	GetStorageMemoryStatistics(s);
}
