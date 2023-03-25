/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: VehicleWeapon Implementation

-------------------------------------------------------------------------
History:
- 20:04:2006   13:01 : Created by Márcio Martins

*************************************************************************/
#ifndef __VEHICLE_WEAPON_H__
#define __VEHICLE_WEAPON_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IItemSystem.h>
#include <CryCharAnimationParams.h>
#include "Weapon.h"

struct IVehicle;
struct IVehiclePart;
struct IVehicleSeat;

class CVehicleWeapon: public CWeapon
{
public:

  CVehicleWeapon();
  
  // CWeapon
  virtual bool Init(IGameObject * pGameObject);
  virtual void PostInit(IGameObject * pGameObject);
  virtual void Reset();

  virtual void MountAtEntity(EntityId entityId, const Vec3 &pos, const Ang3 &angles);

	virtual void StartUse(EntityId userId);
	virtual void StopUse(EntityId userId);
  virtual bool FilterView(SViewParams& viewParams);
	virtual void ApplyViewLimit(EntityId userId, bool apply) {}; // should not be done for vehicle weapons

  virtual void StartFire();
   
  virtual void Update(SEntityUpdateContext& ctx, int update);

  virtual void SetAmmoCount(IEntityClass* pAmmoType, int count);
  virtual void SetInventoryAmmoCount(IEntityClass* pAmmoType, int count);

  virtual bool GetAimBlending(OldBlendSpace& params);
  virtual void UpdateIKMounted(IActor* pActor, const Vec3& vGunXAxis);
  virtual void AttachArms(bool attach, bool shadow);
  virtual bool CanZoom() const;

	virtual void UpdateFPView(float frameTime);

	virtual void GetMemoryStatistics(ICrySizer * s) { s->Add(*this); CWeapon::GetMemoryStatistics(s); }

  virtual bool ApplyActorRecoil() const { return (m_pOwnerSeat == m_pSeatUser); }  
  // ~CWeapon

protected:

  bool CheckWaterLevel() const;
  void CheckForFriendlyAI(float frameTime);
	void CheckForFriendlyPlayers(float frameTime);

  IVehicle* m_pVehicle;
  IVehiclePart* m_pPart;
  IVehicleSeat* m_pOwnerSeat; // owner seat of the weapon
  IVehicleSeat* m_pSeatUser; // seat of the weapons user

private:
	float	  m_timeToUpdate;
  float   m_dtWaterLevelCheck;
	IEntity *m_pLookAtEnemy;
};


#endif//__VEHICLE_WEAPON_H__