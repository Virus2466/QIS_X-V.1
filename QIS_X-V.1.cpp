#include "QIS_X-V.1.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// Global handles
HWND g_hWnd = nullptr;
ID3D11Device* g_pDevice = nullptr;
ID3D11DeviceContext* g_pContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11Texture2D* g_pLowResRT = nullptr;
ID3D11RenderTargetView* g_pLowResRTV = nullptr;
ID3D11ShaderResourceView* g_pLowResSRV = nullptr;
ID3D11Buffer* g_pQuadVB = nullptr;
ID3D11VertexShader* g_pVS = nullptr;
ID3D11PixelShader* g_pPS = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11SamplerState* g_pSamplerState = nullptr;

// Vertex structure for full-screen quad
struct Vertex {
	float x, y, z;
	float u, v;
};

// Forward declarations
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitD3D11();
bool CreateLowResResources();
bool CreateFullscreenQuad();
void RenderFrame();
void Cleanup();

//-----------------------------------------------------------------------------
// Entry point
//-----------------------------------------------------------------------------
// Force linker to use wWinMain instead of main
#ifdef _MSC_VER
#pragma comment(linker, "/ENTRY:wWinMain")
#endif

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
	if (!InitWindow(hInstance, nCmdShow)) return 1;
	if (!InitD3D11()) return 1;
	if (!CreateLowResResources()) return 1;
	if (!CreateFullscreenQuad()) return 1;

	MSG msg = {};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			RenderFrame();
		}
	}

	Cleanup();
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
	g_hWnd = CreateWindow(wc.lpszClassName, L"QIS+ Upscaler Demo",
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
	scd.BufferCount = 2;
	scd.BufferDesc.Width = 1280;
	scd.BufferDesc.Height = 720;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferDesc.RefreshRate = { 60, 1 };
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.OutputWindow = g_hWnd;
	scd.SampleDesc.Count = 1;
	scd.Windowed = TRUE;
	scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		featureLevels, 1, D3D11_SDK_VERSION,
		&scd, &g_pSwapChain, &g_pDevice,
		nullptr, &g_pContext);

	return SUCCEEDED(hr);
}

//-----------------------------------------------------------------------------
// Low-resolution render target setup
//-----------------------------------------------------------------------------
bool CreateLowResResources() {
	// Low-res texture (480p)
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = 854;
	texDesc.Height = 480;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(g_pDevice->CreateTexture2D(&texDesc, nullptr, &g_pLowResRT)))
		return false;

	// Create RTV and SRV
	g_pDevice->CreateRenderTargetView(g_pLowResRT, nullptr, &g_pLowResRTV);
	g_pDevice->CreateShaderResourceView(g_pLowResRT, nullptr, &g_pLowResSRV);

	return true;
}

//-----------------------------------------------------------------------------
// Full-screen quad for upscaling
//-----------------------------------------------------------------------------
bool CreateFullscreenQuad() {
	// Vertex buffer
	Vertex vertices[] = {
		{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
		{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
		{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f }
	};

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(vertices);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA initData = { vertices };
	if (FAILED(g_pDevice->CreateBuffer(&bd, &initData, &g_pQuadVB)))
		return false;

	// Compile shaders
	ID3DBlob* vsBlob = nullptr;
	if (FAILED(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VS_main", "vs_5_0", 0, 0, &vsBlob, nullptr)))
		return false;

	g_pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVS);

	// Input layout
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	g_pDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);
	vsBlob->Release();

	// Pixel shader
	ID3DBlob* psBlob = nullptr;
	if (FAILED(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PS_main", "ps_5_0", 0, 0, &psBlob, nullptr)))
		return false;

	g_pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPS);
	psBlob->Release();

	// Sampler state
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	if (FAILED(g_pDevice->CreateSamplerState(&sampDesc, &g_pSamplerState)))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Main rendering loop
//-----------------------------------------------------------------------------
void RenderFrame() {
	// 1. Render to low-res RT
	float clearColor[4] = { 0.2f, 0.4f, 0.8f, 1.0f };
	g_pContext->ClearRenderTargetView(g_pLowResRTV, clearColor);
	g_pContext->OMSetRenderTargets(1, &g_pLowResRTV, nullptr);

	// [Your game rendering would go here]
	// For demo: Just clear with a color

	// 2. Upscale to backbuffer
	ID3D11RenderTargetView* pBackbufferRTV = nullptr;
	ID3D11Texture2D* pBackBuffer = nullptr;
	g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	g_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pBackbufferRTV);
	pBackBuffer->Release();

	g_pContext->ClearRenderTargetView(pBackbufferRTV, clearColor);
	g_pContext->OMSetRenderTargets(1, &pBackbufferRTV, nullptr);

	// Set upscaling shaders
	g_pContext->VSSetShader(g_pVS, nullptr, 0);
	g_pContext->PSSetShader(g_pPS, nullptr, 0);
	g_pContext->PSSetShaderResources(0, 1, &g_pLowResSRV);
	g_pContext->PSSetSamplers(0, 1, &g_pSamplerState);
	g_pContext->IASetInputLayout(g_pInputLayout);

	// Draw quad
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pQuadVB, &stride, &offset);
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	g_pContext->Draw(4, 0);

	// Present
	g_pSwapChain->Present(1, 0);
	pBackbufferRTV->Release();
}

//-----------------------------------------------------------------------------
// Cleanup
//-----------------------------------------------------------------------------
void Cleanup() {
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