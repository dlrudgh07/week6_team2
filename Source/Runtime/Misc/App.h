#pragma once

#include <chrono>

// winbase.h의 GetCurrentTime() 매크로(GetTickCount)가 FApp::GetCurrentTime을 덮어쓰지 않도록 해제한다.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

class FApp
{
public:
	static void Init();
	static void Tick();

	static float GetDeltaTime() { return DeltaTime; };
	static float GetCurrentTime() { return TotalTime; }
private:
	using Clock = std::chrono::high_resolution_clock;

	inline static Clock::time_point PrevTime;
	inline static float DeltaTime = 0.0f;
	inline static float TotalTime = 0.0f;
};
