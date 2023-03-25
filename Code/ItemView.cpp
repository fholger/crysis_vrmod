/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 30:8:2005   12:52 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "Actor.h"
#include "Player.h"
#include "Alien.h"
#include "GameCVars.h"
#include <IViewSystem.h>


//------------------------------------------------------------------------
void CItem::UpdateFPView(float frameTime)
{ 
	if (!m_stats.selected)
		return;

	CheckViewChange();

	if (!m_stats.fp && !m_stats.mounted)
		return;

	if (GetGameObject()->GetAspectProfile(eEA_Physics)!=eIPhys_NotPhysicalized)
		return;

	if (m_camerastats.animating)
	{
		if (m_camerastats.position)
			m_camerastats.pos=GetSlotHelperPos(eIGS_FirstPerson, m_camerastats.helper.c_str(), false, true);

		if (m_camerastats.rotation)
			m_camerastats.rot=Quat(GetSlotHelperRotation(eIGS_FirstPerson, m_camerastats.helper.c_str(), false, true)); //*Quat::CreateRotationZ(-gf_PI);
	}
	
	if (!m_stats.mounted)
	{
		UpdateFPPosition(frameTime);
		UpdateFPCharacter(frameTime);
	}

	if (IItem *pSlave = GetDualWieldSlave())
		pSlave->UpdateFPView(frameTime);

	//UpdateMounted() is only updated in CItem::Update()
	//if (m_stats.mounted && GetOwnerActor() && GetOwnerActor()->IsClient())
		//UpdateMounted(frameTime);
}

//------------------------------------------------------------------------
void CItem::UpdateFPPosition(float frameTime)
{
	CActor* pActor = GetOwnerActor();
	if (!pActor)
		return;

	SPlayerStats *pStats = static_cast<SPlayerStats *>(pActor->GetActorStats());
	if (!pStats)
		return;

	Matrix34 tm = Matrix33::CreateRotationXYZ(pStats->FPWeaponAngles);

	Vec3 offset(0.0f,0.0f,0.0f);

	float right(g_pGameCVars->i_offset_right);
	float front(g_pGameCVars->i_offset_front);
	float up(g_pGameCVars->i_offset_up);

	if (front!=0.0f || up!=0.0f || right!=0.0f)
	{
		offset += tm.GetColumn(0).GetNormalized() * right;
		offset += tm.GetColumn(1).GetNormalized() * front;
		offset += tm.GetColumn(2).GetNormalized() * up;
	}

	tm.SetTranslation(pStats->FPWeaponPos + offset);
	GetEntity()->SetWorldTM(tm);

	//CryLogAlways("weaponpos: %.3f,%.3f,%.3f // weaponrot: %.3f,%.3f,%.3f", tm.GetTranslation().x,tm.GetTranslation().y,tm.GetTranslation().z, pStats->FPWeaponAngles.x, pStats->FPWeaponAngles.y, pStats->FPWeaponAngles.z);
}

//------------------------------------------------------------------------
void CItem::UpdateFPCharacter(float frameTime)
{
	if (IsClient())
	{
		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(eIGS_FirstPerson);

		if (pCharacter && !m_idleAnimation[eIGS_FirstPerson].empty() && pCharacter->GetISkeletonAnim()->GetNumAnimsInFIFO(0)<1)
			PlayAction(m_idleAnimation[eIGS_FirstPerson], 0, true);
	}

	// need to explicitly update characters at this point
	// cause the entity system update occered earlier, with the last position
	for (int i=0; i<eIGS_Last; i++)
	{
		if (GetEntity()->GetSlotFlags(i)&ENTITY_SLOT_RENDER)
		{
			ICharacterInstance *pCharacter = GetEntity()->GetCharacter(i);
			if (pCharacter)
			{
				Matrix34 mloc = GetEntity()->GetSlotLocalTM(i,false);
				Matrix34 m34=GetEntity()->GetWorldTM()*mloc;
				QuatT renderLocation = QuatT(m34);
				pCharacter->GetISkeletonPose()->SetForceSkeletonUpdate(8);
				pCharacter->SkeletonPreProcess(renderLocation, renderLocation, GetISystem()->GetViewCamera(),0x55 );
				pCharacter->SkeletonPostProcess(renderLocation, renderLocation, 0, 1.0f, 0x55 );
			}
		}
	}

	IEntityRenderProxy *pProxy=GetRenderProxy();
	if (pProxy)
		pProxy->InvalidateLocalBounds();
}

//------------------------------------------------------------------------
bool CItem::FilterView(struct SViewParams &viewParams)
{
	if (m_camerastats.animating && m_camerastats.follow)
	{
		const Matrix34 tm = GetEntity()->GetSlotWorldTM(eIGS_FirstPerson);
		Vec3 offset(0.0f,0.0f,0.0f);

		offset += tm.GetColumn(0).GetNormalized()*m_camerastats.pos.x;
		offset += tm.GetColumn(1).GetNormalized()*m_camerastats.pos.y;
		offset += tm.GetColumn(2).GetNormalized()*m_camerastats.pos.z;

		viewParams.position+=offset;
		viewParams.rotation*=m_camerastats.rot;
		viewParams.blend=true;
		viewParams.viewID=5;
	}

	return m_camerastats.reorient;
}

//------------------------------------------------------------------------
void CItem::PostFilterView(struct SViewParams &viewParams)
{
	if (m_camerastats.animating && !m_camerastats.follow)
	{
		const Matrix34 tm = GetEntity()->GetSlotWorldTM(eIGS_FirstPerson);
		Vec3 offset(0.0f,0.0f,0.0f);

		offset += tm.GetColumn(0).GetNormalized()*m_camerastats.pos.x;
		offset += tm.GetColumn(1).GetNormalized()*m_camerastats.pos.y;
		offset += tm.GetColumn(2).GetNormalized()*m_camerastats.pos.z;

		viewParams.position+=offset;
		viewParams.rotation*=m_camerastats.rot;
		viewParams.blend=true;
		viewParams.viewID=5;
	}

	if (m_camerastats.animating && m_stats.mounted && !m_camerastats.helper.empty() && IsOwnerFP())
	{
		viewParams.position = GetSlotHelperPos(eIGS_FirstPerson, m_camerastats.helper, true);
		viewParams.rotation = Quat(GetSlotHelperRotation(eIGS_FirstPerson, m_camerastats.helper, true));    
		viewParams.blend = true;
		viewParams.viewID=5;

		viewParams.nearplane = 0.1f;
	}
}

//------------------------------------------------------------------------
bool CItem::IsOwnerFP()
{
	CActor *pOwner = GetOwnerActor();
	if (!pOwner)
		return false;

	if (m_pGameFramework->GetClientActor() != pOwner)
		return false;

	return !pOwner->IsThirdPerson();
}

//------------------------------------------------------------------------
bool CItem::IsCurrentItem()
{
	CActor *pOwner = GetOwnerActor();
	if (!pOwner)
		return false;

	if (pOwner->GetCurrentItem() == this)
		return true;

	return false;
}

//------------------------------------------------------------------------
void CItem::UpdateMounted(float frameTime)
{
	IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

	if (!m_ownerId || !m_stats.mounted)
		return;

	CActor *pActor = GetOwnerActor();
	if (!pActor)
		return;

	CheckViewChange();

  if (true)
  {  
	  if (IsClient())
	  {
		  ICharacterInstance *pCharacter = GetEntity()->GetCharacter(eIGS_FirstPerson);

		  if (pCharacter && !m_idleAnimation[eIGS_FirstPerson].empty() && pCharacter->GetISkeletonAnim()->GetNumAnimsInFIFO(0)<1)
			  PlayAction(m_idleAnimation[eIGS_FirstPerson], 0, true);
	  }

		// need to explicitly update characters at this point
		// cause the entity system update occered earlier, with the last position
		for (int i=0; i<eIGS_Last; i++)
		{
			if (GetEntity()->GetSlotFlags(i)&ENTITY_SLOT_RENDER)
			{
				ICharacterInstance *pCharacter = GetEntity()->GetCharacter(i);
				if (pCharacter)
				{
					Matrix34 mloc = GetEntity()->GetSlotLocalTM(i,false);
					Matrix34 m34 = GetEntity()->GetWorldTM()*mloc;
					QuatT renderLocation = QuatT(m34);
					pCharacter->GetISkeletonPose()->SetForceSkeletonUpdate(9);
					pCharacter->SkeletonPreProcess(renderLocation, renderLocation, GetISystem()->GetViewCamera(),0x55 );
					pCharacter->SkeletonPostProcess(renderLocation, renderLocation, 0, 0.0f, 0x55 );		
				}
			}
		}

	//	f32 fColor[4] = {1,1,0,1};
	//	f32 g_YLine=60.0f;
	//	gEnv->pRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false, "Mounted Gun Code" ); 

		//adjust the orientation of the gun based on the aim-direction


		SMovementState info;
		IMovementController* pMC = pActor->GetMovementController();		
		pMC->GetMovementState(info);
		Matrix34 tm = Matrix33::CreateRotationVDir(info.aimDirection.GetNormalized());
		Vec3 vGunXAxis=tm.GetColumn0();

		if (pActor->GetLinkedVehicle()==0)
		{
			if (IMovementController * pMC = pActor->GetMovementController())
			{
				SMovementState info;
				pMC->GetMovementState(info);
				
				Vec3 dir = info.aimDirection.GetNormalized();
				
				if(!pActor->IsPlayer())
				{
					// prevent snapping direction
					Vec3 currentDir = GetEntity()->GetWorldRotation().GetColumn1();
					float dot = currentDir.Dot(dir);
					dot = CLAMP(dot,-1,1);
					float reqAngle = cry_acosf(dot);
					const float maxRotSpeed = 2.0f;
					float maxAngle = frameTime * maxRotSpeed;
					if(fabs(reqAngle) > maxAngle)
					{
						Vec3 axis = currentDir.Cross(dir);
						if(axis.GetLengthSquared()>0.001f) // current dir and new dir are enough different
							dir = currentDir.GetRotated(axis.GetNormalized(),sgn(reqAngle)*maxAngle);
					}
				}				
				//adjust the orientation of the gun based on the aim-direction
				Matrix34 tm = Matrix33::CreateRotationVDir(dir);
				Vec3 vWPos=GetEntity()->GetWorldPos();
				tm.SetTranslation(vWPos);
				GetEntity()->SetWorldTM(tm);  //set the new orientation of the mounted gun

				vGunXAxis=tm.GetColumn0();
				Vec3 vInitialAimDirection = m_stats.mount_dir;
				Matrix33 vInitialPlayerOrientation = Matrix33::CreateRotationVDir(vInitialAimDirection);
				assert( vInitialAimDirection.IsUnit() );

	  		Vec3 newp;

				if (pActor->IsThirdPerson())
				{
					//third person
					f32 dist = m_mountparams.body_distance*1.3f;
					Vec3 oldp = pActor->GetEntity()->GetWorldPos();
					newp = GetEntity()->GetWorldPos()-vInitialAimDirection*dist; //mounted gun
					newp.z = oldp.z;
				}
				else
				{
					//first person
					f32 fMoveBack = (1.0f+(dir.z*dir.z*dir.z*dir.z*4.0f))*0.75f;
					f32	dist = m_mountparams.eye_distance*fMoveBack;
					Vec3 oldp = pActor->GetEntity()->GetWorldPos();
					newp = GetEntity()->GetWorldPos()-dir*dist; //mounted gun
					//newp.z -= 0.75f;
					newp.z = oldp.z;
				}


				Matrix34 actortm(pActor->GetEntity()->GetWorldTM());

				//if (pActor->IsThirdPerson())
				actortm=vInitialPlayerOrientation;

				actortm.SetTranslation(newp);
				pActor->GetEntity()->SetWorldTM(actortm, ENTITY_XFORM_USER);
				pActor->GetAnimationGraphState()->SetInput("Action","gunnerMounted");
				
				//f32 g_YLine=80.0f;
				//gEnv->pRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false, "Mounted Gun Active for FP and AI" ); 

				if (ICharacterInstance *pCharacter = pActor->GetEntity()->GetCharacter(0))
				{
					ISkeletonAnim *pSkeletonAnim = pCharacter->GetISkeletonAnim();
					assert(pSkeletonAnim);

					uint32 numAnimsLayer = pSkeletonAnim->GetNumAnimsInFIFO(0);
					for(uint32 i=0; i<numAnimsLayer; i++)
					{
						CAnimation &animation = pSkeletonAnim->GetAnimFromFIFO(0, i);
						if (animation.m_AnimParams.m_nFlags & CA_MANUAL_UPDATE)
						{
							f32 aimrad = Ang3::CreateRadZ(Vec2(vInitialAimDirection),Vec2(dir));
							animation.m_fAnimTime = clamp_tpl(aimrad/gf_PI,-1.0f,+1.0f)*0.5f+0.5f;
							//if (pActor->IsThirdPerson()==0)
								//animation.m_fAnimTime=0.6f; //Ivo & Benito: high advanced future code. don't ask what it is 
							                                //Benito - Not needed any more ;)

							//f32 g_YLine=100.0f;
							//gEnv->pRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false, "AnimTime: %f    MyAimAngle: %f deg:%   distance:%f", animation.m_fAnimTime, aimrad, RAD2DEG(aimrad),m_mountparams.body_distance ); 
						}
					}

				}

				m_stats.mount_last_aimdir = dir;
				
			}
		}
    
    if (ICharacterInstance* pCharInstance = pActor->GetEntity()->GetCharacter(0))
		{
      if (ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim())
      {   
        OldBlendSpace ap;				
        if (GetAimBlending(ap))
        {
					pSkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TurnSpeed, 0.5f + 0.5f * ap.m_turn, true);
        }        
      }        
    } 

    UpdateIKMounted(pActor, vGunXAxis*0.1f);
   
		RequireUpdate(eIUS_General);
  }
}

//------------------------------------------------------------------------
void CItem::UpdateIKMounted(IActor* pActor, const Vec3& vGunXAxis)
{
	
	if (!m_mountparams.left_hand_helper.empty() || !m_mountparams.right_hand_helper.empty())
  { 
    Vec3 lhpos=GetSlotHelperPos(eIGS_FirstPerson, m_mountparams.left_hand_helper.c_str(), true);
    Vec3 rhpos=GetSlotHelperPos(eIGS_FirstPerson, m_mountparams.right_hand_helper.c_str(), true);
    pActor->SetIKPos("leftArm",		lhpos-vGunXAxis, 1);
    pActor->SetIKPos("rightArm",	rhpos+vGunXAxis, 1);
    
	 // gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(lhpos, 0.075f, ColorB(255, 255, 255, 255));
   // gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(rhpos, 0.075f, ColorB(128, 128, 128, 255));
  }
}

//------------------------------------------------------------------------
bool CItem::GetAimBlending(OldBlendSpace& params)
{ 
  // unused here so far 
  return false;
}


//------------------------------------------------------------------------
void CItem::CheckViewChange()
{
  CActor *pOwner = GetOwnerActor();
  
  if (m_stats.mounted)
	{
    bool fp = pOwner?!pOwner->IsThirdPerson():false;

    if (fp!=m_stats.fp)
    {
      if (fp || !(m_stats.viewmode&eIVM_FirstPerson))
        OnEnterFirstPerson();
      else if (!fp)
        AttachArms(false, false);
    }
    
    m_stats.fp = fp;
 
		return;
	}

  if (!pOwner)
    return;

	if (!pOwner->IsThirdPerson())
	{
		if (!m_stats.fp || !(m_stats.viewmode&eIVM_FirstPerson))
			OnEnterFirstPerson();
		m_stats.fp = true;
	}
	else
	{
		if (m_stats.fp || !(m_stats.viewmode&eIVM_ThirdPerson))
			OnEnterThirdPerson();
		m_stats.fp = false;
	}
}

//------------------------------------------------------------------------
void CItem::SetViewMode(int mode)
{
	m_stats.viewmode = mode;

	if (mode & eIVM_FirstPerson)
	{
		SetHand(m_stats.hand);

		if (!m_parentId)
		{
			uint flags = GetEntity()->GetFlags();
			if (!m_stats.mounted)
				flags &= ~ENTITY_FLAG_CASTSHADOW;
			else
				flags |= ENTITY_FLAG_CASTSHADOW;

			//GetEntity()->SetFlags(flags|ENTITY_FLAG_RECVSHADOW);
			DrawSlot(eIGS_FirstPerson, true, !m_stats.mounted);
		}
		else
			DrawSlot(eIGS_FirstPerson, false, false);
	}
	else
	{
		SetGeometry(eIGS_FirstPerson, 0);
	}

	if (mode & eIVM_ThirdPerson)
	{
		DrawSlot(eIGS_ThirdPerson, true);
		if (!m_stats.mounted)
			CopyRenderFlags(GetOwner());
	}
	else
		DrawSlot(eIGS_ThirdPerson, false);

	for (TAccessoryMap::iterator it = m_accessories.begin(); it != m_accessories.end(); it++)
	{
		IItem *pItem = m_pGameFramework->GetIItemSystem()->GetItem(it->second);
		if (pItem)
		{
			CItem *pCItem = static_cast<CItem *>(pItem);
			if (pCItem)
				pCItem->SetViewMode(mode);
		}
	}
}

//------------------------------------------------------------------------
void CItem::ResetRenderFlags()
{
	if (!GetRenderProxy())
		return;

	IRenderNode *pRenderNode = GetRenderProxy()->GetRenderNode();
	if (pRenderNode)
	{
		pRenderNode->SetViewDistRatio(127);
		pRenderNode->SetLodRatio(127);
		GetEntity()->SetFlags(GetEntity()->GetFlags()|ENTITY_FLAG_CASTSHADOW);	
	}
}

//------------------------------------------------------------------------
void CItem::CopyRenderFlags(IEntity *pOwner)
{
	if (!pOwner || !GetRenderProxy())
		return;

	IRenderNode *pRenderNode = GetRenderProxy()->GetRenderNode();
	if (pRenderNode)
	{
		IEntityRenderProxy *pOwnerRenderProxy = (IEntityRenderProxy *)pOwner->GetProxy(ENTITY_PROXY_RENDER);
		IRenderNode *pOwnerRenderNode = pOwnerRenderProxy?pOwnerRenderProxy->GetRenderNode():NULL;
		if (pOwnerRenderNode)
		{
			pRenderNode->SetViewDistRatio(pOwnerRenderNode->GetViewDistRatio());
			pRenderNode->SetLodRatio(pOwnerRenderNode->GetLodRatio());

			uint flags = pOwner->GetFlags()&(ENTITY_FLAG_CASTSHADOW);
			uint mflags = GetEntity()->GetFlags()&(~(ENTITY_FLAG_CASTSHADOW));
			GetEntity()->SetFlags(mflags|flags);
		}
	}
}
