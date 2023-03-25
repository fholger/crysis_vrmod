/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Fakes a tire blow

-------------------------------------------------------------------------
History:
- 03:28:2006: Created by Michael Rauh

*************************************************************************/
#include "StdAfx.h"
#include "IVehicleSystem.h"
#include "VehicleDamageBehaviorTire.h"
#include "Game.h"
#include "GameRules.h"


#define TIRE_BLOW_EFFECT "vehicle_fx.chinese_truck.blown_tire1"
#define AI_IMMOBILIZED_TIME 4


//------------------------------------------------------------------------
bool CVehicleDamageBehaviorBlowTire::Init(IVehicle* pVehicle, const SmartScriptTable &table)
{
	m_pVehicle = pVehicle;  
	m_isActive = false;  
  m_aiImmobilizedTimer = -1;
  
	SmartScriptTable BlowTireParams;
	if (table->GetValue("BlowTire", BlowTireParams))
	{				
	}

  gEnv->p3DEngine->FindParticleEffect(TIRE_BLOW_EFFECT, "CVehicleDamageBehaviorBlowTire::Init");

	return true;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBlowTire::Reset()
{
  Activate(false);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleDamageBehaviorBlowTire::DamagePlayers()
{
	// check for the nasty case when the player is shooting at the vehicle tires while prone 
	// under or behind the car, In that case the player should get killed by the vehicle, 
	// otherwise he gets stuck forever. Note that he should only get killed when the tier
	// is actually destroyed and not by collision resulting by the impulse added by just 
	// shooting at the tiers. Unfortunately using physics for doing this check is not reliable
	// enough so we have to check it explicitly
	
	IEntityPhysicalProxy *pPhysicsProxy = (IEntityPhysicalProxy*)m_pVehicle->GetEntity()->GetProxy(ENTITY_PROXY_PHYSICS);
	if (!pPhysicsProxy)
		return;

	AABB bbox;
	pPhysicsProxy->GetWorldBounds( bbox );

	IPhysicalWorld *pWorld = gEnv->pSystem->GetIPhysicalWorld();
	IPhysicalEntity **ppColliders;
	int valid=0;
	// check entities in collision with the car
	int cnt = pWorld->GetEntitiesInBox( bbox.min,bbox.max, ppColliders,ent_living);
	for (int i = 0; i < cnt; i++)
	{

		IEntity *pEntity = gEnv->pEntitySystem->GetEntityFromPhysics( ppColliders[i] );
		if (!pEntity)
			continue;
		
		// skip the vehicle itself
		if (pEntity==m_pVehicle->GetEntity())
			continue;

		IPhysicalEntity *pPhysEnt = pEntity->GetPhysics();
		if (!pPhysEnt) 
			continue;

		IActor* pActor = gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId());            
		if(!pActor)
			continue;

		//Jan M.: the player is killed when he entered the vehicle while prone although he is still passenger!
		if(m_pVehicle == pActor->GetLinkedVehicle())
			continue;

		//the player must be prone under the vehicle
		IAnimatedCharacter * animatedCharacter=pActor->GetAnimatedCharacter();
		if (!animatedCharacter)
			continue;

		int stance = animatedCharacter->GetCurrentStance();
		if (stance!=STANCE_PRONE)
			continue;

		pe_player_dimensions dim;
		if (!pPhysEnt->GetParams(&dim))
			continue;
		// position returned is at entity's feet, add head position from physics
		Vec3 vPos1=pEntity->GetPos();
		vPos1.z = vPos1.z + dim.heightHead;

		float fZ=bbox.GetCenter().z;
		if (vPos1.z>fZ)
			continue; // not under the vehicle

		// at this point we have a collision with the car moving down and the guy prone under the car, it is safe
		// to assume he has been squished so let's kill him.
		if (gEnv->bServer && pActor->GetHealth()>0)
		{
			// adding a server hit to make it working in MP
			IGameRules *pGameRules = gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules();
			if (pGameRules)
			{
				HitInfo hit;

				EntityId shooterId=m_pVehicle->GetEntityId();
				if (m_pVehicle->GetDriver())
					shooterId=m_pVehicle->GetDriver()->GetEntityId();					

				hit.targetId = pEntity->GetId();      
				hit.shooterId = shooterId;
				hit.weaponId = m_pVehicle->GetEntityId();
				hit.damage = 1000.f;
				hit.type = 0;
				hit.pos = pActor->GetEntity()->GetWorldPos();

				pGameRules->ServerHit(hit); 
			}  
		} 
	} //i
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBlowTire::Activate(bool activate)
{
  if (activate == m_isActive)
    return;

  if (activate && m_pVehicle->IsDestroyed())
    return;

	if (activate)
	{
		// NOTE: stance and physics position when getting into vehicles is set wrong
		if (!gEnv->pSystem->IsSerializingFile())
			DamagePlayers();
	}

  IVehicleComponent* pComponent = m_pVehicle->GetComponent(m_component.c_str());
  if (!pComponent)
    return;

  IVehiclePart* pPart = pComponent->GetPart(0);
  if (!pPart)
    return;

  // if IVehicleWheel available, execute full damage behavior. if null, only apply effects
  IVehicleWheel* pWheel = pPart->GetIWheel();
  
  if (activate)
  {
    IEntity* pEntity = m_pVehicle->GetEntity();
    IPhysicalEntity* pPhysics = pEntity->GetPhysics();
    const Matrix34& wheelTM = pPart->GetLocalTM(false);
    const SVehicleStatus& status = m_pVehicle->GetStatus();

    if (pWheel)
    { 
      const pe_cargeomparams* pParams = pWheel->GetCarGeomParams();  
            
      // handle destroyed wheel
      pe_params_wheel wheelParams;
      wheelParams.iWheel = pWheel->GetWheelIndex();            
      wheelParams.minFriction = wheelParams.maxFriction = 0.5f * pParams->maxFriction;      
      pPhysics->SetParams(&wheelParams); 
      
      if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
      { 
        SVehicleMovementEventParams params;
        params.pComponent = pComponent;
        params.iValue = pWheel->GetWheelIndex();
        pMovement->OnEvent(IVehicleMovement::eVME_TireBlown, params);
      }

      if (status.speed > 0.1f)
      {
        // add angular impulse
        pe_action_impulse angImp;
        float amount = m_pVehicle->GetMass() * status.speed * Random(0.25f, 0.45f) * -sgn(wheelTM.GetTranslation().x);
        angImp.angImpulse = pEntity->GetWorldTM().TransformVector(Vec3(0,0,amount));    
        pPhysics->Action(&angImp);
      }
      
      m_aiImmobilizedTimer = m_pVehicle->SetTimer(-1, AI_IMMOBILIZED_TIME*1000, this);  
    }

    if (!gEnv->pSystem->IsSerializingFile())
    {
      // add linear impulse       
      pe_action_impulse imp;
      imp.point = pPart->GetWorldTM().GetTranslation();

      float amount = m_pVehicle->GetMass() * Random(0.1f, 0.15f);

      if (pWheel)
      {
        amount *= max(0.5f, min(10.f, status.speed));

        if (status.speed < 0.1f)
          amount = -0.5f*amount;
      }
      else    
        amount *= 0.5f;

      imp.impulse = pEntity->GetWorldTM().TransformVector(Vec3(0,0,amount));
      pPhysics->Action(&imp);     

      // effect
      IParticleEffect* pEffect = gEnv->p3DEngine->FindParticleEffect(TIRE_BLOW_EFFECT);
      if (pEffect)
      {
        int slot = pEntity->LoadParticleEmitter(-1, pEffect);
        if (slot > -1)
        { 
          float rotation = pWheel ? 0.5f * gf_PI * -sgn(wheelTM.GetTranslation().x) : gf_PI;
          Matrix34 tm = Matrix34::CreateRotationZ(rotation);        
          tm.SetTranslation(wheelTM.GetTranslation());        
          pEntity->SetSlotLocalTM(slot, tm);
        }
      }

			// remove affected decals
			{
				Vec3 pos = pPart->GetWorldTM().GetTranslation();
				AABB aabb = pPart->GetLocalBounds();
				float radius = aabb.GetRadius();
				Vec3 vRadius(radius,radius,radius);
        AABB areaBox(pos-vRadius, pos+vRadius);
        
        IRenderNode * pRenderNode = NULL;				
        if (IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER))
					pRenderNode = pRenderProxy->GetRenderNode();

        gEnv->p3DEngine->DeleteDecalsInRange(&areaBox, pRenderNode);
			}
    }    
  }
  else
  { 
    if (pWheel)
    {
      // restore wheel properties        
      IPhysicalEntity* pPhysics = m_pVehicle->GetEntity()->GetPhysics();    
      pe_params_wheel wheelParams;

      for (int i=0; i<m_pVehicle->GetWheelCount(); ++i)
      { 
        const pe_cargeomparams* pParams = m_pVehicle->GetWheelPart(i)->GetIWheel()->GetCarGeomParams();

        wheelParams.iWheel = i;
        wheelParams.bBlocked = 0;
        wheelParams.suspLenMax = pParams->lenMax;
        wheelParams.bDriving = pParams->bDriving;      
        wheelParams.minFriction = pParams->minFriction;
        wheelParams.maxFriction = pParams->maxFriction;
        pPhysics->SetParams(&wheelParams);
      }

			if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
			{ 
				SVehicleMovementEventParams params;
				params.pComponent = pComponent;
				params.iValue = pWheel->GetWheelIndex();
				// reset the particle status
				pMovement->OnEvent(IVehicleMovement::eVME_TireRestored, params);
			}
    }  
    
    m_aiImmobilizedTimer = -1;
  }

  m_isActive = activate;      
}


//------------------------------------------------------------------------
void CVehicleDamageBehaviorBlowTire::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
  if (event == eVDBE_Hit && behaviorParams.componentDamageRatio >= 1.0f && behaviorParams.pVehicleComponent)
  {
  	// SNH: seems like this isn't needed anymore - done by damage resistance in the vehicle xml file.
		//IGameRules* pGameRules = g_pGame->GetGameRules(); 
    //static int htBullet = pGameRules->GetHitTypeId("bullet");
    //static int htGaussBullet = pGameRules->GetHitTypeId("gaussbullet");
    //static int htFire = pGameRules->GetHitTypeId("fire");
		//static int htMelee = pGameRules->GetHitTypeId("melee");
		//static int htFrag = pGameRules->GetHitTypeId("frag");

    //if (behaviorParams.hitType && (behaviorParams.hitType==htBullet || behaviorParams.hitType==htGaussBullet || behaviorParams.hitType==htFire || behaviorParams.hitType==htMelee || behaviorParams.hitType==htFrag))
    {     
      m_component = behaviorParams.pVehicleComponent->GetComponentName();
      Activate(true);    
    }
  }
  else if (event == eVDBE_Repair && behaviorParams.componentDamageRatio < 1.f)
  {
    Activate(false);
  }
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBlowTire::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
  switch (event)
  {
  case eVE_Timer:
    {
      if (params.iParam == m_aiImmobilizedTimer)
      {
        // notify AI passengers
        IScriptTable* pTable = m_pVehicle->GetEntity()->GetScriptTable();
        HSCRIPTFUNCTION scriptFunction(NULL);
        
        if (pTable && pTable->GetValue("OnVehicleImmobilized", scriptFunction) && scriptFunction)
        {
          Script::Call(gEnv->pScriptSystem, scriptFunction, pTable);
        }

        m_aiImmobilizedTimer = -1;
      }
    }
    break;
  }
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBlowTire::Update(const float deltaTime)
{	
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBlowTire::Serialize(TSerialize ser, unsigned aspects)
{
  if (ser.GetSerializationTarget() != eST_Network)
  { 
		if (ser.IsReading())
			Activate(false); // reset

		bool bActive;

    ser.Value("isActive", bActive);
    ser.Value("component", m_component);
    ser.Value("immobilizedTimer", m_aiImmobilizedTimer);
   		  
		Activate(bActive);    		
  }
}

void CVehicleDamageBehaviorBlowTire::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->Add(m_component);
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorBlowTire);
