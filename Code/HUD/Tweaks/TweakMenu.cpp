/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2006.
-------------------------------------------------------------------------

Description: 
	Allows getting structured Tweak metadata, input from LUA script

-------------------------------------------------------------------------
History:
- 28:02:2006  : Created by Matthew Jack

*************************************************************************/

#include "StdAfx.h"
#include "TweakMenu.h"
#include "TweakTraverser.h"
#include "IScriptSystem.h"


//-------------------------------------------------------------------------

CTweakMenu::CTweakMenu(IScriptSystem *pScriptSystem) : CTweakCommon("Tweaks")
{
	SmartScriptTable rootTable;
	if (pScriptSystem->GetGlobalValue("Tweaks",rootTable)) {
		init(*rootTable); // Strip away Smart wrapper and initialise
	}
}

//-------------------------------------------------------------------------

CTweakMenu::CTweakMenu(IScriptTable *pTable) {
	// Identify the name of this menu
	m_sName = FetchStringValue(pTable, "MENU");
	if (m_sName.empty()) m_sName = "Unnamed menu";

	// Perform the rest of the initialisation
	init(pTable);
}

//-------------------------------------------------------------------------

bool CTweakMenu::init(IScriptTable *pTable) {
	// TODO: Sort into index order to maintain order from file

	// A vector for properly numbered submenu elements
	std::vector <CTweakMenu*> submenus; 

	// A vector for properly numbered metadata elements
	std::vector <CTweakMetadata*> metadata; 

	// A vector for unrecognised elements
	std::vector <CTweakCommon*> unrecognised;

	// Identify and recurse on each element of the table
	IScriptTable::Iterator iter = pTable->BeginIteration();
	while (pTable->MoveNext(iter)) {

		// Get the name of this element
		const char *sKey = iter.sKey;

		// Is it a table?
		if (iter.value.type == ANY_TTABLE) {
			CTweakCommon *newComponent = GetNewTweak(iter.value.table);

			switch (newComponent->GetType()) {
				case eTT_Metadata:
					metadata.push_back( (CTweakMetadata*) newComponent);
					break;
				case eTT_Menu:
					submenus.push_back( (CTweakMenu*) newComponent);
					break;
				default:
					unrecognised.push_back( (CTweakCommon*) newComponent);
			}
		}

		// If not, for now, ignore

	}
	pTable->EndIteration(iter);

	// Put each class of item into the menus
	m_items.insert(m_items.end(), submenus.begin(), submenus.end());
	m_items.insert(m_items.end(), metadata.begin(), metadata.end());
	m_items.insert(m_items.end(), unrecognised.begin(), unrecognised.end());

	return true;
}

//-------------------------------------------------------------------------

CTweakMenu::~CTweakMenu() {
	// Deregister all the Traversers
	// Bit of a workaround here to avoid invalidation 
	std::vector<CTweakTraverser*> temp;
	for (std::set<CTweakTraverser*>::iterator it = m_traversers.begin(); it != m_traversers.end(); it++) 
		temp.push_back(*it);
	for (std::vector<CTweakTraverser*>::iterator it = temp.begin(); it != temp.end(); it++) 
		(*it)->Deregister();
	
	for (std::vector<CTweakCommon*>::iterator it = m_items.begin(); it != m_items.end(); it++) 
		SAFE_DELETE(*it);
}


//-------------------------------------------------------------------------


// Register a Traverser with this menu
void CTweakMenu::RegisterTraverser( CTweakTraverser * p_traverser ) {
	m_traversers.insert(p_traverser);
}

//-------------------------------------------------------------------------

// Deregister a Traverser with this menu
bool CTweakMenu::DeregisterTraverser( CTweakTraverser * p_traverser  ) {
	return ( 0 != m_traversers.erase(p_traverser) );
}

//-------------------------------------------------------------------------

CTweakTraverser CTweakMenu::GetTraverser(void) {
	return CTweakTraverser(this);
}


//-------------------------------------------------------------------------


void CTweakMenu::StoreChanges( IScriptTable *pTable )  {
	for (std::vector<CTweakCommon*>::iterator it = m_items.begin(); it != m_items.end(); it++)
		(*it)->StoreChanges( pTable );
}


//-------------------------------------------------------------------------

void CTweakMenu::Init(void) {
	for (std::vector<CTweakCommon*>::iterator it = m_items.begin(); it != m_items.end(); it++)
		(*it)->Init();
	m_bIsInit = true;
}

//-------------------------------------------------------------------------
