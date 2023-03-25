// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#ifndef __FLOWITEMANIMATIOn_H__
#define __FLOWITEMANIMATIOn_H__

#include "Game.h"
#include "Item.h"
#include "Actor.h"
#include "OffHand.h"
#include "Nodes/G2FlowBaseNode.h"

class CFlowItemAnimation : public CFlowBaseNode
{
public:
	CFlowItemAnimation( SActivationInfo * pActInfo )
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowItemAnimation(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Activate", _HELP("Play the animation.")),
			InputPortConfig<EntityId>("ItemId", _HELP("Set item that will play the animation.")),
			InputPortConfig<bool>("Busy", true, _HELP("Set item as busy while the animation is playing.")),
			InputPortConfig<string>("Animation", _HELP("Set the animation to be played.")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Plays an animation on an item.");
		config.SetCategory(EFLN_WIP);
	}

	struct FlowPlayItemAnimationAction
	{
		FlowPlayItemAnimationAction(const char *_anim, bool _busy): anim(_anim), busy(_busy) {};
		string anim;
		bool busy;

		// nested action ;o
		struct FlowPlayItemAnimationActionUnbusy
		{
			void execute(CItem *pItem)
			{
				pItem->SetBusy(false);
			}
		};


		void execute(CItem *pItem)
		{
			pItem->PlayAnimation(anim.c_str());

			if (busy)
			{
				pItem->SetBusy(true);

				pItem->GetScheduler()->TimerAction(pItem->GetCurrentAnimationTime(CItem::eIGS_FirstPerson), CSchedulerAction<FlowPlayItemAnimationActionUnbusy>::Create(), false);
			}
		}
	};

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				CItem *pItem = static_cast<CItem*>(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(GetPortEntityId(pActInfo, 1)));
				
				if (pItem)
				{
					pItem->GetScheduler()->ScheduleAction(CSchedulerAction<FlowPlayItemAnimationAction>::Create(FlowPlayItemAnimationAction(GetPortString(pActInfo, 3), GetPortBool(pActInfo, 2))));
				}
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};



class CFlowOffhandAnimation : public CFlowBaseNode
{
public:
	CFlowOffhandAnimation( SActivationInfo * pActInfo )
	{
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowOffhandAnimation(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Activate", _HELP("Play the animation.")),
			InputPortConfig<EntityId>("ItemId", _HELP("Set item that will play the animation.")),
			InputPortConfig<bool>("Busy", true, _HELP("Set item as busy while the animation is playing.")),
			InputPortConfig<string>("Animation", _HELP("Set the animation to be played.")),
			{0}
		};
		static const SOutputPortConfig outputs[] = {
			{0}
		};
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Plays an animation on an item.");
		config.SetCategory(EFLN_WIP);
	}

	struct FlowPlayOffhandAnimationAction
	{
		FlowPlayOffhandAnimationAction(const char *_anim, bool _busy): anim(_anim), busy(_busy) {};
		string anim;
		bool busy;

		// nested action ;o
		struct FlowPlayOffhandAnimationActionUnbusy
		{
			void execute(CItem *pItem)
			{
				pItem->SetBusy(false);
			}
		};

		// nested action ;o
		struct FlowPlayOffhandAnimationActionDone
		{
			void execute(CItem *pOffHand)
			{
				if (pOffHand->GetEntity()->GetClass()==CItem::sOffHandClass)
				{
					if (COffHand *pCOffHand=static_cast<COffHand *>(pOffHand))
						pCOffHand->CancelAction();

					if (CActor *pOwner=pOffHand->GetOwnerActor())
					{
						CItem *pMainHandItem=static_cast<CItem *>(pOwner->GetCurrentItem());

						if(!pMainHandItem)
						{
							if(!pOffHand->GetOwnerActor()->ShouldSwim())
								pOffHand->GetOwnerActor()->HolsterItem(false);
						}
						else if(!pMainHandItem->IsDualWield())
						{
							pMainHandItem->ResetDualWield();
							pMainHandItem->PlayAction(g_pItemStrings->offhand_off, 0, false, CItem::eIPAF_Default|CItem::eIPAF_NoBlend);
						}
					}
				}
			}
		};

		void execute(CItem *pOffHand)
 		{
			if (pOffHand->GetEntity()->GetClass()==CItem::sOffHandClass)
			{
				if (COffHand *pCOffHand=static_cast<COffHand *>(pOffHand))
					pCOffHand->PreExecuteAction(eOHA_THROW_GRENADE, eAAM_OnPress);
			}

			pOffHand->PlayAnimation(anim.c_str());

			if (busy)
			{
				pOffHand->SetBusy(true);
				pOffHand->GetScheduler()->TimerAction(pOffHand->GetCurrentAnimationTime(CItem::eIGS_FirstPerson), CSchedulerAction<FlowPlayOffhandAnimationActionUnbusy>::Create(), false);
			}

			if (CActor *pOwner=pOffHand->GetOwnerActor())
			{
				if (CItem *pMainHandItem=static_cast<CItem *>(pOwner->GetCurrentItem()))
				{
					if (busy)
					{
						pMainHandItem->SetBusy(true);
						pMainHandItem->GetScheduler()->TimerAction(pOffHand->GetCurrentAnimationTime(CItem::eIGS_FirstPerson), CSchedulerAction<FlowPlayOffhandAnimationActionUnbusy>::Create(), false);
					}

					if (pMainHandItem->TwoHandMode() >= 1 || pMainHandItem->IsDualWield())
						pMainHandItem->GetOwnerActor()->HolsterItem(true);
					else
					{
						pMainHandItem->PlayAction(g_pItemStrings->offhand_on);
						pMainHandItem->SetActionSuffix("akimbo_");
					}
				}
			}

			pOffHand->GetScheduler()->TimerAction(pOffHand->GetCurrentAnimationTime(CItem::eIGS_FirstPerson), CSchedulerAction<FlowPlayOffhandAnimationActionDone>::Create(), false);
		}
	};

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Activate:
			if (IsPortActive(pActInfo, 0))
			{
				CItem *pItem=0;
				if (EntityId id=GetPortEntityId(pActInfo, 1))
					pItem = static_cast<CItem*>(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(id));
				else
				{
					if (CActor *pActor=static_cast<CActor *>(g_pGame->GetIGameFramework()->GetClientActor()))
						pItem=pActor->GetItemByClass(CItem::sOffHandClass);
				}

				if (!pItem)
					return;

				pItem->GetScheduler()->ScheduleAction(CSchedulerAction<FlowPlayOffhandAnimationAction>::Create(FlowPlayOffhandAnimationAction(GetPortString(pActInfo, 3), GetPortBool(pActInfo, 2))));
			}
			break;
		}
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};


#endif //__FLOWITEMANIMATIOn_H__
