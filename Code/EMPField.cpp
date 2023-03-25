/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  Anti-Suit field
-------------------------------------------------------------------------
History:
- 10:04:2007   14:39 : Created by Marco Koegler
- 21:08:2007   Benito G.R. - Not used (not registered in WeaponSytem)

*************************************************************************/
#include "StdAfx.h"
#include "EMPField.h"
#include "Player.h"
#include "HUD/HUD.h"
#include "GameCVars.h"

#include "IEntityProxy.h"

//------------------------------------------------------------------------
CEMPField::CEMPField()
: m_radius(5)
, m_activationTime(3)
, m_charging(false)
, m_empEffectId(-1)
{
}

CEMPField::~CEMPField()
{
	ReleaseAll();
}


bool CEMPField::Init(IGameObject *pGameObject)
{
	if (CProjectile::Init(pGameObject))
	{
		m_activationTime = GetParam("activationTime", m_activationTime);
		m_radius = GetParam("radius", m_radius);

		GetEntity()->SetTimer(ePTIMER_ACTIVATION, (int)(m_activationTime*1000.0f));

		return true;
	}

	return false;
}

void CEMPField::ProcessEvent(SEntityEvent &event)
{
	switch(event.event)
	{
	case ENTITY_EVENT_TIMER:
		{
			switch(event.nParam[0])
			{
			case ePTIMER_ACTIVATION:
				OnEMPActivate();
				break;
			default:
				CProjectile::ProcessEvent(event);
				break;
			}
		}
		break;
	case ENTITY_EVENT_ENTERAREA:
		{
			IEntity * pEntity = gEnv->pEntitySystem->GetEntity(event.nParam[0]);
			if(pEntity)
			{
				CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()));
				if (pActor && pActor->GetSpectatorMode() == 0 && pActor->GetActorClass() == CPlayer::GetActorClassType())
				{
					OnPlayerEnter(static_cast<CPlayer*>(pActor));
				}
			}
		}
		break;
	case ENTITY_EVENT_LEAVEAREA:
		{
			IEntity * pEntity = gEnv->pEntitySystem->GetEntity(event.nParam[0]);
			if(pEntity)
			{
				CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()));
				if (pActor && pActor->GetSpectatorMode() == 0 && pActor->GetActorClass() == CPlayer::GetActorClassType())
				{
					OnPlayerLeave(static_cast<CPlayer*>(pActor));
					RemoveEntity(event.nParam[0]);
				}
			}
		}
		break;
	default:
		CProjectile::ProcessEvent(event);
		break;
	}
}

void CEMPField::Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale)
{
	CProjectile::Launch(pos, dir, velocity, speedScale);
}

void CEMPField::OnEMPActivate()
{
	if (gEnv->bServer)
	{
		IEntityTriggerProxy *pTriggerProxy = (IEntityTriggerProxy*)(GetEntity()->GetProxy(ENTITY_PROXY_TRIGGER));

		if (!pTriggerProxy)
		{
			GetEntity()->CreateProxy(ENTITY_PROXY_TRIGGER);
			pTriggerProxy = (IEntityTriggerProxy*)GetEntity()->GetProxy(ENTITY_PROXY_TRIGGER);
		}

		if(pTriggerProxy)
		{
			AABB boundingBox = AABB(Vec3(-m_radius,-m_radius,-m_radius), Vec3(m_radius,m_radius,m_radius));
			pTriggerProxy->SetTriggerBounds(boundingBox);
		}
	}

	const char* effect=0;
	float scale=1.0f; 
	bool prime = true;
	effect = GetParam("effect", effect);
	scale = GetParam("scale", scale);
	prime = GetParam("prime", prime);
	if (g_pGame->GetCVars()->g_empStyle == 0 && effect && effect[0])
	{
		m_empEffectId = AttachEffect(true, 0, effect, Vec3(0,0,0), Vec3(0,1,0), scale, prime);
	}
}

void CEMPField::OnPlayerEnter(CPlayer* pPlayer)
{
	m_players.push_back(pPlayer->GetEntity()->GetId());

	if (CNanoSuit* pSuit = pPlayer->GetNanoSuit())
	{
		if (!pSuit->IsInvulnerable())
		{
			pSuit->Activate(false);
			pSuit->SetSuitEnergy(0.0f);

			pPlayer->GetGameObject()->InvokeRMI(CPlayer::ClEMP(), CPlayer::EMPParams(1.0f), eRMI_ToClientChannel, pPlayer->GetChannelId());
		}
	}
}

void CEMPField::OnPlayerLeave(CPlayer* pPlayer)
{
	if (pPlayer->GetHealth()<=0)
		return;

	if (CNanoSuit* pSuit = pPlayer->GetNanoSuit())
	{
		if(!pSuit->IsActive())
			pSuit->Activate(true, 10.0f);
	}
	
	pPlayer->GetGameObject()->InvokeRMI(CPlayer::ClEMP(), CPlayer::EMPParams(0.0f), eRMI_ToClientChannel, pPlayer->GetChannelId());
}

void CEMPField::RemoveEntity(EntityId id)
{
	for(TEntities::iterator it = m_players.begin(); it != m_players.end(); ++it)
	{
		if (id == *it)
		{
			m_players.erase(it);
			break;
		}
	}
}

void CEMPField::ReleaseAll()
{
	for(TEntities::iterator it = m_players.begin(); it != m_players.end(); ++it)
	{
		IEntity * pEntity = gEnv->pEntitySystem->GetEntity(*it);
		if(pEntity)
		{
			CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()));
			OnPlayerLeave(pPlayer);
		}
	}
	m_players.clear();
}