#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include <dxgiformat.h>

struct FImageData
{
	FImageData() = default;

	bool IsValid() const { return !Pixels.IsEmpty(); }

	// 행 하나의 바이트 수. FRHITexture2D가 SysMemPitch로 쓴다.
	uint32 GetRowPitch() const { return Width * BytesPerPixel; }

	TArray<uint8> Pixels;
	uint32 Width = 0;
	uint32 Height = 0;
	uint32 BytesPerPixel = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
};

class FImageLoader
{
public:
	static FImageData Load(const FString& Path);
	static FImageData LoadHDR(const FString& Path);
	static FImageData LoadAuto(const FString& Path);
};
