// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "Game.h"
#include "Item.h"
#include "GameRules.h"
#include "Nodes/G2FlowBaseNode.h"

class CFlowHitInfoNode : public CFlowBaseNode, public IHitListener
{
public:
	CFlowHitInfoNode( SActivationInfo * pActInfo )
	{
	}

	~CFlowHitInfoNode()
	{
		// safety unregister
		CGameRules* pGR = g_pGame->GetGameRules();
		if (pGR)
			pGR->RemoveHitListener(this);
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowHitInfoNode(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
		{
			DoRegister(pActInfo);
		}
	}

	enum EInputPorts
	{
		EIP_Enable = 0,
		EIP_ShooterId,
		EIP_TargetId,
		EIP_Weapon,
		EIP_Ammo
	};

	enum EOutputPorts
	{
		EOP_ShooterId = 0,
		EOP_TargetId,
		EOP_WeaponId,
		EOP_ProjectileId,
		EOP_HitPos,
		EOP_HitDir,
		EOP_HitNormal,
		EOP_HitType,
		EOP_Damage,
		EOP_Material,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<bool>("Enable", false, _HELP("Enable/Disable HitInfo")),
			InputPortConfig<EntityId>("ShooterId", _HELP("When connected, limit HitInfo to this shooter")),
			InputPortConfig<EntityId>("TargetId",  _HELP("When connected, limit HitInfo to this target")),
			InputPortConfig<string> ("Weapon", _HELP("When set, limit HitInfo to this weapon"), 0, _UICONFIG("enum_global:weapon")),
			InputPortConfig<string> ("Ammo", _HELP("When set, limit HitInfo to this ammo"), 0, _UICONFIG("enum_global:ammos")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<EntityId>("ShooterId", _HELP("EntityID of the Shooter")),
			OutputPortConfig<EntityId>("TargetId",  _HELP("EntityID of the Target")),
			OutputPortConfig<EntityId>("WeaponId",  _HELP("EntityID of the Weapon")),
			OutputPortConfig<EntityId>("ProjectileId",  _HELP("EntityID of the Projectile if it was a bullet hit")),
			OutputPortConfig<Vec3>    ("HitPos", _HELP("Position of the Hit")),
			OutputPortConfig<Vec3>    ("HitDir", _HELP("Direction of the Hit")),
			OutputPortConfig<Vec3>    ("HitNormal", _HELP("Normal of the Hit Impact")),
			OutputPortConfig<string>  ("HitType",  _HELP("Name of the HitType")),
			OutputPortConfig<float>   ("Damage",    _HELP("Damage amout which was caused")),
			OutputPortConfig<string>  ("Material",  _HELP("Name of the Material")),
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Tracks Hits on Actors.\nAll input conditions (ShooterId, TargetId, Weapon, Ammo) must be fulfilled to output.\nIf a condition is left empty/not connected, it is regarded as fulfilled.");
		config.SetCategory(EFLN_WIP);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;  // fall through and enable/disable listener
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Enable))
			{
				DoRegister(pActInfo);
			}
			break;
		}
	}

	void DoRegister(SActivationInfo* pActInfo)
	{
		CGameRules* pGR = g_pGame->GetGameRules();
		if (!pGR)
		{
			GameWarning("[flow] CFlowHitInfoNode::DoRegister: No GameRules!");
			return;
		}

		bool bEnable = GetPortBool(pActInfo, EIP_Enable);
		if (bEnable)
			pGR->AddHitListener(this);
		else
			pGR->RemoveHitListener(this);
	}

	//CGameRules::IHitInfo
	virtual void OnHit(const HitInfo& hitInfo)
	{
		if (GetPortBool(&m_actInfo, EIP_Enable) == false)
			return;

		EntityId shooter = GetPortEntityId(&m_actInfo, EIP_ShooterId);
		if (shooter != 0 && shooter != hitInfo.shooterId)
			return;

		EntityId target = GetPortEntityId(&m_actInfo, EIP_TargetId);
		if (target != 0 && target != hitInfo.targetId)
			return;

		IEntitySystem* pEntitySys = gEnv->pEntitySystem;
		IEntity* pTempEntity;

		// check weapon match
		const string& weapon = GetPortString(&m_actInfo, EIP_Weapon);
		if (weapon.empty() == false)
		{
			pTempEntity = pEntitySys->GetEntity(hitInfo.weaponId);
			if (pTempEntity == 0 || weapon.compare(pTempEntity->GetClass()->GetName()) != 0)
				return;
		}
		// check ammo match
		const string& ammo = GetPortString(&m_actInfo, EIP_Ammo);
		if (ammo.empty() == false)
		{
			pTempEntity = pEntitySys->GetEntity(hitInfo.projectileId);
			if (pTempEntity == 0 || ammo.compare(pTempEntity->GetClass()->GetName()) != 0)
				return;
		}
		
		ActivateOutput(&m_actInfo, EOP_ShooterId, hitInfo.shooterId);
		ActivateOutput(&m_actInfo, EOP_TargetId, hitInfo.targetId);
		ActivateOutput(&m_actInfo, EOP_WeaponId, hitInfo.weaponId);
		ActivateOutput(&m_actInfo, EOP_ProjectileId, hitInfo.projectileId);
		ActivateOutput(&m_actInfo, EOP_HitPos, hitInfo.pos);
		ActivateOutput(&m_actInfo, EOP_HitDir, hitInfo.dir);
		ActivateOutput(&m_actInfo, EOP_HitNormal, hitInfo.normal);
		ActivateOutput(&m_actInfo, EOP_Damage, hitInfo.damage);
		ISurfaceType* pSurface = g_pGame->GetGameRules()->GetHitMaterial(hitInfo.material);
		ActivateOutput(&m_actInfo, EOP_Material, string(pSurface ? pSurface->GetName() : ""));
		const char* hitType = "";
		if (CGameRules* pGR = g_pGame->GetGameRules())
			hitType = pGR->GetHitType(hitInfo.type);
		ActivateOutput(&m_actInfo, EOP_HitType, string(hitType));
	}

	virtual void OnExplosion(const ExplosionInfo& explosionInfo)
	{
	}

	virtual void OnServerExplosion(const ExplosionInfo& explosionInfo)
	{
	}
	//~CGameRules::IHitInfo

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	SActivationInfo m_actInfo;
};

class CFlowExplosionInfoNode : public CFlowBaseNode, public IHitListener
{
public:
	CFlowExplosionInfoNode( SActivationInfo * pActInfo )
	{
	}

	~CFlowExplosionInfoNode()
	{
		// safety unregister
		CGameRules* pGR = g_pGame->GetGameRules();
		if (pGR)
			pGR->RemoveHitListener(this);
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowExplosionInfoNode(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		if (ser.IsReading())
			DoRegister(pActInfo);
	}

	enum EInputPorts
	{
		EIP_Enable = 0,
		EIP_ShooterId,
		EIP_WeaponLegacyDontUse,
		EIP_Ammo,
		EIP_ImpactTargetId,
	};

	enum EOutputPorts
	{
		EOP_ShooterId = 0,
		EOP_Ammo,
		EOP_Pos,
		EOP_Dir,
		EOP_Radius,
		EOP_Damage,
		EOP_Pressure,
		EOP_HoleSize,
		EOP_Type,
		EOP_ImpactTargetId,
		EOP_ImpactNormal,
		EOP_ImpactVelocity,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig<bool>("Enable", false, _HELP("Enable/Disable ExplosionInfo")),
			InputPortConfig<EntityId>("ShooterId", _HELP("When connected, limit ExplosionInfo to this shooter")),
			InputPortConfig<string> ("Weapon", _HELP("!DONT USE! -> Use Ammo!"), _HELP("!DONT USE! -> Use Ammo!"), _UICONFIG("enum_global:ammos")),
			InputPortConfig<string> ("Ammo", _HELP("When set, limit ExplosionInfo to this ammo"), 0, _UICONFIG("enum_global:ammos")),
			InputPortConfig<EntityId>("ImpactTargetId",  _HELP("When connected, limit ExplosionInfo to this Impact target (e.g. for Rockets)")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<EntityId>("ShooterId", _HELP("EntityID of the Shooter")),
			OutputPortConfig<string>  ("Ammo",  _HELP("Ammo Class")),
			OutputPortConfig<Vec3>    ("Pos", _HELP("Position of the Explosion")),
			OutputPortConfig<Vec3>    ("Dir", _HELP("Direction of the Explosion")),
			OutputPortConfig<float>   ("Radius",   _HELP("Radius of Explosion")),
			OutputPortConfig<float>   ("Damage",    _HELP("Damage amout which was caused")),
			OutputPortConfig<float>   ("Pressure",    _HELP("Pressure amout which was caused")),
			OutputPortConfig<float>   ("HoleSize",    _HELP("Hole size which was caused")),
			OutputPortConfig<string>  ("Type",  _HELP("Name of the Explosion type")),
			OutputPortConfig<EntityId>("ImpactTargetId",  _HELP("EntityID of the Impact Target (e.g. for Rockets)")),
			OutputPortConfig<Vec3>    ("ImpactNormal",  _HELP("Impact Normal (e.g. for Rockets)")),
			OutputPortConfig<float>   ("ImpactVelocity",  _HELP("Impact Normal (e.g. for Rockets)")),
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Tracks Explosions.\nAll input conditions (ShooterId, Ammo, ImpactTargetId) must be fulfilled to output.\nIf a condition is left empty/not connected, it is regarded as fulfilled.");
		config.SetCategory(EFLN_WIP);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			m_actInfo = *pActInfo;  // fall through and enable/disable listener
		case eFE_Activate:
			if (IsPortActive(pActInfo, EIP_Enable))
			{
				DoRegister(pActInfo);
			}
			break;
		}
	}

	void DoRegister(SActivationInfo* pActInfo)
	{
		CGameRules* pGR = g_pGame->GetGameRules();
		if (!pGR)
		{
			GameWarning("[flow] CFlowExplosionInfoNode::DoRegister No GameRules!");
			return;
		}

		bool bEnable = GetPortBool(pActInfo, EIP_Enable);
		if (bEnable)
			pGR->AddHitListener(this);
		else
			pGR->RemoveHitListener(this);
	}

	//CGameRules::IHitInfo
	virtual void OnHit(const HitInfo& hitInfo)
	{
	}

	virtual void OnExplosion(const ExplosionInfo& explosionInfo)
	{
		if (GetPortBool(&m_actInfo, EIP_Enable) == false)
			return;

		EntityId shooter = GetPortEntityId(&m_actInfo, EIP_ShooterId);
		if (shooter != 0 && shooter != explosionInfo.shooterId)
			return;

		EntityId impactTarget = GetPortEntityId(&m_actInfo, EIP_ImpactTargetId);
		if (impactTarget != 0 && explosionInfo.impact && impactTarget != explosionInfo.impact_targetId)
			return;

		IEntitySystem* pEntitySys = gEnv->pEntitySystem;
		IEntity* pTempEntity = pEntitySys->GetEntity(explosionInfo.weaponId);

		// check ammo match
		const string& ammo = GetPortString(&m_actInfo, EIP_Ammo);
		if (ammo.empty() == false)
		{
			if (pTempEntity == 0 || ammo.compare(pTempEntity->GetClass()->GetName()) != 0)
				return;
		}
		string ammoClass = pTempEntity ? pTempEntity->GetClass()->GetName() : "";
		ActivateOutput(&m_actInfo, EOP_ShooterId, explosionInfo.shooterId);
		ActivateOutput(&m_actInfo, EOP_Ammo, ammoClass);
		ActivateOutput(&m_actInfo, EOP_Pos, explosionInfo.pos);
		ActivateOutput(&m_actInfo, EOP_Dir, explosionInfo.dir);
		ActivateOutput(&m_actInfo, EOP_Radius, explosionInfo.radius);
		ActivateOutput(&m_actInfo, EOP_Damage, explosionInfo.damage);
		ActivateOutput(&m_actInfo, EOP_Pressure, explosionInfo.pressure);
		ActivateOutput(&m_actInfo, EOP_HoleSize, explosionInfo.hole_size);
		const char* hitType = 0;
		if (CGameRules* pGR = g_pGame->GetGameRules())
			hitType = pGR->GetHitType(explosionInfo.type);
		hitType = hitType ? hitType : "";
		ActivateOutput(&m_actInfo, EOP_Type, string(hitType));
		if (explosionInfo.impact)
		{
			ActivateOutput(&m_actInfo, EOP_ImpactTargetId, explosionInfo.impact_targetId);
			ActivateOutput(&m_actInfo, EOP_ImpactNormal, explosionInfo.impact_normal);
			ActivateOutput(&m_actInfo, EOP_ImpactVelocity, explosionInfo.impact_velocity);
		}
	}

	virtual void OnServerExplosion(const ExplosionInfo& explosion)
	{

	}
	//~CGameRules::IHitInfo

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	SActivationInfo m_actInfo;
};


class CFlowFreezeEntityNode : public CFlowBaseNode
{
public:
	CFlowFreezeEntityNode( SActivationInfo * pActInfo )
	{
	}

	enum EInputPorts
	{
		EIP_Freeze = 0,
		EIP_UnFreeze,
		EIP_Vapor
	};

	enum EOutputPorts
	{
		EOP_Frozen = 0,
		EOP_UnFrozen
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void  ("Freeze", _HELP("Trigger to freeze entity")),
			InputPortConfig_Void  ("UnFreeze", _HELP("Trigger to un-freeze entity")),
			InputPortConfig<bool> ("UseVapor", true, _HELP("Trigger to un-freeze entity")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("Frozen",   _HELP("Triggered on Freeze")),
			OutputPortConfig_Void("UnFrozen", _HELP("Triggered on UnFreeze")),
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Freezes/Unfreezes attached entity/actor.");
		config.SetCategory(EFLN_WIP);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			CGameRules* pGR = g_pGame->GetGameRules();
			if (!pGR)
				return;
			if (!pActInfo->pEntity)
				return;
			const bool bUseVapor = GetPortBool(pActInfo, EIP_Vapor);
			if (IsPortActive(pActInfo, EIP_Freeze))
			{
				pGR->FreezeEntity(pActInfo->pEntity->GetId(), true, bUseVapor, true);
				ActivateOutput(pActInfo, EOP_Frozen, true);
			}
			else if (IsPortActive(pActInfo, EIP_UnFreeze))
			{
				pGR->FreezeEntity(pActInfo->pEntity->GetId(), false, bUseVapor);
				ActivateOutput(pActInfo, EOP_UnFrozen, true);
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


REGISTER_FLOW_NODE("Crysis:HitInfo", CFlowHitInfoNode);
REGISTER_FLOW_NODE("Crysis:ExplosionInfo", CFlowExplosionInfoNode);
REGISTER_FLOW_NODE_SINGLETON("Crysis:FreezeEntity", CFlowFreezeEntityNode);

