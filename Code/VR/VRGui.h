#pragma once

struct ImFont;
struct ID3D10Device;

class VRGui
{
public:
    void Init(ID3D10Device* device);
    void Shutdown();

    void SetScale(float scale);

    void Render();

private:
    void SetupStyle(float scale);
    void ReloadFonts(float scale);
    void Draw();

    ImFont* LoadFont(const char* filename, float x);

    float m_currentScale = 0.0f;
};
