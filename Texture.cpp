#include "Texture.h"
#include "WICTextureLoader.h"
#pragma comment(lib, "DirectXTK.lib")

ID3D11ShaderResourceView* LoadTextureFromFile(ID3D11Device* device, const wchar_t* filename)
{
	ID3D11ShaderResourceView* texture = nullptr;
	DirectX::CreateWICTextureFromFile(device, filename, nullptr, &texture);
	return texture;
}

void ReleaseTexture(ID3D11ShaderResourceView* texture)
{
	if (texture) {
		texture->Release();
	}
}

