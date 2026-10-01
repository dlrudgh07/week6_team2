#pragma once

#include "Core/Stats.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <array>

// Immediate context / main thread only. Pending slots are never overwritten.
class FGPUProfiler
{
public:
	static FGPUProfiler& Get();
	void BeginFrame(ID3D11Device* Device, ID3D11DeviceContext* Context);
	void EndFrame();
	void Shutdown();
	int32 BeginScope(FStatId Id, const wchar_t* Name);
	void EndScope(int32 Index);

private:
	static constexpr int32 MaxScopes = 32;
	struct FSample
	{
		Microsoft::WRL::ComPtr<ID3D11Query> Start, End;
		FStatId Id = 0;
		bool bEnded = false;
	};
	struct FFrame
	{
		Microsoft::WRL::ComPtr<ID3D11Query> Disjoint;
		std::array<FSample, MaxScopes> Samples;
		int32 Count = 0;
		bool bPending = false;
		uint64 Serial = 0;
	};
	std::array<FFrame, 4> Frames;
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* Context = nullptr;
	Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> Annotation;
	int32 ActiveFrame = -1;
	bool bFailed = false;
	uint64 NextSerial = 0;
	void Resolve();
};

class FGPUStatScope
{
public:
	FGPUStatScope(FStatId Id, const wchar_t* Name) : Index(FGPUProfiler::Get().BeginScope(Id, Name)) {}
	~FGPUStatScope() { FGPUProfiler::Get().EndScope(Index); }
	FGPUStatScope(const FGPUStatScope&) = delete;
	FGPUStatScope& operator=(const FGPUStatScope&) = delete;
private:
	int32 Index;
};
