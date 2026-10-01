#include "EnginePCH.h"
#include "Stats.h"

FStatId FStats::Register(const FStatDesc& Desc)
{
	const FStatId Id = static_cast<FStatId>(Records.Num());
	FStatRecord Record{};
	Record.Desc = Desc;
	Record.bEnabled = Desc.bEnabledByDefault;
	Records.Add(Record);
	return Id;
}

void FStats::BeginFrame()
{
	++FrameNumber;
	for (FStatRecord& Record : Records)
	{
		if (Record.Desc.Mode == EStatMode::FrameSum)
		{
			Record.CurrentValue = 0.0;
		}
	}
}

void FStats::ResetSamples()
{
	for (FStatRecord& Record : Records)
	{
		Record.CurrentValue = Record.TotalValue = Record.MaxValue = 0.0;
		Record.SampleCount = Record.LastSampleFrame = 0;
	}
}

void FStats::Add(FStatId Id, double Value)
{
	FStatRecord& Record = Records[Id];
	assert(Record.Desc.Mode == EStatMode::FrameSum);

	if (!IsEnabled(Id))
		return;

	Record.CurrentValue += Value;
}

void FStats::Set(FStatId Id, double Value)
{
	FStatRecord& Record = Records[Id];
	assert(Record.Desc.Mode == EStatMode::Gauge);

	if (!IsEnabled(Id))
		return;

	Record.CurrentValue = Value;
}

void FStats::RecordEvent(FStatId Id, double Value)
{
	FStatRecord& Record = Records[Id];

	assert(Record.Desc.Mode == EStatMode::Event);

	if (!IsEnabled(Id))
		return;

	Record.CurrentValue = Value;
	Record.TotalValue += Value;
	if (Record.SampleCount == 0 || Value > Record.MaxValue)
	{
		Record.MaxValue = Value;
	}
	Record.SampleCount++;
	Record.LastSampleFrame = FrameNumber;
}

FStatScope::FStatScope(FStatId InId) : Id(InId)
{
	bEnabled = FStats::IsEnabled(Id);

	if (!bEnabled)
		return;

	StartCycles = FPlatformTime::Cycles64();
}

FStatScope::~FStatScope()
{
	if (!bEnabled)
		return;

	const uint64 EndCycles = FPlatformTime::Cycles64();
	const uint64 ElapsedCycles = EndCycles - StartCycles;
	const double Milliseconds = FPlatformTime::ToMilliseconds(ElapsedCycles);

	FStats::RecordEvent(Id, Milliseconds);
}
