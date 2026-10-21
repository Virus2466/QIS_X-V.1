#pragma once
#include "WICTextureLoader.h"
#include <d3d11.h>
#include <wrl/client.h>
namespace qis {
struct Texture {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> resource;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    UINT width = 0, height = 0;
};
Texture CreateTexture(ID3D11Device* device, const ImagePixels& image);
Texture LoadTextureFromFile(ID3D11Device* device, const std::filesystem::path& path);
ImagePixels CreateTestPattern(UINT width = 1280, UINT height = 720);
}
