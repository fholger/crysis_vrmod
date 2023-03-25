/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 
	Silhouettes manager

-------------------------------------------------------------------------
History:
- 20:07:2007: Created by Julien Darre

*************************************************************************/

#include "StdAfx.h"
#include "IActorSystem.h"
#include "IEntitySystem.h"
#include "IItemSystem.h"
#include "IVehicleSystem.h"
#include "HUDSilhouettes.h"
#include "Item.h"

//-----------------------------------------------------------------------------------------------------

CHUDSilhouettes::CHUDSilhouettes()
{
	m_silhouettesVector.resize(256);
}

//-----------------------------------------------------------------------------------------------------

CHUDSilhouettes::~CHUDSilhouettes()
{
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetVisionParams(EntityId uiEntityId,float r,float g,float b,float a)
{
	// When quick loading, entities may have been already destroyed when we do a ShowBinoculars(false)
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(uiEntityId);
	if(!pEntity)
		return;

	// Some actor accessories may not have render proxy
	IEntityRenderProxy *pEntityRenderProxy = static_cast<IEntityRenderProxy *>(pEntity->GetProxy(ENTITY_PROXY_RENDER));
	if(!pEntityRenderProxy)
		return;

	pEntityRenderProxy->SetVisionParams(r,g,b,a);
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetFlowGraphSilhouette(IEntity *pEntity,float r,float g,float b,float a,float fDuration)
{
	if(!pEntity)
		return;

	if(GetFGSilhouette(pEntity->GetId()) != m_silhouettesFGVector.end())
		return;

	SetSilhouette(pEntity, r, g, b ,a, fDuration);
	m_silhouettesFGVector[pEntity->GetId()] = Vec3(r,g,b);
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::ResetFlowGraphSilhouette(EntityId uiEntityId)
{
	std::map<EntityId, Vec3>::iterator it = GetFGSilhouette(uiEntityId);
	if(it != m_silhouettesFGVector.end())
	{
		m_silhouettesFGVector.erase(it);
		ResetSilhouette(uiEntityId);
		return;
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetSilhouette(IEntity *pEntity,float r,float g,float b,float a,float fDuration)
{
	if(!pEntity)
		return;

	SSilhouette *pSilhouette = NULL;

	// First pass: is that Id already in a slot?
	for(TSilhouettesVector::iterator iter=m_silhouettesVector.begin(); iter!=m_silhouettesVector.end(); ++iter)
	{
		SSilhouette *pSil = &(*iter);
		if(pEntity->GetId() == pSil->uiEntityId)
		{
			pSilhouette = pSil;
			break;
		}
	}

	if(!pSilhouette)
	{
		// Second pass: try to find a free slot
		for(TSilhouettesVector::iterator iter=m_silhouettesVector.begin(); iter!=m_silhouettesVector.end(); ++iter)
		{
			SSilhouette *pSil = &(*iter);
			if(!pSil->bValid)
			{
				pSilhouette = pSil;
				break;
			}
		}
	}

	CRY_ASSERT_MESSAGE(pSilhouette,"Vector size should be increased!");

	if(pSilhouette)
	{
		pSilhouette->uiEntityId	= pEntity->GetId();
		pSilhouette->fTime = fDuration;
		pSilhouette->bValid			= true;
		pSilhouette->r = r;
		pSilhouette->g = g;
		pSilhouette->b = b;
		pSilhouette->a = a;

		SetVisionParams(pEntity->GetId(),r,g,b,a);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetSilhouette(IActor *pActor,float r,float g,float b,float a,float fDuration,bool bHighlightCurrentItem,bool bHighlightAccessories)
{
	if(!pActor)
		return;

	SetSilhouette(pActor->GetEntity(),r,g,b,a,fDuration);

	if(bHighlightCurrentItem)
	{
		SetSilhouette(pActor->GetCurrentItem(),r,g,b,a,fDuration,bHighlightAccessories);
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetSilhouette(IItem *pItem,float r,float g,float b,float a,float fDuration,bool bHighlightAccessories)
{
	if(!pItem)
		return;

	SetSilhouette(pItem->GetEntity(),r,g,b,a,fDuration);
	
	if(bHighlightAccessories)
	{
		const CItem::TAccessoryMap *pAccessoryMap = static_cast<CItem *>(pItem)->GetAttachedAccessories();
		for(CItem::TAccessoryMap::const_iterator iter=pAccessoryMap->begin(); iter!=pAccessoryMap->end(); ++iter)
		{
			SetSilhouette(gEnv->pEntitySystem->GetEntity((*iter).second),r,g,b,a,fDuration);
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetSilhouette(IVehicle *pVehicle,float r,float g,float b,float a,float fDuration)
{
	if(!pVehicle)
		return;

	SetSilhouette(pVehicle->GetEntity(),r,g,b,a,fDuration);
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::ResetSilhouette(EntityId uiEntityId)
{
	for(TSilhouettesVector::iterator iter=m_silhouettesVector.begin(); iter!=m_silhouettesVector.end(); ++iter)
	{
		SSilhouette *pSilhouette = &(*iter);

		if(pSilhouette->uiEntityId == uiEntityId && pSilhouette->bValid)
		{
			std::map<EntityId, Vec3>::iterator it = GetFGSilhouette(pSilhouette->uiEntityId);
			if(it != m_silhouettesFGVector.end())
			{
				Vec3 color = it->second;
				SetVisionParams(uiEntityId, color.x, color.y, color.z, 1.0f);
			}
			else
			{
				SetVisionParams(pSilhouette->uiEntityId,0,0,0,0);
				pSilhouette->bValid = false;
			}

			return;
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::SetType(int iType)
{
	// Exit of binoculars: we need to reset all silhouettes
	if(0 == iType)
	{
		for(TSilhouettesVector::iterator iter=m_silhouettesVector.begin(); iter!=m_silhouettesVector.end(); ++iter)
		{
			SSilhouette *pSilhouette = &(*iter);

			if(pSilhouette->bValid)
			{
				std::map<EntityId, Vec3>::iterator it = GetFGSilhouette(pSilhouette->uiEntityId);
				if(it != m_silhouettesFGVector.end())
				{
					Vec3 color = it->second;
					SetVisionParams(pSilhouette->uiEntityId, color.x, color.y, color.z, 1.0f);
				}
				else
				{
					SetVisionParams(pSilhouette->uiEntityId,0,0,0,0);
					pSilhouette->bValid = false;
				}
			}

		}
	}

	gEnv->p3DEngine->SetPostEffectParam("CryVision_Type",1-iType);
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::Update(float frameTime)
{
	for(TSilhouettesVector::iterator iter=m_silhouettesVector.begin(); iter!=m_silhouettesVector.end(); ++iter)
	{
		SSilhouette *pSilhouette = &(*iter);

		if(pSilhouette->bValid && pSilhouette->fTime != -1)
		{
			pSilhouette->fTime -= frameTime;
			if(pSilhouette->fTime < 0.0f)
			{
				SetVisionParams(pSilhouette->uiEntityId,0,0,0,0);
				pSilhouette->bValid = false;
				pSilhouette->fTime = 0.0f;
			}
			else if (pSilhouette->fTime < 1.0f)
			{
				// fade out for the last second
				float scale = pSilhouette->fTime ;
				scale *= scale;
				SetVisionParams(pSilhouette->uiEntityId,pSilhouette->r*scale,pSilhouette->g*scale,pSilhouette->b*scale,pSilhouette->a*scale);
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------

void CHUDSilhouettes::Serialize(TSerialize &ser)
{
	if(ser.IsReading())
		m_silhouettesFGVector.clear();

	int amount = m_silhouettesFGVector.size();
	ser.Value("amount", amount);
	if(amount)
	{
		EntityId id = 0;
		Vec3 color;
		if(ser.IsWriting())
		{
			std::map<EntityId, Vec3>::iterator it = m_silhouettesFGVector.begin();
			std::map<EntityId, Vec3>::iterator end = m_silhouettesFGVector.end();
			for(; it != end; ++it)
			{
				ser.BeginGroup("FlowGraphSilhouette");
				id = it->first;
				color = it->second;
				ser.Value("id", id);
				ser.Value("color", color);
				ser.EndGroup();
			}
		}
		else if(ser.IsReading())
		{
			for(int i = 0; i < amount; ++i)
			{
				ser.BeginGroup("FlowGraphSilhouette");
				ser.Value("id", id);
				ser.Value("color", color);
				ser.EndGroup();
				if(IEntity *pEntity = gEnv->pEntitySystem->GetEntity(id))
					SetFlowGraphSilhouette(pEntity, color.x, color.y, color.z, 1.0f, -1.0f);
			}
		}
	}
}

std::map<EntityId, Vec3>::iterator CHUDSilhouettes::GetFGSilhouette(EntityId id)
{
	std::map<EntityId, Vec3>::iterator returnVal = m_silhouettesFGVector.end();

	std::map<EntityId, Vec3>::iterator it = m_silhouettesFGVector.begin();
	std::map<EntityId, Vec3>::iterator end = m_silhouettesFGVector.end();
	for(; it != end; ++it)
	{
		if(it->first == id)
			returnVal = it;
	}
	return returnVal;
}