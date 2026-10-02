#pragma once

#include "EnginePCH.h"
#include <Windows.h>
#include "Application.h"

extern TUniquePtr<FApplication> CreateApplication();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	TUniquePtr<FApplication> App = CreateApplication();
	if (!App->Init(hInstance)) return -1;
	{
		App->Run();
	}
	App->Shutdown();

	if (FEngineStatics::TotalAllocationCount > 0)
		OutputDebugStringA("leaked UObjects\n");

	return 0;
}
