#include "StdAfx.h"
#include "VR3DEngine.h"
#include "Hooks.h"
#include "VRManager.h"


void VR_I3DEngine_RenderWorld(I3DEngine *pSelf, const int nRenderFlags, const CCamera &cam, const char *szDebugName, const int dwDrawFlags = -1, const int nFilterFlags=-1)
{
	CryLogAlways("I3DEngine::RenderWorld called: %s", szDebugName);
	bool mainPass = (strstr(szDebugName, "3DEngine") != nullptr);

	if (mainPass)
	{
		CryLogAlways("Main 3D render pass");
	}
	hooks::CallOriginal(VR_I3DEngine_RenderWorld)(pSelf, nRenderFlags, cam, szDebugName, dwDrawFlags, nFilterFlags);
}

void VR_I3DEngine_OnFrameStart(I3DEngine *pSelf)
{
	CryLogAlways("I3DEngine::OnFrameStart called");
	hooks::CallOriginal(VR_I3DEngine_OnFrameStart)(pSelf);
}

void VR_I3DEngine_Update(I3DEngine *pSelf)
{
	CryLogAlways("I3DEngine::Update called");
	hooks::CallOriginal(VR_I3DEngine_Update)(pSelf);
	CryLogAlways("I3DEngine::Update ended");
}

void VR_ISystem_SetViewCamera(ISystem *pSelf, CCamera &cam)
{
	CryLogAlways("SetViewCamera called");
	gVR->ModifyViewCamera(cam);
	hooks::CallOriginal(VR_ISystem_SetViewCamera)(pSelf, cam);
}

void VR_Init3DEngineHooks()
{
	hooks::InstallVirtualFunctionHook("ISystem::SetViewCamera", gEnv->pSystem, &ISystem::SetViewCamera, &VR_ISystem_SetViewCamera);
	hooks::InstallVirtualFunctionHook("I3DEngine::RenderWorld", gEnv->p3DEngine, &I3DEngine::RenderWorld, &VR_I3DEngine_RenderWorld);
	hooks::InstallVirtualFunctionHook("I3DEngine::OnFrameStart", gEnv->p3DEngine, &I3DEngine::OnFrameStart, &VR_I3DEngine_OnFrameStart);
	hooks::InstallVirtualFunctionHook("I3DEngine::Update", gEnv->p3DEngine, &I3DEngine::Update, &VR_I3DEngine_Update);
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

void VR_ISystem_RenderBegin(ISystem *pSelf)
{
	CryLogAlways("ISystem::RenderBegin");
	hooks::CallOriginal(VR_ISystem_RenderBegin)(pSelf);
}

void VR_ISystem_Render(ISystem *pSelf)
{
	CryLogAlways("ISystem::Render pre");
	hooks::CallOriginal(VR_ISystem_Render)(pSelf);
	CryLogAlways("ISystem::Render post");
}

void VR_ISystem_RenderEnd(ISystem *pSelf)
{
	CryLogAlways("ISystem::RenderEnd");
	hooks::CallOriginal(VR_ISystem_RenderEnd)(pSelf);
}

void VR_InitRendererHooks()
{
	hooks::InstallVirtualFunctionHook("IRenderer::BeginFrame", gEnv->pRenderer, &IRenderer::BeginFrame, &VR_IRenderer_BeginFrame);
	hooks::InstallVirtualFunctionHook("IRenderer::EndFrame", gEnv->pRenderer, &IRenderer::EndFrame, &VR_IRenderer_EndFrame);
	hooks::InstallVirtualFunctionHook("IRenderer::SetRenderTarget", gEnv->pRenderer, &IRenderer::SetRenderTarget, &VR_IRenderer_SetRenderTarget);
	hooks::InstallVirtualFunctionHook("ISystem::RenderBegin", gEnv->pSystem, &ISystem::RenderBegin, &VR_ISystem_RenderBegin);
	hooks::InstallVirtualFunctionHook("ISystem::Render", gEnv->pSystem, &ISystem::Render, &VR_ISystem_Render);
	hooks::InstallVirtualFunctionHook("ISystem::RenderEnd", gEnv->pSystem, &ISystem::RenderEnd, &VR_ISystem_RenderEnd);
}
