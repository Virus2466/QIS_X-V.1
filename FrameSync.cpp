#include "FrameSync.h"
#include<cstdio>

FrameSync::FrameSync() {
	QueryPerformanceFrequency(&m_frequency);
	QueryPerformanceCounter(&m_lastTime);
	m_deltaTime = 0.0f;
	m_fps = 0.0f;
	m_frameCount = 0;
	m_timeElapsed = 0.0f;
}


void FrameSync::Sync(int targetFPS) {
	// Get current time
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);

	// Calculate delta time
	m_deltaTime = (currentTime.QuadPart - m_lastTime.QuadPart) / static_cast<float>(m_frequency.QuadPart);
	
	m_lastTime = currentTime;

	// Calc FPS
	m_frameCount++;
	m_timeElapsed += m_deltaTime;

	if (m_timeElapsed >= 1.0f) {
		m_fps = m_frameCount / m_timeElapsed;
		m_frameCount = 0;
		m_timeElapsed = 0.0f;


		// output fps to debug console.
		char buffer[256];
		sprintf_s(buffer, "FPS : %.1f\n", m_fps);
		OutputDebugStringA(buffer);
	}


	// Frame Pacing
	if (targetFPS > 0) {
		const float targetFrameTime = 1.0f / targetFPS;
		if (m_deltaTime < targetFrameTime) {
			DWORD sleepTime = static_cast<DWORD>((targetFrameTime - m_deltaTime) * 1000);
		}
	}
}


void FrameSync::Reset() {
	QueryPerformanceCounter(&m_lastTime);
	m_deltaTime = 0.0f;
	m_frameCount = 0;
	m_timeElapsed = 0.0f;
}