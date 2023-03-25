/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: HUD text chat - receives text input and broadcasts it to other players

-------------------------------------------------------------------------
History:
- 07:03:2006: Created by Jan Müller

*************************************************************************/
#ifndef __HUDTEXTCHAT_H__
#define __HUDTEXTCHAT_H__

//-----------------------------------------------------------------------------------------------------

#include "HUDObject.h"
#include <IInput.h>
#include "IFlashPlayer.h"
#include "IActionMapManager.h"

class CGameFlashAnimation;

static const int CHAT_LENGTH = 6;


class CHUDTextChat : public CHUDObject,public IInputEventListener, public IFSCommandHandler
{

	typedef bool (CHUDTextChat::*OpFuncPtr) (const char*, const char*);

public:
	CHUDTextChat();

	~CHUDTextChat();

	void Init(CGameFlashAnimation *pFlashChat);

	// IFSCommandHandler
	virtual void HandleFSCommand(const char* pCommand, const char* pArgs);
	// ~IFSCommandHandler

	virtual void Update(float deltaTime);

	void GetMemoryStatistics(ICrySizer * s);

	// IInputEventListener
	virtual bool OnInputEvent(const SInputEvent &event );
	virtual bool OnInputEventUI(const SInputEvent &event );
	// ~IInputEventListener

	//add arriving multiplayer chat messages here
	virtual void AddChatMessage(EntityId sourceId, const wchar_t* msg, int teamFaction, bool teamChat);
	virtual void AddChatMessage(EntityId sourceId, const char* msg, int teamFaction, bool teamChat);
	virtual void AddChatMessage(const char* nick, const wchar_t* msg, int teamFaction, bool teamChat);
  virtual void AddChatMessage(const char* nick, const char* msg, int teamFaction, bool teamChat);

	ILINE virtual void ShutDown() {Flush();};

	//open/close chat
	void OpenChat(int type);
	ILINE void CloseChat() {Flush();};

private:
	virtual void Flush(bool close=true);
	virtual void ProcessInput(const SInputEvent &event);
	virtual void Delete();
	virtual void Backspace();
	virtual void Left();
	virtual void Right();
	virtual void Insert(const char *key);
	virtual void VirtualKeyboardInput(const char* direction);
	virtual bool ProcessCommands(const string& text);
	virtual bool Vote(const char* type=NULL, const char* kickuser=NULL);
	virtual bool VoteYes(const char* param1=NULL, const char* param2=NULL);
	virtual bool VoteNo(const char* param1=NULL, const char* param2=NULL);
	virtual bool Lowtec(const char* param1=NULL, const char* param2=NULL);
	virtual bool Quarantine(const char* param1=NULL, const char* param2=NULL);

	//option-function mapper
	typedef std::map<string, OpFuncPtr> TOpFuncMap;
	TOpFuncMap	m_opFuncMap;
	typedef TOpFuncMap::iterator TOpFuncMapIt;

	CGameFlashAnimation *m_flashChat;

	string				m_inputText;
	string				m_lastInputText;
	int						m_cursor;
	bool					m_isListening;
	bool					m_teamChat;

	string				m_chatStrings[CHAT_LENGTH];
	float					m_chatSpawnTime[CHAT_LENGTH];
	int						m_chatHead;

	bool					m_textInputActive;
	bool					m_showVirtualKeyboard;

	bool					m_anyCurrentText;
	/*bool					m_showing;
	float					m_lastUpdate;*/
	float					m_repeatTimer;
	SInputEvent		m_repeatEvent;
};

#endif