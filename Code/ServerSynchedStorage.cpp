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
#include "ServerSynchedStorage.h"
#include "ClientSynchedStorage.h"
#include "Game.h"


void CServerSynchedStorage::Reset()
{
	CCryMutex::CLock lock(m_mutex);

	CSynchedStorage::Reset();

	for (TChannelMap::const_iterator it=m_channels.begin(); it!=m_channels.end(); ++it)
		ResetChannel(it->first);
}

void CServerSynchedStorage::ResetChannel(int channelId)
{
	CCryMutex::CLock lock(m_mutex);

	TChannelQueueMap::iterator cit=m_channelQueue.lower_bound(SChannelQueueEnt(channelId, 0));
	while (cit != m_channelQueue.end() && cit->first.channel == channelId)
	{
		TChannelQueueMap::iterator next = cit;
		++next;
		m_channelQueue.erase(cit);
		cit = next;
	}

	TChannelQueueMap::iterator git=m_globalQueue.lower_bound(SChannelQueueEnt(channelId, 0));
	while (git != m_globalQueue.end() && git->first.channel == channelId)
	{
		TChannelQueueMap::iterator next = git;
		++next;
		m_globalQueue.erase(git);
		git = next;
	}

	TChannelEntityQueueMap::iterator eit=m_entityQueue.lower_bound(SChannelEntityQueueEnt(channelId, 0, 0));
	while (eit != m_entityQueue.end() && eit->first.channel == channelId)
	{
		TChannelEntityQueueMap::iterator next = eit;
		++next;
		m_entityQueue.erase(eit);
		eit = next;
	}

	if (SChannel *pChannel = GetChannel(channelId))
	{
		if (pChannel->pNetChannel)
			pChannel->pNetChannel->AddSendable( new CClientSynchedStorage::CResetMsg(channelId, this), 1, &pChannel->lastOrderedMessage, &pChannel->lastOrderedMessage );
	}
}

//------------------------------------------------------------------------
void CServerSynchedStorage::DefineProtocol(IProtocolBuilder *pBuilder)
{
	pBuilder->AddMessageSink(this, CClientSynchedStorage::GetProtocolDef(), CServerSynchedStorage::GetProtocolDef());
}

//------------------------------------------------------------------------
void CServerSynchedStorage::AddToGlobalQueue(TSynchedKey key)
{
	for (TChannelMap::iterator it=m_channels.begin(); it!=m_channels.end(); ++it)
	{
		if (!it->second.local)
			AddToGlobalQueueFor(it->first, key);
	}
}

//------------------------------------------------------------------------
void CServerSynchedStorage::AddToEntityQueue(EntityId entityId, TSynchedKey key)
{
	for (TChannelMap::iterator it=m_channels.begin(); it!=m_channels.end(); ++it)
	{
		if (!it->second.local)
			AddToEntityQueueFor(it->first, entityId, key);
	}
}

//------------------------------------------------------------------------
void CServerSynchedStorage::AddToChannelQueue(int channelId, TSynchedKey key)
{
	SChannel * pChannel = GetChannel(channelId);
	assert(pChannel);
	if (!pChannel || !pChannel->pNetChannel || pChannel->local)
		return;

	SSendableHandle& msgHdl = m_channelQueue[SChannelQueueEnt(channelId, key)];

	TSynchedValue value; 
	bool ok=GetChannelValue(channelId, key, value);
	assert(ok);
	if (!ok)
		return;

	CClientSynchedStorage::CSetChannelMsg *pMsg=0;

	switch (value.GetType())
	{
	case eSVT_Bool:
		pMsg=new CClientSynchedStorage::CSetChannelBoolMsg(channelId, this, key, value);
		break;
	case eSVT_Float:
		pMsg=new CClientSynchedStorage::CSetChannelFloatMsg(channelId, this, key, value);
		break;
	case eSVT_Int:
		pMsg=new CClientSynchedStorage::CSetChannelIntMsg(channelId, this, key, value);
		break;
	case eSVT_EntityId:
		pMsg=new CClientSynchedStorage::CSetChannelEntityIdMsg(channelId, this, key, value);
		break;
	case eSVT_String:
		pMsg=new CClientSynchedStorage::CSetChannelStringMsg(channelId, this, key, value);
		break;
	}

	if (pMsg)
		pChannel->pNetChannel->SubstituteSendable(pMsg, 1, &pChannel->lastOrderedMessage, &msgHdl);
	else
	{
		assert(!"Invalid type!");
	}
}

//------------------------------------------------------------------------
void CServerSynchedStorage::AddToGlobalQueueFor(int channelId, TSynchedKey key)
{
	SChannel * pChannel = GetChannel(channelId);
	assert(pChannel);
	if (!pChannel || !pChannel->pNetChannel || pChannel->local)
		return;

	SSendableHandle& msgHdl = m_globalQueue[SChannelQueueEnt(channelId, key)];

	TSynchedValue value; 
	bool ok=GetGlobalValue(key, value);
	assert(ok);
	if (!ok)
		return;

	CClientSynchedStorage::CSetGlobalMsg *pMsg=0;

	switch (value.GetType())
	{
	case eSVT_Bool:
		pMsg=new CClientSynchedStorage::CSetGlobalBoolMsg(channelId, this, key, value);
		break;
	case eSVT_Float:
		pMsg=new CClientSynchedStorage::CSetGlobalFloatMsg(channelId, this, key, value);
		break;
	case eSVT_Int:
		pMsg=new CClientSynchedStorage::CSetGlobalIntMsg(channelId, this, key, value);
		break;
	case eSVT_EntityId:
		pMsg=new CClientSynchedStorage::CSetGlobalEntityIdMsg(channelId, this, key, value);
		break;
	case eSVT_String:
		pMsg=new CClientSynchedStorage::CSetGlobalStringMsg(channelId, this, key, value);
		break;
	}

	if (pMsg)
		pChannel->pNetChannel->SubstituteSendable(pMsg, 1, &pChannel->lastOrderedMessage, &msgHdl);
	else
	{
		assert(!"Invalid type!");
	}
}

//------------------------------------------------------------------------
void CServerSynchedStorage::AddToEntityQueueFor(int channelId, EntityId entityId, TSynchedKey key)
{
	SChannel * pChannel = GetChannel(channelId);
	assert(pChannel);
	if (!pChannel || !pChannel->pNetChannel || pChannel->local)
		return;

	SSendableHandle& msgHdl = m_entityQueue[SChannelEntityQueueEnt(channelId, entityId, key)];

	TSynchedValue value; 
	bool ok=GetEntityValue(entityId, key, value);
	assert(ok);
	if (!ok)
		return;

	CClientSynchedStorage::CSetEntityMsg *pMsg=0;

	switch (value.GetType())
	{
	case eSVT_Bool:
		pMsg=new CClientSynchedStorage::CSetEntityBoolMsg(channelId, this, entityId, key, value);
		break;
	case eSVT_Float:
		pMsg=new CClientSynchedStorage::CSetEntityFloatMsg(channelId, this, entityId, key, value);
		break;
	case eSVT_Int:
		pMsg=new CClientSynchedStorage::CSetEntityIntMsg(channelId, this, entityId, key, value);
		break;
	case eSVT_EntityId:
		pMsg=new CClientSynchedStorage::CSetEntityEntityIdMsg(channelId, this, entityId, key, value);
		break;
	case eSVT_String:
		pMsg=new CClientSynchedStorage::CSetEntityStringMsg(channelId, this, entityId, key, value);
		break;
	}

	if (pMsg)
		pChannel->pNetChannel->SubstituteSendable(pMsg, 1, &pChannel->lastOrderedMessage, &msgHdl);
	else
	{
		assert(!"Invalid type!");
	}
}

//------------------------------------------------------------------------
void CServerSynchedStorage::FullSynch(int channelId, bool reset)
{
	if (reset)
		ResetChannel(channelId);

	for (TStorage::iterator it=m_channelStorage.begin(); it!=m_channelStorage.end();++it)
		AddToChannelQueue(channelId, it->first);

	for (TStorage::iterator it=m_globalStorage.begin(); it!=m_globalStorage.end();++it)
		AddToGlobalQueueFor(channelId, it->first);

	for (TEntityStorageMap::const_iterator eit=m_entityStorage.begin(); eit!=m_entityStorage.end();++eit)
	{
		const TStorage &storage=eit->second;
		for (TStorage::const_iterator it=storage.begin();it!=storage.end();++it)
			AddToEntityQueueFor(channelId, eit->first, it->first);
	}
}

//------------------------------------------------------------------------
void CServerSynchedStorage::OnGlobalChanged(TSynchedKey key, const TSynchedValue &value)
{
	AddToGlobalQueue(key);
}

//------------------------------------------------------------------------
void CServerSynchedStorage::OnChannelChanged(int channelId, TSynchedKey key, const TSynchedValue &value)
{
	AddToChannelQueue(channelId, key);
}

//------------------------------------------------------------------------
void CServerSynchedStorage::OnEntityChanged(EntityId entityId, TSynchedKey key, const TSynchedValue &value)
{
	AddToEntityQueue(entityId, key);
}

//------------------------------------------------------------------------
void CServerSynchedStorage::OnClientConnect(int channelId)
{
	INetChannel *pNetChannel=g_pGame->GetIGameFramework()->GetNetChannel(channelId);

	SChannel *pChannel=GetChannel(channelId);
	if (pChannel && pChannel->onhold)
		pChannel->pNetChannel=pNetChannel;
	else
	{
		if (pChannel)
		{
			m_channels.erase(m_channels.find(channelId));
			ResetChannel(channelId);	// clear up anything in the queues.
																// the reset message won't be sent since we've deleted the channel from the map
		}
		m_channels.insert(TChannelMap::value_type(channelId, SChannel(pNetChannel, pNetChannel->IsLocal())));
	}
}

//------------------------------------------------------------------------
void CServerSynchedStorage::OnClientDisconnect(int channelId, bool onhold)
{
	SChannel *pChannel=GetChannel(channelId);
	if (pChannel)
		pChannel->pNetChannel=0;

	if (!onhold)
		m_channels.erase(channelId);
	else
		pChannel->onhold=onhold;
//	m_globalQueue.erase(channelId);
//	m_entityQueue.erase(channelId);

	ResetChannel(channelId);
}

//------------------------------------------------------------------------
void CServerSynchedStorage::OnClientEnteredGame(int channelId)
{
	SChannel *pChannel=GetChannel(channelId);
 	if (pChannel && pChannel->pNetChannel && !pChannel->pNetChannel->IsLocal())
		FullSynch(channelId, true);
}

//------------------------------------------------------------------------
bool CServerSynchedStorage::OnSetGlobalMsgComplete(CClientSynchedStorage::CSetGlobalMsg *pMsg, int channelId, uint32 fromSeq, bool ack)
{
	CCryMutex::CLock lock(m_mutex);

	if (ack)
		return true;
	else
	{
		// got a nack, so reque
		AddToGlobalQueueFor(channelId, pMsg->key);
		return true;
	}
}

//------------------------------------------------------------------------
bool CServerSynchedStorage::OnSetChannelMsgComplete(CClientSynchedStorage::CSetChannelMsg *pMsg, int channelId, uint32 fromSeq, bool ack)
{
	CCryMutex::CLock lock(m_mutex);

	if (ack)
		return true;
	else
	{
		// got a nack, so reque
		AddToChannelQueue(channelId, pMsg->key);
		return true;
	}
}

//------------------------------------------------------------------------
bool CServerSynchedStorage::OnSetEntityMsgComplete(CClientSynchedStorage::CSetEntityMsg *pMsg, int channelId, uint32 fromSeq, bool ack)
{
	CCryMutex::CLock lock(m_mutex);

	if (ack)
		return true;
	else
	{
		// got a nack, so reque
		AddToEntityQueueFor(channelId, pMsg->entityId, pMsg->key);
		return true;
	}
}

//------------------------------------------------------------------------
CServerSynchedStorage::SChannel *CServerSynchedStorage::GetChannel(int channelId)
{
	TChannelMap::iterator it=m_channels.find(channelId);
	if (it!=m_channels.end())
		return &it->second;
	return 0;
}

//------------------------------------------------------------------------
CServerSynchedStorage::SChannel *CServerSynchedStorage::GetChannel(INetChannel *pNetChannel)
{
	for (TChannelMap::iterator it=m_channels.begin(); it!=m_channels.end(); ++it)
		if (it->second.pNetChannel==pNetChannel)
			return &it->second;
	return 0;
}

//------------------------------------------------------------------------
int CServerSynchedStorage::GetChannelId(INetChannel *pNetChannel) const
{
	for (TChannelMap::const_iterator it=m_channels.begin(); it!=m_channels.end(); ++it)
		if (it->second.pNetChannel==pNetChannel)
			return it->first;
	return 0;
}

void CServerSynchedStorage::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s,"ServerSychedStorage");
	s->Add(*this);
	s->AddContainer(m_globalQueue);
	s->AddContainer(m_entityQueue);
	s->AddContainer(m_channelQueue);
	s->AddContainer(m_channels);
	GetStorageMemoryStatistics(s);
}
