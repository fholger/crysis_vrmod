#pragma once

struct ImFont;
struct ID3D10Device;

class VRGui
{
public:
    void Init(ID3D10Device* device);
    void Shutdown();

    void SetScale(float scale);
    void ClickedNoFocus();

    void Render();

private:
    void SetupStyle(float scale);
    void ReloadFonts(float scale);
    void Draw();

    void DrawSettingsMenu();
    void DrawManualWindow();

    void LoadManualSections();

    ImFont* LoadFont(const char* filename, float x);

    float m_currentScale = 0.0f;
    bool m_settingsMenuOpen = false;
    bool m_manualWindowOpen = false;

    ImFont* m_fontText = nullptr;
    ImFont* m_fontH1 = nullptr;
    ImFont* m_fontH2 = nullptr;
    ImFont* m_fontH3 = nullptr;

    struct ManualSection
    {
        CryStringLocal filename;
        CryStringLocal name;
        std::vector<char> content;
    };
    std::vector<ManualSection> m_manualSections;
    int m_currentManualSection = 0;
};
