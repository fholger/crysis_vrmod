#include "StdAfx.h"
#include "VRGui.h"

#include "GameCVars.h"
#include "imgui.h"
#include "VRManager.h"
#include "VRRenderer.h"
#include "backends/imgui_impl_dx10.h"

void VRGui::Init(ID3D10Device* device)
{
	ImGui::CreateContext();
	ImGui_ImplDX10_Init(device);
	ImGui::StyleColorsDark();
	SetScale(1.0f);
}

void VRGui::Shutdown()
{
	ImGui_ImplDX10_Shutdown();
	ImGui::DestroyContext();
}

void VRGui::SetScale(float scale)
{
	if (scale != m_currentScale)
	{
		m_currentScale = scale;
		ReloadFonts(scale);
		SetupStyle(scale);
	}
}

void VRGui::Render()
{
	Vec2i windowSize = gVRRenderer->GetWindowSize();
	Vec2i renderSize = gVR->GetRenderSize();
	SetScale(g_pGameCVars->vr_gui_scale * max(renderSize.y / windowSize.y, renderSize.x / windowSize.x));
	ImGui::GetIO().DisplaySize = ImVec2((float)renderSize.x, (float)renderSize.y);
	//ImGui::GetIO().DisplayFramebufferScale = ImVec2((float)renderSize.x / windowSize.x, (float)renderSize.y / windowSize.y);
	ImGui_ImplDX10_NewFrame();
	ImGui::NewFrame();

	ImGui::GetIO().MouseDrawCursor = ImGui::GetIO().WantCaptureMouse;

	Draw();

	ImGui::Render();
	ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());
}

void VRGui::SetupStyle(float scale)
{
	ImGuiStyle& style = ImGui::GetStyle();
	style = ImGuiStyle();
	ImGui::StyleColorsDark();
	style.ScaleAllSizes(scale);
}

void VRGui::ReloadFonts(float scale)
{
	ImFontAtlas* fonts = ImGui::GetIO().Fonts;
	fonts->Clear();

	LoadFont("fonts/DroidSans.ttf", 20 * scale);

	ImGui_ImplDX10_InvalidateDeviceObjects();
	fonts->Build();
}

void VRGui::Draw()
{
	Vec2i windowSize = gVR->GetRenderSize();
	ImGui::Begin("VR", 0, ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoTitleBar);
	ImGui::SetWindowPos(ImVec2(0.02f * windowSize.x, 0.85f * windowSize.y));
	ImGui::SetWindowSize(ImVec2(0.25f * windowSize.x, 0.1f * windowSize.y));
	bool settingsClicked = ImGui::Button("VR Settings");
	ImGui::Button("VR Manual");
	ImGui::End();

	if (settingsClicked)
	{
		ImGui::Begin("VR Settings");
		ImGui::Text("This is a stub for a VR settings menu");
		ImGui::End();
	}
}

ImFont* VRGui::LoadFont(const char* filename, float fontSize)
{
	CCryFile file;
	if (!file.Open(filename, "rb"))
	{
		CryLogAlways("ERROR: Failed to open font file %s", filename);
		return nullptr;
	}

	size_t dataSize = file.GetLength();
	void* data = IM_ALLOC(dataSize);
	file.ReadRaw(data, dataSize);

	return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, dataSize, fontSize);
}
