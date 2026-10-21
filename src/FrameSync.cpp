#include "../include/FrameSync.h"
#include <algorithm>

FrameSync::FrameSync(IDXGISwapChain* swapChain) {
    QueryPerformanceFrequency(&m_frequency);
    Reset();
    IDXGISwapChain2* chain = nullptr;
    if (swapChain && SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&chain)))) {
        chain->SetMaximumFrameLatency(1);
        m_frameLatencyWait = chain->GetFrameLatencyWaitableObject();
        chain->Release();
    }
}
FrameSync::~FrameSync() {
    if (m_frameLatencyWait) CloseHandle(m_frameLatencyWait);
}
void FrameSync::Reset() {
    QueryPerformanceCounter(&m_lastFrameTime);
    m_frameStart = m_lastFrameTime;
    m_frameCount = 0;
    m_timeElapsed = 0.0;
    m_currentFPS = 0.0f;
    m_deltaTime = m_smoothedDeltaTime = 1.0f / 60.0f;
}
void FrameSync::BeginFrame() {
    if (m_frameLatencyWait)
        MsgWaitForMultipleObjectsEx(1, &m_frameLatencyWait, 100, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    QueryPerformanceCounter(&m_frameStart);
    const double elapsed = static_cast<double>(m_frameStart.QuadPart - m_lastFrameTime.QuadPart) /
        static_cast<double>(m_frequency.QuadPart);
    m_lastFrameTime = m_frameStart;
    m_deltaTime = static_cast<float>(std::max(elapsed, 0.000001));
    m_smoothedDeltaTime += 0.15f * (m_deltaTime - m_smoothedDeltaTime);
    ++m_frameCount;
    m_timeElapsed += elapsed;
    if (m_timeElapsed >= 0.5) {
        m_currentFPS = static_cast<float>(m_frameCount / m_timeElapsed);
        m_frameCount = 0;
        m_timeElapsed = 0.0;
    }
}
void FrameSync::EndFrame(int targetFPS) {
    if (targetFPS <= 0) return;
    const double target = 1.0 / targetFPS;
    LARGE_INTEGER now{};
    for (;;) {
        QueryPerformanceCounter(&now);
        const double remaining = target - static_cast<double>(now.QuadPart - m_frameStart.QuadPart) /
            static_cast<double>(m_frequency.QuadPart);
        if (remaining <= 0.0) break;
        if (remaining > 0.002) Sleep(static_cast<DWORD>((remaining - 0.001) * 1000.0));
        else SwitchToThread();
    }
}
