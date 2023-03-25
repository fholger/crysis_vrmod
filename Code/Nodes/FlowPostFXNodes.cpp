// -------------------------------------------------------------------------
// Crytek Source File.
// Copyright (C) Crytek GmbH, 2001-2008.
// -------------------------------------------------------------------------
#include "StdAfx.h"
#include "Game.h"
#include "Nodes/G2FlowBaseNode.h"

struct FXParamsGlobal
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = {
      InputPortConfig<float>("Global_Brightness", 1.0f),
      InputPortConfig<float>("Global_Contrast", 1.0f),
      InputPortConfig<float>("Global_Saturation", 1.0f),
      InputPortConfig<float>("Global_Sharpening", 1.0f),
      InputPortConfig<float>("Global_ColorC", 0.0f),
      InputPortConfig<float>("Global_ColorM", 0.0f),
      InputPortConfig<float>("Global_ColorY", 0.0f),
      InputPortConfig<float>("Global_ColorK", 0.0f),
      InputPortConfig<float>("Global_ColorHue", 0.0f),
      {0}
    };
    static const SOutputPortConfig outputs[] = {
      {0}
    };
    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Sets the global PostFX params");
  }
};

struct FXParamsScreenFrost
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = {
      InputPortConfig<bool>("ScreenFrost_Active", false),
      InputPortConfig<float>("ScreenFrost_Amount", 0.0f),
      InputPortConfig<float>("ScreenFrost_CenterAmount", 1.0f),
      {0}
    };
    static const SOutputPortConfig outputs[] = {
      {0}
    };
    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("ScreenFrost");
  }
};

struct FXParamsWaterDroplets
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = {
      InputPortConfig<bool>("WaterDroplets_Active", false),
      InputPortConfig<float>("WaterDroplets_Amount", 0.0f),
      {0}
    };
    static const SOutputPortConfig outputs[] = {
      {0}
    };
    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("WaterDroplets");
  }
};

struct FXParamsGlow
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = {
      InputPortConfig<bool>("Glow_Active", false),
      InputPortConfig<float>("Glow_Scale", 0.5f),
      {0}
    };
    static const SOutputPortConfig outputs[] = {
      {0}
    };
    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Glow");
  }
};

struct FXParamsBloodSplats
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = {
      InputPortConfig<bool> ("BloodSplats_Active", false),
      InputPortConfig<int>  ("BloodSplats_Type", false),
      InputPortConfig<float>("BloodSplats_Amount", 1.0f),
      InputPortConfig<bool> ("BloodSplats_Spawn", false),
      InputPortConfig<float>("BloodSplats_Scale", 1.0f),
      {0}
    };
    static const SOutputPortConfig outputs[] = {
      {0}
    };
    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Bloodsplats");
  }
};

struct FXParamsGlittering
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = {
      InputPortConfig<bool> ("Glittering_Active", false),
      {0}
    };
    static const SOutputPortConfig outputs[] = {
      {0}
    };
    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("Glittering");
  }
};

struct FXParamsSunShafts
{
  static void GetConfiguration(SFlowNodeConfig& config)
  {
    static const SInputPortConfig inputs[] = {
      InputPortConfig<bool> ("SunShafts_Active", false),
      InputPortConfig<int>  ("SunShafts_Type", 0),
      InputPortConfig<float>("SunShafts_Amount", 0.25),
      {0}
    };
    static const SOutputPortConfig outputs[] = {
      {0}
    };
    config.pInputPorts = inputs;
    config.pOutputPorts = outputs;
    config.sDescription = _HELP("SunShafts");
  }
};

template<class T>
class CFlowFXNode : public CFlowBaseNode
{
public:
  CFlowFXNode( SActivationInfo * pActInfo )
  {
  }

  ~CFlowFXNode()
  {
  }

	/*
  IFlowNodePtr Clone( SActivationInfo * pActInfo )
  {
    return this;
  }
	*/

  virtual void GetConfiguration(SFlowNodeConfig& config)
  {
    T::GetConfiguration(config);
    config.SetCategory(EFLN_WIP);
  }

  virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
  {
    if (event != eFE_Activate)
      return;
    SFlowNodeConfig config;
    T::GetConfiguration(config);
    I3DEngine* pEngine = gEnv->p3DEngine;
    for (int i = 0; config.pInputPorts[i].name; ++i)
    {
      if (true || IsPortActive(pActInfo, i))
      {
        const TFlowInputData& anyVal = GetPortAny(pActInfo, i);
        float fVal;
        bool ok = anyVal.GetValueWithConversion(fVal);
        if (ok)
        {
          // set postfx param
          pEngine->SetPostEffectParam(config.pInputPorts[i].name, fVal);
        }
      }
    }
  }

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE_SINGLETON_EX("CrysisFX:PostFXGlobal", CFlowFXNode<FXParamsGlobal>, FXParamsGlobal);
REGISTER_FLOW_NODE_SINGLETON_EX("CrysisFX:PostFXScreenFrost", CFlowFXNode<FXParamsScreenFrost>, FXParamsScreenFrost);
REGISTER_FLOW_NODE_SINGLETON_EX("CrysisFX:PostFXWaterDroplets", CFlowFXNode<FXParamsWaterDroplets>, FXParamsWaterDroplets);
REGISTER_FLOW_NODE_SINGLETON_EX("CrysisFX:PostFXBloodSplats", CFlowFXNode<FXParamsBloodSplats>, FXParamsBloodSplats);
REGISTER_FLOW_NODE_SINGLETON_EX("CrysisFX:PostFXGlow", CFlowFXNode<FXParamsGlow>, FXParamsGlow);
REGISTER_FLOW_NODE_SINGLETON_EX("CrysisFX:PostFXGlittering", CFlowFXNode<FXParamsGlittering>, FXParamsGlittering);
// REGISTER_FLOW_NODE_EX("CrysisFX:PostFXSunShafts", CFlowFXNode<FXParamsSunShafts>, FXParamsSunShafts);