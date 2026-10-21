#include "Renderer.h"
#include "ShaderSource.h"
#include <d3dcompiler.h>
#include <d2d1_1helper.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace qis {
namespace {
D2D1_RECT_F Full(UINT width, UINT height) {
    return D2D1::RectF(0, 0, static_cast<float>(width), static_cast<float>(height));
}
constexpr float kBackground[] = {0.025f, 0.032f, 0.044f, 1.0f};
}

Microsoft::WRL::ComPtr<ID3DBlob> Renderer::Compile(const char* entry, const char* profile) {
    Ptr<ID3DBlob> code, errors;
    const HRESULT hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1, "QIS-X shaders", nullptr,
        nullptr, entry, profile, D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0, &code, &errors);
    if (FAILED(hr) && errors) throw std::runtime_error(static_cast<const char*>(errors->GetBufferPointer()));
    Check(hr, "Compile embedded shader");
    return code;
}

void Renderer::Initialize(HWND window, UINT width, UINT height, bool forceSoftware) {
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    auto createDevice = [&](D3D_DRIVER_TYPE driver) {
        m_context.Reset();
        m_device.Reset();
        return D3D11CreateDevice(nullptr, driver, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels, 1, D3D11_SDK_VERSION, &m_device, nullptr, &m_context);
    };
    HRESULT hr = forceSoftware ? E_FAIL : createDevice(D3D_DRIVER_TYPE_HARDWARE);
    if (FAILED(hr)) {
        m_software = true;
        Check(createDevice(D3D_DRIVER_TYPE_WARP), "Create Direct3D 11 device (including software fallback)");
    }
    Ptr<IDXGIDevice> dxgiDevice;
    Check(m_device.As(&dxgiDevice), "Get DXGI device");
    Ptr<IDXGIAdapter> adapter;
    Check(dxgiDevice->GetAdapter(&adapter), "Get graphics adapter");
    DXGI_ADAPTER_DESC adapterDesc{};
    Check(adapter->GetDesc(&adapterDesc), "Read graphics adapter");
    m_adapterName = adapterDesc.Description;
    if (window) {
        Ptr<IDXGIFactory2> factory;
        Check(adapter->GetParent(IID_PPV_ARGS(&factory)), "Get swap chain factory");
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        Check(factory->CreateSwapChainForHwnd(m_device.Get(), window, &desc, nullptr, nullptr,
            &m_swapChain), "Create window swap chain");
        factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
    }
    Check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), nullptr,
        reinterpret_cast<void**>(m_uiFactory.GetAddressOf())), "Create UI factory");
    Check(m_uiFactory->CreateDevice(dxgiDevice.Get(), &m_uiDevice), "Create UI device");
    Check(m_uiDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_ui), "Create UI context");
    m_ui->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
    const auto vs = Compile("VS_main", "vs_5_0");
    Check(m_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &m_vs), "Create vertex shader");
    auto pixelShader = [&](const char* name, Ptr<ID3D11PixelShader>& shader) {
        const auto code = Compile(name, "ps_5_0");
        Check(m_device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &shader), "Create pixel shader");
    };
    pixelShader("CopyPS", m_copyPS);
    pixelShader("UpscalePS", m_upscalePS);
    pixelShader("ComparePS", m_comparePS);
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = sizeof(Constants);
    buffer.Usage = D3D11_USAGE_DEFAULT;
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Check(m_device->CreateBuffer(&buffer, nullptr, &m_constants), "Create shader settings buffer");
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    Check(m_device->CreateSamplerState(&sampler, &m_linear), "Create linear sampler");
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    Check(m_device->CreateSamplerState(&sampler, &m_point), "Create point sampler");
    Resize(width, height);
}

Renderer::Target Renderer::MakeTarget(UINT width, UINT height) {
    Target target;
    target.width = width;
    target.height = height;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    Check(m_device->CreateTexture2D(&desc, nullptr, &target.resource), "Create render texture");
    Check(m_device->CreateRenderTargetView(target.resource.Get(), nullptr, &target.rtv), "Create render target");
    Check(m_device->CreateShaderResourceView(target.resource.Get(), nullptr, &target.view), "Create texture view");
    return target;
}

void Renderer::Resize(UINT width, UINT height) {
    if (!width || !height || !m_device) return;
    m_ui->SetTarget(nullptr);
    m_uiTarget.Reset();
    m_context->ClearState();
    m_back = {};
    if (m_swapChain) {
        Check(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN,
            DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT), "Resize window buffers");
        m_back.width = width;
        m_back.height = height;
        Check(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&m_back.resource)), "Get window buffer");
        Check(m_device->CreateRenderTargetView(m_back.resource.Get(), nullptr, &m_back.rtv), "Create window render target");
    } else m_back = MakeTarget(width, height);
    Ptr<IDXGISurface> surface;
    Check(m_back.resource.As(&surface), "Get UI surface");
    const auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96, 96);
    Check(m_ui->CreateBitmapFromDxgiSurface(surface.Get(), &properties, &m_uiTarget), "Create UI target");
    m_ui->SetTarget(m_uiTarget.Get());
}

void Renderer::LoadImage(const std::filesystem::path& path) {
    // Only replace the current texture after the new image decodes successfully.
    auto image = LoadTextureFromFile(m_device.Get(), path);
    m_source = std::move(image);
}
void Renderer::SetImage(const ImagePixels& image) { m_source = CreateTexture(m_device.Get(), image); }

void Renderer::Draw(ID3D11RenderTargetView* target, const D2D1_RECT_F& viewport,
    ID3D11ShaderResourceView* source, ID3D11PixelShader* shader,
    const Constants& constants, ID3D11ShaderResourceView* reference) {
    ID3D11ShaderResourceView* empty[] = {nullptr, nullptr};
    m_context->PSSetShaderResources(0, 2, empty);
    m_context->OMSetRenderTargets(1, &target, nullptr);
    const D3D11_VIEWPORT vp{viewport.left, viewport.top, viewport.right - viewport.left,
        viewport.bottom - viewport.top, 0, 1};
    m_context->RSSetViewports(1, &vp);
    m_context->IASetInputLayout(nullptr);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vs.Get(), nullptr, 0);
    m_context->PSSetShader(shader, nullptr, 0);
    ID3D11SamplerState* samplers[] = {m_linear.Get(), m_point.Get()};
    m_context->PSSetSamplers(0, 2, samplers);
    ID3D11ShaderResourceView* sources[] = {source, reference};
    m_context->PSSetShaderResources(0, 2, sources);
    m_context->UpdateSubresource(m_constants.Get(), 0, nullptr, &constants, 0, 0);
    auto* buffer = m_constants.Get();
    m_context->PSSetConstantBuffers(0, 1, &buffer);
    m_context->Draw(3, 0);
    m_context->PSSetShaderResources(0, 2, empty);
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
}

void Renderer::Process(const UpscaleSettings& settings) {
    if (!m_source.view) throw std::runtime_error("Load an image first.");
    if (settings.width < 16 || settings.height < 16 || settings.width > 3840 || settings.height > 2160 ||
        !std::isfinite(settings.renderScale) || settings.renderScale < 0.25f || settings.renderScale > 1.0f ||
        !std::isfinite(settings.sharpness) || settings.sharpness < 0 || settings.sharpness > 1 || settings.algorithm > 2)
        throw std::runtime_error("Invalid upscale settings.");
    const UINT lowWidth = std::max(1u, static_cast<UINT>(std::lround(settings.width * settings.renderScale)));
    const UINT lowHeight = std::max(1u, static_cast<UINT>(std::lround(settings.height * settings.renderScale)));
    if (m_low.width != lowWidth || m_low.height != lowHeight) m_low = MakeTarget(lowWidth, lowHeight);
    if (m_output.width != settings.width || m_output.height != settings.height) {
        auto output = MakeTarget(settings.width, settings.height);
        auto reference = MakeTarget(settings.width, settings.height);
        m_output = std::move(output);
        m_reference = std::move(reference);
    }
    m_context->ClearRenderTargetView(m_low.rtv.Get(), kBackground);
    const float fit = std::min(static_cast<float>(lowWidth) / m_source.width,
        static_cast<float>(lowHeight) / m_source.height);
    const float w = m_source.width * fit, h = m_source.height * fit;
    const float x = (lowWidth - w) * 0.5f, y = (lowHeight - h) * 0.5f;
    Constants constants{};
    Draw(m_low.rtv.Get(), D2D1::RectF(x, y, x + w, y + h), m_source.view.Get(), m_copyPS.Get(), constants);
    constants.sourceSize[0] = static_cast<float>(lowWidth);
    constants.sourceSize[1] = static_cast<float>(lowHeight);
    Draw(m_reference.rtv.Get(), Full(settings.width, settings.height), m_low.view.Get(), m_upscalePS.Get(), constants);
    constants.algorithm = settings.algorithm;
    constants.sharpness = settings.sharpness;
    Draw(m_output.rtv.Get(), Full(settings.width, settings.height), m_low.view.Get(), m_upscalePS.Get(), constants);
}

void Renderer::BeginPreview(const D2D1_RECT_F& imageRect, float split, UINT comparison) {
    const float clear[] = {0.043f, 0.051f, 0.067f, 1};
    m_context->ClearRenderTargetView(m_back.rtv.Get(), clear);
    if (m_output.view) {
        Constants constants{};
        constants.split = std::clamp(split, 0.0f, 1.0f);
        constants.comparison = comparison;
        Draw(m_back.rtv.Get(), imageRect, m_output.view.Get(), m_comparePS.Get(), constants, m_reference.view.Get());
    }
}
void Renderer::Present() {
    if (m_swapChain) Check(m_swapChain->Present(1, 0), "Present frame (graphics device may have disconnected)");
}

ImagePixels Renderer::ReadTexture(ID3D11Texture2D* texture) {
    if (!texture) throw std::runtime_error("There is no rendered image to save.");
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Ptr<ID3D11Texture2D> staging;
    Check(m_device->CreateTexture2D(&desc, nullptr, &staging), "Create image readback buffer");
    m_context->CopyResource(staging.Get(), texture);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    Check(m_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Read processed pixels");
    ImagePixels pixels{desc.Width, desc.Height, {}};
    try {
        pixels.bgra.resize(static_cast<size_t>(desc.Width) * desc.Height * 4);
        for (UINT y = 0; y < desc.Height; ++y)
            std::memcpy(pixels.bgra.data() + static_cast<size_t>(y) * desc.Width * 4,
                static_cast<const BYTE*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch, desc.Width * 4);
    } catch (...) { m_context->Unmap(staging.Get(), 0); throw; }
    m_context->Unmap(staging.Get(), 0);
    return pixels;
}
ImagePixels Renderer::ReadOutput(bool reference) {
    return ReadTexture(reference ? m_reference.resource.Get() : m_output.resource.Get());
}
void Renderer::SaveOutput(const std::filesystem::path& path, bool reference) {
    const auto image = ReadOutput(reference);
    EncodePNG(path, image.width, image.height, image.bgra.data(), image.width * 4);
}
void Renderer::SavePreview(const std::filesystem::path& path) {
    const auto image = ReadTexture(m_back.resource.Get());
    EncodePNG(path, image.width, image.height, image.bgra.data(), image.width * 4);
}
}
