/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Script Binding for Item
  
 -------------------------------------------------------------------------
  History:
  - 27:10:2004   11:30 : Created by Márcio Martins

*************************************************************************/
#ifndef __SCRIPTBIND_ITEM_H__
#define __SCRIPTBIND_ITEM_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IScriptSystem.h>
#include <ScriptHelpers.h>


struct IItemSystem;
struct IGameFramework;
class CItem;
class CActor;


class CScriptBind_Item :
	public CScriptableBase
{
public:
	CScriptBind_Item(ISystem *pSystem, IGameFramework *pGameFramework);
	virtual ~CScriptBind_Item();

	void AttachTo(CItem *pItem);

	int SetExtensionActivation(IFunctionHandler *pH, const char *extension, bool activation);
	int SetExtensionParams(IFunctionHandler* pH, const char *extension, SmartScriptTable params);
	int GetExtensionParams(IFunctionHandler* pH, const char *extension, SmartScriptTable params);

	int Select(IFunctionHandler *pH, bool select);
	int GetStats(IFunctionHandler *pH);
	int GetParams(IFunctionHandler *pH);

	int Reset(IFunctionHandler *pH);
	int Quiet(IFunctionHandler *pH);

	int CanPickUp(IFunctionHandler *pH, ScriptHandle userId);
	int CanUse(IFunctionHandler *pH, ScriptHandle userId);
	int IsMounted(IFunctionHandler *pH);
	
	int PlayAction(IFunctionHandler *pH, const char *actionName);

	int GetOwnerId(IFunctionHandler *pH);
	int StartUse(IFunctionHandler *pH, ScriptHandle userId);
	int StopUse(IFunctionHandler *pH, ScriptHandle userId);
	int Use(IFunctionHandler *pH, ScriptHandle userId);
	int IsUsed(IFunctionHandler *pH);
	int OnUsed(IFunctionHandler *pH, ScriptHandle userId);

	int GetMountedDir(IFunctionHandler *pH);
	int GetMountedAngleLimits(IFunctionHandler *pH);
	int SetMountedAngleLimits(IFunctionHandler *pH, float min_pitch, float max_pitch, float yaw_range);

  int OnHit(IFunctionHandler *pH, SmartScriptTable hitTable);
  int IsDestroyed(IFunctionHandler *pH);
	int GetHealth(IFunctionHandler *pH);
	int GetMaxHealth(IFunctionHandler *pH);

private:
	void RegisterGlobals();
	void RegisterMethods();

	CItem *GetItem(IFunctionHandler *pH);
	CActor *GetActor(EntityId actorId);

	ISystem						*m_pSystem;
	IScriptSystem			*m_pSS;
	IGameFramework		*m_pGameFW;

	SmartScriptTable	m_stats;
	SmartScriptTable	m_params;
};


#endif //__SCRIPTBIND_ITEM_H__