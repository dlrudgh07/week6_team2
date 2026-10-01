#pragma once

#include "EnginePCH.h"
#include <Windows.h>
#include "Application.h"
#include "PerformanceMode.h"

extern TUniquePtr<FApplication> CreateApplication();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	TUniquePtr<FApplication> App = CreateApplication();
	if (!App->Init(hInstance)) return -1;
	{
#if !defined(_DEBUG)
		// Run 동안만 성능 모드를 유지하고, 스코프를 벗어나면 소멸자가 전원 계획을 되돌린다.
		// Debug 빌드는 디버거 강제 종료 시 복구가 안 되므로 제외한다.
		FPerformanceModeGuard PerformanceMode;
#endif
		App->Run();
	}
	App->Shutdown();

	if (FEngineStatics::TotalAllocationCount > 0)
		OutputDebugStringA("leaked UObjects\n");

	return 0;
}
