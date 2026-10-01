#pragma once

#include "PlatformTime.h"

class FScopeCycleCounter
{
public:
    FScopeCycleCounter() : StartCycles(FPlatformTime::Cycles64())
    {
    }

    ~FScopeCycleCounter()
    {
        Finish();
    }

    uint64 Finish()
    {
        if (!bFinished)
        {
            const uint64 EndCycles = FPlatformTime::Cycles64();
            ElapsedCycles = EndCycles - StartCycles;
            bFinished = true;
        }
        return ElapsedCycles;
    }

private:
    uint64 StartCycles;
    uint64 ElapsedCycles = 0;
    bool bFinished = false;
};