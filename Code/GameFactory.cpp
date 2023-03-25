/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2005.
  -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description:  Register the factory templates used to create classes from names
                e.g. REGISTER_FACTORY(pFramework, "Player", CPlayer, false);
                or   REGISTER_FACTORY(pFramework, "Player", CPlayerG4, false);

                Since overriding this function creates template based linker errors,
                it's been replaced by a standalone function in its own cpp file.

  -------------------------------------------------------------------------
  History:
  - 17:8:2005   Created by Nick Hesketh - Refactor'd from Game.cpp/h

*************************************************************************/

#include "StdAfx.h"
#include "Game.h"
#include "Player.h"
//aliens
#include "Alien.h"
#include "Scout.h"
#include "Hunter.h"
#include "Trooper.h"
//#include "Observer.h"
#include "Shark.h"
//
#include "Item.h"
#include "Weapon.h"
#include "VehicleWeapon.h"
#include "AmmoPickup.h"
#include "Binocular.h"
#include "C4.h"
#include "C4Detonator.h"
#include "DebugGun.h"
#include "PlayerFeature.h"
#include "ReferenceWeapon.h"
#include "OffHand.h"
#include "Fists.h"
#include "Lam.h"
#include "GunTurret.h"
#include "ThrowableWeapon.h"
#include "RocketLauncher.h"

#include "VehicleMovementBase.h"
#include "VehicleActionAutomaticDoor.h"
#include "VehicleActionDeployRope.h"
#include "VehicleActionEntityAttachment.h"
#include "VehicleActionLandingGears.h"
#include "VehicleDamageBehaviorBurn.h"
#include "VehicleDamageBehaviorCameraShake.h"
#include "VehicleDamageBehaviorCollisionEx.h"
#include "VehicleDamageBehaviorExplosion.h"
#include "VehicleDamageBehaviorTire.h"
#include "VehicleMovementStdWheeled.h"
#include "VehicleMovementHovercraft.h"
#include "VehicleMovementHelicopter.h"
#include "VehicleMovementStdBoat.h"
#include "VehicleMovementTank.h"
#include "VehicleMovementVTOL.h"
#include "VehicleMovementWarrior.h"
#include "VehicleMovementAmphibious.h"

#include "ScriptControlledPhysics.h"

#include "HUD/HUD.h"

#include "GameRules.h"

#include "Environment/Tornado.h"
#include "Environment/Shake.h"

#include "Environment/BattleDust.h"

#include <IItemSystem.h>
#include <IVehicleSystem.h>
#include <IGameRulesSystem.h>



#define HIDE_FROM_EDITOR(className)																																				\
  { IEntityClass *pItemClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);\
  pItemClass->SetFlags(pItemClass->GetFlags() | ECLF_INVISIBLE); }																				\

#define REGISTER_GAME_OBJECT(framework, name, script)\
	{\
		IEntityClassRegistry::SEntityClassDesc clsDesc;\
		clsDesc.sName = #name;\
		clsDesc.sScriptFile = script;\
		struct C##name##Creator : public IGameObjectExtensionCreatorBase\
		{\
			C##name *Create()\
			{\
				return new C##name();\
			}\
			void GetGameObjectExtensionRMIData( void ** ppRMI, size_t * nCount )\
			{\
			C##name::GetGameObjectExtensionRMIData( ppRMI, nCount );\
			}\
		};\
		static C##name##Creator _creator;\
		framework->GetIGameObjectSystem()->RegisterExtension(#name, &_creator, &clsDesc);\
	}

#define REGISTER_GAME_OBJECT_EXTENSION(framework, name)\
	{\
		struct C##name##Creator : public IGameObjectExtensionCreatorBase\
		{\
		C##name *Create()\
			{\
			return new C##name();\
			}\
			void GetGameObjectExtensionRMIData( void ** ppRMI, size_t * nCount )\
			{\
			C##name::GetGameObjectExtensionRMIData( ppRMI, nCount );\
			}\
		};\
		static C##name##Creator _creator;\
		framework->GetIGameObjectSystem()->RegisterExtension(#name, &_creator, NULL);\
	}

// Register the factory templates used to create classes from names. Called via CGame::Init()
void InitGameFactory(IGameFramework *pFramework)
{
  assert(pFramework);

  REGISTER_FACTORY(pFramework, "Player", CPlayer, false);
  REGISTER_FACTORY(pFramework, "Grunt", CPlayer, true);
  REGISTER_FACTORY(pFramework, "Civilian", CPlayer, true);

  // Items
  REGISTER_FACTORY(pFramework, "Item", CItem, false);
  REGISTER_FACTORY(pFramework, "PlayerFeature", CPlayerFeature, false);
	REGISTER_FACTORY(pFramework, "LAM", CLam, false);

  // Weapons
  REGISTER_FACTORY(pFramework, "Weapon", CWeapon, false);
	REGISTER_FACTORY(pFramework, "VehicleWeapon", CVehicleWeapon, false);
	REGISTER_FACTORY(pFramework, "AmmoPickup", CAmmoPickup, false);
	REGISTER_FACTORY(pFramework, "AVMine", CThrowableWeapon, false);
	REGISTER_FACTORY(pFramework, "Claymore", CThrowableWeapon, false);
	REGISTER_FACTORY(pFramework, "Binocular", CBinocular, false);
  REGISTER_FACTORY(pFramework, "C4", CC4, false);
	REGISTER_FACTORY(pFramework, "C4Detonator", CC4Detonator, false);
	REGISTER_FACTORY(pFramework, "DebugGun", CDebugGun, false);
	REGISTER_FACTORY(pFramework, "ReferenceWeapon", CReferenceWeapon, false);
	REGISTER_FACTORY(pFramework, "OffHand", COffHand, false);
	REGISTER_FACTORY(pFramework, "Fists", CFists, false);
  REGISTER_FACTORY(pFramework, "GunTurret", CGunTurret, false);
	REGISTER_FACTORY(pFramework, "RocketLauncher", CRocketLauncher, false);
		
	// vehicle objects
  IVehicleSystem* pVehicleSystem = pFramework->GetIVehicleSystem();

#define REGISTER_VEHICLEOBJECT(name, obj) \
	REGISTER_FACTORY((IVehicleSystem*)pVehicleSystem, name, obj, false); \
	obj::m_objectId = pVehicleSystem->AssignVehicleObjectId();

	REGISTER_VEHICLEOBJECT("Burn", CVehicleDamageBehaviorBurn);
	REGISTER_VEHICLEOBJECT("CameraShake", CVehicleDamageBehaviorCameraShake);
	REGISTER_VEHICLEOBJECT("CollisionEx", CVehicleDamageBehaviorCollisionEx);
	REGISTER_VEHICLEOBJECT("Explosion", CVehicleDamageBehaviorExplosion);
  REGISTER_VEHICLEOBJECT("BlowTire", CVehicleDamageBehaviorBlowTire);
	REGISTER_VEHICLEOBJECT("AutomaticDoor", CVehicleActionAutomaticDoor);
	REGISTER_VEHICLEOBJECT("DeployRope", CVehicleActionDeployRope);
	REGISTER_VEHICLEOBJECT("EntityAttachment", CVehicleActionEntityAttachment);
	REGISTER_VEHICLEOBJECT("LandingGears", CVehicleActionLandingGears);

  // vehicle movements
  REGISTER_FACTORY(pVehicleSystem, "Hovercraft", CVehicleMovementHovercraft, false);
  REGISTER_FACTORY(pVehicleSystem, "Helicopter", CVehicleMovementHelicopter, false);
  REGISTER_FACTORY(pVehicleSystem, "StdBoat", CVehicleMovementStdBoat, false);
	REGISTER_FACTORY(pVehicleSystem, "StdWheeled", CVehicleMovementStdWheeled, false);
  REGISTER_FACTORY(pVehicleSystem, "Tank", CVehicleMovementTank, false);
	REGISTER_FACTORY(pVehicleSystem, "VTOL", CVehicleMovementVTOL, false);
  REGISTER_FACTORY(pVehicleSystem, "Warrior", CVehicleMovementWarrior, false);
  REGISTER_FACTORY(pVehicleSystem, "Amphibious", CVehicleMovementAmphibious, false);

#ifndef SP_DEMO
  //aliens
  REGISTER_FACTORY(pFramework, "AlienPlayer", CAlien, false);
  REGISTER_FACTORY(pFramework, "Aliens/Alien", CAlien, true);
//  REGISTER_FACTORY(pFramework, "Aliens/Observer", CObserver, true);
  REGISTER_FACTORY(pFramework, "Aliens/Trooper", CTrooper, true);
  REGISTER_FACTORY(pFramework, "Aliens/Scout", CScout, true);
  REGISTER_FACTORY(pFramework, "Aliens/Hunter", CHunter, true);
	REGISTER_FACTORY(pFramework, "Misc/Shark", CShark, true);
  //	REGISTER_FACTORY(m_pFramework, "Aliens/Warrior", CDrone, true);
  //REGISTER_FACTORY(pFramework, "Aliens/Coordinator", CObserver, true);
#else
	REGISTER_FACTORY(pFramework, "Aliens/Scout", CScout, true);
	REGISTER_FACTORY(pFramework, "Aliens/Trooper", CTrooper, true);
#endif

	// Custom GameObjects
	REGISTER_GAME_OBJECT(pFramework, Tornado, "Scripts/Entities/Environment/Tornado.lua");
	REGISTER_GAME_OBJECT(pFramework, Shake, "Scripts/Entities/Environment/Shake.lua");

	// Custom Extensions
	//REGISTER_GAME_OBJECT(pFramework, CustomFreezing);
	//REGISTER_GAME_OBJECT(pFramework, CustomShatter);

	REGISTER_GAME_OBJECT(pFramework, BattleEvent, "");
	HIDE_FROM_EDITOR("BattleEvent");

	//GameRules
	REGISTER_FACTORY(pFramework, "GameRules", CGameRules, false);


	REGISTER_GAME_OBJECT_EXTENSION(pFramework, ScriptControlledPhysics);

#ifndef CRYSIS_BETA
	pFramework->GetIGameRulesSystem()->RegisterGameRules("SinglePlayer", "GameRules");
	pFramework->GetIGameRulesSystem()->AddGameRulesAlias("SinglePlayer", "sp");

#ifndef SP_DEMO
	pFramework->GetIGameRulesSystem()->RegisterGameRules("InstantAction", "GameRules");
	pFramework->GetIGameRulesSystem()->AddGameRulesAlias("InstantAction", "ia");
	pFramework->GetIGameRulesSystem()->AddGameRulesAlias("InstantAction", "dm");
	pFramework->GetIGameRulesSystem()->AddGameRulesLevelLocation("InstantAction", "multiplayer/ia/");

	pFramework->GetIGameRulesSystem()->RegisterGameRules("TeamInstantAction", "GameRules");
	pFramework->GetIGameRulesSystem()->AddGameRulesAlias("TeamInstantAction", "tia");
	pFramework->GetIGameRulesSystem()->AddGameRulesAlias("TeamInstantAction", "tdm");
	pFramework->GetIGameRulesSystem()->AddGameRulesLevelLocation("TeamInstantAction", "multiplayer/tia/");
	pFramework->GetIGameRulesSystem()->AddGameRulesLevelLocation("TeamInstantAction", "multiplayer/ia/");
#endif //spdemo

	//pFramework->GetIGameRulesSystem()->RegisterGameRules("TeamAction", "GameRules");
#endif //crysis_beta

#ifndef SP_DEMO
	pFramework->GetIGameRulesSystem()->RegisterGameRules("PowerStruggle", "GameRules");
	pFramework->GetIGameRulesSystem()->AddGameRulesAlias("PowerStruggle", "ps");
	pFramework->GetIGameRulesSystem()->AddGameRulesLevelLocation("PowerStruggle", "multiplayer/ps/");
#endif //spdemo
}
