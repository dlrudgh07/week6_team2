#pragma once

#include "Core/EngineString.h"
#include <functional>

class UWorld;

// Default.scene 파일 전용 고속 씬 로더
class FDefaultSceneLoader
{
public:
	static bool LoadScene(UWorld* World, const FString& Path, std::function<void(float)> OnProgress = nullptr);
};
