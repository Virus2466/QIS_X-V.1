#pragma once
#include <Windows.h>

class FrameSync {
private:
	LARGE_INTEGER m_frequency;
	LARGE_INTEGER m_lastTime;
	float m_deltaTime;
	float m_fps;
	int m_frameCount;
	float m_timeElapsed;

public:
	FrameSync();

	// Call at the end of each frame
	void Sync(int targetFPS = 60);
	
	// Get time between frames in seconds 
	float GetDeltaTime() const { return m_deltaTime; }

	// Get Calculated FPS
	float GetFPS() const { return m_fps; }

	// Reset timing stats
	void Reset();

};