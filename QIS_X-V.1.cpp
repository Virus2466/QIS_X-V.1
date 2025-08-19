/*
    Project: QIS-X Upscaler Demo [ Phase - 1 ]
    Author : Dev Patel
    Date   : August 12, 2025
    Version: 1.0

    Description:
    ------------
    This application demonstrates a basic real-time upscaling pipeline using
    Direct3D 11. The rendering flow is as follows:
        1. Render the scene to a low-resolution render target (480p).
        2. Use a full-screen textured quad to upscale the low-res image to
           the back buffer (720p) via a pixel shader.
        3. Present the upscaled frame to the screen with frame synchronization.

    Notes:
    ------
    This is a minimal demonstration intended as a foundation for more advanced
    spatial/temporal upscaling techniques (e.g., FSR, DLSS)

*/



#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include<dxgi1_4.h>

#include<format>
#include<iostream>
#include <cstdio>

#include "QIS_X-V.1.h"
#include "FrameSync.h"
#include "Texture.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib,"windowscodecs.lib")

// Global handles
HWND g_hWnd = nullptr;
ID3D11Device* g_pDevice = nullptr;
ID3D11DeviceContext* g_pContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
FrameSync* g_frameSync = nullptr;
ID3D11ShaderResourceView* g_pDemoTexture = nullptr;
UINT g_textureWidth = 0;
UINT g_textureHeight = 0;
bool g_SplitScreen = true;


// Upscaling Resources
ID3D11Texture2D* g_pLowResRT = nullptr;          // 480p render target
ID3D11RenderTargetView* g_pLowResRTV = nullptr;  // RTV for low-res
ID3D11ShaderResourceView* g_pLowResSRV = nullptr; // SRV for upscaling

// Full-screen Quad Resources
ID3D11Buffer* g_pQuadVB = nullptr;               // Vertex buffer
ID3D11VertexShader* g_pVS = nullptr;             // Vertex shader
ID3D11PixelShader* g_pPS = nullptr;              // Pixel shader
ID3D11InputLayout* g_pInputLayout = nullptr;     // Input layout
ID3D11SamplerState* g_pSamplerState = nullptr;   // Sampler state

// Vertex structure for full-screen quad
struct Vertex {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 uv;
};


struct CB {
    float screenSize;
};


// Helper Function for error Checking
void CheckHR(HRESULT hr, const char* message) {
    if (FAILED(hr)) {
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
    }
}




// Forward declarations
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitD3D11();
bool CreateLowResResources();
bool CreateFullscreenQuad();
void RenderSceneToLowResRT();  
void UpscaleToBackbuffer();    
void Cleanup();

//-----------------------------------------------------------------------------
// Entry point
//-----------------------------------------------------------------------------
#ifdef _MSC_VER
#pragma comment(linker, "/ENTRY:wWinMain")
#endif

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
    if (!InitWindow(hInstance, nCmdShow)) return 1;
    if (!InitD3D11()) return 1;
    if (!CreateLowResResources()) return 1;
    if (!CreateFullscreenQuad()) return 1;
   



    // Load resources After D3D is ready
    ReleaseTexture(g_pDemoTexture);
    g_pDemoTexture = LoadTextureFromFile(g_pDevice, L"grid.jpeg" , true);
    if (!g_pDemoTexture) {
        MessageBox(nullptr, L"Failed to Load Texture!", L"Error", MB_OK);
    }



    if (g_pDemoTexture) {

        // Get Texture Dimensions
        ID3D11Resource* textureResource;
        g_pDemoTexture->GetResource(&textureResource);

        ID3D11Texture2D* texture2D;
        textureResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texture2D);

        D3D11_TEXTURE2D_DESC desc;
        texture2D->GetDesc(&desc);
        g_textureWidth = desc.Width;
        g_textureHeight = desc.Height;

        texture2D->Release();
        textureResource->Release();
    

        // Log Dimensions
        OutputDebugString(std::format(L"Texture size :  {}x{} \n", g_textureWidth, g_textureHeight).c_str());
    }




    FrameSync frameSync(g_pSwapChain);
    float fpsUpdateTimer = 0.0f;

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            
            frameSync.BeginFrame();

            // Clear both render targets
            float clearColor[4] = { 0.0f , 0.0f , 0.0f , 1.0f };
            g_pContext->ClearRenderTargetView(g_pLowResRTV, clearColor);

  
            // 1. Render scene to low-res render target (480p)
            RenderSceneToLowResRT();
            // 2. Upscale to backbuffer (720p)
            UpscaleToBackbuffer();



            static float fpsUpdateTimer = 0.0f;
            fpsUpdateTimer += frameSync.GetDeltaTime();
            if (fpsUpdateTimer >= 0.25f) {
                wchar_t title[256];
                swprintf_s(title, L"QIS-X Upscaler - %.1f FPS (Frame Time: %.2fms)",
                    frameSync.GetFPS(),
                    frameSync.GetDeltaTime() * 1000.0f);
                SetWindowText(g_hWnd, title);
                fpsUpdateTimer = 0.0f;
            }

            // 3. Present
            g_pSwapChain->Present(1, 0);
            frameSync.EndFrame(60);


        }
    }

    Cleanup();
    MessageBoxA(nullptr, "Press OK to exit...", "Debug Pause", MB_OK);
    return 0;

}

//-----------------------------------------------------------------------------
// Window setup 
//-----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

bool InitWindow(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0L, 0L,
        hInstance, nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr,
        nullptr, L"QIS+", nullptr
    };
    RegisterClassEx(&wc);

    RECT rc = { 0, 0, 1280, 720 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindow(wc.lpszClassName, L"QIS-X Upscaler Demo",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hWnd) return false;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    return true;
}

//-----------------------------------------------------------------------------
// Direct3D 11 initialization 
//-----------------------------------------------------------------------------
bool InitD3D11() {
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 3;
    scd.BufferDesc.Width = 1280;
    scd.BufferDesc.Height = 720;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = g_hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        featureLevels, 1, D3D11_SDK_VERSION,
        &scd, &g_pSwapChain, &g_pDevice,
        nullptr, &g_pContext);


    return SUCCEEDED(hr);

    if (g_pSwapChain) {
        g_frameSync = new FrameSync(g_pSwapChain);
    }
    else {
        OutputDebugString(L"Failed to create Swap Chain - frame sync unavailable\n");
    }


    // Set Maximum frame latency
    IDXGISwapChain2* pSwapChain2 = nullptr;
    if (SUCCEEDED(pSwapChain2->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&pSwapChain2))); {
        pSwapChain2->SetMaximumFrameLatency(1);
        pSwapChain2->Release();
    }


    return true;
}

bool CreateLowResResources() {
    // Create low-res texture (854x480)
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = 854;
    texDesc.Height = 480;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = g_pDevice->CreateTexture2D(&texDesc, nullptr, &g_pLowResRT);
    CheckHR(hr, "Failed to create low-res texture");

    hr = g_pDevice->CreateRenderTargetView(g_pLowResRT, nullptr, &g_pLowResRTV);
    CheckHR(hr, "Failed to create RTV");

    hr = g_pDevice->CreateShaderResourceView(g_pLowResRT, nullptr, &g_pLowResSRV);
    CheckHR(hr, "Failed to create SRV");

    return SUCCEEDED(hr);
    if (g_pLowResRT == nullptr) {  
        OutputDebugStringA("Error: g_pLowResRT is null before calling CreateShaderResourceView\n");  
        return false;  
    }  

    hr = g_pDevice->CreateShaderResourceView(g_pLowResRT, nullptr, &g_pLowResSRV);  
    CheckHR(hr, "Failed to create SRV");
}

bool CreateFullscreenQuad() {

    Vertex vertices[] = {
        { {-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f} }, // Top-left
        { { 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f} }, // Top-right
        { {-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f} }, // Bottom-left
        { { 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} }  // Bottom-right
    };

    // Create vertex buffer
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = { vertices };
    HRESULT hr = g_pDevice->CreateBuffer(&bd, &initData, &g_pQuadVB);
    CheckHR(hr, "Failed to create vertex buffer");

     OutputDebugString(std::format(L"UV Top-Left: ({}, {})\n", vertices[0].uv.x, vertices[0].uv.y).c_str());
    OutputDebugString(std::format(L"UV Bottom-Right: ({}, {})\n", vertices[3].uv.x, vertices[3].uv.y).c_str());

    // Compile shaders with error reporting
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr,
        "VS_main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    hr = g_pDevice->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, &g_pVS);
    CheckHR(hr, "Failed to create vertex shader");

    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = g_pDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), &g_pInputLayout);
    CheckHR(hr, "Failed to create input layout");
    vsBlob->Release();

    // Compile pixel shader
    ID3DBlob* psBlob = nullptr;
    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr,
        "PS_main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    hr = g_pDevice->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, &g_pPS);
    CheckHR(hr, "Failed to create pixel shader");
    psBlob->Release();

    // Create sampler state
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

    hr = g_pDevice->CreateSamplerState(&sampDesc, &g_pSamplerState);
    CheckHR(hr, "Failed to create sampler state");

    return true;
}


void RenderSceneToLowResRT() {

    // Set viewport to match low-res render target (854x480)
    D3D11_VIEWPORT vp = { 0.0f, 0.0f, 854.0f, 480.0f, 0.0f, 1.0f };
    g_pContext->RSSetViewports(1, &vp);

    // Clear to a visible color (green)
    float clearColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    g_pContext->ClearRenderTargetView(g_pLowResRTV, clearColor);
    g_pContext->OMSetRenderTargets(1, &g_pLowResRTV, nullptr);


    // Set input layout and primitive topology
    g_pContext->IASetInputLayout(g_pInputLayout);
    g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Render Scene Here in Fututre Main logic here .....
    if (g_pDemoTexture) {
        g_pContext->PSSetShaderResources(0, 1, &g_pDemoTexture);
        g_pContext->VSSetShader(g_pVS, nullptr, 0);
        g_pContext->PSSetShader(g_pPS, nullptr, 0);

        // Draw quad
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        g_pContext->IASetVertexBuffers(0, 1, &g_pQuadVB, &stride, &offset);
        g_pContext->Draw(4, 0);
    }

}

void UpscaleToBackbuffer() {
    // Get back buffer
    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to get back buffer\n");
        return;
    }

    // Create render target view
    ID3D11RenderTargetView* pBackbufferRTV = nullptr;
    hr = g_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pBackbufferRTV);
    pBackBuffer->Release();

    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create RTV for back buffer\n");
        return;
    }

    // Clear to magenta so we know if rendering fails
    float clearColor[4] = { 1.0f, 0.0f, 1.0f, 1.0f };
    g_pContext->ClearRenderTargetView(pBackbufferRTV, clearColor);
    g_pContext->OMSetRenderTargets(1, &pBackbufferRTV, nullptr);

    // Set viewport
    D3D11_VIEWPORT vp = { 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
    g_pContext->RSSetViewports(1, &vp);

    // Set shaders and resources
    g_pContext->VSSetShader(g_pVS, nullptr, 0);
    g_pContext->PSSetShader(g_pPS, nullptr, 0);
    g_pContext->PSSetShaderResources(0, 1, &g_pLowResSRV);
    g_pContext->PSSetSamplers(0, 1, &g_pSamplerState);
    g_pContext->IASetInputLayout(g_pInputLayout);

    // Set vertex buffer
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    g_pContext->IASetVertexBuffers(0, 1, &g_pQuadVB, &stride, &offset);
    g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Draw the quad
    g_pContext->Draw(4, 0);

    // Present
    g_pSwapChain->Present(1, 0);
    pBackbufferRTV->Release();
}

//-----------------------------------------------------------------------------
// Cleanup 
//-----------------------------------------------------------------------------
void Cleanup() {
    if (g_pDemoTexture) {
        g_pDemoTexture->Release();
        g_pDemoTexture = nullptr; // Prevent dangling pointers
    }
    if (g_pSamplerState) g_pSamplerState->Release();
    if (g_pInputLayout) g_pInputLayout->Release();
    if (g_pPS) g_pPS->Release();
    if (g_pVS) g_pVS->Release();
    if (g_pQuadVB) g_pQuadVB->Release();
    if (g_pLowResSRV) g_pLowResSRV->Release();
    if (g_pLowResRTV) g_pLowResRTV->Release();
    if (g_pLowResRT) g_pLowResRT->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pContext) g_pContext->Release();
    if (g_pDevice) g_pDevice->Release();
   
}