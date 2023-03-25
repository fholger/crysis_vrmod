/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  
-------------------------------------------------------------------------
History:
- 08:06:2007   : Created by Benito G.R.

*************************************************************************/
#include "StdAfx.h"

#include "C4Projectile.h"
#include "Player.h"
#include "GameRules.h"
#include "Bullet.h"

CC4Projectile::CC4Projectile():
m_stuck(false),
m_notStick(false),
m_nConstraints(0),
m_frozen(false),
m_teamId(0),
m_parentEntity(0)
{
	m_localChildPos.Set(0.0f,0.0f,0.0f);
	m_localChildRot.SetIdentity();
}

//-------------------------------------------
CC4Projectile::~CC4Projectile()
{
	if(gEnv->bMultiplayer && gEnv->bServer)
	{
		IActor* pOwner = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_ownerId);
		if(pOwner && pOwner->IsPlayer())
		{
			((CPlayer*)pOwner)->RecordExplosiveDestroyed(GetEntityId(), 2);
		}
	}
}

//------------------------------------------
void CC4Projectile::HandleEvent(const SGameObjectEvent &event)
{
	if (m_destroying)
		return;

	CProjectile::HandleEvent(event);

	if (event.event == eGFE_OnCollision)
	{
		EventPhysCollision *pCollision = (EventPhysCollision *)event.ptr;

		if(gEnv->bServer && !m_stuck && !m_notStick)
			Stick(pCollision);
	}
	else if (event.event==eCGE_PostFreeze)
		m_frozen=event.param!=0;
}

//-------------------------------------------
void CC4Projectile::Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale)
{
	CProjectile::Launch(pos, dir, velocity, speedScale);

	if(gEnv->bMultiplayer && gEnv->bServer)
	{
		CActor* pOwner = GetWeapon()->GetOwnerActor();
		if(pOwner && pOwner->IsPlayer())
		{
			((CPlayer*)pOwner)->RecordExplosivePlaced(GetEntityId(), 2);
		}
	}
}

//-------------------------------------------
void CC4Projectile::SetParams(EntityId ownerId, EntityId hostId, EntityId weaponId, int fmId, int damage, int hitTypeId)
{
	// if this is a team game, record which team placed this claymore...
	if(gEnv->bServer)
	{
		if(CGameRules* pGameRules = g_pGame->GetGameRules())
		{
			m_teamId = pGameRules->GetTeam(ownerId);
			pGameRules->SetTeam(m_teamId, GetEntityId());
		}
	}

	CProjectile::SetParams(ownerId, hostId, weaponId, fmId, damage, hitTypeId);
}

//-------------------------------------------
void CC4Projectile::Explode(bool destroy, bool impact/* =false */, const Vec3 &pos/* =ZERO */, const Vec3 &normal/* =FORWARD_DIRECTION */, const Vec3 &vel/* =ZERO */, EntityId targetId/* =0  */)
{
	if (m_frozen)
		return;

	CProjectile::Explode(destroy, impact, pos, normal, vel, targetId);
}

//-------------------------------------------
void CC4Projectile::OnHit(const HitInfo& hit)
{
	if (m_frozen)
		return;

	CProjectile::OnHit(hit);
}

//-----------------------------------------------
//This function is only executed on the server
void CC4Projectile::Stick(EventPhysCollision *pCollision)
{
	assert(pCollision);
	int trgId = 1;
	int srcId = 0;
	IPhysicalEntity *pTarget = pCollision->pEntity[trgId];

	if (pTarget == GetEntity()->GetPhysics())
	{
		trgId = 0;
		srcId = 1;
		pTarget = pCollision->pEntity[trgId];
	}

	//Do not stick to breakable glass
	if(ISurfaceType *pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(pCollision->idmat[trgId]))
	{
		if(pSurfaceType->GetBreakability()==1)
		{
			m_notStick = true;
			return;
		}
	}
	if(pCollision->idmat[trgId]==CBullet::GetWaterMaterialId())
	{
		return; //Skip water collisions
	}

	IEntity *pTargetEntity = pTarget ? gEnv->pEntitySystem->GetEntityFromPhysics(pTarget) : 0;

	if (pTarget && (!pTargetEntity || (pTargetEntity->GetId() != m_ownerId)))
	{
		//Special cases
		if(pTargetEntity)
		{
			//Stick to actors using a character attachment
			CActor *pActor = static_cast<CActor*>(gEnv->pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pTargetEntity->GetId()));
			
			//Not in MP
			if(pActor && gEnv->bMultiplayer)
			{
				m_notStick = true;
				return;
			}

			if(pActor && pActor->GetHealth()>0)
			{
				if(pActor->GetActorSpecies()!=eGCT_HUMAN)
				{
					m_notStick = true;
					return;
				}	

				if(StickToCharacter(true,pTargetEntity))
				{
					GetGameObject()->SetAspectProfile(eEA_Physics, ePT_None);
					m_stuck = true;
				}
				m_notStick = true;
				return;
			}

			//Do not stick to small objects...
			if(!pActor)
			{
				AABB c4Box;
				GetEntity()->GetWorldBounds(c4Box);

				pe_params_part pPart;
				pPart.partid = pCollision->partid[trgId];
				if(pTarget->GetParams(&pPart) && pPart.pPhysGeom && pPart.pPhysGeom->V<c4Box.GetVolume())
				{
					m_notStick = true;
					return;
				}
			}

		}
		else if(pTarget->GetType()==PE_LIVING)
		{
			m_notStick = true;
			return;
		}

		//Test again render mesh if possible
		RefineCollision(pCollision);

		if(!pTargetEntity)
			StickToStaticObject(pCollision,pTarget);
		else
		{
			//Do not attach to items
			if(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pTargetEntity->GetId()))
			{
				m_notStick = true;
				return;
			}

			Matrix34 mat = pTargetEntity->GetWorldTM();
			mat.Invert();
			Matrix33 rotMat = Matrix33::CreateOrientation(mat.TransformVector(-pCollision->n),GetEntity()->GetWorldTM().TransformVector(Vec3(0,0,1)),g_PI);
			m_localChildPos = mat.TransformPoint(pCollision->pt);
			m_localChildRot = Quat(rotMat);

			mat.SetIdentity();
			mat.SetRotation33(rotMat);
			mat.SetTranslation(m_localChildPos);

			//Dephysicalize and stick
			m_parentEntity = pTargetEntity->GetId();
			GetGameObject()->SetAspectProfile(eEA_Physics, ePT_StuckToEntity);
			StickToEntity(pTargetEntity,mat);
		}

		m_stuck = true;
	}
}

//---------------------------------------------------------------------
//This function is only executed on the server
void CC4Projectile::StickToStaticObject(EventPhysCollision *pCollision, IPhysicalEntity* pTarget)
{
	//Calculate new position and orientation 
	Matrix34 mat;
	Vec3 pos = pCollision->pt+(pCollision->n*0.05f);
	mat.SetRotation33(Matrix33::CreateOrientation(-pCollision->n,GetEntity()->GetWorldTM().TransformVector(Vec3(0,0,1)),g_PI));
	Vec3 newUpDir = mat.TransformVector(Vec3(0,0,1));
	pos += (newUpDir*-0.1f);
	mat.SetTranslation(pos+(newUpDir*-0.1f));
	GetEntity()->SetWorldTM(mat);

	GetGameObject()->SetAspectProfile(eEA_Physics, ePT_Static);

	m_stuck = true;
}

//------------------------------------------------------------------------
void CC4Projectile::StickToEntity(IEntity* pEntity, Matrix34 &localMatrix)
{
	pEntity->AttachChild(GetEntity());
	GetEntity()->SetLocalTM(localMatrix);

	m_parentEntity = pEntity->GetId();
	m_stuck = true;
}

//------------------------------------------------------------------------
bool CC4Projectile::StickToCharacter(bool stick,IEntity* pActor)
{
	if(!pActor)
		return false;

	//Check for friendly AI
	if(pActor->GetAI())
	{
		if(CWeapon* pWeapon = GetWeapon())
		{
			if(CActor* pPlayer = pWeapon->GetOwnerActor())
			{
				if(pPlayer->GetEntity()->GetAI())
					if(!pActor->GetAI()->IsHostile(pPlayer->GetEntity()->GetAI(),false))
						return false;
			}
		}
	}

	ICharacterInstance* pCharacter = pActor->GetCharacter(0);
	if(!pCharacter)
		return false;

	//Actors doesn't support constraints, try to stick as character attachment
	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = NULL;

	//Select one of the attachment points
	Vec3 charOrientation = pActor->GetRotation().GetColumn1();
	Vec3 c4ToChar = pActor->GetWorldPos() - GetEntity()->GetWorldPos();
	c4ToChar.Normalize();

	//if(c4ToChar.Dot(charOrientation)>0.0f)
		//pAttachment = pAttachmentManager->GetInterfaceByName("c4_back");
	//else
		pAttachment = pAttachmentManager->GetInterfaceByName("c4_front");

	if (!pAttachment)
	{
		GameWarning("No c4 face attachment found in actor");
		if(!pAttachment)
			return false;
	}

	if(stick)
	{
		//Check if there's already one
		if(IAttachmentObject *pAO = pAttachment->GetIAttachmentObject())
			return false;

		CEntityAttachment *pEntityAttachment = new CEntityAttachment();
		pEntityAttachment->SetEntityId(GetEntityId());

		pAttachment->AddBinding(pEntityAttachment);
		pAttachment->HideAttachment(0);

		m_stuck = true;
	}
	else
	{
		pAttachment->ClearBinding();
		m_stuck = false;
	}
	return true;

}

//-------------------------------------------------------------
void CC4Projectile::RefineCollision(EventPhysCollision *pCollision)
{
	static ray_hit hit;
	static const int objTypes = ent_all;    
	static const unsigned int flags = rwi_stop_at_pierceable|rwi_colltype_any;
	static const float c4Offset = 0.05f;

	Vec3 pos = pCollision->pt+(pCollision->n*0.2f);
	int hits = gEnv->pPhysicalWorld->RayWorldIntersection(pos,pCollision->n*-0.7f,objTypes,flags,&hit,1,&m_pPhysicalEntity,m_pPhysicalEntity?1:0);
	if(hits!=0)
	{
		if(gEnv->p3DEngine->RefineRayHit(&hit,-pCollision->n*1.5f))
		{
			pCollision->n  = hit.n;
			pCollision->pt = hit.pt + (hit.n*c4Offset);
		}
	}
	
}

//---------------------------------------------------------
bool CC4Projectile::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	bool res = CProjectile::NetSerialize(ser,aspect,profile,flags);

	if((aspect == eEA_Physics) && (profile == ePT_StuckToEntity))
	{
		ser.Value("localPos", m_localChildPos);
		ser.Value("localOri", m_localChildRot);
		ser.Value("parentEntity", m_parentEntity, 'eid');

		if(ser.IsReading() && !m_stuck)
		{
			Matrix34 localMat = Matrix34::Create(Vec3(1.0f,1.0f,1.0f),m_localChildRot,m_localChildPos);
			if(IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_parentEntity))
				StickToEntity(pEntity,localMat);
			else
				ser.FlagPartialRead();
		}
		return true;
	}

	return res;
}

