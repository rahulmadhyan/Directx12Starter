#pragma once
#include <Windows.h>
#include <string>

class Timer
{
public:
	Timer();
	Timer(HWND handle, LPCTSTR windowTitle);
	~Timer();

	const float& GetDeltaTime() const;
	const float& GetTotalTime() const;

	void UpdateTimer();
	void UpdateTitleBarStats();

private:
	double perfCounterSeconds;

	float totalTime;
	float deltaTime;

	__int64 startTime;
	__int64 currentTime;
	__int64 previousTime;

	int fpsFrameCount;
	float fpsTimeElapsed;

	HWND hwnd;
	LPCTSTR windowTitle;
};
