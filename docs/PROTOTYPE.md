# Prototype notes

QIS-X 0.2 is an image-based spatial upscaling workbench for Windows 10/11. The goal is a dependable, demonstrable foundation for the longer-term game upscaler.

## Pipeline

1. Decode the first frame of an image using Windows Imaging Component and upload BGRA pixels to Direct3D 11.
2. Fit the image into a low-resolution render target, preserving its aspect ratio. Transparent pixels composite over the canvas background.
3. Reconstruct two output textures from that same render target: a bilinear baseline and the selected reconstruction.
4. Display them using a movable comparison divider, then draw the interface with Direct2D and DirectWrite.
5. Present once, synchronized to the display. Read back the enhanced output only when exporting.

The intermediate size equals the selected output resolution multiplied by render scale, rounded to whole pixels. For example, 1080p at 67% uses a 1280 x 720 intermediate and produces a 1920 x 1080 PNG. Other aspect ratios are letterboxed into the selected 16:9 output.

Static image results are cached until a setting or source changes. Moving the divider does not recompute the reconstruction. UI FPS measures presentation only; it cannot demonstrate game FPS gains or the cost of a continuously changing scene.

## Reconstruction

- **Bilinear:** four neighboring texels interpolated by the linear sampler.
- **Bicubic:** 16-tap Catmull-Rom reconstruction using point samples at texel centers.
- **QIS-X:** bicubic plus a contrast-adjusted unsharp mask. A local color range constrains overshoot; sharpness is adjustable from 0 to 100%.

This is conventional spatial filtering. It has no trained model, motion vectors, temporal history, FSR, or DLSS integration. Sharpening can emphasize noise and cannot recover information absent from the input.

## Code map

| File | Responsibility |
| --- | --- |
| `QIS_X-V.1.cpp` | Win32 lifecycle, input dispatch, DPI, drag/drop, message pump |
| `src/Application.cpp` | Workbench controls, comparison, dialogs, interface |
| `src/Renderer.cpp` | D3D11 device, textures, shader passes, resize, readback |
| `src/FrameSync.cpp` | Frame measurement and optional elapsed-work-aware cap |
| `src/Texture.cpp` | Texture upload and fallback resolution chart |
| `src/WICTextureLoader.cpp` | Image decoding, PNG encoding, error reporting |
| `shaders/shaders.hlsl` | Reconstruction and comparison shaders; embedded at build time |
| `tests/PrototypeTests.cpp` | Offscreen WARP integration checks and preview artifacts |

COM resources use scoped ownership. New imports replace the current image only after successful decoding. Shader/resource failures stop the operation with an error instead of continuing with null pointers. Export writes a temporary PNG beside the destination and replaces it only after encoding succeeds.

## Validation

`ctest --preset release` runs real compiled shaders on Windows' WARP software renderer, without opening a window. It checks the common baseline, actual algorithm differences, zero-sharpness behavior, stable colors at image borders, PNG roundtrips including Unicode paths, invalid-image recovery, orientation, letterboxing, output sizes, controls, comparison, resizing, DPI, and frame-cap timing.

Generated artifacts live in `out/build/windows/test-artifacts/`, including `workbench.png`, `workbench-small.png`, `workbench-150dpi.png`, and `enhanced-1080p.png`.

The prototype has been built and checked locally with MSVC 19.44 and Windows SDK 10.0.26100. The WARP integration suite and the packaged executable's native-window hardware smoke test both passed. The executable links the C++ runtime statically and depends only on Windows system DLLs.

For native-window smoke testing, run `QIS-X.exe --smoke-test <artifact-directory>`. It creates a hidden Win32 window, uses the hardware adapter when available, renders and presents frames, resizes the swap chain, changes controls, writes `native-workbench.png`, and exits with status 0 on success. A nonzero exit indicates failure. This check is separate from the deterministic WARP test.

## Boundaries and next milestones

- Inputs: still PNG, JPEG, BMP, TIFF, and other formats supported by installed WIC codecs. First frame only; no animation, EXIF rotation, or color-profile/HDR processing yet.
- Limit: 8192 pixels per side and 64 megapixels. Outputs: 720p, 1080p, or 1440p. Hardware requires D3D feature level 11.0; WARP is the slower fallback.
- The interface has keyboard shortcuts but does not yet expose custom controls through Windows UI Automation.
- Graphics-device loss reports an error and exits; automatic device recreation is future work.
- Live capture, game integration, temporal reconstruction, GPU timing, and game-performance claims remain outside this prototype.

Suggested development order: add a Windows Graphics Capture source; introduce an asynchronous live-frame interface and GPU timestamp queries; benchmark end-to-end capture/reconstruction latency; then evaluate a temporal method with motion and history inputs.
