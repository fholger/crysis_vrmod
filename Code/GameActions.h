// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#ifndef __GAMEACTIONS_H__
#define __GAMEACTIONS_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <IActionMapManager.h>

#define DECL_ACTION(name) ActionId name;
class CGameActions
{
public:
	CGameActions();
#include "GameActions.actions"

	void Init();
	ILINE IActionFilter*	FilterNoMove() const {	return m_pFilterNoMove;	}
	ILINE IActionFilter*	FilterNoMouse() const {	return m_pFilterNoMouse;	}
	ILINE IActionFilter*	FilterNoGrenades() const {	return m_pFilterNoGrenades;	}
	ILINE IActionFilter*	FilterInVehicleSuitMenu() const {	return m_pFilterInVehicleSuitMenu;	}
	ILINE IActionFilter*	FilterSuitMenu() const {	return m_pFilterSuitMenu;	}
	ILINE IActionFilter*	FilterFreezeTime() const {	return m_pFilterFreezeTime;	}
	ILINE IActionFilter*	FilterNoVehicleExit() const {	return m_pFilterNoVehicleExit;	}
	ILINE IActionFilter*	FilterMPRadio() const {	return m_pFilterMPRadio;	}
	ILINE IActionFilter*	FilterCutscene() const {	return m_pFilterCutscene;	}
	ILINE IActionFilter*	FilterCutsceneNoPlayer() const {	return m_pFilterCutsceneNoPlayer;	}
	ILINE IActionFilter*	FilterNoMapOpen() const {	return m_pFilterNoMapOpen;	}
	ILINE IActionFilter*	FilterNoObjectivesOpen() const {	return m_pFilterNoObjectivesOpen;	}
	ILINE IActionFilter*	FilterVehicleNoSeatChangeAndExit() const {	return m_pFilterVehicleNoSeatChangeAndExit;	}
	ILINE IActionFilter*	FilterNoConnectivity() const {	return m_pFilterNoConnectivity;	}


private:
	void	CreateFilterNoMove();
	void	CreateFilterNoMouse();
	void  CreateFilterNoGrenades();
	void	CreateFilterInVehicleSuitMenu();
	void	CreateFilterSuitMenu();
	void	CreateFilterFreezeTime();
	void	CreateFilterNoVehicleExit();
	void	CreateFilterMPRadio();
	void	CreateFilterCutscene();
	void	CreateFilterCutsceneNoPlayer();
	void	CreateFilterNoMapOpen();
	void	CreateFilterNoObjectivesOpen();
	void	CreateFilterVehicleNoSeatChangeAndExit();
	void	CreateFilterNoConnectivity();

	IActionFilter*	m_pFilterNoMove;
	IActionFilter*	m_pFilterNoMouse;
	IActionFilter*	m_pFilterNoGrenades;
	IActionFilter*	m_pFilterInVehicleSuitMenu;
	IActionFilter*	m_pFilterSuitMenu;
	IActionFilter*	m_pFilterFreezeTime;
	IActionFilter*	m_pFilterNoVehicleExit;
	IActionFilter*	m_pFilterMPRadio;
	IActionFilter*	m_pFilterCutscene;
	IActionFilter*	m_pFilterCutsceneNoPlayer;
	IActionFilter*	m_pFilterNoMapOpen;
	IActionFilter*	m_pFilterNoObjectivesOpen;
	IActionFilter*	m_pFilterVehicleNoSeatChangeAndExit;
	IActionFilter*	m_pFilterNoConnectivity;
};
#undef DECL_ACTION

extern CGameActions* g_pGameActions;

#endif //__GAMEACTIONS_H__
