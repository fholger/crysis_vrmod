/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2006.
-------------------------------------------------------------------------

Description: 
	Header for CTweakTraverser

-------------------------------------------------------------------------
History:
- 28:02:2006  : Created by Matthew Jack

*************************************************************************/


#ifndef __CTWEAKTRAVERSER_H__
#define __CTWEAKTRAVERSER_H__

#pragma once

//-------------------------------------------------------------------------

#include "TweakTraverser.h"

//-------------------------------------------------------------------------

class CTweakCommon;
class CTweakMenu;

//-------------------------------------------------------------------------


class CTweakTraverser {
public:

	friend class CTweakMenu;

	// Create an empty Traverser
	CTweakTraverser(void);

	// Create a Traverser, registering it in the process
	CTweakTraverser(CTweakMenu *root);

	// Copy a Traverser, registering the copy
	CTweakTraverser(const CTweakTraverser &that);

	// Copy a Traverser, registering the copy
	void operator= (const CTweakTraverser &that);

	// Deregister and destroy
	~CTweakTraverser();

	// Compare Traversers to see if they point to the same item
	// If both unregistered, returns true
	bool operator== (const CTweakTraverser &that);

	void Register(CTweakMenu * root);
	void Deregister(void);
	bool IsRegistered(void) const;

	bool Previous(void);		// Go to previous item (true) or to before-start (false)
	bool Next(void);				// Go to next item (true) or after-end (false)
	bool Forward(void);			// If a submenu item move into it (true) or do nothing (false)
	bool Back(void);				// If above root go back one level (true) or do nothing (false)
	void Top(void);					// Go to before-start
	void Bottom(void);			// Go to after-end
	bool First(void);				// Go to first item if any (true) or to before-start (false)
	bool Last(void);				// Go to last item if any (true) or to after-end (false)

	// If you can find this item go there (true) or do nothing (false)
	bool Goto(const char *name);

	// Return whatever item we are currently pointing to
	CTweakCommon *GetItem(void) const;

	// Return the submenu we are currently in
	CTweakMenu *GetMenu(void) const;

protected:
	// Wrapped fetch of menu vector from the relevant CTweakMenu instance
	std::vector<CTweakCommon*> &GetMenuItems(void) const;

	// Pointers to submenus along our path
	// The traverser is only valid while we at least have a root for the path
	std::vector<CTweakMenu*> m_menuPath;

	// The index of the current item
	int m_index;

	const static int START_INDEX;
};

#endif // __CTWEAKCOMMON_H__