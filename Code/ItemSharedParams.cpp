/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2007.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 3:4:2005   15:11 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ItemSharedParams.h"


void CItemSharedParams::GetMemoryStatistics(ICrySizer *s)
{
	s->AddContainer(actions);
	s->AddContainer(accessoryparams);
	s->AddContainer(helpers);
	s->AddContainer(layers);
	s->AddContainer(dualWieldSupport);

	for (CItem::TActionMap::iterator iter = actions.begin(); iter != actions.end(); ++iter)
		s->Add(iter->first);
	for (CItem::THelperVector::iterator iter = helpers.begin(); iter != helpers.end(); ++iter)
		iter->GetMemoryStatistics(s);
	for (CItem::TLayerMap::iterator iter = layers.begin(); iter != layers.end(); ++iter)
	{
		s->Add(iter->first);
		iter->second.GetMemoryStatistics(s);
	}
	for (CItem::TAccessoryParamsMap::iterator iter = accessoryparams.begin(); iter != accessoryparams.end(); ++iter)
	{
		s->Add(iter->first);
		iter->second.GetMemoryStatistics(s);
	}
	for (CItem::TDualWieldSupportMap::iterator iter = dualWieldSupport.begin(); iter != dualWieldSupport.end(); ++iter)
		s->Add(iter->first);
}

CItemSharedParams *CItemSharedParamsList::GetSharedParams(const char *className, bool create)
{
	TSharedParamsMap::iterator it=m_params.find(CONST_TEMP_STRING(className));
	if (it!=m_params.end())
		return it->second;

	if (create)
	{
		CItemSharedParams *params=new CItemSharedParams();
		m_params.insert(TSharedParamsMap::value_type(className, params));

		return params;
	}

	return 0;
}

void CItemSharedParamsList::GetMemoryStatistics(ICrySizer *s)
{
	s->AddContainer(m_params);
	for (TSharedParamsMap::iterator iter = m_params.begin(); iter != m_params.end(); ++iter)
	{
		s->Add(iter->first);
		iter->second->GetMemoryStatistics(s);
	}
}