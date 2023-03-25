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
#include "ScriptBind_Weapon.h"
#include "Item.h"
#include "Weapon.h"
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
CScriptBind_Weapon::CScriptBind_Weapon(ISystem *pSystem, IGameFramework *pGameFramework)
: m_pSystem(pSystem),
	m_pSS(pSystem->GetIScriptSystem()),
	m_pGameFW(pGameFramework)
{
	Init(m_pSS, m_pSystem, 1);

	RegisterMethods();
	RegisterGlobals();
}

//------------------------------------------------------------------------
CScriptBind_Weapon::~CScriptBind_Weapon()
{
}

//------------------------------------------------------------------------
void CScriptBind_Weapon::AttachTo(CWeapon *pWeapon)
{
	IScriptTable *pScriptTable = ((CItem *)pWeapon)->GetEntity()->GetScriptTable();

	if (pScriptTable)
	{
		SmartScriptTable thisTable(m_pSS);

		thisTable->SetValue("__this", ScriptHandle(pWeapon->GetEntityId()));
		thisTable->Delegate(GetMethodsTable());

		pScriptTable->SetValue("weapon", thisTable);
	}
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::SetAmmoCount(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());

	if (pFireMode)
	{
		if (pH->GetParamType(2) != svtNumber)
			return pH->EndFunction();

		const char *ammoName = 0;
		if (pH->GetParamType(1) == svtString)
			pH->GetParam(1, ammoName);

		IEntityClass* pAmmoType = pFireMode->GetAmmoType();

		if (ammoName)
			pAmmoType = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammoName);

		int ammo = 0;
		pH->GetParam(2, ammo);

		pWeapon->SetAmmoCount(pAmmoType, ammo);
	}

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::GetAmmoCount(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
	
	if (pFireMode)
		return pH->EndFunction(pFireMode->GetAmmoCount());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::GetClipSize(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());

	if (pFireMode)
		return pH->EndFunction(pFireMode->GetClipSize());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::IsZoomed(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IZoomMode *pZoomMode = pWeapon->GetZoomMode(pWeapon->GetCurrentZoomMode());

	if (pZoomMode)
		return pH->EndFunction(pZoomMode->IsZoomed());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::IsZooming(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IZoomMode *pZoomMode = pWeapon->GetZoomMode(pWeapon->GetCurrentZoomMode());

	if (pZoomMode)
		return pH->EndFunction(pZoomMode->IsZooming());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::GetDamage(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());

	if (pFireMode)
		return pH->EndFunction(pFireMode->GetDamage());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::GetAmmoType(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());

	if (pFireMode)
		if (IEntityClass * pCls = pFireMode->GetAmmoType())
			return pH->EndFunction(pCls->GetName());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::GetRecoil(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());

	if (pFireMode)
		return pH->EndFunction(pFireMode->GetRecoil());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::GetSpread(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());

	if (pFireMode)
		return pH->EndFunction(pFireMode->GetSpread());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::GetCrosshair(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IFireMode *pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());

	if (pFireMode)
		return pH->EndFunction(pFireMode->GetCrosshair());

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::GetCrosshairOpacity(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	return pH->EndFunction(pWeapon->GetCrosshairOpacity());
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::GetCrosshairVisibility(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	return pH->EndFunction(pWeapon->GetCrosshairVisibility());
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::SetCurrentFireMode(IFunctionHandler *pH, const char *name)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	pWeapon->SetCurrentFireMode(name);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::SetCurrentZoomMode(IFunctionHandler *pH, const char *name)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	pWeapon->SetCurrentZoomMode(name);

	return pH->EndFunction();
}

int CScriptBind_Weapon::ModifyCommit(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	pWeapon->RestoreLayers();
	pWeapon->ReAttachAccessories();

	return pH->EndFunction();
}

class ScheduleAttachClass
{
public:
	ScheduleAttachClass(CWeapon *wep, const char *cname, bool attch)
	{
		m_pWeapon = wep;
		m_className = cname;
		m_attach = attch;
	}
	void execute(CItem *item) {
		//CryLogAlways("attaching %s", _className);
		m_pWeapon->AttachAccessory(m_className, m_attach, false);
		//delete this;
	}
private:
	CWeapon *m_pWeapon;
	const char *m_className;
	bool m_attach;
};

int CScriptBind_Weapon::ScheduleAttach(IFunctionHandler *pH, const char *className, bool attach)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	pWeapon->GetScheduler()->ScheduleAction(CSchedulerAction<ScheduleAttachClass>::Create(ScheduleAttachClass(pWeapon, className, attach)));

	return pH->EndFunction();
}

int CScriptBind_Weapon::SupportsAccessory(IFunctionHandler *pH, const char *accessoryName)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	CItem::SAccessoryParams *params = pWeapon->GetAccessoryParams(accessoryName);
	return pH->EndFunction(params != 0);
}

int CScriptBind_Weapon::GetAccessory(IFunctionHandler *pH, const char *accessoryName)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	CItem *pItem = pWeapon->GetAccessory(accessoryName);					
	
	if(!pItem)
		return 0;

	IEntity *pEntity  = pItem->GetEntity();

	if(!pEntity)
		return 0;
	
	IScriptTable *pScriptTable = pEntity->GetScriptTable();

	return pH->EndFunction( pScriptTable );
}

int CScriptBind_Weapon::AttachAccessory(IFunctionHandler *pH, const char *className, bool attach, bool force)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	if (className)
		pWeapon->AttachAccessory(className, attach, true, force);

	return pH->EndFunction();
}

int CScriptBind_Weapon::SwitchAccessory(IFunctionHandler *pH, const char *className)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	if (className)
		pWeapon->SwitchAccessory(className);

	return pH->EndFunction();
}

int CScriptBind_Weapon::IsFiring(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	int n=pWeapon->GetNumOfFireModes();
	for (int i=0;i<n;i++)
	{
		if (IFireMode *pFireMode=pWeapon->GetFireMode(i))
		{
			if (pFireMode->IsEnabled() && pFireMode->IsFiring())
				return pH->EndFunction(true);
		}
	}

	return pH->EndFunction();
}

int CScriptBind_Weapon::AttachAccessoryPlaceHolder(IFunctionHandler *pH, SmartScriptTable accessory, bool attach)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	const char *accessoryName;
	accessory->GetValue("class", accessoryName);
	ScriptHandle id;
	accessory->GetValue("id", id);
	
	EntityId entId = id.n;
	//CryLogAlways("id = %d", entId);
	IEntity *attachment = gEnv->pEntitySystem->GetEntity(entId);
	

	if (accessoryName)
	{
		//CryLogAlways("got name: %s", accessoryName);
		if (pWeapon->GetAccessoryPlaceHolder(accessoryName))
		{
			//CryLogAlways("found accessory place holder");
			pWeapon->AttachAccessoryPlaceHolder(accessoryName, attach);
		}
		else
		{
			//CryLogAlways("accessory place holder not found");
			CActor *pActor = pWeapon->GetOwnerActor();
			IEntity *wep = pWeapon->GetEntity();
			//IGameObject *pGameObject = pWeapon->GetOwnerActor()->GetGameObject();
			IInventory *pInventory = pActor->GetInventory();
			if (pInventory)
			{
				//CryLogAlways("found inventory");
				if (attachment)
				{
					if (pInventory->FindItem(entId) != -1)
					{
						//CryLogAlways("found attachment in inventory already...");
					}
					else
					{
						//CryLogAlways("attachment not found in inventory, adding...");
					}
					//CryLogAlways("found attachment");
					
					
					//attachment->DetachThis(0);
					//attachment->SetParentId(0);
					//CItem *t = (CItem *)attachment;
					//t->SetParentId(0);
					//pWeapon->GetEntity()->AttachChild(attachment, false)
					pInventory->AddItem(attachment->GetId());
					//for (int i = 0; i < wep->GetChildCount(); i++)
					//{
					//	IEntity *cur = wep->GetChild(i);
					//	CryLogAlways("none of these should be %s", attachment->GetName());
					//	CryLogAlways(" %s", cur->GetName());
					//}
					pWeapon->AttachAccessoryPlaceHolder(accessoryName, attach);
					pInventory->RemoveItem(attachment->GetId());
					
				}
				else
				{
					//CryLogAlways("!attachment");
				}
			}
		}
		
	}
	return pH->EndFunction();
}

int CScriptBind_Weapon::GetAttachmentHelperPos(IFunctionHandler *pH, const char *helperName)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	Vec3 pos = pWeapon->GetSlotHelperPos(CItem::eIGS_FirstPerson, helperName, false);
	Vec3 tpos = pWeapon->GetSlotHelperPos(CItem::eIGS_FirstPerson, helperName, true);
	//gEnv->pRenderer->DrawPoint(tpos.x, tpos.y, tpos.z, 10.0f);
	//CryLogAlways("helperName: %s pos: x=%f y=%f z=%f", helperName, tpos.x, tpos.y, tpos.z);
	//gEnv->pRenderer->DrawBall(tpos, 0.2f);
	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(tpos, 0.075f, ColorB( 255, 10, 10, 255 ));
	//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(pWeapon->GetOwner()->GetWorldPos(), ColorB(0, 0, 255, 255), tpos, ColorB(255, 255, 0, 255));
	return pH->EndFunction(tpos);
}

int CScriptBind_Weapon::GetShooter(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	IEntity *owner = pWeapon->GetOwner();
	return pH->EndFunction(owner->GetScriptTable());
}

//------------------------------------------------------------------------
int CScriptBind_Weapon::AutoShoot(IFunctionHandler *pH, int shots, bool autoReload)
{
	struct AutoShootHelper : public IWeaponEventListener
	{
		AutoShootHelper(int n, bool autoreload): m_nshots(n), m_reload(autoreload) {};
		virtual ~AutoShootHelper() {};

		int		m_nshots;
		bool	m_reload;

		virtual void OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType,
			const Vec3 &pos, const Vec3 &dir, const Vec3 &vel) {};
		virtual void OnStartFire(IWeapon *pWeapon, EntityId shooterId) {};
		virtual void OnStopFire(IWeapon *pWeapon, EntityId shooterId) {};
		virtual void OnStartReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType) {};
		virtual void OnEndReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType) {};
		virtual void OnOutOfAmmo(IWeapon *pWeapon, IEntityClass* pAmmoType)
		{
			if (m_reload)
				pWeapon->Reload(false);
			else
			{
				pWeapon->StopFire();
				pWeapon->RemoveEventListener(this);
			}
		};
		virtual void OnReadyToFire(IWeapon *pWeapon)
		{
			if (!(--m_nshots))
			{
				pWeapon->StopFire();
				pWeapon->RemoveEventListener(this);
			}
			else
			{
				pWeapon->StartFire();
				pWeapon->StopFire();
			}
		};
		virtual void OnPickedUp(IWeapon *pWeapon, EntityId actorId, bool destroyed){}
		virtual void OnDropped(IWeapon *pWeapon, EntityId actorId){}
		virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId){}
		virtual void OnStartTargetting(IWeapon *pWeapon) {}
		virtual void OnStopTargetting(IWeapon *pWeapon) {}
		virtual void OnSelected(IWeapon *pWeapon, bool select) {}
	};

	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	pWeapon->AddEventListener(new AutoShootHelper(shots, autoReload), __FUNCTION__); 	// FIXME: possible small memory leak here. 
	pWeapon->StartFire();
	pWeapon->StopFire();

	return pH->EndFunction();
}

//
//------------------------------------------------------------------------
int CScriptBind_Weapon::Reload(IFunctionHandler *pH)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (pWeapon)
		pWeapon->Reload();

	return pH->EndFunction();
}


//------------------------------------------------------------------------
void CScriptBind_Weapon::RegisterGlobals()
{
}

//------------------------------------------------------------------------
void CScriptBind_Weapon::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Weapon::

	SCRIPT_REG_TEMPLFUNC(SetAmmoCount, "")
	SCRIPT_REG_TEMPLFUNC(GetAmmoCount, "")
	SCRIPT_REG_TEMPLFUNC(GetClipSize, "")
	SCRIPT_REG_TEMPLFUNC(IsZoomed, "")
	SCRIPT_REG_TEMPLFUNC(IsZooming, "")
	SCRIPT_REG_TEMPLFUNC(GetDamage, "")
	SCRIPT_REG_TEMPLFUNC(GetAmmoType, "")

	SCRIPT_REG_TEMPLFUNC(GetRecoil, "");
	SCRIPT_REG_TEMPLFUNC(GetSpread, "");
	SCRIPT_REG_TEMPLFUNC(GetCrosshair, "");
	SCRIPT_REG_TEMPLFUNC(GetCrosshairOpacity, "");
	SCRIPT_REG_TEMPLFUNC(GetCrosshairVisibility, "");
	SCRIPT_REG_TEMPLFUNC(ModifyCommit, "");
	SCRIPT_REG_TEMPLFUNC(SupportsAccessory, "accessoryName");
	SCRIPT_REG_TEMPLFUNC(GetAccessory, "accessoryName");
	SCRIPT_REG_TEMPLFUNC(AttachAccessoryPlaceHolder, "accessory, attach");
	SCRIPT_REG_TEMPLFUNC(GetAttachmentHelperPos, "helperName");
	SCRIPT_REG_TEMPLFUNC(GetShooter, "");
	SCRIPT_REG_TEMPLFUNC(ScheduleAttach, "accessoryName, attach");
	SCRIPT_REG_TEMPLFUNC(AttachAccessory, "accessoryName, attach, force");
	SCRIPT_REG_TEMPLFUNC(SwitchAccessory, "accessoryName");

	SCRIPT_REG_TEMPLFUNC(IsFiring, "");

	SCRIPT_REG_TEMPLFUNC(SetCurrentFireMode, "name")
	SCRIPT_REG_TEMPLFUNC(SetCurrentZoomMode, "name")

	SCRIPT_REG_TEMPLFUNC(AutoShoot, "nshots, autoReload");

	SCRIPT_REG_TEMPLFUNC(Reload, "")

	SCRIPT_REG_TEMPLFUNC(ActivateLamLaser, "activate");
	SCRIPT_REG_TEMPLFUNC(ActivateLamLight, "activate");
}

//------------------------------------------------------------------------
CItem *CScriptBind_Weapon::GetItem(IFunctionHandler *pH)
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
CWeapon *CScriptBind_Weapon::GetWeapon(IFunctionHandler *pH)
{
	void *pThis = pH->GetThis();

	if (pThis)
	{
		IItem *pItem = m_pGameFW->GetIItemSystem()->GetItem((EntityId)(UINT_PTR)pThis);
		if (pItem)
		{
			IWeapon *pWeapon=pItem->GetIWeapon();
			if (pWeapon)
				return static_cast<CWeapon *>(pWeapon);
		}
	}

	return 0;
}

//-----------------------------------------------------------------------
int CScriptBind_Weapon::ActivateLamLaser(IFunctionHandler *pH, bool activate)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if (!pWeapon)
		return pH->EndFunction();

	pWeapon->ActivateLamLaser(activate);

	return pH->EndFunction();
}

//-------------------------------------------------------------------------
int CScriptBind_Weapon::ActivateLamLight(IFunctionHandler *pH, bool activate)
{
	CWeapon *pWeapon = GetWeapon(pH);
	if(!pWeapon)
		return pH->EndFunction();

	pWeapon->ActivateLamLight(activate);

	return pH->EndFunction();

}

#undef REUSE_VECTOR