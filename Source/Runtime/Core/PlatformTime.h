#pragma once

#include "Core/Types.h"

class FWindowsPlatformTime
{
public:
    static void InitTiming();
    static double GetSecondsPerCycle();
    static uint64 GetFrequency();
    static double ToMilliseconds(uint64 CycleDiff);
    static uint64 Cycles64();

private:
    static double GSecondsPerCycle;
    static bool bInitialized;
};

using FPlatformTime = FWindowsPlatformTime;