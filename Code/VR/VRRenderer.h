#pragma once

#include "Cry_Camera.h"
#include "HUD/HUD.h"
#include <d3d10_1.h>
#include <unordered_map>
#include <wrl/client.h>
#undef PlaySound
#undef DrawText
#undef GetMessage
#undef min
#undef max

using Microsoft::WRL::ComPtr;

class VRRenderer : public CHUD::IHUDListener
{
	typedef void (*SystemRenderFunc)(ISystem* system);

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

	void OnBinoculars(bool bShown) override { m_binocularsActive = bShown; }

	bool ShouldIgnoreWindowSizeChanges() const { return m_ignoreWindowSizeChanges; }

	void OnRenderTargetChanged(ID3D10RenderTargetView* rtv, ID3D10DepthStencilView* depth);
	void OnSetRasterizerState(ID3D10RasterizerState*& state);

private:
	ComPtr<ID3D10Device> m_device;
	CCamera m_originalViewCamera;
	bool m_viewCamOverridden = false;
	bool m_binocularsActive = false;
	bool m_ignoreWindowSizeChanges = false;
	int m_windowWidth = 0;
	int m_windowHeight = 0;
	bool m_didRenderThisFrame = false;
	int64 m_lastPresentCallTime = 0;
	bool m_renderTargetIsBackBuffer = false;
	int m_renderingEye = -1;
	std::unordered_map<ID3D10RasterizerState*, ComPtr<ID3D10RasterizerState>> m_replacementStates;

	void RenderSingleEye(int eye, SystemRenderFunc renderFunc, ISystem* pSystem);
	void DrawCrosshair();

	void SetScissorForCurrentEye();
};

extern VRRenderer* gVRRenderer;
