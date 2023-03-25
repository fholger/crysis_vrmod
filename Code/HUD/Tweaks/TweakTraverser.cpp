/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2006.
-------------------------------------------------------------------------

Description: 
	Handle class for traversing a Tweak menu structure

-------------------------------------------------------------------------
History:
- 28:02:2006  : Created by Matthew Jack

*************************************************************************/

#include "StdAfx.h"
#include "TweakTraverser.h"
#include "TweakCommon.h"
#include "TweakMenu.h"
#include "IScriptSystem.h"
#include <string>

//-------------------------------------------------------------------------

const int CTweakTraverser::START_INDEX = -1;



CTweakTraverser::CTweakTraverser(void) {
	m_index = START_INDEX;
}


CTweakTraverser::CTweakTraverser(CTweakMenu * root)
{
	// Register with the root
	Register(root);
}

//-------------------------------------------------------------------------

CTweakTraverser::CTweakTraverser(const CTweakTraverser &that) {
	// Could this be done jsut by calling assignment?
	if (that.IsRegistered()) {
		Register(that.m_menuPath[0]);
	}
	m_menuPath = that.m_menuPath;
	m_index = that.m_index;
}

//-------------------------------------------------------------------------

void CTweakTraverser::operator= (const CTweakTraverser &that) {
	Deregister();

	if (that.IsRegistered()) {
		Register(that.m_menuPath[0]);
	}
	m_menuPath = that.m_menuPath;
	m_index = that.m_index;
}

//-------------------------------------------------------------------------

CTweakTraverser::~CTweakTraverser() {
	// Deregister ourselves
	Deregister();
}

//-------------------------------------------------------------------------

void CTweakTraverser::Register(CTweakMenu * root) {
	// Remove any existing registration
	Deregister();

	// Make this menu our path root
	m_menuPath.push_back(root);

	// Register ourselves with the root
	m_menuPath[0]->RegisterTraverser(this);
	
	// Go to top of that menu
	Top();
}

//-------------------------------------------------------------------------

void CTweakTraverser::Deregister(void) {
	// Deregister from root menu
	if (!m_menuPath.empty()) m_menuPath[0]->DeregisterTraverser(this);

	// Clear the menu path
	m_menuPath.clear();

	// Set index to known value
	m_index = START_INDEX;
}

//-------------------------------------------------------------------------

bool CTweakTraverser::Next(void) {
	m_index++;
	if ( m_index >= GetMenuItems().size() ) {
		// Passed last element
		m_index = GetMenuItems().size();
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------

bool CTweakTraverser::Previous(void) {
	m_index--;
	if ( m_index <= START_INDEX ) {
		// Passed first element
		m_index = START_INDEX;
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------

bool CTweakTraverser::Forward(void) {
	// It there a valid item selected?
	if (!GetItem()) return false;

	// Is it a menu?
	if (GetItem()->GetType() != CTweakCommon::eTT_Menu) return false;

	// Traverse into that menu	
	CTweakMenu *submenu = (CTweakMenu*) GetItem();
	m_menuPath.push_back(submenu);

	// Reset index
	Top();
	return true;
}

//-------------------------------------------------------------------------

bool CTweakTraverser::Back(void) {
	// Is there a parent menu to traverse back to?
	if (m_menuPath.size() <= 1) return false;

	// Get the name of this menu
	const char * name = GetMenu()->GetName().c_str();

	// Go back one level
	m_menuPath.pop_back();

	// Try and find that menu
	Goto(name);

	// Reset index
	//Top();
	return true;
}

//-------------------------------------------------------------------------

void CTweakTraverser::Top(void) {
	// Always set to index before possible items
	m_index = START_INDEX;
}


//-------------------------------------------------------------------------

void CTweakTraverser::Bottom(void) {
	m_index = START_INDEX;  // For empty list, START_INDEX == after-end index
	
	if (IsRegistered())	m_index = GetMenuItems().size();
}

bool CTweakTraverser::First(void) {
	if (! IsRegistered())	return false;
	
	if (GetMenuItems().size() == 0) {
		m_index = START_INDEX;
		return false;
	}

	m_index = 0; 
	return true;
}


bool CTweakTraverser::Last(void) {
	if (! IsRegistered())	return false;

	m_index = GetMenuItems().size();
	if (m_index == 0) m_index = START_INDEX;
	return true;
}


//-------------------------------------------------------------------------

bool CTweakTraverser::Goto(const char *name) {
	// This could become more efficient, by storing Traversers in path, or other ways
	
	// Create a temporary Traverser
	CTweakTraverser t = (*this);
	t.Top();
	while (t.Next()) {
		if (t.GetItem()->GetName() == name) {
			(*this) = t;
			return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------

CTweakCommon * CTweakTraverser::GetItem(void) const {
	// Lower bounds check
	if (m_index == START_INDEX) return NULL;
	// Upper bounds check
	if (m_index >= GetMenuItems().size()) return NULL;
	// Return item
	return GetMenuItems()[m_index];
}

//-------------------------------------------------------------------------

CTweakMenu * CTweakTraverser::GetMenu(void) const {
	if (m_menuPath.empty()) return NULL;
	return m_menuPath.back();
}

//-------------------------------------------------------------------------


bool CTweakTraverser::IsRegistered(void) const {
	return (!m_menuPath.empty());
}

//-------------------------------------------------------------------------

// Note that this is _only_ for internal use!
std::vector<CTweakCommon*> & CTweakTraverser::GetMenuItems(void) const {
	CTweakMenu * menu = m_menuPath.back();
	return menu->m_items;
}

//-------------------------------------------------------------------------

bool CTweakTraverser::operator== (const CTweakTraverser &that) {
	bool thisReg = IsRegistered();
	bool thatReg = that.IsRegistered();
	if (!thisReg && !thatReg) return true;
	if (thisReg ^ thatReg) return false;

	// Otherwise both are registered and must compare contents
	// Note that registered Traversers will always have non-empty m_menuPath
	// Size check isn't really necessary, assuming acyclic menus!
	return (m_index == that.m_index &&
		m_menuPath.size() == that.m_menuPath.size() &&
		m_menuPath.back() == that.m_menuPath.back());
}


