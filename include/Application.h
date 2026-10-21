#pragma once
#include "Renderer.h"
#include <dwrite.h>
#include <array>
#include <vector>

namespace qis {
// Window-independent workbench: the same UI can render to an offscreen test target.
class Application {
public:
    void Initialize(HWND window, UINT width, UINT height, bool software = false);
    void Resize(UINT width, UINT height, float dpiScale = 1.0f);
    void Render(float fps = 0);
    void LoadImage(const std::filesystem::path& path);
    void SavePreview(const std::filesystem::path& path) { m_renderer.SavePreview(path); }
    void MouseMove(float x, float y);
    void MouseDown(float x, float y);
    void MouseUp();
    void KeyDown(UINT key, bool control);
    void ShowError(const std::exception& error);
    bool Ready() const { return m_renderer.Ready(); }
    float DisplayScale() const { return m_dpi; }
    IDXGISwapChain* SwapChain() const { return m_renderer.SwapChain(); }
    LPCWSTR Cursor() const;

private:
    struct Hit { D2D1_RECT_F rect; int action; };
    D2D1_RECT_F ImageRect() const;
    void LoadDemo();
    void DrawUI(float fps);
    void Act(int action);
    void OpenDialog();
    void ExportDialog();
    void Drag(float x);
    void Text(const std::wstring& text, D2D1_RECT_F rect, int style = 0, UINT color = 0xE7EBF3);
    void Fill(D2D1_RECT_F rect, UINT color, float radius = 0, float alpha = 1);
    void Line(float x1, float y1, float x2, float y2, UINT color, float width = 1);
    void Button(D2D1_RECT_F rect, const std::wstring& label, int action, bool active = false, bool primary = false);
    Renderer m_renderer;
    HWND m_window = nullptr;
    UpscaleSettings m_settings;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_write;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brush;
    std::array<Microsoft::WRL::ComPtr<IDWriteTextFormat>, 5> m_fonts;
    std::vector<Hit> m_hits;
    std::wstring m_filename, m_status = L"Ready. Drag an image into the window to begin.";
    float m_width = 1440, m_height = 900, m_dpi = 1;
    float m_split = 0.5f, m_mouseX = -1, m_mouseY = -1;
    UINT m_comparison = 1;
    int m_scalePreset = 1, m_outputPreset = 1, m_drag = 0;
    bool m_dirty = true;
};
}
