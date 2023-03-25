/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 27:10:2004   11:29 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ScriptBind_Item.h"
#include "Item.h"
#include "IGameObject.h"
#include "Actor.h"


#define REUSE_VECTOR(table, name, value)	\
	{ if (table->GetValueType(name) != svtObject) \
	{ \
	table->SetValue(name, (value)); \
	} \
		else \
	{ \
	SmartScriptTable v; \
	table->GetValue(name, v); \
	v->SetValue("x", (value).x); \
	v->SetValue("y", (value).y); \
	v->SetValue("z", (value).z); \
	} \
	}


//------------------------------------------------------------------------
CScriptBind_Item::CScriptBind_Item(ISystem *pSystem, IGameFramework *pGameFramework)
: m_pSystem(pSystem),
	m_pSS(pSystem->GetIScriptSystem()),
	m_pGameFW(pGameFramework)
{
	Init(m_pSS, m_pSystem, 1);

	RegisterMethods();
	RegisterGlobals();

	m_stats.Create(m_pSystem->GetIScriptSystem());
	m_params.Create(m_pSystem->GetIScriptSystem());
}

//------------------------------------------------------------------------
CScriptBind_Item::~CScriptBind_Item()
{
}

//------------------------------------------------------------------------
void CScriptBind_Item::AttachTo(CItem *pItem)
{
	IScriptTable *pScriptTable = pItem->GetEntity()->GetScriptTable();

	if (pScriptTable)
	{
		SmartScriptTable thisTable(m_pSS);

		thisTable->SetValue("__this", ScriptHandle(pItem->GetEntityId()));
		thisTable->Delegate(GetMethodsTable());

		pScriptTable->SetValue("item", thisTable);
	}
}

//------------------------------------------------------------------------
void CScriptBind_Item::RegisterGlobals()
{
}

//------------------------------------------------------------------------
void CScriptBind_Item::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Item::
	SCRIPT_REG_TEMPLFUNC(SetExtensionActivation, "extension, bActivate");
	SCRIPT_REG_TEMPLFUNC(SetExtensionParams, "extension, params");
	SCRIPT_REG_TEMPLFUNC(GetExtensionParams, "extension, params");
	SCRIPT_REG_TEMPLFUNC(GetStats, "");
	SCRIPT_REG_TEMPLFUNC(GetParams, "");
	SCRIPT_REG_TEMPLFUNC(Reset, "");
	SCRIPT_REG_TEMPLFUNC(Quiet, "");

	SCRIPT_REG_TEMPLFUNC(Select, "select");

	SCRIPT_REG_TEMPLFUNC(CanPickUp, "userId");
	SCRIPT_REG_TEMPLFUNC(CanUse, "userId");
	SCRIPT_REG_TEMPLFUNC(IsMounted, "");

	SCRIPT_REG_TEMPLFUNC(PlayAction, "actionName");

	SCRIPT_REG_TEMPLFUNC(GetOwnerId, "");
	SCRIPT_REG_TEMPLFUNC(StartUse, "userId");
	SCRIPT_REG_TEMPLFUNC(StopUse, "userId");
	SCRIPT_REG_TEMPLFUNC(Use, "userId");
	SCRIPT_REG_TEMPLFUNC(IsUsed, "");
 	SCRIPT_REG_TEMPLFUNC(GetMountedDir, "");
	SCRIPT_REG_TEMPLFUNC(GetMountedAngleLimits, "");
	SCRIPT_REG_TEMPLFUNC(SetMountedAngleLimits,"min_pitch, max_pitch, yaw_range");

   SCRIPT_REG_TEMPLFUNC(OnHit, "hit");
  SCRIPT_REG_TEMPLFUNC(IsDestroyed, "");
	SCRIPT_REG_TEMPLFUNC(OnUsed, "userId");

	SCRIPT_REG_TEMPLFUNC(GetHealth, "");
	SCRIPT_REG_TEMPLFUNC(GetMaxHealth, "");
}

//------------------------------------------------------------------------
CItem *CScriptBind_Item::GetItem(IFunctionHandler *pH)
{
	void *pThis = pH->GetThis();

	if (pThis)
	{
		IItem *pItem = m_pGameFW->GetIItemSystem()->GetItem((EntityId)(UINT_PTR)pThis);
		if (pItem)
			return static_cast<CItem *>(pItem);
	}

	return 0;
}

//------------------------------------------------------------------------
CActor *CScriptBind_Item::GetActor(EntityId actorId)
{
	return static_cast<CActor *>(m_pGameFW->GetIActorSystem()->GetActor(actorId));
}

//------------------------------------------------------------------------
int CScriptBind_Item::SetExtensionActivation(IFunctionHandler *pH, const char *extension, bool activation)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	bool ok = false;
	if (pItem)
	{
		if (activation)
			ok = pItem->GetGameObject()->ActivateExtension(extension);
		else
		{
			pItem->GetGameObject()->DeactivateExtension(extension);
			ok = true;
		}
	}
	if (!ok)
		pH->GetIScriptSystem()->RaiseError("Failed to %s extension %s", activation? "enable" : "disable", extension);
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Item::SetExtensionParams(IFunctionHandler* pH, const char *extension, SmartScriptTable params)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	bool ok = false;
	if (pItem)
		ok = pItem->GetGameObject()->SetExtensionParams(extension, params);
	if (!ok)
		pH->GetIScriptSystem()->RaiseError("Failed to set params for extension %s", extension);
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Item::GetExtensionParams(IFunctionHandler* pH, const char *extension, SmartScriptTable params)
{
	CItem * pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	bool ok = false;
	if (pItem)
		ok = pItem->GetGameObject()->GetExtensionParams(extension, params);
	if (!ok)
		pH->GetIScriptSystem()->RaiseError("Failed to set params for extension %s", extension);
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Item::Select(IFunctionHandler *pH, bool select)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	assert(pItem);

	pItem->Select(select);

	return pH->EndFunction();
}

#define GVALUE(table, struc, value)	table->SetValue(#value, struc.value)
#define SVALUE(table, struc, value)	table->SetValue(#value, struc.value.c_str())
//------------------------------------------------------------------------
int CScriptBind_Item::GetStats(IFunctionHandler *pH)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();
	GVALUE(m_stats, pItem->m_stats, fp);
	GVALUE(m_stats, pItem->m_stats, mounted);
	GVALUE(m_stats, pItem->m_stats, pickable);
	GVALUE(m_stats, pItem->m_stats, dropped);
	GVALUE(m_stats, pItem->m_stats, flying);
	GVALUE(m_stats, pItem->m_stats, hand);

	return pH->EndFunction(m_stats);
}

//------------------------------------------------------------------------
int CScriptBind_Item::GetParams(IFunctionHandler *pH)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	GVALUE(m_params, pItem->m_params, selectable);
	GVALUE(m_params, pItem->m_params, droppable);
	GVALUE(m_params, pItem->m_params, pickable);
	GVALUE(m_params, pItem->m_params, mountable);
	GVALUE(m_params, pItem->m_params, usable);
	GVALUE(m_params, pItem->m_params, giveable);
	GVALUE(m_params, pItem->m_params, unique);
	GVALUE(m_params, pItem->m_params, arms);
	GVALUE(m_params, pItem->m_params, mass);
	GVALUE(m_params, pItem->m_params, drop_impulse);
	GVALUE(m_params, pItem->m_params, fly_timer);
	SVALUE(m_params, pItem->m_params, pose);
	m_params->SetValue("attachment_right", pItem->m_params.attachment[IItem::eIH_Right].c_str());
	m_params->SetValue("attachment_left", pItem->m_params.attachment[IItem::eIH_Left].c_str());

	return pH->EndFunction(m_params);
}

//------------------------------------------------------------------------
int CScriptBind_Item::Reset(IFunctionHandler *pH)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	pItem->Reset();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Item::Quiet(IFunctionHandler *pH)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	pItem->Quiet();

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Item::CanPickUp(IFunctionHandler *pH, ScriptHandle userId)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	return pH->EndFunction(pItem->CanPickUp(userId.n));
}

//------------------------------------------------------------------------
int CScriptBind_Item::CanUse(IFunctionHandler *pH, ScriptHandle userId)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	return pH->EndFunction(pItem->CanUse(userId.n));
}

//------------------------------------------------------------------------
int CScriptBind_Item::IsMounted(IFunctionHandler *pH)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	return pH->EndFunction(pItem->IsMounted());
}

//------------------------------------------------------------------------
int CScriptBind_Item::PlayAction(IFunctionHandler *pH, const char *actionName)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	pItem->PlayAction(actionName);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Item::GetOwnerId(IFunctionHandler *pH)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	return pH->EndFunction(ScriptHandle(pItem->GetOwnerId()));
}

//------------------------------------------------------------------------
int CScriptBind_Item::StartUse(IFunctionHandler *pH, ScriptHandle userId)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	pItem->StartUse(userId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Item::StopUse(IFunctionHandler *pH, ScriptHandle userId)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	pItem->StopUse(userId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Item::Use(IFunctionHandler *pH, ScriptHandle userId)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	pItem->Use(userId.n);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Item::IsUsed(IFunctionHandler *pH)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	return pH->EndFunction(pItem->IsUsed());
}

//------------------------------------------------------------------------
int CScriptBind_Item::GetMountedDir(IFunctionHandler *pH)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	return pH->EndFunction(Script::SetCachedVector(pItem->GetStats().mount_dir, pH, 1));
}


//------------------------------------------------------------------------
int CScriptBind_Item::GetMountedAngleLimits(IFunctionHandler *pH)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	return pH->EndFunction(Script::SetCachedVector(pItem->GetMountedAngleLimits(), pH, 1));
}

//------------------------------------------------------------------------
int CScriptBind_Item::SetMountedAngleLimits(IFunctionHandler *pH, float min_pitch, float max_pitch, float yaw_range)
{
	CItem *pItem = GetItem(pH);
	if (pItem)
		pItem->SetMountedAngleLimits(min_pitch, max_pitch, yaw_range);
	return pH->EndFunction();
}


//------------------------------------------------------------------------
int CScriptBind_Item::OnHit(IFunctionHandler *pH, SmartScriptTable hitTable)
{
  CItem *pItem = GetItem(pH);
  if (!pItem)
    return pH->EndFunction();

  float damage = 0.f;
  hitTable->GetValue("damage", damage);
  char* damageType = 0;
  hitTable->GetValue("type",damageType);
  
  pItem->OnHit(damage,damageType);

  return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Item::IsDestroyed(IFunctionHandler *pH)
{
  CItem *pItem = GetItem(pH);
  if (!pItem)
    return pH->EndFunction(false);

  return pH->EndFunction(pItem->IsDestroyed());
}

//------------------------------------------------------------------------
int CScriptBind_Item::OnUsed(IFunctionHandler *pH, ScriptHandle userId)
{
	CItem *pItem = GetItem(pH);
	if (!pItem)
		return pH->EndFunction();

	if (pItem->CanUse((EntityId)userId.n))
	{
		CActor *pActor=GetActor((EntityId)userId.n);
		if (pActor)
		{
			pActor->UseItem(pItem->GetEntityId());
			return pH->EndFunction(true);
		}
	}
	else if (pItem->CanPickUp((EntityId)userId.n))
	{
		CActor *pActor=GetActor((EntityId)userId.n);
		if (pActor)
		{
			pActor->PickUpItem(pItem->GetEntityId(), true);
			return pH->EndFunction(true);
		}
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Item::GetHealth(IFunctionHandler *pH)
{
	CItem *pItem = GetItem(pH);
	if (pItem)
		return pH->EndFunction(pItem->GetStats().health);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Item::GetMaxHealth(IFunctionHandler *pH)
{
	CItem *pItem = GetItem(pH);
	if (pItem)
		return pH->EndFunction(pItem->GetProperties().hitpoints);

	return pH->EndFunction();
}

//------------------------------------------------------------------------

#undef GVALUE
#undef SVALUE
#undef REUSE_VECTOR
