#pragma once
#include "Cry_Camera.h"
#include "VRGui.h"
#include "HUD/HUD.h"
#include "Menus/FlashMenuObject.h"
#include "Hooks.h"

struct IDXGISwapChain;

enum VRRenderMode
{
	RM_VR,
	RM_2D,
	RM_3D,
};

class VRRenderer : public CHUD::IHUDListener
{
	typedef void (FASTCALL *SystemRenderFunc)(ISystem* system);

public:
	void Init();
	void Shutdown();

	void Render(SystemRenderFunc renderFunc, ISystem *pSystem);
	bool OnPrePresent(IDXGISwapChain* swapChain);
	void OnPostPresent();

	const CCamera& GetCurrentViewCamera() const;
	void ProjectToScreenPlayerCam(float ptx, float pty, float ptz, float* sx, float* sy, float* sz);

	void SetDesiredWindowSize(int width, int height);
	Vec2i GetWindowSize() const;
	void ChangeRenderResolution(int width, int height);

	bool ShouldRenderVR() const;
	VRRenderMode GetRenderMode() const;

	void OnBinoculars(bool bShown) override { m_binocularsActive = bShown; }

	bool ShouldIgnoreWindowSizeChanges() const { return m_ignoreWindowSizeChanges || SAFE_MENU_FUNC_RET(IsLoadingScreenActive()); }

	bool ShouldRenderShadowMaps() const { return m_currentEye != 1; }

	bool AreBinocularsActive() const { return m_binocularsActive; }
	void GuiClickedUnfocussed() { m_gui.ClickedNoFocus(); }

private:
	VRGui m_gui;
	CCamera m_originalViewCamera;
	bool m_viewCamOverridden = false;
	bool m_binocularsActive = false;
	bool m_ignoreWindowSizeChanges = false;
	int m_windowWidth = 0;
	int m_windowHeight = 0;
	bool m_didRenderThisFrame = false;
	int64 m_lastPresentCallTime = 0;
	int m_currentEye = -1;
	float m_imguiScale = 1.f;

	void RenderSingleEye(int eye, SystemRenderFunc renderFunc, ISystem* pSystem);
	void DrawCrosshair();

	void UpdateShaderParamsForReflexSight();

	void RenderImGui();
	void DrawImGui();
};

extern VRRenderer* gVRRenderer;
