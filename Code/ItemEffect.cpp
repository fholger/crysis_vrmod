/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 7:9:2005   11:24 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ICryAnimation.h"
#include "Item.h"
#include "Game.h"
#include "GameCVars.h"
#include "Actor.h"


//------------------------------------------------------------------------
uint CItem::AttachEffect(int slot, uint id, bool attach, const char *effectName, const char *helper, const Vec3 &offset, const Vec3 &dir, float scale, bool prime)
{
	if (m_stats.mounted)
		slot=eIGS_FirstPerson;

	if (attach)
	{
		if (!g_pGameCVars->i_particleeffects)
			return 0;

		IParticleEffect *pParticleEffect = gEnv->p3DEngine->FindParticleEffect(effectName);
		if (!pParticleEffect)
			return 0;

		// generate id
		++m_effectGenId;
		while (!m_effectGenId || (m_effects.find(m_effectGenId) != m_effects.end()))
			++m_effectGenId;
	
		SEntitySlotInfo slotInfo;
		SEffectInfo effectInfo;
		
		bool validSlot = GetEntity()->GetSlotInfo(slot, slotInfo) && (slotInfo.pCharacter || slotInfo.pStatObj);

		if (!validSlot || slotInfo.pStatObj || (!helper || !helper[0]))
		{
			// get helper position
			Vec3 position(0,0,0);
			if (validSlot && helper && helper[0])
			{
				IStatObj *pStatsObj = slotInfo.pStatObj;
				position = pStatsObj->GetHelperPos(helper);
				position = GetEntity()->GetSlotLocalTM(slot, false).TransformPoint(position);
			}      
			position+=offset;

			// find a free slot
			SEntitySlotInfo dummy;
			int i=0;
			while (GetEntity()->GetSlotInfo(eIGS_Last+i, dummy))
				i++;

			// move particle slot to the helper position+offset
      effectInfo.helper = helper;
			effectInfo.slot = GetEntity()->LoadParticleEmitter(eIGS_Last+i, pParticleEffect, 0, prime, true);
			Matrix34 tm = IParticleEffect::ParticleLoc(position, dir, scale);
			GetEntity()->SetSlotLocalTM(effectInfo.slot, tm);
		}
		else if (slotInfo.pCharacter)	// bone attachment
		{
			effectInfo.helper = helper;
			effectInfo.characterSlot = slot;
			ICharacterInstance *pCharacter = slotInfo.pCharacter;
			IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
			IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(helper);

			if (!pAttachment)
			{
				GameWarning("Item '%s' trying to attach effect '%s' to attachment '%s' which does not exist!", GetEntity()->GetName(), effectName, helper);
				return 0;
			}

			CEffectAttachment *pEffectAttachment = new CEffectAttachment(effectName, Vec3(0,0,0), Vec3(0,1,0), scale);
			pEffectAttachment->CreateEffect();
			if (prime)
				pEffectAttachment->GetEmitter()->Prime();

			Matrix34 tm = Matrix34(Matrix33::CreateRotationVDir(dir));
			tm.SetTranslation(offset);
			pAttachment->AddBinding(pEffectAttachment);
			pAttachment->SetAttRelativeDefault(QuatT(tm));

			pEffectAttachment->GetEmitter()->SetMatrix( Matrix34(pAttachment->GetAttWorldAbsolute()) );
		}

		m_effects.insert(TEffectInfoMap::value_type(m_effectGenId, effectInfo));
		return m_effectGenId;
	}
	else if (id)
	{
		TEffectInfoMap::iterator it = m_effects.find(id);
		if (it == m_effects.end())
			return 0;

		SEffectInfo &info = it->second;
		if (info.slot>-1)
		{
			GetEntity()->FreeSlot(info.slot);
		}
		else
		{
			ICharacterInstance *pCharacter = GetEntity()->GetCharacter(info.characterSlot);
			if (pCharacter)
			{
				IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
				IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(info.helper.c_str());
				if(pAttachment)
					pAttachment->ClearBinding();
			}
		}
		m_effects.erase(it);
	}

	return 0;
}

namespace
{
	void	TurnOnShadowCasting(CItem* pItem, int shadowType, uint& lightFlags, IRenderNode** pCasterException)
	{
		CActor* pOwner = pItem->GetOwnerActor();
		if(!pOwner)
			return;

		if(shadowType==1)
		{
			//Only for the player
			if (pOwner->IsClient())
			{
				lightFlags |= DLF_CASTSHADOW_MAPS;
				if(!pOwner->IsThirdPerson())
				{
					if(IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(pOwner->GetEntity()->GetProxy(ENTITY_PROXY_RENDER)))
						(*pCasterException) = pRenderProxy->GetRenderNode();
				}
				else
				{
					if(IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(pItem->GetEntity()->GetProxy(ENTITY_PROXY_RENDER)))
						(*pCasterException) = pRenderProxy->GetRenderNode();
				}
			}
		}
		else if(shadowType==2)
		{
			if(!pOwner->IsClient())
			{
				lightFlags |= DLF_CASTSHADOW_MAPS;
				if(IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(pItem->GetEntity()->GetProxy(ENTITY_PROXY_RENDER)))
					(*pCasterException) = pRenderProxy->GetRenderNode();
			}
		}
		else if(shadowType==3)
		{
			lightFlags |= DLF_CASTSHADOW_MAPS;
			if(!pOwner->IsThirdPerson())
			{
				if(IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(pOwner->GetEntity()->GetProxy(ENTITY_PROXY_RENDER)))
					(*pCasterException) = pRenderProxy->GetRenderNode();
			}
			else
			{
				if(IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(pItem->GetEntity()->GetProxy(ENTITY_PROXY_RENDER)))
					(*pCasterException) = pRenderProxy->GetRenderNode();
			}
		}
	}
};

//------------------------------------------------------------------------
uint CItem::AttachLight(int slot, uint id, bool attach, float radius, const Vec3 &color, const float fSpecularMult, const char *projectTexture, float projectFov, const char *helper, const Vec3 &offset, const Vec3 &dir, const char* material, float fHDRDynamic)
{
	if (m_stats.mounted)
		slot=eIGS_FirstPerson;

	if (radius<0.1f || !g_pGameCVars || !g_pGameCVars->i_lighteffects)
		return 0;

	if (attach)
	{
		CDLight light;
		light.SetLightColor(ColorF(color.x, color.y, color.z, 1.0f));
		light.m_SpecMult = fSpecularMult;
		light.m_nLightStyle = 0;
		light.m_Origin.Set(0,0,0);
		light.m_fLightFrustumAngle = 45.0f;
		light.m_fRadius = radius;
		light.m_fDirectFactor = 1.0f;
		light.m_Flags = DLF_LIGHTSOURCE;
		light.m_NumCM = -1;
		light.m_fLifeTime = 0;
		light.m_fLightFrustumAngle = projectFov*0.5f;
		light.m_Flags |= DLF_HEATSOURCE;
		light.m_Flags &= ~DLF_DISABLED;
    light.m_fHDRDynamic = fHDRDynamic;

		IRenderNode* pCasterException = NULL;
		if(g_pGameCVars->i_lighteffectsShadows>0)
			TurnOnShadowCasting(this, g_pGameCVars->i_lighteffectsShadows,light.m_Flags,&pCasterException);

		if (projectTexture && projectTexture[0])
		{
			int flags = FT_FORCE_CUBEMAP;
			light.m_pLightImage = gEnv->pRenderer->EF_LoadTexture(projectTexture, flags, 0);

			if (!light.m_pLightImage || !light.m_pLightImage->IsTextureLoaded())
			{
				GameWarning("Item '%s' failed to load projecting light texture '%s'!", GetEntity()->GetName(), projectTexture);
				return 0;
			}
		}

		if (light.m_fLightFrustumAngle && (light.m_pLightImage != NULL) && light.m_pLightImage->IsTextureLoaded())
			light.m_Flags |= DLF_PROJECT;
		else
		{
			if (light.m_pLightImage)
				light.m_pLightImage->Release();
			light.m_pLightImage = 0;
			light.m_Flags |= DLF_POINT;
		}

    IMaterial* pMaterial = 0;
    if (material && material[0])
      pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(material);

		// generate id
		++m_effectGenId;
		while (!m_effectGenId || (m_effects.find(m_effectGenId) != m_effects.end()))
			++m_effectGenId;

		SEntitySlotInfo slotInfo;
		SEffectInfo effectInfo;
		effectInfo.slot = -1;

		bool validSlot = GetEntity()->GetSlotInfo(slot, slotInfo) && (slotInfo.pCharacter || slotInfo.pStatObj);

		if (!validSlot || slotInfo.pStatObj)
		{
			// get helper position
			Vec3 position(0,0,0);
			if (validSlot)
			{
				IStatObj *pStatsObj = slotInfo.pStatObj;
				position = pStatsObj->GetHelperPos(helper);
				position = GetEntity()->GetSlotLocalTM(slot, false).TransformPoint(position);
			}
			position+=offset;
 
			// find a free slot
			SEntitySlotInfo dummy;
			int i=0;
			while (GetEntity()->GetSlotInfo(eIGS_Last+i, dummy))
				i++;

			// move light slot to the helper position+offset
      effectInfo.helper = helper;
			effectInfo.slot = GetEntity()->LoadLight(eIGS_Last+i, &light);
            
      if (effectInfo.slot != -1 && pMaterial)
        GetEntity()->SetSlotMaterial(effectInfo.slot, pMaterial);
			
			if(light.m_Flags&DLF_CASTSHADOW_MAPS && pCasterException && effectInfo.slot != -1)
			{
				SEntitySlotInfo slotInfo;
				GetEntity()->GetSlotInfo(effectInfo.slot,slotInfo);
				if (ILightSource* pLightSource = static_cast<ILightSource*>(slotInfo.pLight))
					pLightSource->SetCastingException(pCasterException);
			}
            
      Matrix34 tm = Matrix34(Matrix33::CreateRotationVDir(dir));
			tm.SetTranslation(position);
			GetEntity()->SetSlotLocalTM(effectInfo.slot, tm);
		}
		else if (slotInfo.pCharacter)	// bone attachment
		{
			effectInfo.helper = helper;
			effectInfo.characterSlot = slot;
			ICharacterInstance *pCharacter = slotInfo.pCharacter;
			IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
			IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(helper);

			if (!pAttachment)
			{
				GameWarning("Item '%s' trying to attach light to attachment '%s' which does not exist!", GetEntity()->GetName(), helper);
				return 0;
			}

			CLightAttachment *pLightAttachment = new CLightAttachment();
			pLightAttachment->LoadLight(light);
      
      if (pMaterial)
      {
        if (ILightSource* pLightSource = pLightAttachment->GetLightSource())
          pLightSource->SetMaterial(pMaterial);        
      }
			if(light.m_Flags&DLF_CASTSHADOW_MAPS && pCasterException)
			{
				if (ILightSource* pLightSource = pLightAttachment->GetLightSource())
					pLightSource->SetCastingException(pCasterException);
			}
			
      Matrix34 tm =Matrix34(Matrix33::CreateRotationVDir(dir));
			tm.SetTranslation(offset);
			pAttachment->AddBinding(pLightAttachment);
			pAttachment->SetAttRelativeDefault(QuatT(tm));
    }

		m_effects.insert(TEffectInfoMap::value_type(m_effectGenId, effectInfo));
		return m_effectGenId;
	}
	else if (id)
	{
		TEffectInfoMap::iterator it = m_effects.find(id);
		if (it == m_effects.end())
			return 0;

		SEffectInfo &info = it->second;
		if (info.slot>-1) 
		{
			GetEntity()->FreeSlot(info.slot);
		}
		else
		{
			ICharacterInstance *pCharacter = GetEntity()->GetCharacter(info.characterSlot);
			if (pCharacter)
			{
				IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
				IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(info.helper.c_str());

				pAttachment->ClearBinding();
			}
		}
		m_effects.erase(it);
	}

	return 0;
}

//------------------------------------------------------------------------
//Above function is almost the same, just some special stuff for flashlight
//------------------------------------------------------------------------
uint CItem::AttachLightEx(int slot, uint id, bool attach, bool fakeLight /*= false */, bool castShadows /*= false*/, IRenderNode* pCasterException, float radius, const Vec3 &color, const float fSpecularMult, const char *projectTexture, float projectFov, const char *helper, const Vec3 &offset, const Vec3 &dir, const char* material, float fHDRDynamic)
{
	if (m_stats.mounted)
		slot=eIGS_FirstPerson;

	if (radius<0.1f)
		return 0;

	if (attach)
	{
		CDLight light;
		light.SetLightColor(ColorF(color.x, color.y, color.z, 1.0f));
		light.m_SpecMult = fSpecularMult;
		light.m_nLightStyle = 0;
		light.m_Origin.Set(0,0,0);
		light.m_fLightFrustumAngle = 45.0f;
		light.m_fRadius = radius;
		light.m_fDirectFactor = 1.0f;
		light.m_NumCM = -1;
		light.m_fLifeTime = 0;
		light.m_fLightFrustumAngle = projectFov*0.5f;
		light.m_Flags |= DLF_LIGHTSOURCE;
		light.m_Flags |= DLF_HEATSOURCE;
		light.m_Flags &= ~DLF_DISABLED;
		light.m_fHDRDynamic = fHDRDynamic;

		if(fakeLight)
			light.m_Flags |= DLF_FAKE;

		//Only on hight/very hight spec
		if(castShadows && (g_pGameCVars->i_lighteffects!=0))
			light.m_Flags |= DLF_CASTSHADOW_MAPS;


		if (projectTexture && projectTexture[0])
		{
			int flags = FT_FORCE_CUBEMAP;
			light.m_pLightImage = gEnv->pRenderer->EF_LoadTexture(projectTexture, flags, 0);

			if (!light.m_pLightImage || !light.m_pLightImage->IsTextureLoaded())
			{
				GameWarning("Item '%s' failed to load projecting light texture '%s'!", GetEntity()->GetName(), projectTexture);
				return 0;
			}
		}

		if (light.m_fLightFrustumAngle && (light.m_pLightImage != NULL) && light.m_pLightImage->IsTextureLoaded())
			light.m_Flags |= DLF_PROJECT;
		else
		{
			if (light.m_pLightImage)
				light.m_pLightImage->Release();
			light.m_pLightImage = 0;
			light.m_Flags |= DLF_POINT;
		}


		IMaterial* pMaterial = 0;
		if (material && material[0])
			pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(material);

		// generate id
		++m_effectGenId;
		while (!m_effectGenId || (m_effects.find(m_effectGenId) != m_effects.end()))
			++m_effectGenId;

		SEntitySlotInfo slotInfo;
		SEffectInfo effectInfo;
		effectInfo.slot = -1;

		bool validSlot = GetEntity()->GetSlotInfo(slot, slotInfo) && (slotInfo.pCharacter || slotInfo.pStatObj);

		if (!validSlot || slotInfo.pStatObj)
		{
			// get helper position
			Vec3 position(0,0,0);
			if (validSlot)
			{
				IStatObj *pStatsObj = slotInfo.pStatObj;
				position = pStatsObj->GetHelperPos(helper);
				position = GetEntity()->GetSlotLocalTM(slot, false).TransformPoint(position);
			}
			position+=offset;

			// find a free slot
			SEntitySlotInfo dummy;
			int i=0;
			while (GetEntity()->GetSlotInfo(eIGS_Last+i, dummy))
				i++;

			// move light slot to the helper position+offset
			effectInfo.helper = helper;
			effectInfo.slot = GetEntity()->LoadLight(eIGS_Last+i, &light);

			if (effectInfo.slot != -1 && pMaterial)
				GetEntity()->SetSlotMaterial(effectInfo.slot, pMaterial);

			Matrix34 tm = Matrix34(Matrix33::CreateRotationVDir(dir));
			tm.SetTranslation(position);
			GetEntity()->SetSlotLocalTM(effectInfo.slot, tm);
		}
		else if (slotInfo.pCharacter)	// bone attachment
		{
			effectInfo.helper = helper;
			effectInfo.characterSlot = slot;
			ICharacterInstance *pCharacter = slotInfo.pCharacter;
			IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
			IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(helper);

			if (!pAttachment)
			{
				GameWarning("Item '%s' trying to attach light to attachment '%s' which does not exist!", GetEntity()->GetName(), helper);
				return 0;
			}

			CLightAttachment *pLightAttachment = new CLightAttachment();
			pLightAttachment->LoadLight(light);

			if (pMaterial)
			{
				if (ILightSource* pLightSource = pLightAttachment->GetLightSource())
					pLightSource->SetMaterial(pMaterial);        
			}
			if(light.m_Flags&DLF_CASTSHADOW_MAPS)
			{
				if (ILightSource* pLightSource = pLightAttachment->GetLightSource())
					pLightSource->SetCastingException(pCasterException);
			}

			Matrix34 tm =Matrix34(Matrix33::CreateRotationVDir(dir));
			tm.SetTranslation(offset);
			pAttachment->AddBinding(pLightAttachment);
			pAttachment->SetAttRelativeDefault(QuatT(tm));
		}

		m_effects.insert(TEffectInfoMap::value_type(m_effectGenId, effectInfo));
		return m_effectGenId;
	}
	else if (id)
	{
		TEffectInfoMap::iterator it = m_effects.find(id);
		if (it == m_effects.end())
			return 0;

		SEffectInfo &info = it->second;
		if (info.slot>-1) 
		{
			GetEntity()->FreeSlot(info.slot);
		}
		else
		{
			ICharacterInstance *pCharacter = GetEntity()->GetCharacter(info.characterSlot);
			if (pCharacter)
			{
				IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
				IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(info.helper.c_str());

				pAttachment->ClearBinding();
			}
		}
		m_effects.erase(it);
	}

	return 0;
}
//------------------------------------------------------------------------
void CItem::SpawnEffect(int slot, const char *effectName, const char *helper, const Vec3 &offset, const Vec3 &dir, float scale)
{
	if (m_stats.mounted)
		slot=eIGS_FirstPerson;

	Vec3 position(0,0,0);
	Vec3 finalOffset = offset;

  SEntitySlotInfo slotInfo;
  if (GetEntity()->GetSlotInfo(slot, slotInfo))
  {
    if (slotInfo.pStatObj)	// entity slot
    {
      // get helper position
      IStatObj *pStatsObj = slotInfo.pStatObj;
      position = pStatsObj->GetHelperPos(helper);

			position = GetEntity()->GetSlotWorldTM(slot).TransformPoint(position);
    }
    else if (slotInfo.pCharacter)	// bone attachment
    {
      ICharacterInstance *pCharacter = slotInfo.pCharacter;
      IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
      IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(helper);

      if (pAttachment)
      {
        const Matrix34 m = Matrix34(pAttachment->GetAttWorldAbsolute());
        position = m.GetTranslation();
      }
      else
      {
        int16 id = pCharacter->GetISkeletonPose()->GetJointIDByName(helper);
        if (id>=0)
          position = pCharacter->GetISkeletonPose()->GetAbsJointByID(id).t;

				position = GetEntity()->GetSlotWorldTM(slot).TransformPoint(position);
      }
    }
  }
	else if(m_stats.mounted && !m_stats.fp)
	{
		if(GetIWeapon())
		{
				// if no helper specified, try getting pos from firing locator
				IWeaponFiringLocator *pLocator = GetIWeapon()->GetFiringLocator();            

				if (pLocator)
				{
					if(!pLocator->GetFiringPos(GetEntityId(), NULL, position))
						position.Set(0.0f,0.0f,0.0f);
					else
						finalOffset = GetEntity()->GetWorldTM().TransformVector(finalOffset);
				}

		}
	}

	position += finalOffset;

	IParticleEffect *pParticleEffect = gEnv->p3DEngine->FindParticleEffect(effectName);
	if (pParticleEffect)
		pParticleEffect->Spawn(true, IParticleEffect::ParticleLoc(position, dir, scale));
}

//------------------------------------------------------------------------
IParticleEmitter *CItem::GetEffectEmitter(uint id) const
{
	TEffectInfoMap::const_iterator it = m_effects.find(id);
	if (it == m_effects.end()) 
		return 0;

	const SEffectInfo &info = it->second;
	SEntitySlotInfo slotInfo;

	if (GetEntity()->GetSlotInfo(info.slot, slotInfo) && slotInfo.pParticleEmitter)
		return slotInfo.pParticleEmitter;

	if (GetEntity()->GetSlotInfo(info.characterSlot, slotInfo) && slotInfo.pCharacter)
	{
		ICharacterInstance *pCharacter = slotInfo.pCharacter;
		IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
		IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(info.helper.c_str());
		if (pAttachment)
		{
			CEffectAttachment *pEffectAttachment = static_cast<CEffectAttachment *>(pAttachment->GetIAttachmentObject());
			if (!pEffectAttachment)
			{
				// commenting out for silencer functionality
				//GameWarning("CItem::GetEffectEmitter: no effect attachment on %s (helper %s)", GetEntity()->GetName(), info.helper.c_str());
				return 0;
			}
			return pEffectAttachment->GetEmitter();
		}
	}

	return 0;
}

//------------------------------------------------------------------------
void CItem::EnableLight(bool enable, uint id)
{
  TEffectInfoMap::const_iterator it = m_effects.find(id);
  if (it == m_effects.end()) 
    return;

  const SEffectInfo &info = it->second;
  SEntitySlotInfo slotInfo;
  ILightSource* pLightSource = 0;
  
  if (info.slot != -1)
  {
    if (GetEntity()->GetSlotInfo(info.slot, slotInfo) && slotInfo.pLight)
      pLightSource = static_cast<ILightSource*>(slotInfo.pLight);
  }
  else if (info.characterSlot != -1)
  {
    if (GetEntity()->GetSlotInfo(info.characterSlot, slotInfo) && slotInfo.pCharacter)
    { 
      IAttachmentManager *pAttachmentManager = slotInfo.pCharacter->GetIAttachmentManager();
      IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(info.helper.c_str());
      if (pAttachment)
      {
        CLightAttachment *pLightAttachment = static_cast<CLightAttachment *>(pAttachment->GetIAttachmentObject());
        if (pLightAttachment)
          pLightSource = pLightAttachment->GetLightSource();
      }
    }
  }

  if (pLightSource)
  {
    CDLight& light = pLightSource->GetLightProperties();       
    light.m_Flags = (enable) ? light.m_Flags&~DLF_DISABLED : light.m_Flags|DLF_DISABLED;
    pLightSource->SetLightProperties(light);
  }
}

//------------------------------------------------------------------------
void CItem::SetLightRadius(float radius, uint id)
{
	TEffectInfoMap::const_iterator it = m_effects.find(id);
	if (it == m_effects.end()) 
		return;

	const SEffectInfo &info = it->second;
	SEntitySlotInfo slotInfo;
	ILightSource* pLightSource = 0;

	if (info.slot != -1)
	{
		if (GetEntity()->GetSlotInfo(info.slot, slotInfo) && slotInfo.pLight)
			pLightSource = static_cast<ILightSource*>(slotInfo.pLight);
	}
	else if (info.characterSlot != -1)
	{
		if (GetEntity()->GetSlotInfo(info.characterSlot, slotInfo) && slotInfo.pCharacter)
		{ 
			IAttachmentManager *pAttachmentManager = slotInfo.pCharacter->GetIAttachmentManager();
			IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(info.helper.c_str());
			if (pAttachment)
			{
				CLightAttachment *pLightAttachment = static_cast<CLightAttachment *>(pAttachment->GetIAttachmentObject());
				if (pLightAttachment)
					pLightSource = pLightAttachment->GetLightSource();
			}
		}
	}

	if (pLightSource)
	{
		CDLight& light = pLightSource->GetLightProperties();
		light.m_fRadius = radius;
		pLightSource->SetLightProperties(light);
	}
}

//------------------------------------------------------------------------
void CItem::SetEffectWorldTM(uint id, const Matrix34 &tm)
{
	TEffectInfoMap::const_iterator it = m_effects.find(id);
	if (it == m_effects.end()) 
		return;

	const SEffectInfo &info = it->second;
	SEntitySlotInfo slotInfo;

	if (GetEntity()->GetSlotInfo(info.slot, slotInfo) && (slotInfo.pParticleEmitter||slotInfo.pLight))
	{
		Matrix34 worldMatrix = GetEntity()->GetWorldTM();
		Matrix34 localMatrix = worldMatrix.GetInverted()*tm;

		GetEntity()->SetSlotLocalTM(info.slot, localMatrix);
	}
	else if (GetEntity()->GetSlotInfo(info.characterSlot, slotInfo) && slotInfo.pCharacter)
		SetCharacterAttachmentWorldTM(info.characterSlot, info.helper.c_str(), tm);
}

//------------------------------------------------------------------------
Matrix34 CItem::GetEffectWorldTM(uint id)
{
	TEffectInfoMap::const_iterator it = m_effects.find(id);
	if (it == m_effects.end()) 
		return Matrix34::CreateIdentity();

	const SEffectInfo &info = it->second;
	SEntitySlotInfo slotInfo;

	if (GetEntity()->GetSlotInfo(info.slot, slotInfo) && slotInfo.pParticleEmitter)
		return GetEntity()->GetSlotWorldTM(info.slot);
	else if (GetEntity()->GetSlotInfo(info.characterSlot, slotInfo) && slotInfo.pCharacter)
		return GetCharacterAttachmentWorldTM(info.characterSlot, info.helper.c_str());

	return Matrix34::CreateIdentity();
}
