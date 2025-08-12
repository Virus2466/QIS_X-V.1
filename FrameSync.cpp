#include "FrameSync.h"
#include<cstdio>

FrameSync::FrameSync(IDXGISwapChain* pSwapChain) 
    : m_pSwapChain(pSwapChain),
      m_frameCount(0),
      m_timeElapsed(0.0f),
      m_currentFPS(0.0f)
{
    QueryPerformanceFrequency(&m_frequency);
    QueryPerformanceCounter(&m_lastFrameTime);
    m_deltaTime = 0.016f; // Start with 60Hz assumption
    m_smoothedDeltaTime = m_deltaTime;

    IDXGISwapChain2* pSwapChain2 = nullptr;
    if (SUCCEEDED(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&pSwapChain2))) {
        m_frameLatencyWait = pSwapChain2->GetFrameLatencyWaitableObject();
        pSwapChain2->Release();
    }
    else {
        m_frameLatencyWait = NULL;
    }
}

FrameSync::~FrameSync() {
    if (m_frameLatencyWait) {
        CloseHandle(m_frameLatencyWait);
    }
}

void FrameSync::BeginFrame() {
    if (m_frameLatencyWait) {
        WaitForSingleObjectEx(m_frameLatencyWait, 1000, TRUE);
    }

    LARGE_INTEGER currentTime;
    QueryPerformanceCounter(&currentTime);
    m_deltaTime = (currentTime.QuadPart - m_lastFrameTime.QuadPart) / (float)m_frequency.QuadPart;
    m_lastFrameTime = currentTime;

    // Apply exponential smoothing to delta time
    const float alpha = 0.2f;
    m_smoothedDeltaTime = alpha * m_deltaTime + (1.0f - alpha) * m_smoothedDeltaTime;


    // FPS Calculation.
    m_frameCount++;
    m_timeElapsed += m_deltaTime;

    if (m_timeElapsed >= 1.0f) {
        m_currentFPS = m_frameCount / m_timeElapsed;
        m_frameCount = 0;
        m_timeElapsed = 0.0f;

        // Debug output
        char buffer[64];
        sprintf_s(buffer, "FPS: %.1f\n", m_currentFPS);
        OutputDebugStringA(buffer);
    }


}

void FrameSync::EndFrame(int targetFPS) {
    if (targetFPS > 0) {
        float targetFrameTime = 1.0f / targetFPS;
        float remainingTime = targetFrameTime - m_deltaTime;

        if (remainingTime > 0.001f) {
            // Sleep for most of the remaining time
            Sleep(static_cast<DWORD>(remainingTime * 800.0f));

            // Busy-wait for the remainder
            LARGE_INTEGER waitStart, waitCurrent;
            QueryPerformanceCounter(&waitStart);
            do {
                QueryPerformanceCounter(&waitCurrent);
            } while ((waitCurrent.QuadPart - waitStart.QuadPart) /
                (float)m_frequency.QuadPart < remainingTime);
        }
    }
}