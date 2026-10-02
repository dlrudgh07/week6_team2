#include "EnginePCH.h"
#include "Logging/LogMacros.h"

namespace
{
    TArray<FOutputDevice*>& GetSinks()
    {
        static TArray<FOutputDevice*> Sinks;
        return Sinks;
    }
}

void FLog::AddSink(FOutputDevice* Sink)
{
    if (Sink)
        GetSinks().Add(Sink);
}

void FLog::RemoveSink(FOutputDevice* Sink)
{
    TArray<FOutputDevice*>& Sinks = GetSinks();
    for (int32 Index = Sinks.Num() - 1; Index >= 0; --Index)
        if (Sinks[Index] == Sink)
            Sinks.RemoveAt(Index, 1);
}

void FLog::Emit(ELogVerbosity Verbosity, const FString& Message)
{
    for (FOutputDevice* Sink : GetSinks())
        Sink->Serialize(Verbosity, Message);
}