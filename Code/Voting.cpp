/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 4:23:2007   : Created by Stas Spivakov

*************************************************************************/

#include "StdAfx.h"
#include "Voting.h"

CVotingSystem::CVotingSystem():
m_state(eVS_none),
m_id(0)
{

}

CVotingSystem::~CVotingSystem()
{

}

bool  CVotingSystem::StartVoting(int id, const CTimeValue& start, EVotingState st, EntityId eid, const char* subj, int team)
{
  if(st == eVS_none)
    return false;
  if(IsInProgress())
    return false;
  SVoting v;
  v.initiator = id;
  v.startTime = start;
  m_votings.push_back(v);
  m_votes.resize(0);
	m_numVotes = 0;
  m_teamVotes = 0;

	m_subject = subj?subj:"";
  m_state = st;
  m_id = eid;
  m_team = team;

  m_startTime = start;
  return true;
}

void  CVotingSystem::EndVoting()
{
  m_votes.resize(0);
  m_subject.resize(0);
  m_team = 0;
	m_numVotes = 0;
  m_teamVotes = 0;
  m_state = eVS_none;
  m_id = 0;
}

bool  CVotingSystem::GetCooldownTime(int id, CTimeValue& v)
{
  for(int i=m_votings.size()-1;i>=0;--i)
    if(m_votings[i].initiator==id)
    {
      v = m_votings[i].startTime;
      return true;
    }
  return false;
}

bool  CVotingSystem::IsInProgress()const
{
  return m_state!=eVS_none;
}

int   CVotingSystem::GetNumVotes()const
{
  return m_numVotes;
}

int   CVotingSystem::GetTeam()const
{
  return m_team;
}

int   CVotingSystem::GetNumTeamVotes()const
{
  return m_teamVotes;
}

const string& CVotingSystem::GetSubject()const
{
  return m_subject;
}

EntityId CVotingSystem::GetEntityId()const
{
  return m_id;
}

EVotingState CVotingSystem::GetType()const
{
  return m_state;
}

CTimeValue CVotingSystem::GetVotingTime()const
{
  return gEnv->pTimer->GetFrameStartTime()-m_startTime;
}

void  CVotingSystem::Reset()
{
  EndVoting();
  m_votings.resize(0);
}

//clients can vote
void  CVotingSystem::Vote(int id, int team, bool yes)
{
  if(CanVote(id))
  {
    m_votes.push_back(id);
		if(yes)
		{
			++m_numVotes;
			if(m_team == team)
				++m_teamVotes;
		}
  }
}

bool  CVotingSystem::CanVote(int id)const
{
  for(int i=0;i<m_votes.size(); ++i)
    if(m_votes[i] == id)
      return false;
  return true;
}