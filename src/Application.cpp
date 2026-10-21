#include "Application.h"
#include <commdlg.h>
#include <d2d1_1helper.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace qis {
namespace {
constexpr UINT kAccent = 0xA4EDBA, kMuted = 0x8D97A9, kBorder = 0x2A3241;
constexpr float kSidebar = 284;
bool Contains(const D2D1_RECT_F& r, float x, float y) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}
std::wstring Dimensions(UINT w, UINT h) { return std::to_wstring(w) + L" x " + std::to_wstring(h); }
D2D1_COLOR_F Color(UINT rgb, float alpha = 1) { return D2D1::ColorF(rgb, alpha); }
}

void Application::Initialize(HWND window, UINT width, UINT height, bool software) {
    m_window = window;
    m_renderer.Initialize(window, width, height, software);
    Check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_write.GetAddressOf())), "Create text factory");
    const float sizes[] = {13, 11, 18, 30, 13};
    for (size_t i = 0; i < m_fonts.size(); ++i) {
        Check(m_write->CreateTextFormat(L"Segoe UI", nullptr,
            i >= 2 ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sizes[i], L"en-us", &m_fonts[i]), "Create interface font");
        m_fonts[i]->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        m_fonts[i]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    m_fonts[4]->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    Check(m_renderer.UI()->CreateSolidColorBrush(Color(0xFFFFFF), &m_brush), "Create interface brush");
    m_width = static_cast<float>(width);
    m_height = static_cast<float>(height);
    LoadDemo();
}

void Application::Resize(UINT width, UINT height, float dpiScale) {
    if (!width || !height) return;
    // Fit the whole workbench on laptops whose desktop scaling leaves fewer
    // logical pixels than the normal minimum layout.
    m_dpi = std::max(0.5f, std::min({dpiScale, width / 1080.0f, height / 740.0f}));
    m_width = width / m_dpi;
    m_height = height / m_dpi;
    m_renderer.Resize(width, height);
}

void Application::LoadImage(const std::filesystem::path& path) {
    m_renderer.LoadImage(path);
    m_filename = path.filename().wstring();
    m_status = L"Image loaded. Adjust the controls or drag the comparison divider.";
    m_dirty = true;
}

void Application::LoadDemo() {
    const auto path = ExecutableDirectory() / L"assets" / L"demo.png";
    if (std::filesystem::exists(path)) LoadImage(path);
    else {
        m_renderer.SetImage(CreateTestPattern());
        m_filename = L"Built-in resolution chart";
    }
    m_status = L"Demo ready. Drop your own image here, or choose Open image.";
    m_dirty = true;
}

D2D1_RECT_F Application::ImageRect() const {
    const float left = kSidebar + 28, top = 218, right = m_width - 28, bottom = m_height - 122;
    const float width = std::max(1.0f, right - left), height = std::max(1.0f, bottom - top);
    const float scale = std::min(width / m_settings.width, height / m_settings.height);
    const float w = m_settings.width * scale, h = m_settings.height * scale;
    const float x = left + (width - w) * 0.5f, y = top + (height - h) * 0.5f;
    return D2D1::RectF(x, y, x + w, y + h);
}

void Application::Render(float fps) {
    if (m_dirty) {
        m_renderer.Process(m_settings);
        m_dirty = false;
    }
    const auto image = ImageRect();
    m_renderer.BeginPreview(D2D1::RectF(image.left * m_dpi, image.top * m_dpi,
        image.right * m_dpi, image.bottom * m_dpi), m_split, m_comparison);
    auto* ui = m_renderer.UI();
    ui->BeginDraw();
    ui->SetTransform(D2D1::Matrix3x2F::Scale(m_dpi, m_dpi));
    DrawUI(fps);
    Check(ui->EndDraw(), "Draw workbench interface");
    // Present is called exactly once, after both the preview and the UI.
    m_renderer.Present();
}

void Application::Fill(D2D1_RECT_F rect, UINT color, float radius, float alpha) {
    m_brush->SetColor(Color(color, alpha));
    if (radius) m_renderer.UI()->FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), m_brush.Get());
    else m_renderer.UI()->FillRectangle(rect, m_brush.Get());
}
void Application::Line(float x1, float y1, float x2, float y2, UINT color, float width) {
    m_brush->SetColor(Color(color));
    m_renderer.UI()->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), m_brush.Get(), width);
}
void Application::Text(const std::wstring& text, D2D1_RECT_F rect, int style, UINT color) {
    m_brush->SetColor(Color(color));
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    Check(m_write->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), m_fonts[style].Get(),
        std::max(1.0f, rect.right - rect.left), std::max(1.0f, rect.bottom - rect.top), &layout), "Lay out interface text");
    DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    Microsoft::WRL::ComPtr<IDWriteInlineObject> ellipsis;
    Check(m_write->CreateEllipsisTrimmingSign(m_fonts[style].Get(), &ellipsis), "Create text trimming");
    layout->SetTrimming(&trim, ellipsis.Get());
    m_renderer.UI()->DrawTextLayout(D2D1::Point2F(rect.left, rect.top), layout.Get(), m_brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
void Application::Button(D2D1_RECT_F rect, const std::wstring& label, int action, bool active, bool primary) {
    const bool hover = Contains(rect, m_mouseX, m_mouseY);
    Fill(rect, primary ? (hover ? 0xB9F5CB : kAccent) : active ? 0x2B4039 : hover ? 0x2A3342 : 0x1C2431, 6);
    Text(label, rect, 4, primary ? 0x13261D : active ? kAccent : 0xD4DBE7);
    m_hits.push_back({rect, action});
}

void Application::DrawUI(float fps) {
    m_hits.clear();
    auto r = [](float x, float y, float w, float h) { return D2D1::RectF(x, y, x + w, y + h); };
    const float mainX = kSidebar + 28;
    Fill(r(0, 0, kSidebar, m_height - 36), 0x121925);
    Fill(r(kSidebar, 0, m_width - kSidebar, 76), 0x101620);
    Line(kSidebar, 0, kSidebar, m_height - 36, kBorder);
    Line(0, 76, m_width, 76, kBorder);
    Fill(r(24, 22, 32, 32), kAccent, 8);
    Text(L"Q", r(24, 21, 32, 32), 4, 0x14271B);
    Text(L"QIS-X", r(68, 17, 170, 27), 2);
    Text(L"SPATIAL UPSCALING", r(69, 43, 184, 16), 1, kMuted);
    Text(L"Image workbench", r(mainX, 21, 190, 32), 0);
    Text(L"PROTOTYPE / 0.2", r(mainX + 165, 22, 130, 30), 1, kMuted);
    Button(r(m_width - 256, 20, 108, 36), L"Open image", 1);
    Button(r(m_width - 136, 20, 108, 36), L"Export PNG", 2, false, true);

    Text(L"01 / SOURCE", r(24, 100, 230, 24), 1, kMuted);
    Fill(r(24, 136, 236, 72), 0x1B2330, 8);
    Fill(r(38, 153, 5, 38), kAccent, 2);
    Text(m_filename, r(55, 145, 192, 25), 0);
    Text(Dimensions(m_renderer.SourceWidth(), m_renderer.SourceHeight()) + L"  /  STILL IMAGE",
        r(55, 174, 192, 18), 1, kMuted);
    Button(r(24, 220, 236, 36), L"+  Choose an image", 1);
    Text(L"or drag and drop a file", r(24, 263, 236, 20), 1, kMuted);
    Line(24, 299, 260, 299, kBorder);
    Text(L"02 / RECONSTRUCTION", r(24, 310, 236, 24), 1, kMuted);
    const wchar_t* algorithms[] = {L"Bilinear", L"Bicubic", L"QIS-X"};
    for (int i = 0; i < 3; ++i) Button(r(24.0f + i * 80, 346, 76, 34), algorithms[i], 10 + i, m_settings.algorithm == static_cast<UINT>(i));
    const wchar_t* descriptions[] = {L"Fast, smooth interpolation.", L"Cubic detail reconstruction.", L"Bicubic + adaptive sharpening."};
    Text(descriptions[m_settings.algorithm], r(24, 386, 236, 24), 1, kMuted);
    Text(L"Render scale", r(24, 424, 140, 24));
    Text(L"INPUT", r(204, 424, 56, 24), 1, kMuted);
    const wchar_t* scales[] = {L"50%", L"67%", L"77%"};
    for (int i = 0; i < 3; ++i) Button(r(24.0f + i * 80, 458, 76, 34), scales[i], 20 + i, i == m_scalePreset);
    Text(L"Output resolution", r(24, 514, 220, 24));
    const wchar_t* outputs[] = {L"720p", L"1080p", L"1440p"};
    for (int i = 0; i < 3; ++i) Button(r(24.0f + i * 80, 548, 76, 34), outputs[i], 30 + i, i == m_outputPreset);
    Text(L"Sharpness", r(24, 604, 150, 24));
    Text(std::to_wstring(static_cast<int>(std::lround(m_settings.sharpness * 100))) + L"%", r(215, 604, 45, 24), 0, kAccent);
    const bool sharpen = m_settings.algorithm == 2;
    Fill(r(24, 643, 236, 4), 0x303A49, 2);
    Fill(r(24, 643, 236 * m_settings.sharpness, 4), sharpen ? kAccent : 0x637369, 2);
    m_brush->SetColor(Color(sharpen ? kAccent : 0x637369));
    m_renderer.UI()->FillEllipse(D2D1::Ellipse(D2D1::Point2F(24 + 236 * m_settings.sharpness, 645), 7, 7), m_brush.Get());
    if (sharpen) m_hits.push_back({r(17, 632, 250, 26), 50});
    Text(sharpen ? L"Edge-aware. Tuned to limit halos." : L"Select QIS-X to adjust sharpness.", r(24, 664, 236, 22), 1, kMuted);
    if (m_height > 825) {
        Line(24, m_height - 146, 260, m_height - 146, kBorder);
        Text(L"BUILT FOR THE NEXT FRAME", r(24, m_height - 132, 236, 24), 1, kAccent);
        Text(L"Lightweight spatial processing.", r(24, m_height - 104, 236, 22), 1, kMuted);
        Text(L"Image prototype / game capture next", r(24, m_height - 80, 236, 22), 1, kMuted);
    }

    Text(L"Make every pixel count.", r(mainX, 96, m_width - mainX - 24, 43), 3);
    Text(L"Explore spatial upscaling. Compare the detail, then save the result.", r(mainX, 141, m_width - mainX - 24, 24), 0, kMuted);
    Text(L"LIVE PREVIEW", r(mainX, 178, 145, 26), 1, kAccent);
    const float tabsX = m_width - 318;
    Button(r(tabsX, 174, 88, 30), L"Bilinear", 42, m_comparison == 2);
    Button(r(tabsX + 94, 174, 88, 30), L"Compare", 41, m_comparison == 1);
    Button(r(tabsX + 188, 174, 102, 30), L"Enhanced", 40, m_comparison == 0);

    const auto image = ImageRect();
    m_brush->SetColor(Color(kBorder));
    m_renderer.UI()->DrawRectangle(image, m_brush.Get());
    if (m_comparison != 0) {
        Fill(r(image.left + 14, image.top + 14, 134, 26), 0x0E1622, 4, 0.90f);
        Text(L"BILINEAR BASELINE", r(image.left + 24, image.top + 14, 116, 26), 1);
    }
    if (m_comparison != 2) {
        const std::wstring label = m_settings.algorithm == 2 ? L"QIS-X ENHANCED" : m_settings.algorithm == 1 ? L"BICUBIC" : L"BILINEAR";
        Fill(r(image.right - 148, image.top + 14, 134, 26), 0x0E1622, 4, 0.90f);
        Text(label, r(image.right - 138, image.top + 14, 116, 26), 1, kAccent);
    }
    if (m_comparison == 1) {
        const float splitX = image.left + (image.right - image.left) * m_split;
        Line(splitX, image.top, splitX, image.bottom, 0xE7F3ED, 1.5f);
        Fill(r(splitX - 17, (image.top + image.bottom) * 0.5f - 22, 34, 44), 0xE7F3ED, 8);
        Text(L"\u2039  \u203a", r(splitX - 17, (image.top + image.bottom) * 0.5f - 22, 34, 44), 4, 0x14231B);
    }
    Text(m_comparison == 1 ? L"Drag the divider to inspect the difference" : L"Press B to cycle comparison views",
        r(mainX, m_height - 111, m_width - mainX - 30, 22), 1, kMuted);
    Text(Dimensions(m_renderer.InputWidth(), m_renderer.InputHeight()) + L"  \u2192  " + Dimensions(m_settings.width, m_settings.height),
        r(mainX, m_height - 79, 340, 26), 0);
    Text(L"Same input / two reconstructions", r(m_width - 278, m_height - 79, 250, 26), 1, kMuted);

    Fill(r(0, m_height - 36, m_width, 36), 0x0F151F);
    Line(0, m_height - 36, m_width, m_height - 36, kBorder);
    Fill(r(18, m_height - 21, 6, 6), m_renderer.Software() ? 0xF0C879 : kAccent, 3);
    Text(m_renderer.Software() ? L"SOFTWARE / WARP" : L"DIRECT3D 11", r(34, m_height - 32, 160, 28), 1, kMuted);
    Text(m_status, r(200, m_height - 32, m_width - 440, 28), 1, kMuted);
    std::wostringstream performance;
    if (fps > 0) performance << std::fixed << std::setprecision(0) << fps << L" UI fps   /   ";
    performance << L"F1  Shortcuts";
    Text(performance.str(), r(m_width - 215, m_height - 32, 197, 28), 1, kMuted);
}

void Application::OpenDialog() {
    std::vector<wchar_t> filename(32768);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = m_window;
    dialog.lpstrFile = filename.data();
    dialog.nMaxFile = static_cast<DWORD>(filename.size());
    dialog.lpstrFilter = L"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff\0All files\0*.*\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (GetOpenFileNameW(&dialog)) LoadImage(filename.data());
    else if (const auto error = CommDlgExtendedError()) Check(HRESULT_FROM_WIN32(error), "Open image dialog");
}

void Application::ExportDialog() {
    std::vector<wchar_t> filename(32768);
    const std::wstring suggested = L"qis-x-" + std::to_wstring(m_settings.height) + L"p.png";
    std::copy(suggested.begin(), suggested.end(), filename.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = m_window;
    dialog.lpstrFile = filename.data();
    dialog.nMaxFile = static_cast<DWORD>(filename.size());
    dialog.lpstrFilter = L"PNG image\0*.png\0";
    dialog.lpstrDefExt = L"png";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (GetSaveFileNameW(&dialog)) {
        if (m_dirty) { m_renderer.Process(m_settings); m_dirty = false; }
        const std::filesystem::path destination(filename.data());
        auto temporary = destination;
        temporary += L".qis-" + std::to_wstring(GetTickCount64()) + L".tmp";
        try {
            m_renderer.SaveOutput(temporary);
            if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                Check(HRESULT_FROM_WIN32(GetLastError()), "Save exported PNG");
        } catch (...) { std::error_code ignored; std::filesystem::remove(temporary, ignored); throw; }
        m_status = L"Exported " + destination.filename().wstring() + L" / " + Dimensions(m_settings.width, m_settings.height);
    } else if (const auto error = CommDlgExtendedError()) Check(HRESULT_FROM_WIN32(error), "Export PNG dialog");
}

void Application::Act(int action) {
    if (action == 1) OpenDialog();
    else if (action == 2) ExportDialog();
    else if (action >= 10 && action <= 12) { m_settings.algorithm = action - 10; m_dirty = true; }
    else if (action >= 20 && action <= 22) {
        const float scales[] = {0.5f, 2.0f / 3.0f, 0.77f};
        m_scalePreset = action - 20;
        m_settings.renderScale = scales[m_scalePreset];
        m_dirty = true;
    } else if (action >= 30 && action <= 32) {
        const UINT widths[] = {1280, 1920, 2560}, heights[] = {720, 1080, 1440};
        m_outputPreset = action - 30;
        m_settings.width = widths[m_outputPreset];
        m_settings.height = heights[m_outputPreset];
        m_dirty = true;
    } else if (action >= 40 && action <= 42) m_comparison = action - 40;
}
void Application::Drag(float x) {
    if (m_drag == 1) { m_settings.sharpness = std::clamp((x - 24) / 236.0f, 0.0f, 1.0f); m_dirty = true; }
    if (m_drag == 2) {
        const auto image = ImageRect();
        m_split = std::clamp((x - image.left) / (image.right - image.left), 0.02f, 0.98f);
    }
}
void Application::MouseMove(float x, float y) { m_mouseX = x; m_mouseY = y; Drag(x); }
void Application::MouseDown(float x, float y) {
    m_mouseX = x; m_mouseY = y;
    for (const auto& hit : m_hits) if (Contains(hit.rect, x, y)) {
        const int action = hit.action;
        if (action == 50) { m_drag = 1; Drag(x); }
        else Act(action);
        return;
    }
    if (m_comparison == 1 && Contains(ImageRect(), x, y)) { m_drag = 2; Drag(x); }
}
void Application::MouseUp() { m_drag = 0; }
LPCWSTR Application::Cursor() const {
    if (m_drag == 2 || (m_comparison == 1 && Contains(ImageRect(), m_mouseX, m_mouseY))) return IDC_SIZEWE;
    for (const auto& hit : m_hits) if (Contains(hit.rect, m_mouseX, m_mouseY)) return IDC_HAND;
    return IDC_ARROW;
}

void Application::KeyDown(UINT key, bool control) {
    if (control && key == 'O') Act(1);
    else if (control && key == 'S') Act(2);
    else if (key >= '1' && key <= '3') Act(10 + key - '1');
    else if (key == 'B') m_comparison = (m_comparison + 1) % 3;
    else if (key == 'P') Act(20 + (m_scalePreset + 1) % 3);
    else if (key == 'O') Act(30 + (m_outputPreset + 1) % 3);
    else if (key == 'D') LoadDemo();
    else if (key == 'R') {
        m_settings = {};
        m_scalePreset = m_outputPreset = 1;
        m_split = 0.5f; m_comparison = 1; m_dirty = true;
        m_status = L"Controls reset to defaults.";
    } else if (key == VK_LEFT || key == VK_RIGHT)
        m_split = std::clamp(m_split + (key == VK_LEFT ? -0.02f : 0.02f), 0.02f, 0.98f);
    else if (key == VK_OEM_PLUS || key == VK_ADD || key == VK_OEM_MINUS || key == VK_SUBTRACT) {
        m_settings.sharpness = std::clamp(m_settings.sharpness +
            ((key == VK_OEM_PLUS || key == VK_ADD) ? 0.05f : -0.05f), 0.0f, 1.0f);
        m_dirty = true;
    } else if (key == VK_F1) MessageBoxW(m_window, (std::wstring(
        L"Ctrl+O   Open image\nCtrl+S   Export enhanced PNG\n1 / 2 / 3   Bilinear / Bicubic / QIS-X\n"
        L"B   Cycle preview mode\nLeft / Right   Move comparison divider\n+ / -   Adjust sharpness\n"
        L"P   Cycle render scale\nO   Cycle output resolution\nD   Load demo image\nR   Reset controls\n\n"
        L"Both sides use the same low-resolution input. Export always saves the enhanced output.\n"
        L"UI FPS measures presentation, not game performance.\n\nGPU: ") + m_renderer.AdapterName()).c_str(),
        L"QIS-X / Keyboard shortcuts", MB_OK | MB_ICONINFORMATION);
}
void Application::ShowError(const std::exception& error) {
    const std::string message(error.what());
    const std::wstring wide(message.begin(), message.end());
    m_status = L"Operation failed. " + wide;
    if (m_window) MessageBoxW(m_window, wide.c_str(), L"QIS-X / Could not complete the operation", MB_OK | MB_ICONERROR);
}
}
