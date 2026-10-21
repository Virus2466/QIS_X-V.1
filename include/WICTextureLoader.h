#pragma once
#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <vector>
namespace qis {
struct ImagePixels {
    UINT width = 0, height = 0;
    std::vector<std::uint8_t> bgra;
};
// COM must be initialized on the calling thread.
ImagePixels DecodeImage(const std::filesystem::path& path);
void EncodePNG(const std::filesystem::path& path, UINT width, UINT height,
    const std::uint8_t* pixels, UINT rowPitch);
void Check(HRESULT result, const char* operation);
std::filesystem::path ExecutableDirectory();
}
