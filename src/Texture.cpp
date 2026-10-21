#include "../include/Texture.h"
#include <stdexcept>

namespace qis {
Texture CreateTexture(ID3D11Device* device, const ImagePixels& image) {
    if (!device) throw std::invalid_argument("A Direct3D device is required to create a texture.");
    if (!image.width || !image.height || image.width > 8192 || image.height > 8192 ||
        image.bgra.size() != static_cast<size_t>(image.width) * image.height * 4)
        throw std::invalid_argument("Invalid source image pixels.");

    Texture texture;
    texture.width = image.width;
    texture.height = image.height;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = image.width;
    desc.Height = image.height;
    desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const D3D11_SUBRESOURCE_DATA data{image.bgra.data(), image.width * 4, 0};
    Check(device->CreateTexture2D(&desc, &data, &texture.resource), "Upload source image");
    Check(device->CreateShaderResourceView(texture.resource.Get(), nullptr, &texture.view), "Create image view");
    return texture;
}

Texture LoadTextureFromFile(ID3D11Device* device, const std::filesystem::path& path) {
    return CreateTexture(device, DecodeImage(path));
}

ImagePixels CreateTestPattern(UINT width, UINT height) {
    if (!width || !height || width > 8192 || height > 8192)
        throw std::invalid_argument("Test pattern dimensions must be between 1 and 8192 pixels.");
    ImagePixels image{width, height, std::vector<std::uint8_t>(static_cast<size_t>(width) * height * 4)};
    for (UINT y = 0; y < height; ++y) for (UINT x = 0; x < width; ++x) {
        auto* p = image.bgra.data() + (static_cast<size_t>(y) * width + x) * 4;
        const bool line = (x % 48 < 2) || (y % 48 < 2) || (x == y) || (x + y == width - 1);
        p[0] = line ? 210 : static_cast<BYTE>(30 + 65 * x / width);
        p[1] = line ? 238 : static_cast<BYTE>(24 + 85 * y / height);
        p[2] = line ? 142 : 18;
        p[3] = 255;
    }
    return image;
}
}
