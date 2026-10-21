#include "Application.h"
#include "FrameSync.h"
#include <Windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <algorithm>
#include <memory>
#include <system_error>

namespace {
struct WindowState {
    qis::Application app;
    bool minimized = false;
    bool active = true;
    bool smokeTest = false;
    float dpi = 1;
    std::unique_ptr<FrameSync> timing;
};

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<WindowState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = static_cast<WindowState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(window, message, wParam, lParam);
    try {
        switch (message) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: { PAINTSTRUCT paint{}; BeginPaint(window, &paint); EndPaint(window, &paint); return 0; }
        case WM_SIZE:
            state->minimized = wParam == SIZE_MINIMIZED;
            if (state->app.Ready() && !state->minimized)
                state->app.Resize(LOWORD(lParam), HIWORD(lParam), state->dpi);
            if (state->timing) state->timing->Reset();
            return 0;
        case WM_DPICHANGED: {
            state->dpi = HIWORD(wParam) / 96.0f;
            const auto* rect = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left,
                rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = static_cast<LONG>(1080 * state->dpi);
            info->ptMinTrackSize.y = static_cast<LONG>(780 * state->dpi);
            MONITORINFO monitor{sizeof(monitor)};
            if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
                info->ptMinTrackSize.x = std::min(info->ptMinTrackSize.x, monitor.rcWork.right - monitor.rcWork.left);
                info->ptMinTrackSize.y = std::min(info->ptMinTrackSize.y, monitor.rcWork.bottom - monitor.rcWork.top);
            }
            return 0;
        }
        case WM_ACTIVATE:
            state->active = LOWORD(wParam) != WA_INACTIVE;
            if (state->timing) state->timing->Reset();
            return 0;
        case WM_MOUSEMOVE:
            state->app.MouseMove(GET_X_LPARAM(lParam) / state->app.DisplayScale(), GET_Y_LPARAM(lParam) / state->app.DisplayScale());
            { TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0}; TrackMouseEvent(&track); }
            return 0;
        case WM_MOUSELEAVE: state->app.MouseMove(-1, -1); return 0;
        case WM_LBUTTONDOWN:
            SetFocus(window);
            SetCapture(window);
            state->app.MouseDown(GET_X_LPARAM(lParam) / state->app.DisplayScale(), GET_Y_LPARAM(lParam) / state->app.DisplayScale());
            return 0;
        case WM_LBUTTONUP:
            state->app.MouseUp();
            if (GetCapture() == window) ReleaseCapture();
            return 0;
        case WM_CAPTURECHANGED: state->app.MouseUp(); return 0;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && state->app.Ready()) { SetCursor(LoadCursorW(nullptr, state->app.Cursor())); return TRUE; }
            break;
        case WM_KEYDOWN:
            if ((lParam & (1L << 30)) == 0 || wParam == VK_LEFT || wParam == VK_RIGHT ||
                wParam == VK_OEM_PLUS || wParam == VK_OEM_MINUS)
                state->app.KeyDown(static_cast<UINT>(wParam), (GetKeyState(VK_CONTROL) & 0x8000) != 0);
            return 0;
        case WM_DROPFILES: {
            const HDROP drop = reinterpret_cast<HDROP>(wParam);
            const UINT length = DragQueryFileW(drop, 0, nullptr, 0);
            std::vector<wchar_t> filename(static_cast<size_t>(length) + 1);
            DragQueryFileW(drop, 0, filename.data(), static_cast<UINT>(filename.size()));
            DragFinish(drop);
            state->app.LoadImage(filename.data());
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
    } catch (const std::exception& error) {
        if (!state->smokeTest) state->app.ShowError(error);
        if (GetCapture() == window) ReleaseCapture();
        if (message == WM_SIZE) PostQuitMessage(1);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int show) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(com)) { MessageBoxW(nullptr, L"Could not initialize Windows image services.", L"QIS-X", MB_ICONERROR); return 1; }
    int result = 0;
    {
        WindowState state;
        HWND window = nullptr;
        std::filesystem::path smokeArtifacts;
        try {
            int argc = 0;
            LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
            std::filesystem::path inputPath;
            if (argv) {
                state.smokeTest = argc > 1 && std::wstring(argv[1]) == L"--smoke-test";
                if (state.smokeTest) smokeArtifacts = argc > 2 ? argv[2] : L"native-test-artifacts";
                else if (argc > 1) inputPath = argv[1];
                LocalFree(argv);
            }
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = WindowProc;
            wc.hInstance = instance;
            wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
            wc.hIconSm = wc.hIcon;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.lpszClassName = L"QIS-X.Workbench";
            if (!RegisterClassExW(&wc))
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Register application window");
            state.dpi = GetDpiForSystem() / 96.0f;
            const int screenW = GetSystemMetrics(SM_CXSCREEN), screenH = GetSystemMetrics(SM_CYSCREEN);
            RECT workArea{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
            const int width = std::min(static_cast<int>(1440 * state.dpi), static_cast<int>(workArea.right - workArea.left - 32));
            const int height = std::min(static_cast<int>(900 * state.dpi), static_cast<int>(workArea.bottom - workArea.top - 48));
            window = CreateWindowExW(WS_EX_ACCEPTFILES, wc.lpszClassName, L"QIS-X | Spatial Upscaling Workbench",
                WS_OVERLAPPEDWINDOW, (screenW - width) / 2, (screenH - height) / 2, width, height,
                nullptr, nullptr, instance, &state);
            if (!window)
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Create application window");
            const BOOL dark = TRUE;
            DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
            state.dpi = GetDpiForWindow(window) / 96.0f;
            RECT client{}; GetClientRect(window, &client);
            state.app.Initialize(window, client.right, client.bottom);
            state.app.Resize(client.right, client.bottom, state.dpi);
            state.timing = std::make_unique<FrameSync>(state.app.SwapChain());
            if (!inputPath.empty()) {
                try { state.app.LoadImage(inputPath); } catch (const std::exception& error) { state.app.ShowError(error); }
            }
            ShowWindow(window, state.smokeTest ? SW_HIDE : show);
            UpdateWindow(window);
            MSG message{};
            bool running = true;
            int smokeFrames = 0;
            while (running) {
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) { result = static_cast<int>(message.wParam); running = false; break; }
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                if (!running) break;
                if (!state.smokeTest && (state.minimized || !state.active)) { WaitMessage(); continue; }
                state.timing->BeginFrame();
                state.app.Render(state.timing->GetFPS());
                state.timing->EndFrame(); // Vsync is handled by the one Present call.
                if (state.smokeTest) {
                    ++smokeFrames;
                    if (smokeFrames == 2) SetWindowPos(window, nullptr, 0, 0, 1280, 820, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                    if (smokeFrames == 3) state.app.KeyDown('P', false);
                    if (smokeFrames == 4) state.app.KeyDown('2', false);
                    if (smokeFrames == 5) state.app.KeyDown('R', false);
                    if (smokeFrames == 8) {
                        std::filesystem::create_directories(smokeArtifacts);
                        state.app.SavePreview(smokeArtifacts / L"native-workbench.png");
                        SendMessageW(window, WM_CLOSE, 0, 0);
                    }
                }
            }
        } catch (const std::exception& error) {
            if (!state.smokeTest) {
                state.app.ShowError(error);
                if (!window) MessageBoxA(nullptr, error.what(), "QIS-X / Startup failed", MB_OK | MB_ICONERROR);
            } else OutputDebugStringA(error.what());
            result = 1;
        }
        if (window && IsWindow(window)) DestroyWindow(window);
    }
    CoUninitialize();
    return result;
}
