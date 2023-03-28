#include "StdAfx.h"
#include "VR3DEngine.h"
#include "Hooks.h"

struct VR3DEngine
{
	int m_renderTarget;
	int m_depthTarget;
};

VR3DEngine gVR3D;

void VR_I3DEngine_RenderWorld(I3DEngine *pSelf, const int nRenderFlags, const CCamera &cam, const char *szDebugName, const int dwDrawFlags = -1, const int nFilterFlags=-1)
{
	CryLogAlways("I3DEngine::RenderWorld called: %s", szDebugName);
	bool mainPass = (strstr(szDebugName, "3DEngine") != nullptr);

	/*
	int origWidth = gEnv->pRenderer->GetWidth();
	int origHeight = gEnv->pRenderer->GetHeight();
	int origBpp = gEnv->pRenderer->GetColorBpp();

	if (mainPass)
	{
		gEnv->pRenderer->SetRenderTarget(gVR3D.m_renderTarget);
		gEnv->pRenderer->ChangeDisplay(2048, 2048, 8);
		gEnv->pRenderer->SetViewport(0, 0, 2048, 2048);
		gEnv->pRenderer->SetScissor(0, 0, 2048, 2048);
		CryLogAlways("VR custom rendertarget set, renderer size: %i x %i", gEnv->pRenderer->GetWidth(), gEnv->pRenderer->GetHeight());
	}
	*/

	hooks::CallOriginal(VR_I3DEngine_RenderWorld)(pSelf, nRenderFlags, cam, szDebugName, dwDrawFlags, nFilterFlags);

	/*
	if (mainPass)
	{
		gEnv->pRenderer->SetRenderTarget(0);
		gEnv->pRenderer->ChangeDisplay(origWidth, origHeight, origBpp);
		gEnv->pRenderer->SetViewport(0, 0, origWidth, origHeight);
		gEnv->pRenderer->SetScissor(0, 0, origWidth, origHeight);
	}
	else
	{
		gEnv->pRenderer->SetRenderTarget(gVR3D.m_renderTarget);
		gEnv->pRenderer->ChangeDisplay(2048, 2048, 8);
		gEnv->pRenderer->SetViewport(0, 0, 2048, 2048);
		gEnv->pRenderer->SetScissor(0, 0, 2048, 2048);
	}
	*/
}

void VR_Init3DEngineHooks(I3DEngine* p3DEngine)
{
	hooks::InstallVirtualFunctionHook("I3DEngine::RenderWorld", p3DEngine, 2, (void*)&VR_I3DEngine_RenderWorld);
	gVR3D.m_renderTarget = gEnv->pRenderer->CreateRenderTarget(2048, 2048, eTF_A8R8G8B8);
}


void VR_IRenderer_BeginFrame(IRenderer *pSelf)
{
	CryLogAlways("IRenderer::BeginFrame called");
	hooks::CallOriginal(VR_IRenderer_BeginFrame)(pSelf);
}

void VR_IRenderer_EndFrame(IRenderer *pSelf)
{
	CryLogAlways("IRenderer::EndFrame called");
	hooks::CallOriginal(VR_IRenderer_EndFrame)(pSelf);
}

bool VR_IRenderer_SetRenderTarget(IRenderer *pSelf, int nHandle)
{
	CryLogAlways("IRenderer::SetRenderTarget called: %i", nHandle);
	return hooks::CallOriginal(VR_IRenderer_SetRenderTarget)(pSelf, nHandle);
}

void VR_InitRendererHooks(IRenderer* pRenderer)
{
	hooks::InstallVirtualFunctionHook("IRenderer::BeginFrame", pRenderer, 17, (void*)&VR_IRenderer_BeginFrame);
	hooks::InstallVirtualFunctionHook("IRenderer::EndFrame", pRenderer, 19, (void*)&VR_IRenderer_EndFrame);
	hooks::InstallVirtualFunctionHook("IRenderer::SetRenderTarget", pRenderer, 179, (void*)&VR_IRenderer_SetRenderTarget);
}
