/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements Warrior movement

-------------------------------------------------------------------------
History:
- 22:06:2006: Created by Michael Rauh

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameUtils.h"
#include "IVehicleSystem.h"
#include "VehicleMovementWarrior.h"


enum 
{
  AXIS_X = 0,
  AXIS_Y,
  AXIS_Z
};

//------------------------------------------------------------------------
template<class T>
inline T CreateRotation(int axis, float rad)
{
  switch (axis)
  {
  case AXIS_X:
    return T::CreateRotationX(rad);    
  case AXIS_Y:
    return T::CreateRotationY(rad);
  case AXIS_Z:
    return T::CreateRotationZ(rad);  
  }
  return T::CreateIdentity();
}

//------------------------------------------------------------------------
CVehicleMovementWarrior::CVehicleMovementWarrior()
: m_pPlatformPos(NULL)
{ 
  m_maxThrustersDamaged = -1;
  m_thrustersDamaged = 0;
  m_collapsedFeetAngle = 0;
  m_collapsedLegAngle = 0;
  m_collapseTimer = -1.f;
  m_platformDown = false;
  m_collapsed = false;
  m_recoverTime = 20.f;
  m_pTurret = 0;
  m_pCannon = 0;
  m_pWing = 0;
}

//------------------------------------------------------------------------
CVehicleMovementWarrior::~CVehicleMovementWarrior()
{ 
  m_pVehicle->GetGameObject()->DisableUpdateSlot(m_pVehicle, IVehicle::eVUS_EnginePowered);

  std::for_each(m_thrustersInit.begin(), m_thrustersInit.end(), stl::container_object_deleter());
}

//------------------------------------------------------------------------
bool CVehicleMovementWarrior::Init(IVehicle* pVehicle, const SmartScriptTable &table)
{
  SmartScriptTable hovercraftTable;
  if (!table->GetValue("Hovercraft", hovercraftTable))
    return false;

  if (!CVehicleMovementHovercraft::Init(pVehicle, hovercraftTable))
    return false;

  table->GetValue("maxThrustersDamaged", m_maxThrustersDamaged);
  table->GetValue("collapsedFeetAngle", m_collapsedFeetAngle);
  table->GetValue("collapsedLegAngle", m_collapsedLegAngle);
  table->GetValue("recoverTime", m_recoverTime);

  // save original thruster values
  m_thrustersInit.reserve(m_vecThrusters.size());

  for (TThrusters::iterator it=m_vecThrusters.begin(); it!=m_vecThrusters.end(); ++it)
  {
    m_thrustersInit.push_back( new SThruster(**it) );
  }

  m_pTurret = m_pVehicle->GetPart("turret1");
  m_pCannon = m_pVehicle->GetPart("cannon");
  m_pWing   = m_pVehicle->GetPart("generator");

	m_pPlatformPos = m_pVehicle->GetHelper("platform_pos");
  
  return true;
}


//------------------------------------------------------------------------
void CVehicleMovementWarrior::Reset()
{
  CVehicleMovementHovercraft::Reset();  

  if (IsCollapsing() || IsCollapsed())
  {
    m_pVehicle->GetGameObject()->DisableUpdateSlot(m_pVehicle, IVehicle::eVUS_EnginePowered); 
  }

  if (IsCollapsing())
  {
    m_bEngineAlwaysOn = false;
  }

  m_thrustersDamaged = 0;
  m_collapseTimer = -1.f;
  m_platformDown = false;
  m_collapsed = false;
    
  ResetThrusters();
}

//------------------------------------------------------------------------
void CVehicleMovementWarrior::ResetThrusters()
{
  assert(m_vecThrusters.size() == m_thrustersInit.size());
  assert(m_numThrusters == m_thrustersInit.size());

  for (int i=0; i<m_numThrusters; ++i)
  { 
    SThruster* curr = m_vecThrusters[i];
    SThruster* prev = m_thrustersInit[i];

    curr->heightAdaption = prev->heightAdaption;
    curr->hoverHeight    = prev->hoverHeight;
    curr->hoverVariance  = prev->hoverVariance;
  }
}


//------------------------------------------------------------------------
bool CVehicleMovementWarrior::IsCollapsing()
{
  return m_collapseTimer >= 0.f; 
}

//------------------------------------------------------------------------
void CVehicleMovementWarrior::EnableThruster(SThruster* pThruster, bool enable)
{
  pThruster->enabled = enable;
  //if (!enable)
    //pThruster->hoverHeight = 0.2f;
    
  if (!pThruster->pHelper)
  {
    // disable exhaust
    // todo: add direct access to these
    for (std::vector<SExhaustStatus>::iterator it = m_paStats.exhaustStats.begin(); it != m_paStats.exhaustStats.end(); ++it)
    {
      if (it->pHelper == pThruster->pHelper)
      {
        it->enabled = (int)enable;
      }
    }
  }

  int nDamaged = m_thrustersDamaged + (enable ? -1 : 1);

  if (m_maxThrustersDamaged >= 0 && nDamaged >= m_maxThrustersDamaged && m_thrustersDamaged < m_maxThrustersDamaged)
  {
    Collapse();
  }
  m_thrustersDamaged = nDamaged;
}

//------------------------------------------------------------------------
void CVehicleMovementWarrior::OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params)
{
  CVehicleMovementHovercraft::OnEvent(event, params);

  if (event == eVME_Damage && params.pComponent && params.fValue >= 1.f)
  {
    // disable nearest thruster
    const AABB& bounds = params.pComponent->GetBounds();
    float minDistSq = 10000.f;    
    SThruster* pThrusterNearest = 0;
    bool bContained = false;

    for (TThrusters::iterator it=m_vecThrusters.begin(); it!=m_vecThrusters.end(); ++it)
    {       
      if (bounds.IsContainPoint((*it)->pos))
      {
        EnableThruster(*it, false);
        bContained = true;
        break;
      }
        
      float distSq = Vec3((*it)->pos - params.vParam).len2();

      if (distSq < minDistSq)
      {
        minDistSq = distSq;
        pThrusterNearest = *it;
      }
    }

    // if none inside component bbox, disable nearest one
    if (pThrusterNearest && !bContained)
      EnableThruster(pThrusterNearest, false);
  }
  else if (event == eVME_GroundCollision && params.iValue)
  { 
    // register first ground contacts after platform is down
    // maybe hold a map for this
    for (TThrusters::iterator it=m_vecThrusters.begin(); it!=m_vecThrusters.end(); ++it)
    {
      SThruster* pThruster = *it;
      if  (pThruster->pPart && pThruster->pPart->GetPhysId() == params.iValue)
      {
        pThruster->groundContact = true;
      }
    }
    
  }
}



//////////////////////////////////////////////////////////////////////////
// NOTE: This function must be thread-safe. Before adding stuff contact MarcoC.
void CVehicleMovementWarrior::ProcessMovement(const float deltaTime)
{
  if (!IsCollapsed())
  {
    CVehicleMovementHovercraft::ProcessMovement(deltaTime);
  }
}

//------------------------------------------------------------------------
void CVehicleMovementWarrior::Collapse()
{
  if (IsCollapsing() || IsCollapsed())
    return;

  // start collapsing
  m_collapseTimer = 0.f;

  for (TThrusters::iterator it=m_vecThrusters.begin(); it!=m_vecThrusters.end(); ++it)
  {
    SThruster* pThruster = *it;

    if (pThruster->pPart && pThruster->heightAdaption > 0.f)
    {
      pThruster->enabled = true; // enable all for collapsing
      //pThruster->hoverHeight *= 0.5f;
      pThruster->heightAdaption *= 2.f;
    }
    else
    {
      pThruster->enabled = true;      
      pThruster->hoverVariance = 0.f;      
    }
  }

  // make sure engine gets updated
  m_pVehicle->GetGameObject()->EnableUpdateSlot(m_pVehicle, IVehicle::eVUS_EnginePowered);  
  m_bEngineAlwaysOn = true;

  SGameObjectEvent event(eCGE_Event_Collapsing, eGOEF_ToScriptSystem);
  m_pVehicle->GetGameObject()->SendEvent(event);
}


//------------------------------------------------------------------------
void CVehicleMovementWarrior::Collapsed(bool collapsed)
{
  if (collapsed && !m_collapsed)
  {
    m_collapsed = true;
    EnableMovementProcessing(false);
    m_bEngineAlwaysOn = false;
        
    SGameObjectEvent event(eCGE_Event_Collapsed, eGOEF_ToScriptSystem);          
    m_pVehicle->GetGameObject()->SendEvent(event);

  }
  else if (!collapsed && m_collapsed)
  {
    m_collapsed = false;      
    m_collapseTimer = -1.f;
    m_platformDown = false;    
    
    // just reset damage for now
    m_damage = 0.f; 
    m_thrustersDamaged = 0;
    
    ResetThrusters();
    
    for (std::vector<SExhaustStatus>::iterator it = m_paStats.exhaustStats.begin(); it != m_paStats.exhaustStats.end(); ++it)
    {
      it->enabled = 1;      
    }

    m_pVehicle->GetGameObject()->DisableUpdateSlot(m_pVehicle, IVehicle::eVUS_EnginePowered);
    EnableMovementProcessing(true);
  }
}

//------------------------------------------------------------------------
float CVehicleMovementWarrior::RotatePart(IVehiclePart* pPart, float angleGoal, int axis, float speed, float deltaTime, float maxDelta)
{
  assert (axis >= AXIS_X && axis <= AXIS_Z);

  if (!pPart)
    return 0.f;

  const Matrix34& tm = pPart->GetLocalBaseTM();
  Matrix33 tm33(tm);

  float ang = Ang3::GetAnglesXYZ(tm33)[axis];

  float absDelta = abs(angleGoal - ang);
  if (absDelta > 0.01f)
  {
    float angleDelta = min(absDelta, speed*deltaTime);

    if (maxDelta > 0.f)
      angleDelta = min(angleDelta, maxDelta);

    angleDelta *= sgn(angleGoal-ang); 

    Matrix34 newTM( tm33 * CreateRotation<Matrix33>(axis, angleDelta), tm.GetTranslation() );
    pPart->SetLocalBaseTM(newTM);          

    return angleDelta;
  }

  return 0.f;
}

//------------------------------------------------------------------------
void CVehicleMovementWarrior::Physicalize()
{
  CVehicleMovementHovercraft::Physicalize();
}

//------------------------------------------------------------------------
bool CVehicleMovementWarrior::UpdateAlways()
{ 
  //return IsCollapsing() || IsCollapsed();    
  return false;
}

//------------------------------------------------------------------------
bool CVehicleMovementWarrior::StartEngine(EntityId driverId)
{	
  if (IsCollapsing())
  {
    return false;
  }

  if (!CVehicleMovementHovercraft::StartEngine(driverId))
    return false;

  return true;
}

//------------------------------------------------------------------------
void CVehicleMovementWarrior::StopEngine()
{
  CVehicleMovementHovercraft::StopEngine();  
}

//------------------------------------------------------------------------
void CVehicleMovementWarrior::Serialize(TSerialize ser, unsigned aspects)
{
  CVehicleMovementHovercraft::Serialize(ser, aspects);

  if (ser.GetSerializationTarget() != eST_Network)
  {
    ser.Value("m_thrustersDamaged", m_thrustersDamaged);
    ser.Value("m_collapseTimer", m_collapseTimer);
    ser.Value("m_collapsed", m_collapsed);
    ser.Value("m_platformDown", m_platformDown);

    char buf[16];    
    for (int i=0; i<m_numThrusters; ++i)
    {
      _snprintf(buf, 16, "thruster_%d", i);
      ser.BeginGroup(buf);
      ser.Value("enabled", m_vecThrusters[i]->enabled);
      ser.Value("heightAdaption", m_vecThrusters[i]->heightAdaption);
      ser.Value("hoverVariance", m_vecThrusters[i]->hoverVariance);            
      ser.EndGroup();
    }    
  }
}

//------------------------------------------------------------------------
void CVehicleMovementWarrior::Update(const float deltaTime)
{  
  FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );

  if (!IsCollapsing())
    CVehicleMovementHovercraft::Update(deltaTime);
  else
    CVehicleMovementBase::Update(deltaTime);
  
  if (IsCollapsing())
  {
    m_collapseTimer += deltaTime; 

    // check platform
		Vec3 platformPos;

		if (m_pPlatformPos)
			platformPos = m_pPlatformPos->GetWorldTM().GetTranslation();
		else
			platformPos.zero();

    float dist = platformPos.z - GetISystem()->GetI3DEngine()->GetTerrainElevation(platformPos.x, platformPos.y);
    if (dist < 1.f)
    {
      m_platformDown = true;      
    }

    // center turret
    RotatePart(m_pTurret, DEG2RAD(0.f), AXIS_Z, DEG2RAD(2.5f), deltaTime);

    // take down wing and cannon
    RotatePart(m_pWing, DEG2RAD(-12.5f), AXIS_X, DEG2RAD(3.f), deltaTime);
    RotatePart(m_pCannon, DEG2RAD(-20.f), AXIS_X, DEG2RAD(2.5f), deltaTime);

    if (!m_platformDown)
    { 
      // handle legs to bring down platform
      TThrusters::iterator iter;
      for (iter=m_vecThrusters.begin(); iter!=m_vecThrusters.end(); ++iter)
      {
        SThruster* pThruster = *iter;

        if (pThruster->heightAdaption <= 0.f)        
        {
          pThruster->hoverHeight = max(0.1f, pThruster->hoverHeight - 0.6f*deltaTime);
          continue;
        }
        else
        {
          //if (!pThruster->groundContact)          
          //pThruster->hoverHeight = max(0.1f, pThruster->hoverHeight - 0.2f*deltaTime);          
        }

        /* 
        // special legs control
        float collapseSpeed = DEG2RAD(5.f);
        float maxDistMovable = 1.f/0.8f;

        float dist = (isneg(pThruster->prevDist)) ? 0.f : pThruster->hoverHeight - pThruster->prevDist;

        if (isneg(dist))
        {
        collapseSpeed *= max(0.f, 1.f + maxDistMovable*dist);
        }

        if (collapseSpeed > 0.f)
        { 
        float angle = RotatePart(pThruster->pParentPart, DEG2RAD(m_collapsedLegAngle), collapseSpeed, deltaTime);          
        RotatePart(pThruster->pPart, DEG2RAD(m_collapsedFeetAngle), collapseSpeed, deltaTime);
        }
        */
      }      
    }
    else
    {
      if (!m_collapsed)
      {
        Collapsed(true); 
      }
    }
  }
  
  if (IsPowered() && !IsCollapsed())
  { 
    // "normal" legs control here   

    bool bStartComplete = (m_startComplete > 1.5f);
    float adaptionSpeed = IsCollapsing() ? 0.8f : 1.5f;
    int t = 0;

    for (TThrusters::iterator iter=m_vecThrusters.begin(); iter!=m_vecThrusters.end(); ++iter)
    {
      SThruster* pThruster = *iter;
      ++t;

      if (pThruster->heightAdaption > 0.f && bStartComplete && pThruster->pPart && pThruster->pParentPart)
      {         
        const char* footName = pThruster->pPart->GetName().c_str();        
        EWarriorMovement mode = eWM_Hovering;
        float correction = 0.f, maxCorrection = 0.f;        

        // adjust legs        
        float error = 0.f; 

        if (!pThruster->hit)
          error = pThruster->hoverHeight; // when not hit, correct downwards 
        else if (pThruster->prevDist > 0.f)
          error = pThruster->prevDist - pThruster->hoverHeight; 
        
        if (mode != eWM_None && abs(error) > 0.05f)
        {
          float speed = max(0.1f, min(1.f, abs(error))) * adaptionSpeed;
          correction = -sgn(error) * min(speed*deltaTime, abs(error)); // correct up to error

          // don't correct more than heightAdaption allows
          maxCorrection = abs((pThruster->heightInitial + sgn(correction)*pThruster->heightAdaption) - pThruster->pos.z);          
          float minCorrection = (pThruster->groundContact) ? 0.f : -maxCorrection;          

          correction = CLAMP(correction, minCorrection, maxCorrection);

          if (abs(correction) > 0.0001f)
          { 
            // positive correction for leg, negative for foot
            Matrix34 legLocal  = pThruster->pParentPart->GetLocalBaseTM();
            Matrix34 footLocal = pThruster->pPart->GetLocalBaseTM();

            float radius = footLocal.GetTranslation().len();
            float deltaAngle = correction / radius; // this assumes correction on circle (accurate enough for large radius)

            Matrix34 legTM  = Matrix33(legLocal) * Matrix33::CreateRotationX(deltaAngle);
            Matrix34 footTM = Matrix33(footLocal) * Matrix33::CreateRotationX(-deltaAngle);

            legTM.SetTranslation(legLocal.GetTranslation());
            footTM.SetTranslation(footLocal.GetTranslation());

            pThruster->pParentPart->SetLocalBaseTM(legTM);
            pThruster->pPart->SetLocalBaseTM(footTM);
          }          
        }

        if (IsProfilingMovement())
        {
          static ICVar* pDebugLeg = GetISystem()->GetIConsole()->GetCVar("warrior_debug_leg");
          if (pDebugLeg && pDebugLeg->GetIVal() == t)
          {
            //CryLog("hoverErr %.2f, levelErr %.2f, neutralErr %.2f -> %s corr %.3f (max %.2f)", hoverError, levelError, neutralError, sMode, correction, maxCorrection);
          }                    
        }        
      }
    }
  }

  // regain control
  if (m_collapseTimer > m_recoverTime)
  {     
    Collapsed(false);
  }

  for (TThrusters::iterator it=m_vecThrusters.begin(); it!=m_vecThrusters.end(); ++it)
  {
    (*it)->groundContact = false;
  }
}


//------------------------------------------------------------------------
bool CVehicleMovementWarrior::RequestMovement(CMovementRequest& movementRequest)
{
  if (IsCollapsing() || IsCollapsed())
    return false;
 
  return CVehicleMovementHovercraft::RequestMovement(movementRequest);  
}

void CVehicleMovementWarrior::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->AddContainer(m_thrustersInit);
	CVehicleMovementHovercraft::GetMemoryStatistics(s);
}