/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Quick game screen
-------------------------------------------------------------------------
History:
- 03/12/2006: Created by Stas Spivakov

*************************************************************************/

#ifndef __QUICKGAME_H__
#define __QUICKGAME_H__

#pragma once

struct IServerBrowser;
class  CMPHub;
class  CQuickGameDlg;

class CQuickGame
{
  struct SQGServerList;
public:
  CQuickGame();
  ~CQuickGame();
  void StartSearch(CMPHub* hub);
  void Cancel();
  void NextStage();
  int  GetStage()const;
  bool IsSearching()const;
private:
  IServerBrowser* m_browser;
  int             m_stage;
  bool            m_searching;
  std::auto_ptr<SQGServerList>  m_list;
  std::auto_ptr<CQuickGameDlg>  m_ui;
};

#endif //__QUICKGAME_H__