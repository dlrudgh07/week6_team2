#pragma once

enum class ELogVerbosity
{
	Input,
	Info,
	Warning,
	Error
};

class FOutputDevice
{
public:
	virtual void Serialize(ELogVerbosity, const FString&) = 0;
};

class FLog
{
public:
    static void AddSink(FOutputDevice* Sink);
    static void RemoveSink(FOutputDevice* Sink);
    static void Emit(ELogVerbosity Verbosity, const FString& Message);

    template <typename... Args>
    static void Log(ELogVerbosity Verbosity, std::format_string<Args...> Fmt, Args&&... InArgs)
    {
        Emit(Verbosity, std::format(Fmt, std::forward<Args>(InArgs)...));
    }
};

#define LOG(Verbosity, ...) FLog::Log(ELogVerbosity::Verbosity, __VA_ARGS__)