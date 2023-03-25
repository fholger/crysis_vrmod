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
#ifndef __SYNCHEDSTORAGE_H__
#define __SYNCHEDSTORAGE_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <ConfigurableVariant.h>
#include <INetwork.h>
#include <IGameFramework.h>


typedef NTypelist::CConstruct<
	bool,
	float,
	int,
	EntityId,
	string
>::TType TSynchedValueTypes;


enum ESynchedValueTypes
{
	eSVT_None=-1,
	eSVT_Bool=NTypelist::IndexOf<bool, TSynchedValueTypes>::value,
	eSVT_Float=NTypelist::IndexOf<float, TSynchedValueTypes>::value,
	eSVT_Int=NTypelist::IndexOf<int, TSynchedValueTypes>::value,
	eSVT_EntityId=NTypelist::IndexOf<EntityId, TSynchedValueTypes>::value,
	eSVT_String=NTypelist::IndexOf<string, TSynchedValueTypes>::value,
};


typedef uint16																											TSynchedKey;
typedef	CConfigurableVariant<TSynchedValueTypes, sizeof(void *)>		TSynchedValue;


class CSynchedStorage : public INetMessageSink
{
public:
	CSynchedStorage(): m_pGameFramework(0) {};
	virtual ~CSynchedStorage() {};

	typedef std::map<TSynchedKey, TSynchedValue>																							TStorage;
	typedef std::map<EntityId, TStorage>																											TEntityStorageMap;
	typedef std::map<int, TStorage>																														TChannelStorageMap;

public:
	template<typename ValueType>
	void SetGlobalValue(TSynchedKey key, const ValueType &value)
	{
		TSynchedValue _value; _value.Set(value);

		bool changed=true;
		std::pair<TStorage::iterator, bool> result=m_globalStorage.insert(TStorage::value_type(key, _value));
		if (!result.second)
		{
			if ((result.first->second.GetType()==_value.GetType()) && (*result.first->second.GetPtr<ValueType>()==value))
				changed=false;
			else
				result.first->second=_value;
		}

		if (changed)
			OnGlobalChanged(key, _value);
	}

	void SetGlobalValue(TSynchedKey key, const TSynchedValue &value)
	{
		bool changed=true; // always true since we can't compare two TSynchedValue
		std::pair<TStorage::iterator, bool> result=m_globalStorage.insert(TStorage::value_type(key, value));
		if (!result.second)
			result.first->second=value;

		if (changed)
			OnGlobalChanged(key, value);
	}

	template<typename ValueType>
	void SetChannelValue(int channelId, TSynchedKey key, const ValueType &value)
	{
		TSynchedValue _value; _value.Set(value);

		TStorage *pStorage=GetChannelStorage(channelId, true);
		if (!pStorage)
			return;

		bool changed=true;
		std::pair<TStorage::iterator, bool> result=pStorage->insert(TStorage::value_type(key, _value));
		if (!result.second)
		{
			if ((result.first->second.GetType()==_value.GetType()) && (*result.first->second.GetPtr<ValueType>()==value))
				changed=false;
			else
				result.first->second=_value;
		}

		if (changed)
			OnChannelChanged(channelId, key, _value);
	}

	void SetChannelValue(int channelId, TSynchedKey key, const TSynchedValue &value)
	{
		TStorage *pStorage=GetChannelStorage(channelId, true);
		if (!pStorage)
			return;

		bool changed=true; // always true since we can't compare two TSynchedValue
		std::pair<TStorage::iterator, bool> result=pStorage->insert(TStorage::value_type(key, value));
		if (!result.second)
			result.first->second=value;

		if (changed)
			OnChannelChanged(channelId, key, value);
	}

	template<typename ValueType>
	void SetEntityValue(EntityId id, TSynchedKey key, const ValueType &value)
	{
		TSynchedValue _value; _value.Set(value);

		TStorage *pStorage=GetEntityStorage(id, true);

		bool changed=true;
		std::pair<TStorage::iterator, bool> result=pStorage->insert(TStorage::value_type(key, _value));
		if (!result.second)
		{
			if ((result.first->second.GetType()==_value.GetType()) && (*result.first->second.GetPtr<ValueType>()==value))
				changed=false;
			else
				result.first->second=_value;
		}

		if (changed)
			OnEntityChanged(id, key, _value);
	}

	void SetEntityValue(EntityId id, TSynchedKey key, const TSynchedValue &value)
	{
		TStorage *pStorage=GetEntityStorage(id, true);

		bool changed=true; // always true since we can't compare two TSynchedValue
		std::pair<TStorage::iterator, bool> result=pStorage->insert(TStorage::value_type(key, value));
		if (!result.second)
			result.first->second=value;

		if (changed)
			OnEntityChanged(id, key, value);
	}

	template<typename ValueType>
	bool GetGlobalValue(TSynchedKey key, ValueType &value) const
	{
		TStorage::const_iterator it=m_globalStorage.find(key);
		if (it==m_globalStorage.end())
			return false;

		value=*it->second.GetPtr<ValueType>();

		return true;
	}

	bool GetGlobalValue(TSynchedKey key, TSynchedValue &value) const
	{
		TStorage::const_iterator it=m_globalStorage.find(key);
		if (it==m_globalStorage.end())
			return false;

		value=it->second;

		return true;
	}

	template<typename ValueType>
	bool GetChannelValue(int channelId, TSynchedKey key, ValueType &value) const
	{
		if (!gEnv->bServer)
			return false;

		const TStorage *pStorage=0;
		TChannelStorageMap::const_iterator cit=m_channelStorageMap.find(channelId);
		if (cit!=m_channelStorageMap.end())
			pStorage=&cit->second;
		else
		{
			INetChannel *pNetChannel=m_pGameFramework->GetNetChannel(channelId);
			if (pNetChannel && pNetChannel->IsLocal())
				pStorage=&m_channelStorage;
		}
		
		if (!pStorage)
			return false;

		TStorage::const_iterator it=pStorage->find(key);
		if (it==pStorage->end())
			return false;

		value=*it->second.GetPtr<ValueType>();

		return true;
	}

	bool GetChannelValue(int channelId, TSynchedKey key, TSynchedValue &value) const
	{
		const TStorage *pStorage=0;
		TChannelStorageMap::const_iterator cit=m_channelStorageMap.find(channelId);
		if (cit!=m_channelStorageMap.end())
			pStorage=&cit->second;
		else
		{
			INetChannel *pNetChannel=m_pGameFramework->GetNetChannel(channelId);
			if (pNetChannel && pNetChannel->IsLocal())
				pStorage=&m_channelStorage;
		}

		if (!pStorage)
			return false;

		TStorage::const_iterator it=pStorage->find(key);
		if (it==pStorage->end())
			return false;

		value=it->second;

		return true;
	}

	template<typename ValueType>
	bool GetChannelValue(TSynchedKey key, ValueType &value) const
	{
		TStorage::const_iterator it=m_channelStorage.find(key);
		if (it==m_channelStorage.end())
			return false;

		value=*it->second.GetPtr<ValueType>();

		return true;
	}

	bool GetChannelValue(TSynchedKey key, TSynchedValue &value) const
	{
		TStorage::const_iterator it=m_channelStorage.find(key);
		if (it==m_channelStorage.end())
			return false;

		value=it->second;

		return true;
	}

	template<typename ValueType>
	bool GetEntityValue(EntityId entityId, TSynchedKey key, ValueType &value) const
	{
		TEntityStorageMap::const_iterator eit=m_entityStorage.find(entityId);
		if (eit==m_entityStorage.end())
			return false;

		TStorage::const_iterator it=eit->second.find(key);
		if (it==eit->second.end())
			return false;

		value=*it->second.GetPtr<ValueType>();

		return true;
	}

	bool GetEntityValue(EntityId entityId, TSynchedKey key, TSynchedValue &value) const
	{
		TEntityStorageMap::const_iterator eit=m_entityStorage.find(entityId);
		if (eit==m_entityStorage.end())
			return false;

		TStorage::const_iterator it=eit->second.find(key);
		if (it==eit->second.end())
			return false;

		value=it->second;

		return true;
	}

	int GetGlobalValueType(TSynchedKey key) const
	{
		TStorage::const_iterator it=m_globalStorage.find(key);
		if (it==m_globalStorage.end())
			return eSVT_None;

		return it->second.GetType();
	}

	int GetEntityValueType(EntityId id, TSynchedKey key) const
	{
		TEntityStorageMap::const_iterator eit=m_entityStorage.find(id);
		if (eit==m_entityStorage.end())
			return eSVT_None;

		TStorage::const_iterator it=eit->second.find(key);
		if (it==eit->second.end())
			return eSVT_None;

		return it->second.GetType();
	}

	virtual void Reset();

	virtual void Dump();

	virtual void SerializeValue(TSerialize ser, TSynchedKey &key, TSynchedValue &value, int type);
	virtual void SerializeEntityValue(TSerialize ser, EntityId id, TSynchedKey &key, TSynchedValue &value, int type);

	virtual TStorage *GetEntityStorage(EntityId id, bool create=false);
	virtual TStorage *GetChannelStorage(int channelId, bool create=false);

	virtual void OnGlobalChanged(TSynchedKey key, const TSynchedValue &value) {};
	virtual void OnChannelChanged(int channelId, TSynchedKey key, const TSynchedValue &value) {};
	virtual void OnEntityChanged(EntityId id, TSynchedKey key, const TSynchedValue &value) {};

	void GetStorageMemoryStatistics(ICrySizer * s);

	TStorage						m_globalStorage;
	TStorage						m_channelStorage;
	TChannelStorageMap	m_channelStorageMap;
	TEntityStorageMap		m_entityStorage;

	IGameFramework			*m_pGameFramework;
};

#endif //__SYNCHEDSTORAGE_H__
