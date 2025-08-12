#pragma once
#include <Windows.h>
#include<dxgi.h>
#include<dxgi1_4.h>

class FrameSync {
private:
    IDXGISwapChain* m_pSwapChain;
	LARGE_INTEGER m_frequency;
	LARGE_INTEGER m_lastFrameTime;
	float m_deltaTime;
	float m_smoothedDeltaTime;
    int m_frameCount;
    float m_timeElapsed;
    float m_currentFPS;
	HANDLE m_frameLatencyWait;

public:
    FrameSync(IDXGISwapChain* m_pSwapChain);
    ~FrameSync();

    void BeginFrame();
    void EndFrame(int targetFPS = 0);

    float GetDeltaTime() const { return m_deltaTime; }
    float GetSmoothedDeltaTime() const { return m_smoothedDeltaTime; }
    float GetFPS() const { return m_currentFPS;  }

private:
    void WaitForGPU();

};