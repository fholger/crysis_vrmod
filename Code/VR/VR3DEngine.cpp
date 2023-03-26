#include "StdAfx.h"
#include "VR3DEngine.h"
#include "Hooks.h"

void VR_I3DEngine_RenderWorld(I3DEngine *pSelf, const int nRenderFlags, const CCamera &cam, const char *szDebugName, const int dwDrawFlags = -1, const int nFilterFlags=-1)
{
	CryLogAlways("I3DEngine::RenderWorld called: %s", szDebugName);

	hooks::CallOriginal(VR_I3DEngine_RenderWorld)(pSelf, nRenderFlags, cam, szDebugName, dwDrawFlags, nFilterFlags);
}

void VR_Init3DEngineHooks(I3DEngine* p3DEngine)
{
	hooks::InstallVirtualFunctionHook("I3DEngine::RenderWorld", p3DEngine, 2, (void*)&VR_I3DEngine_RenderWorld);
}
