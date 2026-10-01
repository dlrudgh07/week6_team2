#include "EnginePCH.h"
#include "GPUProfiler.h"
#include "Core/StatDefinitions.h"
#include <algorithm>

FGPUProfiler& FGPUProfiler::Get()
{
	static FGPUProfiler Instance;
	return Instance;
}

void FGPUProfiler::BeginFrame(ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
{
	if (!FStats::IsDetailedCollectionEnabled())
	{
		// Previous scopes have ended. Discard pending results without polling the GPU.
		if (Device)
			Shutdown();
		return;
	}
	Device = InDevice;
	Context = InContext;
	if (!Device || !Context || bFailed)
		return;
	Resolve();
	if (bFailed || !(FStats::IsEnabled(StatIds::GpuFrame()) || FStats::IsEnabled(StatIds::GpuOpaque())
		|| FStats::IsEnabled(StatIds::GpuHZB()) || FStats::IsEnabled(StatIds::GpuCull())
		|| FStats::IsEnabled(StatIds::GpuGrid()) || FStats::IsEnabled(StatIds::GpuEditor())))
		return;
	for (int32 Index = 0; Index < static_cast<int32>(Frames.size()); ++Index)
	{
		FFrame& Frame = Frames[Index];
		if (Frame.bPending)
			continue;
		if (!Frame.Disjoint)
		{
			D3D11_QUERY_DESC Desc{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
			if (FAILED(Device->CreateQuery(&Desc, Frame.Disjoint.GetAddressOf())))
			{
				bFailed = true;
				LOG(Warning, "GPU profiling query creation failed; profiling disabled.");
				return;
			}
		}
		if (!Annotation)
			Context->QueryInterface(IID_PPV_ARGS(Annotation.GetAddressOf()));
		Frame.Count = 0;
		Frame.Serial = NextSerial++;
		ActiveFrame = Index;
		Context->Begin(Frame.Disjoint.Get());
		return;
	}
	FStats::Add(StatIds::GpuSkipped(), 1);
}

int32 FGPUProfiler::BeginScope(FStatId Id, const wchar_t* Name)
{
	if (ActiveFrame < 0 || bFailed || !FStats::IsEnabled(Id))
		return -1;
	FFrame& Frame = Frames[ActiveFrame];
	if (Frame.Count == MaxScopes)
		return -1;
	FSample& Sample = Frame.Samples[Frame.Count];
	D3D11_QUERY_DESC Desc{D3D11_QUERY_TIMESTAMP, 0};
	if ((!Sample.Start && FAILED(Device->CreateQuery(&Desc, Sample.Start.GetAddressOf())))
		|| (!Sample.End && FAILED(Device->CreateQuery(&Desc, Sample.End.GetAddressOf()))))
	{
		bFailed = true;
		LOG(Warning, "GPU timestamp creation failed; profiling disabled.");
		return -1;
	}
	Sample.Id = Id;
	Sample.bEnded = false;
	if (Annotation)
		Annotation->BeginEvent(Name);
	Context->End(Sample.Start.Get());
	return Frame.Count++;
}

void FGPUProfiler::EndScope(int32 Index)
{
	if (Index < 0 || ActiveFrame < 0)
		return;
	FSample& Sample = Frames[ActiveFrame].Samples[Index];
	Context->End(Sample.End.Get());
	Sample.bEnded = true;
	if (Annotation)
		Annotation->EndEvent();
}

void FGPUProfiler::EndFrame()
{
	if (ActiveFrame < 0)
		return;
	FFrame& Frame = Frames[ActiveFrame];
	Context->End(Frame.Disjoint.Get());
	Frame.bPending = true;
	ActiveFrame = -1;
}

void FGPUProfiler::Resolve()
{
	constexpr UINT Flags = D3D11_ASYNC_GETDATA_DONOTFLUSH;
	std::array<FFrame*, 4> Ordered{&Frames[0], &Frames[1], &Frames[2], &Frames[3]};
	std::sort(Ordered.begin(), Ordered.end(), [](const FFrame* A, const FFrame* B) { return A->Serial < B->Serial; });
	for (FFrame* Entry : Ordered)
	{
		FFrame& Frame = *Entry;
		if (!Frame.bPending)
			continue;
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT Data{};
		HRESULT Hr = Context->GetData(Frame.Disjoint.Get(), &Data, sizeof(Data), Flags);
		if (Hr == S_FALSE)
			return;
		if (FAILED(Hr))
		{
			bFailed = true;
			LOG(Warning, "GPU profiling readback failed; profiling disabled.");
			return;
		}
		if (Data.Disjoint || Data.Frequency == 0)
		{
			Frame.bPending = false;
			continue;
		}
		std::array<double, MaxScopes> Times{};
		bool bReady = true;
		for (int32 Index = 0; Index < Frame.Count; ++Index)
		{
			const FSample& Sample = Frame.Samples[Index];
			UINT64 Start = 0, End = 0;
			const HRESULT StartHr = Context->GetData(Sample.Start.Get(), &Start, sizeof(Start), Flags);
			const HRESULT EndHr = Sample.bEnded ? Context->GetData(Sample.End.Get(), &End, sizeof(End), Flags) : E_FAIL;
			if (FAILED(StartHr) || FAILED(EndHr))
			{
				bFailed = true;
				LOG(Warning, "GPU timestamp readback failed; profiling disabled.");
				return;
			}
			if (StartHr != S_OK || EndHr != S_OK)
			{
				bReady = false;
				break;
			}
			Times[Index] = End >= Start ? double(End - Start) * 1000.0 / double(Data.Frequency) : -1.0;
		}
		if (!bReady)
			return;
		for (int32 Index = 0; Index < Frame.Count; ++Index)
		{
			if (Times[Index] >= 0)
				FStats::RecordEvent(Frame.Samples[Index].Id, Times[Index]);
		}
		Frame.bPending = false;
	}
}

void FGPUProfiler::Shutdown()
{
	for (FFrame& Frame : Frames)
		Frame = FFrame{};
	Annotation.Reset();
	Device = nullptr;
	Context = nullptr;
	ActiveFrame = -1;
	bFailed = false;
	NextSerial = 0;
}
