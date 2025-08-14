#include "Texture.h"
#include<WICTextureLoader.h>
#include<string>
#include<wincodec.h>
#pragma comment(lib, "DirectXTK.lib")



void ReleaseTexture(ID3D11ShaderResourceView*& textureSRV)
{
	if (textureSRV) {
		textureSRV->Release();
		textureSRV = nullptr;
	}
}

ID3D11ShaderResourceView* LoadTextureFromFile(ID3D11Device* device, const wchar_t* filename, bool forceReload)
{

	static std::wstring lastLoadedFile;
	static ID3D11ShaderResourceView* cachedSRV = nullptr;


	if (forceReload || filename != lastLoadedFile) {
		ReleaseTexture(cachedSRV);
		lastLoadedFile = filename;
	}


	if (!cachedSRV) {
		HRESULT hr = DirectX::CreateWICTextureFromFile(
			device,
			filename,
			nullptr,
			&cachedSRV
		);
		if (FAILED(hr)) {
			OutputDebugString(L"Failed to load Texture Error Code : ");
			OutputDebugString(std::to_wstring(hr).c_str());
		}
	}

	return cachedSRV;
}


