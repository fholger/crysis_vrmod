// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "Shake.h"
#include "../Game.h"
#include <IViewSystem.h>

CShake::CShake()
{
}

CShake::~CShake()
{
}

bool CShake::Init(IGameObject *pGameObject)
{
	SetGameObject(pGameObject);

	//Initialize default values before (in case ScriptTable fails)
	m_radius = 30.0f;
	m_shake = 0.05f;

	SmartScriptTable props;
	IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
	if(!pScriptTable || !pScriptTable->GetValue("Properties", props))
		return false;

	props->GetValue("Radius", m_radius);
	props->GetValue("Shake", m_shake);

	return true;
}

//------------------------------------------------------------------------
void CShake::PostInit(IGameObject *pGameObject)
{
	GetGameObject()->EnableUpdateSlot(this, 0);
}

//------------------------------------------------------------------------
void CShake::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CShake::FullSerialize(TSerialize ser)
{
	ser.Value("Radius", m_radius);
	ser.Value("Shake", m_shake);
}

//------------------------------------------------------------------------
void CShake::Update(SEntityUpdateContext &ctx, int updateSlot)
{
	IActor *pClient = g_pGame->GetIGameFramework()->GetClientActor();
	if (pClient)
	{
		float dist2ToClient((pClient->GetEntity()->GetWorldPos() - GetEntity()->GetWorldPos()).len2());
		float maxRange(m_radius * m_radius);
		if (dist2ToClient<maxRange)
		{
			IView *pView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetViewByEntityId(pClient->GetEntityId());
			IView *pViewActive = g_pGame->GetIGameFramework()->GetIViewSystem()->GetActiveView();
			if (pView)
			{
				float strength = (1.0f - (dist2ToClient/maxRange)) * 0.5;
				pView->SetViewShake(ZERO,Vec3(m_shake*strength,0,m_shake*strength),0.1f,0.0225f,1.5f,1);
			}
		}
	}
}

//------------------------------------------------------------------------
void CShake::HandleEvent(const SGameObjectEvent &event)
{
}

//------------------------------------------------------------------------
void CShake::ProcessEvent(SEntityEvent &event)
{
}

//------------------------------------------------------------------------
void CShake::SetAuthority(bool auth)
{
}
