#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"
#include <d3d11.h>

class UTexture2D;

class FLoadingScreen
{
public:
	FLoadingScreen();
	~FLoadingScreen();

	bool Init();
	void Tick(float DeltaTime);
	void Draw();

	bool IsFinished() const { return bIsFinished; }
	float GetProgress() const { return CurrentProgress; }
	void SetProgress(float InProgress);
	void SetStatusText(const FString& InText);
	void SetSceneLoaded(bool bLoaded);
	void SetAnimationFps(float InFps) { AnimationFps = InFps; }

private:
	UTexture2D* SpriteTexture = nullptr;
	float CurrentProgress = 0.0f;
	float TargetProgress = 0.0f;
	float ElapsedTime = 0.0f;
	float MinDuration = 2.0f;
	float AnimationFps = 0.15f;
	int32 TotalFrames = 24;
	int32 FrameColumns = 6;
	int32 FrameRows = 4;
	int32 CurrentFrameIndex = 0;
	float CompletionHoldTimer = 0.0f;
	bool bIsFinished = false;
	bool bSceneLoaded = false;
	FString CurrentStatusText;
};
