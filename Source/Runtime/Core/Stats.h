#pragma once

#include "Types.h"
#include "EngineString.h"
#include "Container/Array.h"
#include "PlatformTime.h"

#include <cassert>

enum class EStatUnit : uint8
{
	Count,
	Milliseconds,
	Bytes,
};

enum class EStatMode : uint8
{
	FrameSum,
	Gauge,
	Event,
};

using FStatId = uint16;

struct FStatDesc
{
	FString Group;
	FString Name;
	EStatUnit Unit;
	EStatMode Mode;
	bool bEnabledByDefault = false;
	bool bCollectWhenPanelClosed = false;
};

struct FStatRecord
{
	FStatDesc Desc;

	double CurrentValue = 0.0; // FrameSum/Gauge의 현재값, Event의 Last
	double TotalValue = 0.0;
	double MaxValue = 0.0;
	uint64 SampleCount = 0;
	uint64 LastSampleFrame = 0;

	bool bEnabled = false;
};

class FStats
{
  public:
	static FStatId Register(const FStatDesc& Desc);

	static const FStatRecord& GetRecord(FStatId Id)	{ assert(Records.IsValidIndex(Id));	return Records[Id]; }
	static const TArray<FStatRecord>& GetRecords() { return Records; }

	static void BeginFrame();
	static void ResetSamples();
	static uint64 GetFrameNumber() { return FrameNumber; }
	static void SetDetailedCollectionEnabled(bool bEnabled) { bDetailedCollectionEnabled = bEnabled; }
	static bool IsDetailedCollectionEnabled() { return bDetailedCollectionEnabled; }

	static void Add(FStatId Id, double Value);
	static void Set(FStatId Id, double Value);
	static void RecordEvent(FStatId Id, double Value);

	static void SetEnabled(FStatId Id, bool bEnabled) {	assert(Records.IsValidIndex(Id)); Records[Id].bEnabled = bEnabled; }
	static bool IsEnabled(FStatId Id)
	{
		assert(Records.IsValidIndex(Id));
		const FStatRecord& Record = Records[Id];
		return Record.bEnabled && (bDetailedCollectionEnabled || Record.Desc.bCollectWhenPanelClosed);
	}

  private:
	inline static TArray<FStatRecord> Records;
	inline static uint64 FrameNumber = 0;
	// Keep checkbox preferences separate from the panel's collection gate.
	inline static bool bDetailedCollectionEnabled = false;
};

class FStatScope
{
  public:
	explicit FStatScope(FStatId InId);
	~FStatScope();

  private:
	FStatId Id;
	uint64 StartCycles = 0;
	bool bEnabled = false;
};
