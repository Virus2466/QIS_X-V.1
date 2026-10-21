#pragma once
#include "Texture.h"
#include <d2d1_1.h>
#include <dxgi1_3.h>
#include <string>

namespace qis {
struct UpscaleSettings {
    UINT width = 1920, height = 1080;
    float renderScale = 0.666667f;
    float sharpness = 0.55f;
    UINT algorithm = 2; // bilinear, bicubic, adaptive sharpen
};

class Renderer {
public:
    void Initialize(HWND window, UINT width, UINT height, bool forceSoftware = false);
    void Resize(UINT width, UINT height);
    void LoadImage(const std::filesystem::path& path);
    void SetImage(const ImagePixels& image);
    void Process(const UpscaleSettings& settings);
    void BeginPreview(const D2D1_RECT_F& imageRect, float split, UINT comparison);
    void Present();
    void SaveOutput(const std::filesystem::path& path, bool reference = false);
    void SavePreview(const std::filesystem::path& path);
    ImagePixels ReadOutput(bool reference = false);
    ID2D1DeviceContext* UI() const { return m_ui.Get(); }
    IDXGISwapChain* SwapChain() const { return m_swapChain.Get(); }
    UINT SourceWidth() const { return m_source.width; }
    UINT SourceHeight() const { return m_source.height; }
    UINT InputWidth() const { return m_low.width; }
    UINT InputHeight() const { return m_low.height; }
    const std::wstring& AdapterName() const { return m_adapterName; }
    bool Software() const { return m_software; }
    bool Ready() const { return m_device != nullptr; }

private:
    template<class T> using Ptr = Microsoft::WRL::ComPtr<T>;
    struct Target : Texture { Ptr<ID3D11RenderTargetView> rtv; };
    struct Constants {
        float sourceSize[2]{};
        float sharpness = 0;
        UINT algorithm = 0;
        float split = 0.5f;
        UINT comparison = 0;
        float padding[2]{};
    };
    Target MakeTarget(UINT width, UINT height);
    void Draw(ID3D11RenderTargetView* target, const D2D1_RECT_F& viewport,
        ID3D11ShaderResourceView* source, ID3D11PixelShader* shader,
        const Constants& constants, ID3D11ShaderResourceView* reference = nullptr);
    ImagePixels ReadTexture(ID3D11Texture2D* texture);
    Ptr<ID3DBlob> Compile(const char* entry, const char* profile);
    Ptr<ID3D11Device> m_device;
    Ptr<ID3D11DeviceContext> m_context;
    Ptr<IDXGISwapChain1> m_swapChain;
    Ptr<ID3D11VertexShader> m_vs;
    Ptr<ID3D11PixelShader> m_copyPS, m_upscalePS, m_comparePS;
    Ptr<ID3D11Buffer> m_constants;
    Ptr<ID3D11SamplerState> m_linear, m_point;
    Ptr<ID2D1Factory1> m_uiFactory;
    Ptr<ID2D1Device> m_uiDevice;
    Ptr<ID2D1DeviceContext> m_ui;
    Ptr<ID2D1Bitmap1> m_uiTarget;
    Texture m_source;
    Target m_back, m_low, m_output, m_reference;
    std::wstring m_adapterName;
    bool m_software = false;
};
}
