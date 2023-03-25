/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 4:23:2007   : Created by Stas Spivakov

*************************************************************************/

#ifndef __VOTING_H__
#define __VOTING_H__

#pragma once

// Summary
//  Types for the different vote states
enum EVotingState
{
  eVS_none=0,//no voting is currently held
  eVS_kick,//kick vote
  eVS_nextMap,//next map vote
  eVS_changeMap,//change map vote
  eVS_consoleCmd,//execute arbitrary console cmd
  eVS_last//this should be always last!
};

struct SVotingParams
{
  SVotingParams();

};

class CVotingSystem
{
  struct SVoting//once it is initiated
  {
    int         initiator;
    CTimeValue  startTime;
  };
public:
  CVotingSystem();
  ~CVotingSystem();
  bool  StartVoting(int id, const CTimeValue& start, EVotingState st, EntityId eid, const char* subj, int team);
  void  EndVoting();
  bool  GetCooldownTime(int id, CTimeValue& v);
  
  bool  IsInProgress()const;
  int   GetNumVotes()const;
  int   GetNumTeamVotes()const;

  int   GetTeam()const;
  int   GetTeamVotes()const;

  const string& GetSubject()const;
  EntityId GetEntityId()const;
  EVotingState GetType()const;
  
  CTimeValue GetVotingTime()const;

  void  Reset();

  //clients can vote
  void  Vote(int id, int team, bool yes);
  bool  CanVote(int id)const;
private:
  CTimeValue            m_startTime;
  
  EVotingState          m_state;
  string                m_subject;
  EntityId              m_id;
  int                   m_team;

  std::vector<int>      m_votes;
  int                   m_teamVotes;
	int										m_numVotes;
  //recent voting
  std::vector<SVoting>  m_votings;
};

#endif // #ifndef __VOTING_H__