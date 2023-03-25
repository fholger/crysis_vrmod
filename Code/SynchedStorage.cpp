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
#include "SynchedStorage.h"
#include <IEntitySystem.h>


//------------------------------------------------------------------------
void CSynchedStorage::Reset()
{
	m_globalStorage.clear();
	m_entityStorage.clear();
	m_channelStorageMap.clear();
	m_channelStorage.clear();
}

//------------------------------------------------------------------------
void CSynchedStorage::Dump()
{
	struct ValueDumper
	{
		ValueDumper(TSynchedKey key, const TSynchedValue &value)
		{
			switch(value.GetType())
			{
			case eSVT_Bool:
				CryLogAlways("  %.08d -     bool: %s", key, *value.GetPtr<bool>()?"true":"false");
				break;
			case eSVT_Float:
				CryLogAlways("  %.08d -    float: %f", key, *value.GetPtr<float>());
				break;
			case eSVT_Int:
				CryLogAlways("  %.08d -      int: %d", key, *value.GetPtr<int>());
				break;
			case eSVT_EntityId:
				CryLogAlways("  %.08d - entityId: %.08x", key, *value.GetPtr<EntityId>());
				break;
			case eSVT_String:
				CryLogAlways("  %.08d -  string: %s", key, value.GetPtr<string>()->c_str());
				break;
			default:
				CryLogAlways("  %.08d - unknown: %.08x", key, *value.GetPtr<uint32>());
				break;
			}
		}
	};

	CryLogAlways("---------------------------");
	CryLogAlways(" SYNCHED STORAGE DUMP");
	CryLogAlways("---------------------------\n");
	CryLogAlways("Globals:");

	for (TStorage::const_iterator it=m_globalStorage.begin(); it!=m_globalStorage.end(); ++it)
		ValueDumper(it->first, it->second);

	CryLogAlways("---------------------------\n");
	CryLogAlways("Local Channel:");
	for (TStorage::const_iterator it=m_channelStorage.begin(); it!=m_channelStorage.end(); ++it)
		ValueDumper(it->first, it->second);

	if (gEnv->bServer)
	{
		CryLogAlways("---------------------------\n");
		for (TChannelStorageMap::const_iterator cit=m_channelStorageMap.begin(); cit!=m_channelStorageMap.end(); ++cit)
		{
			INetChannel *pNetChannel=m_pGameFramework->GetNetChannel(cit->first);
			CryLogAlways("Channel %d (%s)", cit->first, pNetChannel?pNetChannel->GetName():"null");
			for (TStorage::const_iterator it=cit->second.begin(); it!=cit->second.end(); ++it)
				ValueDumper(it->first, it->second);
		}
	}

	CryLogAlways("---------------------------\n");
	for (TEntityStorageMap::const_iterator eit=m_entityStorage.begin(); eit!=m_entityStorage.end(); ++eit)
	{
		IEntity *pEntity=gEnv->pEntitySystem->GetEntity(eit->first);
		CryLogAlways("Entity %.08d(%s)", eit->first, pEntity?pEntity->GetName():"null");
		for (TStorage::const_iterator it=eit->second.begin(); it!=eit->second.end(); ++it)
			ValueDumper(it->first, it->second);
	}
}

//------------------------------------------------------------------------
void CSynchedStorage::SerializeValue(TSerialize ser, TSynchedKey &key, TSynchedValue &value, int type)
{
	ser.Value("key", key, 'ssk');

	switch (type)
	{
	case eSVT_Bool:
		{
			bool b;
			if (ser.IsWriting())
				b=*value.GetPtr<bool>();
			ser.Value("value", b, 'bool');
			if (ser.IsReading())
				SetGlobalValue(key, b);
		}
		break;
	case eSVT_Float:
		{
			float f;
			if (ser.IsWriting())
				f=*value.GetPtr<float>();
			ser.Value("value", f, 'ssfl');
			if (ser.IsReading())
				SetGlobalValue(key, f);
		}
		break;
	case eSVT_Int:
		{
			int i;
			if (ser.IsWriting())
				i=*value.GetPtr<int>();
			ser.Value("value", i, 'ssi');
			if (ser.IsReading())
				SetGlobalValue(key, i);
		}
		break;
	case eSVT_EntityId:
		{
			EntityId e;
			if (ser.IsWriting())
				e=*value.GetPtr<EntityId>();
			ser.Value("value", e, 'eid');
			if (ser.IsReading())
				SetGlobalValue(key, e);
		}
	case eSVT_String:
		{
			static string s;
			s.resize(0);
			if (ser.IsWriting())
				s=*value.GetPtr<string>();
			ser.Value("value", s);
			if (ser.IsReading())
				SetGlobalValue(key, s);
		}
	break;
	default:
		assert(0);
		break;
	}
}

//------------------------------------------------------------------------
void CSynchedStorage::SerializeEntityValue(TSerialize ser, EntityId id, TSynchedKey &key, TSynchedValue &value, int type)
{
	ser.Value("key", key, 'ssk');

	switch (type)
	{
	case eSVT_Bool:
		{
			bool b;
			if (ser.IsWriting())
				b=*value.GetPtr<bool>();
			ser.Value("value", b, 'bool');
			if (ser.IsReading())
				SetEntityValue(id, key, b);
		}
		break;
	case eSVT_Float:
		{
			float f;
			if (ser.IsWriting())
				f=*value.GetPtr<float>();
			ser.Value("value", f, 'ssfl');
			if (ser.IsReading())
				SetEntityValue(id, key, f);
		}
		break;
	case eSVT_Int:
		{
			int i;
			if (ser.IsWriting())
				i=*value.GetPtr<int>();
			ser.Value("value", i, 'ssi');
			if (ser.IsReading())
				SetEntityValue(id, key, i);
		}
		break;
	case eSVT_EntityId:
		{
			EntityId e;
			if (ser.IsWriting())
				e=*value.GetPtr<EntityId>();
			ser.Value("value", e, 'eid');
			if (ser.IsReading())
				SetEntityValue(id, key, e);
		}
	case eSVT_String:
		{
			static string s;
			s.resize(0);
			if (ser.IsWriting())
				s=*value.GetPtr<string>();
			ser.Value("value", s);
			if (ser.IsReading())
				SetEntityValue(id, key, s);
		}
		break;
	default:
		assert(0);
		break;
	}
}

//------------------------------------------------------------------------
CSynchedStorage::TStorage *CSynchedStorage::GetEntityStorage(EntityId id, bool create)
{
	TEntityStorageMap::iterator it=m_entityStorage.find(id);
	if (it!=m_entityStorage.end())
		return &it->second;
	else if (create)
	{
		std::pair<TEntityStorageMap::iterator, bool> result=m_entityStorage.insert(
			TEntityStorageMap::value_type(id, TStorage()));
		return &result.first->second;
	}

	return 0;
}

//------------------------------------------------------------------------
CSynchedStorage::TStorage *CSynchedStorage::GetChannelStorage(int channelId, bool create)
{
	TChannelStorageMap::iterator it=m_channelStorageMap.find(channelId);
	if (it!=m_channelStorageMap.end())
		return &it->second;
	else
	{
		INetChannel *pNetChannel=m_pGameFramework->GetNetChannel(channelId);
		if ((!gEnv->bServer && !pNetChannel) || (gEnv->bServer && pNetChannel->IsLocal()))
			return &m_channelStorage;

		if (gEnv->bServer && create)
		{
			std::pair<TChannelStorageMap::iterator, bool> result=m_channelStorageMap.insert(
				TEntityStorageMap::value_type(channelId, TStorage()));
			return &result.first->second;
		}
	}

	return 0;
}


static void AddStorageTo( const CSynchedStorage::TStorage& stor, ICrySizer * s )
{
	for (CSynchedStorage::TStorage::const_iterator iter = stor.begin(); iter != stor.end(); ++iter)
	{
		s->Add(iter->first);
		int nSize = iter->second.GetMemorySize();
		s->AddObject( ((const char*)(iter->first)+1),nSize );
	}
}

void CSynchedStorage::GetStorageMemoryStatistics(ICrySizer * s)
{
	AddStorageTo(m_globalStorage, s);
	for (TEntityStorageMap::const_iterator iter = m_entityStorage.begin(); iter != m_entityStorage.end(); ++iter)
	{
		s->Add(*iter);
		AddStorageTo(iter->second, s);
	}
}
