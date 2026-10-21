#include "WICTextureLoader.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <sstream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;

namespace qis {
void Check(HRESULT result, const char* operation) {
    if (SUCCEEDED(result)) return;
    std::ostringstream message;
    message << operation << " (0x" << std::hex << std::uppercase << static_cast<unsigned long>(result) << ")";
    throw std::runtime_error(message.str());
}
std::filesystem::path ExecutableDirectory() {
    std::vector<wchar_t> path(32768);
    const DWORD count = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!count || count >= path.size()) throw std::runtime_error("Cannot resolve the application directory.");
    return std::filesystem::path(std::wstring(path.data(), count)).parent_path();
}
ImagePixels DecodeImage(const std::filesystem::path& path) {
    ComPtr<IWICImagingFactory> factory;
    Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)), "Create image decoder");
    ComPtr<IWICBitmapDecoder> decoder;
    Check(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder), "Open image (PNG, JPEG, BMP or TIFF)");
    ComPtr<IWICBitmapFrameDecode> frame;
    Check(decoder->GetFrame(0, &frame), "Read image frame");
    ImagePixels image;
    Check(frame->GetSize(&image.width, &image.height), "Read image dimensions");
    if (!image.width || !image.height || image.width > 8192 || image.height > 8192 ||
        static_cast<std::uint64_t>(image.width) * image.height > 64ull * 1024 * 1024)
        throw std::runtime_error("Image limit: 8192 pixels per side, 64 megapixels.");
    ComPtr<IWICFormatConverter> converter;
    Check(factory->CreateFormatConverter(&converter), "Create pixel converter");
    Check(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom), "Convert image to BGRA");
    image.bgra.resize(static_cast<size_t>(image.width) * image.height * 4);
    Check(converter->CopyPixels(nullptr, image.width * 4, static_cast<UINT>(image.bgra.size()),
        image.bgra.data()), "Decode image pixels");
    return image;
}
void EncodePNG(const std::filesystem::path& path, UINT width, UINT height,
    const std::uint8_t* pixels, UINT rowPitch) {
    ComPtr<IWICImagingFactory> factory;
    Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)), "Create PNG encoder");
    ComPtr<IWICStream> stream;
    Check(factory->CreateStream(&stream), "Create PNG stream");
    Check(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE), "Open PNG destination");
    ComPtr<IWICBitmapEncoder> encoder;
    Check(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder), "Create PNG encoder");
    Check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache), "Initialize PNG encoder");
    ComPtr<IWICBitmapFrameEncode> frame;
    Check(encoder->CreateNewFrame(&frame, nullptr), "Create PNG frame");
    Check(frame->Initialize(nullptr), "Initialize PNG frame");
    Check(frame->SetSize(width, height), "Set PNG dimensions");
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    Check(frame->SetPixelFormat(&format), "Set PNG pixel format");
    if (format != GUID_WICPixelFormat32bppBGRA) throw std::runtime_error("PNG encoder rejected BGRA pixels.");
    Check(frame->WritePixels(height, rowPitch, rowPitch * height, const_cast<BYTE*>(pixels)), "Write PNG pixels");
    Check(frame->Commit(), "Finish PNG frame");
    Check(encoder->Commit(), "Finish PNG file");
}
}
