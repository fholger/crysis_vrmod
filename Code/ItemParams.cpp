/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 30:8:2005   12:33 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Item.h"
#include "ItemSharedParams.h"
#include "ItemParamReader.h"
#include "Game.h"
#include <ISound.h>


//------------------------------------------------------------------------
bool CItem::ReadItemParams(const IItemParamsNode *root)
{
  if (!root)
  {
    GameWarning("Warning: ItemParams for item <%s> NULL", GetEntity()->GetName());
    return false;
  }

	const IItemParamsNode *params = root->GetChild("params");
	const IItemParamsNode *geometry = root->GetChild("geometry");
	const IItemParamsNode *actions = root->GetChild("actions");
	const IItemParamsNode *layers = root->GetChild("layers");
	const IItemParamsNode *accessories = root->GetChild("accessories");
	const IItemParamsNode *damagelevels = root->GetChild("damagelevels");
	const IItemParamsNode *accessoryAmmo = root->GetChild("accessoryAmmo");

	if (params) ReadParams(params);
	if (actions) ReadActions(actions);
	if (geometry) ReadGeometry(geometry);
	if (layers) ReadLayers(layers);
	if (accessories) ReadAccessories(accessories);
	if (damagelevels) ReadDamageLevels(damagelevels);
	if (accessoryAmmo) ReadAccessoryAmmo(accessoryAmmo);

	m_sharedparams->SetValid(true);

	return true;
}

#define ReadValue(hold, param)	reader.Read(#param, hold.param)
#define ReadValueEx(hold, name, param)	reader.Read(#name, hold.param)

//------------------------------------------------------------------------			
bool CItem::ReadParams(const IItemParamsNode *params)
{
	{
		CItemParamReader reader(params);
		ReadValue(m_params, selectable);
		ReadValue(m_params, droppable);
		ReadValue(m_params, pickable);
		ReadValue(m_params, mountable);
		ReadValue(m_params, usable);
		ReadValue(m_params, giveable);
		ReadValue(m_params, unique);
		ReadValue(m_params, arms);
		ReadValue(m_params, two_hand);
		ReadValue(m_params, mass);
		ReadValue(m_params, fly_timer);
		ReadValue(m_params, drop_impulse);
		ReadValue(m_params, drop_impulse_pos);
		ReadValue(m_params, drop_angles);
		ReadValue(m_params, pose);
		ReadValue(m_params, select_override);
		ReadValueEx(m_params, attachment_right, attachment[eIH_Right]);
		ReadValueEx(m_params, attachment_left, attachment[eIH_Left]);
		ReadValue(m_params, dual_wield_suffix);
		ReadValue(m_params, prone_not_usable);
		ReadValue(m_params, raiseable);
		ReadValue(m_params, raise_distance);
		ReadValue(m_params, update_hud);
		ReadValue(m_params, auto_droppable);
		ReadValue(m_params, has_first_select);
		ReadValue(m_params, attach_to_back);
		ReadValue(m_params, scopeAttachment);
		ReadValue(m_params, attachment_gives_ammo);
		ReadValue(m_params, display_name);
		ReadValueEx(m_params, bone_attachment_01, bone_attachment_01);
		ReadValueEx(m_params, bone_attachment_02, bone_attachment_02);
		ReadValue(m_params, select_on_pickup);
	}

	const IItemParamsNode *dw = params->GetChild("dualWield");
	if (dw)
	{
		int n = dw->GetChildCount();
		for (int i=0; i<n; i++)
		{
			const IItemParamsNode *item = dw->GetChild(i);
			if (!stricmp(dw->GetChildName(i), "item") && !m_sharedparams->Valid())
			{
				const char *name = item->GetAttribute("value");
				if (name && name[0])
					m_sharedparams->dualWieldSupport.insert(TDualWieldSupportMap::value_type(name, true));
			}
			else if (!stricmp(dw->GetChildName(i), "suffix"))
			{
				const char *suffix = item->GetAttribute("value");
				if (suffix)
					m_params.dual_wield_suffix = suffix;
			}
			else if (!stricmp(dw->GetChildName(i), "pose"))
			{
				const char *pose = item->GetAttribute("value");
				if (pose)
					m_params.dual_wield_pose = pose;
			}
		}
	}

	const IItemParamsNode *mp = params->GetChild("mount");
	if (mp)
	{
		CItemParamReader reader(mp);
		ReadValue(m_mountparams, pivot);
		ReadValue(m_mountparams, eye_distance);
		ReadValue(m_mountparams, eye_height);
		ReadValue(m_mountparams, body_distance);
//kirill - those are per-entity properties now, set from script
//		ReadValue(m_mountparams, min_pitch);
//		ReadValue(m_mountparams, max_pitch);
//		ReadValue(m_mountparams, yaw_range);
		ReadValue(m_mountparams, left_hand_helper);
		ReadValue(m_mountparams, right_hand_helper);
	}

	return true;
}

#undef ReadValue

//------------------------------------------------------------------------
bool CItem::ReadGeometry(const IItemParamsNode *geometry)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if (!m_sharedparams->Valid())
	{
		// read teh helpers
		m_sharedparams->helpers.resize(0);

		const IItemParamsNode *attachments = geometry->GetChild("boneattachments");
		if (attachments)
		{
			int n = attachments->GetChildCount();
			SAttachmentHelper helper;
			for (int i=0; i<n; i++)
			{
				const IItemParamsNode *attachment = attachments->GetChild(i);
				const char *slot = attachment->GetAttribute("target");
				const char *name = attachment->GetAttribute("name");
				const char *bone = attachment->GetAttribute("bone");

				int islot = TargetToSlot(slot);
				if (islot == eIGS_Last)
				{
					GameWarning("Invalid attachment helper target for item '%s'! Skipping...", GetEntity()->GetName());
					continue;
				}

				if (!name || !bone)
				{
					GameWarning("Invalid attachment helper specification for item '%s'! Skipping...", GetEntity()->GetName());
					continue;
				}

				helper.name = name;
				helper.bone = bone;
				helper.slot = islot;

				m_sharedparams->helpers.push_back(helper);
			}
		}
	}

	bool result = true;
	int n = geometry->GetChildCount();
	for (int i=0; i<n; i++)
	{
		const IItemParamsNode *child = geometry->GetChild(i);
		int islot=TargetToSlot(geometry->GetChildName(i));
		if (islot!=eIGS_Last)
		{
			result = result && SetGeometryFromParams(islot, child);
			if (result)
				PlayAction(m_idleAnimation[islot], 0, true, (eIPAF_Default|eIPAF_NoBlend)&~eIPAF_Owner);
		}
	}

	ForceSkinning(true);

	return result;
}

//------------------------------------------------------------------------
bool CItem::ReadActions(const IItemParamsNode *actions)
{
	if (!m_sharedparams->Valid())
	{
		m_sharedparams->actions.clear();
		int n = actions->GetChildCount();
		for (int i=0; i<n; i++)
		{
			const IItemParamsNode *actionparams = actions->GetChild(i);
			if (actionparams)
			{
				SAction action;
				if (ReadAction(actionparams, &action))
				{	
					const char *name = actionparams->GetAttribute("name");
					m_sharedparams->actions.insert(TActionMap::value_type(name, action));
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CItem::ReadAction(const IItemParamsNode *actionparams, SAction *pAction)
{
	const char *actionName = actionparams->GetAttribute("name");
	if (!actionName)
	{
		GameWarning("Missing action name for item '%s'! Skipping...", GetEntity()->GetName());
		return false;
	}

	int children=0;
	actionparams->GetAttribute("children", children);
	pAction->children=children!=0;

	int n = actionparams->GetChildCount();
	for (int i=0; i<n; i++)
	{
		const IItemParamsNode *child = actionparams->GetChild(i);
		const char *childName = actionparams->GetChildName(i);
		if (!stricmp(childName, "sound"))
		{
			const char *name = child->GetAttribute("name");
			if (!name)
			{
				GameWarning("Missing name of sound for action '%s' in item '%s'! Skipping...", actionName, GetEntity()->GetName());
				return false;
			}

			const char *slot = child->GetAttribute("target");
			int islot = TargetToSlot(slot);

			if ((islot != eIGS_FirstPerson) && (islot != eIGS_ThirdPerson))
			{
				GameWarning("Invalid sound target '%s' for action '%s' in item '%s'! Skipping...", slot, actionName, GetEntity()->GetName());
				return false;
			}

			if (!pAction->sound[islot].name.empty())
			{
				GameWarning("Sound target '%s' for action '%s' in item '%s' already specified! Skipping...", slot, actionName, GetEntity()->GetName());
				return false;
			}

			float radius = 1.0f; child->GetAttribute("radius", radius);
			float sphere = 0.0f; child->GetAttribute("sphere", sphere);
			int isstatic = 0; child->GetAttribute("static", isstatic);
			int issynched =0; child->GetAttribute("synched", issynched);
			pAction->sound[islot].name = name;
			pAction->sound[islot].airadius = radius;
			pAction->sound[islot].sphere = sphere;
			pAction->sound[islot].isstatic = isstatic!=0;
			pAction->sound[islot].issynched = issynched!=0;
		}
		else if (!stricmp(childName, "animation"))
		{
			const char *name = child->GetAttribute("name");
			if (!name)
			{
				GameWarning("Missing name of animation for action '%s' in item '%s'! Skipping...", actionName, GetEntity()->GetName());
				return false;
			}

			const char *slot = child->GetAttribute("target");
			int islot = TargetToSlot(slot);
			
			if (islot == eIGS_Last)
			{
				GameWarning("Invalid animation target '%s' for action '%s' in item '%s'! Skipping...", slot, actionName, GetEntity()->GetName());
				return false;
			}

			float speed = 1.0f; child->GetAttribute("speed", speed);
			float blend = 0.125f; child->GetAttribute("blendTime", blend);
			SAnimation animation;

			const char *camera_helper = child->GetAttribute("camera_helper");
			if (camera_helper && camera_helper[0])
			{
				int pos=1; child->GetAttribute("camera_position", pos);
				int rot=1; child->GetAttribute("camera_rotation", rot);
				int follow=0; child->GetAttribute("camera_follow", follow);
				int reorient=0; child->GetAttribute("camera_reorient", reorient);

				animation.camera_helper = camera_helper;
				animation.camera_pos=pos!=0;
				animation.camera_rot=rot!=0;
				animation.camera_follow=follow!=0;
				animation.camera_reorient=reorient!=0;
			}
			
			animation.name = name;
			animation.speed = speed;
			animation.blend = blend;

			pAction->animation[islot].push_back(animation);
		}
    else if (!stricmp(childName, "effect"))
    {
      const char *name = child->GetAttribute("name");
      if (!name)
      {
        GameWarning("Missing name of effect for action '%s' in item '%s'! Skipping...", actionName, GetEntity()->GetName());
        return false;
      }

      const char *slot = child->GetAttribute("target");
      int islot = TargetToSlot(slot);

      if ((islot != eIGS_FirstPerson) && (islot != eIGS_ThirdPerson))
      {
        GameWarning("Invalid effect target '%s' for action '%s' in item '%s'! Skipping...", slot, actionName, GetEntity()->GetName());
        return false;
      }

      if (!pAction->effect[islot].name.empty())
      {
        GameWarning("Effect target '%s' for action '%s' in item '%s' already specified! Skipping...", slot, actionName, GetEntity()->GetName());
        return false;
      }
      
      pAction->effect[islot].name = name;
      
      const char *helper = child->GetAttribute("helper");
      if (helper && helper[0])
        pAction->effect[islot].helper = helper;      
    }
		else
		{
			GameWarning("Unknown param '%s' for action '%s' in item '%s'! Skipping...", childName, actionName, GetEntity()->GetName());
			return false;
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CItem::ReadDamageLevels(const IItemParamsNode *damagelevels)
{
	m_damageLevels.resize(0);

	int n = damagelevels->GetChildCount();
	for (int i=0; i<n; i++)
	{
		const IItemParamsNode *levelparams = damagelevels->GetChild(i);
		if (levelparams)
		{
			SDamageLevel level;
			levelparams->GetAttribute("min_health", level.min_health);
			levelparams->GetAttribute("max_health", level.max_health);
			levelparams->GetAttribute("scale", level.scale);
			const char *helper=levelparams->GetAttribute("helper");
			const char *effect=levelparams->GetAttribute("effect");
			if (effect)
				level.effect=effect;
			if (helper)
				level.helper=helper;

			m_damageLevels.push_back(level);
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CItem::ReadLayers(const IItemParamsNode *layers)
{
	if (!m_sharedparams->Valid())
	{
		m_sharedparams->layers.clear();
		int n = layers->GetChildCount();
		for (int i=0; i<n; i++)
		{
			const IItemParamsNode *layer = layers->GetChild(i);
			if (layer)
			{
				SLayer lyr;
				if (ReadLayer(layer, &lyr))
				{	
					const char *name = layer->GetAttribute("name");
					m_sharedparams->layers.insert(TLayerMap::value_type(name, lyr));
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CItem::ReadLayer(const IItemParamsNode *layer, SLayer *pLayer)
{
	const char *layerName = layer->GetAttribute("name");
	if (!layerName)
	{
		GameWarning("Missing layer name for item '%s'! Skipping...", GetEntity()->GetName());
		return false;
	}


	pLayer->isstatic=false;
	int isstatic;
	if (layer->GetAttribute("static", isstatic))
		pLayer->isstatic = isstatic!=0;

	int n = layer->GetChildCount();
	for (int i=0; i<n; i++)
	{
		const IItemParamsNode *child = layer->GetChild(i);
		if (!stricmp(layer->GetChildName(i), "animation"))
		{
			const char *name = child->GetAttribute("name");
			if (!name)
			{
				GameWarning("Missing name of animation for layer '%s' in item '%s'! Skipping...", layerName, GetEntity()->GetName());
				return false;
			}

			const char *slot = child->GetAttribute("target");
			int islot = TargetToSlot(slot);

			if (islot == eIGS_Last)
			{
				GameWarning("Invalid animation target '%s' of for layer '%s' in item '%s'! Skipping...", slot, layerName, GetEntity()->GetName());
				return false;
			}

			pLayer->name[islot] = name;
			pLayer->id[islot] = 0; child->GetAttribute("layerId", pLayer->id[islot]);
		}
		else if (!stricmp(layer->GetChildName(i), "bones"))
		{
			int nb = child->GetChildCount();
			for (int b=0; b<nb; b++)
			{
				const IItemParamsNode *bone = child->GetChild(b);
				if (!stricmp(child->GetChildName(b), "bone"))
				{
					const char *name = bone->GetAttribute("name");
					if (!name)
					{
						GameWarning("Missing name of bone for layer '%s' in item '%s'! Skipping...", layerName, GetEntity()->GetName());
						return false;
					}

					pLayer->bones.push_back(name);
				}
			}
		}
		else
		{
			GameWarning("Unknown param '%s' for layer '%s' in item '%s'! Skipping...", layer->GetChildName(i), layerName, GetEntity()->GetName());
			return false;
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CItem::ReadAccessories(const IItemParamsNode *accessories)
{
	if (!m_sharedparams->Valid())
		m_sharedparams->accessoryparams.clear();
	
	m_initialSetup.resize(0);

	int n = accessories->GetChildCount();
	for (int i=0; i<n; i++)
	{
		const IItemParamsNode *child = accessories->GetChild(i);
		if (!stricmp(accessories->GetChildName(i), "accessory") && !m_sharedparams->Valid())
		{
			SAccessoryParams params;
			if (!ReadAccessoryParams(child, &params))
				continue;

			const char *name = child->GetAttribute("name");
			m_sharedparams->accessoryparams.insert(TAccessoryParamsMap::value_type(name, params));

		}
		else if (!stricmp(accessories->GetChildName(i), "initialsetup"))
		{
			int na = child->GetChildCount();
			for (int k=0; k<na; k++)
			{
				const IItemParamsNode *accessory = child->GetChild(k);
				if (!stricmp(child->GetChildName(k), "accessory"))
				{
					const char *name = accessory->GetAttribute("name");
					if (!name || !name[0])
					{
						GameWarning("Missing accessory name for initial setup in item '%s'! Skipping...", GetEntity()->GetName());
						continue;
					}

					m_initialSetup.push_back(name);
				}
				else
				{
					GameWarning("Unknown param '%s' in initial setup for item '%s'! Skipping...", child->GetChildName(k), GetEntity()->GetName());
					continue;
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CItem::ReadAccessoryParams(const IItemParamsNode *accessory, SAccessoryParams *params)
{
	const char *name = accessory->GetAttribute("name");
	if (!name || !name[0])
	{
		GameWarning("Missing accessory name for item '%s'! Skipping...", GetEntity()->GetName());
		return false;
	}

	const IItemParamsNode *attach = accessory->GetChild("attach");
	const IItemParamsNode *detach = accessory->GetChild("detach");

	if (!attach || !detach)
	{
		GameWarning("Missing attach/detach details for accessory '%s' in item '%s'! Skipping...", name, GetEntity()->GetName());
		return false;
	}

	params->attach_action = attach->GetAttribute("action");
	params->attach_helper = attach->GetAttribute("helper");
	params->attach_layer = attach->GetAttribute("layer");
	params->detach_action = detach->GetAttribute("action");

	string firemodes = accessory->GetAttribute("firemodes");
	int curPos = 0;
	string curToken, nextToken;
	nextToken = firemodes.Tokenize(",", curPos);
	while (!nextToken.empty())
	{
		curToken = nextToken;
		curToken.Trim();
#ifdef ITEM_USE_SHAREDSTRING
		params->firemodes.push_back(curToken.c_str());
#else
		params->firemodes.push_back(curToken);
#endif
		nextToken = firemodes.Tokenize(",", curPos);
	}

	params->switchToFireMode = accessory->GetAttribute("switchToFireMode");
	params->zoommode = accessory->GetAttribute("zoommode");
	
	int exclusive = 0;
	accessory->GetAttribute("exclusive", exclusive);
	params->exclusive = exclusive!=0;
	int client_only=1;
	accessory->GetAttribute("client_only", client_only);
	params->client_only= client_only!=0;

	params->params = accessory->GetChild("params");

	return true;
}

//-----------------------------------------------------------------------
bool CItem::ReadAccessoryAmmo(const IItemParamsNode *ammos)
{
	m_bonusAccessoryAmmo.clear();

	if (!ammos)
		return false;

	for (int i=0; i<ammos->GetChildCount(); i++)
	{
		const IItemParamsNode *ammo = ammos->GetChild(i);
		if (!strcmpi(ammo->GetName(), "ammo"))
		{
			int amount=0;

			const char* name = ammo->GetAttribute("name");
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
			assert(pClass);

			ammo->GetAttribute("amount", amount);

			if (amount)
				m_bonusAccessoryAmmo[pClass]=amount;

		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CItem::SetGeometryFromParams(int slot, const IItemParamsNode *geometry)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	const char *name = geometry->GetAttribute("name");
	if (!name || !name[0])
	{
		GameWarning("Missing geometry name for loading item '%s'!", GetEntity()->GetName());
		return false;
	}

	Vec3 position(0,0,0); geometry->GetAttribute("position", position);
	Ang3 angles(0,0,0);		geometry->GetAttribute("angles", angles);
	float scale=1.0f;			geometry->GetAttribute("scale", scale);

	if (slot == eIGS_FirstPerson)
	{
		const char *hand = geometry->GetAttribute("hand");
		int idx = 0;
		if (hand && hand[0])
		{
			if (!stricmp(hand, "right"))
				idx = 1;
			else if (!stricmp(hand, "left"))
				idx = 2;
			else
			{
				GameWarning("Invalid hand '%s' loading item '%s'!", hand, GetEntity()->GetName());
				return false;
			}
		}

		m_fpgeometry[idx].name=name;
		m_fpgeometry[idx].position=position;
		m_fpgeometry[idx].angles=DEG2RAD(angles);
		m_fpgeometry[idx].scale=scale;

		
		// marcio: first person geometry will be loaded upon selecting the weapon in first person
		//if (((idx<2) && (m_stats.hand == eIH_Right)) || ((idx==2) && (m_stats.hand == eIH_Left)))
		//	SetGeometry(slot, name, position, DEG2RAD(angles), scale, false);
	}
	else
		SetGeometry(slot, name, position, DEG2RAD(angles), scale, false);

	return true;
}

//------------------------------------------------------------------------
int CItem::TargetToSlot(const char *slot)
{
	int islot = eIGS_Last;
	if (slot)
	{
		if (!stricmp(slot, "firstperson"))
			islot = eIGS_FirstPerson;
		else if (!stricmp(slot, "thirdperson"))
			islot = eIGS_ThirdPerson;
		else if (!stricmp(slot, "arms"))
			islot = eIGS_Arms;
		else if (!stricmp(slot, "aux0"))
			islot = eIGS_Aux0;
		else if (!stricmp(slot, "owner"))
			islot = eIGS_Owner;
		else if (!stricmp(slot, "ownerloop"))
			islot = eIGS_OwnerLooped;
		else if (!stricmp(slot, "offhand"))
			islot = eIGS_OffHand;
    else if (!stricmp(slot, "destroyed"))
      islot = eIGS_Destroyed;
		else if (!stricmp(slot, "thirdpersonAux"))
			islot = eIGS_ThirdPersonAux;
		else if (!stricmp(slot, "aux1"))
			islot = eIGS_Aux1;
	}

	return islot;
}

//------------------------------------------------------------------------
void CItem::ReadProperties(IScriptTable *pProperties)
{
	if (pProperties)
	{
		GetEntityProperty("HitPoints", m_properties.hitpoints);
		GetEntityProperty("bPickable", m_properties.pickable);
		GetEntityProperty("bMounted", m_properties.mounted);
		GetEntityProperty("bPhysics", m_properties.physics);
		GetEntityProperty("bUsable", m_properties.usable);


		GetEntityProperty("Respawn", "bRespawn", m_respawnprops.respawn);
		GetEntityProperty("Respawn", "nTimer", m_respawnprops.timer);
		GetEntityProperty("Respawn", "bUnique", m_respawnprops.unique);
	}
}