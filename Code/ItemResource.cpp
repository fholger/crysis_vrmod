/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 30:8:2005   12:30 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "ItemSharedParams.h"

#include <ICryAnimation.h>
#include <ISound.h>
#include <CryPath.h>
#include <IVehicleSystem.h>
#include "Actor.h"
#include "Game.h"
#include "GameCVars.h"
#include "Player.h"


//------------------------------------------------------------------------
void CItem::RemoveEntity(bool force)
{
	if (gEnv->pSystem->IsEditor() && !force)
		Hide(true);
	else if (IsServer() || force)
		gEnv->pEntitySystem->RemoveEntity(GetEntityId());
}

//------------------------------------------------------------------------
bool CItem::CreateCharacterAttachment(int slot, const char *name, int type, const char *bone)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return false;

	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(name);

	if (pAttachment)
	{
//		GameWarning("Item '%s' trying to create attachment '%s' which already exists!", GetEntity()->GetName(), name);
		return false;
	}

	pAttachment = pAttachmentManager->CreateAttachment(name, type, bone);

	if (!pAttachment)
	{
		if (type == CA_BONE)
			GameWarning("Item '%s' failed to create attachment '%s' on bone '%s'!", GetEntity()->GetName(), name, bone);
		return false;
	}

	return true;
}

//------------------------------------------------------------------------
void CItem::DestroyCharacterAttachment(int slot, const char *name)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return;

	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	pAttachmentManager->RemoveAttachmentByName(name);
}

//------------------------------------------------------------------------
void CItem::ResetCharacterAttachment(int slot, const char *name)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return;

	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(name);

	if (!pAttachment)
	{
		GameWarning("Item '%s' trying to reset attachment '%s' which does not exist!", GetEntity()->GetName(), name);
		return;
	}

	pAttachment->ClearBinding();
}

//------------------------------------------------------------------------
const char *CItem::GetCharacterAttachmentBone(int slot, const char *name)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return 0;

	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(name);

	if (!pAttachment)
	{
		GameWarning("Item '%s' trying to get attachment bone on '%s' which does not exist!", GetEntity()->GetName(), name);
		return 0;
	}

	return pCharacter->GetISkeletonPose()->GetJointNameByID(pAttachment->GetBoneID());
}

//------------------------------------------------------------------------
void CItem::SetCharacterAttachment(int slot, const char *name, IEntity *pEntity, int flags)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return;

	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(name);

	if (!pAttachment)
	{
		GameWarning("Item '%s' trying to attach entity on '%s' which does not exist!", GetEntity()->GetName(), name);
		return;
	}

	CEntityAttachment *pEntityAttachment = new CEntityAttachment();
	pEntityAttachment->SetEntityId(pEntity->GetId());

	pAttachment->AddBinding(pEntityAttachment);
	pAttachment->HideAttachment(0);
}

//------------------------------------------------------------------------
void CItem::SetCharacterAttachment(int slot, const char *name, IStatObj *pObj, int flags)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return;

	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(name);

	if (!pAttachment)
	{
		GameWarning("Item '%s' trying to attach static object on '%s' which does not exist!", GetEntity()->GetName(), name);
		return;
	}

	CCGFAttachment *pStatAttachment = new CCGFAttachment();
	pStatAttachment->pObj  = pObj;

	pAttachment->AddBinding(pStatAttachment);
}

//------------------------------------------------------------------------
void CItem::SetCharacterAttachment(int slot, const char *name, ICharacterInstance *pAttachedCharacter, int flags)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return;

	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(name);

	if (!pAttachment)
	{
		GameWarning("Item '%s' trying to attach character on '%s' which does not exist!", GetEntity()->GetName(), name);
		return;
	}

	CCHRAttachment *pCharacterAttachment = new CCHRAttachment();
	pCharacterAttachment->m_pCharInstance  = pAttachedCharacter;

	// sub skin ?
	if (pAttachment->GetType() == CA_SKIN)
	{
		pAttachment->AddBinding(pCharacterAttachment);
	}
	else
	{
		pAttachment->AddBinding(pCharacterAttachment);
	}
}

//------------------------------------------------------------------------
void CItem::SetCharacterAttachment(int slot, const char *name, CDLight &light, int flags)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return;

	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(name);

	if (!pAttachment)
	{
		GameWarning("Item '%s' trying to attach light on '%s' which does not exist!", GetEntity()->GetName(), name);
		return;
	}

	CLightAttachment *pLightAttachment = new CLightAttachment();
	pLightAttachment->LoadLight(light);

	pAttachment->AddBinding(pLightAttachment);
	pAttachment->HideAttachment(0);
}

//------------------------------------------------------------------------
void CItem::SetCharacterAttachment(int slot, const char *name, IEntity *pEntity, int objSlot, int flags)
{
	SEntitySlotInfo info;
	if (!pEntity->GetSlotInfo(objSlot, info))
		return;

	if (info.pCharacter)
		SetCharacterAttachment(slot, name, info.pCharacter, flags);
	else if (info.pStatObj)
		SetCharacterAttachment(slot, name, info.pStatObj, flags);
}

//------------------------------------------------------------------------
void CItem::SetCharacterAttachmentLocalTM(int slot, const char *name, const Matrix34 &tm)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return;

	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(name);

	if (!pAttachment)
	{
		GameWarning("Item '%s' trying to set local TM on attachment '%s' which does not exist!", GetEntity()->GetName(), name);
		return;
	}

	pAttachment->SetAttRelativeDefault( QuatT(tm));
}

//------------------------------------------------------------------------
void CItem::SetCharacterAttachmentWorldTM(int slot, const char *name, const Matrix34 &tm)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return;

	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(name);

	if (!pAttachment)
	{
		GameWarning("Item '%s' trying to set world TM on attachment '%s' which does not exist!", GetEntity()->GetName(), name);
		return;
	}

//	Matrix34 boneWorldMatrix = GetEntity()->GetSlotWorldTM(slot) *	pCharacter->GetISkeleton()->GetAbsJMatrixByID(pAttachment->GetBoneID());
	Matrix34 boneWorldMatrix = GetEntity()->GetSlotWorldTM(slot) *	Matrix34(pCharacter->GetISkeletonPose()->GetAbsJointByID(pAttachment->GetBoneID()) );

	Matrix34 localAttachmentMatrix = (boneWorldMatrix.GetInverted()*tm);
	pAttachment->SetAttRelativeDefault(QuatT(localAttachmentMatrix));
}

//------------------------------------------------------------------------
Matrix34 CItem::GetCharacterAttachmentLocalTM(int slot, const char *name)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return Matrix34::CreateIdentity();;

	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(name);

	if (!pAttachment)
	{
		GameWarning("Item '%s' trying to get local TM on attachment '%s' which does not exist!", GetEntity()->GetName(), name);
		return Matrix34::CreateIdentity();
	}

	return Matrix34(pAttachment->GetAttRelativeDefault());
}

//------------------------------------------------------------------------
Matrix34 CItem::GetCharacterAttachmentWorldTM(int slot, const char *name)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return Matrix34::CreateIdentity();

	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(name);

	if (!pAttachment)
	{
		GameWarning("Item '%s' trying to get local TM on attachment '%s' which does not exist!", GetEntity()->GetName(), name);
		return Matrix34::CreateIdentity();
	}

	return Matrix34(pAttachment->GetAttWorldAbsolute());
}

//------------------------------------------------------------------------
void CItem::HideCharacterAttachment(int slot, const char *name, bool hide)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return;

	IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();
	IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(name);

	if (!pAttachment)
	{
		GameWarning("Item '%s' trying to hide attachment '%s' which does not exist!", GetEntity()->GetName(), name);
		return;
	}

	pAttachment->HideAttachment(hide?1:0);
}

//------------------------------------------------------------------------
void CItem::HideCharacterAttachmentMaster(int slot, const char *name, bool hide)
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (!pCharacter)
		return;

	pCharacter->HideMaster(hide?1:0);
}

//------------------------------------------------------------------------
void CItem::CreateAttachmentHelpers(int slot)
{
	for (THelperVector::iterator it = m_sharedparams->helpers.begin(); it != m_sharedparams->helpers.end(); it++)
	{
		if (it->slot != slot)
			continue;

		CreateCharacterAttachment(slot, it->name.c_str(), CA_BONE, it->bone.c_str());
	}

	if (slot == eIGS_FirstPerson)
		CreateCharacterAttachment(slot, ITEM_ARMS_ATTACHMENT_NAME, CA_SKIN, 0);
}

//------------------------------------------------------------------------
void CItem::DestroyAttachmentHelpers(int slot)
{
	for (THelperVector::iterator it = m_sharedparams->helpers.begin(); it != m_sharedparams->helpers.end(); ++it)
	{
		if (it->slot != slot)
			continue;

		DestroyCharacterAttachment(slot, it->name.c_str());
	}
}

//------------------------------------------------------------------------
const CItem::THelperVector& CItem::GetAttachmentHelpers()
{
	return m_sharedparams->helpers;
}

//------------------------------------------------------------------------
bool CItem::SetGeometry(int slot, const ItemString& name, const Vec3& poffset, const Ang3& aoffset, float scale, bool forceReload)
{
	bool changedfp=false;
	switch(slot)
	{
	case eIGS_Arms:
		{
			if (!name || forceReload)
			{
				GetEntity()->FreeSlot(slot);
#ifndef ITEM_USE_SHAREDSTRING
				m_geometry[slot].resize(0);
#else
				m_geometry[slot].reset();
#endif
			}

			ResetCharacterAttachment(eIGS_FirstPerson, ITEM_ARMS_ATTACHMENT_NAME);

			ICharacterInstance *pCharacter=0;

			if (name && name[0])
			{
				if (name != m_geometry[slot])
					GetEntity()->LoadCharacter(slot, name);
				DrawSlot(eIGS_Arms, false);

				pCharacter = GetEntity()->GetCharacter(eIGS_Arms);
			}
			else if (m_pForcedArms)
			{
				pCharacter = m_pForcedArms;
			}
			else
			{
				int armsId=m_stats.hand==eIH_Right?0:1;
				pCharacter = GetOwnerActor()?GetOwnerActor()->GetFPArms(armsId):0;
			}

			if (pCharacter)
			{
				pCharacter->SetFlags(pCharacter->GetFlags()&(~CS_FLAG_UPDATE));
				SetCharacterAttachment(eIGS_FirstPerson, ITEM_ARMS_ATTACHMENT_NAME, pCharacter, 0);
			}
		}
		break;
	case eIGS_FirstPerson:
	case eIGS_ThirdPerson:
	default:
		{
			if (!name || forceReload)
			{
				GetEntity()->FreeSlot(slot);
#ifndef ITEM_USE_SHAREDSTRING
				m_geometry[slot].resize(0);
#else
				m_geometry[slot].reset();
#endif
			}
	
			DestroyAttachmentHelpers(slot);

			if (name && name[0])
			{
				if (m_geometry[slot] != name)
				{
					const char* ext = PathUtil::GetExt(name.c_str());
					if ((stricmp(ext, "chr") == 0) || (stricmp(ext, "cdf") == 0) || (stricmp(ext, "cga") == 0) )
						GetEntity()->LoadCharacter(slot, name, 0);
					else
						GetEntity()->LoadGeometry(slot, name, 0, 0);

					changedfp=slot==eIGS_FirstPerson;
				}
				
				CreateAttachmentHelpers(slot);

				SetDefaultIdleAnimation(slot, g_pItemStrings->idle);
			}

			if (slot == eIGS_FirstPerson)
			{
				ICharacterInstance *pCharacter = GetEntity()->GetCharacter(eIGS_FirstPerson);
				if (pCharacter)
				{
					pCharacter->SetFlags(pCharacter->GetFlags()&(~CS_FLAG_UPDATE));
				}
			}
      else if (slot == eIGS_Destroyed)
        DrawSlot(eIGS_Destroyed, false);
		}
		break;
	}

	Matrix34 slotTM;
	slotTM = Matrix34::CreateRotationXYZ(aoffset);
	slotTM.Scale(Vec3(scale, scale, scale));
	slotTM.SetTranslation(poffset);
	GetEntity()->SetSlotLocalTM(slot, slotTM);

	if (changedfp && m_stats.mounted)
	{
		PlayAction(m_idleAnimation[eIGS_FirstPerson], 0, true);
		ForceSkinning(true);

		if (!m_mountparams.pivot.empty())
		{
			Matrix34 tm=GetEntity()->GetSlotLocalTM(eIGS_FirstPerson, false);
			Vec3 pivot = GetSlotHelperPos(eIGS_FirstPerson, m_mountparams.pivot.c_str(), false);
			tm.AddTranslation(pivot);

			GetEntity()->SetSlotLocalTM(eIGS_FirstPerson, tm);
		}

		GetEntity()->InvalidateTM();
	}

	m_geometry[slot] = name ? name : "";

	ReAttachAccessories();

	return true;
}

//------------------------------------------------------------------------
void CItem::SetDefaultIdleAnimation(int slot, const ItemString& actionName)
{
	TActionMap::iterator it = m_sharedparams->actions.find(CONST_TEMPITEM_STRING(actionName));
	if (it == m_sharedparams->actions.end())
	{
//		GameWarning("Action '%s' not found on item '%s'!", actionName, GetEntity()->GetName());
		return;
	}

	SAction &action = it->second;

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
	if (pCharacter)
	{
		if (action.animation[slot].size() > 0)
		{
			TempResourceName name;
			FixResourceName(action.animation[slot][0].name, name, 0);
		}
		//changed by ivo!
		//pCharacter->SetDefaultIdleAnimation(0, name.c_str());
	}
	m_idleAnimation[slot] = actionName;
}

//------------------------------------------------------------------------
void CItem::ForceSkinning(bool always)
{
	for (int slot=0; slot<eIGS_Last; slot++)
	{
		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
		if (pCharacter)
		{
			Matrix34 m34=GetEntity()->GetSlotWorldTM(slot);
			QuatT renderLocation = QuatT(m34);

			Vec3 CharOffset = GetEntity()->GetSlotLocalTM(slot,false).GetTranslation();

			pCharacter->GetISkeletonPose()->SetForceSkeletonUpdate(7);
			pCharacter->SkeletonPreProcess(renderLocation, renderLocation, GetISystem()->GetViewCamera(),0x55 );
			pCharacter->SkeletonPostProcess(renderLocation, renderLocation, 0, 0.0f, 0x55 );
			if (!always)
				pCharacter->GetISkeletonPose()->SetForceSkeletonUpdate(0);
		}
	}
}

//------------------------------------------------------------------------
void CItem::EnableHiddenSkinning(bool enable)
{
	/*
	for (int slot=0; slot<eIGS_Last; slot++)
	{
		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);
		if (pCharacter)
		{
			if (enable)
				pCharacter->SetFlags(pCharacter->GetFlags()|CS_FLAG_UPDATE_ALWAYS);
			else
				pCharacter->SetFlags(pCharacter->GetFlags()&(~CS_FLAG_UPDATE_ALWAYS));
		}
	}
	*/
}

//------------------------------------------------------------------------
void CItem::FixResourceName(const ItemString& inName, TempResourceName& name, int flags, const char *hand, const char *suffix, const char *pose, const char *pov, const char *env)
{
	// the whole thing of fixing is not nice, but at least we don't allocate too often
	// StringHelper<TempResourceName::SIZE> name (inName.c_str(), inName.length());
	name.assign(inName.c_str(), inName.length());

	if (!hand)
	{
		if (m_stats.hand == eIH_Left)
			hand = "left";
		else
			hand = "right";
	}
	name.replace("%hand%", hand);

	if (m_stats.hand == eIH_Left)
		name.replace("%offhand%", "right");
	else
		name.replace("%offhand%", "left");

	if (!suffix)
		suffix = m_actionSuffix.c_str();
	name.replace("%suffix%", suffix);

	if (!pose)
	{
		if (!m_params.pose.empty())
			pose = m_params.pose.c_str();
		else
			pose = "";
	}
	name.replace("%pose%", "");

	if (!pov)
	{
		if ((m_stats.fp || flags&eIPAF_ForceFirstPerson) && !(flags&eIPAF_ForceThirdPerson))
			pov = ITEM_FIRST_PERSON_TOKEN;
		else
			pov = ITEM_THIRD_PERSON_TOKEN;
	}
	name.replace("%pov%", pov);

	if (!env)
	{
		// Instead if the weapons sound proxy, the owners is used to retrieve the tail name
		IEntity* pOwner = GetOwner();
		if (GetIWeapon() && pOwner) // restricting to weapon sounds only
		{
			if (pOwner)
			{
				IEntitySoundProxy *pSoundProxy = (IEntitySoundProxy *)pOwner->GetProxy(ENTITY_PROXY_SOUND);

				if (!pSoundProxy)
					pSoundProxy = (IEntitySoundProxy *)pOwner->CreateProxy(ENTITY_PROXY_SOUND);

				if (pSoundProxy)
				{
					// check for a roof 10m above the Owner
					// recalculate visibility when owner move more than 2 meters
					pSoundProxy->CheckVisibilityForTailName(10.0f, 2.0f);
					env = pSoundProxy->GetTailName();
				}
			}
		}


		if (!env || !env[0] || !stricmp("indoor", env))
			name.replace("%env%", "");
		else
		{
			static const size_t MAX_LEN = 256;
			char envstr[MAX_LEN];
			envstr[0] = '_';
			strncpy(envstr+1, env, MAX_LEN-1); // no 0 pad, if MAX_LEN-1 are copied
			envstr[MAX_LEN-1] = '\0'; // always zero-terminate
			name.replace("%env%", envstr);
		}
	}
	else
		name.replace("%env%", env);
}

//------------------------------------------------------------------------
tSoundID CItem::PlayAction(const ItemString& actionName, int layer, bool loop, uint flags, float speedOverride)
{
	if (!m_enableAnimations)
		return -1;

	TActionMap::iterator it = m_sharedparams->actions.find(CONST_TEMPITEM_STRING(actionName));
	if (it == m_sharedparams->actions.end())
	{
//		GameWarning("Action '%s' not found on item '%s'!", actionName, GetEntity()->GetName());

		for (int i=0;i<eIGS_Last;i++)
		{
			m_animationTime[i]=0;
			m_animationSpeed[i]=1.0f;
			m_animationEnd[i]=0;
		}
		return 0;
	}

	bool fp = m_stats.fp;
	
	if (m_parentId)
	{
		CItem *pParent=static_cast<CItem *>(m_pItemSystem->GetItem(m_parentId));
		if (pParent)
			fp=pParent->GetStats().fp;
	}
	
	if (flags&eIPAF_ForceFirstPerson)
		fp = true;
	if (flags&eIPAF_ForceThirdPerson)
		fp = false;

	int sid=fp?eIGS_FirstPerson:eIGS_ThirdPerson;
	SAction &action = it->second;
	
	tSoundID result = INVALID_SOUNDID;
	if ((flags&eIPAF_Sound) && !action.sound[sid].name.empty() && IsSoundEnabled() && g_pGameCVars->i_soundeffects)
	{
		int nSoundFlags = FLAG_SOUND_DEFAULT_3D;
		nSoundFlags |= flags&eIPAF_SoundStartPaused?FLAG_SOUND_START_PAUSED:0;
		IEntitySoundProxy *pSoundProxy = GetSoundProxy(true);

		//GetSound proxy from dualwield master if neccesary
		if(IsDualWieldSlave())
		{
			CItem* pMaster = static_cast<CItem*>(GetDualWieldMaster());
			if(pMaster)
			{
				pSoundProxy = pMaster->GetSoundProxy(true);
			}
		}

		EntityId pSkipEnts[3];
		int nSkipEnts = 0;

		// TODO for Marcio :)
		// check code changes

		// Skip the Item
		pSkipEnts[nSkipEnts] = GetEntity()->GetId();
		++nSkipEnts;

		// Skip the Owner
		if (GetOwner())
		{
			pSkipEnts[nSkipEnts] = GetOwner()->GetId();
			++nSkipEnts;
		}

		if (pSoundProxy)
		{
			
			TempResourceName name;
			FixResourceName(action.sound[sid].name, name, flags);
			//nSoundFlags = nSoundFlags | (fp?FLAG_SOUND_DEFAULT_3D|FLAG_SOUND_RELATIVE:FLAG_SOUND_DEFAULT_3D);
			Vec3 vOffset(0,0,0);
			if (fp)
				vOffset.x = 0.3f; // offset for first person weapon to the front

			if (!g_pGameCVars->i_staticfiresounds)
			{
				result = pSoundProxy->PlaySoundEx(name, vOffset, FORWARD_DIRECTION, nSoundFlags, 1.0f, 0, 0, eSoundSemantic_Weapon, pSkipEnts, nSkipEnts);
				ISound *pSound = pSoundProxy->GetSound(result);

				if (pSound && action.sound[sid].sphere>0.0f)
					pSound->SetSphereSpec(action.sound[sid].sphere);
			}
			else
			{
				SInstanceAudio *pInstanceAudio=0;

				if (action.sound[sid].isstatic)
				{
					TInstanceActionMap::iterator iit = m_instanceActions.find(CONST_TEMPITEM_STRING(actionName));
					if (iit == m_instanceActions.end())
					{
						std::pair<TInstanceActionMap::iterator, bool> insertion=m_instanceActions.insert(TInstanceActionMap::value_type(actionName, SInstanceAction()));
						pInstanceAudio=&insertion.first->second.sound[sid];
					}
					else
						pInstanceAudio=&iit->second.sound[sid];
				}

				if (pInstanceAudio && (pInstanceAudio->id != INVALID_SOUNDID) && (name != pInstanceAudio->static_name))
					ReleaseStaticSound(pInstanceAudio);

				if (!pInstanceAudio || pInstanceAudio->id == INVALID_SOUNDID)
				{
          result = pSoundProxy->PlaySoundEx(name, vOffset, FORWARD_DIRECTION, nSoundFlags, 1.0f, 0, 0, eSoundSemantic_Weapon, pSkipEnts, nSkipEnts);
					ISound *pSound = pSoundProxy->GetSound(result);
					
					if (pSound && action.sound[sid].sphere>0.0f)
						pSound->SetSphereSpec(action.sound[sid].sphere);
				}

				if (action.sound[sid].isstatic)
				{
					if (pInstanceAudio->id == INVALID_SOUNDID)
					{
						if(pSoundProxy->SetStaticSound(result, true))
						{
							pInstanceAudio->id = result;
							pInstanceAudio->static_name = name;
							pInstanceAudio->synch = action.sound[sid].issynched;
						}
					}
					else
					{
						ISound *pSound = pSoundProxy->GetSound(pInstanceAudio->id);
						if (pSound)
							pSound->Play(1.0, true, true, pSoundProxy);
					}
				}
			}
			
			if (action.sound[sid].airadius > 0.0f)
			{
				IEntity	*pOwner = GetOwner();
				// associate sound event with vehicle if the shooter is in a vehicle (tank cannon shot, etc)
				IAIObject	*pAIOwner = pOwner ? pOwner->GetAI() : NULL;
				CActor *pOwnerActor = GetOwnerActor();
				if(pOwnerActor)
				{
					IVehicle* pOvnerVehicle = pOwnerActor->GetLinkedVehicle();
					if(pOvnerVehicle && pOvnerVehicle->GetEntity() && pOvnerVehicle->GetEntity()->GetAI())
						pAIOwner = pOvnerVehicle->GetEntity()->GetAI();
				}
				if (gEnv->pAISystem)
					gEnv->pAISystem->SoundEvent(GetEntity()->GetWorldPos(), action.sound[sid].airadius, AISE_WEAPON, pAIOwner);
			}
		}
	}


	if (flags&eIPAF_Animation)
	{
		TempResourceName name;
		// generate random number only once per call to allow animations to
		// match across geometry slots (like first person and third person)
		float randomNumber = Random();
		for (int i=0; i<eIGS_Last; i++)
		{
			if (!(flags&(1<<i)))
				continue;
			int nanimations=action.animation[i].size();
			if (nanimations <= 0)
				continue;
			int anim = int( randomNumber * float(nanimations) );
			if (action.animation[i][anim].name.empty())
				continue;

			FixResourceName(action.animation[i][anim].name, name, flags);

			if ((i == eIGS_Owner) || (i == eIGS_OwnerLooped))
			{
				if (!action.animation[i][anim].name.empty())
				{
					bool looping=(eIGS_OwnerLooped==i);

					CActor *pOwner = GetOwnerActor();
					if (pOwner)
					{
						if (IsDualWield() && !m_params.dual_wield_pose.empty())
							pOwner->PlayAction(name, m_params.dual_wield_pose.c_str(), looping);
						else
							pOwner->PlayAction(name, m_params.pose.c_str(), looping);
					}
				}
				continue;
			}
			else if (i == eIGS_OffHand)
			{
				if (!action.animation[eIGS_OffHand][anim].name.empty())
				{
					CActor *pOwner = GetOwnerActor();
					if (pOwner)
					{
						CItem *pOffHand = pOwner->GetItemByClass(CItem::sOffHandClass);
						if (pOffHand && pOffHand!=this)
						{
							uint ohflags=eIPAF_Default;
							if (action.animation[eIGS_OffHand][anim].blend==0.0f)
								ohflags|=eIPAF_NoBlend;
							pOffHand->PlayAction(action.animation[eIGS_OffHand][anim].name, 0, false, ohflags);
						}
					}
				}

				continue;
			}

			SAnimation &animation=action.animation[i][anim];
			if (!animation.name.empty())
			{
				float blend = animation.blend;
				if (flags&eIPAF_NoBlend)
					blend = 0.0f;
				if (speedOverride > 0.0f)
					PlayAnimationEx(name, i, layer, loop, blend, speedOverride, flags);
				else
					PlayAnimationEx(name, i, layer, loop, blend, animation.speed, flags);
			}

			if ((m_stats.fp || m_stats.viewmode&eIVM_FirstPerson) && i==eIGS_FirstPerson && !animation.camera_helper.empty())
			{
				m_camerastats.animating=true;
				m_camerastats.helper=animation.camera_helper;
				m_camerastats.position=animation.camera_pos;
				m_camerastats.rotation=animation.camera_rot;
				m_camerastats.follow=animation.camera_follow;
				m_camerastats.reorient=animation.camera_reorient;
			}
			else if (m_camerastats.animating)
				m_camerastats=SCameraAnimationStats();
		}
	}

  if (flags&eIPAF_Effect && !action.effect[sid].name.empty())
  {
    // change this to attach, if needed
    SpawnEffect(sid, action.effect[sid].name.c_str(), action.effect[sid].helper.c_str());
  }

	if (action.children)
	{
		for (TAccessoryMap::iterator ait=m_accessories.begin(); ait!=m_accessories.end(); ait++)
		{
			EntityId aId=(EntityId)ait->second;
			CItem *pAccessory=static_cast<CItem *>(m_pItemSystem->GetItem(aId));
			if (pAccessory)
				pAccessory->PlayAction(actionName, layer, loop, flags, speedOverride);
		}
	}

	return result;
}

//------------------------------------------------------------------------
void CItem::PlayAnimation(const char* animationName, int layer, bool loop, uint flags)
{
	for (int i=0; i<eIGS_Last; i++)
	{
		if (!(flags&1<<i))
			continue;

		PlayAnimationEx(animationName, i, layer, loop, 0.175f, 1.0f, flags);
	}
}

//------------------------------------------------------------------------
void CItem::PlayAnimationEx(const char* animationName, int slot, int layer, bool loop, float blend, float speed, uint flags)
{
	bool start=true;

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(slot);

	if (!pCharacter && slot==eIGS_FirstPerson && ((m_stats.viewmode&eIVM_FirstPerson)==0))
	{
		start=false;

		int idx = 0;
		if (m_stats.hand == eIH_Right)
			idx = 1;
		else if (m_stats.hand == eIH_Left)
			idx = 2;
		if (m_fpgeometry[idx].name.empty())
			idx = 0;

		pCharacter = m_pItemSystem->GetCachedCharacter(m_fpgeometry[idx].name.c_str());
	}

	if (pCharacter && animationName)
	{
		ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();

		if (flags&eIPAF_CleanBlending)
		{
			while(pSkeletonAnim->GetNumAnimsInFIFO(layer)>1)
			{
				if (!pSkeletonAnim->RemoveAnimFromFIFO(layer, pSkeletonAnim->GetNumAnimsInFIFO(layer)-1))
					break;
			}
		}

		if (flags&eIPAF_NoBlend)
			blend = 0.0f;

		if (start)
		{
			CryCharAnimationParams params;
			params.m_fTransTime = blend;
			params.m_nLayerID = layer;
			params.m_nFlags = (loop?CA_LOOP_ANIMATION:0)|(flags&eIPAF_RestartAnimation?CA_ALLOW_ANIM_RESTART:0)|(flags&eIPAF_RepeatLastFrame?CA_REPEAT_LAST_KEY:0);
			pSkeletonAnim->StartAnimation(animationName, 0, 0,0,  params);
			pSkeletonAnim->SetLayerUpdateMultiplier(layer, speed);

			//pCharacter->GetISkeleton()->SetDebugging( true );
		}

		float duration=0.0f;
		int animationId = pCharacter->GetIAnimationSet()->GetAnimIDByName(animationName);
		if (animationId>=0)
			duration = pCharacter->GetIAnimationSet()->GetDuration_sec(animationId);
		
		m_animationTime[slot] = (uint)(duration*1000.0f/speed);
		m_animationEnd[slot] = (uint)(gEnv->pTimer->GetCurrTime()*1000.0f)+m_animationTime[slot];
		m_animationSpeed[slot] = speed;
	}
}



//------------------------------------------------------------------------
void CItem::PlayLayer(const ItemString& layerName, int flags, bool record)
{
	TLayerMap::iterator it = m_sharedparams->layers.find(CONST_TEMPITEM_STRING(layerName));
	if (it == m_sharedparams->layers.end())
		return;

	TempResourceName tempResourceName;
	for (int i=0; i<eIGS_Last; i++)
	{
		if (!(flags&1<<i))
			continue;

		SLayer &layer = it->second;

		if (!layer.name[i].empty())
		{
			ICharacterInstance *pCharacter = GetEntity()->GetCharacter(i);
			if (pCharacter)
			{
				CryCharAnimationParams params;
				float blend = 0.125f;
				if (flags&eIPAF_NoBlend)
					blend = 0.0f;
				params.m_fTransTime = blend;
				params.m_fLayerBlendIn = 0;
				params.m_nLayerID = layer.id[i];
				params.m_nFlags = CA_LOOP_ANIMATION;
				
				FixResourceName(layer.name[i], tempResourceName, flags);

				ISkeletonAnim* pSkeletonAnim=pCharacter->GetISkeletonAnim();
			//	pSkeleton->SetRedirectToLayer0(1);
				pSkeletonAnim->StartAnimation(tempResourceName, 0, 0,0,  params);

				if (layer.bones.empty())
				{
					pCharacter->GetISkeletonPose()->SetLayerMask(layer.id[i], 1);
				}
				else
				{
					pCharacter->GetISkeletonPose()->SetLayerMask(layer.id[i], 0);
					for (std::vector<ItemString>::iterator bit = layer.bones.begin(); bit != layer.bones.end(); bit++)
						pCharacter->GetISkeletonPose()->SetJointMask(bit->c_str(), layer.id[i], 1);
				}
			}
		}
	}

	if (record)
	{
		TActiveLayerMap::iterator ait = m_activelayers.find(CONST_TEMPITEM_STRING(layerName));
		if (ait == m_activelayers.end())
			m_activelayers.insert(TActiveLayerMap::value_type(layerName, flags));
	}
}

//------------------------------------------------------------------------
void CItem::StopLayer(const ItemString& layerName, int flags, bool record)
{
	TLayerMap::iterator it = m_sharedparams->layers.find(CONST_TEMPITEM_STRING(layerName));
	if (it == m_sharedparams->layers.end())
		return;

	for (int i=0; i<eIGS_Last; i++)
	{
		if (!(flags&1<<i))
			continue;

		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(i);
		if (pCharacter)
			pCharacter->GetISkeletonAnim()->StopAnimationInLayer(it->second.id[i],0.0f);
	}

	if (record)
	{
		TActiveLayerMap::iterator ait = m_activelayers.find(CONST_TEMPITEM_STRING(layerName));
		if (ait != m_activelayers.end())
			m_activelayers.erase(ait);
	}
}

//------------------------------------------------------------------------
void CItem::RestoreLayers()
{
	for (TActiveLayerMap::iterator it = m_activelayers.begin(); it != m_activelayers.end(); it++)
		PlayLayer(it->first, it->second, false);

	for (TLayerMap::iterator lit = m_sharedparams->layers.begin(); lit != m_sharedparams->layers.end(); lit++)
	{
		if (lit->second.isstatic)
			PlayLayer(lit->first, eIPAF_Default, false);
	}
}

//------------------------------------------------------------------------
void CItem::ResetAnimation(int layer, uint flags)
{
	for (int i=0; i<eIGS_Last; i++)
	{
		if (!(flags&1<<i))
			continue;

		if ((i == eIGS_Owner) || (i == eIGS_OwnerLooped))
			continue;

		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(i);
		if (pCharacter)
			pCharacter->GetISkeletonAnim()->StopAnimationsAllLayers();
	}
}

//------------------------------------------------------------------------
uint CItem::GetCurrentAnimationTime(int slot)
{
	return m_animationTime[slot];
}

//------------------------------------------------------------------------
uint CItem::GetCurrentAnimationEnd(int slot)
{
	return m_animationEnd[slot];
}

//------------------------------------------------------------------------
uint CItem::GetCurrentAnimationStart(int slot)
{
	return m_animationEnd[slot]-m_animationTime[slot];
}

//------------------------------------------------------------------------
void CItem::DrawSlot(int slot, bool bDraw, bool bNear)
{
	uint flags = GetEntity()->GetSlotFlags(slot);
	if (bDraw)
		flags |= ENTITY_SLOT_RENDER;
	else
		flags &= ~ENTITY_SLOT_RENDER;

	if (bNear)
		flags |= ENTITY_SLOT_RENDER_NEAREST;
	else
		flags &= ~ENTITY_SLOT_RENDER_NEAREST;
	
	GetEntity()->SetSlotFlags(slot, flags);
}

//------------------------------------------------------------------------
Vec3 CItem::GetSlotHelperPos(int slot, const char *helper, bool worldSpace, bool relative)
{
	Vec3 position(0,0,0);

	// if mounted force the slot to be 1st person
	if (m_stats.mounted)
		slot=eIGS_FirstPerson;

	SEntitySlotInfo info;
	if (GetEntity()->GetSlotInfo(slot, info))
	{
		if (info.pStatObj)
		{
			IStatObj *pStatsObj = info.pStatObj;
			position = pStatsObj->GetHelperPos(helper);
			position = GetEntity()->GetSlotLocalTM(slot, false).TransformPoint(position);
		}
		else if (info.pCharacter)
		{
			ICharacterInstance *pCharacter = info.pCharacter;
			int16 id = pCharacter->GetISkeletonPose()->GetJointIDByName(helper);
			if (id > -1)
			{
				if (relative)
					position = pCharacter->GetISkeletonPose()->GetRelJointByID(id).t;
				else
					position = pCharacter->GetISkeletonPose()->GetAbsJointByID(id).t;
			}

			if (!relative)
				position = GetEntity()->GetSlotLocalTM(slot, false).TransformPoint(position);
		}
	}

	if (worldSpace)
		return GetEntity()->GetWorldTM().TransformPoint(position);

	return position;
}

//------------------------------------------------------------------------
const Matrix33 &CItem::GetSlotHelperRotation(int slot, const char *helper, bool worldSpace, bool relative)
{
	// if mounted force the slot to be 1st person
	if (m_stats.mounted)
		slot=eIGS_FirstPerson;

	static Matrix33 rotation;
	rotation.SetIdentity();

	IEntity* pEntity = GetEntity();
	if(!pEntity)
		return rotation;

	SEntitySlotInfo info;
	if (pEntity->GetSlotInfo(slot, info))
	{
    if (info.pStatObj)
    {
      IStatObj *pStatObj = info.pStatObj;
      rotation = Matrix33(pStatObj->GetHelperTM(helper));
      rotation.OrthonormalizeFast();
      rotation = Matrix33(GetEntity()->GetSlotLocalTM(slot, false))*rotation;        
    }
		else if (info.pCharacter)
		{
			ICharacterInstance *pCharacter = info.pCharacter;
			if(!pCharacter)
				return rotation;
			int16 id = pCharacter->GetISkeletonPose()->GetJointIDByName(helper);
		//	if (id > -1) rotation = Matrix33(pCharacter->GetISkeleton()->GetAbsJMatrixByID(id));
			if (id > -1)
			{
				if (relative)
					rotation = Matrix33(pCharacter->GetISkeletonPose()->GetRelJointByID(id).q);
				else
					rotation = Matrix33(pCharacter->GetISkeletonPose()->GetAbsJointByID(id).q);
			}

			if (!relative)
				rotation = Matrix33(pEntity->GetSlotLocalTM(slot, false))*rotation;
		}    
	}

	if (worldSpace)
		rotation=Matrix33(pEntity->GetWorldTM())*rotation;

	return rotation;
}

//------------------------------------------------------------------------
void CItem::StopSound(tSoundID id)
{
  if (id == INVALID_SOUNDID)
    return;

	bool synchSound = false;
	IEntitySoundProxy *pSoundProxy = GetSoundProxy(false);
	if (pSoundProxy)
	{
		for (TInstanceActionMap::iterator it = m_instanceActions.begin(); it != m_instanceActions.end(); ++it)
		{
			SInstanceAction &action = it->second;
			for (int i=0;i<2;i++)
			{
				if (action.sound[i].id == id)
				{
					pSoundProxy->SetStaticSound(id, false);
					action.sound[i].id = INVALID_SOUNDID;
					synchSound = action.sound[i].synch;
					break;
				}
			}
		}
		if(synchSound)
			pSoundProxy->StopSound(id, ESoundStopMode_OnSyncPoint);
		else
			pSoundProxy->StopSound(id);
	}
}

//------------------------------------------------------------------------
void CItem::Quiet()
{
	IEntitySoundProxy *pSoundProxy = GetSoundProxy(false);
	if (pSoundProxy)
	{
		for (TInstanceActionMap::iterator it = m_instanceActions.begin(); it != m_instanceActions.end(); ++it)
		{
			SInstanceAction &action = it->second;
			for (int i=0;i<2;i++)
			{
				if (action.sound[i].id != INVALID_SOUNDID)
				{
					pSoundProxy->SetStaticSound(action.sound[i].id, false);
					action.sound[i].id = INVALID_SOUNDID;
				}
			}
		}

		pSoundProxy->StopAllSounds();
	}
}

//------------------------------------------------------------------------
ISound *CItem::GetISound(tSoundID id)
{
	IEntitySoundProxy *pSoundProxy = GetSoundProxy(false);
	if (pSoundProxy)
		return pSoundProxy->GetSound(id);

	return 0;
}

//------------------------------------------------------------------------
void CItem::ReleaseStaticSound(SInstanceAudio *sound)
{
	if (sound->id != INVALID_SOUNDID)
	{
		IEntitySoundProxy *pSoundProxy = GetSoundProxy(false);
		if (pSoundProxy)
		{
			pSoundProxy->SetStaticSound(sound->id, false);
			if(sound->synch)
				pSoundProxy->StopSound(sound->id,ESoundStopMode_OnSyncPoint);
			else
				pSoundProxy->StopSound(sound->id);
			sound->id = INVALID_SOUNDID;
#ifndef ITEM_USE_SHAREDSTRING
			sound->static_name.resize(0);
#else
			sound->static_name.reset();
#endif
		}
	}
}

//------------------------------------------------------------------------
void CItem::ReleaseStaticSounds()
{
	for (TInstanceActionMap::iterator it = m_instanceActions.begin(); it != m_instanceActions.end(); ++it)
	{
		ReleaseStaticSound(&it->second.sound[0]);
		ReleaseStaticSound(&it->second.sound[1]);
	}
}

//------------------------------------------------------------------------
IEntitySoundProxy *CItem::GetSoundProxy(bool create)
{
	IEntitySoundProxy *pSoundProxy = (IEntitySoundProxy *)GetEntity()->GetProxy(ENTITY_PROXY_SOUND);
	if (!pSoundProxy && create)
		pSoundProxy = (IEntitySoundProxy *)GetEntity()->CreateProxy(ENTITY_PROXY_SOUND);

	return pSoundProxy;
}

//------------------------------------------------------------------------
IEntityRenderProxy *CItem::GetRenderProxy(bool create)
{
	IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy *)GetEntity()->GetProxy(ENTITY_PROXY_RENDER);
	if (!pRenderProxy && create)
		pRenderProxy = (IEntityRenderProxy *)GetEntity()->CreateProxy(ENTITY_PROXY_RENDER);

	return pRenderProxy;
}

//------------------------------------------------------------------------
IEntityPhysicalProxy *CItem::GetPhysicalProxy(bool create)
{
	IEntityPhysicalProxy *pPhysicalProxy = (IEntityPhysicalProxy *)GetEntity()->GetProxy(ENTITY_PROXY_PHYSICS);
	if (!pPhysicalProxy && create)
		pPhysicalProxy = (IEntityPhysicalProxy *)GetEntity()->CreateProxy(ENTITY_PROXY_PHYSICS);

	return pPhysicalProxy;
}

//------------------------------------------------------------------------
void CItem::DestroyedGeometry(bool use)
{
  if (!m_geometry[eIGS_Destroyed].empty())
  {    
    DrawSlot(eIGS_Destroyed, use);
		if (m_stats.viewmode&eIVM_FirstPerson)
			DrawSlot(eIGS_FirstPerson, !use);
		else
			DrawSlot(eIGS_ThirdPerson, !use);

    if (use)
      GetEntity()->SetSlotLocalTM(eIGS_Destroyed, GetEntity()->GetSlotLocalTM(eIGS_ThirdPerson, false));
  }  
}
