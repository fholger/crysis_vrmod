/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: HUD radar

-------------------------------------------------------------------------
History:
- 2006/2007: Jan Müller

*************************************************************************/
#include "StdAfx.h"
#include "IUIDraw.h"
#include "Actor.h"
#include "Weapon.h"
#include "HUD.h"
#include "HUDRadar.h"
#include "Game.h"
#include "IWorldQuery.h"
#include "../GameRules.h"
#include "IVehicleSystem.h"
#include "IAIGroup.h"
#include "HUDSilhouettes.h"

#include "GameFlashAnimation.h"
#include "GameFlashLogic.h"
#include "GameCVars.h"

#include "HUD/HUDPowerStruggle.h"
#include "HUD/HUDScopes.h"

#define RANDOM() ((((float)cry_rand()/(float)RAND_MAX)*2.0f)-1.0f)

const static float fRadarSizeOverTwo = 47.0f;
const static float fEntitySize = 4.0f;
const static float fEntityMaxDistance = fRadarSizeOverTwo - fEntitySize;
const static float fRadarDefaultRadius = 75.0f;

//-----------------------------------------------------------------------------------------------------

CHUDRadar::CHUDRadar()
{
	m_pActorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	m_pVehicleSystem = gEnv->pGame->GetIGameFramework()->GetIVehicleSystem();

	m_fX = 93.75f; // RadarCenterX in Flash * 800 (CryEngineRefSizeX) / 1024 (FlashRefSizeX)
	m_fY = 525.0f; // RadarCenterX in Flash * 600 (CryEngineRefSizeY) / 768  (FlashRefSizeY)

	m_radarDotSizes[0] = 1.3f;
	m_radarDotSizes[1] = 2.0f;
	m_radarDotSizes[2] = 2.5f;
	m_radarDotSizes[3] = 2.0f;	//this is currently used as a multiplier for the hunter

	m_fTime = 0.0f;

	m_taggedPointer = 0;
	ResetTaggedEntities();

	m_soundIdCounter = unsigned(1<<26);

	m_fLastViewRot = m_fLastCompassRot = m_fLastStealthValue = m_fLastStealthValueStatic = m_fLastFov = -9999.99f;
	m_lookAtObjectID = 0;
	m_lookAtTimer = 0.0f;
	m_scannerObjectID = 0;
	m_scannerTimer = 0.0f;
	m_scannerGatherTimer = 0.50f;
	m_startBroadScanTime = .0f;
	m_lastScan = 0.0f;
	m_jammingValue = 0.0f;
	m_jammerID = 0;
	m_jammerDisconnectMap = false;
	m_mapId = 0;
	m_fPDAZoomFactor = 1.0;
	m_fPDATempZoomFactor = 1.0;
	m_vPDAMapTranslation = Vec2(0,0);
	m_vPDATempMapTranslation = Vec2(0,0);
	m_bDragMap = false;
	m_fLastRadarRatio = -1.0f;

	for(int i = 0; i < NUM_MAP_TEXTURES; ++i)
	{
		m_miniMapStartX[i] = m_miniMapStartY[i] = 0;
		m_miniMapEndX[i] = m_miniMapEndY[i] = 2048;
		m_mapRadarRadius[i] = -1;
	}

	m_startOnScreenX = m_startOnScreenY = m_endOnScreenX = m_endOnScreenY = 0;
	m_mapDivX = m_mapDivY = 1.0f;
	m_mapDimX = m_mapDimY = 1;

	m_renderMiniMap = false;

	m_bsRadius = 0.0f;
	m_bsUseParameter = m_bsKeepEntries = false;
	m_bsPosition = Vec3(0,0,0);

	m_flashPDA = NULL;
	m_flashRadar = NULL;
	m_iMultiplayerEnemyNear = 0;

	m_pLevelData = NULL;

	m_coordinateToString[0] = "A%d";
	m_coordinateToString[1] = "B%d";
	m_coordinateToString[2] = "C%d";
	m_coordinateToString[3] = "D%d";
	m_coordinateToString[4] = "E%d";
	m_coordinateToString[5] = "F%d";
	m_coordinateToString[6] = "G%d";
	m_coordinateToString[7] = "H%d";

	IEntityClassRegistry *pEntityClassRegistry = gEnv->pEntitySystem->GetClassRegistry();

	//save some classes for comparison
	m_pVTOL					= pEntityClassRegistry->FindClass( "US_vtol" );
	m_pHeli					= pEntityClassRegistry->FindClass( "Asian_helicopter" );
	m_pHunter				= pEntityClassRegistry->FindClass( "Hunter" );
	m_pWarrior			= pEntityClassRegistry->FindClass( "Alien_warrior" );
	m_pAlien				= pEntityClassRegistry->FindClass( "Alien" );
	m_pTrooper			= pEntityClassRegistry->FindClass( "Trooper" );
	m_pPlayerClass	= pEntityClassRegistry->FindClass( "Player" );
	m_pGrunt				= pEntityClassRegistry->FindClass( "Grunt" );
	m_pScout				= pEntityClassRegistry->FindClass( "Scout" );
	m_pTankUS				= pEntityClassRegistry->FindClass( "US_tank" );
	m_pTankA				= pEntityClassRegistry->FindClass( "Asian_tank" );
	m_pLTVUS				= pEntityClassRegistry->FindClass( "US_ltv" );
	m_pLTVA					= pEntityClassRegistry->FindClass( "Asian_ltv" );
	m_pAAA					= pEntityClassRegistry->FindClass( "Asian_aaa" );
	m_pTruck				= pEntityClassRegistry->FindClass( "Asian_truck" );
	m_pAPCUS				= pEntityClassRegistry->FindClass( "US_apc" );
	m_pAPCA					= pEntityClassRegistry->FindClass( "Asian_apc" );
	m_pBoatCiv			= pEntityClassRegistry->FindClass( "Civ_speedboat" );
	m_pHover				= pEntityClassRegistry->FindClass( "US_hovercraft" );
	m_pBoatUS				= pEntityClassRegistry->FindClass( "US_smallboat" );
	m_pBoatA				= pEntityClassRegistry->FindClass( "Asian_patrolboat" );
	m_pCarCiv				= pEntityClassRegistry->FindClass( "Civ_car1" );
	m_pParachute		= pEntityClassRegistry->FindClass( "Parachute" );

	assert ( m_pLTVA && m_pLTVUS && m_pTankA && m_pTankUS && m_pWarrior && m_pHunter && m_pAlien && m_pScout && m_pGrunt && m_pHeli && m_pVTOL && m_pAAA && m_pTruck && m_pAPCUS && m_pAPCA && m_pBoatCiv && m_pHover && m_pBoatUS && m_pBoatA && m_pCarCiv && m_pParachute);
}

//-----------------------------------------------------------------------------------------------------

CHUDRadar::~CHUDRadar()
{
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::AddTaggedEntity(EntityId iEntityId)
{
	m_taggedEntities[m_taggedPointer] = iEntityId;
	m_taggedPointer ++;
	if(m_taggedPointer > NUM_TAGGED_ENTITIES)
		m_taggedPointer = 0;
}

void CHUDRadar::AddEntityToRadar(EntityId id)
{
	if (!IsOnRadar(id, 0, m_entitiesOnRadar.size()-1))
	{
		AddToRadar(id);
		g_pGame->GetHUD()->OnEntityAddedToRadar(id);
	}
}

void CHUDRadar::AddEntityTemporarily(EntityId id, float time)
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(id);
	if(pEntity)
		ShowEntityTemporarily(ChooseType(pEntity, true), id, time);
}

void CHUDRadar::ShowEntityTemporarily(FlashRadarType type, EntityId id, float timeLimit)
{
	if(IsOnRadar(id, 0, m_entitiesOnRadar.size()-1))	//already scanned ?
		return;

	for(int e = 0; e < m_tempEntitiesOnRadar.size(); ++e)
	{
		if(id == m_tempEntitiesOnRadar[e].m_id)
			return;
	}
	m_tempEntitiesOnRadar.push_back(TempRadarEntity(id, type, timeLimit));
}

void CHUDRadar::AddStoryEntity(EntityId id, FlashRadarType type /* = EWayPoint */, const char* text /* = NULL */)
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(id);
	if(pEntity)
	{
		std::vector<TempRadarEntity>::iterator it = m_storyEntitiesOnRadar.begin();
		std::vector<TempRadarEntity>::iterator end = m_storyEntitiesOnRadar.end();
		for(; it != end; ++it)
			if((*it).m_id == id)
				return;
		TempRadarEntity temp(id, type, 0.0f, text);
		m_storyEntitiesOnRadar.push_back(temp);
	}
}

void CHUDRadar::RemoveStoryEntity(EntityId id)
{
	std::vector<TempRadarEntity>::iterator it = m_storyEntitiesOnRadar.begin();
	std::vector<TempRadarEntity>::iterator end = m_storyEntitiesOnRadar.end();
	for(; it != end; ++it)
	{
		if((*it).m_id == id)
		{
			m_storyEntitiesOnRadar.erase(it);
			return;
		}
	}
}

void CHUDRadar::ShowSoundOnRadar(Vec3 pos, float intensity)
{
	intensity = std::min(8.0f, intensity); //limit the size of the sound on radar ...
	CActor *pActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	m_soundsOnRadar.push_back(RadarSound(pos, intensity, m_soundIdCounter++));

	if(m_soundIdCounter > unsigned(1<<29))
		m_soundIdCounter = unsigned(1<<26);
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::RemoveFromRadar(EntityId id)
{
	{
		std::deque<RadarEntity>::iterator it = m_entitiesOnRadar.begin();
		std::deque<RadarEntity>::iterator end = m_entitiesOnRadar.end();
		for(; it != end; ++it)
		{
			if((*it).m_id == id)
			{
				m_entitiesOnRadar.erase(it);
				return;
			}
		}
	}
	{
		for(int i = 0; i < NUM_TAGGED_ENTITIES; ++i)
		{
			if(m_taggedEntities[i] == id)
			{
				m_taggedEntities[i] = 0;
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

template <typename T, EFlashVariableArrayType flashType, int NUM_VALS> struct ArrayFillHelper
{
	ArrayFillHelper(IFlashPlayer* pFlashPlayer, const char* varName) : m_pFlashPlayer(pFlashPlayer), m_varName(varName), m_nIndex(0), m_nValues(0)
	{
	}

	~ArrayFillHelper()
	{
		Flush();
	}

	ILINE void push_back(const T& v)
	{
		m_values[m_nValues++] = v;
		if (m_nValues == NUM_VALS)
			Flush();
	}

	void Flush()
	{
		if (m_nValues > 0)
		{
			m_pFlashPlayer->SetVariableArray(flashType, m_varName, m_nIndex, m_values, m_nValues);
			m_nIndex+=m_nValues;
			m_nValues = 0;
		}
	}

	IFlashPlayer* m_pFlashPlayer;
	const char* m_varName;
	int m_nIndex;
	int m_nValues;
	T m_values[NUM_VALS];
};


template<typename T, EFlashVariableArrayType flashType, int NUM_VALS>
int FillUpDoubleArray(ArrayFillHelper<T,flashType,NUM_VALS> *fillHelper,
											double a, double b, double c, double d,
											double e, double f, double g = 100.0f, double h = 100.0f, double i = 0.0f, double j = 0.0f, double k = 0.0f)
{
	fillHelper->push_back(a);
	fillHelper->push_back(b);
	fillHelper->push_back(c);
	fillHelper->push_back(d);
	fillHelper->push_back(e);
	fillHelper->push_back(f);
	fillHelper->push_back(g);
	fillHelper->push_back(h);
	fillHelper->push_back(i);
	fillHelper->push_back(j);
	fillHelper->push_back(k);
	return 11;
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::Update(float fDeltaTime)
{
	FUNCTION_PROFILER(GetISystem(),PROFILE_GAME);

	if(!m_flashRadar)
		return; //we require the flash radar now

	CActor *pActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if (!pActor)
		return;
	if(pActor->GetHealth() <= 0)
	{
		if(gEnv->bMultiplayer)
		{
			//render the mini map
			if(m_renderMiniMap)
				RenderMapOverlay();
		}
		return;
	}

	if(!m_flashRadar->IsAvailable("setObjectArray"))
		return;

	//double array buffer for flash transfer optimization
	ArrayFillHelper<double, FVAT_Double, NUM_ARRAY_FILL_HELPER_SIZE> entityValues (m_flashRadar->GetFlashPlayer(), "m_allValues");
	int numOfValues = 0;

	float now = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	float fRadius = fRadarDefaultRadius;
	if(m_mapRadarRadius[m_mapId] > 2)
		fRadius = (float)m_mapRadarRadius[m_mapId];

	//check for a broadscan:
	if(m_startBroadScanTime && now - m_startBroadScanTime > 1.5f)
		StartBroadScan();

	m_fTime += fDeltaTime * 3.0f;
	IRenderer *pRenderer = gEnv->pRenderer;
	float fWidth43 = pRenderer->GetHeight()*1.333f;
	float lowerBoundX = m_fX - fRadarSizeOverTwo;	//used for flash radar position computation

	CPlayer *pPlayer = static_cast<CPlayer*> (pActor);

	Matrix34	playerViewMtxInverted = pPlayer->GetViewMatrix().GetInverted();
	Vec3			playerViewMtxTranslation = pPlayer->GetViewMatrix().GetTranslation();

	//CompassStealth***************************************************************
	UpdateCompassStealth(pActor, fDeltaTime);
	//~CompassStealth**************************************************************

	//binocs***********************************************************************
	UpdateBinoculars(pActor, fDeltaTime);
	//~binocs**********************************************************************

	//jammer***********************************************************************
	UpdateRadarJammer(pActor);
	//~jammer**********************************************************************

	//*********************************ACTUAL SCANNING****************************
	Vec3 vOrigin = GetISystem()->GetViewCamera().GetPosition();
	//scan proximity for entities
	if(now - m_lastScan > 0.2f) //5hz scanning
	{
		float scanningRadius = fRadius*1.3f;
		ScanProximity(vOrigin, scanningRadius);
		m_lastScan = now;
	}
	
	//*********************************MAIN ENTITY UPDATE*************************
	UpdateRadarEntities(pActor, fRadius, playerViewMtxInverted, numOfValues, &entityValues);
	//****************************************************************************

	// Draw MissionObjectives or their target entities
	// we draw after normal targets, so AI guys which are targets show up as Objective
	std::map<EntityId, RadarObjective>::const_iterator it = m_missionObjectives.begin();
	std::map<EntityId, RadarObjective>::const_iterator end = m_missionObjectives.end();
	for(; it != end; ++it)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(it->first);
		if (pEntity && !pEntity->IsHidden())
		{
			Vec3 vTransformed = pEntity->GetWorldPos();
			vTransformed = playerViewMtxInverted * vTransformed;
			vTransformed.z = 0;

			float scaledX = (vTransformed.x/fRadius)*fEntityMaxDistance;
			float scaledY = (vTransformed.y/fRadius)*fEntityMaxDistance;

			vTransformed.Set(scaledX, scaledY, 0.0f);

			FlashRadarFaction faction = ENeutral;
			float fAngle = 0.0f;
			float distSq = scaledX*scaledX+scaledY*scaledY;

			float fX = m_fX + vTransformed.x;
			float fY = m_fY - vTransformed.y;
			float fAlpha	= 0.75f;

			Vec2 intersection;
			if(!RadarBounds_Inside(Vec2(fX, fY), intersection))
			{
				fX = intersection.x;
				fY = intersection.y;
				faction = EFriend;
			}

			if(faction == EFriend) //dirty angle computation for arrow
			{
				Vec2 dir(m_fX - fX, m_fY - fY);
				dir.Normalize();

				fAngle = acos_tpl( Vec2(0.0f,1.0f).Dot(dir) );
				if(m_fX > fX)
					fAngle = 2*gf_PI - fAngle;
			}

			if(it->second.secondaryObjective)
				faction = (FlashRadarFaction)(int(faction)+2);	//yellow version

			//in flash
			float lowerBoundY = m_fY - fRadarSizeOverTwo;
			float dimX = (m_fX + fRadarSizeOverTwo) - lowerBoundX;
			float dimY = (m_fY + fRadarSizeOverTwo) - lowerBoundY;
			numOfValues += ::FillUpDoubleArray(&entityValues, pEntity->GetId(), 5 /* MO */, (fX - lowerBoundX) / dimX, (fY - lowerBoundY) / dimY, 180+RAD2DEG(fAngle), faction, 75.0f, fAlpha*100.0f);
		}
	}

	//temp units on radar
	for(int t = 0; t < m_tempEntitiesOnRadar.size(); ++t)
	{
		TempRadarEntity temp = m_tempEntitiesOnRadar[t];
		float diffTime = now - temp.m_spawnTime;
		if(diffTime > temp.m_timeLimit || diffTime < 0.0f)
		{
			m_tempEntitiesOnRadar.erase(m_tempEntitiesOnRadar.begin() + t);
			--t;
			continue;
		}
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(temp.m_id);
		if (pEntity && !pEntity->IsHidden())
		{
			if(IActor *tempActor = m_pActorSystem->GetActor(pEntity->GetId()))
			{
				if(tempActor->GetHealth() <= 0)
					continue;
			}

			Vec3 vTransformed = pEntity->GetWorldPos();
			vTransformed = playerViewMtxInverted * vTransformed;
			vTransformed.z = 0;

			float scaledX = (vTransformed.x/fRadius)*fEntityMaxDistance;
			float scaledY = (vTransformed.y/fRadius)*fEntityMaxDistance;
			vTransformed.Set(scaledX, scaledY, 0.0f);

			float distSq = scaledX*scaledX+scaledY*scaledY;
			float fX = m_fX + vTransformed.x;
			float fY = m_fY - vTransformed.y;

			Vec2 intersection;
			if(!RadarBounds_Inside(Vec2(fX, fY), intersection))
			{
				fX = intersection.x;
				fY = intersection.y;
			}

			float fAngle = pActor->GetAngles().z - pEntity->GetWorldAngles().z;
			float fAlpha	= 0.85f;
			float sizeScale = GetRadarSize(pEntity, pActor);

			//in flash
			float lowerBoundY = m_fY - fRadarSizeOverTwo;
			float dimX = (m_fX + fRadarSizeOverTwo) - lowerBoundX;
			float dimY = (m_fY + fRadarSizeOverTwo) - lowerBoundY;

			int playerTeam = g_pGame->GetGameRules()->GetTeam(pActor->GetEntityId());

			numOfValues += ::FillUpDoubleArray(&entityValues, temp.m_id, temp.m_type, (fX - lowerBoundX) / dimX, (fY - lowerBoundY) / dimY,
				180.0f+RAD2DEG(fAngle), FriendOrFoe(gEnv->bMultiplayer, playerTeam, pEntity,
				g_pGame->GetGameRules()), sizeScale*25.0f, fAlpha*100.0f);
		}
	}

	//tac bullet marked entities
	for(int i = 0; i < NUM_TAGGED_ENTITIES; ++i)
	{
		EntityId id = m_taggedEntities[i];
		if(id)
		{
			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(id);

			if(pEntity)
			{
				Vec3 vTransformed = pEntity->GetWorldPos();
				vTransformed = playerViewMtxInverted * vTransformed;
				vTransformed.z = 0;

				if(vTransformed.len() > fEntityMaxDistance)
				{
					vTransformed.SetLength(fEntityMaxDistance);
				}

				float fX = m_fX + vTransformed.x;
				float fY = m_fY - vTransformed.y;
				float sizeScale = GetRadarSize(pEntity, pActor);

				//in flash
				float lowerBoundY = m_fY - fRadarSizeOverTwo;
				float dimX = (m_fX + fRadarSizeOverTwo) - lowerBoundX;
				float dimY = (m_fY + fRadarSizeOverTwo) - lowerBoundY;
				numOfValues += ::FillUpDoubleArray(&entityValues, pEntity->GetId(), 7 /*tagged entity*/, (fX - lowerBoundX) / dimX, (fY - lowerBoundY) / dimY, 0.0f, 0, sizeScale*100.0f, 100.0f);
			}
		}
	}

	//sounds on radar
	for(int i = 0; i < m_soundsOnRadar.size(); ++i)
	{
		RadarSound temp = m_soundsOnRadar[i];
		float diffTime = now - temp.m_spawnTime;
		if(diffTime > 2.0f || diffTime < 0.0f) //show only 2 seconds (and catch errors)
		{
			m_soundsOnRadar.erase(m_soundsOnRadar.begin() + i);
			--i;
			continue;
		}
		else
		{
			Vec3 pos = temp.m_pos;
			pos = playerViewMtxInverted * pos;
			
			float intensity = temp.m_intensity;
			if(intensity > 4.0f)
			{
				if(!temp.m_hasChild && diffTime > (0.05 + 0.1f * RANDOM()))
				{
					m_soundsOnRadar[i].m_hasChild = true;
					ShowSoundOnRadar(temp.m_pos, temp.m_intensity * (0.5f + 0.2f * RANDOM()));	//aftermatch
				}
				
				intensity = 4.0f;
			}

			float scaledX = (pos.x/fRadius)*fEntityMaxDistance;
			float scaledY = (pos.y/fRadius)*fEntityMaxDistance;
			float distSq = (scaledX*scaledX+scaledY*scaledY)*0.70f;
			float size = intensity*(4.0f+diffTime*4.0f);
			if(size > fEntityMaxDistance)
				continue;
			if(distSq > (fEntityMaxDistance*fEntityMaxDistance*1.5f))
				continue;
			float fX = m_fX + scaledX;
			float fY = m_fY - scaledY;
			//in flash
			float lowerBoundY = m_fY - fRadarSizeOverTwo;
			float dimX = (m_fX + fRadarSizeOverTwo) - lowerBoundX;
			float dimY = (m_fY + fRadarSizeOverTwo) - lowerBoundY;
			numOfValues += ::FillUpDoubleArray(&entityValues, temp.m_id, 6 /*sound*/, (fX - lowerBoundX) / dimX, (fY - lowerBoundY) / dimY, 0.0f, 0, size*10.0f, (1.0f-(0.5f*diffTime))*100.0f);
		}
	}
	//render the mini map
	if(m_renderMiniMap)
		RenderMapOverlay();

	entityValues.Flush();
	float playerX = 0.5f;
	float playerY = 0.5f;
	if(pActor)
	{
		GetPosOnMap(pActor->GetEntity(), playerX, playerY, true);
		SFlashVarValue args[3] = {playerX, playerY, 270.0f-RAD2DEG(pActor->GetAngles().z)};
		m_flashRadar->Invoke("setObjectArray",args, 3);
		float radarRatio = (((float)(m_miniMapEndX[m_mapId] - m_miniMapStartX[m_mapId])) / fRadius) * 50.0f; 
		if(radarRatio != m_fLastRadarRatio)
		{
			m_flashRadar->Invoke("setMapScale", SFlashVarValue(radarRatio / 2048.0f) );
			m_fLastRadarRatio = radarRatio;
		}
	}
	else
	{
		m_flashRadar->Invoke("setObjectArray");
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::UpdateRadarEntities(CActor *pActor, float &fRadius, Matrix34 &playerViewMtxInverted, int &numOfValues, ArrayFillHelper<double, FVAT_Double, NUM_ARRAY_FILL_HELPER_SIZE> *entityValues)
{
	float fCos	= cosf(m_fTime); 
	float lowerBoundX = m_fX - fRadarSizeOverTwo;	//used for flash radar position computation

	//****************************************************************************
	//singleplayer squadmates are now 100% design controlled, see "SetTeamMate"

	//we get the player's team mates for team-based MP
	CGameRules *pGameRules = static_cast<CGameRules*>(gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules());
	int clientTeam = pGameRules->GetTeam(pActor->GetEntityId());
	if(gEnv->bMultiplayer)
	{
		m_teamMates.clear();
		pGameRules->GetTeamPlayers(clientTeam, m_teamMates);
	}
	//****************************************************************************

	//extra vehicle checks
	bool inVehicle = false;
	bool inAAA = false;
	float aaaDamage = 0.0f;
	if(IVehicle *pVehicle = pActor->GetLinkedVehicle())
	{
		fRadius *= 2.0f;
		inVehicle = true;

		if(pVehicle->GetEntity()->GetClass() == m_pAAA)
		{
			inAAA = true;
			if(IVehicleComponent *pAAARadar = pVehicle->GetComponent("radar"))
			{
				aaaDamage = pAAARadar->GetDamageRatio();
				m_flashRadar->Invoke("setDamage", aaaDamage);
			}
		}
	}

	//*********************************CHECKED FOUND ENTITIES*********************
	int amount = m_entitiesInProximity.size();
	for(int i = 0; i < amount; ++i)
	{
		EntityId id = m_entitiesInProximity[i];
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(id);

		if(pEntity)
		{
			//reasons (not) to go on ... *******************************************************************
			if(IsEntityTagged(id))
				continue;

			//is it a corpse ?
			IActor* tempActor = m_pActorSystem->GetActor(pEntity->GetId());
			if(tempActor && !(tempActor->GetHealth() > 0))
				continue;

			//is it the player ?
			if(pActor->GetEntityId() == id)
				continue;

			//lets find out whether this entity belongs on the radar
			bool isOnRadar = false;
			bool mate = false;
			bool unknownEnemyObject = false;
			bool unknownEnemyActor = false;
			bool airPlaneInAAA = false;

			if(inAAA)	//check whether it's an unknown, flying airplane when you are in an AAA (show all airplanes > 3m over ground)
			{
				FlashRadarType eType = ChooseType(pEntity);
				if(eType == EHeli)
				{
					Vec3 pos = pEntity->GetPos();
					if(pos.z - gEnv->p3DEngine->GetTerrainZ((int)pos.x, (int)pos.y) > 5.0f)
					{
						IVehicle *pAirplane = m_pVehicleSystem->GetVehicle(pEntity->GetId());
						if(pAirplane && pAirplane->IsPlayerDriving(false) && pAirplane->GetAltitude() > 2.5f) //GetAltitude also checks game brushes
							airPlaneInAAA = isOnRadar = true;
					}
				}
			}

			//is it a team mate in multiplayer or is it a squad mate in singleplayer ?
			if(!isOnRadar && stl::find(m_teamMates, id))
				isOnRadar = mate = true;

			//has the object been scanned already?
			if(!isOnRadar && IsOnRadar(id, 0, m_entitiesOnRadar.size()-1))
				isOnRadar = true;

			if(!isOnRadar)	//check whether it's an aggressive (non-vehicle) AI (in possible proximity),
				//which is not yet on the radar
			{
				IAIObject *pTemp = pEntity->GetAI();
				if(pTemp && AIOBJECT_VEHICLE != pTemp->GetAIType() && pTemp->IsHostile(pActor->GetEntity()->GetAI(),false))
				{
					isOnRadar = true;
					unknownEnemyObject = true;
				}
				else if(tempActor && gEnv->bMultiplayer)	//not teammate, not AI -> enemy!?
				{
					isOnRadar = true;
					unknownEnemyActor = true;
				}
			}

			if(!isOnRadar)
				continue;

			//**********************************************************************************************

			Vec3 vTransformed = pEntity->GetWorldPos();
			vTransformed = playerViewMtxInverted * vTransformed;

			//distance check********************************************************************************

			if(fabsf(vTransformed.z) > 500.0f)
				continue;
			vTransformed.z = 0;

			float sizeScale = GetRadarSize(pEntity, pActor);
			float scaledX = (vTransformed.x/fRadius)*fEntityMaxDistance;
			float scaledY = (vTransformed.y/fRadius)*fEntityMaxDistance;

			float fX = m_fX + scaledX;
			float fY = m_fY - scaledY;

			float distSq = scaledX*scaledX+scaledY*scaledY;

			if(distSq < fEntityMaxDistance*fEntityMaxDistance*3.0f)
			{	
				//these entities are near enough to check them against the border
				Vec2 intersection;
				if(!RadarBounds_Inside(Vec2(fX, fY), intersection))
				{
					if(airPlaneInAAA && !(aaaDamage == 1.0f || aaaDamage > RANDOM()))	// AAA can see airplanes over far distances
					{
						fX = intersection.x;
						fY = intersection.y;
					}
					else
						continue;
				}
			}
			else		//far away
				continue;

			float fAngle = pActor->GetAngles().z - pEntity->GetWorldAngles().z;
			float fAlpha	= 0.85f;

			//faction***************************************************************************************

			//AI Object
			IAIObject *pAIObject = pEntity->GetAI();
			int friendly = mate?EFriend:ENeutral;
			bool checkDriver = false;
			if(pAIObject)
			{
				if(AIOBJECT_VEHICLE == pAIObject->GetAIType())	//switch to vehicle texture ?
				{
					if(IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(id))
					{
						if (IVehicleSeat* pSeat = pVehicle->GetSeatById(1))
						{
							if (pSeat->IsDriver())
							{
								EntityId driverId = pSeat->GetPassenger();
								IEntity *temp = gEnv->pEntitySystem->GetEntity(driverId);
								if(temp && temp->GetAI())
								{
									pAIObject = temp->GetAI(); //check the driver instead of the vehicle
									checkDriver = true;
								}
							}
						}
					}
				}

				if((unknownEnemyObject || pAIObject->IsHostile(pActor->GetEntity()->GetAI(),false)) && (checkDriver || AIOBJECT_VEHICLE != pAIObject->GetAIType()))
				{
					friendly = EEnemy;

					IUnknownProxy *pUnknownProxy = pAIObject->GetProxy();
					if(pUnknownProxy)
					{
						int iAlertnessState = pUnknownProxy->GetAlertnessState();

						if(unknownEnemyObject) //check whether the object is near enough and alerted
						{
							if(iAlertnessState < 1)
								continue;
							if(distSq > 225.0f)
								continue;
							else if(distSq > 25.0f)
								fAlpha -= 0.5f - ((225.0f - distSq) * 0.0025f); //fade out with distance

							if(tempActor && g_pGameCVars->g_difficultyLevel < 4) //new rule : once aggroing the player, enemies get added permanently
								AddToRadar(id);
						}

						if(1 == iAlertnessState)
						{
							fAlpha = 0.65f + fCos * 0.35f;
							friendly = EAggressor;
						}
						else if(2 == iAlertnessState)
						{
							fAlpha = 0.65f + fCos * 0.35f;
							friendly = ESelf;
						}
					}
					else
						continue;
				}
				else if(gEnv->bMultiplayer)
				{
					if(mate)
						friendly = EFriend;
					else
						friendly = EEnemy;
				}
				else
				{
					if(checkDriver || mate)	//probably the own player
						friendly = EFriend;
					else
						friendly = ENeutral;
				}
			}
			else if(gEnv->bMultiplayer)	//treats factions in multiplayer
			{
				if(tempActor)
				{
					bool scannedEnemy = false;
					if(!mate && !unknownEnemyActor) //probably MP scanned enemy
					{
						friendly = EEnemy;
						scannedEnemy = true;
					}

					if(unknownEnemyActor || scannedEnemy)	//unknown or known enemy in MP !?
					{
						CPlayer *pPlayer=0;
						if (static_cast<CActor *>(tempActor)->GetActorClass()==CPlayer::GetActorClassType())
							pPlayer=static_cast<CPlayer *>(tempActor);

						if (pPlayer && !(pPlayer->GetNanoSuit() && pPlayer->GetNanoSuit()->GetCloak()->GetState()!=0))
						{
							float length = vTransformed.GetLength();
							if(length < 20.0f)
							{
								if(length < 0.1f)
									length = 0.1f;
								const int cLenThres = (int)(100.0f / length);
								if(m_iMultiplayerEnemyNear < cLenThres)
									m_iMultiplayerEnemyNear = cLenThres;
							}
						}
					}
					if(!scannedEnemy && !mate)
						continue;
				}
				else
				{
					IVehicle *pVehicle = m_pVehicleSystem->GetVehicle(pEntity->GetId());
					if(pVehicle && !pVehicle->GetDriver())
					{
						int team = g_pGame->GetGameRules()->GetTeam(pEntity->GetId());
						if(team == 0)
							friendly = ENeutral;
						else if(team == clientTeam)
							friendly = EFriend;
						else
							friendly = EEnemy;
					}
					else
						continue;
				}
			}
			else
				continue;
			//**********************************************************************************************

			//requested by Sten : teammates should be more transparent
			if(mate)
				fAlpha *= 0.50f;

			//draw entity
			float lowerBoundY = m_fY - fRadarSizeOverTwo;
			float dimX = (m_fX + fRadarSizeOverTwo) - lowerBoundX;
			float dimY = (m_fY + fRadarSizeOverTwo) - lowerBoundY;
			numOfValues += ::FillUpDoubleArray(entityValues, pEntity->GetId(), ChooseType(pEntity, true), (fX - lowerBoundX) / dimX, (fY - lowerBoundY) / dimY, 180.0f+RAD2DEG(fAngle), friendly, sizeScale*25.0f, fAlpha*100.0f);
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::UpdateCompassStealth(CActor *pActor, float fDeltaTime)
{
	IEntity *pPlayerEntity = pActor->GetEntity();

	//**********************update flash radar values
	float fStealthValue = 0;
	float fStealthValueStatic = 0;

	if(!gEnv->bMultiplayer)
	{
		SAIDetectionLevels levels;
		gEnv->pAISystem->GetDetectionLevels(0, levels);
		//			float exposure = 0, threat = 0;
		//			gEnv->pAISystem->GetExposureAndThreat(NULL, exposure, threat);

		static int test = 0;
		if (test == 0)
		{
			fStealthValue = max(levels.puppetExposure, levels.puppetThreat) * 100.0f;
			fStealthValueStatic = max(levels.vehicleExposure, levels.vehicleThreat) * 100.0f;
		}
		else if (test == 1)
		{
			fStealthValue = max(levels.vehicleThreat, levels.puppetThreat) * 100.0f;
			fStealthValueStatic = levels.puppetExposure * 100.0f;
		}

	}
	else if(g_pGameCVars->g_enableMPStealthOMeter && m_iMultiplayerEnemyNear)
	{
		fStealthValueStatic = fStealthValue = m_iMultiplayerEnemyNear * 10.0f;
		m_iMultiplayerEnemyNear = 0;
	}

	if(m_fLastStealthValue != fStealthValue)
	{
		m_flashRadar->CheckedInvoke("Root.RadarCompassStealth.StealthBar.gotoAndStop", fStealthValue);
		m_fLastStealthValue = fStealthValue;
	}		
	if(m_fLastStealthValueStatic != fStealthValueStatic && !gEnv->bMultiplayer)
	{
		//m_flashRadar->CheckedInvoke("Root.RadarCompassStealth.StealthExposure.gotoAndStop", fStealthValueStatic);

		g_pGame->GetHUD()->SetStealthExposure(fStealthValueStatic);
		m_fLastStealthValueStatic = fStealthValueStatic;
	}

	if(m_fLastViewRot != 0.0f)
	{
		m_flashRadar->Invoke("setView", 0.0f);
		m_fLastViewRot = 0.0f;
	}
	float fCompass = pActor->GetAngles().z;

	if(m_jammingValue >= g_pGameCVars->hud_radarJammingThreshold) //spin compass
	{
		float compassDir = cosf(gEnv->pTimer->GetAsyncCurTime());
		float originalDirection = (fCompass + gf_PI) - (m_fLastCompassRot + gf_PI);
		if(originalDirection > 0.1f)
			originalDirection = 2.0f;
		else if(originalDirection < -0.1f)
			originalDirection = -2.0f;
		fCompass = m_fLastCompassRot + (compassDir * m_jammingValue * 10.0f * fDeltaTime) /*+ fDeltaTime*((compassDir > 0)?3.0f:(-3.0f))*/ + originalDirection * fDeltaTime;
		if(fCompass > gf_PI)
			fCompass = -gf_PI + (fabsf(fCompass) - fabsf(int(fCompass / gf_PI) * gf_PI));
		else if(fCompass < -gf_PI)
			fCompass = gf_PI - (fabsf(fCompass) - fabsf(int(fCompass / gf_PI) * gf_PI));
	}

#define COMPASS_EPSILON (0.01f)
	if(	m_fLastCompassRot <= fCompass-COMPASS_EPSILON ||
		m_fLastCompassRot >= fCompass+COMPASS_EPSILON)
	{
		char szCompass[HUD_MAX_STRING_SIZE];
		sprintf(szCompass,"%f",fCompass*180.0f/gf_PI-90.0f);
		m_flashRadar->Invoke("setCompassRotation", szCompass);
		m_fLastCompassRot = fCompass;
	}

	float fFov = g_pGameCVars->cl_fov * pActor->GetActorParams()->viewFoVScale;
	if(m_fLastFov != fFov)
	{
		m_flashRadar->Invoke("setFOV", fFov*0.5f);
		m_fLastFov = fFov;
	}

	if(gEnv->bMultiplayer)	//shows the player coordinates in MP
	{
		float fX, fY;
		fX = fY = -1;
		GetPosOnMap(pPlayerEntity, fX, fY);
		char strCoords[32];
		float value = ceil(fX*8.0f);
		int index = min(int(fY / 0.125f), 7);
		sprintf(strCoords, m_coordinateToString[index].c_str(), (int)value);
		if(strcmp(strCoords, m_lastPlayerCoords.c_str()))
		{
			m_flashRadar->Invoke("setSector", strCoords);
			m_lastPlayerCoords = strCoords;
		}
	}
	//************************* end of flash compass
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::UpdateRadarJammer(CActor *pActor)
{
	//update radar jammer**********************************************************
	if(m_jammerID && m_jammerRadius > 0.0f)
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_jammerID);
		if(pEntity)
		{
			Vec3 jamPos = pEntity->GetWorldPos();
			float dist = jamPos.GetDistance(pActor->GetEntity()->GetWorldPos());
			if(dist < m_jammerRadius)
			{
				m_jammingValue = 1.0f - dist/m_jammerRadius;
				m_flashRadar->Invoke("setNoiseValue", m_jammingValue*g_pGameCVars->hud_radarJammingEffectScale);

				if(m_jammingValue >= 0.25f)
				{
					if(!m_jammerDisconnectMap)
					{
						m_jammerDisconnectMap = true;
						m_flashPDA->Invoke("setDisconnect", m_jammerDisconnectMap);
					}
				}
				else if(m_jammerDisconnectMap)
				{
					m_jammerDisconnectMap = false;
					m_flashPDA->Invoke("setDisconnect", m_jammerDisconnectMap);
				}

				if(m_jammingValue >= g_pGameCVars->hud_radarJammingThreshold)
				{
					//update to remove entities
					m_flashRadar->Invoke("setObjectArray");
					if(m_renderMiniMap)
					{
						m_flashPDA->Invoke("Root.PDAArea.Map_M.MapArea.setObjectArray");
					}
					return;
				}
			}
			else if(m_jammingValue)
			{
				m_jammingValue = 0.0f;
				m_flashRadar->Invoke("setNoiseValue", 0.0f);
				if(m_jammerDisconnectMap)
				{
					m_jammerDisconnectMap = false;
					m_flashPDA->Invoke("setDisconnect", m_jammerDisconnectMap);
				}
			}
		}
	}
	else if(m_jammerDisconnectMap)
	{
		m_jammerDisconnectMap = false;
		m_flashPDA->Invoke("setDisconnect", m_jammerDisconnectMap);
	}
}

//-----------------------------------------------------------------------------------------------------

float CHUDRadar::GetRadarSize(IEntity* entity, CActor* actor)
{
	float posEnt = entity->GetWorldPos().z;
	float posPlayer = actor->GetEntity()->GetWorldPos().z;
	float returnValue = 1.0f;

	if((posEnt + 15) < posPlayer)
		returnValue = m_radarDotSizes[0];
	else if((posEnt - 15) > posPlayer)
		returnValue = m_radarDotSizes[2];
	else
		returnValue = m_radarDotSizes[0] + ((posEnt - (posPlayer-15)) / 30.0f) * (m_radarDotSizes[2] - m_radarDotSizes[0]);

	if (entity->GetClass() == m_pHunter || entity->GetClass() == m_pWarrior)
		returnValue *= m_radarDotSizes[3];

	return returnValue;
}

bool CHUDRadar::IsOnRadar(EntityId id, int first, int last)
{
	int size = last - first;
	if(size > 1)
	{
		int mid = first + (size / 2);
		EntityId midId = m_entitiesOnRadar[mid].m_id;
		if(midId == id)
			return true;
		else if(midId > id)
			return IsOnRadar(id, first, mid-1);
		else
			return IsOnRadar(id, mid+1, last);
	}
	else if(size == 1)
		return (m_entitiesOnRadar[first].m_id == id  || m_entitiesOnRadar[last].m_id == id);
	else if(m_entitiesOnRadar.size() > 0)
		return (m_entitiesOnRadar[first].m_id == id);
	return false;
}

void CHUDRadar::AddToRadar(EntityId id)
{
	if(m_entitiesOnRadar.size()>0)
	{
		if(m_entitiesOnRadar[0].m_id > id)
			m_entitiesOnRadar.push_front(RadarEntity(id));
		else if(m_entitiesOnRadar[m_entitiesOnRadar.size()-1].m_id < id)
			m_entitiesOnRadar.push_back(RadarEntity(id));
		else
		{
			std::deque<RadarEntity>::iterator it, itTemp;
			for(it = m_entitiesOnRadar.begin(); it != m_entitiesOnRadar.end(); ++it)
			{
				itTemp = it;
				itTemp++;
				if(itTemp != m_entitiesOnRadar.end())
				{
					if(it->m_id < id && itTemp->m_id > id)
					{
						m_entitiesOnRadar.insert(itTemp, 1, RadarEntity(id));
						break;
					}
				}
				else { assert(false); }
			}
		}
	}
	else
		m_entitiesOnRadar.push_back(RadarEntity(id));
}

bool CHUDRadar::ScanObject(EntityId id)
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (!pEntity)
		return false;

	IPhysicalEntity *pPE=pEntity->GetPhysics();
	if (!pPE)
		return false;

	if (IActor* pActor = m_pActorSystem->GetActor(id))
	{
		if (CActor *pCActor=static_cast<CActor *>(pActor))
		{
			if (pCActor->GetActorClass()==CPlayer::GetActorClassType())
			{
				CPlayer *pPlayer=static_cast<CPlayer *>(pCActor);
				if (CNanoSuit *pNanoSuit=pPlayer->GetNanoSuit())
				{
					if (pNanoSuit->GetMode()==NANOMODE_CLOAK && pNanoSuit->GetCloak()->GetType()==CLOAKMODE_REFRACTION)
						return false;
				}
			}
		}
	}
	
	if(gEnv->bMultiplayer && !CheckObjectMultiplayer(id))
	{
		return false;
	}

	if (CheckObject(pEntity, id!=m_lookAtObjectID, id!=m_lookAtObjectID) &&
		!IsOnRadar(id, 0, m_entitiesOnRadar.size()-1))
	{
		g_pGame->GetHUD()->AutoAimNoText(id);	

		m_scannerObjectID=id;
		m_scannerTimer=g_pGameCVars->hud_binocsScanningDelay;//0.55f;

		return true;
	}

	return false;
}

void CHUDRadar::UpdateScanner(float frameTime)
{
	FUNCTION_PROFILER(GetISystem(),PROFILE_GAME);

	if (m_scannerTimer>0.0f)
	{
		m_scannerTimer-=frameTime;
		if (m_scannerTimer<=0.0f)
		{
			m_scannerTimer=0.0f;

			g_pGame->GetHUD()->AutoAimUnlock(0);

			if (m_scannerObjectID)
			{
				//don't scan items etc
				if(m_pActorSystem->GetActor(m_scannerObjectID) ||
					m_pVehicleSystem->GetVehicle(m_scannerObjectID))
				{
					if(!gEnv->bMultiplayer || CheckObjectMultiplayer(m_scannerObjectID))
					{
						EntityId clientId = g_pGame->GetIGameFramework()->GetClientActor()->GetEntityId();
						g_pGame->GetGameRules()->ClientSimpleHit(SimpleHitInfo(clientId, m_scannerObjectID, 0, 0));
						g_pGame->GetHUD()->PlaySound(ESound_BinocularsLock);
					}
				}
			}

			m_scannerObjectID=0;
		}

		if(m_scannerTimer < 0.3f) //make them blink
		{
			g_pGame->GetHUD()->GetSilhouettes()->SetSilhouette(gEnv->pEntitySystem->GetEntity(m_scannerObjectID), 0.8f, 0.8f, 1.0f, 0.5f, -1.0f);
		}
	}

	if (m_scannerTimer<=0.0f && !m_scannerQueue.empty())
	{
		EntityId scanId=0;
		do
		{
			scanId = m_scannerQueue.front();
			m_scannerQueue.pop_front();

		} while (!ScanObject(scanId) && !m_scannerQueue.empty());
	}

	if (m_scannerGatherTimer>0.0f)
	{
		m_scannerGatherTimer-=frameTime;
		if (m_scannerGatherTimer<=0.0f)
		{
			m_scannerGatherTimer=0.0f;
			//GatherScannableObjects(); // design @GC 2007 : only scan lookat

			m_scannerGatherTimer=0.50f;
		}
	}
}

void CHUDRadar::Reset()
{
	ResetScanner();

	//remove scanned / tac'd entities
	m_entitiesOnRadar.clear();
	ResetTaggedEntities();
	m_tempEntitiesOnRadar.clear();
	m_storyEntitiesOnRadar.clear();
}

void CHUDRadar::ResetScanner()
{
	m_scannerQueue.clear();

	if (m_scannerTimer>0.0f)
	{
		g_pGame->GetHUD()->AutoAimUnlock(0);
		m_scannerTimer=0.0f;
	}
}

bool CHUDRadar::IsObjectInQueue(EntityId id)
{
	if (std::find(m_scannerQueue.begin(), m_scannerQueue.end(), id) != m_scannerQueue.end())
		return true;
	return false;
}

bool CHUDRadar::IsNextObject(EntityId id)
{
	if (m_scannerQueue.empty())
		return false;

	return m_scannerQueue.front() == id;
}


bool CHUDRadar::CheckObject(IEntity *pEntity, bool checkVelocity, bool checkVisibility)
{
	IVehicle *pVehicle = m_pVehicleSystem->GetVehicle(pEntity->GetId());
	if(pVehicle && pVehicle->IsDestroyed())
		return false;

	CCamera &rCamera = GetISystem()->GetViewCamera();
	uint32 uiFlags = pEntity->GetFlags();
	IPhysicalEntity *pPE = pEntity->GetPhysics();
	float fSquaredDistance = pEntity->GetWorldPos().GetSquaredDistance(rCamera.GetPosition());
	bool bPointVisible = rCamera.IsPointVisible(pEntity->GetWorldPos());

	if ((uiFlags&ENTITY_FLAG_ON_RADAR) && pPE && (!checkVisibility || (checkVisibility && fSquaredDistance < 1000000.0f && bPointVisible)))
	{
		pe_status_dynamics dyn;
		pPE->GetStatus(&dyn);

		if (checkVelocity)
		{
			if (dyn.v.len2()<0.5f*0.5f)
				return false;
		}

		if (checkVisibility)
		{
			IActor *pActor=gEnv->pGame->GetIGameFramework()->GetClientActor();
			IPhysicalEntity *pSkipEnt=pActor?pActor->GetEntity()->GetPhysics():0;

			ray_hit hit;
			Vec3 dir=(dyn.centerOfMass-rCamera.GetPosition())*1.15f;

			if (gEnv->pPhysicalWorld->RayWorldIntersection(rCamera.GetPosition(), dir, ent_all, (13&rwi_pierceability_mask), &hit, 1, &pSkipEnt, pSkipEnt?1:0))
			{
				if (!hit.bTerrain && hit.pCollider!=pPE)
				{
					IEntity *pHitEntity = gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider);
					if (pHitEntity != pEntity)
						return false;
				}
			}
		}

		return true;
	}

	return false;
}

Vec2i CHUDRadar::GetMapGridPosition(IEntity *pEntity)
{
	if (pEntity)
	{
		Vec3 pos=pEntity->GetWorldPos();
		return GetMapGridPosition(pos.x, pos.y);
	}

	return Vec2i(-1, -1);
}

Vec2i CHUDRadar::GetMapGridPosition(float x, float y)
{
	Vec2i retVal(-1, -1);

	float outX=0.0f, outY=0.0f;
	
	GetPosOnMap(x, y, outX, outY);

	retVal.y=(int)cry_ceilf(outX*8.0f);
	retVal.x=(int)cry_ceilf(outY*8.0f);

	return retVal;
}

void CHUDRadar::GatherScannableObjects()
{
	CActor *pActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if (!pActor)
		return;

	/*IEntityItPtr pIt=gEnv->pEntitySystem->GetEntityIterator();

	while (!pIt->IsEnd())
	{
		if (IEntity *pEntity=pIt->Next())
		{
			EntityId id=pEntity->GetId();
			if (CheckObject(pEntity, true, false) && !IsOnRadar(id, 0, m_entitiesOnRadar.size()-1) && !IsObjectInQueue(id))
				m_scannerQueue.push_back(id);
		}
	}*/

	int count = m_pActorSystem->GetActorCount();
	if (count > 0)
	{
		IActorIteratorPtr pIter = m_pActorSystem->CreateActorIterator();
		while (IActor* pActor = pIter->Next())
		{
			if(IEntity *pEntity = pActor->GetEntity())
			{
				EntityId id = pEntity->GetId();
				if (CheckObject(pEntity, true, false) && !IsOnRadar(id, 0, m_entitiesOnRadar.size()-1) && !IsObjectInQueue(id))
					m_scannerQueue.push_back(id);
			}
		}
	}

	count = m_pVehicleSystem->GetVehicleCount();
	if (count > 0)
	{
		IVehicleIteratorPtr pIter = m_pVehicleSystem->CreateVehicleIterator();
		while (IVehicle* pVehicle = pIter->Next())
		{
			if(IEntity *pEntity = pVehicle->GetEntity())
			{
				EntityId id = pEntity->GetId();
				if (CheckObject(pEntity, true, false) && !IsOnRadar(id, 0, m_entitiesOnRadar.size()-1) && !IsObjectInQueue(id))
					m_scannerQueue.push_back(id);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------
bool CHUDRadar::GetPosOnMap(float inX, float inY, float &outX, float &outY, bool flashCoordinates)
{
	float posX = std::max(0.0f, inX - std::min((float)m_miniMapStartX[m_mapId], (float)m_miniMapEndX[m_mapId]));
	float posY = std::max(0.0f, inY - std::min((float)m_miniMapStartY[m_mapId], (float)m_miniMapEndY[m_mapId]));
	float temp = posX;
	posX = posY;
	posY = temp;
	outX = std::max((float)m_startOnScreenX, (float)m_startOnScreenX + (std::min(posX * m_mapDivX, 1.0f) * (float)m_mapDimX));
	outY = std::max((float)m_startOnScreenY, (float)m_startOnScreenY + (std::min(posY * m_mapDivY, 1.0f) * (float)m_mapDimY));
	if(flashCoordinates)
	{
		outX = (outX-(float)m_startOnScreenX) / (float)m_mapDimX;
		outY = (outY-(float)m_startOnScreenY) / (float)m_mapDimY;
	}
	return true;
}

//-----------------------------------------------------------------------------------------------------
bool CHUDRadar::GetPosOnMap(IEntity* pEntity, float &outX, float &outY, bool flashCoordinates)
{
	if(!pEntity)
		return false;
	SEntitySlotInfo info;
	Vec3 pos = Vec3(0,0,0);
	int slotCount = pEntity->GetSlotCount();
	for(int i=0; i<slotCount; ++i)
	{
		if (pEntity->GetSlotInfo(i, info))
		{
			if (info.pCharacter)
			{
				int16 id = info.pCharacter->GetISkeletonPose()->GetJointIDByName("objectiveicon");
				if (id >= 0)
				{
					//vPos = pCharacter->GetISkeleton()->GetHelperPos(helper);
					pos = info.pCharacter->GetISkeletonPose()->GetAbsJointByID(id).t;
					if (!pos.IsZero())
					{
						pos = pEntity->GetSlotWorldTM(i).TransformPoint(pos);
						break;
					}
				}
			}
		}
	}
	if(pos.IsZero())
		pos = pEntity->GetWorldPos();

	return GetPosOnMap(pos.x, pos.y, outX, outY, flashCoordinates);
}

//---------------------------------- -------------------------------------------------------------------
void CHUDRadar::ComputeMiniMapResolution()
{
	//size of the displayed 3d area
	const float mapSizeX = fabsf(m_miniMapEndX[m_mapId] - m_miniMapStartX[m_mapId]);
	m_mapDivX = 1.0f / mapSizeX;
	const float mapSizeY = fabsf(m_miniMapEndY[m_mapId] - m_miniMapStartY[m_mapId]);
	m_mapDivY = 1.0f / mapSizeY;
	float width = (float)gEnv->pRenderer->GetWidth();
	float height = (float)gEnv->pRenderer->GetHeight();
	//coordinates of the map in screen pixels in 4:3 at a reference resolution of 800*600
	m_startOnScreenX = 358; m_startOnScreenY = 50; m_endOnScreenX = 756; m_endOnScreenY = 446;
	//adopt x-coordinates to current resolution
	m_startOnScreenX	= (int)(800 - ((800-m_startOnScreenX) / 600.0f * 800 / width * height));
	m_endOnScreenX		= (int)(800 - ((800-m_endOnScreenX) / 600.0f * 800 / width * height));
	//compute screen dimensions
	m_mapDimX = m_endOnScreenX - m_startOnScreenX;
	m_mapDimY = m_endOnScreenY - m_startOnScreenY;
}

//------------------------------------------------------------------------
void CHUDRadar::OnLoadingComplete(ILevel *pLevel)
{
	if(!pLevel)
	{
		if(gEnv->pSystem->IsEditor())
		{
			char *levelName;
			char *levelPath;
			g_pGame->GetIGameFramework()->GetEditorLevel(&levelName, &levelPath);		//this is buggy, returns path for both
			LoadMiniMap(levelPath);
		}
	}
	else
	{
		LoadMiniMap(pLevel->GetLevelInfo()->GetPath()); 
		m_pLevelData = pLevel;
	}

	RenderMapOverlay();
	SetMiniMapTexture(m_mapId, true);
}

//------------------------------------------------------------------------
void CHUDRadar::ReloadMiniMap()
{
	if(m_pLevelData)
		OnLoadingComplete(m_pLevelData);
	SetMiniMapTexture(m_mapId, true);
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::LoadMiniMap(const char* mapPath)
{
	//get the factories (and other buildings)
	if(gEnv->bMultiplayer)
	{
		IEntityClass *factoryClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( "Factory" );
		IEntityClass *hqClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( "HQ" );
		IEntityClass *alienClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( "AlienEnergyPoint" );

		m_buildingsOnRadar.clear();

		IEntityItPtr pIt=gEnv->pEntitySystem->GetEntityIterator();

		if(!gEnv->pGame->GetIGameFramework()->GetClientActor())
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Tried loading a map without having a client.");
			return;
		}

		float minDistance = 0;
		IEntity *pLocalActor = gEnv->pGame->GetIGameFramework()->GetClientActor()->GetEntity();

		EntityId uiOnScreenObjectiveEntityId = 0;
		while (!pIt->IsEnd())
		{
			if (IEntity *pEntity = pIt->Next())
			{
				IEntityClass *cls = pEntity->GetClass();
				if(cls==factoryClass || cls==hqClass || cls==alienClass)
				{
					m_buildingsOnRadar.push_back(RadarEntity(pEntity->GetId()));
				}
				if(cls==factoryClass && pLocalActor && g_pGame->GetHUD()->GetPowerStruggleHUD() && g_pGame->GetHUD()->GetPowerStruggleHUD()->IsFactoryType(pEntity->GetId(), CHUDPowerStruggle::E_PROTOTYPES))
				{
					Vec3 dirvec = (pLocalActor->GetPos()-pEntity->GetPos());
					float distance = dirvec.GetLength2D();
					if(!minDistance || minDistance>distance)
					{
						minDistance = distance;
						uiOnScreenObjectiveEntityId = pEntity->GetId();
					}
				}
			}
		}

		EntityId uiOldOnScreenObjectiveEntityId = SAFE_HUD_FUNC_RET(GetOnScreenObjective());
		if(uiOldOnScreenObjectiveEntityId)
		{
			// If there was already an objective id, it's certainly because we unloaded the map to go into menus
			SAFE_HUD_FUNC(SetOnScreenObjective(uiOldOnScreenObjectiveEntityId));
		}
		else
		{
			// Nothing, pickup the default map objective
			SAFE_HUD_FUNC(SetOnScreenObjective(uiOnScreenObjectiveEntityId));
		}
	}

	//now load the actual map
	string fullPath(mapPath);
	int slashPos = fullPath.rfind('\\');
	if(slashPos == -1)
		slashPos = fullPath.rfind('/');
	string mapName = fullPath.substr(slashPos+1, fullPath.length()-slashPos);
	m_currentLevel = mapName;

	fullPath.append("\\");
	fullPath.append(mapName);
	fullPath.append(".xml");
	XmlNodeRef mapInfo = GetISystem()->LoadXmlFile(fullPath.c_str());
	if(mapInfo == 0)
	{
		GameWarning("Did not find a level meta data file %s in %s.", fullPath.c_str(), mapName.c_str());
		return;
	}

	m_fLastRadarRatio = -1.0f;
	//retrieve the coordinates of the map
	if(mapInfo)
	{
		for(int i = 0; i < NUM_MAP_TEXTURES; ++i)
		{
			m_mapFile[i].clear();
			m_mapRadarRadius[i] = -1;
		}

		for(int n = 0; n < mapInfo->getChildCount(); ++n)
		{
			XmlNodeRef mapNode = mapInfo->getChild(n);
			const char* name = mapNode->getTag();
			if(!stricmp(name, "MiniMap"))
			{
				int attribs = mapNode->getNumAttributes();
				const char* key;
				const char* value;
				int mapNr = 0;
				for(int i = 0; i < attribs; ++i)
				{
					mapNode->getAttributeByIndex(i, &key, &value);
					CryFixedStringT<64> keyString(key);
					int pos = keyString.find(CryFixedStringT<16>("Filename"));
					if(pos != string::npos)
					{
						pos += 8; //add size of "FileName"
						if(pos == keyString.length())
							mapNr = 0;
						else
						{
							CryFixedStringT<64> number = keyString.substr(pos, keyString.length()-pos);
							int n = atoi(number.c_str());
							if(n >= 0 && n < NUM_MAP_TEXTURES)
								mapNr = n;
							else
							{
								CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Couldn't read map index correctly : %s.",keyString.c_str());
								return;
							}
						}

						m_mapFile[mapNr] = mapPath;
						m_mapFile[mapNr].append("\\");
						m_mapFile[mapNr].append(value);

						if(mapNr == 0)
						{
							m_flashPDA->Invoke("setMapBackground", m_mapFile[mapNr].c_str());
							m_flashRadar->Invoke("setMapBackground", m_mapFile[mapNr].c_str());
						}
					}
					else if(keyString.find("startX") != CryFixedStringT<64>::npos)
						m_miniMapStartX[mapNr] = atoi(value);
					else if(keyString.find("startY") != CryFixedStringT<64>::npos)
						m_miniMapStartY[mapNr] = atoi(value);
					else if(keyString.find("endX") != CryFixedStringT<64>::npos)
						m_miniMapEndX[mapNr] = atoi(value);
					else if(keyString.find("endY") != CryFixedStringT<64>::npos)
						m_miniMapEndY[mapNr] = atoi(value);
					else if(keyString.find("mapRadius") != CryFixedStringT<64>::npos)
						m_mapRadarRadius[mapNr] = atoi(value);
				}
				break;
			}
		}
	}

	//compute the dimensions of the miniMap
	ComputeMiniMapResolution();
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::RenderMapOverlay()
{
	CGameFlashAnimation *m_flashMap = m_flashPDA;
	if(!m_flashMap || !m_flashMap->IsAvailable("Root.PDAArea.Map_M.MapArea"))
		return;
	if(m_jammerDisconnectMap)
		return;
	//LoadMiniMap(m_currentLevel);

	m_possibleOnScreenObjectives.resize(0);
	//double array buffer for flash transfer optimization
	std::vector<double> entityValues;
	int numOfValues = 0;
	//array of text strings
	std::map<EntityId, string> textOnMap;

	float fX = 0;
	float fY = 0;

	//draw vehicles only once
	std::map<EntityId, bool> drawnVehicles;
	
	//the current GameRules
	CGameRules *pGameRules = (CGameRules*)(gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules());

	//the local player
	CActor *pActor = static_cast<CActor *>(gEnv->pGame->GetIGameFramework()->GetClientActor());
	if(!pActor ||!pGameRules)
		return;

	EntityId iOnScreenObjective = g_pGame->GetHUD()->GetOnScreenObjective();
	if(iOnScreenObjective)
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(iOnScreenObjective);
		if(pEntity)
		{
			GetPosOnMap(pEntity, fX, fY);
			char strCoords[32];
			float value = ceil(fX*8.0f);

			int index = min(int(fY / 0.125f), 7);
			sprintf(strCoords, m_coordinateToString[index].c_str(), (int)value);
			m_flashMap->CheckedSetVariable("Root.PDAArea.TextBottom.Colorset.ObjectiveText.text",strCoords);
		}
	}
	EntityId iCurrentSpawnPoint = pGameRules->GetPlayerSpawnGroup(pActor);
	if(iCurrentSpawnPoint)
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(iCurrentSpawnPoint);
		if(pEntity)
		{
			GetPosOnMap(pEntity, fX, fY);
			char strCoords[3];
			float value = ceil(fX*8.0f);

			int index = min(int(fY / 0.125f), 7);
			sprintf(strCoords, m_coordinateToString[index].c_str(), (int)value);
			m_flashMap->CheckedSetVariable("Root.PDAArea.TextBottom.Colorset.DeploySpotText.text",strCoords);
		}
	}
	
	bool isMultiplayer = gEnv->bMultiplayer;

	int team = pGameRules->GetTeam(pActor->GetEntityId());

	//draw buildings first
	for(int i = 0; i < m_buildingsOnRadar.size(); ++i)
	{
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(m_buildingsOnRadar[i].m_id);
		if(GetPosOnMap(pEntity, fX, fY))
		{

			int friendly = FriendOrFoe(isMultiplayer, team, pEntity, pGameRules);
			bool addBuilding = true;
			if(friendly==2)
			{
				SmartScriptTable props;
				if (pEntity->GetScriptTable() && pEntity->GetScriptTable()->GetValue("Properties", props))
				{
					int capturable=0;
					if (props->GetValue("bCapturable", addBuilding))
					{
						if(!addBuilding)
						{
							CGameRules *pGameRules = g_pGame->GetGameRules();
							if(pGameRules && pActor)
							{
								if(pGameRules->GetTeam(pActor->GetEntityId()) == pGameRules->GetTeam(pEntity->GetId()))
								{
									addBuilding = true;
								}
							}
						}
					}
				}
			}

			FlashRadarType type = ChooseType(pEntity);
			if((type == EHeadquarter || type == EHeadquarter2) && g_pGame->GetHUD()->HasTACWeapon() && friendly==EEnemy)
			{
				type = EHitZone;
			}
			if(addBuilding==true)
			{
				bool underAttack = false;
				if(g_pGame->GetHUD())
				{
					underAttack = g_pGame->GetHUD()->IsUnderAttack(pEntity) && friendly==EFriend;
				}
				m_possibleOnScreenObjectives.push_back(pEntity->GetId());
				numOfValues += FillUpDoubleArray(&entityValues, pEntity->GetId(), type, fX, fY, 270.0f-RAD2DEG(pEntity->GetWorldAngles().z), friendly, 100, 100, iOnScreenObjective==m_buildingsOnRadar[i].m_id, iCurrentSpawnPoint==m_buildingsOnRadar[i].m_id, underAttack);
			}
		}
	}

	//we need he players position later int the code
	Vec2 vPlayerPos(0.5f,0.5f);

	if(isMultiplayer)
	{
		//special units
		const std::vector<CGameRules::SMinimapEntity> synchEntities = pGameRules->GetMinimapEntities();
		for(int m = 0; m < synchEntities.size(); ++m)
		{
			CGameRules::SMinimapEntity mEntity = synchEntities[m];
			FlashRadarType type = GetSynchedEntityType(mEntity.type);
			IEntity *pEntity = NULL;
			if(type == ENuclearWeapon || type == ETechCharger)	//might be a gun
			{
				if(IItem *pWeapon = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(mEntity.entityId))
				{
					if(pWeapon->GetOwnerId())
						pEntity = gEnv->pEntitySystem->GetEntity(pWeapon->GetOwnerId());
				}
				else
					pEntity = gEnv->pEntitySystem->GetEntity(mEntity.entityId);
		}
				
			if(GetPosOnMap(pEntity, fX, fY))
			{
				int friendly = FriendOrFoe(isMultiplayer, team, pEntity, pGameRules);
				if(friendly == EFriend || friendly == ENeutral)
				{
					if(type == EAmmoTruck && stl::find_in_map(drawnVehicles, mEntity.entityId, false))
					{
						numOfValues += FillUpDoubleArray(&entityValues, pEntity->GetId(), EBarracks, fX, fY, 270.0f-RAD2DEG(pEntity->GetWorldAngles().z), friendly, 100, 100, iOnScreenObjective==mEntity.entityId, iCurrentSpawnPoint==mEntity.entityId);
					}
					else
						numOfValues += FillUpDoubleArray(&entityValues, pEntity->GetId(), type, fX, fY, 270.0f-RAD2DEG(pEntity->GetWorldAngles().z), friendly, 100, 100, iOnScreenObjective==mEntity.entityId, iCurrentSpawnPoint==mEntity.entityId);
				}
			}
		}
	}

	//helpers for actor rendering
	IActor* pTempActor = NULL;
	//draw tagged guys and vehicles...
	{
		for(int i = 0; i < NUM_TAGGED_ENTITIES; ++i)
		{
			EntityId id = m_taggedEntities[i];
			if(id)
			{
				if(pTempActor = m_pActorSystem->GetActor(id))
				{
					if(IVehicle* pVehicle = pTempActor->GetLinkedVehicle())
					{
						if(!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
						{
							GetPosOnMap(pVehicle->GetEntity(), fX, fY);
							numOfValues += FillUpDoubleArray(&entityValues, pVehicle->GetEntity()->GetId(), ETaggedEntity, fX, fY, 270.0f-RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z), ENeutral, 100, 100, iOnScreenObjective==pVehicle->GetEntity()->GetId(), iCurrentSpawnPoint==pVehicle->GetEntity()->GetId());
							drawnVehicles[pVehicle->GetEntityId()] = true;
						}
					}
					else
					{
						GetPosOnMap(pTempActor->GetEntity(), fX, fY);
						numOfValues += FillUpDoubleArray(&entityValues, pActor->GetEntity()->GetId(), ETaggedEntity, fX, fY, 270.0f-RAD2DEG(pTempActor->GetEntity()->GetWorldAngles().z), ENeutral, 100, 100, iOnScreenObjective==pActor->GetEntity()->GetId(), iCurrentSpawnPoint==pActor->GetEntity()->GetId());
					}
				}
				else if(IVehicle *pVehicle = m_pVehicleSystem->GetVehicle(id))
				{
					if(!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
					{
						GetPosOnMap(pVehicle->GetEntity(), fX, fY);
						numOfValues += FillUpDoubleArray(&entityValues, pVehicle->GetEntity()->GetId(), ETaggedEntity, fX, fY, 270.0f-RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z), ENeutral, 100, 100, iOnScreenObjective==pVehicle->GetEntity()->GetId(), iCurrentSpawnPoint==pVehicle->GetEntity()->GetId());
						drawnVehicles[pVehicle->GetEntityId()] = true;
					}
				}
			}
		}
	}

	//draw temporarily tagged units
	{
		for(int e = 0; e < m_tempEntitiesOnRadar.size(); ++e)
		{
			EntityId id = m_tempEntitiesOnRadar[e].m_id;
			if(pTempActor = m_pActorSystem->GetActor(id))
			{
				if(IVehicle* pVehicle = pTempActor->GetLinkedVehicle())
				{
					if(!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
					{
						GetPosOnMap(pVehicle->GetEntity(), fX, fY);
						numOfValues += FillUpDoubleArray(&entityValues, pVehicle->GetEntity()->GetId(), ChooseType(pVehicle->GetEntity(), false), fX, fY,
							270.0f-RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z), FriendOrFoe(isMultiplayer, team, pVehicle->GetEntity(), pGameRules), 100, 100, iOnScreenObjective==pVehicle->GetEntity()->GetId(), iCurrentSpawnPoint==pVehicle->GetEntity()->GetId());
						drawnVehicles[pVehicle->GetEntityId()] = true;
					}
				}
				else
				{
					GetPosOnMap(pTempActor->GetEntity(), fX, fY);
					numOfValues += FillUpDoubleArray(&entityValues, pActor->GetEntity()->GetId(), ChooseType(pTempActor->GetEntity(), false), fX, fY,
						270.0f-RAD2DEG(pTempActor->GetEntity()->GetWorldAngles().z), FriendOrFoe(isMultiplayer, team, pTempActor->GetEntity(), pGameRules), 100, 100, iOnScreenObjective==pTempActor->GetEntity()->GetId(), iCurrentSpawnPoint==pTempActor->GetEntity()->GetId());
				}
			}
			else if(IVehicle *pVehicle = m_pVehicleSystem->GetVehicle(id))
			{
				if(!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
				{
					GetPosOnMap(pVehicle->GetEntity(), fX, fY);
					numOfValues += FillUpDoubleArray(&entityValues, pVehicle->GetEntity()->GetId(), ChooseType(pVehicle->GetEntity(), false), fX, fY,
						270.0f-RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z), FriendOrFoe(isMultiplayer, team, pVehicle->GetEntity(), pGameRules), 100, 100, iOnScreenObjective==pVehicle->GetEntity()->GetId(), iCurrentSpawnPoint==pVehicle->GetEntity()->GetId());
					drawnVehicles[pVehicle->GetEntityId()] = true;
				}
			}
		}
	}

	//draw story entities (icons with text)
	{
		for(int e = 0; e < m_storyEntitiesOnRadar.size(); ++e)
		{
			EntityId id = m_storyEntitiesOnRadar[e].m_id;
			if(IEntity *pEntity = gEnv->pEntitySystem->GetEntity(id))
			{
				GetPosOnMap(pEntity, fX, fY);
				numOfValues += FillUpDoubleArray(&entityValues, id, m_storyEntitiesOnRadar[e].m_type, fX, fY,
					270.0f-RAD2DEG(pEntity->GetWorldAngles().z), ENeutral, 100, 100, iOnScreenObjective==id, iCurrentSpawnPoint==id);
				if(!m_storyEntitiesOnRadar[e].m_text.empty())
					textOnMap[id] = m_storyEntitiesOnRadar[e].m_text;
			}
		}
	}

	//draw position of teammates ...
	{
		std::vector<EntityId>::const_iterator it = m_teamMates.begin();
		std::vector<EntityId>::const_iterator end = m_teamMates.end();
		for(;it != end; ++it)
		{
			pTempActor = m_pActorSystem->GetActor(*it);
			if(pTempActor && pTempActor != pActor)
			{
				if(IVehicle *pVehicle = pTempActor->GetLinkedVehicle())
				{
					if(!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
					{
						GetPosOnMap(pVehicle->GetEntity(), fX, fY);
						numOfValues += FillUpDoubleArray(&entityValues, pVehicle->GetEntity()->GetId(), ChooseType(pVehicle->GetEntity()), fX, fY, 270.0f-RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z), EFriend, 100, 100, iOnScreenObjective==pVehicle->GetEntity()->GetId(), iCurrentSpawnPoint==pVehicle->GetEntity()->GetId());
						drawnVehicles[pVehicle->GetEntityId()] = true;
					}
				}
				else
				{
					bool spectating = false;
					if(isMultiplayer) 
						spectating = (static_cast<CActor*>(pTempActor)->GetSpectatorMode() != CActor::eASM_None);
					if(!spectating)
					{
						GetPosOnMap(pTempActor->GetEntity(), fX, fY);
						numOfValues += FillUpDoubleArray(&entityValues, pTempActor->GetEntity()->GetId(), EPlayer, fX, fY, 270.0f-RAD2DEG(pTempActor->GetEntity()->GetWorldAngles().z), EFriend, 100, 100, iOnScreenObjective==pTempActor->GetEntity()->GetId(), iCurrentSpawnPoint==pTempActor->GetEntity()->GetId());
						//draw teammate name if selected
						if(gEnv->bMultiplayer)
						{
							EntityId id = pTempActor->GetEntityId();
							for(int i = 0; i < m_selectedTeamMates.size(); ++i)
							{
								if(m_selectedTeamMates[i] == id)
								{
									textOnMap[id] = pTempActor->GetEntity()->GetName();
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	//draw radar entities on the map (scanned enemies and vehicles) 
	//scanned vehicles have to be drawn last to find the "neutral" ones correctly
	for(int i = 0; i < m_entitiesOnRadar.size(); ++i)
	{
		EntityId uiEntityId = m_entitiesOnRadar[i].m_id;
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(uiEntityId);
		if(!pEntity || pEntity->IsHidden())
		{
			RemoveFromRadar(uiEntityId);
			continue;
		}

		pTempActor = m_pActorSystem->GetActor(uiEntityId);
		if(pTempActor)
		{
			IVehicle* pVehicle = pTempActor->GetLinkedVehicle();
			if(pVehicle && !pVehicle->IsDestroyed())
			{
				if(!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
				{
					GetPosOnMap(pVehicle->GetEntity(), fX, fY);
					int friendly = FriendOrFoe(isMultiplayer, team, pVehicle->GetEntity(), pGameRules);
					numOfValues += FillUpDoubleArray(&entityValues, pVehicle->GetEntity()->GetId(), ChooseType(pVehicle->GetEntity()), fX, fY, 270.0f-RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z), friendly, 100, 100, iOnScreenObjective==pVehicle->GetEntity()->GetId(), iCurrentSpawnPoint==pVehicle->GetEntity()->GetId());
					drawnVehicles[pVehicle->GetEntityId()] = true;
				}
			}
			else
			{
				int friendly = FriendOrFoe(isMultiplayer, team, pTempActor->GetEntity(), pGameRules);
				GetPosOnMap(pTempActor->GetEntity(), fX, fY);
				numOfValues += FillUpDoubleArray(&entityValues, pTempActor->GetEntity()->GetId(), EPlayer, fX, fY, 270.0f-RAD2DEG(pTempActor->GetEntity()->GetWorldAngles().z), friendly, 100, 100, iOnScreenObjective==pTempActor->GetEntity()->GetId(), iCurrentSpawnPoint==pTempActor->GetEntity()->GetId());
			}
		}
		else if(IVehicle *pVehicle=m_pVehicleSystem->GetVehicle(uiEntityId))
		{
			if(!stl::find_in_map(drawnVehicles, uiEntityId, false))
			{
				if(pVehicle->IsDestroyed())
				{
					RemoveFromRadar(uiEntityId);
					continue;
				}

				int friendly = FriendOrFoe(isMultiplayer, team, pEntity, pGameRules);
				GetPosOnMap(pEntity, fX, fY);
				numOfValues += FillUpDoubleArray(&entityValues, uiEntityId, ChooseType(pEntity), fX, fY, 270.0f-RAD2DEG(pEntity->GetWorldAngles().z), friendly, 100, 100, iOnScreenObjective==uiEntityId, iCurrentSpawnPoint==uiEntityId);
				drawnVehicles[uiEntityId] = true;
			}
		}
	}

	//draw player VEHICLE position
	if(pActor)
	{
		if(IVehicle* pVehicle = pActor->GetLinkedVehicle())
		{
			//if(!stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false))
			//{
				GetPosOnMap(pVehicle->GetEntity(), fX, fY);
				vPlayerPos.x = fX;
				vPlayerPos.y = fY;
				numOfValues += FillUpDoubleArray(&entityValues, pVehicle->GetEntity()->GetId(), ChooseType(pVehicle->GetEntity()), fX, fY, 270.0f-RAD2DEG(pVehicle->GetEntity()->GetWorldAngles().z), ESelf, 100, 100, iOnScreenObjective==pVehicle->GetEntity()->GetId(), iCurrentSpawnPoint==pVehicle->GetEntity()->GetId());
				drawnVehicles[pVehicle->GetEntityId()] = true;
			//}
		}
	}

	if(isMultiplayer)
	{
		//now spawn points
		std::vector<EntityId> locations;
		pGameRules->GetSpawnGroups(locations);
		for(int i = 0; i < locations.size(); ++i)
		{
			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(locations[i]);
			if(!pEntity)
				continue;
			if (pEntity)
			{
				IVehicle *pVehicle = m_pVehicleSystem->GetVehicle(pEntity->GetId());
				bool isVehicle = (pVehicle)?true:false;
				if(GetPosOnMap(pEntity, fX, fY))
				{
					int friendly = FriendOrFoe(isMultiplayer, team, pEntity, pGameRules);
					if(isVehicle /*&& !stl::find_in_map(drawnVehicles, pVehicle->GetEntityId(), false)*/)
					{
						if(friendly == EFriend)
						{
							numOfValues += FillUpDoubleArray(&entityValues, pEntity->GetId(), ESpawnTruck, fX, fY, 270.0f-RAD2DEG(pEntity->GetWorldAngles().z), friendly, 100, 100, iOnScreenObjective==locations[i], iCurrentSpawnPoint==locations[i]);
							drawnVehicles[pVehicle->GetEntityId()] = true;
						}
					}
					else
					{
						if(friendly!=2)
						{
							numOfValues += FillUpDoubleArray(&entityValues, pEntity->GetId(), ESpawnPoint, fX, fY, 270.0f-RAD2DEG(pEntity->GetWorldAngles().z), friendly, 100, 100, iOnScreenObjective==locations[i], iCurrentSpawnPoint==locations[i]);
							m_possibleOnScreenObjectives.push_back(pEntity->GetId());
						}
						else
						{
							SmartScriptTable props;
							if (pEntity->GetScriptTable() && pEntity->GetScriptTable()->GetValue("Properties", props))
							{
								int capturable=0;
								if (props->GetValue("bCapturable", capturable) && capturable)
								{
									numOfValues += FillUpDoubleArray(&entityValues, pEntity->GetId(), ESpawnPoint, fX, fY, 270.0f-RAD2DEG(pEntity->GetWorldAngles().z), friendly, 100, 100, iOnScreenObjective==locations[i], iCurrentSpawnPoint==locations[i]);
									m_possibleOnScreenObjectives.push_back(pEntity->GetId());
								}
							}
						}
					}
				}
			}
		}
	}

	//draw player position
	if(pActor)
	{
		if(!pActor->GetLinkedVehicle())
		{
			GetPosOnMap(pActor->GetEntity(), fX, fY);
			vPlayerPos.x = fX;
			vPlayerPos.y = fY;
			string name(pActor->GetEntity()->GetName());
			numOfValues += FillUpDoubleArray(&entityValues, pActor->GetEntity()->GetId(), (name.find("Quarantine",0)!=string::npos)?ENuclearWeapon:EPlayer, fX, fY, 270.0f-RAD2DEG(pActor->GetEntity()->GetWorldAngles().z), ESelf, 100, 100, iOnScreenObjective==pActor->GetEntity()->GetId(), iCurrentSpawnPoint==pActor->GetEntity()->GetId());
		}
	}

	//.. and mission objectives
	if(!gEnv->bMultiplayer)
	{
		std::map<EntityId, RadarObjective>::const_iterator it = m_missionObjectives.begin();
		std::map<EntityId, RadarObjective>::const_iterator end = m_missionObjectives.end();

		for(; it != end; ++it)
		{
			IEntity *pEntity = gEnv->pEntitySystem->GetEntity(it->first);
			if(GetPosOnMap(pEntity, fX, fY))
			{
				numOfValues += FillUpDoubleArray(&entityValues, pEntity->GetId(), (it->second.secondaryObjective)?ESecondaryObjective:EWayPoint, fX, fY, 180.0f, ENeutral, 100, 100, iOnScreenObjective==it->first, iCurrentSpawnPoint==it->first);
				textOnMap[pEntity->GetId()] = it->second.text;
			}
		}
	}


	ComputePositioning(vPlayerPos, &entityValues);

	//tell flash file that we are done ...
	//m_flashMap->Invoke("updateObjects", "");
	if(entityValues.size())
		m_flashMap->GetFlashPlayer()->SetVariableArray(FVAT_Double, "Root.PDAArea.Map_M.MapArea.m_allValues", 0, &entityValues[0], numOfValues);
	m_flashMap->Invoke("Root.PDAArea.Map_M.MapArea.setObjectArray");
	//render text strings
	std::map<EntityId, string>::const_iterator itText = textOnMap.begin();
	for(; itText != textOnMap.end(); ++itText)
	{
		SFlashVarValue args[2] = {itText->first, itText->second.c_str()};
		m_flashMap->Invoke("Root.PDAArea.Map_M.MapArea.setText", args, 2);
	}
}

//-----------------------------------------------------------------------------------------------------

//calculate correct map positions, while the map is zoomed or translated
void CHUDRadar::ComputePositioning(Vec2 playerpos, std::vector<double> *doubleArray)
{

	bool bUpdate = false;
	//update zooming
	if(m_fPDATempZoomFactor!=m_fPDAZoomFactor)
	{
		float fRatio = m_fPDATempZoomFactor-m_fPDAZoomFactor;
		if(cry_fabsf(fRatio) > 0.01)
		{
			m_fPDAZoomFactor += fRatio*1.0;
		}
		else
		{
			m_fPDAZoomFactor = m_fPDATempZoomFactor;
		}
		bUpdate = true;
	}

	float fMapSize = 508.0f * m_fPDAZoomFactor;

	//update dragging
	float fLimit = (1.0f / fMapSize);

	if(m_vPDAMapTranslation.x != m_vPDATempMapTranslation.x)
	{
		float fDiffX = m_vPDATempMapTranslation.x-m_vPDAMapTranslation.x;
		if(cry_fabsf(fDiffX) > fLimit)
		{
			m_vPDAMapTranslation.x += fDiffX * 1.0f;
		}
		else
		{
			m_vPDAMapTranslation.x = m_vPDATempMapTranslation.x;
		}
		bUpdate = true;
	}
	if(m_vPDAMapTranslation.y != m_vPDATempMapTranslation.y)
	{
		float fDiffY = m_vPDATempMapTranslation.y-m_vPDAMapTranslation.y;
		if(cry_fabsf(fDiffY) > fLimit)
		{
			m_vPDAMapTranslation.y += fDiffY * 1.0;
		}
		else
		{
			m_vPDAMapTranslation.y = m_vPDATempMapTranslation.y;
		}
		bUpdate= true;
	}

	//calculate offset
	Vec2 vOffset = Vec2(0,0);
	Vec2 vMapPos = Vec2((254.0f - playerpos.x * fMapSize) + m_vPDAMapTranslation.x, (254.0f + (1.0f - playerpos.y) * fMapSize) + m_vPDAMapTranslation.y);

	if(vMapPos.x>0.0f)
	{
		vOffset.x = -vMapPos.x;
	}
	else if(vMapPos.x<(508.0f-fMapSize))
	{
		vOffset.x = (508.0f-fMapSize) - vMapPos.x;
	}

	if(vMapPos.y<508)
	{
		vOffset.y = 508 - vMapPos.y;
	}
	else if(vMapPos.y>fMapSize)
	{
		vOffset.y = fMapSize - vMapPos.y;
	}
	//if(bUpdate || m_initMap)
	{
		m_initMap = false;

		m_flashPDA->SetVariable("Root.PDAArea.Map_M.MapArea.Map.Map_G._xscale", SFlashVarValue(100.0f * m_fPDAZoomFactor));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.MapArea.Map.Map_G._yscale", SFlashVarValue(100.0f * m_fPDAZoomFactor));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.MapArea.Map.Map_G._x", SFlashVarValue(vMapPos.x + vOffset.x));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.MapArea.Map.Map_G._y", SFlashVarValue(vMapPos.y + vOffset.y));

		float fStep = 63.5f * m_fPDAZoomFactor;
		float fOffsetX = (fStep * 0.5f)-70.0f;
		float fOffsetY = (fStep * 0.5f)-18.0f;

		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorA._y", SFlashVarValue((vMapPos.y + vOffset.y)- fStep*8.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorB._y", SFlashVarValue((vMapPos.y + vOffset.y)- fStep*7.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorC._y", SFlashVarValue((vMapPos.y + vOffset.y)- fStep*6.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorD._y", SFlashVarValue((vMapPos.y + vOffset.y)- fStep*5.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorE._y", SFlashVarValue((vMapPos.y + vOffset.y)- fStep*4.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorF._y", SFlashVarValue((vMapPos.y + vOffset.y)- fStep*3.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorG._y", SFlashVarValue((vMapPos.y + vOffset.y)- fStep*2.0f + fOffsetY));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.SectorH._y", SFlashVarValue((vMapPos.y + vOffset.y)- fStep*1.0f + fOffsetY));

		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector1._x", SFlashVarValue((vMapPos.x + vOffset.x)+ fStep*1.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector2._x", SFlashVarValue((vMapPos.x + vOffset.x)+ fStep*2.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector3._x", SFlashVarValue((vMapPos.x + vOffset.x)+ fStep*3.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector4._x", SFlashVarValue((vMapPos.x + vOffset.x)+ fStep*4.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector5._x", SFlashVarValue((vMapPos.x + vOffset.x)+ fStep*5.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector6._x", SFlashVarValue((vMapPos.x + vOffset.x)+ fStep*6.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector7._x", SFlashVarValue((vMapPos.x + vOffset.x)+ fStep*7.0f - fOffsetX));
		m_flashPDA->SetVariable("Root.PDAArea.Map_M.Sector8._x", SFlashVarValue((vMapPos.x + vOffset.x)+ fStep*8.0f - fOffsetX));
		
		float value = 0;
		value = (vMapPos.x + vOffset.x)+ fStep*4.0f - fOffsetX;
		value = (vMapPos.x + vOffset.x)+ fStep*4.0f - fOffsetX;
	}

	//calculate icon positions
	for(int i(0); i < doubleArray->size(); i+=11)
	{
		if(doubleArray->size() > i+6)
		{
			(*doubleArray)[i+2] = 254.0f - ((playerpos.x - (*doubleArray)[i+2]) *fMapSize ) + m_vPDAMapTranslation.x + vOffset.x;
			(*doubleArray)[i+3] = 254.0f - ((playerpos.y - (*doubleArray)[i+3]) *fMapSize ) + m_vPDAMapTranslation.y + vOffset.y;
			(*doubleArray)[i+6] = (*doubleArray)[i+6] * (m_fPDAZoomFactor+2.0f) * 0.25f;
		}
	}
	if(m_bDragMap)
		m_vPDATempMapTranslation += vOffset;
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::InitMap()
{
	m_initMap = true;
	m_vPDATempMapTranslation = Vec2(0,0);
}

bool CHUDRadar::ZoomPDA(bool bZoomDirection)
{
	float fNextStep = 0.25f - (0.25f / m_fPDATempZoomFactor);

	if(bZoomDirection)
		m_fPDATempZoomFactor *= (1.1f + fNextStep);
	else
		m_fPDATempZoomFactor /= (1.1f + fNextStep);

	m_fPDATempZoomFactor = clamp_tpl(m_fPDATempZoomFactor, 1.0f, 8.0f);
	return true;
}

bool CHUDRadar::ZoomChangePDA(float p_fValue)
{
	m_fPDATempZoomFactor-=(p_fValue*0.03);
	m_fPDATempZoomFactor = clamp_tpl(m_fPDATempZoomFactor, 1.0f, 8.0f);
	return true;
}

bool CHUDRadar::DragMap(Vec2 p_vDir)
{
	m_vPDATempMapTranslation += p_vDir * m_fPDAZoomFactor;
	return true;
}

void CHUDRadar::SetDrag(bool enabled)
{
	m_bDragMap = enabled;
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::Serialize(TSerialize ser)
{
	if(ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("HUDRadar");
		int amount = 0;
		amount = m_entitiesOnRadar.size();
		ser.Value("AmountOfRadarEntities", amount);
		if(ser.IsReading())
		{
			m_entitiesOnRadar.clear();
			m_entitiesOnRadar.resize(amount);
		}
		for(int h = 0; h < amount; ++h)
		{
			ser.BeginGroup("RadarEntity");
			ser.Value("id", m_entitiesOnRadar[h].m_id);
			ser.EndGroup();
		}

		amount = m_storyEntitiesOnRadar.size();
		ser.Value("AmountOfStoryEntities", amount);
		if(ser.IsReading())
		{
			m_storyEntitiesOnRadar.clear();
			m_storyEntitiesOnRadar.resize(amount);
		}
		for(int s = 0; s < amount; ++s)
		{
			ser.BeginGroup("StoryEntity");
			ser.Value("id", m_storyEntitiesOnRadar[s].m_id);
			ser.EnumValue("type", m_storyEntitiesOnRadar[s].m_type, EFirstType, ELastType);
			ser.Value("spawnTime", m_storyEntitiesOnRadar[s].m_spawnTime);
			ser.Value("timeLimit", m_storyEntitiesOnRadar[s].m_timeLimit);
			ser.Value("text", m_storyEntitiesOnRadar[s].m_text);
			ser.EndGroup();
		}

		amount = m_teamMates.size();
		ser.Value("teamMatesAmount", amount);
		if(amount)
		{
			if(ser.IsReading())
			{
				m_teamMates.clear();
				m_teamMates.resize(amount);
			}

			for(int i = 0; i < amount; ++i)
			{
				ser.BeginGroup("teamMateId");
				ser.Value("mateID", m_teamMates[i]);
				ser.EndGroup();
			}
		}

		ser.Value("PDAZoomFactor", m_fPDAZoomFactor);

		for(int h = 0; h < NUM_TAGGED_ENTITIES; ++h)
		{
			ser.BeginGroup("TaggedEntity");
			ser.Value("id", m_taggedEntities[h]);
			ser.EndGroup();
		}

		ser.Value("jammerID", m_jammerID);
		ser.Value("jammerRadius", m_jammerRadius);
		ser.Value("jammerValue", m_jammingValue);
		ser.Value("m_jammerDisconnectMap", m_jammerDisconnectMap);

		ser.Value("miniMapId", m_mapId);

		amount = m_missionObjectives.size();
		ser.Value("AmountOfMissionObjectives", amount);
		if(ser.IsReading())
		{
			if(gEnv->pSystem->IsSerializingFile() == 1) //only when quickloading
				SetMiniMapTexture(m_mapId);
			m_flashRadar->Invoke("setNoiseValue", m_jammingValue);
			m_flashPDA->Invoke("setDisconnect", m_jammerDisconnectMap);

			m_entitiesInProximity.clear();
			m_itemsInProximity.clear();

			m_missionObjectives.clear();
			EntityId id = 0;
			string text;
			for(int i = 0; i < amount; ++i)
			{
				ser.BeginGroup("MissionObjective");
				ser.Value("Active", id);
				RadarObjective objective;
				ser.Value("Text", objective.text);
				ser.Value("Secondary", objective.secondaryObjective);
				ser.EndGroup();
				m_missionObjectives[id] = objective;
			}

			//reset update timer
			m_lastScan = 0.0f;
			m_startBroadScanTime = 0.0f;
			m_fLastRadarRatio = -1.0f;
		}
		else //writing
		{
			std::map<EntityId, RadarObjective>::iterator it = m_missionObjectives.begin();
			for(int i = 0; i < amount; ++i, ++it)
			{
				EntityId id = it->first;
				string text = it->second.text;
				ser.BeginGroup("MissionObjective");
				ser.Value("Active", id);
				ser.Value("Text", text);
				ser.Value("Secondary", it->second.secondaryObjective);
				ser.EndGroup();
			}
		}

		ser.EndGroup(); //HUDRadar;
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDRadar::UpdateMissionObjective(EntityId id, bool active, const char* description, bool secondary)
{
	if (id == 0)
		return;

	std::map<EntityId, RadarObjective>::iterator iter = m_missionObjectives.find(id);
	if (iter != m_missionObjectives.end() && !active)
		m_missionObjectives.erase(iter);	
	else if (active)
		m_missionObjectives.insert(std::map<EntityId, RadarObjective>::value_type (id, RadarObjective(description,secondary)));
}

//-----------------------------------------------------------------------------------------------------

const char* CHUDRadar::GetObjectiveDescription(EntityId id)
{
	std::map<EntityId, RadarObjective>::iterator iter = m_missionObjectives.find(id);
	if (iter != m_missionObjectives.end())
	{
		return iter->second.text.c_str();
	}
	return "";
}

//-----------------------------------------------------------------------------------------------------

FlashRadarType CHUDRadar::ChooseType(IEntity* pEntity, bool radarOnly)
{
	if(!pEntity)
		return EFirstType;
	const IEntityClass *pCls = pEntity->GetClass();
	const char* cls = pCls->GetName();
	const char* name = pEntity->GetName();

	FlashRadarType returnType = ELTV;

	if(pCls == m_pPlayerClass || pCls == m_pGrunt)
		returnType = EPlayer;
	else if(pCls == m_pAlien || pCls == m_pTrooper)
		returnType = EPlayer;
	else if(pCls == m_pLTVUS || pCls == m_pLTVA)
		returnType = ELTV;
	else if(pCls == m_pTankUS || pCls == m_pTankA)
		returnType = ETank;
	else if(pCls == m_pAAA)
		returnType = EAAA;
	else if(pCls == m_pTruck)
		returnType = ETruck;
	else if(pCls == m_pTruck || pCls == m_pAPCUS || pCls == m_pAPCA)
		returnType = EAPC;
	else if(pCls == m_pHeli)
		returnType = EHeli;
	else if(pCls == m_pVTOL)
		returnType = EVTOL;
	else if(pCls == m_pScout)
		returnType = EHeli;
	else if(pCls == m_pWarrior || pCls == m_pHunter)
		returnType = EINVALID1;
	else if(pCls == m_pBoatCiv)
		returnType = ESmallBoat;
	else if(pCls == m_pHover)
		returnType = EHovercraft;
	else if(pCls == m_pBoatA)
		returnType = EPatrolBoat;
	else if(pCls == m_pBoatUS)
		returnType = ESpeedBoat;
	else if(!stricmp(cls, "HQ"))
	{
		CGameRules *pGameRules = static_cast<CGameRules*>(gEnv->pGame->GetIGameFramework()->GetIGameRulesSystem()->GetCurrentGameRules());
		if(pGameRules->GetTeam(pEntity->GetId()) == 2) //us team
			returnType = EHeadquarter2;
		else
			returnType = EHeadquarter;
	}
	else if(!stricmp(cls, "Factory") && g_pGame->GetHUD()->GetPowerStruggleHUD())	//this should be much bigger and choose out of the different factory versions (not yet existing)
	{
		if(g_pGame->GetHUD()->GetPowerStruggleHUD()->CanBuild(pEntity,"ustank"))
		{
			returnType = EFactoryTank;
		}
		else if(g_pGame->GetHUD()->GetPowerStruggleHUD()->CanBuild(pEntity,"nkhelicopter"))
		{
			returnType = EFactoryAir;
		}
		else if(g_pGame->GetHUD()->GetPowerStruggleHUD()->CanBuild(pEntity,"nkboat"))
		{
			returnType = EFactorySea;
		}
		else if(g_pGame->GetHUD()->GetPowerStruggleHUD()->CanBuild(pEntity,"us4wd"))
		{
			returnType = EFactoryVehicle;
		}
		else
		{
			returnType = EFactoryPrototype;
		}
	}
	else if(!stricmp(cls,"AlienEnergyPoint"))
	{
		returnType = EAlienEnergySource;
	}
	if(radarOnly)
	{
		if(returnType == EPlayer)
			returnType = ETank; //1
		else if(returnType == EHeli)
			returnType = EAPC; //2
		else if(returnType == EINVALID1) //currently big aliens like hunter
			returnType = ETank;
		else
			returnType = ECivilCar; //3
	}
		
	return returnType;
}

//-----------------------------------------------------------------------------------------------------

FlashRadarFaction CHUDRadar::FriendOrFoe(bool multiplayer, int playerTeam, IEntity *entity, CGameRules *pGameRules)
{
	FlashRadarFaction val = ENeutral;

	if(multiplayer)
	{
		int friendly = pGameRules->GetTeam(entity->GetId());
		if(friendly != ENeutral)
		{
			if(friendly == playerTeam)
				friendly = EFriend;
			else
				friendly = EEnemy;
		}
		val = (FlashRadarFaction)friendly;
	}
	else if(entity->GetAI())
	{
		IAIObject *pAI = entity->GetAI();
		if(pAI && pAI->GetAIType() == AIOBJECT_VEHICLE)
		{
			if(IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(entity->GetId()))
			{	
				if (IVehicleSeat* pSeat = pVehicle->GetSeatById(1))
					if (pSeat->IsDriver())
					{
						EntityId driverId = pSeat->GetPassenger();
						IEntity *temp = gEnv->pEntitySystem->GetEntity(driverId);
						if(temp && temp->GetAI())
							pAI = temp->GetAI();
					}
			}
		}
		if(pAI && pAI->GetAIType() != AIOBJECT_VEHICLE) //also checking driver from former vehicle
		{
			IEntity *playerEntity = g_pGame->GetIGameFramework()->GetClientActor()->GetEntity();
			if(playerEntity && pAI->IsHostile(playerEntity->GetAI(),false))
				val = EEnemy;
			else
				val = EFriend;
		}
	}

	return val;
}

FlashRadarType CHUDRadar::GetSynchedEntityType(int type)
{
	switch(type)
	{
	case 1:
		return EAmmoTruck;
		break;
	case 2:
		return ENuclearWeapon;	//this is a tac entity
		break;
	case 3:
		return ETechCharger;
		break;
	default:
		break;
	}
	return EFirstType;
}

int CHUDRadar::FillUpDoubleArray(std::vector<double> *doubleArray, double a, double b, double c, double d, double e, double f, double g, double h, double i, double j, double k)
{
	doubleArray->push_back(a);
	doubleArray->push_back(b);
	doubleArray->push_back(c);
	doubleArray->push_back(d);
	doubleArray->push_back(e);
	doubleArray->push_back(f);
	doubleArray->push_back(g);
	doubleArray->push_back(h);
	doubleArray->push_back(i);
	doubleArray->push_back(j);
	doubleArray->push_back(k);

	return 11;
}

void CHUDRadar::StartBroadScan(bool useParameters, bool keepEntities, Vec3 pos, float radius)
{
	IActor *pClient = g_pGame->GetIGameFramework()->GetClientActor();
	if(!pClient)
		return;

	if(!m_startBroadScanTime) //wait (1.5) seconds before actually scanning - delete timer after scan 
	{
		m_startBroadScanTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		//set scanning parameter
		m_bsUseParameter = useParameters;
		m_bsKeepEntries = keepEntities;
		m_bsPosition = pos;
		m_bsRadius = radius;

		return;
	}
	else if(m_bsUseParameter)	//this is a custom scan
	{
		ScanProximity(m_bsPosition, m_bsRadius);
		if(m_bsKeepEntries)
			g_pGame->GetHUD()->ShowDownloadSequence();
	}
	else	//this is a quick proximity scan
		m_bsKeepEntries = false;

	int playerTeam = g_pGame->GetGameRules()->GetTeam(pClient->GetEntityId());
	for(int e = 0; e < m_entitiesInProximity.size(); ++e)
	{
		EntityId id = m_entitiesInProximity[e];
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(id);
		if(IsOnRadar(id, 0, m_entitiesOnRadar.size()-1) || IsEntityTagged(id))
			continue;
		if(stl::find(m_teamMates, id))
			continue;

		g_pGame->GetGameRules()->AddTaggedEntity(pClient->GetEntityId(), id, m_bsKeepEntries?false:true);
	}
	m_startBroadScanTime = 0.0f;
	m_bsKeepEntries = false;
}

void CHUDRadar::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->AddContainer(m_scannerQueue);
	s->AddContainer(m_teamMates);
	s->AddContainer(m_entitiesInProximity);
	s->AddContainer(m_itemsInProximity);
	s->AddContainer(m_tempEntitiesOnRadar);
	s->AddContainer(m_storyEntitiesOnRadar);
	s->AddContainer(m_entitiesOnRadar);
	s->AddContainer(m_soundsOnRadar);
	s->AddContainer(m_buildingsOnRadar);
	s->AddContainer(m_missionObjectives);
	for (std::map<EntityId, RadarObjective>::iterator iter = m_missionObjectives.begin(); iter != m_missionObjectives.end(); ++iter)
		s->Add(iter->second.text);
}

void CHUDRadar::SetTeamMate(EntityId id, bool active)
{
	bool found = false;

	std::vector<EntityId>::iterator it = m_teamMates.begin();
	for(; it != m_teamMates.end(); ++it)
	{
		if(*it == id)
		{
			if(!active)
			{
				m_teamMates.erase(it);
				return;
			}

			found = true;
			break;
		}
	}

	if(!found && active)
		if(IActor *pActor = m_pActorSystem->GetActor(id))
			m_teamMates.push_back(id);
}

void CHUDRadar::SelectTeamMate(EntityId id, bool active)
{
	bool found = false;
	std::vector<EntityId>::iterator it = m_selectedTeamMates.begin();
	for(; it != m_selectedTeamMates.end(); ++it)
	{
		if(*it == id)
		{
			if(!active)
			{
				m_selectedTeamMates.erase(it);
				return;
			}
			else
			{
				found = true;
				break;
			}
		}
	}

	if(!found)
		if(IActor *pActor = m_pActorSystem->GetActor(id))
			m_selectedTeamMates.push_back(id);
}

void CHUDRadar::ScanProximity(Vec3 &pos, float &radius)
{
	m_entitiesInProximity.clear();
	m_itemsInProximity.clear();

	SEntityProximityQuery query;
	query.box = AABB( Vec3(pos.x-radius,pos.y-(radius*1.33f),pos.z-radius),
		Vec3(pos.x+radius,pos.y+(radius*1.33f),pos.z+radius) );
	query.nEntityFlags = ENTITY_FLAG_ON_RADAR; // Filter by entity flag.
	gEnv->pEntitySystem->QueryProximity( query );

	IEntity *pActorEntity = g_pGame->GetIGameFramework()->GetClientActor()->GetEntity();

	for(int iEntity=0; iEntity<query.nCount; iEntity++)
	{
		IEntity *pEntity = query.pEntities[iEntity];
		if(pEntity && !pEntity->IsHidden() && pEntity != pActorEntity)
		{
			EntityId id = pEntity->GetId();
			if(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(id))
				m_itemsInProximity.push_back(id);
			else
				m_entitiesInProximity.push_back(id);	//create a list of all nearby entities
		}
	}
}

EntityId CHUDRadar::RayCastBinoculars(CPlayer *pPlayer,ray_hit *pRayHit)
{
	if(!pPlayer)
		return 0;

	Vec3 pos, dir, up, right;

	const Matrix34 &rmatCamera = gEnv->pSystem->GetViewCamera().GetMatrix();
	pos = rmatCamera.GetTranslation();
	dir = rmatCamera.GetColumn1();
	up	= rmatCamera.GetColumn2();
	right = (dir % up).GetNormalizedSafe();

	IPhysicalEntity * pPhysEnt = pPlayer->GetEntity()->GetPhysics();

	if(!pPhysEnt)
		return 0;

	static const int obj_types = ent_all; // ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid|ent_living;
	static const unsigned int flags = rwi_pierceability(1);

	right = right * g_pGameCVars->hud_binocsScanningWidth;
	up = up * g_pGameCVars->hud_binocsScanningWidth;

	Vec3 positions[4];
	positions[0] = pos + right + up;
	positions[1] = pos + right - up;
	positions[2] = pos - right - up;
	positions[3] = pos - right + up;
	for(int i = 0; i < 4; ++i)
	{
		memset(pRayHit,0,sizeof(ray_hit));
		if(gEnv->pPhysicalWorld->RayWorldIntersection( positions[i], 300.0f * dir, obj_types, (13&rwi_pierceability_mask), pRayHit, 1, &pPhysEnt, 1 ))
		{
			IEntity *pLookAt=gEnv->pEntitySystem->GetEntityFromPhysics(pRayHit->pCollider);
			
			if (pLookAt)
				return pLookAt->GetId();
		}
	}
	return 0;
}

bool CHUDRadar::IsEntityTagged(const EntityId &id) const
{
	for(int i = 0; i < NUM_TAGGED_ENTITIES; ++i)
		if(m_taggedEntities[i] == id)
			return true;
	return false;
}

void CHUDRadar::SetJammer(EntityId id, float radius)
{
	m_jammerID = id;
	m_jammerRadius = radius;

	if(!id && m_jammingValue)
	{
		m_jammingValue = 0.0f;
		m_flashRadar->Invoke("setNoiseValue", 0.0f);
	}
}

void CHUDRadar::SetMiniMapTexture(int mapId, bool forceUpdate)
{
	if(mapId < 0 || mapId >= NUM_MAP_TEXTURES)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed loading map texture id %i. (out of id space, current maximum are %i)", mapId, NUM_MAP_TEXTURES);
		return;
	}

	string *mapFile = &(m_mapFile[mapId]);

	if(mapFile && mapFile->size() > 0)
	{
		if(mapId != m_mapId || forceUpdate)
		{
			m_mapId = mapId;
			ComputeMiniMapResolution();

			m_flashPDA->Invoke("setMapBackground", mapFile->c_str());
			m_flashRadar->Invoke("setMapBackground", mapFile->c_str());
		}

	}
	else
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed loading map texture id %i.", mapId);
}

bool CHUDRadar::RadarBounds_Inside(const Vec2 &pos, Vec2 &intersectionPoint)
{
	Vec2 center(m_fX, m_fY);
	static const Vec2 radarPolygon[] =
	{
		Vec2(center + Vec2(-58, 17.0f)),
		Vec2(center + Vec2(-58, -42)),
		Vec2(center + Vec2(62, -42)),
		Vec2(center + Vec2(62, 32.15f)),
		Vec2(center + Vec2(46.25f, 45)),
		Vec2(center + Vec2(-46.75f, 45)),
		Vec2(center + Vec2(-46.75f, 25.8f))
	};

	int a, b;
	for(a = 0, b = 1; a < 7; ++a, ++b)
	{
		if(b == 7)
			b = 0;

		bool intersection = RadarBounds_IntersectsLineFromOutside(radarPolygon[a], radarPolygon[b], pos, center, intersectionPoint);
		if(intersection)
		{
			return false;
		}
	}
	return true;
}

bool CHUDRadar::RadarBounds_IntersectsLineFromOutside(const Vec2 &A, const Vec2 &B, const Vec2 &P, const Vec2 &M, Vec2 &resultingIntersection)
{
	Vec2 line = B-A; //k
	Vec2 lineNormal = Vec2(-line.y, line.x);
	Vec2 faceLine = A-P;

	bool intersects = false;

	float dot = lineNormal.Dot(faceLine);
	if(dot > 0) //it's possibly crossing, try to compute the intersection
	{
		Vec2 crossingLine = (P-M); //t

		if(line.x == 0)
		{
			float i2 = crossingLine.y / crossingLine.x;
			float r2 = M.y - ((M.x * crossingLine.y) / crossingLine.x); //p
			resultingIntersection.x = A.x;
			resultingIntersection.y = i2 * resultingIntersection.x + r2;
		}
		else if(crossingLine.x == 0)
		{
			float i1 = line.y / line.x;
			float r1 = A.y - ((A.x * line.y) / line.x); //b
			resultingIntersection.x = M.x;
			resultingIntersection.y = i1 * resultingIntersection.x + r1;
		}
		else
		{
			float i1 = line.y / line.x;	//gradients m
			float i2 = crossingLine.y / crossingLine.x; //n
			float r1 = A.y - ((A.x * line.y) / line.x); //b
			float r2 = M.y - ((M.x * crossingLine.y) / crossingLine.x); //p
			resultingIntersection.x = (r1-r2) / (i2-i1);
			resultingIntersection.y = i1 * resultingIntersection.x + r1;
		}

		float onLine = -1;
		if(line.y != 0)
			onLine = (resultingIntersection.y - A.y) / line.y;
		else if(line.x != 0)
			onLine = (resultingIntersection.x - A.x) / line.x;

		if(onLine > 0 && onLine < 1.0f)
			intersects = true;
	}
	return intersects;
}

void CHUDRadar::UpdateBinoculars(CActor *pActor, float fDeltaTime)
{
	if(SAFE_HUD_FUNC_RET(GetScopes()->IsBinocularsShown()))
	{
		EntityId lookAtObjectID = pActor->GetGameObject()->GetWorldQuery()->GetLookAtEntityId();
		ray_hit rayHit;
		if(!lookAtObjectID)
		{
			CPlayer *pPlayer = static_cast<CPlayer*> (pActor);
			lookAtObjectID = RayCastBinoculars(pPlayer,&rayHit);
		}

		// When in a cinematic, pActor view doesn't move (it's faked), so let's not use the lookAtEntityId
		if (lookAtObjectID && !g_pGame->GetHUD()->IsInCinematic())
		{
			IEntity *pEntity=gEnv->pEntitySystem->GetEntity(lookAtObjectID);
			if (pEntity)
			{
				float fDistance = (pEntity->GetWorldPos()-pActor->GetEntity()->GetWorldPos()).GetLength();
				g_pGame->GetHUD()->GetScopes()->SetBinocularsDistance(fDistance);
			}
		}
		else if(rayHit.pCollider)
			g_pGame->GetHUD()->GetScopes()->SetBinocularsDistance(rayHit.dist);
		else
			g_pGame->GetHUD()->GetScopes()->SetBinocularsDistance(0);

		if (lookAtObjectID && lookAtObjectID!=m_lookAtObjectID)
		{
			// DevTrack 17224: set the delay to a very small value
			m_lookAtTimer=0.05f;
			m_lookAtObjectID=lookAtObjectID;
		}
		else if (lookAtObjectID)
		{
			if (m_lookAtTimer>0.0f)
			{
				m_lookAtTimer-=fDeltaTime;
				if (m_lookAtTimer<=0.0f)
				{
					m_lookAtTimer=0.0f;

					if(gEnv->bMultiplayer)
					{
						bool add = CheckObjectMultiplayer(lookAtObjectID);
						
						if(add)
							m_scannerQueue.push_front(lookAtObjectID);
					}
					else
					{
						if(!IsOnRadar(lookAtObjectID, 0, m_entitiesOnRadar.size()-1) && !IsNextObject(lookAtObjectID))
							m_scannerQueue.push_front(lookAtObjectID);
					}
				}
			}
		}
		else
			m_lookAtObjectID=0;

		UpdateScanner(fDeltaTime);
	}
	else
		ResetScanner();
}

bool CHUDRadar::CheckObjectMultiplayer(EntityId id)
{
	if(!gEnv->bMultiplayer)
		return true;

	// MP binoculars only add entities temporarily. Don't add if they are already there.
	for(int e = 0; e < m_tempEntitiesOnRadar.size(); ++e)
	{
		if(id == m_tempEntitiesOnRadar[e].m_id)
		{
			return false;
		}
	}

	// also, don't allow tagging of unoccupied vehicles
	if(IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(id))
	{
		if(!pVehicle->IsPlayerDriving())
			return false;
	}

	// don't tag dead players
	if(CActor* pActor = static_cast<CActor*>(m_pActorSystem->GetActor(id)))
	{
		if(pActor->GetHealth() <= 0)
			return false;
	}

	return true;
}
