#include "EnginePCH.h"

#include "Core/PlatformTime.h"

double FWindowsPlatformTime::GSecondsPerCycle = 0.0;
bool FWindowsPlatformTime::bInitialized = false;

void FWindowsPlatformTime::InitTiming()
{
    if (!bInitialized)
    {
        bInitialized = true;

        double Frequency = static_cast<double>(GetFrequency());
        if (Frequency <= 0.0)
        {
            Frequency = 1.0;
        }

        GSecondsPerCycle = 1.0 / Frequency;
    }
}

double FWindowsPlatformTime::GetSecondsPerCycle()
{
    if (!bInitialized)
    {
        InitTiming();
    }
    return GSecondsPerCycle;
}

uint64 FWindowsPlatformTime::Cycles64()
{
    LARGE_INTEGER Counter;
    QueryPerformanceCounter(&Counter);
    return static_cast<uint64>(Counter.QuadPart);
}

uint64 FWindowsPlatformTime::GetFrequency()
{
    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency);
    return static_cast<uint64>(Frequency.QuadPart);
}

double FWindowsPlatformTime::ToMilliseconds(uint64 CycleDiff)
{
    return CycleDiff * GetSecondsPerCycle() * 1000;
}