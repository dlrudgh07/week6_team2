#pragma once

#include "HAL/Platform.h"

struct FEngineStatics
{
public:
	static uint32 GetUUID()
	{
		return NextUUID++;
	}
	inline static uint32 NextUUID = 1;

	inline static uint64 TotalAllocationBytes = 0;
	inline static uint32 TotalAllocationCount = 0;
};