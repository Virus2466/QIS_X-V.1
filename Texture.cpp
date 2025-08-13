#include "Texture.h"
#include<WICTextureLoader.h>
#include<string>
#pragma comment(lib, "DirectXTK.lib")

ID3D11ShaderResourceView* LoadTextureFromFile(ID3D11Device* device, const wchar_t* filename)
{
	ID3D11ShaderResourceView* texture = nullptr;
	HRESULT hr  = DirectX::CreateWICTextureFromFile(device, filename, nullptr, &texture);
	if (FAILED(hr)) {
		OutputDebugString(L"Failed to load Texture Error Code : ");
		OutputDebugString(std::to_wstring(hr).c_str());
	}

	
	return texture;
}

void ReleaseTexture(ID3D11ShaderResourceView* texture)
{
	if (texture) {
		texture->Release();
	}
}

