#include "StdAfx.h"
#include "VRGui.h"

#include "GameCVars.h"
#include "imgui.h"
#include "imgui_markdown.h"
#include "VRManager.h"
#include "VRRenderer.h"
#include "backends/imgui_impl_dx10.h"
#include "Menus/OptionsManager.h"

static void OpenLinkInBrowser(const std::string& url)
{
	ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNOACTIVATE);
	Sleep(5000);
	SwitchToThisWindow((HWND)gEnv->pRenderer->GetHWND(), TRUE);
}

static void MarkdownLinkCallback(ImGui::MarkdownLinkCallbackData data)
{
	std::string url(data.link, data.linkLength);
	if (!data.isImage)
	{
		OpenLinkInBrowser(url);
	}
}

void VRGui::Init(ID3D10Device* device)
{
	LoadManualSections();
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

void VRGui::ClickedNoFocus()
{
	m_settingsMenuOpen = false;
	m_manualWindowOpen = false;
}

void VRGui::Render()
{
	Vec2i windowSize = gVRRenderer->GetWindowSize();
	Vec2i renderSize = gVR->GetRenderSize();
	SetScale(g_pGameCVars->vr_gui_scale * windowSize.y / 1080.f * max(renderSize.y / windowSize.y, renderSize.x / windowSize.x));
	ImGui::GetIO().DisplaySize = ImVec2((float)renderSize.x, (float)renderSize.y);
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
	style.ItemSpacing = ImVec2(16, 16);
	style.ScaleAllSizes(scale);
}

void VRGui::ReloadFonts(float scale)
{
	ImFontAtlas* fonts = ImGui::GetIO().Fonts;
	fonts->Clear();

	LoadFont("fonts/DroidSans.ttf", 20 * scale);
	m_fontText = LoadFont("fonts/Karla-Regular.ttf", 16 * scale);
	m_fontH1 = LoadFont("fonts/Roboto-Medium.ttf", 32 * scale);
	m_fontH2 = LoadFont("fonts/Roboto-Medium.ttf", 26 * scale);
	m_fontH3 = LoadFont("fonts/Roboto-Medium.ttf", 20 * scale);

	ImGui_ImplDX10_InvalidateDeviceObjects();
	fonts->Build();
}

void VRGui::Draw()
{
	ImVec2 windowSize = ImGui::GetIO().DisplaySize;
	ImGui::Begin("VR", 0, ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoTitleBar);
	ImGui::SetWindowPos(ImVec2(0.02f * windowSize.x, 0.85f * windowSize.y));
	ImGui::SetWindowSize(ImVec2(0.25f * windowSize.x, 0.2f * windowSize.y));
	m_settingsMenuOpen = ImGui::Button("VR Settings") || m_settingsMenuOpen;
	m_manualWindowOpen = ImGui::Button("VR Manual") || m_manualWindowOpen;

	ImGui::Spacing();
	if (ImGui::Button("Donate"))
	{
		OpenLinkInBrowser("https://ko-fi.com/fholger");
	}
	ImGui::End();

	if (m_settingsMenuOpen)
	{
		DrawSettingsMenu();
	}

	if (m_manualWindowOpen)
	{
		DrawManualWindow();
	}
}

void VRGui::DrawSettingsMenu()
{
	ImVec2 windowSize = ImGui::GetIO().DisplaySize;

	ImGui::Begin("VR Settings", &m_settingsMenuOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
	ImGui::SetWindowPos(ImVec2(0.25f * windowSize.x, 0.25f * windowSize.y));
	ImGui::SetWindowSize(ImVec2(0.7f * windowSize.x, 0.7f * windowSize.y));
	ImGui::Text("Configure your VR experience here");
	ImGui::Separator();

	ImGui::Text("Weapon hand");
	ImGui::SetItemTooltip("Choose which hand will hold and fire weapons");
	ImGui::SameLine(0.2f * windowSize.x);
	ImGui::RadioButton("Left##weapon", &g_pGameCVars->vr_weapon_hand, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Right##weapon", &g_pGameCVars->vr_weapon_hand, 1);
	ImGui::Text("Movement hand");
	ImGui::SetItemTooltip("This hand's thumbstick is used for moving, other hand for turning");
	ImGui::SameLine(0.2f * windowSize.x);
	ImGui::RadioButton("Left##movement", &g_pGameCVars->vr_movement_hand, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Right##movement", &g_pGameCVars->vr_movement_hand, 1);
	ImGui::Separator();

	bool weaponCrosshair = g_pGameCVars->vr_weapon_crosshair;
	ImGui::Checkbox("Crosshair helper for player weapons", &weaponCrosshair);
	ImGui::SetItemTooltip("Draws a small sphere to help you aim");
	g_pGameCVars->vr_weapon_crosshair = weaponCrosshair;
	bool vehicleCrosshair = g_pGameCVars->vr_vehicle_crosshair;
	ImGui::Checkbox("Crosshair helper for mounted/vehicle weapons", &vehicleCrosshair);
	ImGui::SetItemTooltip("Draws a small sphere to help you aim");
	g_pGameCVars->vr_vehicle_crosshair = vehicleCrosshair;

	ImGui::SliderAngle("Weapon angle offset", &g_pGameCVars->vr_weapon_angle_offset, -45.f, 45.f);
	ImGui::SetItemTooltip("Change the gun pitch relative to your controller");
	ImGui::Separator();

	bool hapticsEnabled = g_pGameCVars->vr_haptics_enabled;
	ImGui::Checkbox("Enable controller haptics", &hapticsEnabled);
	g_pGameCVars->vr_haptics_enabled = hapticsEnabled;
	ImGui::SliderFloat("Haptics strength", &g_pGameCVars->vr_haptics_strength, 0.f, 2.f);
	ImGui::Separator();

	ImGui::Text("Mirrored eye");
	ImGui::SetItemTooltip("Choose which eye will be shown in the game's desktop window");
	ImGui::SameLine(0.2f * windowSize.x);
	ImGui::RadioButton("Left##eye", &g_pGameCVars->vr_mirror_eye, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Right##eye", &g_pGameCVars->vr_mirror_eye, 1);

	const char* CutsceneOptions[] = {"VR", "2D Cinema", "3D Cinema"};
	int cutsceneMode = g_pGameCVars->vr_cutscenes_2d ? (g_pGameCVars->vr_cinema_3d ? 2 : 1) : 0;
	ImGui::Combo("Cutscene mode", &cutsceneMode, CutsceneOptions, IM_ARRAYSIZE(CutsceneOptions));
	g_pGameCVars->vr_cutscenes_2d = cutsceneMode > 0;
	g_pGameCVars->vr_cinema_3d = cutsceneMode == 2;

	ImGui::Separator();
	if (ImGui::Button("Close"))
	{
		m_settingsMenuOpen = false;
	}

	ImGui::End();

	if (!m_settingsMenuOpen)
	{
		// make sure any changes we made get saved to the game.cfg file
		g_pGame->GetOptions()->WriteGameCfg();
	}
}

void VRGui::DrawManualWindow()
{
	ImVec2 windowSize = ImGui::GetIO().DisplaySize;

	ImGui::Begin("VR Manual", &m_manualWindowOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
	ImGui::Spacing();
	ImGui::SetWindowPos(ImVec2(0.1f * windowSize.x, 0.1f * windowSize.y));
	ImGui::SetWindowSize(ImVec2(0.8f * windowSize.x, 0.8f * windowSize.y));

	ImGui::BeginGroup();
	for (int i = 0; i < m_manualSections.size(); i++)
	{
		if (ImGui::Selectable(m_manualSections[i].name.c_str(), m_currentManualSection == i, 0, ImVec2(.15f * windowSize.x, 0)))
			m_currentManualSection = i;
	}
	ImGui::EndGroup();
	ImGui::SameLine();

	ImGui::MarkdownConfig config;
	config.headingFormats[0] = { m_fontH1, true };
	config.headingFormats[1] = { m_fontH2, true };
	config.headingFormats[2] = { m_fontH3, false };
	config.linkCallback = &MarkdownLinkCallback;
	ImGui::BeginChild("manual_text");
	ImGui::PushFont(m_fontText);
	if (m_currentManualSection >= 0 && m_currentManualSection < m_manualSections.size())
	{
		const ManualSection& section = m_manualSections[m_currentManualSection];
		ImGui::Markdown(section.content.data(), section.content.size(), config);
	}
	ImGui::PopFont();
	ImGui::EndChild();

	ImGui::End();
}

void VRGui::LoadManualSections()
{
	_finddata_t fd;
	intptr_t handle = gEnv->pCryPak->FindFirst("Manual/*.md", &fd, ICryPak::FLAGS_ONLY_MOD_DIRS);
	if (handle >= 0)
	{
		do
		{
			CryLogAlways("Found manual file: %s", fd.name);
			ManualSection section;
			section.filename = fd.name;
			section.name = section.filename.substr(3, section.filename.length() - 6).replace('_', ' ');

			CryStringLocal filepath = "Manual/" + section.filename;
			CCryFile file(filepath.c_str(), "r");
			size_t fileSize = file.GetLength();

			section.content.resize(fileSize);
			file.ReadRaw(section.content.data(), fileSize);

			m_manualSections.emplace_back(section);
		}
		while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);
		gEnv->pCryPak->FindClose(handle);
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
