/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Game-side part of Palyer's network profile

-------------------------------------------------------------------------
History:
- 03/2007: Created by Stas Spivakov

*************************************************************************/
#include "StdAfx.h"
#include "INetwork.h"
#include "GameNetworkProfile.h"
#include "MPHub.h"

#include "Game.h"
#include "GameCVars.h"
#include "HUD/HUD.h"
#include "HUD/HUDRadar.h"
#include "HUD/HUDTextChat.h"

static const bool DEBUG_VERBOSE = false;

enum EUserInfoKey
{
  eUIK_none,
  eUIK_nick,
	eUIK_namespace,
  eUIK_country
};

static TKeyValuePair<EUserInfoKey,const char*>
gUserInfoKeys[] = {
  {eUIK_none,""},
  {eUIK_nick,"nick"},
	{eUIK_namespace,"namespace"},
  {eUIK_country,"country"},
};

struct CGameNetworkProfile::SChatText
{
  struct SChatLine 
  {
    string  text;
    int     id;
  };
  SChatText():m_viewed(false),m_size(1000)
  {}
  void AddText(const char* str, int text_id)
  {
    m_viewed = false;
    m_text.push_back(SChatLine());
    m_text.back().text = str;
    m_text.back().id = text_id;
    while(m_size && m_text.size()>m_size)
    {
      int id = m_text.front().id;
      do
      {
        m_text.pop_front();
      }while(m_text.front().id == id);
    }
  }
  int                     m_size;
  bool                    m_viewed;
  std::list<SChatLine>    m_text;
  std::vector<SChatUser>  m_chatList;
};


struct IStoredSerialize
{
	virtual void Serialize(int& val,const char* name) = 0;
	virtual void Serialize(float& val, const char* name) = 0;
	virtual void Serialize(string& val, const char* name) = 0;
};

class CStoredBase
{
public:
	struct SMetaData
	{
		SMetaData(int recid = -1, bool fr = false):recordId(recid),free(fr),locked(false){}
		int		recordId;
		bool	free:1;
		bool  locked:1;
	};

	enum EFieldType
	{
		eFT_int,
		eFT_float,
		eFT_string
	};

	struct SFieldConfig
	{
		SFieldConfig(EFieldType t = eFT_int, const char* n = ""):type(t),name(n){}
		EFieldType	type;
		const char* name;
	};

	struct SConfigBuilder : public IStoredSerialize
	{
		SConfigBuilder(std::vector<SFieldConfig>& cfg):m_cfg(cfg){}

		virtual void Serialize(int& val,const char* name)
		{
			m_cfg.push_back(SFieldConfig(eFT_int,name));
		}
		virtual void Serialize(float& val, const char* name)
		{
			m_cfg.push_back(SFieldConfig(eFT_float,name));
		}
		virtual void Serialize(string& val, const char* name)
		{
			m_cfg.push_back(SFieldConfig(eFT_string,name));
		}
		std::vector<SFieldConfig>& m_cfg;
	};

	class SReadSerializer : public IStoredSerialize
	{
	public:
		SReadSerializer(int value, int idx):ivalue(value),type(eFT_int),field_idx(idx),cur_field(0){}
		SReadSerializer(float value, int idx):fvalue(value),type(eFT_float),field_idx(idx),cur_field(0){}
		SReadSerializer(const char* value, int idx):svalue(value),type(eFT_string),field_idx(idx),cur_field(0){}
		virtual void Serialize(int& val,const char* name)
		{
			if(cur_field == field_idx)
			{
				val = ivalue;
			}
			cur_field++;
		}
		virtual void Serialize(float& val, const char* name)
		{
			if(cur_field == field_idx)
			{
				val = fvalue;
			}
			cur_field++;
		}
		virtual void Serialize(string& val, const char* name)
		{
			if(cur_field == field_idx)
			{
				val = svalue;
			}
			cur_field++;			
		}
	private:
		int					ivalue;
		float				fvalue;
		const char* svalue;
		int					field_idx;
		int					cur_field;
		EFieldType	type;
	};


	class SWriteSerializer : public IStoredSerialize
	{
	public:
		SWriteSerializer(int idx):field_idx(idx),cur_field(0){}
		virtual void Serialize(int& val,const char* name)
		{
			if(cur_field == field_idx)
			{
				ivalue = val;
				type = eFT_int;
			}
			cur_field++;
		}
		virtual void Serialize(float& val, const char* name)
		{
			if(cur_field == field_idx)
			{
				fvalue = val;
				type = eFT_float;
			}
			cur_field++;
		}
		virtual void Serialize(string& val, const char* name)
		{
			if(cur_field == field_idx)
			{
				svalue = val;
				type = eFT_string;
			}
			cur_field++;			
		}
		const char* GetSValue()const
		{
			return svalue;
		}
		const int		GetIValue()const
		{
			return ivalue;
		}
		const float GetFValue()const
		{
			return fvalue;
		}
	private:
		int					ivalue;
		float				fvalue;
		const char* svalue;
		int					field_idx;
		int					cur_field;
		EFieldType	type;
	};

	CStoredBase()
	{
		
	}

	template<class T>
	void InitConfig()
	{
		T val;
		SConfigBuilder bld(m_config);
		val.Serialize(&bld);
	}
	
	virtual ~CStoredBase()
	{}

	std::vector<SFieldConfig>		m_config;
};

template<class T>
class CGameNetworkProfile::TStoredArray : public CStoredBase
{
private:
	struct SReader : public CGameNetworkProfile::SStorageQuery, public IStatsReader
	{
		SReader(TStoredArray* p,CGameNetworkProfile* pr, const char* table_name):SStorageQuery(pr),m_array(p),m_id(-1),m_table(table_name){}
		
		virtual const char* GetTableName()
		{
			return m_table;
		}
		virtual int         GetFieldsNum()
		{
			return m_array->m_config.size();
		}
		virtual const char* GetFieldName(int i)
		{
			return m_array->m_config[i].name;
		}
		virtual void        NextRecord(int id)
		{
			AddValue();
			m_id = id;
		}
		virtual void        OnValue(int field, const char* val)
		{
			SReadSerializer s(val,field);
			m_value.Serialize(&s);
		}
		virtual void        OnValue(int field, int val)
		{
			SReadSerializer s(val,field);
			m_value.Serialize(&s);
		}
		virtual void        OnValue(int field, float val)
		{
			SReadSerializer s(val,field);
			m_value.Serialize(&s);
		}
		virtual void        End(bool ok)
		{
			if(ok)
				AddValue();
			delete this;//we're done
		}
		void AddValue()
		{
			if(m_id != -1 && m_parent)
			{
				m_array->m_array.push_back(std::make_pair(m_value,SMetaData(m_id)));
			}
		}
		T							m_value;
		int						m_id;
		TStoredArray*	m_array;
		const char*   m_table;
	};

	struct SWriter : public CGameNetworkProfile::SStorageQuery, public IStatsWriter
	{
		SWriter(TStoredArray* p, CGameNetworkProfile* pr, const char* table_name):SStorageQuery(pr),m_array(p),m_curidx(-1),m_processed(0),m_table(table_name)
		{
			for(int i=0;i<m_array->m_array.size();++i)
			{
				SMetaData& md = m_array->GetMetaData(i);
				if(!md.free && md.recordId == -1 && !md.locked)
				{
					md.locked = true;
					if(DEBUG_VERBOSE)
						CryLog("Writing %d",i);
					m_ids.push_back(i);
				}
			}
		}

		virtual const char* GetTableName()
		{
			return m_table;
		}
		virtual int GetFieldsNum()
		{
			return m_array->m_config.size();
		}
		virtual const char* GetFieldName(int i)
		{
			return m_array->m_config[i].name;			
		}
		virtual int GetRecordsNum()
		{
			return m_ids.size();
		}
		virtual int NextRecord()
		{
			++m_curidx;
			return -1;
		}
		virtual bool GetValue(int field, const char*& val)
		{
			if(m_array->m_config[field].type != eFT_string)
				return false;
			SWriteSerializer ser(field);
			m_array->m_array[m_ids[m_curidx]].first.Serialize(&ser);
			val = ser.GetSValue();
			return true;
		}
		virtual bool GetValue(int field, int& val)
		{
			if(m_array->m_config[field].type != eFT_int)
				return false;
			SWriteSerializer ser(field);
			m_array->m_array[m_ids[m_curidx]].first.Serialize(&ser);
			val = ser.GetIValue();
			return true;
		}
		virtual bool GetValue(int field, float& val)
		{
			if(m_array->m_config[field].type != eFT_float)
				return false;
			SWriteSerializer ser(field);
			m_array->m_array[m_ids[m_curidx]].first.Serialize(&ser);
			val = ser.GetFValue();
			return true;			
		}
		//output
		virtual void  OnResult(int idx, int id, bool success)
		{
			m_processed++;
			if(DEBUG_VERBOSE)
				CryLog("Written %s %d, id:%x",success?"ok":"failed",m_ids[idx],id);
			if(m_parent)
			{
				m_array->GetMetaData(m_ids[idx]).locked = false;
				if(success)
				{
					m_array->GetMetaData(m_ids[idx]).recordId = id;				
				}
			}
		}

		virtual void End(bool ok)
		{			
			delete this;
		}
		int								m_processed;
		int								m_curidx;
		std::vector<int>	m_ids;
		TStoredArray*			m_array;
		const	char*				m_table;
	};

	struct SDeleter : public CGameNetworkProfile::SStorageQuery, public IStatsDeleter
	{
		SDeleter(TStoredArray* p, CGameNetworkProfile* pr, const char* table_name):SStorageQuery(pr),m_array(p),cur_idx(0),processed(0),m_table(table_name)
		{
			for(int i=0;i<m_array->m_array.size();++i)
			{
				SMetaData& md = m_array->GetMetaData(i);
				if(md.free && md.recordId != -1 && !md.locked)
				{
					md.locked = true;
					if(DEBUG_VERBOSE)
						CryLog("Deleting %d, id:%x",i,md.recordId);
					ids.push_back(i);
				}
			}
		}
		virtual const char* GetTableName()
		{
			return m_table;
		}
		virtual int   GetRecordsNum()
		{
			return ids.size();
		}
		virtual int   NextRecord()
		{
			return m_array->GetMetaData(ids[cur_idx++]).recordId;
		}
		virtual void  OnResult(int idx, int id, bool success)
		{
			processed++;
			if(DEBUG_VERBOSE)
				CryLog("Deleted %s %d, id:%x",success?"ok":"failed",ids[idx],id);
			if(m_parent)
			{
				m_array->GetMetaData(ids[idx]).locked = false;
				if(success)
					m_array->GetMetaData(ids[idx]).recordId = -1;
			}
		}
		virtual void End(bool success)
		{
			delete this;//we're done
		}
		std::vector<int>	ids;
		int								cur_idx;
		int								processed;
		TStoredArray*			m_array;
		const char*				m_table;
	};

	SMetaData& GetMetaData(int idx)
	{
		return m_array[idx].second;
	}

public:
	TStoredArray():m_numFree(0)
	{
		InitConfig<T>();
	}

	void push(const T& val)
	{
		if(m_numFree)
		{
			for(int i=0;i<m_array.size();++i)
			{
				if(m_array[i].second.free)
				{
					m_array[i].first = val;
					m_array[i].second.free = false;
					m_numFree--;
					return;
				}
			}
		}
		m_array.push_back(std::make_pair(val,SMetaData()));
	}

	void erase(int idx)
	{
		assert(idx<size());
		int r_idx = 0;
		for(int i=0;i<m_array.size();++i)
		{
			if(m_array[i].second.free)
				continue;
			if(r_idx == idx)
			{
				m_array[i].first = T();
				m_array[i].second.free = true;
				m_numFree++;
				return;
			}
			++r_idx;
		}
		assert(false);
	}

	bool empty()const
	{
		return m_array.empty() || m_array.size() == m_numFree;
	}

	int size()const
	{
		return m_array.size()-m_numFree;
	}

	const T& operator[](int idx)const
	{
		assert(idx<size());
		int r_idx = 0;
		for(int i=0;i<m_array.size();++i)
		{
			if(m_array[i].second.free)
				continue;
			if(r_idx == idx)
			{
				return m_array[i].first;
			}
			++r_idx;
		}
		assert(false);
		return m_array[0].first;
	}

	void clear()
	{
		m_array.resize(0);
		m_numFree = 0;
	}
	
	void LoadRecords(CGameNetworkProfile* np, const char* table_name)
	{
		np->m_profile->ReadStats(new SReader(this,np,table_name));
	}

	void SaveRecords(CGameNetworkProfile* np, const char* table_name)
	{
		if(m_numFree)
		{
			for(int i=0;i<m_array.size();++i)
				if(GetMetaData(i).free && !GetMetaData(i).locked && GetMetaData(i).recordId != -1)
				{
					np->m_profile->DeleteStats(new SDeleter(this, np, table_name));
					break;
				}
		}
		for(int i=0;i<m_array.size();++i)
			if(!GetMetaData(i).free && !GetMetaData(i).locked && GetMetaData(i).recordId == -1)
			{
				np->m_profile->WriteStats(new SWriter(this, np, table_name));
			}
	}

	std::vector<std::pair<T,SMetaData> >	m_array;
	int																		m_numFree;
};

void SStoredUserInfo::Serialize(struct IStoredSerialize* ser)
{
	ser->Serialize(m_kills,"Kills");
	ser->Serialize(m_deaths,"Deaths");
	ser->Serialize(m_shots,"Shots");
	ser->Serialize(m_hits,"Hits");
	ser->Serialize(m_played,"Played");
	ser->Serialize(m_mode,"FavoriteMode");
	ser->Serialize(m_map,"FavoriteMap");
	ser->Serialize(m_weapon,"FavoriteWeapon");
	ser->Serialize(m_vehicle,"FavoriteVehicle");
	ser->Serialize(m_suitmode,"FavoriteSuitMode");
}

struct CGameNetworkProfile::SUserInfoReader : public CStoredBase
{
	struct SReader : public CGameNetworkProfile::SStorageQuery, public IStatsReader
	{
		SReader(SUserInfoReader* r,CGameNetworkProfile* pr, const char* table_name, int id):SStorageQuery(pr),m_reader(r),m_id(-1),m_table(table_name),m_profile(id){}

		virtual const char* GetTableName()
		{
			return m_table;
		}
		virtual int         GetFieldsNum()
		{
			return m_reader->m_config.size();
		}
		virtual const char* GetFieldName(int i)
		{
			return m_reader->m_config[i].name;
		}
		virtual void        NextRecord(int id)
		{
			AddValue();
			m_id = id;
		}
		virtual void        OnValue(int field, const char* val)
		{
			SReadSerializer s(val,field);
			m_value.Serialize(&s);
		}
		virtual void        OnValue(int field, int val)
		{
			SReadSerializer s(val,field);
			m_value.Serialize(&s);
		}
		virtual void        OnValue(int field, float val)
		{
			SReadSerializer s(val,field);
			m_value.Serialize(&s);
		}
		virtual void        End(bool ok)
		{
			if(ok)
				AddValue();
			delete this;//we're done
		}
		void AddValue()
		{
			if(m_id != -1 && m_parent)
			{
				m_reader->OnValue(m_profile, m_value);
			}
		}
		SStoredUserInfo		m_value;
		int								m_id;
		SUserInfoReader*	m_reader;
		const char*				m_table;
		int								m_profile;
	};

	SUserInfoReader(CGameNetworkProfile *pr):m_profile(pr)
	{
		InitConfig<SStoredUserInfo>();
	}

	void ReadInfo(int profileId)
	{
		static string m_table;
		if(m_table.empty())
		{
			m_table.Format("PlayerStats_v%d",g_pGame->GetIGameFramework()->GetIGameStatsConfig()->GetStatsVersion());
		}
		m_profile->m_profile->ReadStats(profileId, new SReader(this, m_profile, m_table, profileId));
	}

	void OnValue(int id, SStoredUserInfo& info)
	{
		if(DEBUG_VERBOSE)
			CryLog("Info for %d read successfully - kills %d", id, info.m_kills);
		
		IGameStatsConfig* config = g_pGame->GetIGameFramework()->GetIGameStatsConfig();

		SUserStats stats;
		stats.m_accuracy = info.m_shots?(info.m_hits/float(info.m_shots)):0.0f;
		stats.m_deaths = info.m_deaths;
		stats.m_kills = info.m_kills;
		stats.m_played = info.m_played;
		stats.m_killsPerMinute = info.m_played?(info.m_kills/(float)info.m_played):0;

		if(int mod = config->GetCategoryMod("mode"))
		{
			stats.m_gameMode = config->GetValueNameByCode("mode",info.m_mode%mod);
		}

		if(int mod = config->GetCategoryMod("map"))
		{
			stats.m_map = config->GetValueNameByCode("map",info.m_map%mod);
		}

		if(int mod = config->GetCategoryMod("weapon"))
		{
			stats.m_weapon = config->GetValueNameByCode("weapon",info.m_weapon%mod);
		}
		
		if(int mod = config->GetCategoryMod("vehicle"))
		{
			stats.m_vehicle = config->GetValueNameByCode("vehicle",info.m_vehicle%mod);
		}

		if(int mod = config->GetCategoryMod("suit_mode"))
		{
			stats.m_suitMode = config->GetValueNameByCode("suit_mode",info.m_suitmode%mod);
		}

		m_profile->OnUserStats(id, stats, eUIS_backend);
	}
	CGameNetworkProfile *m_profile;
};


static TKeyValuePair<ENetworkProfileError,const char*>
gProfileErrors[] = {	{eNPE_ok,""},
											{eNPE_connectFailed,"@ui_menu_connectFailed"},
											{eNPE_disconnected,"@ui_menu_disconnected"},
											{eNPE_loginFailed,"@ui_menu_loginFailed"},
											{eNPE_loginTimeout,"@ui_menu_loginTimeout"},
											{eNPE_anotherLogin,"@ui_menu_anotherLogin"},
											{eNPE_nickTaken,"@ui_menu_nickTaken"},
											{eNPE_registerAccountError,"@ui_menu_registerAccountError"},
											{eNPE_registerGeneric,"@ui_menu_registerGeneric"},
											{eNPE_nickTooLong,"@ui_menu_nickTooLong"},
											{eNPE_nickFirstNumber,"@ui_menu_nickFirstNumber"},
											{eNPE_nickSlash,"@ui_menu_nickSlash"},
											{eNPE_nickFirstSpecial,"@ui_menu_nickFirstSpecial"},
											{eNPE_nickNoSpaces,"@ui_menu_nickNoSpaces"},
											{eNPE_nickEmpty,"@ui_menu_nickEmpty"},
											{eNPE_profileEmpty,"@ui_menu_profileEmpty"},
											{eNPE_passTooLong,"@ui_menu_passTooLong"},
											{eNPE_passEmpty,"@ui_menu_passEmpty"},
											{eNPE_mailTooLong,"@ui_menu_mailTooLong"},
											{eNPE_mailEmpty,"@ui_menu_mailEmpty"}
};


struct CGameNetworkProfile::SBuddies  : public INetworkProfileListener
{
  enum EQueryReason
  {
    eQR_addBuddy,
    eQR_showInfo,
    eQR_addIgnore,
    eQR_stopIgnore,
    eQR_getStats,
    eQR_buddyRequest,
  };

  struct SPendingOperation
  {
    SPendingOperation(){}
    SPendingOperation(EQueryReason t):m_id(0),m_type(t)
    {}
    string        m_nick;
    int           m_id;
    string        m_param;
    EQueryReason  m_type;
  };


  struct SBuddyRequest
  {
		SBuddyRequest(const string& n, const string& m):nick(n),message(m){}
    string nick;
    string message;
  };

  struct SIgnoredProfile
  {
    int id;
    string nick;

		void Serialize(IStoredSerialize* ser)
		{
			ser->Serialize(id,"profile");
			ser->Serialize(nick,"nick");
		}
  };

  typedef std::map<int, SBuddyRequest> TBuddyRequestsMap;

  SBuddies(CGameNetworkProfile* p):m_parent(p),m_textId(0),m_ui(0)
  {}

  void AddNick(const char* nick){}

  void UpdateFriend(int id, const char* nick, EUserStatus s, const char* location, bool foreign)
  {
    SChatUser *u = 0;
    for(int i=0;i<m_buddyList.size();++i)
    {
      if(m_buddyList[i].m_id == id)
      {
        u = &m_buddyList[i];
        break;
      }
    }
    bool added = false;
    if(!u)
    {
      m_buddyList.push_back(SChatUser());
      u = &m_buddyList.back();
      added = true;
    }
    u->m_nick = nick;
    u->m_status = s;
    u->m_id = id;
    u->m_location = location;
		u->m_foreign = foreign;
    //updated user with id
    if(m_ui)
    {
      if(added)
        m_ui->AddBuddy(*u);
      else
        m_ui->UpdateBuddy(*u);
    }
  }

  void RemoveFriend(int id)
  {
    for(int i=0;i<m_buddyList.size();++i)
    {
      if(m_buddyList[i].m_id == id)
      {
        //remove
        if(m_ui)
          m_ui->RemoveBuddy(id);
        m_buddyList.erase(m_buddyList.begin()+i);
        break;
      }
    }
  }

  void AddIgnore(int id, const char* nick)
  {
    SIgnoredProfile ip;
    ip.id = id;
    ip.nick = nick;
    m_ignoreList.push(ip);

    SaveIgnoreList();

    if(m_ui)
    {
      SChatUser u;
      u.m_id = id;
      u.m_nick = nick;
      m_ui->AddIgnore(u);
    }
  }

  void RemoveIgnore(int id)
  {
    for(int i=0;i<m_ignoreList.size();++i)
    {
      if(m_ignoreList[i].id == id)
      {
        m_ignoreList.erase(i);
        if(m_ui)
          m_ui->RemoveIgnore(id);
        SaveIgnoreList();
        break;
      }
    }
  }

  void OnFriendRequest(int id, const char* message)
  {
		if(IsIgnoring(id))
		{
      m_parent->m_profile->AuthFriend(id,false);
			return;
		}

		SPendingOperation op(eQR_buddyRequest);
		op.m_id = id;
		op.m_param = message;
		m_operations.push_back(op);
		//if(!CheckId(id)) - not checking because there is a lag about having buddy added - omitted for possibility of auto add
		m_parent->m_profile->GetUserNick(id);
  }

  void OnMessage(int id, const char* message)
  {
    if(IsIgnoring(id))
      return;
      
    TChatTextMap::iterator it = m_buddyChats.find(id);
    if(it==m_buddyChats.end())
      it = m_buddyChats.insert(std::make_pair(id,SChatText())).first;
    
    m_textId++;
    it->second.AddText(message,m_textId);
    if(m_ui)
      m_ui->OnMessage(id,message);

    if(m_parent->m_hub->IsIngame() && g_pGame->GetCVars()->g_buddyMessagesIngame)
    {
      if(CHUDTextChat *pChat = SAFE_HUD_FUNC_RET(GetMPChat()))
      {
        const char* name = "";
        for(int i=0;i<m_buddyList.size();++i)
        {
          if(m_buddyList[i].m_id == id)
          {
            name = m_buddyList[i].m_nick.c_str();
            break;
          }
        }
        pChat->AddChatMessage(string().Format("From [%s] :",name), message, 0, false);
      }
    }
  }

  void LoginResult(ENetworkProfileError res, const char* descr, int id, const char* nick)
  {
    if(res == eNPE_ok)
    {
      m_parent->OnLoggedIn(id, nick);
    }
    else
    {
			m_parent->m_loggingIn = false;
			
			const char* err = VALUE_BY_KEY(res,gProfileErrors);
			m_parent->m_hub->OnLoginFailed(err);
    }
  }

  void OnError(ENetworkProfileError res, const char* descr)
  {
		bool logoff = false;
		switch(res)
		{
			case eNPE_connectFailed:
			case eNPE_disconnected:		
			case eNPE_anotherLogin:			
				{
					const char* err = VALUE_BY_KEY(res,gProfileErrors);
					m_parent->m_hub->ShowError(err,true);
					logoff = true;
				}
				break;
		}	

		if(m_parent->m_loggingIn && m_parent->m_hub)
			m_parent->m_hub->CloseLoadingDlg();

		if(logoff && m_parent->m_hub)
			m_parent->m_hub->DoLogoff();
  }

  void OnProfileInfo(int id, const char* key, const char* value)
  {
    TUserInfoMap::iterator it = m_infoCache.find(id);
    if(it == m_infoCache.end())
    {
      it = m_infoCache.insert(std::make_pair(id,SUserInfo())).first;
    }
    SUserInfo &u = it->second;
    EUserInfoKey k = KEY_BY_VALUE(string(key),gUserInfoKeys);
    switch(k)
    {
    case eUIK_nick:
      u.m_nick = value;
      break;
		case eUIK_namespace:
			u.m_foreignName = !strcmp(value,"foreign");
			break;
    case eUIK_country:
      u.m_country = value;
			if(id == m_parent->m_profileId)
				m_parent->m_country = value;
      break;
    default:;
    }		
  }

  virtual void OnProfileComplete(int id)
  {
    TUserInfoMap::iterator iit = m_infoCache.find(id);
    if(iit == m_infoCache.end())
      return;
    if(m_ui)
      m_ui->ProfileInfo(id,iit->second);
  }

  virtual void OnSearchResult(int id, const char* nick)
  {
    if(m_ui)
      m_ui->SearchResult(id,nick);
  }

  virtual void OnSearchComplete()
  {
    if(m_ui && !m_uisearch.empty())
    {
      m_uisearch.resize(0);
      m_ui->SearchCompleted();
    }
  }

  virtual void OnUserId(const char* nick, int id)
  {
    for(int i=0;i<m_operations.size();++i)
    {
      if(m_operations[i].m_nick == nick)
      {
        m_operations[i].m_id = id;
        ExecuteOperation(i);
        break;
      }
    }
  }
  
  virtual void OnUserNick(int id, const char* nick, bool foreign_name)
  {
		if(m_infoCache.find(id) == m_infoCache.end())
		{
			OnProfileInfo(id,"namespace",foreign_name?"foreign":"home");
			OnProfileInfo(id,"nick",nick);
		}

    for(int i=0;i<m_operations.size();++i)
    {
      if(m_operations[i].m_id == id && m_operations[i].m_nick.empty())
      {
        m_operations[i].m_nick = nick;
        ExecuteOperation(i);
        break;
      }
    }
  }

	bool GetUserInfo(int id, SUserInfo& info)
	{
		TUserInfoMap::iterator it = m_infoCache.find(id);
		if(it != m_infoCache.end())
		{
			if(!it->second.m_basic)//we have extended info
			{
				if(it->second.m_expires < gEnv->pTimer->GetFrameStartTime())
				{
					if(DEBUG_VERBOSE)
						CryLog("Stats for %d expired on %d",it->first, uint32(it->second.m_expires.GetValue()&0xFFFFFFFF));
					it->second.m_basic = true;//clear it
				}
				else
				{
					info = it->second;
					return true;
				}
			}			
		}
		return false;
	}

	bool GetUserInfo(const char* nick, SUserInfo& info, int& id)
	{
		for(TUserInfoMap::iterator it = m_infoCache.begin(),eit = m_infoCache.end(); it!=eit; ++it)
		{
			if(it->second.m_nick == nick && !it->second.m_foreignName)
			{
				if(!it->second.m_basic)//we have extended info
				{
					if(it->second.m_expires < gEnv->pTimer->GetFrameStartTime())
					{
						if(DEBUG_VERBOSE)
							CryLog("Stats for %d expired on %d",it->first, uint32(it->second.m_expires.GetValue()&0xFFFFFFFF));
						it->second.m_basic = true;//clear it
					}
					else
					{
						id = it->first;
						info = it->second;
						return true;
					}
				}
			}
		}
		return false;
	}

	void RetrievePasswordResult(bool ok)
	{
		if(m_parent->m_hub)
			m_parent->m_hub->ShowRetrivePasswordResult(ok);
	}

  void ExecuteOperation(int idx)
  {
    assert(0 <= idx && idx < m_operations.size());
    SPendingOperation &o = m_operations[idx];
    assert(!o.m_nick.empty() && o.m_id);

    switch(o.m_type)
    {
    case eQR_addBuddy:
      m_parent->m_profile->AddFriend(o.m_id,o.m_param);
      break;
    case eQR_showInfo:
      m_parent->m_profile->GetProfileInfo(o.m_id);
      break;
    case eQR_addIgnore:
      AddIgnore(o.m_id,o.m_nick);
      break;
    case eQR_stopIgnore:
      RemoveIgnore(o.m_id);
      break;
    case eQR_getStats:
      m_parent->m_profile->GetProfileInfo(o.m_id);
			m_parent->m_infoReader->ReadInfo(o.m_id);
      break;
    case eQR_buddyRequest:
			{
				if(!strncmp(o.m_param.c_str(), AUTO_INVITE_TEXT, strlen(AUTO_INVITE_TEXT)))//this may be autogenerated
				{
					bool buddy = false;
					for(int i=0;i<m_buddyList.size();++i)
						if(m_buddyList[i].m_id == o.m_id)
						{
							buddy = true;
							break;
						}
						if(buddy)
						{
							m_parent->m_profile->AuthFriend(o.m_id,true);
							return;
						}
						else
						{
							o.m_param = "@ui_menu_auto_invite";								
						}
				}
				m_requests.insert(std::make_pair(o.m_id,SBuddyRequest(o.m_nick,o.m_param)));
				if(m_ui)
	        m_ui->OnAuthRequest(o.m_id,o.m_nick,o.m_param);
			}
      break;
    }
    m_operations.erase(m_operations.begin()+idx);
  }

  void Accept(int id, bool accept)
  {
    TBuddyRequestsMap::iterator it = m_requests.find(id);
    if(it!= m_requests.end())  
    {
      m_parent->m_profile->AuthFriend(it->first,accept);
      m_requests.erase(it);
    }
  }

  void Request(const char* nick, const char* reason)
  {
    SPendingOperation op(eQR_addBuddy);
    op.m_nick = nick;
    op.m_param = reason;
    m_operations.push_back(op);
    if(!CheckNick(nick))
      m_parent->m_profile->GetUserId(nick);
  }

  void Request(int id, const char* reason)
  {
    m_parent->m_profile->AddFriend(id,reason);
  }

  bool CheckNick(const char* nick)
  {
    for(TUserInfoMap::iterator it = m_infoCache.begin();it!=m_infoCache.end();++it)
    {
      if(it->second.m_nick == nick)
      {
        OnUserId(nick,it->first);
        return true;
      }
    }
    return false;
  }

  bool CheckId(int id)
  {
    TUserInfoMap::iterator it = m_infoCache.find(id);
    if(it!=m_infoCache.end())
    {
      OnUserNick(id,it->second.m_nick,it->second.m_foreignName);
      return true;
    }
    return false;
  }

  void Remove(const char* nick)
  {
    for(int i=0;i<m_buddyList.size();++i)
    {
      if(m_buddyList[i].m_nick == nick)
      {
        m_parent->m_profile->RemoveFriend(m_buddyList[i].m_id,false);
        break;
      }
    }
  }

  void Ignore(const char* nick)
  {
		if(!CanIgnore(nick))
			return;

    Remove(nick);

    SPendingOperation op(eQR_addIgnore);
    op.m_nick = nick;
    m_operations.push_back(op);
    if(!CheckNick(nick))
      m_parent->m_profile->GetUserId(nick);
  }
  
  void QueryUserInfo(const char* nick)
  {
    SPendingOperation op(eQR_getStats);
    op.m_nick = nick;
    m_operations.push_back(op);
    if(!CheckNick(nick))
      m_parent->m_profile->GetUserId(nick);
  }

  void QueryUserInfo(int id)
  {
    SPendingOperation op(eQR_getStats);
    op.m_id = id;
    m_operations.push_back(op);
    if(!CheckId(id))
      m_parent->m_profile->GetUserNick(id);
  }

  void StopIgnore(const char* nick)
  {
    SPendingOperation op(eQR_stopIgnore);
    op.m_nick = nick;
    m_operations.push_back(op);
    if(!CheckNick(nick))
      m_parent->m_profile->GetUserId(nick);
  }

  bool IsIgnoring(int id)
  {
    for(int i=0;i<m_ignoreList.size();++i)
    {
      if(m_ignoreList[i].id == id)
        return true;
    }
    return false;
  }

  bool IsIgnoring(const char* nick)
  {
    for(int i=0;i<m_ignoreList.size();++i)
    {
      if(m_ignoreList[i].nick == nick)
        return true;
    }
    return false;
  }

  void SendUserMessage(int id, const char* message)
  {
    m_parent->m_profile->SendFriendMessage(id,message);
  }

	void OnUserStats(int id, const SUserStats& stats, EUserInfoSource src)
	{
		TUserInfoMap::iterator it = m_infoCache.find(id);
		if(it != m_infoCache.end())
		{
			it->second.m_stats = stats;
			it->second.m_source = src;
			it->second.m_basic = false;
			if(src == eUIS_backend)
				it->second.m_expires = gEnv->pTimer->GetFrameStartTime() + 10*60.0f;//10 minutes cache
			else
				it->second.m_expires = gEnv->pTimer->GetFrameStartTime() + 20*60.0f;//20 minutes for chat users
	
			if(DEBUG_VERBOSE)
				CryLog("Stats for %d cached on %d",it->first, uint32(it->second.m_expires.GetValue()&0xFFFFFFFF));
			if(m_ui)
				m_ui->ProfileInfo(id, it->second);
		}
		else
		{
			it = m_infoCache.insert(std::make_pair(id,SUserInfo())).first;
			it->second.m_stats = stats;
			if(m_ui)
				m_ui->ProfileInfo(id, it->second);
		}
	}

  void ReadIgnoreList()
  {
		m_ignoreList.LoadRecords(m_parent,"ignorelist");
  }

  void SaveIgnoreList()
  {
		m_ignoreList.SaveRecords(m_parent,"ignorelist");
  }

  bool CanInvite(const char* nick)
  {
    if(!strcmp(nick,"ChatMonitor"))
      return false;
    for(TUserInfoMap::iterator it = m_infoCache.begin();it!=m_infoCache.end();++it)
    {
      if(it->second.m_nick == nick)
      {
        return CanInvite(it->first);
      }
    }
    return true;
  }

  bool CanInvite(int id)
  {
    for(int i=0;i<m_buddyList.size();++i)
      if(m_buddyList[i].m_id == id)
        return false;

    return true;
  }

  bool CanIgnore(const char* nick)
  {
    for(int i=0;i<m_ignoreList.size();++i)
    {
      if(m_ignoreList[i].nick == nick)
      {
        return false;
      }
    }
    return true;
  }

  bool CanIgnore(int id)
  {
    for(int i=0;i<m_ignoreList.size();++i)
    {
      if(m_ignoreList[i].id == id)
      {
        return false;
      }
    }
    return true;
  }

  void UIActivated(INProfileUI* ui)
  {
    m_ui = ui;
    if(!m_ui)
      return;

    for(int i=0;i<m_buddyList.size();++i)
    {
      m_ui->AddBuddy(m_buddyList[i]);
    }
    for(int i=0;i<m_ignoreList.size();++i)
    {
      SChatUser u;
      u.m_id = m_ignoreList[i].id;
      u.m_nick = m_ignoreList[i].nick;
      m_ui->AddIgnore(u);
    }
  }

  std::vector<SChatUser>        m_buddyList;
  //std::vector<SIgnoredProfile>  m_ignoreList;
	TStoredArray<SIgnoredProfile> m_ignoreList;
  TUserInfoMap                  m_infoCache;

  TBuddyRequestsMap       m_requests;

  TChatTextMap              m_buddyChats;
  std::auto_ptr<SChatText>  m_globalChat;

  std::vector<SPendingOperation>  m_operations;

  int                   m_textId;
  CGameNetworkProfile*  m_parent;
  INProfileUI*          m_ui;

  string                m_uisearch;
};

void SStoredServer::Serialize(struct IStoredSerialize* ser)
{
	ser->Serialize(ip,"ip");
	ser->Serialize(port,"port");
}

struct CGameNetworkProfile::SStoredServerLists
{
  SStoredServerLists(CGameNetworkProfile* p):
  m_parent(p)
  {

  }

  ~SStoredServerLists()
  {
  }

  void ReadLists()
  {
		m_favorites.LoadRecords(m_parent,"favorites");
		m_recent.LoadRecords(m_parent,"recent");
  }

	void SaveFavoritesList()
	{
		m_favorites.SaveRecords(m_parent,"favorites");
	}

	void SaveRecentList()
	{
		m_recent.SaveRecords(m_parent,"recent");
	}

  void UIActivated(INProfileUI* ui)
  {
    m_ui = ui;
    if(!m_ui)
      return;
    for(int i=0;i<m_favorites.size();++i)
      m_ui->AddFavoriteServer(m_favorites[i].ip,m_favorites[i].port);
    for(int i=0;i<m_recent.size();++i)
      m_ui->AddRecentServer(m_recent[i].ip,m_recent[i].port);
  }

  void AddFavoriteServer(uint ip, ushort port)
  {
    SStoredServer s;
    s.ip = ip;
    s.port = port;
    m_favorites.push(s);
		SaveFavoritesList();
  }

  void RemoveFavoriteServer(uint ip, ushort port)
  {
    for(int i=0;i<m_favorites.size();++i)
    {
      if(m_favorites[i].ip==ip && m_favorites[i].port==port)
      {
        m_favorites.erase(i);
				SaveFavoritesList();
        break;
      }
    }
  }

  void AddRecentServer(uint ip, ushort port)
  {
    SStoredServer s;
    s.ip = ip;
    s.port = port;
    m_recent.push(s);
		if(m_recent.size()>20)
			m_recent.erase(0);
		SaveRecentList();
  }

  TStoredArray<SStoredServer>	m_favorites;
  TStoredArray<SStoredServer>	m_recent;
  INProfileUI*								m_ui;

  CGameNetworkProfile*				m_parent;
};

CGameNetworkProfile::CGameNetworkProfile(CMPHub* hub):
m_hub(hub),m_loggingIn(false),m_profileId(-1)
{
  INetworkService *serv = gEnv->pNetwork->GetService("GameSpy");
  if(serv)
  {
    m_profile = serv->GetNetworkProfile();
		m_infoReader.reset(new SUserInfoReader(this));
  }
  else
  {
    m_profile = 0;
  }
}

CGameNetworkProfile::~CGameNetworkProfile()
{
  if(m_profile)
     m_profile->RemoveListener(m_buddies.get());
}

void CGameNetworkProfile::Login(const char* login, const char* password)
{
  m_login = login;
	m_password = password;
	m_loggingIn = true;
	m_stroredServers.reset(new SStoredServerLists(this));
  m_buddies.reset(new SBuddies(this));
  m_profile->AddListener(m_buddies.get());
  m_profile->Login(login,password);
}

void CGameNetworkProfile::LoginProfile(const char* email, const char* password, const char* profile)
{
	m_login = profile;
	m_password = password;
	m_loggingIn = true;
	m_buddies.reset(new SBuddies(this));
	m_stroredServers.reset(new SStoredServerLists(this));
	m_profile->AddListener(m_buddies.get());
	m_profile->LoginProfile(email,password,profile);
}

void CGameNetworkProfile::Register(const char* login, const char* email, const char* pass, const char* country, SRegisterDayOfBirth dob)
{
	assert(m_profile);
  m_login = login;
	m_password = pass;
	m_loggingIn = true;
  m_buddies.reset(new SBuddies(this));
  m_stroredServers.reset(new SStoredServerLists(this));
  m_profile->AddListener(m_buddies.get());
  m_profile->Register(login,email,pass,country, dob);
}

void CGameNetworkProfile::Logoff()
{
  assert(m_profile);

  //cancel pending storage queries..
  //TODO: wait for important queries to finish.
  CleanUpQueries();

	m_loggingIn = false;
  m_profile->Logoff();
  m_profile->RemoveListener(m_buddies.get());
  m_buddies.reset(0);
  m_stroredServers.reset(0);
}

void CGameNetworkProfile::AcceptBuddy(int id, bool accept)
{
  m_buddies->Accept(id,accept);
}

void CGameNetworkProfile::RequestBuddy(const char* nick, const char* reason)
{
  m_buddies->Request(nick, reason);
}

void CGameNetworkProfile::RequestBuddy(int id, const char* reason)
{
  m_buddies->Request(id,reason);
}

void CGameNetworkProfile::RemoveBuddy(const char* nick)
{
  m_buddies->Remove(nick);
}

void CGameNetworkProfile::AddIgnore(const char* nick)
{
  m_buddies->Ignore(nick);
}

void CGameNetworkProfile::StopIgnore(const char* nick)
{
  m_buddies->StopIgnore(nick);
}

void CGameNetworkProfile::QueryUserInfo(const char* nick)
{
  m_buddies->QueryUserInfo(nick);
}

void CGameNetworkProfile::QueryUserInfo(int id)
{
	//m_infoReader->ReadInfo(id);
  m_buddies->QueryUserInfo(id);
}

void CGameNetworkProfile::SendBuddyMessage(int id, const char* message)
{
  m_buddies->SendUserMessage(id,message);
}

void CGameNetworkProfile::InitUI(INProfileUI* a)
{
  if(m_buddies.get())
    m_buddies->UIActivated(a);
  if(m_stroredServers.get())
    m_stroredServers->UIActivated(a);
}

void CGameNetworkProfile::DestroyUI()
{
  if(m_buddies.get())
    m_buddies->m_ui = 0;
}

void CGameNetworkProfile::AddFavoriteServer(uint ip, ushort port)
{
  m_stroredServers->AddFavoriteServer(ip,port);
}

void CGameNetworkProfile::RemoveFavoriteServer(uint ip, ushort port)
{
  m_stroredServers->RemoveFavoriteServer(ip,port);
}

void CGameNetworkProfile::AddRecentServer(uint ip, ushort port)
{
  m_stroredServers->AddRecentServer(ip,port);
}

bool CGameNetworkProfile::IsLoggedIn()const
{
  return m_profile->IsLoggedIn() && !m_loggingIn;
}

bool CGameNetworkProfile::IsLoggingIn()const
{
	return m_loggingIn;
}

const char* CGameNetworkProfile::GetLogin()const
{
	return m_login.c_str();
}

const char* CGameNetworkProfile::GetPassword()const
{
	return m_password.c_str();
}

const char* CGameNetworkProfile::GetCountry()const
{
	return m_country.c_str();
}

const int CGameNetworkProfile::GetProfileId()const
{
	return m_profileId;
}

void CGameNetworkProfile::SearchUsers(const char* nick)
{
  m_buddies->m_uisearch=nick;
	if(m_profile)
		m_profile->SearchFriends(nick);
}

bool CGameNetworkProfile::CanInvite(const char* nick)
{
  if(!stricmp(m_login.c_str(),nick))
    return false;
  return m_buddies->CanInvite(nick);
}

bool CGameNetworkProfile::CanIgnore(const char* nick)
{
  if(!stricmp(m_login.c_str(),nick))
    return false;
  return m_buddies->CanIgnore(nick);
}

bool CGameNetworkProfile::CanInvite(int id)
{
  if(m_profileId == id)
    return false;
  return m_buddies->CanInvite(id);
}

bool CGameNetworkProfile::CanIgnore(int id)
{
  if(m_profileId == id)
    return false;
  return m_buddies->CanIgnore(id);
}

bool CGameNetworkProfile::GetUserInfo(int id, SUserInfo& info)
{
	return m_buddies->GetUserInfo(id, info);
}

bool CGameNetworkProfile::GetUserInfo(const char* nick, SUserInfo& info, int& id)
{
	return m_buddies->GetUserInfo(nick, info, id);
}

bool CGameNetworkProfile::IsIgnored(const char* nick)
{
	if(!stricmp(m_login.c_str(),nick))
		return false;
	return m_buddies->IsIgnoring(nick);
}

void CGameNetworkProfile::SetPlayingStatus(uint ip, ushort port, ushort publicport, const char* game_type)
{
  string loc;
  loc.Format("%d.%d.%d.%d:%d/?type=game&queryport=%d&game=%s",ip&0xFF,(ip>>8)&0xFF,(ip>>16)&0xFF,(ip>>24)&0xFF,port,publicport,game_type);

  if(m_profile)
		m_profile->SetStatus(eUS_playing,loc);
}

void CGameNetworkProfile::SetChattingStatus()
{
	assert(m_profile);
	if(m_profile)
		m_profile->SetStatus(eUS_chatting,"/?chatting");
}

void CGameNetworkProfile::OnUserStats(int id, const SUserStats& stats, EUserInfoSource src, const char* country)
{
	m_buddies->OnUserStats(id, stats, src);
	if(strlen(country))
		m_buddies->OnProfileInfo(id, "country", country);
	if(id == m_profileId)//my stats
		m_stats = stats;
}

void CGameNetworkProfile::OnUserNick(int profile, const char* user)
{
	m_buddies->OnUserNick(profile, user, false);
}

void CGameNetworkProfile::OnLoggedIn(int id, const char* nick)
{
  m_profileId = id;
  m_login = nick;

  m_stroredServers->ReadLists();
	m_buddies->ReadIgnoreList();
	m_infoReader->ReadInfo(m_profileId);

  if(m_profile)
		m_profile->SetStatus(eUS_online,"");
}

void CGameNetworkProfile::OnEndQuery()
{
	if(m_queries.empty() && m_loggingIn)
	{
		m_loggingIn = false;
		m_hub->OnLoginSuccess(m_login);
	}
}

void CGameNetworkProfile::RetrievePassword(const char *email)
{
	if(m_profile)
	{
		m_buddies.reset(new SBuddies(this));
		m_profile->AddListener(m_buddies.get());
		m_profile->RetrievePassword(email);
	}
}

void CGameNetworkProfile::CleanUpQueries()
{
  for(int i=0;i<m_queries.size();++i)
    m_queries[i]->m_parent = 0;
}

bool CGameNetworkProfile::IsDead() const
{
	return m_profile == 0;
}

const SUserStats&	CGameNetworkProfile::GetMyStats()const
{
	return m_stats;
}

bool CGameNetworkProfile::GetFavoriteServers(std::vector<SStoredServer>& svr_list)
{
	if(m_profile && m_profile->IsLoggedIn())
	{
		for(int i=0;i<m_stroredServers->m_favorites.size();++i)
			svr_list.push_back(m_stroredServers->m_favorites[i]);	
		return true;
	}
	return false;
}