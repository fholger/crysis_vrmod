/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2006.
-------------------------------------------------------------------------

Description: 
	Header for CTweakMenu

-------------------------------------------------------------------------
History:
- 28:02:2006  : Created by Matthew Jack

Notes:
	I don't like this method of accessing the container. Need a really simple general purpose iterator

*************************************************************************/

#ifndef __CTWEAKMENU_H__
#define __CTWEAKMENU_H__

#pragma once

#include "TweakMetadata.h"
#include <vector>
#include <set>

//-------------------------------------------------------------------------
// Forward declarations

struct IScriptSystem;
class SmartScriptTable;
class CTweakTraverser;

//-------------------------------------------------------------------------


class CTweakMenu : public CTweakCommon{
public:

	friend class CTweakTraverser;

	// Used for recursively creating the entire menu from the root
	CTweakMenu(IScriptSystem *pScriptSystem);
	
	// Recursively creates a submenu tree from a Tweak menu table
	CTweakMenu(IScriptTable *pTable);

	// Destructor
	~CTweakMenu();

	// Get a traverser 
	CTweakTraverser GetTraverser(void);

	// Register a Traverser with this menu
	void RegisterTraverser( CTweakTraverser * traverser );

	// Deregister a Traverser with this menu
	bool DeregisterTraverser( CTweakTraverser * traverser );

	// Get the type of the instance
	ETweakType GetType(void) { return eTT_Menu; }

	void StoreChanges( IScriptTable *pTable );

	void Init(void);

protected:

	// (Re)initialisation
	bool init(IScriptTable *pTable);

	// Items in this menu
	std::vector<CTweakCommon*> m_items;

	// Traversers associated with this menu
	std::set<CTweakTraverser*> m_traversers;

};

#endif  // __CTWEAKMENU_H__
