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

#ifndef __GAMENETWORKPROFILE_H__
#define __GAMENETWORKPROFILE_H__

#pragma once

#include "INetworkService.h"

class CMPHub;

enum EChatUserType
{
  eCUT_buddy,
  eCUT_general,
  eCUT_ignore
};

struct SChatUser
{
  int         m_id;
  string      m_nick;
  EUserStatus m_status;
  string      m_location;
	bool				m_foreign;
};

enum EUserInfoSource
{
	eUIS_backend,
	eUIS_chat
};

struct SUserStats
{
	SUserStats():m_played(0),m_accuracy(0),m_killsPerMinute(0),m_kills(0),m_deaths(0)
	{}
	int     m_played;
	string	m_gameMode;
	string  m_map;
	string	m_weapon;
	string	m_vehicle;
	string  m_suitMode;
	float		m_accuracy;
	float		m_killsPerMinute;
	int			m_kills;
	int			m_deaths;
};

struct SUserInfo
{
	SUserInfo():
	m_basic(true),m_foreignName(false)
	{
	}

  string						m_nick; 
  string						m_country;
  
	SUserStats				m_stats;
	// flag for displaying
	bool							m_foreignName;
	//
	bool							m_basic;//if false - means we have full info
	EUserInfoSource		m_source;//source... - info will be re-read from backend in some cases if this is eUIS_chat
	CTimeValue				m_expires;//non-basic information expires at this point
};

struct SStoredUserInfo
{
	int			m_kills;
	int			m_deaths;
	int			m_hits;
	int			m_shots;
	int     m_played;
	int			m_mode;
	int			m_map;
	int			m_weapon;
	int			m_vehicle;
	int			m_suitmode;
	void		Serialize(struct IStoredSerialize* ser);
};

struct INProfileUI
{
  virtual void AddBuddy(SChatUser s)=0;
  virtual void UpdateBuddy(SChatUser s)=0;
  virtual void RemoveBuddy(int id)=0;
  virtual void AddIgnore(SChatUser s)=0;
  virtual void RemoveIgnore(int id)=0;
  virtual void ProfileInfo(int id, SUserInfo ui)=0;
  virtual void SearchResult(int id, const char* nick)=0;
  virtual void OnMessage(int id, const char* message)=0;
  virtual void SearchCompleted()=0;
  virtual void ShowError(const char* descr)=0;
  virtual void OnAuthRequest(int id, const char* nick, const char* message)=0;
  virtual void AddFavoriteServer(uint ip, ushort port)=0;
  virtual void AddRecentServer(uint ip, ushort port)=0;
};

struct SStoredServer
{
  SStoredServer(uint i = 0, ushort p = 0):ip(i),port(p){}
  int			ip;
  int			port;
	bool operator==(const SStoredServer& svr)
	{
		return ip == svr.ip && port == svr.port;
	}
	void Serialize(struct IStoredSerialize* ser);
};

static const char* AUTO_INVITE_TEXT  = "Crysis auto generated invitation";

class CGameNetworkProfile
{
private:
  typedef std::map<int,SUserInfo> TUserInfoMap;

	template<class T>
	class TStoredArray;

  struct SBuddies;
  struct SChat;
  struct SChatText;
  struct SStoredServerLists;
	struct SUserInfoReader;

	struct SStorageQuery
  {
    SStorageQuery(CGameNetworkProfile* p):m_parent(p)	
    {
      m_parent->m_queries.push_back(this);
    }
    virtual ~SStorageQuery()
    {
      if(m_parent)
			{
        stl::find_and_erase(m_parent->m_queries,this);
				m_parent->OnEndQuery();
			}
    }
    CGameNetworkProfile *m_parent;
  };

  typedef std::map<int, SChatText> TChatTextMap;
public:
  CGameNetworkProfile(CMPHub* hub);
  ~CGameNetworkProfile();

  void Login(const char* login, const char* password);
	void LoginProfile(const char* email, const char* password, const char* profile);
  void Register(const char* login, const char* email, const char* pass, const char* country, SRegisterDayOfBirth dob);
  void Logoff();
  
  void AcceptBuddy(int id, bool accept);
  void RequestBuddy(const char* nick, const char* reason);
  void RequestBuddy(int id, const char* reason);
  void RemoveBuddy(const char* nick);
  void AddIgnore(const char* nick);
  void StopIgnore(const char* nick);
  void QueryUserInfo(const char* nick);
  void QueryUserInfo(int id);
	void SendBuddyMessage(int id, const char* message);
  void SearchUsers(const char* nick);
  bool CanInvite(const char* nick);
  bool CanIgnore(const char* nick);
	bool IsIgnored(const char* nick);
  bool CanInvite(int id);
  bool CanIgnore(int id);
	bool GetUserInfo(int id, SUserInfo& info);
	bool GetUserInfo(const char* nick, SUserInfo& info, int &id);

  void SetPlayingStatus(uint ip, ushort port, ushort publicport, const char* game_type);
  void SetChattingStatus();

	void OnUserStats(int id, const SUserStats& stats, EUserInfoSource src, const char* country ="");
	void OnUserNick(int profile, const char* user);
	  
  void InitUI(INProfileUI* a);
  void DestroyUI();

  void AddFavoriteServer(uint ip, ushort port);
  void RemoveFavoriteServer(uint ip, ushort port);
  void AddRecentServer(uint ip, ushort port);

  bool IsLoggedIn()const;
	bool IsLoggingIn()const;
	const char* GetLogin()const;
	const char* GetPassword()const;
	const char* GetCountry()const;
	const int GetProfileId()const;

	void RetrievePassword(const char *email);
	bool IsDead()const;

	const SUserStats&	GetMyStats()const;

	bool GetFavoriteServers(std::vector<SStoredServer>& svr);  
private:
  void OnLoggedIn(int id, const char* nick);
	void OnEndQuery();
  void CleanUpQueries();

private:
  CMPHub*                 m_hub;
  INetworkProfile*        m_profile;
  string                  m_login;
	string									m_password;
	string									m_country;
  int                     m_profileId;
	bool										m_loggingIn;
	SUserStats							m_stats;

  std::auto_ptr<SBuddies>           m_buddies;
  std::auto_ptr<SStoredServerLists> m_stroredServers;
	std::auto_ptr<SUserInfoReader>		m_infoReader;
  std::vector<SStorageQuery*>       m_queries;
};

#endif //__GAMENETWORKPROFILE_H__
