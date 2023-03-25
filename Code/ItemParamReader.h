/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Parameter reading helper...

-------------------------------------------------------------------------
History:
- 27:10:2004   11:30 : Created by Márcio Martins

*************************************************************************/
#ifndef __ITEMPARAMREADER_H__
#define __ITEMPARAMREADER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "ItemString.h"

class CItemParamReader
{
public:
	CItemParamReader(const IItemParamsNode *node): m_node(node) {};

	template<typename T>
	void Read(const char *name, T &value)
	{
		const IItemParamsNode *node = FindNode(name);
		if (node)
		{
			int v;
			if (node->GetAttribute("value", v))
				value=(T)v;
		}
	}

	void Read(const char *name, bool &value)
	{
		const IItemParamsNode *node = FindNode(name);
		if (node)
		{
			int v;
			if (node->GetAttribute("value", v))
				value=v!=0;
		}
	}

	void Read(const char *name, float &value)
	{
		const IItemParamsNode *node = FindNode(name);
		if (node)
			node->GetAttribute("value", value);
	}

	void Read(const char *name, Vec3 &value)
	{
		const IItemParamsNode *node = FindNode(name);
		if (node)
			node->GetAttribute("value", value);
	}

	void Read(const char *name, string &value)
	{
		const IItemParamsNode *node = FindNode(name);
		if (node)
		{
			const char *v;
			if (v=node->GetAttribute("value"))
				value=v;
		}
	}

#ifdef ITEM_USE_SHAREDSTRING
	void Read(const char *name, ItemString &value)
	{
		const IItemParamsNode *node = FindNode(name);
		if (node)
		{
			const char *v;
			if (v=node->GetAttribute("value"))
				value=v;
		}
	}
#endif

	void Read(const char *name, const char *&value)
	{
		const IItemParamsNode *node = FindNode(name);
		if (node)
			value=node->GetAttribute("value");
	}

private:
	const IItemParamsNode *FindNode(const char *name)
	{
		if (m_node)
		{
			int n=m_node->GetChildCount();
			for (int i=0; i<n; i++)
			{
				const IItemParamsNode *node=m_node->GetChild(i);
				if (node)
				{
					const char *nodeName = node->GetNameAttribute();
					if (nodeName && nodeName[0] && !strcmpi(nodeName, name))
						return node;
				}
			}
		}
		return 0;
	}

	const IItemParamsNode *m_node;
};



#endif //__ITEMPARAMREADER_H__