#pragma once
#include <Windows.h>
#include <dxgi1_3.h>

// Present owns vsync. Optional caps measure work since this frame began.
class FrameSync {
public:
    explicit FrameSync(IDXGISwapChain* swapChain = nullptr);
    ~FrameSync();
    FrameSync(const FrameSync&) = delete;
    FrameSync& operator=(const FrameSync&) = delete;
    void BeginFrame();
    void EndFrame(int targetFPS = 0);
    void Reset();
    float GetDeltaTime() const { return m_deltaTime; }
    float GetSmoothedDeltaTime() const { return m_smoothedDeltaTime; }
    float GetFPS() const { return m_currentFPS; }
private:
    LARGE_INTEGER m_frequency{}, m_lastFrameTime{}, m_frameStart{};
    float m_deltaTime = 1.0f / 60.0f;
    float m_smoothedDeltaTime = 1.0f / 60.0f;
    int m_frameCount = 0;
    double m_timeElapsed = 0.0;
    float m_currentFPS = 0.0f;
    HANDLE m_frameLatencyWait = nullptr;
};
