/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Multiplayer lobby

-------------------------------------------------------------------------
History:
- 02/6/2006: Created by Stas Spivakov

*************************************************************************/
#include "StdAfx.h"
#include "MPLobbyUI.h"
#include "Game.h"
#include "OptionsManager.h"
#include "GameNetworkProfile.h"
#include "GameCVars.h"

static const char* MPPath = "_root.Root.MainMenu.MultiPlayer.\0";

static TKeyValuePair<ESortColumn,const char*>
gSortColumnNames[] = {
                      {eSC_none,""},
                      {eSC_name,"Header_ServerName"},
                      {eSC_ping,"Header_Ping"},
                      {eSC_players,"Header_Players"},
                      {eSC_map,"Header_MapName"},
                      {eSC_mode,"Header_GameMode"},
                      {eSC_private,"Header_Lock"},
                      {eSC_favorite,"Header_Favorites"},
                      {eSC_official,"Header_Rank"},
                      {eSC_anticheat,"Header_Punkbuster"}
                     };
struct CMPLobbyUI::SServerFilter
{
  SServerFilter(CMPLobbyUI* p)
    : m_parent(p),
      m_on(false),
      m_minping(0),
      m_notfull(false),
      m_notempty(false),
      m_notprivate(false),
      m_notcustomized(false),
      m_autoteambalance(false),
      m_anticheat(false),
      m_friendlyfire(false),
      m_gamepadsonly(false),
      m_novoicecomms(false),
      m_dedicated(false),
      m_dx10(false)
  {
		if(g_pGame->GetOptions())
		{
			g_pGame->GetOptions()->GetProfileValue("Filter.Enabled",m_on);
			g_pGame->GetOptions()->GetProfileValue("Filter.GameMode",m_gamemode);
			g_pGame->GetOptions()->GetProfileValue("Filter.MapName",m_mapname);
			g_pGame->GetOptions()->GetProfileValue("Filter.MinPing",m_minping);
			g_pGame->GetOptions()->GetProfileValue("Filter.NotFull",m_notfull);
			g_pGame->GetOptions()->GetProfileValue("Filter.NotEmpty",m_notempty);
			g_pGame->GetOptions()->GetProfileValue("Filter.NotPrivate",m_notprivate);
			g_pGame->GetOptions()->GetProfileValue("Filter.NotCustomized",m_notcustomized);
			g_pGame->GetOptions()->GetProfileValue("Filter.AutoTeam",m_autoteambalance);
			g_pGame->GetOptions()->GetProfileValue("Filter.AntiCheat",m_anticheat);
			g_pGame->GetOptions()->GetProfileValue("Filter.FriendlyFire",m_friendlyfire);
			g_pGame->GetOptions()->GetProfileValue("Filter.GamePadsOnly",m_gamepadsonly);
			g_pGame->GetOptions()->GetProfileValue("Filter.NoVoiceComm",m_novoicecomms);
			g_pGame->GetOptions()->GetProfileValue("Filter.Dedicated",m_dedicated);
			g_pGame->GetOptions()->GetProfileValue("Filter.DX10",m_dx10);
		}
  }

	~SServerFilter()
	{
		g_pGame->GetOptions()->SaveValueToProfile("Filter.Enabled",m_on);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.GameMode",m_gamemode);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.MapName",m_mapname);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.MinPing",m_minping);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.NotFull",m_notfull);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.NotEmpty",m_notempty);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.NotPrivate",m_notprivate);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.NotCustomized",m_notcustomized);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.AutoTeam",m_autoteambalance);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.AntiCheat",m_anticheat);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.FriendlyFire",m_friendlyfire);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.GamePadsOnly",m_gamepadsonly);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.NoVoiceComm",m_novoicecomms);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.Dedicated",m_dedicated);
		g_pGame->GetOptions()->SaveValueToProfile("Filter.DX10",m_dx10);
		g_pGame->GetOptions()->SaveProfile();
	}

  bool  IsEnabled()const
  {
    return m_on;
  }

  bool  Filter(const SServerInfo& i)const
  {
		if( !m_on )
			return true;
    if( !m_gamemode.empty() && i.m_gameType != m_gamemode)
      return false;
    if( !m_mapname.empty())
    {
      static std::string a;
      a.resize(i.m_mapName.size());
      for(int k=0;k<a.size();++k)
        a[k] = char(tolower(i.m_mapName[k]));
      const char* f = strstr(a.c_str(),m_mapname.c_str());
      if(f==0)
        return false;
    }
    if( m_minping!=0 && i.m_ping > m_minping )
      return false;
    if( m_notfull && i.m_maxPlayers == i.m_numPlayers)
      return false;
    if( m_notempty && i.m_numPlayers == 0)
      return false;
    if( m_notprivate && i.m_private )
      return false;
    //if( m_notcustomized && i.m_custom )
    //  return false;
    //if( m_autoteambalance && i.m_autoteambalance )
    //  return false;
    if( m_anticheat && !i.m_anticheat )
      return false;
    if( m_friendlyfire && !i.m_friendlyfire )
      return false;
    if( m_gamepadsonly && !i.m_gamepadsonly )
      return false;
    if( m_novoicecomms && i.m_voicecomm )
      return false;
    if( m_dedicated && !i.m_dedicated )
      return false;
    if( m_dx10 && !i.m_dx10 )
      return false;
    return true;
  }

  bool HandleFSCommand(EGsUiCommand cmd, const char* pArgs);


	void UpdateUI()
	{
		m_parent->m_cmd = MPPath;
		m_parent->m_cmd += "GameLobby_M.GameLobby.LeftLobby.setUseFilters";
		SFlashVarValue arg[1] = {m_on};
		m_parent->m_player->Invoke(m_parent->m_cmd,arg,1);

		//setFilters = function(gameMode, mapName, ping, notFull, notEmpty, noPassword, autoTeam, antiCheat, friendlyFire, GamepadsOnly, noVoiceComm, dediServer, directX)
		SFlashVarValue args[] = 
		{
			SFlashVarValue(m_gamemode.empty()?"all":m_gamemode.c_str()),
			SFlashVarValue(m_mapname),
			SFlashVarValue(m_minping),
			SFlashVarValue(m_notfull),
			SFlashVarValue(m_notempty),
			SFlashVarValue(m_notprivate),
			SFlashVarValue(m_autoteambalance),
			SFlashVarValue(m_anticheat),
			SFlashVarValue(m_friendlyfire),
			SFlashVarValue(m_gamepadsonly),
			SFlashVarValue(m_novoicecomms),
			SFlashVarValue(m_dedicated),
			SFlashVarValue(m_dx10)
		};
		m_parent->m_cmd = MPPath;
		m_parent->m_cmd += "GameLobby_M.GameLobby.LeftLobby.setFilters";
		m_parent->m_player->Invoke(m_parent->m_cmd,args,sizeof(args)/sizeof(args[0]));
	}

	void EnableFilters(bool enable)
	{
		if(m_on != enable)
		{
			m_on = enable;
			m_parent->m_cmd = MPPath;
			m_parent->m_cmd += "GameLobby_M.GameLobby.LeftLobby.setUseFilters";
			SFlashVarValue arg[1] = {enable};
			m_parent->m_player->Invoke(m_parent->m_cmd,arg,1);
		}
	}

  CMPLobbyUI *m_parent;
  bool      m_on;
  string    m_gamemode;
  string    m_mapname;
  int       m_minping;
  bool      m_notfull;
  bool      m_notempty;
  bool      m_notprivate;
  bool      m_notcustomized;
  bool      m_autoteambalance;
  bool      m_anticheat;
  bool      m_friendlyfire;
  bool      m_gamepadsonly;
  bool      m_novoicecomms;
  bool      m_dedicated;
  bool      m_dx10;

};

struct SMPServerList
{
  typedef std::vector<CMPLobbyUI::SServerInfo> ServerInfoVector;
  typedef std::vector<int> DisplayedServersVector;

  SMPServerList():
    m_startIndex(0),
    m_visibleCount(13),
    m_total(0),
    m_done(0),
    m_sortcolumn(eSC_none),
    m_sorttype(eST_ascending),
    m_dirty(false),
    m_selectedServer(-1),
    m_displayMode(0)
  {

  }

  void AddToVisible(int idx)
  {
    m_all.push_back(idx);
  }

  void RemoveFromVisible(int idx)
  {
    stl::find_and_erase(m_all,idx);
  }

  void AddToFavorites(int idx)
  {
    m_favorites.push_back(idx);
    if(m_displayMode==1)
    {
      AddToVisible(idx);
    }
  }

  void AddToRecent(int idx)
  {
    m_recent.push_back(idx);
    if(m_displayMode==2)
    {
      AddToVisible(idx);
    }
  }

  void RemoveFromFavorites(int idx)
  {
    stl::find_and_erase(m_favorites,idx);
    if(m_displayMode==1)
    {
      RemoveFromVisible(idx);
      if(m_selectedServer == idx)
        ClearSelection();
    }
  }

  void RemoveFromRecent(int idx)
  {
    stl::find_and_erase(m_recent,idx);
    if(m_displayMode==2)
    {
      RemoveFromVisible(idx);
      if(m_selectedServer == idx)
        ClearSelection();
    }
  }

  void AddServer(const CMPLobbyUI::SServerInfo& srv)
  {
    int idx = m_allServers.size();
    m_allServers.push_back(srv);

    if(srv.m_favorite)
    {
      AddToFavorites(idx);
    }
    if(srv.m_recent)
    {
      AddToRecent(idx);
    }
    if(m_displayMode == 0)
      AddToVisible(idx);
    m_dirty = true;
  }

  void UpdateServer(const CMPLobbyUI::SServerInfo& srv)
  {
    int idx = GetServerIdxById(srv.m_serverId);
    if(idx!=-1)
    {
      int ping = m_allServers[idx].m_ping;
      //if(srv.m_ping != 9999)
      ping = srv.m_ping;
      
      if(srv.m_favorite != m_allServers[idx].m_favorite)
      {
        if(!srv.m_favorite)
          RemoveFromFavorites(idx);
        else
          AddToFavorites(idx);
      }

      if(srv.m_recent != m_allServers[idx].m_recent)
      {
        if(!srv.m_recent)
          RemoveFromRecent(idx);
        else
          AddToRecent(idx);
      }
    
      m_allServers[idx] = srv;
      
      m_allServers[idx].m_ping = ping;
    }
  }

  void UpdatePing(int id, int ping)
  {
    int idx = GetServerIdxById(id);
    if(idx != -1)
    {
      m_allServers[idx].m_ping = ping;
    }
  }

  void RemoveServer(const int id)
  {
    int idx = GetServerIdxById(id);
    if(idx != -1)
    {
      m_allServers.erase(m_allServers.begin()+idx);
      RemoveFromVisible(idx);
      stl::find_and_erase(m_favorites,idx);
      stl::find_and_erase(m_recent,idx);
    }
  }

  int GetServerIdxById(int id)
  {
    for(uint32 i=0;i<m_allServers.size();i++)
      if(m_allServers[i].m_serverId == id)
        return i;
    return -1;
  }

  bool    SetScrollPos(double fr)
  {
    int pos = (int)(fr*(m_all.size()-m_visibleCount));
    return SetStartIndex(pos);
  }

  bool    SetStartIndex(int i)
  {
    int set_idx = i;
    if(set_idx+m_visibleCount>m_all.size())
      set_idx = m_all.size()-m_visibleCount;

    if(set_idx<0)
      set_idx = 0;

    if(set_idx!= m_startIndex)
    {
      m_startIndex = set_idx;
      return true;
    }

    return false;
  }

  void    SetVisibleCount(int c)
  {
    if(m_startIndex + c > m_all.size())
      m_startIndex = max(int(m_all.size())-c,0);
    m_visibleCount = c;
  }

  void Clear()
  {
    m_recent.resize(0);
    m_favorites.resize(0);
    m_all.resize(0);
    m_allServers.resize(0);
    m_startIndex = 0;
    m_selectedServer = -1;
  }

  void SetDisplayMode(int mode)
  {
    m_startIndex = 0;
    switch(mode)
    {
    case 0:
      m_all.resize(m_allServers.size());
      for(int i=0;i<m_allServers.size();++i)
        m_all[i] = i;
      m_dirty = true;
      break;
    case 1:
      m_all = m_favorites;
      m_dirty = true;
     break;
    case 2:
      m_all = m_recent;
      m_dirty = true;
      break;
    }
    m_displayMode = mode;
  }

  bool SelectServer(int id)
  {
    int idx = GetServerIdxById(id);
    if(idx != -1)
    {
      m_selectedServer = idx;                        
    }
    return m_selectedServer != -1;
  }
  
  void ClearSelection()
  {
    m_selectedServer = -1;
  }

  bool IsVisible(int idx)
  {
    return idx>=m_startIndex && idx<min(int(m_all.size()),m_startIndex+m_visibleCount);
  }
  
  struct SSort 
  {
    SSort(const SMPServerList& sl):m_sl(sl)
    {
    }

    bool operator()(int i, int j)const//SORT!
    {
      int less = -1;
      switch(m_sl.m_sortcolumn)
      {
      case eSC_name:
				{
					bool fn = !m_sl.m_allServers[i].m_hostName.empty();
					bool sn = !m_sl.m_allServers[j].m_hostName.empty();
					if(!fn && !sn)
						less = -1;
					else if(fn && sn)
						less = (m_sl.m_allServers[i].m_hostName < m_sl.m_allServers[j].m_hostName)?1:0;
					else
						less = (fn && !sn)?1:0;
				}
				break;
      case eSC_ping:
        less = (m_sl.m_allServers[i].m_ping < m_sl.m_allServers[j].m_ping)?1:0;
        break;
      case eSC_players:
        less = (m_sl.m_allServers[i].m_numPlayers < m_sl.m_allServers[j].m_numPlayers)?1:0;
        break;
      case eSC_map:
        less = (m_sl.m_allServers[i].m_mapName < m_sl.m_allServers[j].m_mapName)?1:0;
        break;
      case eSC_mode:
        less = (m_sl.m_allServers[i].m_gameType < m_sl.m_allServers[j].m_gameType)?1:0;
        break;
      case eSC_private:
        less = (m_sl.m_allServers[i].m_private && !m_sl.m_allServers[j].m_private)?1:0;
        break;
      case eSC_favorite:
        less = (m_sl.m_allServers[i].m_favorite && !m_sl.m_allServers[j].m_favorite)?1:0;
        break;
      case eSC_official:
        less = (m_sl.m_allServers[i].m_official && !m_sl.m_allServers[j].m_official)?1:0;
        break;
      case eSC_anticheat:
        less = (m_sl.m_allServers[i].m_anticheat && !m_sl.m_allServers[j].m_anticheat)?1:0;
        break;
      }
      if(less == -1)
        less = (m_sl.m_allServers[i].m_publicIP<m_sl.m_allServers[j].m_publicIP)?1:0;

      return m_sl.m_sorttype==eST_ascending?(less!=0):(less==0);
    }
    const SMPServerList& m_sl;
  };

  void DoFilter(CMPLobbyUI::SServerFilter* filter)
  {
    m_all.resize(0);
    switch(m_displayMode)
    {
    case 0:
      {
        for(int i=0;i<m_allServers.size();++i)
          if(filter->Filter(m_allServers[i]))
            m_all.push_back(i);
      }
      break;
    case 1:
      {
        for(int i=0;i<m_favorites.size();++i)
          if(filter->Filter(m_allServers[m_favorites[i]]))
            m_all.push_back(m_favorites[i]);
      }
      break;
    case 2:
      {
        for(int i=0;i<m_recent.size();++i)
          if(filter->Filter(m_allServers[m_recent[i]]))
            m_all.push_back(m_recent[i]);
      }
      break;
    }
    if(m_selectedServer != -1)
    {
      if(std::find(m_all.begin(),m_all.end(),m_selectedServer)==m_all.end())
        ClearSelection();        
    }
  }

  void DoSort()
  {
    if(m_sortcolumn == eSC_none)
      return;
    int selected_pos = 0;
    if(m_selectedServer != -1)
    {
      for(int i=0;i<m_all.size();++i)
        if(m_all[i] == m_selectedServer)
        {
          selected_pos = i;
          break;
        }
    }
    
    std::stable_sort(m_all.begin(),m_all.end(),SSort(*this));

    if(m_selectedServer != -1)
    {
      int new_selected = -1;
      for(int i=0;i<m_all.size();++i)
      {
        if(m_all[i] == m_selectedServer)
        {
          new_selected = i;
          break;
        }
      }
      
      if(new_selected!=-1)
      {
        if(IsVisible(selected_pos))//if selected server is visible keep it's position on screen
        {
          //finally adjust view position
          SetStartIndex(m_startIndex + new_selected-selected_pos);
        }
      }
    }
    m_dirty = false;
  }

  CMPLobbyUI::SServerInfo& GetSelectedServer()
  {
    static CMPLobbyUI::SServerInfo dummy;
    if(m_selectedServer == -1)
      return dummy;
    return m_allServers[m_selectedServer];
  }

  void SetSelectedFavorite(bool fav)
  {
    if(m_selectedServer == -1)
      return;
    const CMPLobbyUI::SServerInfo &sel = GetSelectedServer(); 
    for(int i=0;i<m_allServers.size();++i)
    {
      if(m_allServers[i].m_publicIP == sel.m_publicIP &&
         m_allServers[i].m_hostPort == sel.m_hostPort &&
         m_allServers[i].m_favorite != fav)
      {
         m_allServers[i].m_favorite = fav;
         if(!fav)
           RemoveFromFavorites(i);
         else
         {
           AddToFavorites(i);
         }
      }
    }
  }

  DisplayedServersVector m_all;
  DisplayedServersVector m_favorites;
  DisplayedServersVector m_recent;
  bool                m_updateCompleted;

  ServerInfoVector    m_allServers;
  //server list info
  int                 m_displayMode;
  int                 m_startIndex;
  int                 m_visibleCount;
  int                 m_selectedServer;
  int                 m_total;
  int                 m_done;
  bool                m_dirty;
  ESortColumn         m_sortcolumn;
  ESortType           m_sorttype;
};

ESortColumn         gSortColumn = eSC_none;
ESortType           gSortType = eST_ascending;


struct SUserStatus
{
	SUserStatus():port(0),ping(-1){}
	string			server;
	int					port;
	string			gamemode;
	int					ping;
	EUserStatus status;
};

struct SUserListItem
{
  int					id;
  string			name;
  int					arrow;
  bool				header;
  bool				bubble;
  bool				playing;
  bool				offline;
	bool				foreign;
	SUserStatus status;
	bool operator<(const SUserListItem& u) const
	{
		return name.compareNoCase(u.name)<0;
	}
	bool operator==(const SUserListItem& u) const
	{
		return name.compareNoCase(u.name)==0;
	}
};

bool ParseUserStatus(SUserStatus& status, const char* str)
{
	const char* after_game = strstr(str,"://");
	if (after_game)
	{
		string game(str, after_game);
		after_game += 2;
		if(!strncmp("chatting",after_game,strlen("chatting")))
		{
			status.status = eUS_chatting;
		}
		else if(!strncmp("playing",after_game,strlen("playing")))
		{
			status.status = eUS_playing;
		}
		else if(!strncmp("away",after_game,strlen("playing")))
		{
			status.status = eUS_away;
		}
		status.status = eUS_online;
	}
	else
		return false;
	return true;
}

struct CMPLobbyUI::SMPUserList
{
  struct SVisibleEntry
  {
    int idx;
    EVisibleUserType type;
  };

	SMPUserList(CMPLobbyUI*	parent):
    m_visibleCount(22),
    m_firstVisible(0),
    m_selected(-1),
		m_parent(parent)
  {
    InitHeaders();
  }

  void InitHeaders()
  {
    SUserListItem i;
    m_headers.resize(0);
    i.arrow = 2;
    i.header = true;
    i.bubble = false;
    i.playing = false;
    i.offline = false;
		i.foreign = false;
    i.id = 0;
    i.name = "@ui_menu_BUDDIES";
    m_headers.push_back(i);
    i.id = 1;
    i.name = "@ui_menu_GLOBAL_CHAT";
    m_headers.push_back(i);
    i.id = 2;
		i.name = "@ui_menu_IGNORE_LIST";
    m_headers.push_back(i);
  }

  int GetDisplayCount()
  {
    return min(m_visibleCount,int(m_visibleList.size()));
  }

  SUserListItem& GetListItem(int i)
  {
    SVisibleEntry& v = m_visibleList[i];
    switch(v.type)
    {
    case eVUT_header:
      return m_headers[v.idx];
    case eVUT_buddy:
      return m_buddies[v.idx];
    case eVUT_global:
      return m_chat[v.idx];
      break;
    case eVUT_ignore:
      return m_ignore[v.idx];
    }
    assert(false);
    static SUserListItem dummy;
    return dummy;
  }

  bool    SetScrollPos(double fr)
  {
    int pos = (int)(fr*(m_visibleList.size()-m_visibleCount));
    return SetStartIndex(pos);
  }

  bool SetStartIndex(int i)
  {
    int set_idx = i;
    if(set_idx+m_visibleCount>m_visibleList.size())
      set_idx = m_visibleList.size()-m_visibleCount;

    if(set_idx<0)
      set_idx = 0;

    if(set_idx!= m_firstVisible)
    {
      m_firstVisible = set_idx;
      return true;
    }
    return false;
  }

	void UpdateVisibleList()
	{
		SSelectionSave save(*this);
		m_visibleList.resize(0);

    SVisibleEntry e;
    e.type = eVUT_header;
    e.idx = 0;
    m_visibleList.push_back(e);
    if(m_headers[0].arrow == 2)
		{
			e.type = eVUT_buddy;
			for(int i=0;i<m_buddies.size();++i)
			{
				e.idx = i;
				m_visibleList.push_back(e);
			}
		}

		e.type = eVUT_header;
		e.idx = 1;
		m_visibleList.push_back(e);

    if(m_headers[1].arrow == 2)
		{
			e.type = eVUT_global;
			for(int i=0;i<m_chat.size();++i)
			{
				e.idx = i;
				m_visibleList.push_back(e);
			}
		}

    if(g_pGameCVars->g_displayIgnoreList)
    {
      e.type = eVUT_header;
      e.idx = 2;
      m_visibleList.push_back(e);
    
      e.type = eVUT_ignore;
			if(m_headers[2].arrow == 2)
			{
				for(int i=0;i<m_ignore.size();++i)
				{
					e.idx = i;
					m_visibleList.push_back(e);
				}
			}
		}
		SetStartIndex(m_firstVisible);
	}

  void AddUser(EChatCategory cat, int id, const char *name, bool foreign)
  {
		SSelectionSave save(*this);
    SUserListItem i;
    i.id = id;
    i.name = name;
    i.header = false;
    i.bubble = false;
    i.arrow = 0;
    i.offline = false;
    i.playing = false;
		i.foreign = foreign;
    switch(cat)
    {
    case eCC_buddy:
      stl::binary_insert_unique(m_buddies,i);
      break;
    case eCC_global:
			stl::binary_insert_unique(m_chat,i);
			break;
    case eCC_ignored:
			stl::binary_insert_unique(m_ignore,i);
      break;
    }
		UpdateHeaders();
  }

  void RemoveUser(EChatCategory cat, int id)
  {
		SSelectionSave save(*this);
    std::vector<SUserListItem> *p = 0;
    switch(cat)
    {
    case eCC_buddy:
      p = &m_buddies;
      break;
    case eCC_global:
      p = &m_chat;
      break;
    case eCC_ignored:
      p = &m_ignore;
      break;
    default:
      return;
    }
    std::vector<SUserListItem> &list = *p;    
    for(int i=0;i<list.size();++i)
    {
      if(list[i].id == id)
      {
        list.erase(list.begin()+i);
        break;
      }
    }
		UpdateHeaders();
  }

	void UpdateHeaders()
	{
		/*m_headers[0].name.Format("@ui_menu_BUDDIES (%d)",m_buddies.size());
		m_headers[1].name.Format("@ui_menu_GLOBAL_CHAT (%d)",m_chat.size());
		m_headers[2].name.Format("@ui_menu_IGNORE_LIST (%d)",m_ignore.size());*/
	}

  void SetUserStatus(EChatCategory cat, int id, EUserStatus status, const char* descr)
  {
    std::vector<SUserListItem> *p = 0;
    switch(cat)
    {
    case eCC_buddy:
      p = &m_buddies;
      break;
    case eCC_global:
      p = &m_chat;
      break;
    case eCC_ignored:
      p = &m_ignore;
      break;
    default:
      return;
    }
    std::vector<SUserListItem> &list = *p;    
    for(int i=0;i<list.size();++i)
    {
      if(list[i].id == id)
      {
        SUserListItem &u = list[i];
        if(!ParseUserStatus(u.status,descr))
				{
					u.status.status = status;
				}
        switch(u.status.status)
        {
        case eUS_offline:
          u.offline = true;
          u.playing = false;
          u.bubble = false;
          break;
        case eUS_online:
          u.offline = false;
          u.playing = false;
          u.bubble = false;
          break;
        case eUS_playing:
          u.offline = false;
          u.playing = true;
          u.bubble = false;
          break;
        }
        break;
      }
    }
  }

  void SelectUser(int id)
  {
    m_selected = id;
  }

  
	void OpenGroup(int id)
	{
		if( id >= 0 && id < m_visibleList.size() && m_visibleList[id].type == eVUT_header )
		{
			SUserListItem& item = m_headers[m_visibleList[id].idx];
			if(item.arrow == 1)
				item.arrow = 2;
			else
				item.arrow = 1;
		}			
	}

  struct SSelectionSave
  {
    SSelectionSave(SMPUserList& p):m_parent(p),m_visibleDelta(0)
    {
      if(m_parent.m_selected == -1)
        return;
      if(!m_parent.m_visibleList.empty())
			{
        m_type = m_parent.m_visibleList[m_parent.m_selected].type;
				const SUserListItem& item = m_parent.GetListItem(m_parent.m_selected);
				m_id = item.id;
				m_visibleDelta = m_parent.m_selected-m_parent.m_firstVisible;
			}
      else //invalid
      {
        m_id = -1;
        m_type = eVUT_header;
      }
    }
    ~SSelectionSave()
    {
      if(m_parent.m_selected == -1 || m_parent.m_visibleList.empty())
        return;
      for(int i=0;i<m_parent.m_visibleList.size();++i)
        if(m_parent.m_visibleList[i].type == m_type && m_parent.GetListItem(i).id == m_id)
        {
          m_parent.m_selected = i;
					if(m_visibleDelta>=0 && m_visibleDelta<m_parent.m_visibleCount)//item was visible
					{
						m_parent.SetStartIndex(i - m_visibleDelta);
					}
          return;
        }
      m_parent.m_selected = -1;
			if(m_parent.m_parent)
				m_parent.m_parent->SetChatButtonsMode(false,0);
    }

		int 							m_visibleDelta;

    SMPUserList&			m_parent;
    EVisibleUserType	m_type;
		int								m_id;
  };

  std::vector<SVisibleEntry> m_visibleList;
  int                        m_firstVisible;
  int                        m_visibleCount;
  int                        m_selected;

  SVisibleEntry              m_tempSelection;

  std::vector<SUserListItem> m_headers;
  std::vector<SUserListItem> m_chat;
  std::vector<SUserListItem> m_ignore;
  std::vector<SUserListItem> m_buddies;
	CMPLobbyUI*								 m_parent;
};


struct SMPChatText
{
  SMPChatText():
  m_firstVisible(0),
  m_visibleCount(22)
  {}

  bool    SetScrollPos(double fr)
  {
    int pos = (int)(fr*(m_text.size()-m_visibleCount));
    return SetStartIndex(pos);
  }

  bool SetStartIndex(int i)
  {
    int set_idx = i;
    if(set_idx+m_visibleCount>m_text.size())
      set_idx = m_text.size()-m_visibleCount;

    if(set_idx<0)
      set_idx = 0;

    if(set_idx!= m_firstVisible)
    {
      m_firstVisible = set_idx;
      return true;
    }
    return false;
  }

  void AddText(const char* text)
  {
    m_text.push_back(text);
    if(m_text.size()>m_visibleCount)
      m_firstVisible++;
  }

  void Clear()
  {
    m_firstVisible = 0;
    m_text.resize(0);
  }
  
  int                 m_firstVisible;
  int                 m_visibleCount;
  std::vector<string> m_text;
};

CMPLobbyUI::CMPLobbyUI(IFlashPlayer* plr):
m_player(plr),
m_serverlist(new SMPServerList()),
m_currentTab(-1),
m_chatBlink(false)
{
  m_cmd.reserve(256);
  SetSortParams(gSortColumn, gSortType);
  m_userlist.reset(new SMPUserList(this));
  m_chatlist.reset(new SMPChatText());
  m_filter.reset(new SServerFilter(this));

	DisplayChatText();
	SetChatHeader("@ui_menu_GLOBALCHAT",0);
}

CMPLobbyUI::~CMPLobbyUI()
{
  gSortColumn = m_serverlist->m_sortcolumn;
  gSortType = m_serverlist->m_sorttype;
}


bool CMPLobbyUI::HandleFSCommand(EGsUiCommand cmd, const char* pArgs)
{
  bool handled = true;
  switch(cmd)
  {
  case eGUC_setVisibleServers:
    {
      int i=atoi(pArgs);
      m_serverlist->SetVisibleCount(i);
      DisplayServerList();
    }
    break;
  case eGUC_displayServerList:
    {
      DisplayServerList();
    }
    break;
  case eGUC_serverScrollBarPos:
    {
      double pos = atof(pArgs);
      SetServerListPos(pos);
    }
    break;
  case eGUC_serverScroll:
    {
      int d = atoi(pArgs);
      ChangeServerListPos(d);
    }
    break;
  case eGUC_userScrollBarPos:
    {
      double pos = atof(pArgs);
      SetUserListPos(pos);
    }
    break;
  case eGUC_userScroll:
    {
      int d = atoi(pArgs);
      ChangeUserListPos(d);
    }
    break;
  case eGUC_chatScrollBarPos:
    {
      double pos = atof(pArgs);
      SetChatTextPos(pos);
    }
    break;
  case eGUC_chatScroll:
    {
      int d = atoi(pArgs);
      ChangeChatTextPos(d);
    }
    break;
  case eGUC_selectServer:
    {
      int id = atoi(pArgs);
      SelectServer(id);
    }
    break;
  case eGUC_tab:
    {
      int id = atoi(pArgs);
      ChangeTab(id);
    }
    break;
  case eGUC_find:
    {
      ClearSearchResults();
    }
    break;
  case eGUC_addBuddy:
    {
      int id = atoi(pArgs);
      const SUserListItem& u = m_userlist->GetListItem(id);
      ShowRequestDialog(u.name);
    }
    break;
	case eGUC_addBuddyFromFind:
		{
			if(pArgs && pArgs[0])
			{
				ShowRequestDialog(pArgs);
			}
		}
		break;
	case eGUC_addBuddyFromInfo:
		{
			ShowRequestDialog(m_infoNick);
		}
		break;
  case eGUC_addIgnoreFromInfo:
    {
      OnAddIgnore(m_infoNick);
    }
    break;
  case eGUC_addIgnore:
    {
      int id = atoi(pArgs);
      const SUserListItem& u = m_userlist->GetListItem(id);
      OnAddIgnore(u.name);
    }
    break;
  case eGUC_stopIgnore:
    {
      int id = atoi(pArgs);
			if(id != -1)
			{
				const SUserListItem& u = m_userlist->GetListItem(id);
				OnStopIgnore(u.name);
			}
    }
    break;
  case eGUC_inviteBuddy:
    {
      OnAddBuddy(m_inviteNick,pArgs);
    }
    break;
  case eGUC_removeBuddy:
    {
      int id = atoi(pArgs);
			if(id != -1)
			{
				OnRemoveBuddy(m_userlist->GetListItem(id).name);
			}
    }
    break;
  case eGUC_chatClick:
    {
      int id = atoi(pArgs);
			if(id != -1)
			{
				SelectUser(id);
			}
    }
    break;
  case eGUC_sortColumn:
    {
      ESortColumn old_mode = m_serverlist->m_sortcolumn;
      ESortType   old_type = m_serverlist->m_sorttype;
      string header = pArgs;
      const char* comma = strchr(pArgs,',');
      if(comma!=0)
      {
        header = header.substr(0,comma-pArgs);
        m_serverlist->m_sorttype = comma[1]=='1'?eST_ascending:eST_descending;
      }

      ESortColumn c = KEY_BY_VALUE(header,gSortColumnNames);
      m_serverlist->m_sortcolumn = c;

      if(old_mode!=m_serverlist->m_sortcolumn || old_type!=m_serverlist->m_sorttype)
      {
        m_serverlist->m_dirty = true;
        DisplayServerList();
      }    
    }
    break;
  case eGUC_displayInfo:
    OnShowUserInfo(atoi(pArgs),0);
    break;
  case eGUC_displayInfoInList:
    {
      int id = atoi(pArgs);
			if(id>=0 && id<m_userlist->m_visibleList.size())
			{
				EVisibleUserType t = m_userlist->m_visibleList[id].type;
				switch(t)
				{
				case eVUT_global:
					OnShowUserInfo(0, m_userlist->GetListItem(id).name);
					break;
				case eVUT_buddy:
				case eVUT_ignore:
					OnShowUserInfo(m_userlist->GetListItem(id).id, m_userlist->GetListItem(id).name);
					break;
				}
			}
    }
    break;
  case eGUC_addFavorite:
		OnAddFavorite();
    m_serverlist->SetSelectedFavorite(true);
    DisplayServerList();
    break;
  case eGUC_removeFavorite:
    {
			OnRemoveFavorite();
      int sel = m_serverlist->m_selectedServer;
      m_serverlist->SetSelectedFavorite(false);
      if(sel!=-1 && m_serverlist->m_selectedServer ==-1)
        ClearSelection();        
      DisplayServerList();
    }
    break;
  case eGUC_joinPassword:
    {
      OnJoinWithPassword(atoi(pArgs));
    }
    break;
	case eGUC_chatOpen:
		m_userlist->OpenGroup(atoi(pArgs));
		DisplayUserList();
		break;
  default:
    handled = false;
  }

  handled = OnHandleCommand(cmd,pArgs) || handled;
  handled = m_filter->HandleFSCommand(cmd,pArgs) || handled;  

  return handled;
}

void  CMPLobbyUI::ClearServerList()
{
  m_cmd = MPPath;
  m_cmd += "ClearServerList\0";
  m_player->Invoke0(m_cmd);
  m_serverlist->Clear();
  ClearSelection();
}

void  CMPLobbyUI::AddServer(const SServerInfo& srv)
{
  m_serverlist->AddServer(srv);
}

void  CMPLobbyUI::UpdateServer(const SServerInfo& srv)
{
  m_serverlist->UpdateServer(srv);
}

void  CMPLobbyUI::UpdatePing(int id, int ping)//applies sorting
{
  m_serverlist->UpdatePing(id,ping);
}

void  CMPLobbyUI::RemoveServer(int id)
{
  if(m_serverlist->m_selectedServer!=-1 && m_serverlist->GetSelectedServer().m_serverId == id)
  {
    m_cmd = MPPath;
    m_cmd += "ClearSelectedServer\0";
    m_player->Invoke0(m_cmd);
  }
  m_serverlist->RemoveServer(id);
}

void  CMPLobbyUI::ClearSelection()
{
  m_serverlist->ClearSelection();
  m_cmd = MPPath;
  m_cmd += "ClearSelectedServer\0";
  m_player->Invoke0(m_cmd);

}

bool  CMPLobbyUI::GetSelectedServer(SServerInfo& srv)
{
  if(m_serverlist->m_selectedServer==-1)
    return false;
  srv = m_serverlist->GetSelectedServer();
  return true;
}

bool  CMPLobbyUI::GetServer(int id, SServerInfo& srv)
{
  int idx = m_serverlist->GetServerIdxById(id);
  if(idx==-1)
    return false;
  srv = m_serverlist->m_allServers[idx];
  return true;
}
void  CMPLobbyUI::FinishUpdate()
{
  m_cmd = MPPath;
  m_cmd += "GameLobby_M.GameLobby.LeftLobby.switchUpdateButton\0";
  m_player->Invoke1(m_cmd,0);
  DisplayServerList();
}

void  CMPLobbyUI::StartUpdate()
{
  m_cmd = MPPath;
  m_cmd += "GameLobby_M.GameLobby.LeftLobby.switchUpdateButton\0";
  m_player->Invoke1(m_cmd,1);
  DisplayServerList();
}

void  CMPLobbyUI::SetUpdateProgress(int done, int total)
{
  m_serverlist->m_done = done;
  m_serverlist->m_total = total;
}

void  CMPLobbyUI::ChangeTab(int tab)
{
  if(m_currentTab == tab)
    return;
  if(m_currentTab != -1)
    OnDeactivateTab(m_currentTab);
  m_currentTab = tab;
  m_serverlist->SetDisplayMode(m_currentTab);
  ClearSelection();
  DisplayServerList();
  if(tab != -1)
    OnActivateTab(tab);
  if(tab == 3)
	{
		m_chatBlink = false;
    SetBlinkChat(false);
		SelectUser(m_userlist->m_selected);
	}
  else
	{
		if(m_chatBlink)
			SetBlinkChat(true);  
		SetStatusString("");
		}
}

void  CMPLobbyUI::DisplayServerList()
{
  if(m_serverlist->m_dirty)
  {
		if(m_filter.get())
			m_serverlist->DoFilter(m_filter.get());
    m_serverlist->DoSort();
  }
  m_cmd = MPPath;
  m_cmd += "ClearServerList\0";
  m_player->Invoke0(m_cmd.c_str());
  
  char convert[32]      = "\0";
  char uiNumPlayers[32] = "\0";
  char uiIP[32] = "\0";

  //scrolling

  int num_servers = m_serverlist->m_all.size();

  itoa(num_servers , convert, 10);
  m_cmd = MPPath;
  m_cmd += "NumServers\0";
  m_player->SetVariable(m_cmd.c_str(), convert);

  itoa(m_serverlist->m_startIndex, convert, 10);
  m_cmd = MPPath;
  m_cmd += "DisplayServerIndex\0";
  m_player->SetVariable(m_cmd.c_str(), convert);

  m_cmd = MPPath;
  m_cmd += "ManageServerScrollbar\0";
  m_player->Invoke0(m_cmd.c_str());
  
  static string count;
  count.resize(0);

  if(m_serverlist->m_total != -1)
  {
    itoa(m_serverlist->m_done, convert, 10);
    count = convert;
    count += "/";
    itoa(m_serverlist->m_total, convert, 10);
    count += convert;
  }
  m_cmd = MPPath;
  m_cmd +="GameLobby_M.GameLobby.Texts2.Colorset.ServerCount.text";
  m_player->SetVariable(m_cmd.c_str(), count.c_str());

  for(int i=0;i<m_serverlist->m_visibleCount;i++)
  {
    int idx = i + m_serverlist->m_startIndex;
    if(idx>=num_servers)
      break;

    const SServerInfo &server = m_serverlist->m_allServers[m_serverlist->m_all[idx]];

    itoa(server.m_numPlayers, convert, 10);
    if (server.m_numPlayers < 10)
    {
      convert[1] = convert[0];
      convert[0] = ' ';
    }
    convert[2] = '\0';

    strcpy(uiNumPlayers, convert);
    strcat(uiNumPlayers, " / \0");

    itoa(server.m_maxPlayers, convert, 10);
    if (server.m_maxPlayers < 10)
    {
      convert[1] = ' ';
    }
    convert[2] = '\0';

    strcat(uiNumPlayers, convert);

    _snprintf(uiIP,32,"%d.%d.%d.%d",server.m_publicIP&0xFF,(server.m_publicIP>>8)&0xFF,(server.m_publicIP>>16)&0xFF,(server.m_publicIP>>24)&0xFF);
    uiIP[31] = 0;

    m_cmd = MPPath;
    m_cmd += "AddServer\0";

		SFlashVarValue args[] = {server.m_serverId, server.m_hostName.c_str(), server.m_ping, uiNumPlayers, server.m_mapName.c_str(), server.m_gameTypeName.c_str(), uiIP, server.m_hostPort, server.m_private, server.m_official, server.m_favorite, server.m_anticheat, server.m_canjoin, server.m_gameVersion};
    m_player->Invoke(m_cmd.c_str(), args, sizeof(args)/sizeof(args[0]));

  }

  m_cmd = MPPath;
  m_cmd += "SetServerListInfo\0";
  m_player->Invoke0(m_cmd.c_str());
}

void    CMPLobbyUI::SetServerListPos(double sb_pos)
{
  if(m_serverlist->SetScrollPos(sb_pos))
    DisplayServerList();
}

void    CMPLobbyUI::ChangeServerListPos(int delta)//either +1 or -1
{
  if(m_serverlist->SetStartIndex(m_serverlist->m_startIndex+delta))
    DisplayServerList();    
}

void  CMPLobbyUI::SetUserListPos(double sb_pos)
{
  if(m_userlist->SetScrollPos(sb_pos))  
    DisplayUserList();
}

void  CMPLobbyUI::ChangeUserListPos(int delta)
{
  if(m_userlist->SetStartIndex(m_userlist->m_firstVisible + delta))
    DisplayUserList();
}

void  CMPLobbyUI::SetChatTextPos(double sb_pos)
{
  if(m_chatlist->SetScrollPos(sb_pos))  
    DisplayChatText();
}

void  CMPLobbyUI::ChangeChatTextPos(int delta)
{
  if(m_chatlist->SetStartIndex(m_chatlist->m_firstVisible + delta))
    DisplayChatText();
}

void CMPLobbyUI::SetChatMode(const char* buddy)
{
  BlinkChat(true);
  if(buddy)
    SetChatHeader("@ui_menu_CHAT_WITH",buddy);
  else
    SetChatHeader("@ui_menu_GLOBALCHAT",0);
}

void CMPLobbyUI::SelectServer(int id)
{
  if(m_serverlist->SelectServer(id))
  {
    ResetServerDetails();
  }
}

void  CMPLobbyUI::SelectUser(int id)
{
  if(id>=0 && id<m_userlist->m_visibleList.size())
  {
    EVisibleUserType user_type = m_userlist->m_visibleList[id].type;
    const SUserListItem& item = m_userlist->GetListItem(id);

    switch(user_type)
    {
    case eVUT_buddy:
      SetChatButtonsMode(!item.foreign,1);
      OnChatModeChange(false,item.id);
      OnUserSelected(eCC_buddy,item.id);
			switch(item.status.status)
			{
			case eUS_online:
				SetStatusString("@ui_menu_Buddy_is_online",item.name.c_str());
				break;
			case eUS_chatting:
				SetStatusString("@ui_menu_Buddy_is_chatting",item.name.c_str());
				break;
			case eUS_playing:
				SetStatusString("@ui_menu_Buddy_is_playing",item.name.c_str());
				break;
			case eUS_away:
				SetStatusString("@ui_menu_Buddy_is_away",item.name.c_str());
				break;
			case eUS_offline:
				SetStatusString("@ui_menu_Buddy_is_offline",item.name.c_str());
				break;
			default:
				SetStatusString("");
			}
      SetChatMode(item.name.c_str());
      break;
    case eVUT_global:
      SetChatButtonsMode(true,CanAdd(eCC_global,item.id,item.name)?2:0);
      OnChatModeChange(true,0);
      OnUserSelected(eCC_global,item.id);
      SetChatMode(0);
      SetStatusString("");
      break;
    case eVUT_ignore:
      SetChatButtonsMode(true,3);
      OnChatModeChange(true,0);
      OnUserSelected(eCC_ignored,item.id);
      SetChatMode(0);
      SetStatusString("");
      break;
    case eVUT_header:
      SetChatButtonsMode(false,0);
      OnChatModeChange(true,0);
      SetChatMode(0);
      SetStatusString("");
      break;
    }
  }
	else
	{
		SetChatButtonsMode(false,0);
	}
  m_userlist->SelectUser(id);
}

void  CMPLobbyUI::SetChatButtonsMode(bool info, int action)
{
  m_cmd = MPPath;
  m_cmd += "setBuddyListMenuEnabled";
  SFlashVarValue args[]={true,info,action};
  m_player->Invoke(m_cmd,args,sizeof(args)/sizeof(args[0]));
}

void  CMPLobbyUI::BreakChatText(const char* text)
{
  m_cmd = MPPath;
  m_cmd += "TestChat";
  m_player->Invoke1(m_cmd,text);
  m_cmd = MPPath;
  m_cmd += "back_TextSize";
  SFlashVarValue v(int(1));
  m_player->GetVariable(m_cmd,&v);
  m_cmd = MPPath;
  m_cmd += "back_Text";
  SFlashVarValue sv("");
  m_player->GetVariable(m_cmd,&sv);
  const char* str = sv.GetConstStrPtr();
  if(v.GetDouble()>1)
  {
    const char* next = strchr(str,'\n');
    while(next)
    {
      string a(str,next);
      m_chatlist->AddText(a);
      str = next+1;
      next = strchr(str,'\n');
    }
    m_chatlist->AddText(str);
  }
  else
  {
    m_chatlist->AddText(str);
  }
}

void  CMPLobbyUI::ResetServerDetails()
{
  m_cmd = MPPath;
  m_cmd += "GameLobby_M.GameLobby.resetServerInfo";
  m_player->Invoke0(m_cmd);

  m_cmd = MPPath;
  m_cmd += "ClearPlayerList";
  m_player->Invoke0(m_cmd);

  m_cmd = MPPath;
  m_cmd += "SetPlayerListInfo";
  m_player->Invoke0(m_cmd);
}

void  CMPLobbyUI::SetServerDetails(const SServerDetails& sd)
{
  m_cmd = MPPath;
  m_cmd += "GameLobby_M.GameLobby.setServerInfo";
	if(sd.m_noResponce)
	{
		SFlashVarValue args[]={"@ui_menu_N_A","@ui_menu_N_A","@ui_menu_N_A","@ui_menu_N_A","@ui_menu_N_A","@ui_menu_N_A","@ui_menu_N_A","@ui_menu_N_A","@ui_menu_N_A","@ui_menu_N_A","@ui_menu_N_A"};
		m_player->Invoke(m_cmd,args,sizeof(args)/sizeof(args[0]));
	}
	else
	{
		static string timesting;
		if(sd.m_timelimit && sd.m_timeleft)
			timesting.Format("%02d:%02d",sd.m_timeleft/60,sd.m_timeleft%60);
		else
			timesting = "-";
		SFlashVarValue args[]={sd.m_gamever.c_str(),
													timesting.c_str(),
													0,
													sd.m_anticheat?"@ui_menu_ON":"@ui_menu_OFF",
													sd.m_friendlyfire?"@ui_menu_ON":"@ui_menu_OFF",
													"-", 
													sd.m_dedicated?"@ui_menu_YES":"@ui_menu_NO", 
													sd.m_voicecomm?"@ui_menu_ENABLED":"@ui_menu_DISABLED", 
													sd.m_gamepadsonly?"@ui_menu_YES":"@ui_menu_NO",
													sd.m_modname,
													sd.m_modversion};

		m_player->Invoke(m_cmd,args,sizeof(args)/sizeof(args[0]));
	}

  m_cmd = MPPath;
  m_cmd += "SetPlayerGameMode";
  m_player->Invoke1(m_cmd,sd.m_gamemode.c_str());

  if(!sd.m_players.empty())
  {
    m_cmd = MPPath;
    m_cmd += "AddPlayer";

    for(int i=0;i<sd.m_players.size();++i)
    {
      SFlashVarValue args[]={
				SFlashVarValue(i),
				SFlashVarValue(sd.m_players[i].m_team),
				SFlashVarValue(sd.m_players[i].m_name.c_str()),
				SFlashVarValue(sd.m_players[i].m_rank),
				SFlashVarValue(sd.m_players[i].m_kills),
				SFlashVarValue(sd.m_players[i].m_deaths)
			};
      m_player->Invoke(m_cmd,args,sizeof(args)/sizeof(args[0]));
    }
    m_cmd = MPPath;
    m_cmd += "SetPlayerListInfo";
    m_player->Invoke0(m_cmd);
  }
  DisplayServerList();
}

void  CMPLobbyUI::SetSortParams(ESortColumn sc, ESortType st)
{
  m_serverlist->m_sortcolumn = sc;
  m_serverlist->m_sorttype = st;
  m_cmd = MPPath;
  m_cmd += "SortOrder";
  m_player->SetVariable(m_cmd,st==eST_ascending?"ASC":"DSC");
  m_cmd = MPPath;
  m_cmd += "SortColumn";
  const char* col = VALUE_BY_KEY(sc,gSortColumnNames);
  m_player->SetVariable(m_cmd,col);
}

void  CMPLobbyUI::ClearUserList()
{
  m_cmd = MPPath;
  m_cmd += "ClearBuddyList";
  m_player->Invoke0(m_cmd);
}

void  CMPLobbyUI::AddChatUser(EChatCategory cat, int id,const char *name, bool foreign)
{
  m_userlist->AddUser(cat,id,name,foreign);
  DisplayUserList();
}

void  CMPLobbyUI::ChatUserStatus(EChatCategory cat, int id, EUserStatus status, const char* descr )
{
  m_userlist->SetUserStatus(cat,id,status,descr);
  DisplayUserList();
}

void  CMPLobbyUI::RemoveCharUser(EChatCategory cat, int id)
{
  m_userlist->RemoveUser(cat,id);
  DisplayUserList();
}

void  CMPLobbyUI::AddFindPerson(int id, const char *name)
{
  m_cmd = "addFoundPerson";
  SFlashVarValue args[]={name};
  m_player->Invoke(m_cmd,args,sizeof(args)/sizeof(args[0]));  
}

void  CMPLobbyUI::UpdateUsers()
{
  m_cmd = MPPath;
  m_cmd += "SelectedBuddy";
  m_player->SetVariable(m_cmd,m_userlist->m_selected);
  m_cmd = MPPath;
  m_cmd += "SetBuddyListInfo";
  m_player->Invoke0(m_cmd);
}

void  CMPLobbyUI::AddChatText(const char *text)
{
  BlinkChat(true);
//  m_chatlist->AddText(text);
  BreakChatText(text);
  DisplayChatText();
}

void  CMPLobbyUI::DisplayChatText()
{
  m_cmd = MPPath;
  m_cmd += "ClearChatList";
  m_player->Invoke0(m_cmd);
  
  m_cmd = MPPath;
  m_cmd += "NumChats";
  m_player->SetVariable(m_cmd,int(m_chatlist->m_text.size()));

  m_cmd = MPPath;
  m_cmd += "DisplayChatIndex";
  m_player->SetVariable(m_cmd,m_chatlist->m_firstVisible);

  m_cmd = MPPath;
  m_cmd += "ManageChatScrollbar";
  m_player->Invoke0(m_cmd);

  m_cmd = MPPath;
  m_cmd += "AddChatText";

  for(int i=0;i<m_chatlist->m_visibleCount;++i)
  {
    int idx = m_chatlist->m_firstVisible+i;
    if(idx>=m_chatlist->m_text.size())
      break;
    SFlashVarValue args[]={0,m_chatlist->m_text[idx].c_str()};
    m_player->Invoke(m_cmd,args,sizeof(args)/sizeof(args[0]));
  }
}

void  CMPLobbyUI::BlinkChat(bool blink)
{
  m_chatBlink = blink;
  SetBlinkChat(blink);
}

void  CMPLobbyUI::EnableTabs(bool fav, bool recent, bool chat)
{
	m_cmd = MPPath;
	m_cmd += "setMPTabs";
  SFlashVarValue args[]={1,fav?1:0,recent?1:0,chat?1:0};
  m_player->Invoke(m_cmd,args,sizeof(args)/sizeof(args[0]));
}

void  CMPLobbyUI::SetChatHeader(const char* text, const char* name)
{
	if(name && strlen(name))
	{
		static wstring tmp, tmp2;
		gEnv->pSystem->GetLocalizationManager()->LocalizeLabel(text, tmp);

		StrToWstr(name,tmp2);
		
		static wstring header;
		header.resize(0);

		gEnv->pSystem->GetLocalizationManager()->FormatStringMessage(header,tmp,tmp2.c_str());
		m_cmd = "setChatHeaderText";
		m_player->Invoke1(m_cmd,header.c_str());
	}
	else
	{
		m_cmd = "setChatHeaderText";
		m_player->Invoke1(m_cmd,text);
	}
}

void  CMPLobbyUI::AddSearchResult(int id, const char* nick)
{
  m_cmd = "addFoundPerson";
  SFlashVarValue args[]={nick,id};
  m_player->Invoke(m_cmd,args,sizeof(args)/sizeof(args[0]));
}

void CMPLobbyUI::ClearSearchResults()
{
  m_cmd = "clearFindPerson";
  m_player->Invoke0(m_cmd);
}

void CMPLobbyUI::EnableSearchButton(bool f)
{
  m_cmd = "setFindPersonDisabled";
  m_player->Invoke1(m_cmd,!f);
}

void  CMPLobbyUI::SetInfoScreenId(int id)
{
  m_player->Invoke1("setPlayerInfoID",id);
}

void  CMPLobbyUI::EnableInfoScreenContorls(bool add, bool ignore)
{
  m_player->Invoke1("setPlayerInfoAddBuddyDisabled",!add);
  m_player->Invoke1("setPlayerInfoIgnoreDisabled",!ignore);
}

void CMPLobbyUI::SetUserDetails(const char* nick, const char* country)
{
  m_cmd = "setPlayerInfo";
  // (Nick, Country, HoursPlayed, BattleWon, BattleLost, Kills, Death, FavMap, FavWeapon, TotalScorefor)
  SFlashVarValue args[]={nick,country};
  m_player->Invoke(m_cmd,args,sizeof(args)/sizeof(args[0]));
}

void CMPLobbyUI::ShowInvitation(int id, const char* nick, const char* message)
{
  m_cmd = "showInvitationReceived";
	//showInvitationReceived(Playername, Text)
  SFlashVarValue args[]={id, nick, message};
  m_player->Invoke(m_cmd,args,sizeof(args)/sizeof(args[0]));
}


void CMPLobbyUI::SetJoinPassword()
{
  SFlashVarValue p("");
  m_player->GetVariable("_root.MPServer_Password",&p);
  ICVar* pV = gEnv->pConsole->GetCVar("sv_password");
  if(pV)
    pV->Set(p.GetConstStrPtr());
}

void	CMPLobbyUI::EnableResume(bool enable)
{
	m_player->Invoke1("setResumeEnabled",enable);
}

void  CMPLobbyUI::SetBlinkChat(bool blink)
{
  if(m_currentTab == 3 && blink)
    return;
  m_cmd = "setBlinkChat";
  m_player->Invoke1(m_cmd,blink?1:0);
}

void  CMPLobbyUI::DisplayUserList()
{
  m_userlist->UpdateVisibleList();
  ClearUserList();
  
  m_cmd = MPPath;
  m_cmd += "NumBuddies";
  m_player->SetVariable(m_cmd,int(m_userlist->m_visibleList.size()));

  m_cmd = MPPath;
  m_cmd += "DisplayBuddyIndex";
  m_player->SetVariable(m_cmd,int(m_userlist->m_firstVisible));

  m_cmd = MPPath;
  m_cmd += "ManageBuddyScrollbar";
  m_player->Invoke0(m_cmd);

  m_cmd = MPPath;
  m_cmd += "AddBuddy";
  for(int i=0, num=m_userlist->GetDisplayCount(); i<num; ++i)
  {
    const SUserListItem &item = m_userlist->GetListItem(i+m_userlist->m_firstVisible);
    SFlashVarValue args[]={i+m_userlist->m_firstVisible,item.name.c_str(),item.arrow,item.bubble,item.playing,item.playing,item.offline,!item.foreign};
    m_player->Invoke(m_cmd,args,sizeof(args)/sizeof(args[0]));
  }
  UpdateUsers();
}

void  CMPLobbyUI::SetStatusString(const char* str)
{
  m_player->Invoke1("setToolTipText",str);
}

void  CMPLobbyUI::SetStatusString(const char* fmt, const char* param)
{
	static wstring tmp, tmp2;
	gEnv->pSystem->GetLocalizationManager()->LocalizeLabel(fmt, tmp);

	StrToWstr(param,tmp2);

	static wstring text;
	text.resize(0);

	gEnv->pSystem->GetLocalizationManager()->FormatStringMessage(text,tmp,tmp2.c_str());
	m_cmd = "setToolTipText";
	m_player->Invoke1(m_cmd,text.c_str());
}

void  CMPLobbyUI::SetProfileInfo(const SUserInfo& ui)
{
  m_infoNick = ui.m_nick;
  m_cmd = "setPlayerInfo";
	static string country_code;
	if(!ui.m_country.empty())
	{
		country_code = "@ui_country_";
		country_code += ui.m_country;
	}
	else
		country_code = "";

	static string played, accuracy, kills, deaths;
	
	if(ui.m_stats.m_played)
		played.Format("%d",ui.m_stats.m_played/60);
	else
		played.resize(0);
	
	if(ui.m_stats.m_accuracy)
		accuracy.Format("%.2f%%",ui.m_stats.m_accuracy);
	else
		accuracy.resize(0);

	if(ui.m_stats.m_kills)
		kills.Format("%d",ui.m_stats.m_kills);
	else
		kills.resize(0);

	if(ui.m_stats.m_deaths)
		deaths.Format("%d",ui.m_stats.m_deaths);
	else
		deaths.resize(0);

  SFlashVarValue args[]={ui.m_nick.c_str(),
												 country_code.c_str(),
												 played.c_str(),
												 accuracy.c_str(),
												 ui.m_stats.m_suitMode.c_str(),
												 kills.c_str(),
												 deaths.c_str(),
												 ui.m_stats.m_map.c_str(),
												 ui.m_stats.m_weapon.c_str()};
  m_player->Invoke(m_cmd, args, sizeof(args)/sizeof(args[0]));
}

void  CMPLobbyUI::SetProfileInfoNick(const char* nick)
{
	m_cmd = "setPlayerInfo";

	SFlashVarValue args[]={nick, "", "", "", "", "", "", "", ""};

	m_player->Invoke(m_cmd, args, sizeof(args)/sizeof(args[0]));
	m_infoNick = nick;
}

void  CMPLobbyUI::SetJoinButtonMode(EJoinButtonMode m)
{
  if(m == eJBM_default)
    m_player->Invoke1("_root.Root.MainMenu.MultiPlayer.setJoinServerButtonState", -1);
  else
    m_player->Invoke1("_root.Root.MainMenu.MultiPlayer.setJoinServerButtonState", int(m));
}

void CMPLobbyUI::ShowRequestDialog(const char* nick)
{
  m_inviteNick = nick;
  m_player->Invoke1("showSendInvitation",nick);
}

void  CMPLobbyUI::OpenPasswordDialog(int id)
{
  m_player->Invoke1("showEnterPassword",id);
}

void CMPLobbyUI::TestChatUI()
{
/*  ClearChatList();
  AddChatUser(1,"YoYo", eCC_buddy, false);
  AddChatUser(2,"GYoYo2", eCC_global, false);
  AddChatUser(3,"GYoYo4", eCC_global, true);
  AddChatUser(4,"GYoYo8", eCC_global, false);
  AddChatUser(5,"GYoYof1", eCC_ignored, true);
  AddChatUser(6,"GYoYof2", eCC_ignored, true);
  AddChatUser(7,"GYoYof3", eCC_ignored, true);
  AddChatUser(8,"GYoYof4", eCC_ignored, true);
  AddChatUser(9,"GYoYof5", eCC_ignored, true);
  AddChatUser(10,"GYoYof6", eCC_ignored, true);
  AddChatUser(11,"GYoYof7", eCC_ignored, true);
  AddChatUser(12,"GYoYoh2", eCC_ignored, true);
  AddChatUser(13,"GYoYoh3", eCC_ignored, true);
  AddChatUser(14,"GYoYoh4", eCC_ignored, true);

  AddChatUser(15,"GYoYof1", eCC_ignored, true);
  AddChatUser(16,"GYoYof2", eCC_ignored, true);
  AddChatUser(17,"GYoYof3", eCC_ignored, true);
  AddChatUser(18,"GYoYof4", eCC_ignored, true);
  AddChatUser(19,"GYoYof5", eCC_ignored, true);
  AddChatUser(20,"GYoYof6", eCC_ignored, true);
  AddChatUser(21,"GYoYof7", eCC_ignored, true);
  AddChatUser(22,"GYoYoh2", eCC_ignored, true);
  AddChatUser(23,"GYoYoh3", eCC_ignored, true);
  AddChatUser(24,"GYoYoh4", eCC_ignored, true);

  AddChatText("@ui_menu_DD");*/
  /*ExpandCategory(eCC_global, true);
  ExpandCategory(eCC_buddy, false);
  SetChatHeader("Testing chat asset");
  UpdateUsers();*/
  
  BlinkChat(true);
  EnableTabs(false, false, true);
  m_player->Invoke1("setToolTipText","Bla bla bla");
  for(int i=0;i<10;++i)
    m_player->Invoke1("addFoundPerson",string().Format("Testgyu%d",i).c_str());
}

bool CMPLobbyUI::SServerFilter::HandleFSCommand(EGsUiCommand cmd, const char* pArgs)
{
  switch(cmd)
  {
	case eGUC_filtersDisplay:
		UpdateUI();
		return true;
  case eGUC_filtersEnable:
    m_on = atoi(pArgs)!=0;
    break;
  case eGUC_filtersMode:
    m_gamemode = strcmp(pArgs,"all")!=0?pArgs:"";
    break;
  case eGUC_filtersMap:
    m_mapname = pArgs;
    m_mapname.MakeLowerLocale();
    break;
  case eGUC_filtersPing:
    m_minping = atoi(pArgs);
    break;
  case eGUC_filtersNotFull:
    m_notfull = atoi(pArgs)!=0;
    break;
  case eGUC_filtersNotEmpty:
    m_notempty = atoi(pArgs)!=0;
    break;
  case eGUC_filtersNoPassword:
    m_notprivate = atoi(pArgs)!=0;
    break;
  case eGUC_filtersAutoTeamBalance:
    m_autoteambalance = atoi(pArgs)!=0;
    break;
  case eGUC_filtersAntiCheat:
    m_anticheat = atoi(pArgs)!=0;
    break;
  case eGUC_filtersFriendlyFire:
    m_friendlyfire = atoi(pArgs)!=0;
    break;
  case eGUC_filtersGamepadsOnly:
    m_gamepadsonly = atoi(pArgs)!=0;
    break;
  case eGUC_filtersNoVoiceComms:
    m_novoicecomms = atoi(pArgs)!=0;
    break;
  case eGUC_filtersDedicated:
    m_dedicated = atoi(pArgs)!=0;
    break;
  case eGUC_filtersDX10:
    m_dx10 = atoi(pArgs)!=0;
    break;
  default:
    return false;
  }
	if(cmd != eGUC_filtersEnable && cmd != eGUC_filtersDisplay)//if we're changed any filters setting, enable them
	{
		EnableFilters(true);
	}
  //changed
  if(m_on || cmd==eGUC_filtersEnable)
  {
    m_parent->m_serverlist->m_dirty = true;
    m_parent->DisplayServerList();
  }
  return true;
}
