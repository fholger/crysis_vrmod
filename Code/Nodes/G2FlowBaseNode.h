/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Based on G4FlowBaseNode.h which was written by Nick Hesketh

-------------------------------------------------------------------------
History:
- 16:01:2006: Created by Mathieu Pinard

*************************************************************************/
#ifndef __G2FLOWBASENODE_H__
#define __G2FLOWBASENODE_H__

#include <IFlowSystem.h>

//////////////////////////////////////////////////////////////////////////
// Auto registration for flow nodes.
//////////////////////////////////////////////////////////////////////////
class CG2AutoRegFlowNodeBase : public IFlowNodeFactory
{
public:
	CG2AutoRegFlowNodeBase( const char *sClassName )
	{
		m_sClassName = sClassName;
		m_pNext = 0;
		if (!m_pLast)
		{
			m_pFirst = this;
		}
		else
			m_pLast->m_pNext = this;
		m_pLast = this;
	}
	void AddRef() {}
	void Release() {}

	//////////////////////////////////////////////////////////////////////////
	const char *m_sClassName;
	CG2AutoRegFlowNodeBase* m_pNext;
	static CG2AutoRegFlowNodeBase *m_pFirst;
	static CG2AutoRegFlowNodeBase *m_pLast;
	//////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////
template <class T>
class CG2AutoRegFlowNode : public CG2AutoRegFlowNodeBase
{
public:
	CG2AutoRegFlowNode( const char *sClassName ) : CG2AutoRegFlowNodeBase( sClassName ) {}
	IFlowNodePtr Create( IFlowNode::SActivationInfo * pActInfo ) { return new T(pActInfo); }
	void GetMemoryStatistics(ICrySizer * s)
	{ 
		SIZER_SUBCOMPONENT_NAME(s, "CG2AutoRegFlowNode");
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowBaseNode;

template <class T>
class CG2AutoRegFlowNodeSingleton : public CG2AutoRegFlowNodeBase
{
public:
	CG2AutoRegFlowNodeSingleton( const char *sClassName ) : CG2AutoRegFlowNodeBase( sClassName )
	{
		// this makes sure, the derived class DOES NOT implement a Clone method
		typedef IFlowNodePtr (CFlowBaseNode::*PtrToMemFunc) (IFlowNode::SActivationInfo*);
		static const PtrToMemFunc f = &T::Clone; // likely to get optimized away
	}
	IFlowNodePtr Create( IFlowNode::SActivationInfo * pActInfo ) 
	{ 
		if (!m_pInstance)
			m_pInstance = new T(pActInfo);
		return m_pInstance;
	}
	void GetMemoryStatistics(ICrySizer * s)
	{ 
		SIZER_SUBCOMPONENT_NAME(s, "CG2AutoRegFlowNodeSingleton");
		s->Add(*this);
	}
private:
	IFlowNodePtr m_pInstance;
};

//////////////////////////////////////////////////////////////////////////
// Use this define to register a new flow node class.
// Ex. REGISTER_FLOW_NODE( "Delay",CFlowDelayNode )
//////////////////////////////////////////////////////////////////////////
#define REGISTER_FLOW_NODE( FlowNodeClassName,FlowNodeClass ) \
	CG2AutoRegFlowNode<FlowNodeClass> g_AutoReg##FlowNodeClass ( FlowNodeClassName );

#define REGISTER_FLOW_NODE_EX( FlowNodeClassName,FlowNodeClass,RegName ) \
	CG2AutoRegFlowNode<FlowNodeClass> g_AutoReg##RegName ( FlowNodeClassName );

#define REGISTER_FLOW_NODE_SINGLETON( FlowNodeClassName,FlowNodeClass ) \
	CG2AutoRegFlowNodeSingleton<FlowNodeClass> g_AutoReg##FlowNodeClass ( FlowNodeClassName );

#define REGISTER_FLOW_NODE_SINGLETON_EX( FlowNodeClassName,FlowNodeClass,RegName ) \
	CG2AutoRegFlowNodeSingleton<FlowNodeClass> g_AutoReg##RegName ( FlowNodeClassName );


class CFlowBaseNode : public IFlowNode
{
public:
	CFlowBaseNode() { m_refs = 0; };
	virtual ~CFlowBaseNode() {}

	//////////////////////////////////////////////////////////////////////////
	// IFlowNode
	virtual void AddRef() { ++m_refs; };
	virtual void Release() { if (0 >= --m_refs)	delete this; };

	virtual IFlowNodePtr Clone( SActivationInfo *pActInfo ) { return this; }
	virtual bool SerializeXML( SActivationInfo *, const XmlNodeRef&, bool ) { return true; }
	virtual void Serialize(SActivationInfo *, TSerialize ser) {}
	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo ) {};
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Common functions to use in derived classes.
	//////////////////////////////////////////////////////////////////////////
	bool IsPortActive( SActivationInfo *pActInfo,int nPort ) const
	{
		return pActInfo->pInputPorts[nPort].IsUserFlagSet();
	}
	bool IsBoolPortActive( SActivationInfo *pActInfo,int nPort ) const
	{
		if (IsPortActive(pActInfo,nPort) && GetPortBool(pActInfo,nPort))
			return true;
		else
			return false;
	}
	EFlowDataTypes GetPortType( SActivationInfo *pActInfo,int nPort ) const
	{
		return (EFlowDataTypes)pActInfo->pInputPorts[nPort].GetType();
	}

	const TFlowInputData& GetPortAny( SActivationInfo *pActInfo,int nPort ) const
	{
		return pActInfo->pInputPorts[nPort];
	}

	bool GetPortBool( SActivationInfo *pActInfo,int nPort ) const
	{
		bool* p_x = (pActInfo->pInputPorts[nPort].GetPtr<bool>());
		if (p_x != 0) return *p_x;
		SFlowNodeConfig config;
		const_cast<CFlowBaseNode*> (this)->GetConfiguration(config);
		GameWarning("CFlowBaseNode::GetPortBool: Node=%p Port=%d '%s' Tag=%d -> Not a bool tag!", this, nPort,
			config.pInputPorts[nPort].name,
			pActInfo->pInputPorts[nPort].GetTag());
		return false;
	}
	int GetPortInt( SActivationInfo *pActInfo,int nPort ) const
	{
		int x = *(pActInfo->pInputPorts[nPort].GetPtr<int>());
		return x;
	}
	EntityId GetPortEntityId( SActivationInfo *pActInfo,int nPort )
	{
		EntityId x = *(pActInfo->pInputPorts[nPort].GetPtr<EntityId>());
		return x;
	}
	float GetPortFloat( SActivationInfo *pActInfo,int nPort ) const
	{
		float x = *(pActInfo->pInputPorts[nPort].GetPtr<float>());
		return x;
	}
	Vec3 GetPortVec3( SActivationInfo *pActInfo,int nPort ) const
	{
		Vec3 x = *(pActInfo->pInputPorts[nPort].GetPtr<Vec3>());
		return x;
	}
	EntityId GetPortEntityId( SActivationInfo *pActInfo,int nPort ) const
	{
		EntityId x = *(pActInfo->pInputPorts[nPort].GetPtr<EntityId>());
		return x;
	}
	const string& GetPortString( SActivationInfo *pActInfo,int nPort ) const
	{
		const string* p_x = (pActInfo->pInputPorts[nPort].GetPtr<string>());
		if (p_x != 0) return *p_x;
		const static string empty ("");
		SFlowNodeConfig config;
		const_cast<CFlowBaseNode*> (this)->GetConfiguration(config);
		GameWarning("CFlowBaseNode::GetPortString: Node=%p Port=%d '%s' Tag=%d -> Not a string tag!", this, nPort,
			config.pInputPorts[nPort].name,
			pActInfo->pInputPorts[nPort].GetTag());
		return empty;
	}

	//////////////////////////////////////////////////////////////////////////
	// Sends data to output port.
	//////////////////////////////////////////////////////////////////////////
	template <class T>
		void ActivateOutput( SActivationInfo *pActInfo,int nPort, const T &value )
	{
		SFlowAddress addr( pActInfo->myID, nPort, true );
		pActInfo->pGraph->ActivatePort( addr, value );
	}
	//////////////////////////////////////////////////////////////////////////
	bool IsOutputConnected( SActivationInfo *pActInfo,int nPort ) const
	{
		SFlowAddress addr( pActInfo->myID, nPort, true );
		return pActInfo->pGraph->IsOutputConnected( addr );
	}
	//////////////////////////////////////////////////////////////////////////

private:
	int m_refs;
};

#endif
