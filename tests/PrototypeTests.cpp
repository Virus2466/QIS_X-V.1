#include "Application.h"
#include "FrameSync.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
size_t Differences(const qis::ImagePixels& a, const qis::ImagePixels& b) {
    Require(a.width == b.width && a.height == b.height, "Image dimensions differ unexpectedly");
    size_t differences = 0;
    for (size_t i = 0; i < a.bgra.size(); ++i) differences += a.bgra[i] != b.bgra[i];
    return differences;
}
}

int main(int argc, char** argv) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com)) return 1;
    int result = 0;
    try {
        const std::filesystem::path artifacts = argc > 1 ? argv[1] : "test-artifacts";
        std::filesystem::create_directories(artifacts);
        qis::Renderer renderer;
        renderer.Initialize(nullptr, 960, 540, true);
        Require(renderer.Software(), "Tests must use the deterministic WARP backend");
        auto pattern = qis::CreateTestPattern(320, 180);
        renderer.SetImage(pattern);
        {
            bool rejected = false;
            try { qis::CreateTexture(nullptr, pattern); } catch (const std::invalid_argument&) { rejected = true; }
            Require(rejected, "Texture upload must reject a missing device");
            auto incomplete = pattern;
            incomplete.bgra.pop_back();
            rejected = false;
            try { renderer.SetImage(incomplete); } catch (const std::invalid_argument&) { rejected = true; }
            Require(rejected && renderer.SourceWidth() == pattern.width,
                "Incomplete pixel data must be rejected without replacing the source");
            rejected = false;
            try { qis::CreateTestPattern(0, 180); } catch (const std::invalid_argument&) { rejected = true; }
            Require(rejected, "Test pattern must reject zero dimensions");
            std::cout << "PASS: texture API, missing-device and malformed-pixel validation\n";
        }
        qis::UpscaleSettings settings{640, 360, 0.5f, 0.55f, 0};
        renderer.Process(settings);
        const auto baseline = renderer.ReadOutput();
        Require(Differences(baseline, renderer.ReadOutput(true)) == 0, "Bilinear mode must equal its baseline");
        settings.algorithm = 1;
        renderer.Process(settings);
        const auto cubic = renderer.ReadOutput();
        Require(Differences(baseline, cubic) > 1000, "Bicubic shader did not change the reconstruction");
        settings.algorithm = 2;
        settings.sharpness = 0;
        renderer.Process(settings);
        Require(Differences(cubic, renderer.ReadOutput()) == 0, "Zero sharpness must preserve bicubic reconstruction");
        settings.sharpness = 0.8f;
        renderer.Process(settings);
        Require(Differences(cubic, renderer.ReadOutput()) > 1000, "Sharpness control did not change the image");
        std::cout << "PASS: compiled shaders, shared baseline, bicubic reconstruction, adaptive sharpening\n";

        auto flat = pattern;
        for (size_t i = 0; i < flat.bgra.size(); i += 4) {
            flat.bgra[i] = 47; flat.bgra[i + 1] = 93; flat.bgra[i + 2] = 137; flat.bgra[i + 3] = 255;
        }
        renderer.SetImage(flat);
        renderer.Process(settings);
        auto uniform = renderer.ReadOutput();
        for (size_t i = 0; i < uniform.bgra.size(); i += 4) {
            Require(std::abs(int(uniform.bgra[i]) - 47) <= 1 && std::abs(int(uniform.bgra[i + 1]) - 93) <= 1 &&
                std::abs(int(uniform.bgra[i + 2]) - 137) <= 1 && uniform.bgra[i + 3] == 255,
                "Flat colors or borders changed (possible shader edge artifacts)");
        }
        const auto roundtrip = artifacts / L"roundtrip-\u03a9.png";
        renderer.SaveOutput(roundtrip);
        Require(Differences(uniform, qis::DecodeImage(roundtrip)) == 0, "Unicode PNG export roundtrip changed pixels");
        bool rejected = false;
        try { renderer.LoadImage(artifacts / "missing-file.png"); } catch (const std::exception&) { rejected = true; }
        Require(rejected && renderer.SourceWidth() == flat.width, "Failed import must preserve the current source");
        auto invalid = settings; invalid.renderScale = 0;
        rejected = false;
        try { renderer.Process(invalid); } catch (const std::exception&) { rejected = true; }
        Require(rejected, "Invalid settings were accepted");
        std::cout << "PASS: stable border colors, lossless PNG export, Unicode paths, failed-import recovery\n";

        // Four distinct quadrants catch channel swapping, UV flips and aspect distortion.
        auto quadrants = qis::CreateTestPattern(100, 100);
        for (UINT y = 0; y < 100; ++y) for (UINT x = 0; x < 100; ++x) {
            auto* p = quadrants.bgra.data() + (y * 100 + x) * 4;
            p[0] = x >= 50 ? 220 : 20;
            p[1] = y >= 50 ? 200 : 30;
            p[2] = 60;
        }
        renderer.SetImage(quadrants);
        renderer.Process(settings);
        const auto fitted = renderer.ReadOutput();
        auto pixel = [&](UINT x, UINT y, UINT channel) { return fitted.bgra[(y * fitted.width + x) * 4 + channel]; };
        Require(pixel(180, 40, 0) < 30 && pixel(460, 40, 0) > 210 && pixel(180, 320, 1) > 190,
            "Image orientation or channel order is incorrect");
        Require(pixel(10, 180, 0) < 15 && pixel(630, 180, 0) < 15, "Non-widescreen input must be letterboxed");
        std::cout << "PASS: image orientation, BGRA channels, preserved aspect ratio\n";

        renderer.LoadImage(qis::ExecutableDirectory() / "assets" / "demo.png");
        settings = {};
        renderer.Process(settings);
        renderer.SaveOutput(artifacts / "enhanced-1080p.png");
        const auto exported = qis::DecodeImage(artifacts / "enhanced-1080p.png");
        Require(exported.width == 1920 && exported.height == 1080, "Export ignored the selected output resolution");
        settings.width = 2560; settings.height = 1440; settings.renderScale = 0.77f;
        renderer.Process(settings);
        Require(renderer.InputWidth() == 1971 && renderer.InputHeight() == 1109, "Render-scale dimensions are incorrect");
        std::cout << "PASS: bundled demo, full-resolution 1080p export, 1440p target recreation\n";

        qis::Application app;
        app.Initialize(nullptr, 1440, 900, true);
        app.Render();
        app.SavePreview(artifacts / "workbench.png");
        app.MouseDown(45, 645); app.MouseMove(210, 645); app.MouseUp();
        app.KeyDown('P', false); app.KeyDown('O', false);
        app.Render();
        app.MouseDown(600, 400); app.MouseMove(800, 400); app.MouseUp();
        app.KeyDown('B', false); app.KeyDown('1', false); app.Render();
        app.KeyDown('2', false); app.Render();
        app.KeyDown('R', false);
        app.Resize(2160, 1350, 1.5f); app.Render();
        app.SavePreview(artifacts / "workbench-150dpi.png");
        app.Resize(1100, 740); app.Render();
        app.SavePreview(artifacts / "workbench-small.png");
        app.Resize(1280, 800, 1.5f); app.Render();
        Require(app.DisplayScale() < 1.5f, "Scaled laptop layout must fit the available work area");
        app.SavePreview(artifacts / "workbench-laptop.png");
        std::cout << "PASS: workbench render, controls, comparison drag, resize and 150% DPI\n";

        FrameSync sync;
        sync.BeginFrame();
        Sleep(20);
        LARGE_INTEGER start{}, finish{}, frequency{};
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start);
        sync.EndFrame(100);
        QueryPerformanceCounter(&finish);
        const double additional = static_cast<double>(finish.QuadPart - start.QuadPart) / frequency.QuadPart;
        Require(additional < 0.01, "Frame cap slept again after this frame had already exceeded its budget");
        std::cout << "PASS: frame cap includes current frame work; null swap chain is safe\n";
        std::cout << "All prototype checks passed. Artifacts: " << artifacts.string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; result = 1;
    }
    CoUninitialize();
    return result;
}
